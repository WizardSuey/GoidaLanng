/* std_arrays.c — модуль «массивы»: сортировка, map/filter/reduce и пр. */
#include "stdlib.h"
#include "gc.h"
#include "native.h"
#include "utf8.h"

#include <stdlib.h>
#include <string.h>

static Val *n_переверни(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "переверни", 0);
    Val *r = v_arr();
    gc_add_root_slot(&r);
    for (size_t i = a->u.a.len; i > 0; i--)
        arr_push(r, a->u.a.v[i - 1]);
    gc_remove_root_slot(&r);
    return r;
}

static Val *n_срежь(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "срежь", 0);
    long long n = (long long)a->u.a.len;
    long long s = argc > 1 ? nat_int(I, argc, argv, 1, "срежь", 0) : 0;
    long long e = argc > 2 ? nat_int(I, argc, argv, 2, "срежь", 0) : n;
    if (s < 0) s += n;
    if (e < 0) e += n;
    if (s < 0) s = 0;
    if (e > n) e = n;
    Val *r = v_arr();
    gc_add_root_slot(&r);
    for (long long i = s; i < e; i++) arr_push(r, a->u.a.v[i]);
    gc_remove_root_slot(&r);
    return r;
}

static Val *n_включает(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "включает", 0);
    nat_min(I, argc, 2, "включает", 0);
    for (size_t i = 0; i < a->u.a.len; i++)
        if (val_equals(a->u.a.v[i], argv[1])) return v_bool(1);
    return v_bool(0);
}

static Val *n_позиция(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "позиция", 0);
    nat_min(I, argc, 2, "позиция", 0);
    for (size_t i = 0; i < a->u.a.len; i++)
        if (val_equals(a->u.a.v[i], argv[1])) return v_int((long long)i);
    return v_int(-1);
}

/* сравнение для сортировки: числа, затем строки */
static int cmp_val(const void *pa, const void *pb)
{
    Val *a = *(Val *const *)pa;
    Val *b = *(Val *const *)pb;
    if ((a->t == V_INT || a->t == V_FLOAT) && (b->t == V_INT || b->t == V_FLOAT)) {
        double x = a->t == V_INT ? (double)a->u.i : a->u.f;
        double y = b->t == V_INT ? (double)b->u.i : b->u.f;
        return x < y ? -1 : x > y ? 1 : 0;
    }
    if (a->t == V_STR && b->t == V_STR)
        return strcmp(a->u.s.p, b->u.s.p);
    return 0;
}

static Val *n_сортируй(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "сортируй", 0);
    Val *r = v_arr();
    gc_add_root_slot(&r);
    for (size_t i = 0; i < a->u.a.len; i++) arr_push(r, a->u.a.v[i]);
    if (r->u.a.len > 1)
        qsort(r->u.a.v, r->u.a.len, sizeof(Val *), cmp_val);
    gc_remove_root_slot(&r);
    return r;
}

/* сортируй_по(массив, дело(эл) -> ключ) */
static Val *n_сортируй_по(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "сортируй_по", 0);
    nat_min(I, argc, 2, "сортируй_по", 0);
    Val *fn = argv[1];
    Val *pairs = v_arr();   /* [ [ключ, эл], ... ] */
    gc_add_root_slot(&pairs);
    for (size_t i = 0; i < a->u.a.len; i++) {
        Val *key = vm_call(I, fn, 1, &a->u.a.v[i], 0);
        Val *pair = v_arr();
        arr_push(pair, key);
        arr_push(pair, a->u.a.v[i]);
        arr_push(pairs, pair);
    }
    if (pairs->u.a.len > 1)
        qsort(pairs->u.a.v, pairs->u.a.len, sizeof(Val *), cmp_val);
    Val *r = v_arr();
    gc_add_root_slot(&r);
    for (size_t i = 0; i < pairs->u.a.len; i++)
        arr_push(r, pairs->u.a.v[i]->u.a.v[1]);
    gc_remove_root_slot(&r);
    gc_remove_root_slot(&pairs);
    return r;
}

static Val *n_отобрази(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "отобрази", 0);
    nat_min(I, argc, 2, "отобрази", 0);
    Val *fn = argv[1];
    Val *r = v_arr();
    gc_add_root_slot(&r);
    for (size_t i = 0; i < a->u.a.len; i++)
        arr_push(r, vm_call(I, fn, 1, &a->u.a.v[i], 0));
    gc_remove_root_slot(&r);
    return r;
}

static Val *n_отфильтруй(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "отфильтруй", 0);
    nat_min(I, argc, 2, "отфильтруй", 0);
    Val *fn = argv[1];
    Val *r = v_arr();
    gc_add_root_slot(&r);
    for (size_t i = 0; i < a->u.a.len; i++) {
        Val *keep = vm_call(I, fn, 1, &a->u.a.v[i], 0);
        if (truthy(keep)) arr_push(r, a->u.a.v[i]);
    }
    gc_remove_root_slot(&r);
    return r;
}

static Val *n_сверни(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "сверни", 0);
    nat_min(I, argc, 3, "сверни", 0);
    Val *fn = argv[1];
    Val *acc = argv[2];
    for (size_t i = 0; i < a->u.a.len; i++) {
        Val *args[2] = { acc, a->u.a.v[i] };
        acc = vm_call(I, fn, 2, args, 0);
    }
    return acc;
}

static Val *n_каждый(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "каждый", 0);
    nat_min(I, argc, 2, "каждый", 0);
    Val *fn = argv[1];
    for (size_t i = 0; i < a->u.a.len; i++)
        vm_call(I, fn, 1, &a->u.a.v[i], 0);
    return v_nil();
}

static Val *n_сумма(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "сумма", 0);
    int all_int = 1;
    double sum = 0;
    long long isum = 0;
    for (size_t i = 0; i < a->u.a.len; i++) {
        Val *v = a->u.a.v[i];
        if (v->t == V_INT) { isum += v->u.i; sum += (double)v->u.i; }
        else if (v->t == V_FLOAT) { all_int = 0; sum += v->u.f; }
        else rt_error(I, 0, "сумма: элемент %zu не число", i);
    }
    return all_int ? v_int(isum) : v_float(sum);
}

void std_arrays_install(Interp *I, Table *t)
{
    (void)I;
    table_set(t, "переверни",  v_native("переверни",  n_переверни));
    table_set(t, "срежь",      v_native("срежь",      n_срежь));
    table_set(t, "включает",   v_native("включает",   n_включает));
    table_set(t, "позиция",    v_native("позиция",    n_позиция));
    table_set(t, "сортируй",   v_native("сортируй",   n_сортируй));
    table_set(t, "сортируй_по", v_native("сортируй_по", n_сортируй_по));
    table_set(t, "отобрази",   v_native("отобрази",   n_отобрази));
    table_set(t, "отфильтруй", v_native("отфильтруй", n_отфильтруй));
    table_set(t, "сверни",     v_native("сверни",     n_сверни));
    table_set(t, "каждый",     v_native("каждый",     n_каждый));
    table_set(t, "сумма",      v_native("сумма",      n_сумма));
}
