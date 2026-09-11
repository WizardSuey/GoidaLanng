/* gc.c — сборщик мусора mark & sweep. */
#include "gc.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static Val   *g_all = NULL;
static size_t g_count = 0;
static size_t g_next_gc = 1024 * 1024;   /* порог в «условных байтах» */
static size_t g_bytes = 0;

static Val ***g_slots = NULL;
static size_t g_slots_n = 0, g_slots_cap = 0;
static void (*g_extra)(void) = NULL;

static size_t val_size(Val *v)
{
    switch (v->t) {
        case V_STR: return sizeof(Val) + v->u.s.len + 1;
        case V_ARR: return sizeof(Val) + v->u.a.cap * sizeof(Val *);
        case V_MAP: return sizeof(Val) + v->u.m.nb * sizeof(MapEntry *)
                     + v->u.m.count * (sizeof(MapEntry) + 24);
        case V_CLOSURE: return sizeof(Val) + (size_t)v->u.cl.upcount * sizeof(Val *);
        default: return sizeof(Val);
    }
}

void gc_init(void)
{
    g_all = NULL;
    g_count = 0;
    g_bytes = 0;
    g_next_gc = 1024 * 1024;
}

void gc_add_root_slot(Val **slot)
{
    if (g_slots_n == g_slots_cap) {
        g_slots_cap = g_slots_cap ? g_slots_cap * 2 : 16;
        g_slots = (Val ***)xrealloc(g_slots, sizeof(Val **) * g_slots_cap);
    }
    g_slots[g_slots_n++] = slot;
}

void gc_remove_root_slot(Val **slot)
{
    for (size_t i = 0; i < g_slots_n; i++) {
        if (g_slots[i] == slot) {
            g_slots[i] = g_slots[g_slots_n - 1];
            g_slots_n--;
            return;
        }
    }
}

void gc_set_extra_roots(void (*fn)(void))
{
    g_extra = fn;
}

void gc_mark_function(Function *f)
{
    if (!f) return;
    Chunk *c = &f->chunk;
    for (size_t i = 0; i < c->consts_n; i++)
        gc_mark_val(c->consts[i]);
}

void gc_mark_val(Val *v)
{
    while (v) {
        if (v->marked) return;
        v->marked = 1;
        switch (v->t) {
            case V_STR: case V_INT: case V_FLOAT:
            case V_BOOL: case V_NIL: case V_NATIVE:
                break;
            case V_ARR:
                for (size_t i = 0; i < v->u.a.len; i++)
                    gc_mark_val(v->u.a.v[i]);
                break;
            case V_MAP:
                for (MapEntry *e = v->u.m.head; e; e = e->next_o)
                    gc_mark_val(e->val);
                break;
            case V_FN:
                gc_mark_function(v->u.fun.f);
                break;
            case V_CLOSURE:
                gc_mark_function(v->u.cl.fn);
                for (int i = 0; i < v->u.cl.upcount; i++)
                    gc_mark_val(v->u.cl.ups[i]);
                break;
            case V_UPVALUE:
                gc_mark_val(v->u.up.closed);
                if (v->u.up.loc) gc_mark_val(*v->u.up.loc);
                break;
            case V_OBJ:
                break;   /* нативные данные помечает ВМ, если нужно */
        }
        return;
    }
}

static void free_val(Val *v)
{
    switch (v->t) {
        case V_STR: free(v->u.s.p); break;
        case V_ARR: free(v->u.a.v); break;
        case V_MAP: {
            MapEntry *e = v->u.m.head;
            while (e) {
                MapEntry *n = e->next_o;
                free(e->key);
                free(e);
                e = n;
            }
            free(v->u.m.bkt);
            break;
        }
        case V_FN:
            function_free(v->u.fun.f);
            break;
        case V_CLOSURE:
            free(v->u.cl.ups);
            break;   /* функцию владеет V_FN-константа */
        case V_UPVALUE:
            break;
        case V_OBJ:
            if (v->u.o.fin) v->u.o.fin(v->u.o.data);
            break;
        default:
            break;
    }
    free(v);
}

void gc_collect(void)
{
    /* пометка */
    for (size_t i = 0; i < g_slots_n; i++) {
        if (g_slots[i] && *g_slots[i]) gc_mark_val(*g_slots[i]);
    }
    if (g_extra) g_extra();

    /* очистка */
    Val **link = &g_all;
    size_t live_bytes = 0;
    size_t live = 0;
    while (*link) {
        Val *v = *link;
        if (v->marked) {
            v->marked = 0;
            live_bytes += val_size(v);
            live++;
            link = &v->gnext;
        } else {
            *link = v->gnext;
            free_val(v);
            g_count--;
        }
    }
    g_bytes = live_bytes;
    g_count = live;
    if (g_next_gc < live_bytes * 2) g_next_gc = live_bytes * 2;
    if (g_next_gc < 256 * 1024) g_next_gc = 256 * 1024;
}

Val *gc_alloc(VType t)
{
    if (g_bytes > g_next_gc)
        gc_collect();

    Val *v = (Val *)xcalloc(1, sizeof(Val));
    v->t = t;
    v->marked = 0;
    v->gnext = g_all;
    g_all = v;
    g_count++;
    g_bytes += sizeof(Val);
    return v;
}

size_t gc_object_count(void)
{
    return g_count;
}
