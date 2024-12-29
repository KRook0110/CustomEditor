#ifndef _APPEND_BUFFER_H
#define _APPEND_BUFFER_H

/* Append Buffer */
struct ABuffer {
    char *buffer;
    int len;
};
#define ABUF_INIT {NULL, 0}
void ab_append(struct ABuffer *ab, const char *s, int len);
void ab_free(struct ABuffer *ab);


#endif
