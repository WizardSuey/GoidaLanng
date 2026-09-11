/* std_strings.c — модуль «строки» (UTF-8 корректно). */
#include "stdlib.h"
#include "gc.h"
#include "native.h"
#include "util.h"
#include "utf8.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static Val *n_вверх(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    const char *s = nat_str(I, argc, argv, 0, "регистр_вверх", 0);
    Val *v = nat_val(I, argc, argv, 0, "регистр_вверх", 0);
    Buf b; buf_init(&b);
    size_t i = 0;
    while (i < v->u.s.len) {
        uint32_t cp;
        size_t w = utf8_decode(s + i, v->u.s.len - i, &cp);
        if (cp >= 0x430 && cp <= 0x44F) cp += 0x410 - 0x430;   /* а-я -> А-Я */
        else if (cp >= 'a' && cp <= 'z') cp += 'A' - 'a';
        else if (cp == 0x451) cp = 0x401;                       /* ё -> Ё */
        char tmp[4];
        buf_add(&b, tmp, utf8_encode(cp, tmp));
        i += w;
    }
    Val *r = v_strn(b.p, b.len);
    buf_free(&b);
    return r;
}

static Val *n_вниз(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    const char *s = nat_str(I, argc, argv, 0, "регистр_вниз", 0);
    Val *v = nat_val(I, argc, argv, 0, "регистр_вниз", 0);
    Buf b; buf_init(&b);
    size_t i = 0;
    while (i < v->u.s.len) {
        uint32_t cp;
        size_t w = utf8_decode(s + i, v->u.s.len - i, &cp);
        if (cp >= 0x410 && cp <= 0x42F) cp += 0x430 - 0x410;   /* А-Я -> а-я */
        else if (cp >= 'A' && cp <= 'Z') cp += 'a' - 'A';
        else if (cp == 0x401) cp = 0x451;                       /* Ё -> ё */
        char tmp[4];
        buf_add(&b, tmp, utf8_encode(cp, tmp));
        i += w;
    }
    Val *r = v_strn(b.p, b.len);
    buf_free(&b);
    return r;
}

static Val *n_раздели(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *sv = nat_val(I, argc, argv, 0, "раздели", 0);
    const char *sep = argc > 1 ? nat_str(I, argc, argv, 1, "раздели", 0) : " ";
    Val *a = v_arr();
    gc_add_root_slot(&a);
    const char *p = sv->u.s.p;
    size_t len = sv->u.s.len, sl = strlen(sep);
    if (sl == 0) {   /* по символам */
        size_t i = 0;
        while (i < len) {
            size_t w = utf8_charlen((unsigned char)p[i]);
            if (w == 0 || i + w > len) w = 1;
            arr_push(a, v_strn(p + i, w));
            i += w;
        }
        gc_remove_root_slot(&a);
        return a;
    }
    size_t start = 0;
    while (start <= len) {
        const char *f = NULL;
        if (start < len)
            f = memmem(p + start, len - start, sep, sl);
        if (!f) {
            arr_push(a, v_strn(p + start, len - start));
            break;
        }
        (void)0;
        size_t off = (size_t)(f - p);
        arr_push(a, v_strn(p + start, off - start));
        start = off + sl;
        if (f + sl == p + len) { arr_push(a, v_str("")); break; }
    }
    gc_remove_root_slot(&a);
    return a;
}

static Val *n_соедини(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "соедини", 0);
    const char *sep = argc > 1 ? nat_str(I, argc, argv, 1, "соедини", 0) : "";
    Buf b; buf_init(&b);
    for (size_t i = 0; i < a->u.a.len; i++) {
        if (i) buf_adds(&b, sep);
        char *s = val_to_str(a->u.a.v[i]);
        buf_adds(&b, s);
        free(s);
    }
    Val *r = v_strn(b.p, b.len);
    buf_free(&b);
    return r;
}

static Val *n_найди(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *sv = nat_val(I, argc, argv, 0, "найди", 0);
    const char *needle = nat_str(I, argc, argv, 1, "найди", 0);
    const char *f = memmem(sv->u.s.p, sv->u.s.len, needle, strlen(needle));
    if (!f) return v_int(-1);
    return v_int((long long)utf8_strlen(sv->u.s.p, (size_t)(f - sv->u.s.p)));
}

static Val *n_содержит(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *sv = nat_val(I, argc, argv, 0, "содержит", 0);
    const char *needle = nat_str(I, argc, argv, 1, "содержит", 0);
    return v_bool(memmem(sv->u.s.p, sv->u.s.len, needle, strlen(needle)) != NULL);
}

static Val *n_подрежь(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *sv = nat_val(I, argc, argv, 0, "подрежь", 0);
    long long n = (long long)utf8_strlen(sv->u.s.p, sv->u.s.len);
    long long a = argc > 1 ? nat_int(I, argc, argv, 1, "подрежь", 0) : 0;
    long long b = argc > 2 ? nat_int(I, argc, argv, 2, "подрежь", 0) : n;
    if (a < 0) a += n;
    if (b < 0) b += n;
    if (a < 0) a = 0;
    if (b > n) b = n;
    if (a >= b) return v_str("");
    size_t off = utf8_offset(sv->u.s.p, sv->u.s.len, (size_t)a);
    size_t end = utf8_offset(sv->u.s.p, sv->u.s.len, (size_t)b);
    return v_strn(sv->u.s.p + off, end - off);
}

static Val *n_обрежь(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *sv = nat_val(I, argc, argv, 0, "обрежь", 0);
    size_t i = 0, j = sv->u.s.len;
    while (i < j && isspace((unsigned char)sv->u.s.p[i])) i++;
    while (j > i && isspace((unsigned char)sv->u.s.p[j - 1])) j--;
    return v_strn(sv->u.s.p + i, j - i);
}

static Val *n_замени(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *sv = nat_val(I, argc, argv, 0, "замени", 0);
    const char *from = nat_str(I, argc, argv, 1, "замени", 0);
    const char *to = nat_str(I, argc, argv, 2, "замени", 0);
    size_t fl = strlen(from);
    if (fl == 0) return sv;
    Buf b; buf_init(&b);
    const char *p = sv->u.s.p;
    size_t len = sv->u.s.len, start = 0;
    while (start < len) {
        const char *f = memmem(p + start, len - start, from, fl);
        if (!f) { buf_add(&b, p + start, len - start); break; }
        buf_add(&b, p + start, (size_t)(f - p) - start);
        buf_adds(&b, to);
        start = (size_t)(f - p) + fl;
    }
    Val *r = v_strn(b.p, b.len);
    buf_free(&b);
    return r;
}

void std_strings_install(Interp *I, Table *t)
{
    (void)I;
    table_set(t, "регистр_вверх", v_native("регистр_вверх", n_вверх));
    table_set(t, "регистр_вниз",  v_native("регистр_вниз",  n_вниз));
    table_set(t, "раздели",  v_native("раздели",  n_раздели));
    table_set(t, "соедини",  v_native("соедини",  n_соедини));
    table_set(t, "найди",    v_native("найди",    n_найди));
    table_set(t, "содержит", v_native("содержит", n_содержит));
    table_set(t, "подрежь",  v_native("подрежь",  n_подрежь));
    table_set(t, "обрежь",   v_native("обрежь",   n_обрежь));
    table_set(t, "замени",   v_native("замени",   n_замени));
}
