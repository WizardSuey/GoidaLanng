/* table.c — реализация хеш-таблицы. */
#include "table.h"
#include "gc.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static size_t hash_key(const char *s)
{
    size_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

void table_init(Table *t)
{
    t->nb = 16;
    t->count = 0;
    t->head = t->tail = NULL;
    t->bkt = (TEntry **)xcalloc(t->nb, sizeof(TEntry *));
}

void table_free(Table *t)
{
    TEntry *e = t->head;
    while (e) {
        TEntry *n = e->next_o;
        free(e->key);
        free(e);
        e = n;
    }
    free(t->bkt);
    t->bkt = NULL;
    t->head = t->tail = NULL;
    t->count = 0;
}

static void rehash(Table *t, size_t nb)
{
    TEntry **nbkt = (TEntry **)xcalloc(nb, sizeof(TEntry *));
    for (TEntry *e = t->head; e; e = e->next_o) {
        size_t h = hash_key(e->key) & (nb - 1);
        e->next = nbkt[h];
        nbkt[h] = e;
    }
    free(t->bkt);
    t->bkt = nbkt;
    t->nb = nb;
}

static TEntry *find(Table *t, const char *key)
{
    size_t h = hash_key(key) & (t->nb - 1);
    for (TEntry *e = t->bkt[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e;
    return NULL;
}

void table_set(Table *t, const char *key, Val *v)
{
    TEntry *e = find(t, key);
    if (e) { e->val = v; return; }
    e = (TEntry *)xmalloc(sizeof(TEntry));
    e->key = xstrdup(key);
    e->val = v;
    e->next = NULL;
    e->prev_o = t->tail;
    e->next_o = NULL;
    if (t->tail) t->tail->next_o = e;
    else t->head = e;
    t->tail = e;
    size_t h = hash_key(key) & (t->nb - 1);
    e->next = t->bkt[h];
    t->bkt[h] = e;
    t->count++;
    if (t->count > t->nb * 3 / 4)
        rehash(t, t->nb * 2);
}

Val *table_get(Table *t, const char *key)
{
    TEntry *e = find(t, key);
    return e ? e->val : NULL;
}

int table_has(Table *t, const char *key)
{
    return find(t, key) != NULL;
}

int table_delete(Table *t, const char *key)
{
    size_t h = hash_key(key) & (t->nb - 1);
    TEntry *prev = NULL;
    for (TEntry *e = t->bkt[h]; e; prev = e, e = e->next) {
        if (strcmp(e->key, key) != 0) continue;
        if (prev) prev->next = e->next;
        else t->bkt[h] = e->next;
        if (e->prev_o) e->prev_o->next_o = e->next_o;
        else t->head = e->next_o;
        if (e->next_o) e->next_o->prev_o = e->prev_o;
        else t->tail = e->prev_o;
        free(e->key);
        free(e);
        t->count--;
        return 1;
    }
    return 0;
}

size_t table_count(Table *t)
{
    return t->count;
}

void table_mark(Table *t)
{
    for (TEntry *e = t->head; e; e = e->next_o)
        gc_mark_val(e->val);
}

Val *table_to_map(Table *t)
{
    Val *m = v_map();
    for (TEntry *e = t->head; e; e = e->next_o)
        map_set(m, e->key, e->val);
    return m;
}

void table_foreach(Table *t, void (*cb)(const char *, Val *, void *), void *ud)
{
    for (TEntry *e = t->head; e; e = e->next_o)
        cb(e->key, e->val, ud);
}
