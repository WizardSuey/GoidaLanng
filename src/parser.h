/* parser.h — рекурсивно-нисходящий парсер AST. */
#ifndef GOIDA_PARSER_H
#define GOIDA_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer L;
    Token cur;
    int   errored;
    int   errline;
    char  errmsg[512];
} Parser;

/* 0 — лексер инициализирован. */
int   parser_init(Parser *P, const char *src, const char *file);
/* Корень N_BLOCK или NULL при ошибке (текст — в P->errmsg). */
Node *parse_program(Parser *P);

#endif /* GOIDA_PARSER_H */
