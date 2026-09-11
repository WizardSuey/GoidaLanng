/* chunk.c — байткод и пул констант. */
#include "chunk.h"
#include "value.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

void chunk_init(Chunk *c)
{
    c->code = NULL;
    c->lines = NULL;
    c->count = c->cap = 0;
    c->consts = NULL;
    c->consts_n = c->consts_cap = 0;
}

void chunk_free(Chunk *c)
{
    free(c->code);
    free(c->lines);
    free(c->consts);   /* сами Val-объекты принадлежат GC */
    chunk_init(c);
}

void chunk_write(Chunk *c, uint8_t b, int line)
{
    if (c->count == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 256;
        c->code = (uint8_t *)xrealloc(c->code, c->cap);
        c->lines = (int *)xrealloc(c->lines, c->cap * sizeof(int));
    }
    c->code[c->count] = b;
    c->lines[c->count] = line;
    c->count++;
}

void chunk_write2(Chunk *c, uint8_t op, uint16_t operand, int line)
{
    chunk_write(c, op, line);
    chunk_write(c, (uint8_t)((operand >> 8) & 0xFF), line);
    chunk_write(c, (uint8_t)(operand & 0xFF), line);
}

int chunk_add_const(Chunk *c, Val *v)
{
    if (c->consts_n == c->consts_cap) {
        c->consts_cap = c->consts_cap ? c->consts_cap * 2 : 16;
        c->consts = (Val **)xrealloc(c->consts, sizeof(Val *) * c->consts_cap);
    }
    c->consts[c->consts_n] = v;
    return (int)c->consts_n++;
}

Function *function_new(const char *name)
{
    Function *f = (Function *)xmalloc(sizeof(Function));
    chunk_init(&f->chunk);
    f->name = xstrdup(name);
    f->arity = 0;
    f->stack_max = 0;
    f->upvalue_count = 0;
    return f;
}

void function_free(Function *f)
{
    if (!f) return;
    chunk_free(&f->chunk);
    free(f->name);
    free(f);
}
