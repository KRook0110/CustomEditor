#ifndef TERMINAL_UTIL_H
#define TERMINAL_UTIL_H

int get_cursor_location(int *row, int *col);

void die(const char *s);
int get_window_size(int *rows, int *cols);

#endif
