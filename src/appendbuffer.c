#include "headers/appendbuffer.h"
#include "headers/debug.h"
#include <stdlib.h>
#include <string.h>

void ab_append(struct ABuffer *ab, const char *s, int len) {
    char *new = (char*) realloc(ab->buffer, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->buffer = new;
    ab->len += len;
}

void ab_free(struct ABuffer *ab) {
    free(ab->buffer);
    memset(ab, 0, sizeof(*ab));
    DEBUG_PRINT("Freed ABuffer\r\n");
}
