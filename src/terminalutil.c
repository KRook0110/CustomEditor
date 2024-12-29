#include "headers/terminalutil.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

int get_cursor_location(int *row, int *col) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", row, col) != 2) return -1;
    return 0;
}

int get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        int err = get_cursor_location(rows, cols);
        if (write(STDOUT_FILENO, "\x1b[H", 3) != 3) return -1;
        return err;
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

void die(const char *s) {
    write(STDIN_FILENO, "\x1b[2J", 4);
    write(STDIN_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

