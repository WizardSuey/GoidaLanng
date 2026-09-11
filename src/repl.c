/* repl.c — REPL: читает строку, балансирует скобки, печатает результат
 * одиночного выражения. Глобалы живут между строками.
 */
#include "repl.h"
#include "module.h"
#include "parser.h"
#include "compiler.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_line(char *buf, size_t sz)
{
    if (!fgets(buf, (int)sz, stdin)) return 0;
    size_t l = strlen(buf);
    while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = '\0';
    return 1;
}

/* незакрытых скобок/строк */
static int unbalanced(const char *s)
{
    int depth = 0;
    int in_str = 0, in_comment = 0;
    for (const char *p = s; *p; p++) {
        if (in_comment) { if (*p == '\n') in_comment = 0; continue; }
        if (in_str) {
            if (*p == '\\') { if (p[1]) p++; }
            else if (*p == '"') in_str = 0;
            continue;
        }
        if (*p == '#') { in_comment = 1; continue; }
        if (*p == '/' && p[1] == '/') { in_comment = 1; continue; }
        if (*p == '"') { in_str = 1; continue; }
        if (*p == '(' || *p == '{' || *p == '[') depth++;
        if (*p == ')' || *p == '}' || *p == ']') depth--;
    }
    return depth > 0 || in_str;
}

static char *read_multiline(void)
{
    char line[8192];
    Buf b; buf_init(&b);
    if (!read_line(line, sizeof line)) { buf_free(&b); return NULL; }
    buf_adds(&b, line);
    while (unbalanced(b.p)) {
        fputs(".... ", stdout);
        fflush(stdout);
        if (!read_line(line, sizeof line)) break;
        buf_addc(&b, '\n');
        buf_adds(&b, line);
    }
    return buf_take(&b);
}

void repl_run(Interp *I)
{
    Module *mod = module_new_script(I, "<интерактив>", NULL);
    I->repl_mode = 1;
    fputs("GoidaLang " "0.1.0" " — гойда! (Ctrl-D — выход)\n", stdout);

    for (;;) {
        fputs("goida> ", stdout);
        fflush(stdout);
        char *src = read_multiline();
        if (!src) { fputc('\n', stdout); break; }
        if (!src[0]) { free(src); continue; }

        Parser P;
        if (parser_init(&P, src, "<repl>") != 0) {
            fprintf(stderr, "ГОЙДА! %s\n", P.errmsg);
            free(src);
            continue;
        }
        Node *ast = parse_program(&P);
        if (!ast) {
            fprintf(stderr, "ГОЙДА! %s\n", P.errmsg);
            free(src);
            continue;
        }

        /* одиночное выражение печатаем */
        volatile int single_expr = (ast->u.block.n == 1 &&
                           ast->u.block.items[0]->k == N_EXPR);
        if (single_expr) {
            Node *stmt = ast->u.block.items[0];
            Node *e = stmt->u.expr.e;
            stmt->u.expr.e = NULL;   /* e переезжает в N_RETURN */
            ast_free(stmt);
            Node *ret = node_new(N_RETURN, e->line);
            ret->u.ret.has = 1;
            ret->u.ret.value = e;
            ast->u.block.items[0] = ret;
        }

        char err[512];
        Function *fn = compile("<repl>", ast, err, sizeof err);
        ast_free(ast);
        free(src);
        if (!fn) {
            fprintf(stderr, "ГОЙДА! %s\n", err);
            continue;
        }
        mod->main_fn = v_fn(fn);

        jmp_buf saved;
        int armed = I->err_armed;
        memcpy(saved, I->err_jmp, sizeof(jmp_buf));
        I->err_armed = 1;
        if (setjmp(I->err_jmp) == 0) {
            Val *res = vm_run(I, mod);
            if (single_expr && res && res->t != V_NIL) {
                char *s = val_to_str(res);
                fputs(s, stdout);
                fputc('\n', stdout);
                free(s);
            }
        } else {
            fprintf(stderr, "ГОЙДА! Ошибка: %s%s\n", I->errmsg, I->trace);
        }
        I->err_armed = armed;
        memcpy(I->err_jmp, saved, sizeof(jmp_buf));
    }
}
