/* util.c — аллокаторы и динамический буфер. */
#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void oom(void)
{
    fputs("goida: закончилась память\n", stderr);
    abort();
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) oom();
    return p;
}

void *xcalloc(size_t n, size_t sz)
{
    void *p = calloc(n ? n : 1, sz ? sz : 1);
    if (!p) oom();
    return p;
}

void *xrealloc(void *p, size_t n)
{
    void *r = realloc(p, n ? n : 1);
    if (!r) oom();
    return r;
}

char *xstrdup(const char *s)
{
    return xstrndup(s, strlen(s));
}

char *xstrndup(const char *s, size_t n)
{
    char *r = (char *)xmalloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

char *read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)xmalloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

void buf_init(Buf *b)
{
    b->p   = (char *)xmalloc(64);
    b->p[0] = '\0';
    b->len = 0;
    b->cap = 64;
}

void buf_free(Buf *b)
{
    free(b->p);
    b->p = NULL;
    b->len = b->cap = 0;
}

void buf_reserve(Buf *b, size_t extra)
{
    size_t need = b->len + extra + 1;
    if (need <= b->cap) return;
    size_t cap = b->cap;
    while (cap < need) cap *= 2;
    b->p = (char *)xrealloc(b->p, cap);
    b->cap = cap;
}

void buf_addc(Buf *b, char c)
{
    buf_reserve(b, 1);
    b->p[b->len++] = c;
    b->p[b->len] = '\0';
}

void buf_add(Buf *b, const char *s, size_t n)
{
    buf_reserve(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

void buf_adds(Buf *b, const char *s)
{
    buf_add(b, s, strlen(s));
}

void buf_addf(Buf *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) return;
    buf_reserve(b, (size_t)need);
    va_start(ap, fmt);
    vsnprintf(b->p + b->len, (size_t)need + 1, fmt, ap);
    va_end(ap);
    b->len += (size_t)need;
}

char *buf_take(Buf *b)
{
    char *r = b->p;
    b->p = NULL;
    b->len = b->cap = 0;
    return r;
}
