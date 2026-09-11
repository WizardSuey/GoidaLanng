/* std_random.c — модуль «случай». */
#include "stdlib.h"
#include "native.h"

#include <stdlib.h>
#include <time.h>

static int seeded = 0;

static void ensure_seed(void)
{
    if (!seeded) {
        srand((unsigned)time(NULL) ^ (unsigned)(size_t)&seeded);
        seeded = 1;
    }
}

static Val *n_сид(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    long long s = nat_int(I, argc, argv, 0, "сид", 0);
    srand((unsigned)s);
    seeded = 1;
    return v_nil();
}

static Val *n_целое(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    ensure_seed();
    long long a = nat_int(I, argc, argv, 0, "случайное_целое", 0);
    long long b = argc > 1 ? nat_int(I, argc, argv, 1, "случайное_целое", 0) : a;
    if (a > b) { long long t = a; a = b; b = t; }
    long long span = b - a + 1;
    return v_int(a + (long long)(rand() % span));
}

static Val *n_дробное(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self; (void)argc; (void)argv;
    ensure_seed();
    return v_float((double)rand() / ((double)RAND_MAX + 1.0));
}

static Val *n_выбор(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    ensure_seed();
    Val *a = nat_arr(I, argc, argv, 0, "выбор", 0);
    if (a->u.a.len == 0) rt_error(I, 0, "выбор: массив пуст");
    return a->u.a.v[rand() % a->u.a.len];
}

void std_random_install(Interp *I, Table *t)
{
    (void)I;
    table_set(t, "сид",            v_native("сид", n_сид));
    table_set(t, "случайное_целое", v_native("случайное_целое", n_целое));
    table_set(t, "случайное_дробное", v_native("случайное_дробное", n_дробное));
    table_set(t, "выбор",          v_native("выбор", n_выбор));
}
