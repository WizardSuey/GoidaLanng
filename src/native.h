/* native.h — хелперы проверки аргументов для встроенных функций. */
#ifndef GOIDA_NATIVE_H
#define GOIDA_NATIVE_H

#include "vm.h"

long long   nat_int(Interp *I, int argc, Val **argv, int i, const char *fn, int line);
double      nat_num(Interp *I, int argc, Val **argv, int i, const char *fn, int line);
const char *nat_str(Interp *I, int argc, Val **argv, int i, const char *fn, int line);
Val        *nat_arr(Interp *I, int argc, Val **argv, int i, const char *fn, int line);
Val        *nat_map(Interp *I, int argc, Val **argv, int i, const char *fn, int line);
Val        *nat_val(Interp *I, int argc, Val **argv, int i, const char *fn, int line);
int         nat_bool(Interp *I, int argc, Val **argv, int i, const char *fn, int line);
void        nat_min(Interp *I, int argc, int need, const char *fn, int line);

#endif /* GOIDA_NATIVE_H */
