/* util.h — аллокаторы и динамический байтовый буфер. */
#ifndef GOIDA_UTIL_H
#define GOIDA_UTIL_H

#include <stddef.h>

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* Читает файл целиком. Возвращает malloc'нутый буфер (с завершающим NUL)
 * или NULL, если файл недоступен. *out_len — длина без NUL. */
char *read_file(const char *path, size_t *out_len);

typedef struct {
    char  *p;
    size_t len, cap;
} Buf;

void  buf_init(Buf *b);
void  buf_free(Buf *b);
void  buf_reserve(Buf *b, size_t extra);
void  buf_addc(Buf *b, char c);
void  buf_add(Buf *b, const char *s, size_t n);
void  buf_adds(Buf *b, const char *s);
void  buf_addf(Buf *b, const char *fmt, ...);
/* Отдаёт владение буфером наружу; вызывающий обязан вызвать free(). */
char *buf_take(Buf *b);

#endif /* GOIDA_UTIL_H */
