/* std_time.c — модуль «время». */
#include "stdlib.h"
#include "native.h"

#include <time.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(long long ms) { Sleep((DWORD)ms); }
#else
#include <unistd.h>
static void sleep_ms(long long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif

static double now_ms(void)
{
#ifdef _WIN32
    return (double)GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

static Val *n_время(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self; (void)argc; (void)argv;
    return v_float((double)time(NULL));
}

static Val *n_миллисекунды(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self; (void)argc; (void)argv;
    return v_float(now_ms());
}

static Val *n_спи(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    long long ms = nat_int(I, argc, argv, 0, "спи", 0);
    if (ms > 0) sleep_ms(ms);
    return v_nil();
}

static Val *n_дата(Interp *I, Val *self, int argc, Val **argv)
{
    (void)I; (void)self; (void)argc; (void)argv;
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    Val *m = v_map();
    map_set(m, "год",    v_int(tmv.tm_year + 1900));
    map_set(m, "месяц",  v_int(tmv.tm_mon + 1));
    map_set(m, "день",   v_int(tmv.tm_mday));
    map_set(m, "час",    v_int(tmv.tm_hour));
    map_set(m, "минута", v_int(tmv.tm_min));
    map_set(m, "секунда", v_int(tmv.tm_sec));
    return m;
}

void std_time_install(Interp *I, Table *t)
{
    (void)I;
    table_set(t, "время",        v_native("время", n_время));
    table_set(t, "миллисекунды", v_native("миллисекунды", n_миллисекунды));
    table_set(t, "спи",          v_native("спи", n_спи));
    table_set(t, "дата",         v_native("дата", n_дата));
}
