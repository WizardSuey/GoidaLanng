/* native.c — проверки аргументов встроенных функций. */
#include "native.h"

void nat_min(Interp *I, int argc, int need, const char *fn, int line)
{
    if (argc < need)
        rt_error(I, line, "%s: аргументов %d, нужно не меньше %d", fn, argc, need);
}

Val *nat_val(Interp *I, int argc, Val **argv, int i, const char *fn, int line)
{
    nat_min(I, argc, i + 1, fn, line);
    return argv[i];
}

long long nat_int(Interp *I, int argc, Val **argv, int i, const char *fn, int line)
{
    Val *v = nat_val(I, argc, argv, i, fn, line);
    if (v->t == V_INT) return v->u.i;
    if (v->t == V_FLOAT) return (long long)v->u.f;
    rt_error(I, line, "%s: аргумент %d должен быть целым, а не %s", fn, i + 1, type_name(v));
    return 0;
}

double nat_num(Interp *I, int argc, Val **argv, int i, const char *fn, int line)
{
    Val *v = nat_val(I, argc, argv, i, fn, line);
    if (v->t == V_INT) return (double)v->u.i;
    if (v->t == V_FLOAT) return v->u.f;
    rt_error(I, line, "%s: аргумент %d должен быть числом, а не %s", fn, i + 1, type_name(v));
    return 0;
}

const char *nat_str(Interp *I, int argc, Val **argv, int i, const char *fn, int line)
{
    Val *v = nat_val(I, argc, argv, i, fn, line);
    if (v->t != V_STR)
        rt_error(I, line, "%s: аргумент %d должен быть строкой, а не %s", fn, i + 1, type_name(v));
    return v->u.s.p;
}

Val *nat_arr(Interp *I, int argc, Val **argv, int i, const char *fn, int line)
{
    Val *v = nat_val(I, argc, argv, i, fn, line);
    if (v->t != V_ARR)
        rt_error(I, line, "%s: аргумент %d должен быть массивом, а не %s", fn, i + 1, type_name(v));
    return v;
}

Val *nat_map(Interp *I, int argc, Val **argv, int i, const char *fn, int line)
{
    Val *v = nat_val(I, argc, argv, i, fn, line);
    if (v->t != V_MAP)
        rt_error(I, line, "%s: аргумент %d должен быть словарём, а не %s", fn, i + 1, type_name(v));
    return v;
}

int nat_bool(Interp *I, int argc, Val **argv, int i, const char *fn, int line)
{
    return truthy(nat_val(I, argc, argv, i, fn, line));
}
