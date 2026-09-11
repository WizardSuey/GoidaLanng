/* std_core.c — ядро стандартки: вывод, преобразования, коллекции.
 * Мем-слой: «гойда»/«айда» (вывод), «шепни»/«кринж» (stderr),
 * «краш»/«паника» (ошибка), «база» (assert), «понятно» (no-op),
 * «пофиг» (сброс), «респект» (repr-вывод), константы ZOV/ГОЙДА.
 * Слой нейтрален: только интернет-фольклор, без политики и лексики.
 */
#include "stdlib.h"
#include "gc.h"
#include "native.h"
#include "util.h"
#include "utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------------- вывод ---------------- */

static void print_args(Interp *I, int argc, Val **argv, FILE *out, int nl)
{
    (void)I;
    for (int i = 0; i < argc; i++) {
        if (i) fputc(' ', out);
        char *s = val_to_str(argv[i]);
        fputs(s, out);
        free(s);
    }
    if (nl) fputc('\n', out);
    fflush(out);
}

static Val *n_goida(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    print_args(I, argc, argv, I->out, 1);
    return v_nil();
}

static Val *n_pishi(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    print_args(I, argc, argv, I->out, 0);
    return v_nil();
}

static Val *n_shepni(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    print_args(I, argc, argv, I->err, 1);
    return v_nil();
}

static Val *n_krinzh(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    fputs("кринж: ", I->err);
    print_args(I, argc, argv, I->err, 1);
    return v_nil();
}

static Val *n_krash(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    char *s = argc ? val_to_str(argv[0]) : xstrdup("краш");
    rt_error(I, 0, "паника: %s", s);
    return v_nil();
}

static Val *n_baza(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    nat_min(I, argc, 1, "база", 0);
    if (!truthy(argv[0])) {
        char *s = argc > 1 ? val_to_str(argv[1]) : xstrdup("условие ложно");
        rt_error(I, 0, "база не сошлась: %s", s);
    }
    return v_nil();
}

static Val *n_ponyatno(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self; (void)argc; (void)argv;
    return v_nil();   /* понятно, идём дальше */
}

static Val *n_pofig(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self; (void)argc; (void)argv;
    return v_nil();
}

static Val *n_respekt(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    for (int i = 0; i < argc; i++) {
        if (i) fputc(' ', I->out);
        char *s = val_repr(argv[i]);
        fputs(s, I->out);
        free(s);
    }
    fputc('\n', I->out);
    fflush(I->out);
    return v_nil();
}

static Val *n_sprosi(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    if (argc) print_args(I, argc, argv, I->out, 0);
    char buf[4096];
    if (!fgets(buf, sizeof buf, I->in)) return v_str("");
    size_t l = strlen(buf);
    while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = '\0';
    return v_strn(buf, l);
}

/* ---------------- преобразования ---------------- */

static Val *n_str(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self;
    nat_min(I, argc, 1, "строка", 0);
    char *s = val_to_str(argv[0]);
    Val *r = v_str(s);
    free(s);
    return r;
}

static Val *n_repr(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self;
    nat_min(I, argc, 1, "repr", 0);
    char *s = val_repr(argv[0]);
    Val *r = v_str(s);
    free(s);
    return r;
}

static Val *n_chislo(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *v = nat_val(I, argc, argv, 0, "число", 0);
    if (v->t == V_INT || v->t == V_FLOAT) return v;
    if (v->t == V_BOOL) return v_int(v->u.b);
    if (v->t == V_STR) {
        char *end = NULL;
        double d = strtod(v->u.s.p, &end);
        if (end && *end == '\0' && end != v->u.s.p) {
            if (d == (double)(long long)d && !strpbrk(v->u.s.p, ".eE"))
                return v_int((long long)d);
            return v_float(d);
        }
        rt_error(I, 0, "число: строка '%s' не является числом", v->u.s.p);
    }
    rt_error(I, 0, "число: неприменимо к типу %s", type_name(v));
    return v_nil();
}

static Val *n_tseloe(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *v = nat_val(I, argc, argv, 0, "целое", 0);
    if (v->t == V_INT) return v;
    if (v->t == V_FLOAT) return v_int((long long)v->u.f);
    if (v->t == V_BOOL) return v_int(v->u.b);
    rt_error(I, 0, "целое: неприменимо к типу %s", type_name(v));
    return v_nil();
}

static Val *n_drobnoe(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *v = nat_val(I, argc, argv, 0, "дробное", 0);
    if (v->t == V_FLOAT) return v;
    if (v->t == V_INT) return v_float((double)v->u.i);
    rt_error(I, 0, "дробное: неприменимо к типу %s", type_name(v));
    return v_nil();
}

static Val *n_tip(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self;
    nat_min(I, argc, 1, "тип", 0);
    return v_str(type_name(argv[0]));
}

static Val *n_dlina(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *v = nat_val(I, argc, argv, 0, "длина", 0);
    if (v->t == V_STR) return v_int((long long)utf8_strlen(v->u.s.p, v->u.s.len));
    if (v->t == V_ARR) return v_int((long long)v->u.a.len);
    if (v->t == V_MAP) return v_int((long long)map_count(v));
    rt_error(I, 0, "длина: неприменимо к типу %s", type_name(v));
    return v_nil();
}

/* ---------------- коллекции ---------------- */

static Val *n_dobav(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "добавь", 0);
    nat_min(I, argc, 2, "добавь", 0);
    arr_push(a, argv[1]);
    return a;
}

static Val *n_vykin(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *a = nat_arr(I, argc, argv, 0, "выкинь", 0);
    if (a->u.a.len == 0) rt_error(I, 0, "выкинь: массив пуст");
    return a->u.a.v[--a->u.a.len];
}

static Val *n_kluchi(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self;
    Val *m = nat_map(I, argc, argv, 0, "ключи", 0);
    Val *a = v_arr();
    gc_add_root_slot(&a);
    for (MapEntry *e = m->u.m.head; e; e = e->next_o)
        arr_push(a, v_str(e->key));
    gc_remove_root_slot(&a);
    return a;
}

static Val *n_znach(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self;
    Val *m = nat_map(I, argc, argv, 0, "значения", 0);
    Val *a = v_arr();
    gc_add_root_slot(&a);
    for (MapEntry *e = m->u.m.head; e; e = e->next_o)
        arr_push(a, e->val);
    gc_remove_root_slot(&a);
    return a;
}

static Val *n_est(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self;
    Val *m = nat_map(I, argc, argv, 0, "есть", 0);
    const char *k = nat_str(I, argc, argv, 1, "есть", 0);
    return v_bool(map_has(m, k));
}

static Val *n_min(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    nat_min(I, argc, 2, "мин", 0);
    Val *best = argv[0];
    double bestv = nat_num(I, argc, argv, 0, "мин", 0);
    for (int i = 1; i < argc; i++) {
        double a = nat_num(I, argc, argv, i, "мин", 0);
        if (a < bestv) { best = argv[i]; bestv = a; }
    }
    return best;
}

static Val *n_max(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    nat_min(I, argc, 2, "макс", 0);
    Val *best = argv[0];
    for (int i = 1; i < argc; i++) {
        double a = nat_num(I, argc, argv, i, "макс", 0);
        double bestv = (best->t == V_INT) ? (double)best->u.i : best->u.f;
        if (a > bestv) best = argv[i];
    }
    return best;
}

static Val *n_modul(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    double x = nat_num(I, argc, argv, 0, "модуль", 0);
    if (argv[0]->t == V_INT) return v_int(llabs(argv[0]->u.i));
    return v_float(fabs(x));
}

static Val *n_okrugli(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    Val *v = nat_val(I, argc, argv, 0, "округли", 0);
    if (v->t == V_INT) return v;
    if (v->t == V_FLOAT) return v_int(llround(v->u.f));
    rt_error(I, 0, "округли: неприменимо к типу %s", type_name(v));
    return v_nil();
}

/* ---------------- установка ---------------- */

static void def(Table *t, const char *name, NativeFn fn)
{
    table_set(t, name, v_native(name, fn));
}

void std_core_install(Interp *I, Table *t)
{
    (void)I;
    /* вывод */
    def(t, "гойда",  n_goida);
    def(t, "айда",   n_pishi);
    def(t, "пиши",   n_pishi);
    def(t, "шепни",  n_shepni);
    def(t, "кринж",  n_krinzh);
    def(t, "спроси", n_sprosi);
    def(t, "респект", n_respekt);
    /* управление */
    def(t, "краш",   n_krash);
    def(t, "паника", n_krash);
    def(t, "база",   n_baza);
    def(t, "проверь", n_baza);
    def(t, "понятно", n_ponyatno);
    def(t, "пофиг",  n_pofig);
    /* преобразования */
    def(t, "строка",  n_str);
    def(t, "repr",    n_repr);
    def(t, "число",   n_chislo);
    def(t, "целое",   n_tseloe);
    def(t, "дробное", n_drobnoe);
    def(t, "тип",     n_tip);
    def(t, "длина",   n_dlina);
    /* коллекции */
    def(t, "добавь",  n_dobav);
    def(t, "выкинь",  n_vykin);
    def(t, "ключи",   n_kluchi);
    def(t, "значения", n_znach);
    def(t, "есть",    n_est);
    def(t, "мин",     n_min);
    def(t, "макс",    n_max);
    def(t, "модуль",  n_modul);
    def(t, "округли", n_okrugli);
    /* мем-константы (нейтральный интернет-фольклор) */
    table_set(t, "ZOV", v_str("ZOV"));
    table_set(t, "ГОЙДА", v_str("ГОЙДА!"));
    table_set(t, "версия", v_str("0.1.0"));
}
