/* forward.h — только предварительные объявления типов.
 * Нужен, чтобы модули могли ссылаться друг на друга без циклических include.
 */
#ifndef GOIDA_FORWARD_H
#define GOIDA_FORWARD_H

typedef struct Val     Val;
typedef struct Node    Node;
typedef struct FnDecl  FnDecl;
typedef struct Interp  Interp;
typedef struct Function Function;
typedef struct Module  Module;
typedef struct Table   Table;
typedef struct Chunk   Chunk;

#endif /* GOIDA_FORWARD_H */
