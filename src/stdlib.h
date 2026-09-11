/* stdlib.h — установка модулей стандартной библиотеки в таблицу. */
#ifndef GOIDA_STDLIB_H
#define GOIDA_STDLIB_H

#include "vm.h"

/* Ядро: автоматически в каждом модуле (вывод, преобразования, мем-слой). */
void std_core_install(Interp *I, Table *t);

void std_math_install(Interp *I, Table *t);     /* «математика» */
void std_strings_install(Interp *I, Table *t);  /* «строки»     */
void std_arrays_install(Interp *I, Table *t);   /* «массивы»    */
void std_files_install(Interp *I, Table *t);    /* «файлы»      */
void std_time_install(Interp *I, Table *t);     /* «время»      */
void std_system_install(Interp *I, Table *t);   /* «система»    */
void std_random_install(Interp *I, Table *t);   /* «случай»     */
void sock_install(Interp *I, Table *t);         /* «сокеты»     */

#endif /* GOIDA_STDLIB_H */
