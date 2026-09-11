/* parser.c — рекурсивно-нисходящий парсер. */
#include "parser.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parser_init(Parser *P, const char *src, const char *file)
{
    lex_init(&P->L, src, file);
    memset(&P->cur, 0, sizeof P->cur);
    P->cur.kind = T_EOF;
    P->errored = 0;
    P->errmsg[0] = '\0';
    char err[256];
    if (lex_next(&P->L, &P->cur, err, sizeof err) != 0) {
        P->errored = 1;
        snprintf(P->errmsg, sizeof P->errmsg, "%s", err);
        return -1;
    }
    return 0;
}

static Token advance(Parser *P)
{
    Token prev = P->cur;
    char err[256];
    if (lex_next(&P->L, &P->cur, err, sizeof err) != 0) {
        P->errored = 1;
        P->errline = P->L.line;
        snprintf(P->errmsg, sizeof P->errmsg, "%s", err);
        memset(&P->cur, 0, sizeof P->cur);
        P->cur.kind = T_EOF;
        P->cur.line = P->L.line;
    }
    return prev;
}

/* двухтокеновый lookahead: следующий за cur */
static Token lookahead(Parser *P)
{
    Token save = P->cur;
    Token next;
    char err[256];
    /* временно продвинуться */
    P->cur = save;
    Lexer saveL = P->L;
    if (lex_next(&P->L, &next, err, sizeof err) != 0) {
        memset(&next, 0, sizeof next);
        next.kind = T_EOF;
    }
    P->L = saveL;
    token_free(&next);   /* только kind нужен */
    return next;
}

static int check(Parser *P, TokKind k)
{
    return P->cur.kind == k;
}

static int match(Parser *P, TokKind k)
{
    if (check(P, k)) {
        Token t = advance(P);
        token_free(&t);
        return 1;
    }
    return 0;
}

static void perr(Parser *P, const char *msg)
{
    if (!P->errored) {
        P->errored = 1;
        P->errline = P->cur.line;
        snprintf(P->errmsg, sizeof P->errmsg, "%s:%d: %s",
                 P->L.file, P->cur.line, msg);
    }
}

static int expect(Parser *P, TokKind k, const char *msg)
{
    if (check(P, k)) {
        Token t = advance(P);
        token_free(&t);
        return 1;
    }
    perr(P, msg);
    return 0;
}

/* ---------------- выражения ---------------- */

static Node *parse_expr(Parser *P);
static Node *parse_block(Parser *P);
static Node *parse_stmt(Parser *P);

static Node *parse_primary(Parser *P)
{
    if (P->errored) return NULL;
    Token t = P->cur;

    switch (t.kind) {
        case T_NUM: {
            advance(P);
            Node *n = node_new(N_NUM, t.line);
            n->u.num = t.num;
            return n;
        }
        case T_STR: {
            advance(P);
            Node *n = node_new(N_STR, t.line);
            n->u.str.p = t.str;
            n->u.str.len = t.slen;
            return n;
        }
        case T_KW_TRUE:
        case T_KW_FALSE: {
            advance(P);
            Node *n = node_new(N_BOOL, t.line);
            n->u.boolean = (t.kind == T_KW_TRUE);
            return n;
        }
        case T_KW_NIL: {
            advance(P);
            return node_new(N_NIL, t.line);
        }
        case T_IDENT: {
            advance(P);
            Node *n = node_new(N_IDENT, t.line);
            n->u.name = t.text;
            return n;
        }
        case T_LPAR: {
            advance(P);
            Node *e = parse_expr(P);
            expect(P, T_RPAR, "ожидалась ')'");
            return e;
        }
        case T_LBRACK: {
            advance(P);
            Node *n = node_new(N_ARR, t.line);
            int cap = 0;
            if (!check(P, T_RBRACK)) {
                for (;;) {
                    Node *e = parse_expr(P);
                    if (n->u.arr.n == cap) {
                        cap = cap ? cap * 2 : 4;
                        n->u.arr.items = (Node **)xrealloc(n->u.arr.items, sizeof(Node *) * (size_t)cap);
                    }
                    n->u.arr.items[n->u.arr.n++] = e;
                    if (!match(P, T_COMMA)) break;
                    if (P->errored) break;
                }
            }
            expect(P, T_RBRACK, "ожидался ']'");
            return n;
        }
        case T_LBRACE: {
            advance(P);
            Node *n = node_new(N_MAP, t.line);
            int cap = 0;
            if (!check(P, T_RBRACE)) {
                for (;;) {
                    char *key = NULL;
                    if (check(P, T_IDENT)) {
                        Token kt = advance(P);
                        key = kt.text;
                    } else if (check(P, T_STR)) {
                        Token kt = advance(P);
                        key = kt.str;
                    } else {
                        perr(P, "ожидался ключ словаря");
                        return n;
                    }
                    expect(P, T_COLON, "ожидалось ':'");
                    Node *val = parse_expr(P);
                    if (n->u.map.n == cap) {
                        cap = cap ? cap * 2 : 4;
                        n->u.map.keys = (char **)xrealloc(n->u.map.keys, sizeof(char *) * (size_t)cap);
                        n->u.map.vals = (Node **)xrealloc(n->u.map.vals, sizeof(Node *) * (size_t)cap);
                    }
                    n->u.map.keys[n->u.map.n] = key;
                    n->u.map.vals[n->u.map.n] = val;
                    n->u.map.n++;
                    if (!match(P, T_COMMA)) break;
                    if (P->errored) break;
                }
            }
            expect(P, T_RBRACE, "ожидался '}'");
            return n;
        }
        case T_KW_FN: {
            advance(P);
            /* анонимная функция: дело (a, b) { ... } */
            FnDecl *f = (FnDecl *)xcalloc(1, sizeof(FnDecl));
            f->name = xstrdup("<аноним>");
            expect(P, T_LPAR, "ожидался '(' после 'дело'");
            int cap = 0;
            if (!check(P, T_RPAR)) {
                for (;;) {
                    if (!check(P, T_IDENT)) { perr(P, "ожидался параметр"); break; }
                    Token pt = advance(P);
                    Node *def = NULL;
                    if (match(P, T_ASSIGN)) def = parse_expr(P);
                    if (f->nparams == cap) {
                        cap = cap ? cap * 2 : 4;
                        f->params = (Param *)xrealloc(f->params, sizeof(Param) * (size_t)cap);
                    }
                    f->params[f->nparams].name = pt.text;
                    f->params[f->nparams].def = def;
                    f->nparams++;
                    if (!match(P, T_COMMA)) break;
                    if (P->errored) break;
                }
            }
            expect(P, T_RPAR, "ожидался ')'");
            f->body = parse_block(P);
            Node *n = node_new(N_FN, t.line);
            n->u.fn.fn = f;
            return n;
        }
        default:
            perr(P, "ожидалось выражение");
            return NULL;
    }
}

static Node *parse_postfix(Parser *P)
{
    Node *e = parse_primary(P);
    if (!e) return NULL;
    for (;;) {
        if (P->errored) return e;
        if (check(P, T_DOT)) {
            Token dot = advance(P);
            if (!check(P, T_IDENT)) {
                perr(P, "ожидалось имя после '.'");
                token_free(&dot);
                return e;
            }
            Token name = advance(P);
            Node *f = node_new(N_FIELD, dot.line);
            f->u.field.obj = e;
            f->u.field.name = name.text;
            e = f;
            token_free(&dot);
            if (check(P, T_LPAR)) {
                /* вызов метода: obj.name(args) */
                Token lp = advance(P);
                Node *c = node_new(N_CALL, lp.line);
                c->u.call.callee = e;
                c->u.call.is_method = 1;
                int cap = 0;
                if (!check(P, T_RPAR)) {
                    for (;;) {
                        Node *a = parse_expr(P);
                        if (c->u.call.nargs == cap) {
                            cap = cap ? cap * 2 : 4;
                            c->u.call.args = (Node **)xrealloc(c->u.call.args, sizeof(Node *) * (size_t)cap);
                        }
                        c->u.call.args[c->u.call.nargs++] = a;
                        if (!match(P, T_COMMA)) break;
                        if (P->errored) break;
                    }
                }
                expect(P, T_RPAR, "ожидался ')'");
                e = c;
            }
        } else if (check(P, T_LBRACK)) {
            Token lb = advance(P);
            Node *idx = parse_expr(P);
            expect(P, T_RBRACK, "ожидался ']'");
            Node *n = node_new(N_INDEX, lb.line);
            n->u.index.obj = e;
            n->u.index.index = idx;
            e = n;
        } else if (check(P, T_LPAR)) {
            Token lp = advance(P);
            Node *c = node_new(N_CALL, lp.line);
            c->u.call.callee = e;
            c->u.call.is_method = 0;
            int cap = 0;
            if (!check(P, T_RPAR)) {
                for (;;) {
                    Node *a = parse_expr(P);
                    if (c->u.call.nargs == cap) {
                        cap = cap ? cap * 2 : 4;
                        c->u.call.args = (Node **)xrealloc(c->u.call.args, sizeof(Node *) * (size_t)cap);
                    }
                    c->u.call.args[c->u.call.nargs++] = a;
                    if (!match(P, T_COMMA)) break;
                    if (P->errored) break;
                }
            }
            expect(P, T_RPAR, "ожидался ')'");
            e = c;
        } else {
            break;
        }
    }
    return e;
}

static Node *parse_unary(Parser *P);

static Node *parse_factor(Parser *P)
{
    Node *l = parse_unary(P);
    for (;;) {
        if (P->errored) return l;
        Op op;
        if (check(P, T_STAR)) op = A_MUL;
        else if (check(P, T_SLASH)) op = A_DIV;
        else if (check(P, T_PERCENT)) op = A_MOD;
        else break;
        Token t = advance(P);
        Node *r = parse_unary(P);
        Node *n = node_new(N_BIN, t.line);
        n->u.bin.op = op;
        n->u.bin.l = l;
        n->u.bin.r = r;
        l = n;
    }
    return l;
}

static Node *parse_term(Parser *P)
{
    Node *l = parse_factor(P);
    for (;;) {
        if (P->errored) return l;
        Op op;
        if (check(P, T_PLUS)) op = A_ADD;
        else if (check(P, T_MINUS)) op = A_SUB;
        else break;
        Token t = advance(P);
        Node *r = parse_factor(P);
        Node *n = node_new(N_BIN, t.line);
        n->u.bin.op = op;
        n->u.bin.l = l;
        n->u.bin.r = r;
        l = n;
    }
    return l;
}

static Node *parse_comparison(Parser *P)
{
    Node *l = parse_term(P);
    for (;;) {
        if (P->errored) return l;
        Op op;
        if (check(P, T_LT)) op = A_LT;
        else if (check(P, T_LE)) op = A_LE;
        else if (check(P, T_GT)) op = A_GT;
        else if (check(P, T_GE)) op = A_GE;
        else break;
        Token t = advance(P);
        Node *r = parse_term(P);
        Node *n = node_new(N_BIN, t.line);
        n->u.bin.op = op;
        n->u.bin.l = l;
        n->u.bin.r = r;
        l = n;
    }
    return l;
}

static Node *parse_equality(Parser *P)
{
    Node *l = parse_comparison(P);
    for (;;) {
        if (P->errored) return l;
        Op op;
        if (check(P, T_EQ)) op = A_EQ;
        else if (check(P, T_NE)) op = A_NE;
        else break;
        Token t = advance(P);
        Node *r = parse_comparison(P);
        Node *n = node_new(N_BIN, t.line);
        n->u.bin.op = op;
        n->u.bin.l = l;
        n->u.bin.r = r;
        l = n;
    }
    return l;
}

static Node *parse_unary(Parser *P)
{
    if (check(P, T_MINUS) || check(P, T_KW_NOT)) {
        Token t = advance(P);
        Node *e = parse_unary(P);
        Node *n = node_new(N_UN, t.line);
        n->u.un.op = (t.kind == T_MINUS) ? A_NEG : A_NOT;
        n->u.un.e = e;
        return n;
    }
    return parse_postfix(P);
}

static Node *parse_and(Parser *P)
{
    Node *l = parse_equality(P);
    while (check(P, T_KW_AND)) {
        Token t = advance(P);
        Node *r = parse_equality(P);
        Node *n = node_new(N_BIN, t.line);
        n->u.bin.op = A_AND;
        n->u.bin.l = l;
        n->u.bin.r = r;
        l = n;
    }
    return l;
}

static Node *parse_or(Parser *P)
{
    Node *l = parse_and(P);
    while (check(P, T_KW_OR)) {
        Token t = advance(P);
        Node *r = parse_and(P);
        Node *n = node_new(N_BIN, t.line);
        n->u.bin.op = A_OR;
        n->u.bin.l = l;
        n->u.bin.r = r;
        l = n;
    }
    return l;
}

Node *node_clone(Node *n);

static Node *parse_assignment(Parser *P)
{
    Node *l = parse_or(P);
    if (P->errored) return l;

    TokKind k = P->cur.kind;
    int compound = (k == T_ASSIGN || k == T_PLUS_ASSIGN || k == T_MINUS_ASSIGN ||
                    k == T_STAR_ASSIGN || k == T_SLASH_ASSIGN);
    if (!compound) return l;

    Token t = advance(P);
    Node *value = parse_assignment(P);   /* правоассоциативность */

    if (k != T_ASSIGN) {
        Op op = (k == T_PLUS_ASSIGN) ? A_ADD :
                (k == T_MINUS_ASSIGN) ? A_SUB :
                (k == T_STAR_ASSIGN) ? A_MUL : A_DIV;
        Node *bin = node_new(N_BIN, t.line);
        bin->u.bin.op = op;
        bin->u.bin.l = node_clone(l);
        bin->u.bin.r = value;
        value = bin;
    }

    if (l->k != N_IDENT && l->k != N_INDEX && l->k != N_FIELD) {
        perr(P, "некорректная цель присваивания");
        return l;
    }
    Node *n = node_new(N_ASSIGN, t.line);
    n->u.assign.target = l;
    n->u.assign.value = value;
    return n;
}

static Node *parse_expr(Parser *P)
{
    return parse_assignment(P);
}

/* ---------------- операторы ---------------- */

static Node *parse_block(Parser *P)
{
    Token t = P->cur;
    if (!expect(P, T_LBRACE, "ожидался '{'")) return NULL;
    Node *b = node_new(N_BLOCK, t.line);
    int cap = 0;
    while (!check(P, T_RBRACE) && !check(P, T_EOF)) {
        Node *s = parse_stmt(P);
        if (!s) break;
        if (b->u.block.n == cap) {
            cap = cap ? cap * 2 : 8;
            b->u.block.items = (Node **)xrealloc(b->u.block.items, sizeof(Node *) * (size_t)cap);
        }
        b->u.block.items[b->u.block.n++] = s;
        if (P->errored) break;
    }
    expect(P, T_RBRACE, "ожидался '}'");
    return b;
}

static Node *parse_if(Parser *P)   /* 'если' уже съедено */
{
    int line = P->cur.line;
    Node *n = node_new(N_IF, line);
    n->u.iff.cond = parse_expr(P);
    n->u.iff.then_b = parse_block(P);
    if (match(P, T_KW_ELSE)) {
        if (check(P, T_KW_IF)) {
            advance(P);
            n->u.iff.else_b = parse_if(P);
        } else {
            n->u.iff.else_b = parse_block(P);
        }
    }
    return n;
}

static Node *parse_for(Parser *P)  /* 'для' уже съедено */
{
    int line = P->cur.line;
    expect(P, T_LPAR, "ожидался '('");

    /* foreach: для (x в expr) или для (пускай x в expr) */
    int had_let = 0;
    if (check(P, T_KW_LET)) { advance(P); had_let = 1; }
    (void)had_let;
    if (check(P, T_IDENT) && lookahead(P).kind == T_KW_IN) {
        Token vt = advance(P);
        advance(P);   /* 'в' */
        Node *n = node_new(N_FOREACH, line);
        n->u.each.var = vt.text;
        n->u.each.iter = parse_expr(P);
        expect(P, T_RPAR, "ожидался ')'");
        n->u.each.body = parse_block(P);
        return n;
    }

    Node *n = node_new(N_FOR, line);
    if (had_let) {
        /* «для (пускай i = 0; ...)» */
        if (!check(P, T_IDENT)) { perr(P, "ожидалось имя"); return n; }
        Token nt = advance(P);
        Node *v = node_new(N_VAR, nt.line);
        v->u.var.name = nt.text;
        v->u.var.is_const = 0;
        if (match(P, T_ASSIGN)) v->u.var.value = parse_expr(P);
        n->u.fors.init = v;
    } else if (!check(P, T_SEMI)) {
        n->u.fors.init = node_new(N_EXPR, line);
        n->u.fors.init->u.expr.e = parse_expr(P);
    }
    expect(P, T_SEMI, "ожидалась ';'");
    if (!check(P, T_SEMI)) n->u.fors.cond = parse_expr(P);
    expect(P, T_SEMI, "ожидалась ';'");
    if (!check(P, T_RPAR)) n->u.fors.step = parse_expr(P);
    expect(P, T_RPAR, "ожидался ')'");
    n->u.fors.body = parse_block(P);
    return n;
}

static Node *parse_fn_decl(Parser *P, int line)  /* 'дело' уже съедено */
{
    if (!check(P, T_IDENT)) { perr(P, "ожидалось имя дела"); return NULL; }
    Token nt = advance(P);
    FnDecl *f = (FnDecl *)xcalloc(1, sizeof(FnDecl));
    f->name = nt.text;
    expect(P, T_LPAR, "ожидался '('");
    int cap = 0;
    if (!check(P, T_RPAR)) {
        for (;;) {
            if (!check(P, T_IDENT)) { perr(P, "ожидался параметр"); break; }
            Token pt = advance(P);
            Node *def = NULL;
            if (match(P, T_ASSIGN)) def = parse_expr(P);
            if (f->nparams == cap) {
                cap = cap ? cap * 2 : 4;
                f->params = (Param *)xrealloc(f->params, sizeof(Param) * (size_t)cap);
            }
            f->params[f->nparams].name = pt.text;
            f->params[f->nparams].def = def;
            f->nparams++;
            if (!match(P, T_COMMA)) break;
            if (P->errored) break;
        }
    }
    expect(P, T_RPAR, "ожидался ')'");
    f->body = parse_block(P);
    Node *n = node_new(N_FNDECL, line);
    n->u.fn.fn = f;
    return n;
}

static Node *parse_import(Parser *P, int line)
{
    Node *n = node_new(N_IMPORT, line);
    if (check(P, T_STR)) {
        Token t = advance(P);
        n->u.imp.path = t.str;
    } else if (check(P, T_IDENT)) {
        Token t = advance(P);
        n->u.imp.path = t.text;
    } else {
        perr(P, "ожидалось имя или путь модуля");
        return n;
    }
    if (match(P, T_KW_AS)) {
        if (!check(P, T_IDENT)) { perr(P, "ожидалось имя-псевдоним"); return n; }
        Token a = advance(P);
        n->u.imp.alias = a.text;
    }
    return n;
}

static Node *parse_stmt(Parser *P)
{
    if (P->errored) return NULL;
    Token t = P->cur;

    switch (t.kind) {
        case T_KW_LET: {
            advance(P);
            if (!check(P, T_IDENT)) { perr(P, "ожидалось имя переменной"); return NULL; }
            Token nt = advance(P);
            Node *n = node_new(N_VAR, nt.line);
            n->u.var.name = nt.text;
            n->u.var.is_const = 0;
            if (match(P, T_ASSIGN)) n->u.var.value = parse_expr(P);
            match(P, T_SEMI);
            return n;
        }
        case T_KW_CONST: {
            advance(P);
            if (!check(P, T_IDENT)) { perr(P, "ожидалось имя константы"); return NULL; }
            Token nt = advance(P);
            Node *n = node_new(N_VAR, nt.line);
            n->u.var.name = nt.text;
            n->u.var.is_const = 1;
            if (match(P, T_ASSIGN)) n->u.var.value = parse_expr(P);
            else perr(P, "константа требует значения");
            match(P, T_SEMI);
            return n;
        }
        case T_KW_FN: {
            advance(P);
            return parse_fn_decl(P, t.line);
        }
        case T_KW_IF: {
            advance(P);
            return parse_if(P);
        }
        case T_KW_WHILE: {
            advance(P);
            Node *n = node_new(N_WHILE, t.line);
            n->u.whil.cond = parse_expr(P);
            n->u.whil.body = parse_block(P);
            return n;
        }
        case T_KW_FOR: {
            advance(P);
            return parse_for(P);
        }
        case T_KW_LOOP: {
            advance(P);
            Node *n = node_new(N_LOOP, t.line);
            n->u.loopb.body = parse_block(P);
            return n;
        }
        case T_KW_BREAK: {
            advance(P);
            match(P, T_SEMI);
            return node_new(N_BREAK, t.line);
        }
        case T_KW_CONTINUE: {
            advance(P);
            match(P, T_SEMI);
            return node_new(N_CONTINUE, t.line);
        }
        case T_KW_RETURN: {
            advance(P);
            Node *n = node_new(N_RETURN, t.line);
            n->u.ret.has = 0;
            if (!check(P, T_SEMI) && !check(P, T_RBRACE) && !check(P, T_EOF)) {
                n->u.ret.has = 1;
                n->u.ret.value = parse_expr(P);
            }
            match(P, T_SEMI);
            return n;
        }
        case T_KW_IMPORT: {
            advance(P);
            Node *n = parse_import(P, t.line);
            match(P, T_SEMI);
            return n;
        }
        case T_LBRACE:
            return parse_block(P);
        default: {
            Node *e = parse_expr(P);
            if (!e) return NULL;
            Node *n = node_new(N_EXPR, t.line);
            n->u.expr.e = e;
            match(P, T_SEMI);
            return n;
        }
    }
}

Node *parse_program(Parser *P)
{
    Node *root = node_new(N_BLOCK, 1);
    int cap = 0;
    while (!check(P, T_EOF) && !P->errored) {
        Node *s = parse_stmt(P);
        if (!s) break;
        if (root->u.block.n == cap) {
            cap = cap ? cap * 2 : 16;
            root->u.block.items = (Node **)xrealloc(root->u.block.items, sizeof(Node *) * (size_t)cap);
        }
        root->u.block.items[root->u.block.n++] = s;
    }
    if (P->errored) {
        ast_free(root);
        return NULL;
    }
    return root;
}
