/* value.h — представления значений GoidaLang.
 * Все значения — указатели на Val (боксирование), управляются GC (gc.h).
 */
#ifndef GOIDA_VALUE_H
#define GOIDA_VALUE_H

#include <stddef.h>
#include "forward.h"

typedef enum {
    V_NIL,        /* пусто                       */
    V_BOOL,       /* истина / ложь               */
    V_INT,        /* целое (long long)           */
    V_FLOAT,      /* дробное (double)            */
    V_STR,        /* строка (байты + длина)      */
    V_ARR,        /* массив                      */
    V_MAP,        /* словарь (порядок вставки)   */
    V_FN,         /* функция-константа (байткод) */
    V_CLOSURE,    /* замыкание                   */
    V_UPVALUE,    /* открытое upvalue (служебное)*/
    V_NATIVE,     /* встроенная C-функция        */
    V_OBJ         /* нативный объект (сокеты..)  */
} VType;

typedef Val *(*NativeFn)(Interp *I, Val *self, int argc, Val **argv);

typedef struct MapEntry {
    char            *key;
    Val             *val;
    struct MapEntry *next;              /* цепочка в бакете    */
    struct MapEntry *prev_o, *next_o;   /* порядок вставки     */
} MapEntry;

struct Val {
    VType         t;
    unsigned char marked;
    struct Val   *gnext;                /* список всех объектов GC */
    union {
        int         b;
        long long   i;
        double      f;
        struct { char *p; size_t len; }              s;
        struct { Val **v; size_t len, cap; }         a;
        struct { MapEntry *head, *tail;
                 MapEntry **bkt; size_t nb, count; } m;
        struct { Function *f; }                      fun;
        struct { Function *fn; Val **ups;
                 int upcount; Module *mod; }         cl;
        struct { Val **loc; Val *closed;
                 struct Val *next; }                 up;
        struct { NativeFn fn; const char *name;
                 unsigned char with_self; }          nat;
        struct { const char *klass; void *data;
                 void (*fin)(void *); }              o;
    } u;
};

/* Конструкторы (результат — объект под управлением GC). */
Val *v_nil(void);
Val *v_bool(int b);
Val *v_int(long long i);
Val *v_float(double f);
Val *v_str(const char *s);
Val *v_strn(const char *s, size_t len);
Val *v_arr(void);
Val *v_map(void);
Val *v_fn(Function *f);
Val *v_closure(Function *f, int upcount);
Val *v_upvalue(Val **slot);
Val *v_native(const char *name, NativeFn fn);
Val *v_obj(const char *klass, void *data, void (*fin)(void *));

int          truthy(Val *v);
int          val_equals(Val *a, Val *b);
const char  *type_name(Val *v);
/* Печатное представление; возвращает malloc-строку, вызывать free(). */
char        *val_to_str(Val *v);
/* repr-представление (строки в кавычках); malloc-строка. */
char        *val_repr(Val *v);
const char  *val_cstr(Val *v);   /* только V_STR */

void arr_push(Val *arr, Val *v);
Val *arr_get(Val *arr, long long idx);   /* NULL при выходе за границы */

Val *map_get(Val *map, const char *key); /* NULL если нет */
int  map_has(Val *map, const char *key);
void map_set(Val *map, const char *key, Val *val);
int  map_del(Val *map, const char *key);
size_t map_count(Val *map);

#endif /* GOIDA_VALUE_H */
