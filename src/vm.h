/* vm.h — стековая виртуальная машина GoidaLang. */
#ifndef GOIDA_VM_H
#define GOIDA_VM_H

#include <setjmp.h>
#include <stdio.h>
#include "forward.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

#define VM_STACK_MAX  8192
#define VM_FRAMES_MAX 512

typedef struct Frame {
    Val     *closure;    /* V_CLOSURE */
    uint8_t *ip;
    Val    **slots;      /* slot 0 = вызываемое значение */
    Module  *mod;
} Frame;

struct Interp {
    Val  *stack[VM_STACK_MAX];
    Val **top;
    Frame frames[VM_FRAMES_MAX];
    int   frame_count;
    Val  *open_upvalues;          /* связный список V_UPVALUE */

    jmp_buf err_jmp;
    int     err_armed;
    char    errmsg[1024];
    char    trace[2048];

    int   argc;
    char **argv;
    FILE *out, *err, *in;
    int   trace_enabled;
    int   repl_mode;
};

/* Инициализация ВМ (GC-корни, stdio). */
void vm_init(Interp *I, int argc, char **argv);

/* Запуск главной функции модуля; возвращает её результат. */
Val *vm_run(Interp *I, Module *mod);

/* Ошибка выполнения: строит трассировку и делает longjmp. */
void rt_error(Interp *I, int line, const char *fmt, ...);

/* Вызов значения из нативных функций (для eval-подобных вещей). */
Val *vm_call(Interp *I, Val *fn, int argc, Val **argv, int line);

/* Реестр методов нативных объектов. */
typedef struct { const char *name; NativeFn fn; } MethodEntry;
void     vm_register_class(const char *klass, const MethodEntry *m, int n);
NativeFn vm_lookup_method(const char *klass, const char *name);

/* GC-корни ВМ. */
void vm_gc_mark(void);

#endif /* GOIDA_VM_H */
