/* main.c — точка входа интерпретатора GoidaLang. */
#include "vm.h"
#include "module.h"
#include "parser.h"
#include "compiler.h"
#include "repl.h"
#include "gc.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GOIDA_VERSION_STRING
#define GOIDA_VERSION_STRING "0.1.0"
#endif

static void usage(FILE *out)
{
    fputs(
        "GoidaLang " GOIDA_VERSION_STRING "\n"
        "Использование:\n"
        "  goida файл.goida [аргументы...]   выполнить скрипт\n"
        "  goida                             интерактивный режим\n"
        "  goida -e 'код'                    выполнить строку кода\n"
        "  goida --версия | --version        версия\n"
        "  goida --помощь | --help           эта справка\n",
        out);
}

static int run_source(Interp *I, const char *src, const char *name, const char *path)
{
    Parser P;
    if (parser_init(&P, src, name) != 0) {
        fprintf(I->err, "ГОЙДА! %s\n", P.errmsg);
        return 1;
    }
    Node *ast = parse_program(&P);
    if (!ast) {
        fprintf(I->err, "ГОЙДА! %s\n", P.errmsg);
        return 1;
    }
    char cerr_[512];
    Function *fn = compile(name, ast, cerr_, sizeof cerr_);
    ast_free(ast);
    if (!fn) {
        fprintf(I->err, "ГОЙДА! %s\n", cerr_);
        return 1;
    }
    Module *m = module_new_script(I, name, path);
    m->main_fn = v_fn(fn);

    I->err_armed = 1;
    if (setjmp(I->err_jmp) == 0) {
        vm_run(I, m);
        return 0;
    }
    fprintf(I->err, "ГОЙДА! Ошибка: %s%s\n", I->errmsg, I->trace);
    return 1;
}

int main(int argc, char **argv)
{
    gc_init();
    Interp I;
    vm_init(&I, argc, argv);

    if (argc < 2) {
        repl_run(&I);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "--помощь") == 0) {
        usage(stdout);
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "--версия") == 0) {
        puts(GOIDA_VERSION_STRING);
        return 0;
    }

    if (strcmp(argv[1], "-e") == 0) {
        if (argc < 3) {
            fputs("goida: после -e нужен код\n", stderr);
            return 2;
        }
        I.argc = argc - 3;
        I.argv = argv + 3;
        return run_source(&I, argv[2], "<-e>", NULL);
    }

    const char *path = argv[1];
    size_t len;
    char *src = read_file(path, &len);
    if (!src) {
        fprintf(stderr, "goida: не удалось открыть '%s'\n", path);
        return 2;
    }
    I.argc = argc - 2;
    I.argv = argv + 2;
    int rc = run_source(&I, src, path, path);
    free(src);
    return rc;
}
