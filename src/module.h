/* module.h — менеджер модулей: встроенные + .goida-файлы + $GOIDA_PATH. */
#ifndef GOIDA_MODULE_H
#define GOIDA_MODULE_H

#include "forward.h"
#include "table.h"

struct Module {
    char  *name;      /* «сокеты», «моя_библиотека», «<main>» */
    char  *path;      /* путь к файлу или NULL для встроенных */
    Table  globals;
    Val   *main_fn;   /* V_FN или NULL */
    Val   *ns;        /* пространство имён (V_MAP) для импортёров */
    struct Module *next;
};

/* Создаёт модуль-скрипт с ядром стандартки в глобалах и регистрирует его. */
Module *module_new_script(Interp *I, const char *name, const char *path);

/* Загрузка модуля по имени/пути из оператора «подключить».
 * Возвращает V_MAP — пространство имён. Ошибки — rt_error. */
Val *module_load(Interp *I, Module *from, const char *spec);

/* Поиск уже загруженного встроенного модуля по имени (или NULL). */
Module *module_find(const char *name);

/* GC-корни всех модулей. */
void module_gc_mark(void);

#endif /* GOIDA_MODULE_H */
