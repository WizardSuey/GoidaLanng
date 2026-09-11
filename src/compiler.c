/* compiler.c — однопроходной компилятор AST → байткод.
 * Локали: слот 0 зарезервирован под вызываемое значение (self/функция),
 * параметры — слоты 1..arity.
 * OP_JUMP_IF_FALSE смотрит условие, не снимая его — после него всегда POP.
 * OP_LOOP: операнд = байт назад от позиции после операнда до цели.
 * Инструкции с 8-битным операндом: emit(op) + emit_u8(x).
 */
#include "compiler.h"
#include "value.h"
#include "gc.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOCALS 256

typedef struct {
    const char *name;
    int  depth;      /* -1, пока не инициализирована */
    int  captured;
} Local;

typedef struct { uint8_t index; int is_local; } Upv;

typedef struct LoopCtx {
    int start;
    int scope_depth;
    int sd_start;
    int breaks[64],    nbreaks;
    int continues[64], ncontinues;      /* OP_LOOP-заглушки (назад) */
    int fwd[64],       nfwd;            /* OP_JUMP-заглушки (вперёд, к step) */
    int forward_cont;                   /* 1 для for/foreach */
    struct LoopCtx *prev;
} LoopCtx;

typedef struct Compiler {
    struct Compiler *enclosing;
    struct Compiler *active_prev;   /* для GC */
    Function *fn;
    Local     locals[MAX_LOCALS];
    int       local_count;
    int       scope_depth;
    Upv       upvalues[MAX_LOCALS];
    int       is_script;
    LoopCtx  *loop;
    int       sd;
    int       errored;
    char     *err;
    size_t    errsz;
} Compiler;

static Compiler *g_active = NULL;

void compiler_gc_mark(void)
{
    for (Compiler *c = g_active; c; c = c->active_prev)
        gc_mark_function(c->fn);
}

/* ---------------- ошибки и эмиссия ---------------- */

static void cerr(Compiler *C, int line, const char *msg)
{
    if (C->errored) return;
    C->errored = 1;
    snprintf(C->err, C->errsz, "ошибка компиляции, строка %d: %s", line, msg);
}

static void emit(Compiler *C, OpCode op, int line)
{
    chunk_write(&C->fn->chunk, (uint8_t)op, line);
}

static void emit_u8(Compiler *C, uint8_t b, int line)
{
    chunk_write(&C->fn->chunk, b, line);
}

static void emit2(Compiler *C, OpCode op, uint16_t v, int line)
{
    chunk_write2(&C->fn->chunk, (uint8_t)op, v, line);
}

static int const_idx(Compiler *C, Val *v, int line)
{
    int i = chunk_add_const(&C->fn->chunk, v);
    if (i > 0xFFFF) cerr(C, line, "слишком много констант");
    return i;
}

static int name_idx(Compiler *C, const char *name, int line)
{
    return const_idx(C, v_str(name), line);
}

static void push(Compiler *C)
{
    C->sd++;
    if (C->sd > C->fn->stack_max) C->fn->stack_max = C->sd;
}
static void pop(Compiler *C) { C->sd--; }

static int emit_jump(Compiler *C, OpCode op, int line)
{
    emit2(C, op, 0xFFFF, line);
    return (int)C->fn->chunk.count;
}

static void patch_jump(Compiler *C, int at)
{
    int jump = (int)C->fn->chunk.count - at;
    if (jump > 0xFFFF) { cerr(C, C->fn->chunk.lines[at], "слишком длинный переход"); return; }
    C->fn->chunk.code[at - 2] = (uint8_t)(jump >> 8);
    C->fn->chunk.code[at - 1] = (uint8_t)(jump & 0xFF);
}

static void emit_loop_to(Compiler *C, int target, int line)
{
    emit2(C, OP_LOOP, 0, line);
    int at = (int)C->fn->chunk.count;
    int off = at - target;
    if (off < 0 || off > 0xFFFF) { cerr(C, line, "слишком большое тело цикла"); return; }
    C->fn->chunk.code[at - 2] = (uint8_t)(off >> 8);
    C->fn->chunk.code[at - 1] = (uint8_t)(off & 0xFF);
}

static int emit_continue(Compiler *C, int line)
{
    emit2(C, OP_LOOP, 0xFFFF, line);
    return (int)C->fn->chunk.count;
}

static void patch_loop_to(Compiler *C, int at, int target)
{
    int off = at - target;
    if (off < 0 || off > 0xFFFF) { cerr(C, C->fn->chunk.lines[at], "слишком большое тело цикла"); return; }
    C->fn->chunk.code[at - 2] = (uint8_t)(off >> 8);
    C->fn->chunk.code[at - 1] = (uint8_t)(off & 0xFF);
}

/* ---------------- локалы/области ---------------- */

static void add_local(Compiler *C, const char *name, int line)
{
    if (C->local_count >= MAX_LOCALS) { cerr(C, line, "слишком много локальных переменных"); return; }
    Local *l = &C->locals[C->local_count++];
    l->name = name;
    l->depth = -1;
    l->captured = 0;
    push(C);
}

static void begin_scope(Compiler *C) { C->scope_depth++; }

static void end_scope(Compiler *C, int line)
{
    C->scope_depth--;
    while (C->local_count > 0 &&
           C->locals[C->local_count - 1].depth > C->scope_depth) {
        Local *l = &C->locals[C->local_count - 1];
        if (l->captured) emit(C, OP_CLOSE_UPVALUE, line);
        else             emit(C, OP_POP, line);
        pop(C);
        C->local_count--;
    }
}

static int resolve_local(Compiler *C, const char *name)
{
    for (int i = C->local_count - 1; i >= 0; i--) {
        if (strcmp(C->locals[i].name, name) == 0) return i;
    }
    return -1;
}

static int add_upvalue(Compiler *C, int index, int is_local, int line)
{
    for (int i = 0; i < C->fn->upvalue_count; i++) {
        if (C->upvalues[i].index == index && C->upvalues[i].is_local == is_local)
            return i;
    }
    if (C->fn->upvalue_count >= MAX_LOCALS) { cerr(C, line, "слишком много upvalue"); return 0; }
    int i = C->fn->upvalue_count++;
    C->upvalues[i].index = (uint8_t)index;
    C->upvalues[i].is_local = is_local;
    return i;
}

static int resolve_upvalue(Compiler *C, const char *name, int line)
{
    if (!C->enclosing) return -1;
    int local = resolve_local(C->enclosing, name);
    if (local != -1) {
        if (C->enclosing->locals[local].depth < 0) return -1;
        C->enclosing->locals[local].captured = 1;
        return add_upvalue(C, (uint8_t)local, 1, line);
    }
    int up = resolve_upvalue(C->enclosing, name, line);
    if (up != -1) return add_upvalue(C, (uint8_t)up, 0, line);
    return -1;
}

/* ---------------- выражения ---------------- */

static void compile_expr(Compiler *C, Node *n);
static void compile_stmt(Compiler *C, Node *n);
static void compile_function(Compiler *C, FnDecl *fd, int line);

static void compile_call(Compiler *C, Node *n)
{
    if (n->u.call.is_method) {
        Node *field = n->u.call.callee;   /* N_FIELD */
        compile_expr(C, field->u.field.obj);
        emit(C, OP_DUP, n->line); push(C);
        emit2(C, OP_GET_FIELD, (uint16_t)name_idx(C, field->u.field.name, n->line), n->line);
        emit(C, OP_SWAP, n->line);
        for (int i = 0; i < n->u.call.nargs; i++) compile_expr(C, n->u.call.args[i]);
        emit(C, OP_CALLM, n->line);
        emit_u8(C, (uint8_t)(n->u.call.nargs + 1), n->line);
        C->sd -= n->u.call.nargs + 1;
    } else {
        compile_expr(C, n->u.call.callee);
        for (int i = 0; i < n->u.call.nargs; i++) compile_expr(C, n->u.call.args[i]);
        emit(C, OP_CALL, n->line);
        emit_u8(C, (uint8_t)n->u.call.nargs, n->line);
        C->sd -= n->u.call.nargs;
    }
}

static void compile_expr(Compiler *C, Node *n)
{
    if (C->errored || !n) return;
    switch (n->k) {
        case N_NUM: {
            double d = n->u.num;
            Val *v;
            if (d == (double)(long long)d && d >= -9e15 && d <= 9e15)
                v = v_int((long long)d);
            else
                v = v_float(d);
            emit2(C, OP_CONST, (uint16_t)const_idx(C, v, n->line), n->line);
            push(C);
            return;
        }
        case N_STR:
            emit2(C, OP_CONST,
                  (uint16_t)const_idx(C, v_strn(n->u.str.p, n->u.str.len), n->line),
                  n->line);
            push(C);
            return;
        case N_BOOL:
            emit(C, n->u.boolean ? OP_TRUE : OP_FALSE, n->line);
            push(C);
            return;
        case N_NIL:
            emit(C, OP_NIL, n->line);
            push(C);
            return;
        case N_IDENT: {
            int l = resolve_local(C, n->u.name);
            if (l != -1) {
                emit(C, OP_GET_LOCAL, n->line);
                emit_u8(C, (uint8_t)l, n->line);
                push(C);
                return;
            }
            int u = resolve_upvalue(C, n->u.name, n->line);
            if (u != -1) {
                emit(C, OP_GET_UPVALUE, n->line);
                emit_u8(C, (uint8_t)u, n->line);
                push(C);
                return;
            }
            emit2(C, OP_GET_GLOBAL, (uint16_t)name_idx(C, n->u.name, n->line), n->line);
            push(C);
            return;
        }
        case N_UN:
            compile_expr(C, n->u.un.e);
            emit(C, n->u.un.op == A_NEG ? OP_NEG : OP_NOT, n->line);
            return;
        case N_BIN: {
            if (n->u.bin.op == A_AND) {
                compile_expr(C, n->u.bin.l);
                int j = emit_jump(C, OP_JUMP_IF_FALSE, n->line);
                emit(C, OP_POP, n->line); pop(C);
                compile_expr(C, n->u.bin.r);
                patch_jump(C, j);
                return;
            }
            if (n->u.bin.op == A_OR) {
                compile_expr(C, n->u.bin.l);
                int j1 = emit_jump(C, OP_JUMP_IF_FALSE, n->line);  /* ложь → правое */
                int j2 = emit_jump(C, OP_JUMP, n->line);           /* истина → конец */
                patch_jump(C, j1);
                emit(C, OP_POP, n->line); pop(C);
                compile_expr(C, n->u.bin.r);
                patch_jump(C, j2);
                return;
            }
            compile_expr(C, n->u.bin.l);
            compile_expr(C, n->u.bin.r);
            OpCode op;
            switch (n->u.bin.op) {
                case A_ADD: op = OP_ADD; break;
                case A_SUB: op = OP_SUB; break;
                case A_MUL: op = OP_MUL; break;
                case A_DIV: op = OP_DIV; break;
                case A_MOD: op = OP_MOD; break;
                case A_EQ:  op = OP_EQ;  break;
                case A_NE:  op = OP_NE;  break;
                case A_LT:  op = OP_LT;  break;
                case A_LE:  op = OP_LE;  break;
                case A_GT:  op = OP_GT;  break;
                case A_GE:  op = OP_GE;  break;
                default:    op = OP_ADD; break;
            }
            emit(C, op, n->line);
            pop(C);
            return;
        }
        case N_ASSIGN: {
            Node *t = n->u.assign.target;
            if (t->k == N_IDENT) {
                int l = resolve_local(C, t->u.name);
                int u = (l == -1) ? resolve_upvalue(C, t->u.name, n->line) : -1;
                compile_expr(C, n->u.assign.value);
                if (l != -1) {
                    emit(C, OP_SET_LOCAL, n->line);
                    emit_u8(C, (uint8_t)l, n->line);
                } else if (u != -1) {
                    emit(C, OP_SET_UPVALUE, n->line);
                    emit_u8(C, (uint8_t)u, n->line);
                } else {
                    emit2(C, OP_SET_GLOBAL,
                          (uint16_t)name_idx(C, t->u.name, n->line), n->line);
                }
                return;
            }
            if (t->k == N_FIELD) {
                compile_expr(C, t->u.field.obj);
                compile_expr(C, n->u.assign.value);
                emit2(C, OP_SET_FIELD, (uint16_t)name_idx(C, t->u.field.name, n->line), n->line);
                pop(C);
                return;
            }
            if (t->k == N_INDEX) {
                compile_expr(C, t->u.index.obj);
                compile_expr(C, t->u.index.index);
                compile_expr(C, n->u.assign.value);
                emit(C, OP_SET_INDEX, n->line);
                pop(C); pop(C);
                return;
            }
            cerr(C, n->line, "некорректная цель присваивания");
            return;
        }
        case N_INDEX:
            compile_expr(C, n->u.index.obj);
            compile_expr(C, n->u.index.index);
            emit(C, OP_GET_INDEX, n->line);
            return;
        case N_FIELD:
            compile_expr(C, n->u.field.obj);
            emit2(C, OP_GET_FIELD, (uint16_t)name_idx(C, n->u.field.name, n->line), n->line);
            return;
        case N_CALL:
            compile_call(C, n);
            return;
        case N_ARR:
            for (int i = 0; i < n->u.arr.n; i++) compile_expr(C, n->u.arr.items[i]);
            emit2(C, OP_NEW_ARRAY, (uint16_t)n->u.arr.n, n->line);
            C->sd -= n->u.arr.n - 1;
            return;
        case N_MAP:
            for (int i = 0; i < n->u.map.n; i++) {
                emit2(C, OP_CONST, (uint16_t)const_idx(C, v_str(n->u.map.keys[i]), n->line), n->line);
                push(C);
                compile_expr(C, n->u.map.vals[i]);
            }
            emit2(C, OP_NEW_MAP, (uint16_t)n->u.map.n, n->line);
            C->sd -= 2 * n->u.map.n - 1;
            return;
        case N_FN:
            compile_function(C, n->u.fn.fn, n->line);
            return;
        default:
            cerr(C, n->line, "неизвестное выражение");
            return;
    }
}

/* ---------------- функции ---------------- */

static void compile_function(Compiler *C, FnDecl *fd, int line)
{
    Compiler sub;
    memset(&sub, 0, sizeof sub);
    sub.enclosing = C;
    sub.fn = function_new(fd->name);
    sub.is_script = 0;
    sub.err = C->err;
    sub.errsz = C->errsz;
    sub.active_prev = g_active;
    g_active = &sub;

    sub.locals[0].name = "";
    sub.locals[0].depth = 0;
    sub.local_count = 1;

    for (int i = 0; i < fd->nparams; i++) {
        add_local(&sub, fd->params[i].name, line);
        sub.locals[sub.local_count - 1].depth = 0;
    }
    sub.fn->arity = fd->nparams;

    /* значения по умолчанию: если аргумент == пусто, подставить */
    for (int i = 0; i < fd->nparams; i++) {
        if (!fd->params[i].def) continue;
        int slot = i + 1;
        emit(&sub, OP_GET_LOCAL, line);
        emit_u8(&sub, (uint8_t)slot, line);
        int jnil = emit_jump(&sub, OP_IF_NIL_JUMP, line);
        int jend = emit_jump(&sub, OP_JUMP, line);
        patch_jump(&sub, jnil);
        compile_expr(&sub, fd->params[i].def);
        emit(&sub, OP_SET_LOCAL, line);
        emit_u8(&sub, (uint8_t)slot, line);
        emit(&sub, OP_POP, line); pop(&sub);
        patch_jump(&sub, jend);
    }

    for (int i = 0; i < fd->body->u.block.n; i++)
        compile_stmt(&sub, fd->body->u.block.items[i]);
    emit(&sub, OP_NIL, fd->body->line);
    push(&sub);
    emit(&sub, OP_RETURN, fd->body->line);

    g_active = sub.active_prev;
    if (sub.errored) C->errored = 1;

    Val *fnval = v_fn(sub.fn);
    int idx = const_idx(C, fnval, line);
    emit2(C, OP_CLOSURE, (uint16_t)idx, line);
    push(C);
    for (int i = 0; i < sub.fn->upvalue_count; i++) {
        emit_u8(C, sub.upvalues[i].is_local ? 1 : 0, line);
        emit_u8(C, sub.upvalues[i].index, line);
    }
}

/* ---------------- операторы ---------------- */

static void pop_to(Compiler *C, int depth, int line)
{
    for (int i = C->local_count - 1; i >= 0; i--) {
        if (C->locals[i].depth <= depth) break;
        if (C->locals[i].captured) emit(C, OP_CLOSE_UPVALUE, line);
        else                       emit(C, OP_POP, line);
        pop(C);
    }
}

static void compile_block(Compiler *C, Node *b)
{
    begin_scope(C);
    for (int i = 0; i < b->u.block.n; i++) {
        compile_stmt(C, b->u.block.items[i]);
        if (C->errored) break;
    }
    end_scope(C, b->line);
}

static void compile_fn_decl(Compiler *C, Node *n)
{
    FnDecl *fd = n->u.fn.fn;
    if (C->is_script && C->scope_depth == 0) {
        compile_function(C, fd, n->line);
        emit2(C, OP_DEFINE_GLOBAL, (uint16_t)name_idx(C, fd->name, n->line), n->line);
        pop(C);
        return;
    }
    add_local(C, fd->name, n->line);
    int slot = C->local_count - 1;
    compile_function(C, fd, n->line);
    emit(C, OP_SET_LOCAL, n->line);
    emit_u8(C, (uint8_t)slot, n->line);
    emit(C, OP_POP, n->line);
    pop(C);
    C->locals[slot].depth = C->scope_depth;
}

static void finish_loop(Compiler *C, LoopCtx *lp, int jexit, int line, int cont_target)
{
    C->loop = lp->prev;
    if (jexit >= 0) {
        patch_jump(C, jexit);
        emit(C, OP_POP, line); pop(C);   /* условие */
    }
    for (int i = 0; i < lp->nbreaks; i++) patch_jump(C, lp->breaks[i]);
    if (lp->forward_cont)
        for (int i = 0; i < lp->nfwd; i++) patch_jump(C, lp->fwd[i]);
    else
        for (int i = 0; i < lp->ncontinues; i++)
            patch_loop_to(C, lp->continues[i], cont_target);
}

static void compile_while(Compiler *C, Node *n)
{
    int start = (int)C->fn->chunk.count;
    compile_expr(C, n->u.whil.cond);
    int jexit = emit_jump(C, OP_JUMP_IF_FALSE, n->line);
    emit(C, OP_POP, n->line); pop(C);

    LoopCtx lp; memset(&lp, 0, sizeof lp);
    lp.start = start;
    lp.scope_depth = C->scope_depth;
    lp.sd_start = C->sd;
    lp.prev = C->loop;
    C->loop = &lp;

    compile_block(C, n->u.whil.body);
    emit_loop_to(C, start, n->line);

    finish_loop(C, &lp, jexit, n->line, start);
}

static void compile_loop_stmt(Compiler *C, Node *n)
{
    int start = (int)C->fn->chunk.count;
    LoopCtx lp; memset(&lp, 0, sizeof lp);
    lp.start = start;
    lp.scope_depth = C->scope_depth;
    lp.sd_start = C->sd;
    lp.prev = C->loop;
    C->loop = &lp;

    compile_block(C, n->u.loopb.body);
    emit_loop_to(C, start, n->line);

    finish_loop(C, &lp, -1, n->line, start);
}

static void compile_for(Compiler *C, Node *n)
{
    begin_scope(C);
    if (n->u.fors.init) compile_stmt(C, n->u.fors.init);

    int start = (int)C->fn->chunk.count;
    if (n->u.fors.cond) compile_expr(C, n->u.fors.cond);
    else { emit(C, OP_TRUE, n->line); push(C); }
    int jexit = emit_jump(C, OP_JUMP_IF_FALSE, n->line);
    emit(C, OP_POP, n->line); pop(C);

    LoopCtx lp; memset(&lp, 0, sizeof lp);
    lp.start = start;
    lp.scope_depth = C->scope_depth;
    lp.sd_start = C->sd;
    lp.forward_cont = 1;
    lp.prev = C->loop;
    C->loop = &lp;

    compile_block(C, n->u.fors.body);

    int stepat = (int)C->fn->chunk.count;
    if (n->u.fors.step) {
        compile_expr(C, n->u.fors.step);
        emit(C, OP_POP, n->line);
        pop(C);
    }
    emit_loop_to(C, start, n->line);

    finish_loop(C, &lp, jexit, n->line, stepat);
    end_scope(C, n->line);
}

static void compile_foreach(Compiler *C, Node *n)
{
    begin_scope(C);
    compile_expr(C, n->u.each.iter);
    add_local(C, "\x01ит", n->line);
    C->locals[C->local_count - 1].depth = C->scope_depth;
    int it = C->local_count - 1;
    emit2(C, OP_CONST, (uint16_t)const_idx(C, v_int(0), n->line), n->line);
    push(C);
    add_local(C, "\x01и", n->line);
    C->locals[C->local_count - 1].depth = C->scope_depth;
    int ii = C->local_count - 1;

    int start = (int)C->fn->chunk.count;
    emit(C, OP_GET_LOCAL, n->line); emit_u8(C, (uint8_t)ii, n->line); push(C);
    emit(C, OP_GET_LOCAL, n->line); emit_u8(C, (uint8_t)it, n->line); push(C);
    emit(C, OP_LENGTH, n->line);
    emit(C, OP_LT, n->line); pop(C);
    int jexit = emit_jump(C, OP_JUMP_IF_FALSE, n->line);
    emit(C, OP_POP, n->line); pop(C);

    LoopCtx lp; memset(&lp, 0, sizeof lp);
    lp.start = start;
    lp.scope_depth = C->scope_depth;
    lp.sd_start = C->sd;
    lp.forward_cont = 1;
    lp.prev = C->loop;
    C->loop = &lp;

    begin_scope(C);
    emit(C, OP_GET_LOCAL, n->line); emit_u8(C, (uint8_t)it, n->line); push(C);
    emit(C, OP_GET_LOCAL, n->line); emit_u8(C, (uint8_t)ii, n->line); push(C);
    emit(C, OP_GET_INDEX, n->line); pop(C);
    add_local(C, n->u.each.var, n->line);
    C->locals[C->local_count - 1].depth = C->scope_depth;

    compile_block(C, n->u.each.body);
    end_scope(C, n->line);   /* переменная цикла */

    int stepat = (int)C->fn->chunk.count;
    emit(C, OP_GET_LOCAL, n->line); emit_u8(C, (uint8_t)ii, n->line); push(C);
    emit2(C, OP_CONST, (uint16_t)const_idx(C, v_int(1), n->line), n->line); push(C);
    emit(C, OP_ADD, n->line); pop(C);
    emit(C, OP_SET_LOCAL, n->line); emit_u8(C, (uint8_t)ii, n->line);
    emit(C, OP_POP, n->line); pop(C);
    emit_loop_to(C, start, n->line);

    finish_loop(C, &lp, jexit, n->line, stepat);
    end_scope(C, n->line);   /* скрытые локалы */
}

static void compile_stmt(Compiler *C, Node *n)
{
    if (C->errored || !n) return;
    switch (n->k) {
        case N_VAR: {
            if (n->u.var.value) compile_expr(C, n->u.var.value);
            else { emit(C, OP_NIL, n->line); push(C); }
            if (C->is_script && C->scope_depth == 0) {
                emit2(C, OP_DEFINE_GLOBAL, (uint16_t)name_idx(C, n->u.var.name, n->line), n->line);
                pop(C);
            } else {
                add_local(C, n->u.var.name, n->line);
                C->locals[C->local_count - 1].depth = C->scope_depth;
            }
            return;
        }
        case N_FNDECL:
            compile_fn_decl(C, n);
            return;
        case N_IF: {
            compile_expr(C, n->u.iff.cond);
            int jelse = emit_jump(C, OP_JUMP_IF_FALSE, n->line);
            emit(C, OP_POP, n->line); pop(C);
            compile_stmt(C, n->u.iff.then_b);
            if (n->u.iff.else_b) {
                int jend = emit_jump(C, OP_JUMP, n->line);
                patch_jump(C, jelse);
                emit(C, OP_POP, n->line); pop(C);
                compile_stmt(C, n->u.iff.else_b);
                patch_jump(C, jend);
            } else {
                patch_jump(C, jelse);
                emit(C, OP_POP, n->line); pop(C);
            }
            return;
        }
        case N_WHILE:    compile_while(C, n); return;
        case N_LOOP:     compile_loop_stmt(C, n); return;
        case N_FOR:      compile_for(C, n); return;
        case N_FOREACH:  compile_foreach(C, n); return;
        case N_BREAK: {
            if (!C->loop) { cerr(C, n->line, "'прерви' вне цикла"); return; }
            pop_to(C, C->loop->scope_depth, n->line);
            C->sd = C->loop->sd_start;
            if (C->loop->nbreaks < 64)
                C->loop->breaks[C->loop->nbreaks++] = emit_jump(C, OP_JUMP, n->line);
            return;
        }
        case N_CONTINUE: {
            if (!C->loop) { cerr(C, n->line, "'продолжи' вне цикла"); return; }
            pop_to(C, C->loop->scope_depth, n->line);
            C->sd = C->loop->sd_start;
            if (C->loop->forward_cont) {
                if (C->loop->nfwd < 64)
                    C->loop->fwd[C->loop->nfwd++] = emit_jump(C, OP_JUMP, n->line);
            } else {
                if (C->loop->ncontinues < 64)
                    C->loop->continues[C->loop->ncontinues++] = emit_continue(C, n->line);
            }
            return;
        }
        case N_RETURN: {
            if (n->u.ret.has) compile_expr(C, n->u.ret.value);
            else { emit(C, OP_NIL, n->line); push(C); }
            emit(C, OP_RETURN, n->line);
            return;
        }
        case N_IMPORT: {
            emit2(C, OP_IMPORT, (uint16_t)const_idx(C, v_str(n->u.imp.path), n->line), n->line);
            push(C);
            char alias[256];
            if (n->u.imp.alias) {
                snprintf(alias, sizeof alias, "%s", n->u.imp.alias);
            } else {
                const char *p = n->u.imp.path;
                const char *slash = strrchr(p, '/');
                const char *base = slash ? slash + 1 : p;
                snprintf(alias, sizeof alias, "%s", base);
                char *dot = strrchr(alias, '.');
                if (dot && strcmp(dot, ".goida") == 0) *dot = '\0';
            }
            emit2(C, OP_DEFINE_GLOBAL, (uint16_t)name_idx(C, alias, n->line), n->line);
            pop(C);
            return;
        }
        case N_BLOCK:
            compile_block(C, n);
            return;
        case N_EXPR:
            compile_expr(C, n->u.expr.e);
            emit(C, OP_POP, n->line);
            pop(C);
            return;
        default:
            cerr(C, n->line, "неизвестный оператор");
            return;
    }
}

Function *compile(const char *modname, Node *ast, char *err, size_t errsz)
{
    if (errsz) err[0] = '\0';
    Compiler C;
    memset(&C, 0, sizeof C);
    char name[300];
    snprintf(name, sizeof name, "<%s>", modname);
    C.fn = function_new(name);
    C.is_script = 1;
    C.err = err;
    C.errsz = errsz;
    C.locals[0].name = "";
    C.locals[0].depth = 0;
    C.local_count = 1;
    C.active_prev = g_active;
    g_active = &C;

    for (int i = 0; i < ast->u.block.n; i++) {
        compile_stmt(&C, ast->u.block.items[i]);
        if (C.errored) break;
    }
    emit(&C, OP_NIL, ast->line);
    push(&C);
    emit(&C, OP_RETURN, ast->line);

    g_active = C.active_prev;

    if (C.errored) {
        function_free(C.fn);
        return NULL;
    }
    return C.fn;
}
