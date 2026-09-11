/* std_system.c — модуль «система»: аргументы, выход, окружение. */
#include "stdlib.h"
#include "native.h"

#include <stdlib.h>
#include <string.h>

static Val *n_аргументы(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self; (void)argc; (void)argv;
    Val *a = v_arr();
    for (int i = 0; i < I->argc; i++)
        arr_push(a, v_str(I->argv[i]));
    return a;
}

static Val *n_выйди(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    long long code = argc ? nat_int(I, argc, argv, 0, "выйди", 0) : 0;
    exit((int)code);
}

static Val *n_окружение(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    const char *name = nat_str(I, argc, argv, 0, "окружение", 0);
    const char *v = getenv(name);
    return v ? v_str(v) : v_nil();
}

static Val *n_выполни(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    const char *cmd = nat_str(I, argc, argv, 0, "выполни", 0);
    int rc = system(cmd);
    return v_int(rc);
}

static Val *n_платформа(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self; (void)argc; (void)argv;
#ifdef _WIN32
    return v_str("windows");
#else
    return v_str("posix");
#endif
}

void std_system_install(Interp *I, Table *t)
{
    (void)I;
    table_set(t, "аргументы",  v_native("аргументы", n_аргументы));
    table_set(t, "выйди",      v_native("выйди", n_выйди));
    table_set(t, "окружение",  v_native("окружение", n_окружение));
    table_set(t, "выполни",    v_native("выполни", n_выполни));
    table_set(t, "платформа",  v_native("платформа", n_платформа));
}
