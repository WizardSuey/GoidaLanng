/* lexer.h — токенизатор GoidaLang (русские ключевые слова, UTF-8). */
#ifndef GOIDA_LEXER_H
#define GOIDA_LEXER_H

#include <stddef.h>
#include "forward.h"

typedef enum {
    T_EOF, T_NUM, T_STR, T_IDENT,
    T_LPAR, T_RPAR, T_LBRACE, T_RBRACE, T_LBRACK, T_RBRACK,
    T_COMMA, T_DOT, T_SEMI, T_COLON,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_ASSIGN, T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,
    T_PLUS_ASSIGN, T_MINUS_ASSIGN, T_STAR_ASSIGN, T_SLASH_ASSIGN,
    /* ключевые слова */
    T_KW_LET, T_KW_CONST, T_KW_FN, T_KW_IF, T_KW_ELSE,
    T_KW_WHILE, T_KW_FOR, T_KW_IN, T_KW_LOOP,
    T_KW_BREAK, T_KW_CONTINUE, T_KW_RETURN,
    T_KW_AND, T_KW_OR, T_KW_NOT,
    T_KW_TRUE, T_KW_FALSE, T_KW_NIL,
    T_KW_IMPORT, T_KW_AS
} TokKind;

typedef struct {
    TokKind kind;
    int     line;
    double  num;
    char   *text;   /* для T_IDENT */
    char   *str;    /* для T_STR (malloc, может содержать NUL) */
    size_t  slen;
} Token;

typedef struct {
    const char *src;
    size_t      pos, len;
    int         line;
    const char *file;
} Lexer;

void lex_init(Lexer *L, const char *src, const char *file);
/* 0 — токен получен, -1 — ошибка (текст в err). */
int  lex_next(Lexer *L, Token *out, char *err, size_t errsz);
void token_free(Token *t);

#endif /* GOIDA_LEXER_H */
