/* gc.h — сборщик мусора: mark & sweep.
 * Корни: слоты Val**, регистрируемые модулями, плюс callback
 * виртуальной машины (стек, кадры, открытые upvalue, модули).
 */
#ifndef GOIDA_GC_H
#define GOIDA_GC_H

#include <stddef.h>
#include "value.h"
#include "chunk.h"

void  gc_init(void);
/* Выделение объекта; при превышении порога запускает сборку. */
Val  *gc_alloc(VType t);

/* Пометка (рекурсивная). */
void  gc_mark_val(Val *v);
void  gc_mark_function(Function *f);

/* Ручная сборка. */
void  gc_collect(void);

/* Постоянные корни-слоты. */
void  gc_add_root_slot(Val **slot);
void  gc_remove_root_slot(Val **slot);
/* Дополнительные корни (вызывает ВМ: стек/кадры/модули). */
void  gc_set_extra_roots(void (*fn)(void));

size_t gc_object_count(void);

#endif /* GOIDA_GC_H */
