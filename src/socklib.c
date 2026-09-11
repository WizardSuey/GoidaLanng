/* socklib.c — модуль «сокеты»: TCP-сервер/клиент и UDP-датаграммы.
 * API (self приходит первым аргументом при вызове методом):
 *   сокеты.Сервер(адрес, порт)      -> объект Сервер
 *     .слушай(очередь=16)  .прими() -> Соединение   .закрой()  .адрес()
 *   сокеты.Клиент(хост, порт)       -> объект Соединение
 *   Соединение: .отправь(строка) .отправь_строку(s) .получи(n=4096)
 *               .получи_строку() .закрой() .адрес()
 *   сокеты.Датаграм()               -> объект Датаграм
 *     .привяжи(адрес, порт) .отправь(данные, хост, порт)
 *     .получи(n=4096) -> {данные, хост, порт}   .закрой()
 * Получи() возвращает пусто при закрытии соединения.
 */
#include "stdlib.h"
#include "native.h"
#include "sock_compat.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct { sc_sock_t s; } SockObj;

static void sock_fin(void *data)
{
    SockObj *so = (SockObj *)data;
    if (so->s != SC_INVALID) sc_close(so->s);
    free(so);
}

static SockObj *self_sock(Interp *I, Val *self, const char *klass)
{
    if (!self || self->t != V_OBJ || strcmp(self->u.o.klass, klass) != 0)
        rt_error(I, 0, "ожидался объект <%s>", klass);
    return (SockObj *)self->u.o.data;
}

/* ---------------- Сервер ---------------- */

static Val *n_Сервер(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    sc_init();
    const char *addr = nat_str(I, argc, argv, 0, "Сервер", 0);
    long long port = nat_int(I, argc, argv, 1, "Сервер", 0);
    sc_sock_t s = sc_socket(0);
    if (s == SC_INVALID) rt_error(I, 0, "Сервер: не удалось создать сокет");
    if (sc_bind(s, addr, (int)port) != 0) {
        sc_close(s);
        rt_error(I, 0, "Сервер: не удалось привязаться к %s:%lld", addr, port);
    }
    SockObj *so = (SockObj *)xmalloc(sizeof(SockObj));
    so->s = s;
    return v_obj("Сервер", so, sock_fin);
}

static Val *m_слушай(Interp *I, Val *self, int argc, Val **argv)
{
    SockObj *so = self_sock(I, self, "Сервер");
    long long backlog = argc > 1 ? nat_int(I, argc, argv, 1, "слушай", 0) : 16;
    if (sc_listen(so->s, (int)backlog) != 0)
        rt_error(I, 0, "слушай: ошибка listen");
    return v_nil();
}

static Val *m_прими(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    SockObj *so = self_sock(I, self, "Сервер");
    char peer[64] = {0};
    sc_sock_t c = sc_accept(so->s, peer, sizeof peer);
    if (c == SC_INVALID) rt_error(I, 0, "прими: ошибка accept");
    SockObj *co = (SockObj *)xmalloc(sizeof(SockObj));
    co->s = c;
    Val *conn = v_obj("Соединение", co, sock_fin);
    /* положим адрес пира в обёртку: вернём словарь? нет — метод .адрес() */
    (void)peer;
    return conn;
}

static Val *m_закрой_сервер(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    SockObj *so = self_sock(I, self, "Сервер");
    if (so->s != SC_INVALID) { sc_close(so->s); so->s = SC_INVALID; }
    return v_nil();
}

static Val *m_адрес_сервер(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    SockObj *so = self_sock(I, self, "Сервер");
    char buf[64];
    sc_local_addr(so->s, buf, sizeof buf);
    return v_str(buf);
}

/* ---------------- Соединение ---------------- */

static Val *n_Клиент(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self;
    sc_init();
    const char *host = nat_str(I, argc, argv, 0, "Клиент", 0);
    long long port = nat_int(I, argc, argv, 1, "Клиент", 0);
    sc_sock_t s = sc_socket(0);
    if (s == SC_INVALID) rt_error(I, 0, "Клиент: не удалось создать сокет");
    if (sc_connect(s, host, (int)port) != 0) {
        sc_close(s);
        rt_error(I, 0, "Клиент: не удалось соединиться с %s:%lld", host, port);
    }
    SockObj *so = (SockObj *)xmalloc(sizeof(SockObj));
    so->s = s;
    return v_obj("Соединение", so, sock_fin);
}

static Val *m_отправь(Interp *I, Val *self, int argc, Val **argv)
{
    SockObj *so = self_sock(I, self, "Соединение");
    Val *data = nat_val(I, argc, argv, 1, "отправь", 0);
    const char *p;
    size_t len;
    char *tmp = NULL;
    if (data->t == V_STR) { p = data->u.s.p; len = data->u.s.len; }
    else { tmp = val_to_str(data); p = tmp; len = strlen(tmp); }
    long r = sc_send(so->s, p, len);
    free(tmp);
    if (r < 0) rt_error(I, 0, "отправь: ошибка записи");
    return v_int(r);
}

static Val *m_отправь_строку(Interp *I, Val *self, int argc, Val **argv)
{
    SockObj *so = self_sock(I, self, "Соединение");
    const char *s = nat_str(I, argc, argv, 1, "отправь_строку", 0);
    size_t len = strlen(s);
    char *buf = (char *)xmalloc(len + 2);
    memcpy(buf, s, len);
    buf[len] = '\n';
    long r = sc_send(so->s, buf, len + 1);
    free(buf);
    if (r < 0) rt_error(I, 0, "отправь_строку: ошибка записи");
    return v_int(r);
}

static Val *m_получи(Interp *I, Val *self, int argc, Val **argv)
{
    SockObj *so = self_sock(I, self, "Соединение");
    long long cap = argc > 1 ? nat_int(I, argc, argv, 1, "получи", 0) : 4096;
    if (cap <= 0 || cap > 1 << 20) cap = 4096;
    char *buf = (char *)xmalloc((size_t)cap);
    long r = sc_recv(so->s, buf, (size_t)cap);
    if (r < 0) { free(buf); rt_error(I, 0, "получи: ошибка чтения"); }
    if (r == 0) { free(buf); return v_nil(); }   /* закрыто */
    Val *res = v_strn(buf, (size_t)r);
    free(buf);
    return res;
}

static Val *m_получи_строку(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    SockObj *so = self_sock(I, self, "Соединение");
    Buf b; buf_init(&b);
    char c;
    for (;;) {
        long r = sc_recv(so->s, &c, 1);
        if (r < 0) { buf_free(&b); rt_error(I, 0, "получи_строку: ошибка чтения"); }
        if (r == 0) {
            if (b.len == 0) { buf_free(&b); return v_nil(); }
            break;
        }
        if (c == '\n') break;
        if (c != '\r') buf_addc(&b, c);
    }
    Val *res = v_strn(b.p, b.len);
    buf_free(&b);
    return res;
}

static Val *m_закрой(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    SockObj *so = self_sock(I, self, "Соединение");
    if (so->s != SC_INVALID) { sc_close(so->s); so->s = SC_INVALID; }
    return v_nil();
}

static Val *m_адрес(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    SockObj *so = self_sock(I, self, "Соединение");
    char buf[64];
    sc_peer_addr(so->s, buf, sizeof buf);
    return v_str(buf);
}

/* ---------------- Датаграм (UDP) ---------------- */

static Val *n_Датаграм(Interp *I, Val *self, int argc, Val **argv)
{
    (void)self; (void)argc; (void)argv;
    sc_init();
    sc_sock_t s = sc_socket(1);
    if (s == SC_INVALID) rt_error(I, 0, "Датаграм: не удалось создать сокет");
    SockObj *so = (SockObj *)xmalloc(sizeof(SockObj));
    so->s = s;
    return v_obj("Датаграм", so, sock_fin);
}

static Val *m_привяжи(Interp *I, Val *self, int argc, Val **argv)
{
    SockObj *so = self_sock(I, self, "Датаграм");
    const char *addr = nat_str(I, argc, argv, 1, "привяжи", 0);
    long long port = nat_int(I, argc, argv, 2, "привяжи", 0);
    if (sc_bind(so->s, addr, (int)port) != 0)
        rt_error(I, 0, "привяжи: не удалось привязаться к %s:%lld", addr, port);
    return v_nil();
}

static Val *m_отправь_кому(Interp *I, Val *self, int argc, Val **argv)
{
    SockObj *so = self_sock(I, self, "Датаграм");
    Val *data = nat_val(I, argc, argv, 1, "отправь", 0);
    const char *host = nat_str(I, argc, argv, 2, "отправь", 0);
    long long port = nat_int(I, argc, argv, 3, "отправь", 0);
    const char *p; size_t len; char *tmp = NULL;
    if (data->t == V_STR) { p = data->u.s.p; len = data->u.s.len; }
    else { tmp = val_to_str(data); p = tmp; len = strlen(tmp); }
    long r = sc_sendto(so->s, p, len, host, (int)port);
    free(tmp);
    if (r < 0) rt_error(I, 0, "отправь: ошибка UDP-записи");
    return v_int(r);
}

static Val *m_получи_откуда(Interp *I, Val *self, int argc, Val **argv)
{
    SockObj *so = self_sock(I, self, "Датаграм");
    long long cap = argc > 1 ? nat_int(I, argc, argv, 1, "получи", 0) : 4096;
    if (cap <= 0 || cap > 1 << 20) cap = 4096;
    char *buf = (char *)xmalloc((size_t)cap);
    char host[64] = {0};
    int port = 0;
    long r = sc_recvfrom(so->s, buf, (size_t)cap, host, sizeof host, &port);
    if (r < 0) { free(buf); rt_error(I, 0, "получи: ошибка UDP-чтения"); }
    Val *m = v_map();
    map_set(m, "данные", v_strn(buf, (size_t)r));
    map_set(m, "хост", v_str(host));
    map_set(m, "порт", v_int(port));
    free(buf);
    return m;
}

static Val *m_закрой_udp(Interp *I, Val *self, int argc, Val **argv)
{
    (void)argc; (void)argv;
    SockObj *so = self_sock(I, self, "Датаграм");
    if (so->s != SC_INVALID) { sc_close(so->s); so->s = SC_INVALID; }
    return v_nil();
}

/* ---------------- установка модуля ---------------- */

void sock_install(Interp *I, Table *t)
{
    (void)I;
    table_set(t, "Сервер",   v_native("Сервер", n_Сервер));
    table_set(t, "Клиент",   v_native("Клиент", n_Клиент));
    table_set(t, "Датаграм", v_native("Датаграм", n_Датаграм));

    static const MethodEntry srv[] = {
        { "слушай", m_слушай },
        { "прими",  m_прими },
        { "закрой", m_закрой_сервер },
        { "адрес",  m_адрес_сервер },
    };
    vm_register_class("Сервер", srv, (int)(sizeof(srv) / sizeof(srv[0])));

    static const MethodEntry conn[] = {
        { "отправь",        m_отправь },
        { "отправь_строку", m_отправь_строку },
        { "получи",         m_получи },
        { "получи_строку",  m_получи_строку },
        { "закрой",         m_закрой },
        { "адрес",          m_адрес },
    };
    vm_register_class("Соединение", conn, (int)(sizeof(conn) / sizeof(conn[0])));

    static const MethodEntry udp[] = {
        { "привяжи", m_привяжи },
        { "отправь", m_отправь_кому },
        { "получи",  m_получи_откуда },
        { "закрой",  m_закрой_udp },
    };
    vm_register_class("Датаграм", udp, (int)(sizeof(udp) / sizeof(udp[0])));
}
