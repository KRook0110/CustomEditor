#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifdef _WIN32
#incldue <windows.h>
#endif

#include "headers/appendbuffer.h"
#include "headers/debug.h"
#include "headers/terminalutil.h"

/*
TODO:
1. E.cursor_y can go over num_rows and other resources are accessing
E.rows[E.cursor_y];
*/

/* Macros and constants*/

#define CTRL_KEY(k) ((k) & 0x1f)
#define EDITOR_VERSION "0.0.1"
#define MAX_PATH_SIZE 4096

enum EditorState { IN_HOMEMENU, EDITING_FILE };

enum EditorKey {
    ENTER = 13,  // carrige return
    ARROW_UP = 1000,
    ARROW_RIGHT,
    ARROW_DOWN,
    ARROW_LEFT,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DELETE_KEY
};

/* data */
struct erow {
    int size;
    char *chars;
};

struct FileData {
    char filepath[MAX_PATH_SIZE];
    size_t path_size;
};

struct EditorConfig {
    enum EditorState state;
    int cursor_x, cursor_y;
    int screen_rows;
    int screen_cols;
    int row_offset;
    int col_offset;
    struct termios orig_termios;
    int num_rows;
    struct erow *rows;

    struct FileData editing_file;
    struct FileData executable;
};

void swap_erow(struct erow *a, struct erow *b) {
    struct erow t = *a;
    *a = *b;
    *b = t;
}

void swap_char(char *a, char *b) {
    char t = *a;
    *a = *b;
    *b = t;
}

struct EditorConfig E;

/* Terminal */
void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr failed in disable_raw_mode");
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr in enable_raw_mode");
    }
    atexit(disable_raw_mode);

    struct termios raw = E.orig_termios;
    // Turning off flags
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] =
        0;  // minimum number of characters before read() can return
    raw.c_cc[VTIME] = 1;  // maximum number of time before read() return

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr in enable_raw_mode");
    }
}

/* file io */
void save_file() {
    FILE *p_file = fopen(E.editing_file.filepath, "w");
    if(p_file == NULL) {
        die("Failed trying to open file in save_file()\n");
    }
    for(int i =0 ;i < E.num_rows;i++) {
        fprintf(p_file, "%s\n",E.rows[i].chars);
    }
    fclose(p_file);
}

void row_append(const char *line, size_t line_len) {
    E.num_rows++;
    E.rows = (struct erow *)realloc(E.rows, sizeof(struct erow) * E.num_rows);
    E.rows[E.num_rows - 1].size = line_len;
    E.rows[E.num_rows - 1].chars = (char *)malloc(line_len + 1);
    memcpy(E.rows[E.num_rows - 1].chars, line, line_len);
    E.rows[E.num_rows - 1].chars[line_len] = '\0';
}

void free_rows() {
    for (int i = 0; i < E.num_rows; i++) {
        free(E.rows[i].chars);
    }
    DEBUG_PRINT("Freed rows");
}

void editor_open(const char *filename) {
    if (filename == NULL) {
        row_append("", 0);
        E.state = EDITING_FILE;
        return;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        char temp[64];
        snprintf(temp, 64, "fopen fail on editor_open() trying to open %s\r\n",
                 filename);
        die(temp);
    }
    char *line = NULL;
    size_t linecap = 0;
    ssize_t line_len;
    while ((line_len = getline(&line, &linecap, fp)) != -1) {
        while (line_len > 0 &&
               (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line_len--;
        }
        row_append(line, line_len);
    }
    free(line);
    fclose(fp);
    E.state = EDITING_FILE;
}

int editor_read_key() {
    char c = '\0';
    int nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read in editor_read_key");
        }
    }

    if (c == '\x1b') {  // process arrows keys or any extra control characters
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DELETE_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                }
            }
            switch (seq[1]) {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        }

        return '\x1b';
    }

    return c;
}

/* input */
void row_insert(const char *s, size_t len, int y) {
    row_append(s, len);
    for (int i = E.num_rows - 1; i > y; i--) {
        swap_erow(E.rows + i, E.rows + i - 1);
    }
}

void char_append(const char c, int y) {
    E.rows[y].size++;
    E.rows[y].chars = (char *)realloc(E.rows[y].chars, E.rows[y].size + 1);
    E.rows[y].chars[E.rows[y].size] = '\0';
    E.rows[y].chars[E.rows[y].size - 1] = c;
}

void char_insert(const char c, int x, int y) {
    char_append(c, y);
    for (int i = E.rows[y].size - 1; i > x; i--) {
        swap_char(&(E.rows[y].chars[i]), &(E.rows[y].chars[i - 1]));
    }
}

void editor_move_cursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if (E.cursor_x == 0) {
                editor_move_cursor(ARROW_UP);
                E.cursor_x = E.rows[E.cursor_y].size;
            } else {
                if (E.cursor_x > E.rows[E.cursor_y].size)
                    E.cursor_x = E.rows[E.cursor_y].size;
                E.cursor_x--;
            }
            break;
        case ARROW_DOWN:
            if (E.cursor_y < E.num_rows) E.cursor_y++;
            break;
        case ARROW_UP:
            if (E.cursor_y != 0) E.cursor_y--;
            break;
        case ARROW_RIGHT:
            // if (E.cursor_x != E.screen_cols - 1) E.cursor_x++;
            if (E.cursor_x <= E.rows[E.cursor_y].size) {
                if (E.cursor_x == E.rows[E.cursor_y].size) {
                    editor_move_cursor(ARROW_DOWN);
                    E.cursor_x = 0;
                    break;
                }
                E.cursor_x++;
            }
            break;
    }
}

void editor_process_key() {
    int c = editor_read_key();
    if (c != '\0' && E.state == IN_HOMEMENU) {
        editor_open(NULL);
    }

    int cx = E.cursor_x;
    if (cx >= E.rows[E.cursor_y].size) {
        cx = E.rows[E.cursor_y].size;
    }
    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case CTRL_KEY('s'):
            save_file();
            break;
        case ARROW_DOWN:
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(c);
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            for (int i = 0; i < E.screen_rows; i++) {
                editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        case HOME_KEY:
            E.cursor_x = 0;
            break;
        case END_KEY:
            E.cursor_x = E.rows[E.cursor_y].size;
            break;
        case ENTER:
            if (E.cursor_x < E.rows[E.cursor_y].size) {
                row_insert(&E.rows[E.cursor_y].chars[E.cursor_x],
                           E.rows[E.cursor_y].size - E.cursor_x,
                           E.cursor_y + 1);
                E.rows[E.cursor_y].size = E.cursor_x;
                E.rows[E.cursor_y].chars[E.cursor_x] = '\0';
                editor_move_cursor(ARROW_RIGHT);
            } else {
                row_insert("", 0, E.cursor_y + 1);
            }
            break;
        default:
            char_insert(c, cx, E.cursor_y);
            E.cursor_x = cx;
            editor_move_cursor(ARROW_RIGHT);
            break;
    }
}

void editor_scroll() {
    if (E.cursor_y < E.row_offset) {
        E.row_offset = E.cursor_y;
    }
    if (E.cursor_y >= E.row_offset + E.screen_rows) {
        E.row_offset = E.cursor_y - E.screen_rows + 1;
    }
    if (E.cursor_x < E.col_offset) {
        E.col_offset = E.cursor_x;
    }
    if (E.cursor_x >= E.col_offset + E.screen_cols) {
        E.col_offset = E.cursor_x - E.screen_cols + 1;
    }
}

void editor_draw_rows(struct ABuffer *ab) {
    for (int i = 0; i < E.screen_rows; i++) {
        int file_row = i + E.row_offset;
        if( i == E.screen_rows - 1 && E.state == EDITING_FILE) {
            char s[E.screen_cols + 1];
            int len = snprintf(s, E.screen_cols, "Editing file : %s", E.editing_file.filepath);
            if(len >= E.screen_cols) len = E.screen_cols;
            ab_append(ab, s, len);
        }
        else if (file_row >= E.num_rows) {
            if (E.num_rows == 0 && i == E.screen_rows / 3) {
                char welcome[80];
                int welcomelen =
                    snprintf(welcome, sizeof(welcome),
                             "Shawn Editor -- version %s", EDITOR_VERSION);
                if (welcomelen > E.screen_cols) welcomelen = E.screen_cols;

                int padding = (E.screen_cols - welcomelen) / 2;
                if (padding) {
                    ab_append(ab, "~", 1);
                    padding--;
                }
                while (padding--) ab_append(ab, " ", 1);
                ab_append(ab, welcome, welcomelen);
            } else {
                ab_append(ab, "~", 1);
            }
        } else {
            int len = E.rows[file_row].size - E.col_offset;
            if (len > E.screen_cols) len = E.screen_cols;
            if (len < 0) len = 0;
            ab_append(ab, &E.rows[file_row].chars[E.col_offset], len);
        }

        ab_append(ab, "\x1b[K",
                  3);  // removal of all the characters after the cursor
        if (i != E.screen_rows - 1) {
            ab_append(ab, "\r\n", 2);
        }
    }
}

void editor_refresh_screen() {
    editor_scroll();

    struct ABuffer ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6);  // cursor hiding
    ab_append(&ab, "\x1b[H", 3);     // place cursor in top left

    editor_draw_rows(&ab);

    char buf[32];
    int ecx = E.cursor_x - E.col_offset;
    if (E.num_rows > 0 && E.rows[E.cursor_y].size - E.col_offset < ecx)
        ecx = E.rows[E.cursor_y].size - E.col_offset;
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cursor_y - E.row_offset + 1,
             ecx + 1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6);  // addition removal of cursor hiding
    write(STDIN_FILENO, ab.buffer, ab.len);

    ab_free(&ab);
}

/* init */
void get_file_path(struct FileData *fd) {

#ifdef __linux__
    ssize_t len = readlink("/proc/self/exe", fd->filepath, MAX_PATH_SIZE - 1);
    if (len == -1) {
        die("readlink in get_file_path failed for linux");
    }
    if(len + 1 >= MAX_PATH_SIZE) die("Path is too long");
    fd->filepath[len] = '\0';
    fd->path_size = len;
#endif

#ifdef _WIN32
    DWORD length = GetModuleFileName(NULL, fd->filepath, MAX_PATH_SIZE);
    if(length == 0) die("readlink in get_file_path failed for windows");
    if(length >= MAX_PATH_SIZE) die("Path is too long");
    fd->path_size = length;
#endif

}

void init_editor() {
    E.cursor_x = 0;
    E.cursor_y = 0;
    E.num_rows = 0;
    E.row_offset = 0;
    E.rows = NULL;
    E.state = IN_HOMEMENU;
    enable_raw_mode();
    get_file_path(&E.executable);
    if (get_window_size(&E.screen_rows, &E.screen_cols) == -1) {
        die("get_window_size died at main");
    }
}

int main(int argc, char *argv[]) {
    init_editor();
    if (argc >= 2) {
        editor_open(argv[1]);

        // handling editing file path
        char *s = argv[1];
        if(argv[1][0] == '.' && argv[1][1] =='/') {
            s = &argv[1][2];
        }
        size_t s_size = strlen(s);

        memcpy(E.editing_file.filepath, E.executable.filepath, E.executable.path_size);
        int i = E.executable.path_size - 1;
        while(i >= 0 && E.editing_file.filepath[i] != '/' ) i--;
        if(i == 0) die("File path invalid in main");
        i++;
        if(i + s_size >= MAX_PATH_SIZE) die("Path too long in main()");

        memcpy(E.editing_file.filepath + i, s, s_size); // check if editing_file.filepath + i  + s_size >= MAX_PATH_SIZE
        E.editing_file.filepath[i + s_size] = '\0';
        E.editing_file.path_size = i + s_size;

        fprintf(stderr, "i : %d\n", i);
        fprintf(stderr, "editing filepath : %s\n", E.editing_file.filepath);
        fprintf(stderr, "editing pathsize : %zu\n", E.editing_file.path_size);
        fprintf(stderr, "executable filepath : %s\n", E.executable.filepath);
        fprintf(stderr, "executable pathsize : %zu\n", E.executable.path_size);

        atexit(free_rows);
    }

    while (1) {
        editor_refresh_screen();
        editor_process_key();
    }

    for (int i = 0; i < E.num_rows; i++) {
        DEBUG_PRINT("%d - %s\r\n", i, E.rows[i].chars);
    }

    return 0;
}
