/* std_files.c — модуль «файлы»: чтение/запись, объект Файл. */
#include "stdlib.h"
#include "gc.h"
#include "native.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { FILE *f; } FileObj;

static void file_fin(void *data)
{
    FileObj *fo = (FileObj *)data;
    if (fo->f) fclose(fo->f);
    free(fo);
}

static Val *n_открой(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    const char *path = nat_str(I, argc, argv, 0, "открой", 0);
    const char *mode = argc > 1 ? nat_str(I, argc, argv, 1, "открой", 0) : "r";
    char m[4] = {0};
    size_t mi = 0;
    for (const char *p = mode; *p && mi < 2; p++) {
        if (strchr("rwab", *p) || (*p == '+' && mi)) m[mi++] = *p;
    }
    if (!m[0]) rt_error(I, 0, "открой: плохой режим '%s'", mode);
    FILE *f = fopen(path, m);
    if (!f) rt_error(I, 0, "открой: не удалось открыть '%s'", path);
    FileObj *fo = (FileObj *)xmalloc(sizeof(FileObj));
    fo->f = f;
    return v_obj("Файл", fo, file_fin);
}

static Val *n_прочитай_файл(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    const char *path = nat_str(I, argc, argv, 0, "прочитай_файл", 0);
    size_t len;
    char *data = read_file(path, &len);
    if (!data) rt_error(I, 0, "прочитай_файл: не удалось прочитать '%s'", path);
    Val *r = v_strn(data, len);
    free(data);
    return r;
}

static Val *n_запиши_файл(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    const char *path = nat_str(I, argc, argv, 0, "запиши_файл", 0);
    Val *data = nat_val(I, argc, argv, 1, "запиши_файл", 0);
    FILE *f = fopen(path, "wb");
    if (!f) rt_error(I, 0, "запиши_файл: не удалось открыть '%s'", path);
    if (data->t == V_STR) {
        fwrite(data->u.s.p, 1, data->u.s.len, f);
    } else {
        char *s = val_to_str(data);
        fputs(s, f);
        free(s);
    }
    fclose(f);
    return v_bool(1);
}

static Val *n_существует(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    const char *path = nat_str(I, argc, argv, 0, "существует", 0);
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return v_bool(1); }
    return v_bool(0);
}

/* методы объекта Файл: self в argv[0] */
static FileObj *self_file(Interp *I, Val *self)
{
    if (!self || self->t != V_OBJ || strcmp(self->u.o.klass, "Файл") != 0)
        rt_error(I, 0, "внутренняя ошибка: не Файл");
    return (FileObj *)self->u.o.data;
}

static Val *m_запиши(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argv;
    FileObj *fo = self_file(I, self);
    nat_min(I, argc, 2, "файл.запиши", 0);
    Val *data = argv[1];
    if (data->t == V_STR) fwrite(data->u.s.p, 1, data->u.s.len, fo->f);
    else {
        char *s = val_to_str(data);
        fputs(s, fo->f);
        free(s);
    }
    fflush(fo->f);
    return v_nil();
}

static Val *m_прочитай(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    FileObj *fo = self_file(I, self);
    Buf b; buf_init(&b);
    char tmp[4096];
    size_t r;
    while ((r = fread(tmp, 1, sizeof tmp, fo->f)) > 0)
        buf_add(&b, tmp, r);
    Val *res = v_strn(b.p, b.len);
    buf_free(&b);
    return res;
}

static Val *m_строка(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    FileObj *fo = self_file(I, self);
    char buf[4096];
    if (!fgets(buf, sizeof buf, fo->f)) return v_nil();
    size_t l = strlen(buf);
    while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = '\0';
    return v_strn(buf, l);
}

static Val *m_закрой(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    FileObj *fo = self_file(I, self);
    if (fo->f) { fclose(fo->f); fo->f = NULL; }
    return v_nil();
}

void std_files_install(Interp *I, Table *t)
{
    (void)I;
    table_set(t, "открой",         v_native("открой", n_открой));
    table_set(t, "прочитай_файл",  v_native("прочитай_файл", n_прочитай_файл));
    table_set(t, "запиши_файл",    v_native("запиши_файл", n_запиши_файл));
    table_set(t, "существует",     v_native("существует", n_существует));

    static const MethodEntry methods[] = {
        { "запиши",   m_запиши },
        { "прочитай", m_прочитай },
        { "строка",   m_строка },
        { "закрой",   m_закрой },
    };
    vm_register_class("Файл", methods, (int)(sizeof(methods) / sizeof(methods[0])));
}
