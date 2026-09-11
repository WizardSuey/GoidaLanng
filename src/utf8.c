/* utf8.c — реализация UTF-8 хелперов. */
#include "utf8.h"

size_t utf8_charlen(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 0; /* continuation-байт или невалидный */
}

size_t utf8_decode(const char *s, size_t len, uint32_t *cp)
{
    if (len == 0) return 0;
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) { *cp = c0; return 1; }

    size_t n = utf8_charlen(c0);
    if (n == 0 || n > len) { *cp = 0xFFFD; return 1; }

    uint32_t v;
    switch (n) {
        case 2: v = c0 & 0x1Fu; break;
        case 3: v = c0 & 0x0Fu; break;
        default: v = c0 & 0x07u; break;
    }
    for (size_t i = 1; i < n; i++) {
        unsigned char ci = (unsigned char)s[i];
        if ((ci & 0xC0) != 0x80) { *cp = 0xFFFD; return 1; }
        v = (v << 6) | (uint32_t)(ci & 0x3Fu);
    }
    /* проверка на overlong и суррогаты */
    static const uint32_t minv[5] = { 0, 0, 0x80, 0x800, 0x10000 };
    if (v < minv[n] || (v >= 0xD800 && v <= 0xDFFF) || v > 0x10FFFF) {
        *cp = 0xFFFD;
        return 1;
    }
    *cp = v;
    return n;
}

size_t utf8_encode(uint32_t cp, char *out)
{
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

int utf8_is_ident_start(const char *s, size_t len)
{
    if (len == 0) return 0;
    unsigned char c = (unsigned char)s[0];
    if (c >= 0x80) return 1;                 /* кириллица и всё прочее */
    if (c == '_') return 1;
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    return 0;
}

int utf8_is_ident_part(const char *s, size_t len)
{
    if (utf8_is_ident_start(s, len)) return 1;
    if (len == 0) return 0;
    unsigned char c = (unsigned char)s[0];
    return c >= '0' && c <= '9';
}

size_t utf8_strlen(const char *s, size_t len)
{
    size_t n = 0, i = 0;
    while (i < len) {
        size_t w = utf8_charlen((unsigned char)s[i]);
        if (w == 0 || i + w > len) { i++; }
        else { i += w; }
        n++;
    }
    return n;
}

size_t utf8_offset(const char *s, size_t len, size_t idx)
{
    size_t i = 0, n = 0;
    while (i < len && n < idx) {
        size_t w = utf8_charlen((unsigned char)s[i]);
        if (w == 0 || i + w > len) i++;
        else i += w;
        n++;
    }
    return i;
}
