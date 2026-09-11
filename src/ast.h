/* ast.h — узлы синтаксического дерева. */
#ifndef GOIDA_AST_H
#define GOIDA_AST_H

#include <stddef.h>
#include "forward.h"

typedef enum {
    A_ADD, A_SUB, A_MUL, A_DIV, A_MOD,
    A_EQ, A_NE, A_LT, A_LE, A_GT, A_GE,
    A_AND, A_OR, A_NOT, A_NEG
} Op;

typedef enum {
    N_NUM, N_STR, N_BOOL, N_NIL, N_IDENT,
    N_ARR, N_MAP, N_FN,
    N_UN, N_BIN, N_ASSIGN, N_INDEX, N_FIELD, N_CALL,
    N_BLOCK, N_IF, N_WHILE, N_FOR, N_FOREACH, N_LOOP,
    N_BREAK, N_CONTINUE, N_RETURN, N_VAR, N_FNDECL, N_IMPORT, N_EXPR
} NKind;

typedef struct {
    char *name;
    Node *def;      /* значение по умолчанию или NULL */
} Param;

struct FnDecl {
    char  *name;
    Param *params;
    int    nparams;
    Node  *body;    /* N_BLOCK */
};

struct Node {
    NKind k;
    int   line;
    union {
        double num;
        struct { char *p; size_t len; } str;
        int    boolean;
        char  *name;
        struct { Node **items; int n; } arr;
        struct { char **keys; Node **vals; int n; } map;
        struct { FnDecl *fn; } fn;
        struct { Op op; Node *e; } un;
        struct { Op op; Node *l, *r; } bin;
        struct { Node *target, *value; } assign;
        struct { Node *obj, *index; } index;
        struct { Node *obj; char *name; } field;
        struct { Node *callee; Node **args; int nargs;
                 int is_method; } call;
        struct { Node **items; int n; } block;
        struct { Node *cond, *then_b, *else_b; } iff;
        struct { Node *cond, *body; } whil;
        struct { Node *init, *cond, *step, *body; } fors;
        struct { char *var; Node *iter, *body; } each;
        struct { Node *body; } loopb;
        struct { Node *value; int has; } ret;
        struct { char *name; Node *value; int is_const; } var;
        struct { char *path; char *alias; } imp;
        struct { Node *e; } expr;
    } u;
};

Node *node_new(NKind k, int line);
void  ast_free(Node *n);
void  fndecl_free(FnDecl *f);
/* Клон цели присваивания (IDENT/INDEX/FIELD) — для составных «+=». */
Node *node_clone(Node *n);

#endif /* GOIDA_AST_H */
