/* compiler.h — компилятор AST в байткод стековой ВМ. */
#ifndef GOIDA_COMPILER_H
#define GOIDA_COMPILER_H

#include "ast.h"
#include "chunk.h"

/* Компилирует программу (N_BLOCK) в функцию «<имя>».
 * Возвращает V_FN-совместимый Function* или NULL (текст ошибки в err). */
Function *compile(const char *modname, Node *ast, char *err, size_t errsz);

/* Корни GC, живущие во время компиляции (вызывает gc-колбэк ВМ). */
void compiler_gc_mark(void);

#endif /* GOIDA_COMPILER_H */
