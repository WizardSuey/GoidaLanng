/* value.c — конструкторы значений и работа с массивами/словарями. */
#include "value.h"
#include "gc.h"
#include "util.h"
#include "utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Val *v_nil(void)
{
    static Val nil = { .t = V_NIL, .marked = 1 };
    return &nil;
}

Val *v_bool(int b)
{
    static Val t = { .t = V_BOOL, .marked = 1, .u = { .b = 1 } };
    static Val f = { .t = V_BOOL, .marked = 1, .u = { .b = 0 } };
    return b ? &t : &f;
}

Val *v_int(long long i)
{
    Val *v = gc_alloc(V_INT);
    v->u.i = i;
    return v;
}

Val *v_float(double f)
{
    Val *v = gc_alloc(V_FLOAT);
    v->u.f = f;
    return v;
}

Val *v_strn(const char *s, size_t len)
{
    Val *v = gc_alloc(V_STR);
    v->u.s.p = xstrndup(s, len);
    v->u.s.len = len;
    return v;
}

Val *v_str(const char *s)
{
    return v_strn(s, strlen(s));
}

Val *v_arr(void)
{
    Val *v = gc_alloc(V_ARR);
    v->u.a.cap = 8;
    v->u.a.len = 0;
    v->u.a.v = (Val **)xmalloc(sizeof(Val *) * 8);
    return v;
}

Val *v_map(void)
{
    Val *v = gc_alloc(V_MAP);
    v->u.m.nb = 8;
    v->u.m.count = 0;
    v->u.m.head = v->u.m.tail = NULL;
    v->u.m.bkt = (MapEntry **)xcalloc(8, sizeof(MapEntry *));
    return v;
}

Val *v_fn(Function *f)
{
    Val *v = gc_alloc(V_FN);
    v->u.fun.f = f;
    return v;
}

Val *v_closure(Function *f, int upcount)
{
    Val *v = gc_alloc(V_CLOSURE);
    v->u.cl.fn = f;
    v->u.cl.upcount = upcount;
    v->u.cl.ups = upcount ? (Val **)xcalloc((size_t)upcount, sizeof(Val *)) : NULL;
    return v;
}

Val *v_upvalue(Val **slot)
{
    Val *v = gc_alloc(V_UPVALUE);
    v->u.up.loc = slot;
    v->u.up.closed = v_nil();
    v->u.up.next = NULL;
    return v;
}

Val *v_native(const char *name, NativeFn fn)
{
    Val *v = gc_alloc(V_NATIVE);
    v->u.nat.name = name;
    v->u.nat.fn = fn;
    v->u.nat.with_self = 0;
    return v;
}

Val *v_obj(const char *klass, void *data, void (*fin)(void *))
{
    Val *v = gc_alloc(V_OBJ);
    v->u.o.klass = klass;
    v->u.o.data = data;
    v->u.o.fin = fin;
    return v;
}

int truthy(Val *v)
{
    if (v->t == V_NIL) return 0;
    if (v->t == V_BOOL) return v->u.b;
    return 1;
}

int val_equals(Val *a, Val *b)
{
    if (a->t != b->t) {
        /* int == float сравниваем численно */
        if ((a->t == V_INT && b->t == V_FLOAT) || (a->t == V_FLOAT && b->t == V_INT)) {
            double x = a->t == V_INT ? (double)a->u.i : a->u.f;
            double y = b->t == V_INT ? (double)b->u.i : b->u.f;
            return x == y;
        }
        return 0;
    }
    switch (a->t) {
        case V_NIL:  return 1;
        case V_BOOL: return a->u.b == b->u.b;
        case V_INT:  return a->u.i == b->u.i;
        case V_FLOAT:return a->u.f == b->u.f;
        case V_STR:
            return a->u.s.len == b->u.s.len &&
                   memcmp(a->u.s.p, b->u.s.p, a->u.s.len) == 0;
        default:     return a == b;
    }
}

const char *type_name(Val *v)
{
    switch (v->t) {
        case V_NIL:    return "пусто";
        case V_BOOL:   return "логическое";
        case V_INT:    return "целое";
        case V_FLOAT:  return "дробное";
        case V_STR:    return "строка";
        case V_ARR:    return "массив";
        case V_MAP:    return "словарь";
        case V_FN:     return "функция";
        case V_CLOSURE:return "функция";
        case V_UPVALUE:return "<upvalue>";
        case V_NATIVE: return "встроенная";
        case V_OBJ:    return v->u.o.klass;
    }
    return "?";
}

const char *val_cstr(Val *v)
{
    return v->u.s.p;
}

static void to_buf(Buf *b, Val *v, int repr, int depth);

static void str_repr(Buf *b, const char *p, size_t len)
{
    buf_addc(b, '"');
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)p[i];
        switch (c) {
            case '\n': buf_adds(b, "\\n"); break;
            case '\t': buf_adds(b, "\\t"); break;
            case '\r': buf_adds(b, "\\r"); break;
            case '"':  buf_adds(b, "\\\""); break;
            case '\\': buf_adds(b, "\\\\"); break;
            default:
                if (c < 0x20) buf_addf(b, "\\x%02x", c);
                else buf_addc(b, (char)c);
        }
    }
    buf_addc(b, '"');
}

static void to_buf(Buf *b, Val *v, int repr, int depth)
{
    if (depth > 8) { buf_adds(b, "..."); return; }
    switch (v->t) {
        case V_NIL:  buf_adds(b, "пусто"); break;
        case V_BOOL: buf_adds(b, v->u.b ? "истина" : "ложь"); break;
        case V_INT:  buf_addf(b, "%lld", v->u.i); break;
        case V_FLOAT: {
            char tmp[40];
            snprintf(tmp, sizeof tmp, "%.14g", v->u.f);
            buf_adds(b, tmp);
            if (!strpbrk(tmp, ".eE")) buf_adds(b, ".0");
            break;
        }
        case V_STR:
            if (repr) str_repr(b, v->u.s.p, v->u.s.len);
            else buf_add(b, v->u.s.p, v->u.s.len);
            break;
        case V_ARR: {
            buf_addc(b, '[');
            for (size_t i = 0; i < v->u.a.len; i++) {
                if (i) buf_adds(b, ", ");
                to_buf(b, v->u.a.v[i], 1, depth + 1);
            }
            buf_addc(b, ']');
            break;
        }
        case V_MAP: {
            buf_addc(b, '{');
            MapEntry *e = v->u.m.head;
            int first = 1;
            for (; e; e = e->next_o) {
                if (!first) buf_adds(b, ", ");
                first = 0;
                str_repr(b, e->key, strlen(e->key));
                buf_adds(b, ": ");
                to_buf(b, e->val, 1, depth + 1);
            }
            buf_addc(b, '}');
            break;
        }
        case V_FN:      buf_addf(b, "<дело %s>", v->u.fun.f->name); break;
        case V_CLOSURE: buf_addf(b, "<дело %s>", v->u.cl.fn->name); break;
        case V_NATIVE:  buf_addf(b, "<встроенная %s>", v->u.nat.name); break;
        case V_UPVALUE: buf_adds(b, "<upvalue>"); break;
        case V_OBJ:     buf_addf(b, "<%s>", v->u.o.klass); break;
    }
}

char *val_to_str(Val *v)
{
    Buf b; buf_init(&b);
    to_buf(&b, v, 0, 0);
    return buf_take(&b);
}

char *val_repr(Val *v)
{
    Buf b; buf_init(&b);
    to_buf(&b, v, 1, 0);
    return buf_take(&b);
}

/* ------------------------- массивы ------------------------- */

void arr_push(Val *arr, Val *v)
{
    if (arr->u.a.len == arr->u.a.cap) {
        arr->u.a.cap *= 2;
        arr->u.a.v = (Val **)xrealloc(arr->u.a.v, sizeof(Val *) * arr->u.a.cap);
    }
    arr->u.a.v[arr->u.a.len++] = v;
}

Val *arr_get(Val *arr, long long idx)
{
    long long n = (long long)arr->u.a.len;
    if (idx < 0) idx += n;
    if (idx < 0 || idx >= n) return NULL;
    return arr->u.a.v[idx];
}

/* ------------------------- словари ------------------------- */

static size_t hash_key(const char *s)
{
    size_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

static void map_rehash(Val *m, size_t nb)
{
    MapEntry **nbkt = (MapEntry **)xcalloc(nb, sizeof(MapEntry *));
    for (MapEntry *e = m->u.m.head; e; e = e->next_o) {
        size_t h = hash_key(e->key) & (nb - 1);
        e->next = nbkt[h];
        nbkt[h] = e;
    }
    free(m->u.m.bkt);
    m->u.m.bkt = nbkt;
    m->u.m.nb = nb;
}

static MapEntry *map_find(Val *m, const char *key)
{
    size_t h = hash_key(key) & (m->u.m.nb - 1);
    for (MapEntry *e = m->u.m.bkt[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e;
    return NULL;
}

Val *map_get(Val *map, const char *key)
{
    MapEntry *e = map_find(map, key);
    return e ? e->val : NULL;
}

int map_has(Val *map, const char *key)
{
    return map_find(map, key) != NULL;
}

void map_set(Val *map, const char *key, Val *val)
{
    MapEntry *e = map_find(map, key);
    if (e) { e->val = val; return; }
    e = (MapEntry *)xmalloc(sizeof(MapEntry));
    e->key = xstrdup(key);
    e->val = val;
    e->next = NULL;
    e->prev_o = map->u.m.tail;
    e->next_o = NULL;
    if (map->u.m.tail) map->u.m.tail->next_o = e;
    else map->u.m.head = e;
    map->u.m.tail = e;
    size_t h = hash_key(key) & (map->u.m.nb - 1);
    e->next = map->u.m.bkt[h];
    map->u.m.bkt[h] = e;
    map->u.m.count++;
    if (map->u.m.count > map->u.m.nb * 3 / 4)
        map_rehash(map, map->u.m.nb * 2);
}

int map_del(Val *map, const char *key)
{
    size_t h = hash_key(key) & (map->u.m.nb - 1);
    MapEntry *prev = NULL;
    for (MapEntry *e = map->u.m.bkt[h]; e; prev = e, e = e->next) {
        if (strcmp(e->key, key) != 0) continue;
        if (prev) prev->next = e->next;
        else map->u.m.bkt[h] = e->next;
        if (e->prev_o) e->prev_o->next_o = e->next_o;
        else map->u.m.head = e->next_o;
        if (e->next_o) e->next_o->prev_o = e->prev_o;
        else map->u.m.tail = e->prev_o;
        free(e->key);
        free(e);
        map->u.m.count--;
        return 1;
    }
    return 0;
}

size_t map_count(Val *map)
{
    return map->u.m.count;
}
