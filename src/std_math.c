/* std_math.c — модуль «математика». */
#include "stdlib.h"
#include "native.h"

#include <math.h>

#define M(fn, expr) \
static Val *n_##fn(Interp *I, Val *self, int argc, Val **argv) \
{ (void)self; double x = nat_num(I, argc, argv, 0, #fn, 0); return v_float(expr); }

M(корень,  sqrt(x))
M(синус,   sin(x))
M(косинус, cos(x))
M(тангенс, tan(x))
M(лог,     log(x))
M(лог10,   log10(x))
M(эксп,    exp(x))
M(пол,     floor(x))
M(потолок, ceil(x))

static Val *n_степень(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    double a = nat_num(I, argc, argv, 0, "степень", 0);
    double b = nat_num(I, argc, argv, 1, "степень", 0);
    double r = pow(a, b);
    if (argv[0]->t == V_INT && argv[1]->t == V_INT && b >= 0 && r == floor(r))
        return v_int((long long)r);
    return v_float(r);
}

static Val *n_атан2(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    double a = nat_num(I, argc, argv, 0, "атан2", 0);
    double b = nat_num(I, argc, argv, 1, "атан2", 0);
    return v_float(atan2(a, b));
}

void std_math_install(Interp *I, Table *t)
{
    (void)I;
    table_set(t, "пи", v_float(3.14159265358979323846));
    table_set(t, "е",  v_float(2.71828182845904523536));
    table_set(t, "корень",  v_native("корень",  n_корень));
    table_set(t, "синус",   v_native("синус",   n_синус));
    table_set(t, "косинус", v_native("косинус", n_косинус));
    table_set(t, "тангенс", v_native("тангенс", n_тангенс));
    table_set(t, "лог",     v_native("лог",     n_лог));
    table_set(t, "лог10",   v_native("лог10",   n_лог10));
    table_set(t, "эксп",    v_native("эксп",    n_эксп));
    table_set(t, "пол",     v_native("пол",     n_пол));
    table_set(t, "потолок", v_native("потолок", n_потолок));
    table_set(t, "степень", v_native("степень", n_степень));
    table_set(t, "атан2",   v_native("атан2",   n_атан2));
}
