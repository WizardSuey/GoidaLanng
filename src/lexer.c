/* lexer.c — токенизатор. Комментарии: #, //, блок --[[? нет: # и // и /' '*'/ */
#include "lexer.h"
#include "utf8.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lex_init(Lexer *L, const char *src, const char *file)
{
    L->src = src;
    L->len = strlen(src);
    L->pos = 0;
    L->line = 1;
    L->file = file;
}

void token_free(Token *t)
{
    free(t->text);
    free(t->str);
    t->text = NULL;
    t->str = NULL;
}

static int peekc(Lexer *L)
{
    return L->pos < L->len ? (unsigned char)L->src[L->pos] : -1;
}

static int nextc(Lexer *L)
{
    if (L->pos >= L->len) return -1;
    unsigned char c = (unsigned char)L->src[L->pos++];
    if (c == '\n') L->line++;
    return c;
}

typedef struct { const char *word; TokKind kind; } Kw;

static const Kw keywords[] = {
    { "пускай",     T_KW_LET },
    { "пусть",      T_KW_LET },
    { "перем",      T_KW_LET },
    { "постоянно",  T_KW_CONST },
    { "дело",       T_KW_FN },
    { "функция",    T_KW_FN },
    { "если",       T_KW_IF },
    { "иначе",      T_KW_ELSE },
    { "пока",       T_KW_WHILE },
    { "для",        T_KW_FOR },
    { "в",          T_KW_IN },
    { "цикл",       T_KW_LOOP },
    { "погнали",    T_KW_LOOP },
    { "прерви",     T_KW_BREAK },
    { "стоп",       T_KW_BREAK },
    { "тормози",    T_KW_BREAK },
    { "продолжи",   T_KW_CONTINUE },
    { "дальше",     T_KW_CONTINUE },
    { "верни",      T_KW_RETURN },
    { "возврат",    T_KW_RETURN },
    { "и",          T_KW_AND },
    { "или",        T_KW_OR },
    { "не",         T_KW_NOT },
    { "истина",     T_KW_TRUE },
    { "правда",     T_KW_TRUE },
    { "ложь",       T_KW_FALSE },
    { "пусто",      T_KW_NIL },
    { "ничто",      T_KW_NIL },
    { "подключить", T_KW_IMPORT },
    { "импорт",     T_KW_IMPORT },
    { "как",        T_KW_AS },
};
#define NKW (sizeof(keywords)/sizeof(keywords[0]))

static TokKind kw_lookup(const char *s, size_t len)
{
    for (size_t i = 0; i < NKW; i++) {
        if (strlen(keywords[i].word) == len &&
            memcmp(keywords[i].word, s, len) == 0)
            return keywords[i].kind;
    }
    return T_EOF; /* не ключевое */
}

int lex_next(Lexer *L, Token *out, char *err, size_t errsz)
{
    memset(out, 0, sizeof *out);
    for (;;) {
        int c = peekc(L);
        if (c == -1) { out->kind = T_EOF; out->line = L->line; return 0; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { nextc(L); continue; }
        if (c == '#') { while (peekc(L) != -1 && peekc(L) != '\n') nextc(L); continue; }
        if (c == '/' && L->pos + 1 < L->len && L->src[L->pos + 1] == '/') {
            while (peekc(L) != -1 && peekc(L) != '\n') nextc(L);
            continue;
        }
        if (c == '/' && L->pos + 1 < L->len && L->src[L->pos + 1] == '*') {
            int line = L->line;
            nextc(L); nextc(L);
            while (peekc(L) != -1) {
                if (peekc(L) == '*' && L->pos + 1 < L->len && L->src[L->pos + 1] == '/') {
                    nextc(L); nextc(L);
                    break;
                }
                nextc(L);
            }
            if (peekc(L) == -1) {
                snprintf(err, errsz, "%s:%d: незакрытый блочный комментарий", L->file, line);
                return -1;
            }
            continue;
        }
        break;
    }

    out->line = L->line;
    size_t start = L->pos;
    int c = nextc(L);

    /* числа */
    if (c >= '0' && c <= '9') {
        const char *s = L->src + start;
        size_t end = start;
        if (c == '0' && end < L->len && (L->src[end] == 'x' || L->src[end] == 'X')) {
            end++;
            while (end < L->len &&
                   ((L->src[end] >= '0' && L->src[end] <= '9') ||
                    (L->src[end] >= 'a' && L->src[end] <= 'f') ||
                    (L->src[end] >= 'A' && L->src[end] <= 'F')))
                end++;
            out->num = (double)strtoll(s + 2, NULL, 16);
        } else {
            end = start;
            while (end < L->len && (L->src[end] >= '0' && L->src[end] <= '9')) end++;
            if (end + 1 < L->len && L->src[end] == '.' && L->src[end+1] >= '0' && L->src[end+1] <= '9') {
                end++;
                while (end < L->len && (L->src[end] >= '0' && L->src[end] <= '9')) end++;
            }
            if (end < L->len && (L->src[end] == 'e' || L->src[end] == 'E')) {
                size_t e2 = end + 1;
                if (e2 < L->len && (L->src[e2] == '+' || L->src[e2] == '-')) e2++;
                if (e2 < L->len && L->src[e2] >= '0' && L->src[e2] <= '9') {
                    end = e2;
                    while (end < L->len && (L->src[end] >= '0' && L->src[end] <= '9')) end++;
                }
            }
            out->num = strtod(s, NULL);
        }
        L->pos = end;
        out->kind = T_NUM;
        return 0;
    }

    /* идентификаторы / ключевые слова (UTF-8) */
    if (utf8_is_ident_start(L->src + start, L->len - start)) {
        size_t end = start;
        while (end < L->len && utf8_is_ident_part(L->src + end, L->len - end)) {
            size_t w = utf8_charlen((unsigned char)L->src[end]);
            end += w ? w : 1;
        }
        L->pos = end;
        size_t len = end - start;
        TokKind kw = kw_lookup(L->src + start, len);
        if (kw != T_EOF) {
            out->kind = kw;
        } else {
            out->kind = T_IDENT;
            out->text = xstrndup(L->src + start, len);
        }
        return 0;
    }

    /* строки */
    if (c == '"') {
        Buf b; buf_init(&b);
        for (;;) {
            int ch = nextc(L);
            if (ch == -1 || ch == '\n') {
                buf_free(&b);
                snprintf(err, errsz, "%s:%d: незакрытая строка", L->file, out->line);
                return -1;
            }
            if (ch == '"') break;
            if (ch == '\\') {
                int e = nextc(L);
                switch (e) {
                    case 'n':  buf_addc(&b, '\n'); break;
                    case 't':  buf_addc(&b, '\t'); break;
                    case 'r':  buf_addc(&b, '\r'); break;
                    case '0':  buf_addc(&b, '\0'); break;
                    case '\\': buf_addc(&b, '\\'); break;
                    case '"':  buf_addc(&b, '"'); break;
                    case 'x': {
                        int h1 = nextc(L), h2 = nextc(L);
                        char hx[3] = { (char)h1, (char)h2, 0 };
                        buf_addc(&b, (char)strtol(hx, NULL, 16));
                        break;
                    }
                    default:
                        buf_addc(&b, '\\');
                        buf_addc(&b, (char)e);
                }
            } else {
                buf_addc(&b, (char)ch);
            }
        }
        out->kind = T_STR;
        out->slen = b.len;
        out->str = (char *)xmalloc(b.len + 1);
        memcpy(out->str, b.p, b.len);
        out->str[b.len] = '\0';
        buf_free(&b);
        return 0;
    }

    /* операторы и скобки */
    #define TWO(ch2, k2, k1) \
        if (peekc(L) == (ch2)) { nextc(L); out->kind = (k2); return 0; } \
        out->kind = (k1); return 0;

    switch (c) {
        case '(': out->kind = T_LPAR;   return 0;
        case ')': out->kind = T_RPAR;   return 0;
        case '{': out->kind = T_LBRACE; return 0;
        case '}': out->kind = T_RBRACE; return 0;
        case '[': out->kind = T_LBRACK; return 0;
        case ']': out->kind = T_RBRACK; return 0;
        case ',': out->kind = T_COMMA;  return 0;
        case '.': out->kind = T_DOT;    return 0;
        case ';': out->kind = T_SEMI;   return 0;
        case ':': out->kind = T_COLON;  return 0;
        case '+': TWO('=', T_PLUS_ASSIGN, T_PLUS)
        case '-': TWO('=', T_MINUS_ASSIGN, T_MINUS)
        case '*': TWO('=', T_STAR_ASSIGN, T_STAR)
        case '/': TWO('=', T_SLASH_ASSIGN, T_SLASH)
        case '%': out->kind = T_PERCENT; return 0;
        case '=': TWO('=', T_EQ, T_ASSIGN)
        case '!': TWO('=', T_NE, T_EOF)
        case '<': TWO('=', T_LE, T_LT)
        case '>': TWO('=', T_GE, T_GT)
    }
    #undef TWO

    if (out->kind == T_EOF) {
        snprintf(err, errsz, "%s:%d: неожиданный символ '%c'", L->file, out->line, c);
        return -1;
    }
    return 0;
}
