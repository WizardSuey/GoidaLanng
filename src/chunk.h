/* chunk.h — байткод: код, таблицы констант и отладочные строки. */
#ifndef GOIDA_CHUNK_H
#define GOIDA_CHUNK_H

#include <stdint.h>
#include <stddef.h>
#include "forward.h"
#include "opcode.h"

struct Chunk {
    uint8_t *code;
    int     *lines;      /* номер строки исходника на каждый байт */
    size_t   count, cap;

    Val    **consts;     /* пул констант (корень GC) */
    size_t   consts_n, consts_cap;
};

struct Function {
    Chunk  chunk;
    char  *name;
    int    arity;         /* число параметров */
    int    stack_max;     /* максимум стека, байт */
    int    upvalue_count;
};

void chunk_init(Chunk *c);
void chunk_free(Chunk *c);
void chunk_write(Chunk *c, uint8_t b, int line);
void chunk_write2(Chunk *c, uint8_t op, uint16_t operand, int line);
int  chunk_add_const(Chunk *c, Val *v);   /* возвращает индекс */

Function *function_new(const char *name);
void      function_free(Function *f);

#endif /* GOIDA_CHUNK_H */
