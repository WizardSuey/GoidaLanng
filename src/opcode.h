/* opcode.h — набор инструкций стековой виртуальной машины GoidaLang. */
#ifndef GOIDA_OPCODE_H
#define GOIDA_OPCODE_H

typedef enum {
    OP_CONST,          /* u16: индекс константы            */
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DUP,
    OP_SWAP,           /* меняет местами два верхних        */

    OP_GET_LOCAL,      /* u8  */
    OP_SET_LOCAL,      /* u8  */
    OP_GET_GLOBAL,     /* u16: индекс строки-имени          */
    OP_SET_GLOBAL,     /* u16 */
    OP_DEFINE_GLOBAL,  /* u16 */

    OP_GET_UPVALUE,    /* u8  */
    OP_SET_UPVALUE,    /* u8  */
    OP_CLOSE_UPVALUE,

    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG, OP_NOT,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_LENGTH,

    OP_JUMP,           /* u16 вперёд */
    OP_JUMP_IF_FALSE,  /* u16 вперёд (не снимает условие)   */
    OP_IF_NIL_JUMP,    /* u16 вперёд, если верх == пусто    */
    OP_LOOP,           /* u16 назад  */

    OP_CLOSURE,        /* u16: индекс V_FN + пары (is_local,index) */
    OP_CALL,           /* u8: число аргументов */
    OP_CALLM,          /* u8: вызов метода (арг[0]=self) */
    OP_RETURN,

    OP_NEW_ARRAY,      /* u16: число элементов */
    OP_NEW_MAP,        /* u16: число пар       */
    OP_GET_INDEX,
    OP_SET_INDEX,
    OP_GET_FIELD,      /* u16: индекс строки-имени */
    OP_SET_FIELD,      /* u16 */

    OP_IMPORT,         /* u16: индекс строки-пути/имени */
    OP_HALT
} OpCode;

/* человеко-читаемое имя инструкции (для дизассемблера/отладки) */
const char *op_name(OpCode op);

#endif /* GOIDA_OPCODE_H */
