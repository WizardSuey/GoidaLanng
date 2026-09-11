/* module.c — менеджер модулей: встроенные, .goida-файлы, $GOIDA_PATH. */
#include "module.h"
#include "vm.h"
#include "stdlib.h"
#include "parser.h"
#include "compiler.h"
#include "gc.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Module *g_modules = NULL;

Module *module_find(const char *name)
{
    for (Module *m = g_modules; m; m = m->next)
        if (strcmp(m->name, name) == 0) return m;
    return NULL;
}

static void module_register(Module *m)
{
    m->next = g_modules;
    g_modules = m;
}

Module *module_new_script(Interp *I, const char *name, const char *path)
{
    Module *m = (Module *)xcalloc(1, sizeof(Module));
    m->name = xstrdup(name);
    m->path = path ? xstrdup(path) : NULL;
    table_init(&m->globals);
    std_core_install(I, &m->globals);
    module_register(m);
    return m;
}

void module_gc_mark(void)
{
    for (Module *m = g_modules; m; m = m->next) {
        table_mark(&m->globals);
        if (m->main_fn) gc_mark_val(m->main_fn);
        if (m->ns) gc_mark_val(m->ns);
    }
}

/* ---------------- встроенные модули ---------------- */

typedef struct {
    const char *n1, *n2;
    void (*install)(Interp *, Table *);
} NativeMod;

static const NativeMod native_mods[] = {
    { "стандарт", "std",      std_core_install },
    { "математика", "math",   std_math_install },
    { "строки",   "strings",  std_strings_install },
    { "массивы",  "arrays",   std_arrays_install },
    { "файлы",    "files",    std_files_install },
    { "время",    "time",     std_time_install },
    { "система",  "system",   std_system_install },
    { "случай",   "random",   std_random_install },
    { "сокеты",   "sockets",  sock_install },
};
#define NNATIVE (sizeof(native_mods)/sizeof(native_mods[0]))

static Module *load_native(Interp *I, const char *name)
{
    for (size_t i = 0; i < NNATIVE; i++) {
        if (strcmp(native_mods[i].n1, name) != 0 &&
            (!native_mods[i].n2 || strcmp(native_mods[i].n2, name) != 0))
            continue;
        Module *m = (Module *)xcalloc(1, sizeof(Module));
        m->name = xstrdup(name);
        table_init(&m->globals);
        native_mods[i].install(I, &m->globals);
        m->ns = table_to_map(&m->globals);
        module_register(m);
        return m;
    }
    return NULL;
}

/* ---------------- файловые модули ---------------- */

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* Собирает список кандидатов пути в buf (malloc), возвращает число. */
static char *resolve_module_path(Interp *I, Module *from, const char *spec)
{
    (void)I;
    char cand[4096];
    const char *variants[2] = { spec, NULL };
    char withext[4096];
    if (!strstr(spec, ".goida")) {
        snprintf(withext, sizeof withext, "%s.goida", spec);
        variants[1] = withext;
    }

    /* 1) каталог импортирующего файла */
    if (from && from->path) {
        char dir[2048];
        snprintf(dir, sizeof dir, "%s", from->path);
        char *sl = strrchr(dir, '/');
        if (sl) { *sl = '\0';
            for (int v = 0; v < 2 && variants[v]; v++) {
                snprintf(cand, sizeof cand, "%s/%s", dir, variants[v]);
                if (file_exists(cand)) return xstrdup(cand);
            }
        }
    }
    /* 2) текущий каталог */
    for (int v = 0; v < 2 && variants[v]; v++) {
        snprintf(cand, sizeof cand, "./%s", variants[v]);
        if (file_exists(cand)) return xstrdup(cand);
    }
    /* 3) $GOIDA_PATH */
    const char *gp = getenv("GOIDA_PATH");
    if (gp) {
        char *copy = xstrdup(gp);
        char *save = NULL;
        for (char *dir = strtok_r(copy, ":", &save); dir;
             dir = strtok_r(NULL, ":", &save)) {
            for (int v = 0; v < 2 && variants[v]; v++) {
                snprintf(cand, sizeof cand, "%s/%s", dir, variants[v]);
                if (file_exists(cand)) {
                    char *r = xstrdup(cand);
                    free(copy);
                    return r;
                }
            }
        }
        free(copy);
    }
    return NULL;
}

static Val *load_file_module(Interp *I, const char *path)
{
    size_t len;
    char *src = read_file(path, &len);
    if (!src) return NULL;

    Parser P;
    if (parser_init(&P, src, path) != 0) {
        free(src);
        rt_error(I, 0, "%s", P.errmsg);
    }
    Node *ast = parse_program(&P);
    if (!ast) {
        char msg[600];
        snprintf(msg, sizeof msg, "%s", P.errmsg);
        free(src);
        rt_error(I, 0, "%s", msg);
    }
    char err[512];
    Function *fn = compile(path, ast, err, sizeof err);
    ast_free(ast);
    free(src);
    if (!fn) rt_error(I, 0, "%s", err);

    /* имя модуля из базы файла */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    Module *m = module_new_script(I, base, path);
    m->main_fn = v_fn(fn);

    vm_run(I, m);
    m->ns = table_to_map(&m->globals);
    return m->ns;
}

Val *module_load(Interp *I, Module *from, const char *spec)
{
    Module *m = module_find(spec);
    if (m) {
        if (m->ns) return m->ns;
        return table_to_map(&m->globals);
    }
    m = load_native(I, spec);
    if (m) return m->ns;

    char *path = resolve_module_path(I, from, spec);
    if (!path)
        rt_error(I, 0, "модуль '%s' не найден (ищи в ., каталоге скрипта и $GOIDA_PATH)", spec);
    Val *ns = load_file_module(I, path);
    free(path);
    if (!ns) rt_error(I, 0, "модуль '%s': не удалось прочитать файл", spec);
    return ns;
}
