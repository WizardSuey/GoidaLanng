/* table.h — хеш-таблица «имя → значение» для глобалов модулей. */
#ifndef GOIDA_TABLE_H
#define GOIDA_TABLE_H

#include <stddef.h>
#include "forward.h"
#include "value.h"

typedef struct TEntry {
    char           *key;
    Val            *val;
    struct TEntry  *next;
    struct TEntry  *prev_o, *next_o;   /* порядок определения */
} TEntry;

struct Table {
    TEntry **bkt;
    size_t   nb, count;
    TEntry  *head, *tail;
};

void   table_init(Table *t);
void   table_free(Table *t);
void   table_set(Table *t, const char *key, Val *v);
Val   *table_get(Table *t, const char *key);   /* NULL если нет */
int    table_has(Table *t, const char *key);
int    table_delete(Table *t, const char *key);
size_t table_count(Table *t);
void   table_mark(Table *t);                   /* для GC */
/* Снимок таблицы в V_MAP (порядок определения сохраняется). */
Val   *table_to_map(Table *t);
void   table_foreach(Table *t, void (*cb)(const char *, Val *, void *), void *ud);

#endif /* GOIDA_TABLE_H */
