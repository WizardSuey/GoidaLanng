/* vm.c — стековая виртуальная машина: диспетчер инструкций,
 * вызовы, замыкания, upvalue, трассировка ошибок.
 */
#include "vm.h"
#include "gc.h"
#include "module.h"
#include "compiler.h"
#include "util.h"
#include "utf8.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int frame_line(Frame *f)
{
    if (!f->closure || f->closure->t != V_CLOSURE) return 0;
    Function *fn = f->closure->u.cl.fn;
    size_t off = (size_t)(f->ip - fn->chunk.code);
    if (off > 0 && off <= fn->chunk.count) return fn->chunk.lines[off - 1];
    return 0;
}

static Interp *g_I = NULL;

/* ---------------- реестр классов нативных объектов ---------------- */

typedef struct {
    const char *klass;
    const MethodEntry *m;
    int n;
} ClassReg;

static ClassReg g_classes[32];
static int g_nclasses = 0;

void vm_register_class(const char *klass, const MethodEntry *m, int n)
{
    if (g_nclasses < 32) {
        g_classes[g_nclasses].klass = klass;
        g_classes[g_nclasses].m = m;
        g_classes[g_nclasses].n = n;
        g_nclasses++;
    }
}

NativeFn vm_lookup_method(const char *klass, const char *name)
{
    for (int i = 0; i < g_nclasses; i++) {
        if (strcmp(g_classes[i].klass, klass) != 0) continue;
        for (int j = 0; j < g_classes[i].n; j++)
            if (strcmp(g_classes[i].m[j].name, name) == 0)
                return g_classes[i].m[j].fn;
    }
    return NULL;
}

/* ---------------- инициализация и GC ---------------- */

void vm_init(Interp *I, int argc, char **argv)
{
    memset(I, 0, sizeof *I);
    I->top = I->stack;
    I->argc = argc;
    I->argv = argv;
    I->out = stdout;
    I->err = stderr;
    I->in = stdin;
    g_I = I;
    gc_set_extra_roots(vm_gc_mark);
}

void vm_gc_mark(void)
{
    Interp *I = g_I;
    if (!I) return;
    for (Val **s = I->stack; s < I->top; s++)
        gc_mark_val(*s);
    for (int i = 0; i < I->frame_count; i++)
        gc_mark_val(I->frames[i].closure);
    for (Val *u = I->open_upvalues; u; u = u->u.up.next)
        gc_mark_val(u);
    module_gc_mark();
    compiler_gc_mark();
}

/* ---------------- ошибки ---------------- */

static void build_trace(Interp *I, int line)
{
    size_t off = 0;
    I->trace[0] = '\0';
    for (int i = I->frame_count - 1; i >= 0 && off < sizeof(I->trace) - 80; i--) {
        Frame *f = &I->frames[i];
        const char *fn = "<?>";
        int ln = line;
        if (f->closure && f->closure->t == V_CLOSURE) {
            Function *func = f->closure->u.cl.fn;
            fn = func->name;
            size_t ipoff = (size_t)(f->ip - func->chunk.code);
            if (ipoff > 0 && ipoff <= func->chunk.count)
                ln = func->chunk.lines[ipoff - 1];
        }
        const char *mod = f->mod ? f->mod->name : "?";
        int w = snprintf(I->trace + off, sizeof(I->trace) - off,
                         "\n  строка %d, %s (модуль %s)", ln, fn, mod);
        if (w < 0) break;
        off += (size_t)w;
        line = ln;
    }
}

void rt_error(Interp *I, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(I->errmsg, sizeof I->errmsg, fmt, ap);
    va_end(ap);
    build_trace(I, line);
    if (I->err_armed)
        longjmp(I->err_jmp, 1);
    /* вне защищённой зоны — печатаем и завершаем */
    fprintf(I->err, "ГОЙДА! Ошибка: %s%s\n", I->errmsg, I->trace);
    exit(1);
}

/* ---------------- стек ---------------- */

static void push(Interp *I, Val *v)
{
    if (I->top - I->stack >= VM_STACK_MAX)
        rt_error(I, 0, "переполнение стека");
    *I->top++ = v;
}

static Val *pop(Interp *I)
{
    return *--I->top;
}

static Val *peek(Interp *I, int n)
{
    return I->top[-1 - n];
}

/* ---------------- upvalue ---------------- */

static Val *capture_upvalue(Interp *I, Val **slot)
{
    Val *prev = NULL;
    Val *u = I->open_upvalues;
    while (u && u->u.up.loc > slot) {
        prev = u;
        u = u->u.up.next;
    }
    if (u && u->u.up.loc == slot) return u;
    Val *created = v_upvalue(slot);
    created->u.up.next = u;
    if (prev) prev->u.up.next = created;
    else I->open_upvalues = created;
    return created;
}

static void close_upvalues(Interp *I, Val **last)
{
    while (I->open_upvalues && I->open_upvalues->u.up.loc >= last) {
        Val *u = I->open_upvalues;
        u->u.up.closed = *u->u.up.loc;
        u->u.up.loc = NULL;
        I->open_upvalues = u->u.up.next;
    }
}

/* ---------------- арифметика ---------------- */

static int both_num(Val *a, Val *b, double *x, double *y, int *ints, long long *ix, long long *iy)
{
    int ai = a->t == V_INT, bi = b->t == V_INT;
    if (a->t != V_INT && a->t != V_FLOAT) return 0;
    if (b->t != V_INT && b->t != V_FLOAT) return 0;
    *ints = ai && bi;
    *x = ai ? (double)a->u.i : a->u.f;
    *y = bi ? (double)b->u.i : b->u.f;
    *ix = a->u.i;
    *iy = b->u.i;
    return 1;
}

static Val *num_result(Interp *I, int ints, double d, long long i)
{
    (void)I;
    return ints ? v_int(i) : v_float(d);
}

static void do_binary(Interp *I, OpCode op, int line)
{
    Val *b = pop(I);
    Val *a = pop(I);
    double x, y; long long ix, iy; int ints;

    if (op == OP_ADD) {
        if (a->t == V_STR || b->t == V_STR) {
            char *sa = val_to_str(a);
            char *sb = val_to_str(b);
            size_t la = strlen(sa), lb = strlen(sb);
            char *buf = (char *)xmalloc(la + lb + 1);
            memcpy(buf, sa, la);
            memcpy(buf + la, sb, lb + 1);
            free(sa); free(sb);
            Val *r = v_strn(buf, la + lb);
            free(buf);
            push(I, r);
            return;
        }
        if (a->t == V_ARR && b->t == V_ARR) {
            Val *r = v_arr();
            for (size_t i = 0; i < a->u.a.len; i++) arr_push(r, a->u.a.v[i]);
            for (size_t i = 0; i < b->u.a.len; i++) arr_push(r, b->u.a.v[i]);
            push(I, r);
            return;
        }
    }

    if (op == OP_EQ) { push(I, v_bool(val_equals(a, b))); return; }
    if (op == OP_NE) { push(I, v_bool(!val_equals(a, b))); return; }

    if (!both_num(a, b, &x, &y, &ints, &ix, &iy))
        rt_error(I, line, "операция '%s' неприменима к типам %s и %s",
                 op_name(op), type_name(a), type_name(b));

    switch (op) {
        case OP_ADD: push(I, num_result(I, ints, x + y, ix + iy)); return;
        case OP_SUB: push(I, num_result(I, ints, x - y, ix - iy)); return;
        case OP_MUL: push(I, num_result(I, ints, x * y, ix * iy)); return;
        case OP_DIV:
            if (y == 0) rt_error(I, line, "деление на ноль");
            if (ints && ix % iy == 0) push(I, v_int(ix / iy));
            else push(I, v_float(x / y));
            return;
        case OP_MOD:
            if (y == 0) rt_error(I, line, "остаток от деления на ноль");
            if (ints) push(I, v_int(ix % iy));
            else push(I, v_float(fmod(x, y)));
            return;
        case OP_EQ: push(I, v_bool(val_equals(a, b))); return;
        case OP_NE: push(I, v_bool(!val_equals(a, b))); return;
        case OP_LT: case OP_LE: case OP_GT: case OP_GE: {
            if (a->t == V_STR && b->t == V_STR) {
                int c = strcmp(a->u.s.p, b->u.s.p);
                int r = op == OP_LT ? c < 0 : op == OP_LE ? c <= 0 :
                        op == OP_GT ? c > 0 : c >= 0;
                push(I, v_bool(r));
                return;
            }
            int r = op == OP_LT ? x < y : op == OP_LE ? x <= y :
                    op == OP_GT ? x > y : x >= y;
            push(I, v_bool(r));
            return;
        }
        default:
            rt_error(I, line, "внутренняя ошибка оператора");
    }
}

/* ---------------- вызовы ---------------- */

static void do_call(Interp *I, int argc, int line)
{
    Val *callee = peek(I, argc);
    if (callee->t == V_FN) {
        Val *c = v_closure(callee->u.fun.f, 0);
        c->u.cl.mod = I->frame_count ? I->frames[I->frame_count - 1].mod : NULL;
        I->top[-argc - 1] = c;
        callee = c;
    }
    if (callee->t == V_NATIVE) {
        Val *selfv = (callee->u.nat.with_self && argc) ? I->top[-argc] : NULL;
        Val *res = callee->u.nat.fn(I, selfv, argc, I->top - argc);
        I->top -= argc + 1;
        push(I, res);
        return;
    }
    if (callee->t != V_CLOSURE)
        rt_error(I, line, "вызов значения, которое не является делом (тип: %s)",
                 type_name(callee));

    Function *fn = callee->u.cl.fn;
    if (argc > fn->arity)
        rt_error(I, line, "дело '%s': аргументов %d, ожидалось %d", fn->name, argc, fn->arity);
    while (argc < fn->arity) { push(I, v_nil()); argc++; }

    if (I->frame_count >= VM_FRAMES_MAX)
        rt_error(I, line, "переполнение стека вызовов");
    Frame *f = &I->frames[I->frame_count++];
    f->closure = callee;
    f->ip = fn->chunk.code;
    f->slots = I->top - argc - 1;
    f->mod = callee->u.cl.mod;
}

Val *vm_call(Interp *I, Val *fn, int argc, Val **argv, int line)
{
    int stop = I->frame_count;
    push(I, fn);
    for (int i = 0; i < argc; i++) push(I, argv[i]);
    do_call(I, argc, line);
    /* крутим цикл, пока кадр вызова не вернётся */
    extern int vm_run_loop(Interp *I, int stop);
    vm_run_loop(I, stop);
    return pop(I);
}

/* ---------------- индексация/поля ---------------- */

static void do_get_index(Interp *I, int line)
{
    Val *idx = pop(I);
    Val *obj = pop(I);
    if (obj->t == V_ARR) {
        if (idx->t != V_INT) rt_error(I, line, "индекс массива должен быть целым");
        Val *r = arr_get(obj, idx->u.i);
        if (!r) rt_error(I, line, "индекс %lld вне границ массива (длина %zu)",
                         idx->u.i, obj->u.a.len);
        push(I, r);
        return;
    }
    if (obj->t == V_STR) {
        if (idx->t != V_INT) rt_error(I, line, "индекс строки должен быть целым");
        long long n = (long long)utf8_strlen(obj->u.s.p, obj->u.s.len);
        long long i = idx->u.i;
        if (i < 0) i += n;
        if (i < 0 || i >= n) rt_error(I, line, "индекс %lld вне границ строки", idx->u.i);
        size_t off = utf8_offset(obj->u.s.p, obj->u.s.len, (size_t)i);
        size_t w = utf8_charlen((unsigned char)obj->u.s.p[off]);
        if (w == 0) w = 1;
        push(I, v_strn(obj->u.s.p + off, w));
        return;
    }
    if (obj->t == V_MAP) {
        char keybuf[32];
        const char *key;
        if (idx->t == V_STR) key = idx->u.s.p;
        else if (idx->t == V_INT) { snprintf(keybuf, sizeof keybuf, "%lld", idx->u.i); key = keybuf; }
        else rt_error(I, line, "ключ словаря должен быть строкой");
        Val *r = map_get(obj, key);
        if (!r) rt_error(I, line, "в словаре нет ключа '%s'", key);
        push(I, r);
        return;
    }
    rt_error(I, line, "индексация неприменима к типу %s", type_name(obj));
}

static void do_set_index(Interp *I, int line)
{
    Val *val = pop(I);
    Val *idx = pop(I);
    Val *obj = pop(I);
    if (obj->t == V_ARR) {
        if (idx->t != V_INT) rt_error(I, line, "индекс массива должен быть целым");
        long long n = (long long)obj->u.a.len;
        long long i = idx->u.i;
        if (i < 0) i += n;
        if (i == n) { arr_push(obj, val); push(I, val); return; }
        if (i < 0 || i > n) rt_error(I, line, "индекс %lld вне границ массива", idx->u.i);
        obj->u.a.v[i] = val;
        push(I, val);
        return;
    }
    if (obj->t == V_MAP) {
        char keybuf[32];
        const char *key;
        if (idx->t == V_STR) key = idx->u.s.p;
        else if (idx->t == V_INT) { snprintf(keybuf, sizeof keybuf, "%lld", idx->u.i); key = keybuf; }
        else rt_error(I, line, "ключ словаря должен быть строкой");
        map_set(obj, key, val);
        push(I, val);
        return;
    }
    rt_error(I, line, "запись по индексу неприменима к типу %s", type_name(obj));
}

static void do_get_field(Interp *I, const char *name, int line)
{
    Val *obj = pop(I);
    if (obj->t == V_MAP) {
        Val *r = map_get(obj, name);
        if (!r) rt_error(I, line, "в словаре нет поля '%s'", name);
        push(I, r);
        return;
    }
    if (obj->t == V_OBJ) {
        NativeFn m = vm_lookup_method(obj->u.o.klass, name);
        if (!m) rt_error(I, line, "у объекта <%s> нет метода '%s'", obj->u.o.klass, name);
        Val *nv = v_native(name, m);
        nv->u.nat.with_self = 1;
        push(I, nv);
        return;
    }
    rt_error(I, line, "обращение к полю '%s' неприменимо к типу %s", name, type_name(obj));
}

static void do_set_field(Interp *I, const char *name, int line)
{
    Val *val = pop(I);
    Val *obj = pop(I);
    if (obj->t != V_MAP)
        rt_error(I, line, "запись поля '%s' неприменима к типу %s", name, type_name(obj));
    map_set(obj, name, val);
    push(I, val);
}

static void do_length(Interp *I, int line)
{
    Val *v = pop(I);
    if (v->t == V_STR) { push(I, v_int((long long)utf8_strlen(v->u.s.p, v->u.s.len))); return; }
    if (v->t == V_ARR) { push(I, v_int((long long)v->u.a.len)); return; }
    if (v->t == V_MAP) { push(I, v_int((long long)map_count(v))); return; }
    rt_error(I, line, "длина() неприменима к типу %s", type_name(v));
}

/* ---------------- главный цикл ---------------- */

static uint16_t read_u16(Frame *f)
{
    uint16_t v = (uint16_t)((f->ip[0] << 8) | f->ip[1]);
    f->ip += 2;
    return v;
}
#define READ_U16() read_u16(f)
#define READ_U8()  (*f->ip++)
#define CONST_AT(i) (f->closure->u.cl.fn->chunk.consts[i])

int vm_run_loop(Interp *I, int stop_fc)
{
    for (;;) {
        Frame *f = &I->frames[I->frame_count - 1];
        OpCode op = (OpCode)*f->ip++;
        switch (op) {
            case OP_CONST: push(I, CONST_AT(READ_U16())); break;
            case OP_NIL:   push(I, v_nil()); break;
            case OP_TRUE:  push(I, v_bool(1)); break;
            case OP_FALSE: push(I, v_bool(0)); break;
            case OP_POP:   pop(I); break;
            case OP_DUP:   push(I, peek(I, 0)); break;
            case OP_SWAP: {
                Val *a = I->top[-1];
                I->top[-1] = I->top[-2];
                I->top[-2] = a;
                break;
            }
            case OP_GET_LOCAL: push(I, f->slots[READ_U8()]); break;
            case OP_SET_LOCAL: f->slots[READ_U8()] = peek(I, 0); break;
            case OP_GET_GLOBAL: {
                const char *name = CONST_AT(READ_U16())->u.s.p;
                Val *v = table_get(&f->mod->globals, name);
                if (!v) rt_error(I, frame_line(f), "неизвестное имя '%s'", name);
                push(I, v);
                break;
            }
            case OP_SET_GLOBAL: {
                const char *name = CONST_AT(READ_U16())->u.s.p;
                if (!table_has(&f->mod->globals, name))
                    rt_error(I, frame_line(f), "неизвестная переменная '%s' (используй 'пускай')", name);
                table_set(&f->mod->globals, name, peek(I, 0));
                break;
            }
            case OP_DEFINE_GLOBAL: {
                const char *name = CONST_AT(READ_U16())->u.s.p;
                table_set(&f->mod->globals, name, pop(I));
                break;
            }
            case OP_GET_UPVALUE: {
                Val *u = f->closure->u.cl.ups[READ_U8()];
                push(I, u->u.up.loc ? *u->u.up.loc : u->u.up.closed);
                break;
            }
            case OP_SET_UPVALUE: {
                Val *u = f->closure->u.cl.ups[READ_U8()];
                if (u->u.up.loc) *u->u.up.loc = peek(I, 0);
                else u->u.up.closed = peek(I, 0);
                break;
            }
            case OP_CLOSE_UPVALUE:
                close_upvalues(I, I->top - 1);
                pop(I);
                break;
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
            case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE:
                do_binary(I, op, frame_line(f));
                break;
            case OP_NEG: {
                Val *v = pop(I);
                if (v->t == V_INT) push(I, v_int(-v->u.i));
                else if (v->t == V_FLOAT) push(I, v_float(-v->u.f));
                else rt_error(I, frame_line(f), "унарный минус неприменим к типу %s", type_name(v));
                break;
            }
            case OP_NOT: push(I, v_bool(!truthy(pop(I)))); break;
            case OP_LENGTH: do_length(I, frame_line(f)); break;
            case OP_JUMP: { uint16_t j = READ_U16(); f->ip += j; break; }
            case OP_JUMP_IF_FALSE: {
                uint16_t j = READ_U16();
                if (!truthy(peek(I, 0))) f->ip += j;
                break;
            }
            case OP_IF_NIL_JUMP: {
                uint16_t j = READ_U16();
                if (pop(I)->t == V_NIL) f->ip += j;
                break;
            }
            case OP_LOOP: { uint16_t j = READ_U16(); f->ip -= j; break; }
            case OP_CLOSURE: {
                Val *fnv = CONST_AT(READ_U16());
                Function *fn = fnv->u.fun.f;
                Val *cl = v_closure(fn, fn->upvalue_count);
                cl->u.cl.mod = f->mod;
                for (int i = 0; i < fn->upvalue_count; i++) {
                    int is_local = READ_U8();
                    int index = READ_U8();
                    cl->u.cl.ups[i] = is_local
                        ? capture_upvalue(I, f->slots + index)
                        : f->closure->u.cl.ups[index];
                }
                push(I, cl);
                break;
            }
            case OP_CALL: {
                int argc = READ_U8();
                do_call(I, argc, 0);
                break;
            }
            case OP_CALLM: {
                int argc = READ_U8();
                Val *callee = peek(I, argc);
                int drop = 0;
                if (callee->t == V_NATIVE && !callee->u.nat.with_self) {
                    drop = 1;   /* нативка из словаря модуля self не ждёт */
                } else if (argc > 0 &&
                           (callee->t == V_CLOSURE || callee->t == V_FN)) {
                    int arity = callee->t == V_CLOSURE
                        ? callee->u.cl.fn->arity : callee->u.fun.f->arity;
                    if (arity == argc - 1) drop = 1;   /* self не предусмотрен */
                }
                if (drop) {
                    Val **objp = I->top - argc;
                    for (int i = 0; i < argc - 1; i++) objp[i] = objp[i + 1];
                    I->top--;
                    argc--;
                }
                do_call(I, argc, 0);
                break;
            }
            case OP_RETURN: {
                Val *res = pop(I);
                close_upvalues(I, f->slots);
                I->frame_count--;
                I->top = f->slots;   /* всегда rewinding стека кадра */
                push(I, res);
                if (I->frame_count <= stop_fc) return 0;
                break;
            }
            case OP_NEW_ARRAY: {
                uint16_t n = READ_U16();
                Val *arr = v_arr();
                for (uint16_t i = 0; i < n; i++)
                    arr_push(arr, I->top[-(int)n + i]);
                I->top -= n;
                push(I, arr);
                break;
            }
            case OP_NEW_MAP: {
                uint16_t n = READ_U16();
                Val *m = v_map();
                for (uint16_t i = 0; i < n; i++) {
                    Val *key = I->top[-(int)(2 * n) + 2 * i];
                    Val *val = I->top[-(int)(2 * n) + 2 * i + 1];
                    if (key->t != V_STR) rt_error(I, frame_line(f), "ключ словаря должен быть строкой");
                    map_set(m, key->u.s.p, val);
                }
                I->top -= 2 * n;
                push(I, m);
                break;
            }
            case OP_GET_INDEX: do_get_index(I, frame_line(f)); break;
            case OP_SET_INDEX: do_set_index(I, frame_line(f)); break;
            case OP_GET_FIELD: do_get_field(I, CONST_AT(READ_U16())->u.s.p, frame_line(f)); break;
            case OP_SET_FIELD: do_set_field(I, CONST_AT(READ_U16())->u.s.p, frame_line(f)); break;
            case OP_IMPORT: {
                const char *spec = CONST_AT(READ_U16())->u.s.p;
                Val *ns = module_load(I, f->mod, spec);
                push(I, ns);
                break;
            }
            case OP_HALT:
                return 0;
        }
    }
}

Val *vm_run(Interp *I, Module *mod)
{
    if (!mod->main_fn) return v_nil();
    int stop = I->frame_count;   /* вернуться, как только модуль завершится */
    Val *cl = v_closure(mod->main_fn->u.fun.f, 0);
    cl->u.cl.mod = mod;
    push(I, cl);
    do_call(I, 0, 0);
    vm_run_loop(I, stop);
    return pop(I);
}
