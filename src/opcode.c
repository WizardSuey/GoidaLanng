/* opcode.c — имена инструкций для отладки/дизассемблера. */
#include "opcode.h"

const char *op_name(OpCode op)
{
    switch (op) {
        case OP_CONST:         return "CONST";
        case OP_NIL:           return "NIL";
        case OP_TRUE:          return "TRUE";
        case OP_FALSE:         return "FALSE";
        case OP_POP:           return "POP";
        case OP_DUP:           return "DUP";
        case OP_SWAP:          return "SWAP";
        case OP_GET_LOCAL:     return "GET_LOCAL";
        case OP_SET_LOCAL:     return "SET_LOCAL";
        case OP_GET_GLOBAL:    return "GET_GLOBAL";
        case OP_SET_GLOBAL:    return "SET_GLOBAL";
        case OP_DEFINE_GLOBAL: return "DEFINE_GLOBAL";
        case OP_GET_UPVALUE:   return "GET_UPVALUE";
        case OP_SET_UPVALUE:   return "SET_UPVALUE";
        case OP_CLOSE_UPVALUE: return "CLOSE_UPVALUE";
        case OP_ADD:           return "ADD";
        case OP_SUB:           return "SUB";
        case OP_MUL:           return "MUL";
        case OP_DIV:           return "DIV";
        case OP_MOD:           return "MOD";
        case OP_NEG:           return "NEG";
        case OP_NOT:           return "NOT";
        case OP_EQ:            return "EQ";
        case OP_NE:            return "NE";
        case OP_LT:            return "LT";
        case OP_LE:            return "LE";
        case OP_GT:            return "GT";
        case OP_GE:            return "GE";
        case OP_LENGTH:        return "LENGTH";
        case OP_JUMP:          return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_IF_NIL_JUMP:   return "IF_NIL_JUMP";
        case OP_LOOP:          return "LOOP";
        case OP_CLOSURE:       return "CLOSURE";
        case OP_CALL:          return "CALL";
        case OP_CALLM:         return "CALLM";
        case OP_RETURN:        return "RETURN";
        case OP_NEW_ARRAY:     return "NEW_ARRAY";
        case OP_NEW_MAP:       return "NEW_MAP";
        case OP_GET_INDEX:     return "GET_INDEX";
        case OP_SET_INDEX:     return "SET_INDEX";
        case OP_GET_FIELD:     return "GET_FIELD";
        case OP_SET_FIELD:     return "SET_FIELD";
        case OP_IMPORT:        return "IMPORT";
        case OP_HALT:          return "HALT";
    }
    return "?";
}
