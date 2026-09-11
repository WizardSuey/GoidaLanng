/* ast.c — создание и освобождение узлов AST. */
#include "ast.h"
#include "util.h"

#include <stdlib.h>

Node *node_new(NKind k, int line)
{
    Node *n = (Node *)xcalloc(1, sizeof(Node));
    n->k = k;
    n->line = line;
    return n;
}

void fndecl_free(FnDecl *f)
{
    if (!f) return;
    free(f->name);
    for (int i = 0; i < f->nparams; i++) {
        free(f->params[i].name);
        ast_free(f->params[i].def);
    }
    free(f->params);
    ast_free(f->body);
    free(f);
}

Node *node_clone(Node *n)
{
    if (!n) return NULL;
    switch (n->k) {
        case N_IDENT: {
            Node *c = node_new(N_IDENT, n->line);
            c->u.name = xstrdup(n->u.name);
            return c;
        }
        case N_INDEX: {
            Node *c = node_new(N_INDEX, n->line);
            c->u.index.obj = node_clone(n->u.index.obj);
            /* индекс вычисляется один раз в исходном узле:
             * для составного присваивания клонируем только объект,
             * а индекс берём как есть (документировано) */
            c->u.index.index = node_clone(n->u.index.index);
            return c;
        }
        case N_FIELD: {
            Node *c = node_new(N_FIELD, n->line);
            c->u.field.obj = node_clone(n->u.field.obj);
            c->u.field.name = xstrdup(n->u.field.name);
            return c;
        }
        default: {
            Node *c = node_new(N_IDENT, n->line);
            c->u.name = xstrdup("<цель>");
            return c;
        }
    }
}

void ast_free(Node *n)
{
    if (!n) return;
    switch (n->k) {
        case N_NUM: case N_BOOL: case N_NIL: break;
        case N_STR:  free(n->u.str.p); break;
        case N_IDENT: free(n->u.name); break;
        case N_ARR:
            for (int i = 0; i < n->u.arr.n; i++) ast_free(n->u.arr.items[i]);
            free(n->u.arr.items);
            break;
        case N_MAP:
            for (int i = 0; i < n->u.map.n; i++) {
                free(n->u.map.keys[i]);
                ast_free(n->u.map.vals[i]);
            }
            free(n->u.map.keys);
            free(n->u.map.vals);
            break;
        case N_FN:
            fndecl_free(n->u.fn.fn);
            break;
        case N_UN:
            ast_free(n->u.un.e);
            break;
        case N_BIN:
            ast_free(n->u.bin.l);
            ast_free(n->u.bin.r);
            break;
        case N_ASSIGN:
            ast_free(n->u.assign.target);
            ast_free(n->u.assign.value);
            break;
        case N_INDEX:
            ast_free(n->u.index.obj);
            ast_free(n->u.index.index);
            break;
        case N_FIELD:
            ast_free(n->u.field.obj);
            free(n->u.field.name);
            break;
        case N_CALL:
            ast_free(n->u.call.callee);
            for (int i = 0; i < n->u.call.nargs; i++) ast_free(n->u.call.args[i]);
            free(n->u.call.args);
            break;
        case N_BLOCK:
            for (int i = 0; i < n->u.block.n; i++) ast_free(n->u.block.items[i]);
            free(n->u.block.items);
            break;
        case N_IF:
            ast_free(n->u.iff.cond);
            ast_free(n->u.iff.then_b);
            ast_free(n->u.iff.else_b);
            break;
        case N_WHILE:
            ast_free(n->u.whil.cond);
            ast_free(n->u.whil.body);
            break;
        case N_FOR:
            ast_free(n->u.fors.init);
            ast_free(n->u.fors.cond);
            ast_free(n->u.fors.step);
            ast_free(n->u.fors.body);
            break;
        case N_FOREACH:
            free(n->u.each.var);
            ast_free(n->u.each.iter);
            ast_free(n->u.each.body);
            break;
        case N_LOOP:
            ast_free(n->u.loopb.body);
            break;
        case N_BREAK: case N_CONTINUE: break;
        case N_RETURN:
            if (n->u.ret.has) ast_free(n->u.ret.value);
            break;
        case N_VAR:
            free(n->u.var.name);
            ast_free(n->u.var.value);
            break;
        case N_FNDECL:
            fndecl_free(n->u.fn.fn);
            break;
        case N_IMPORT:
            free(n->u.imp.path);
            free(n->u.imp.alias);
            break;
        case N_EXPR:
            ast_free(n->u.expr.e);
            break;
    }
    free(n);
}
