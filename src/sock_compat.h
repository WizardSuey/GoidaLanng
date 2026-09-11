/* sock_compat.h — тонкая прослойка POSIX / winsock.
 * Весь платформозависимый код сокетов живёт только здесь.
 */
#ifndef GOIDA_SOCK_COMPAT_H
#define GOIDA_SOCK_COMPAT_H

#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sc_sock_t;
#define SC_INVALID INVALID_SOCKET
#else
typedef int sc_sock_t;
#define SC_INVALID (-1)
#endif

/* Инициализация стека (WSAStartup на Windows). 0 — ок. */
int  sc_init(void);

/* type: 0 = TCP (SOCK_STREAM), 1 = UDP (SOCK_DGRAM). */
sc_sock_t sc_socket(int type);

int  sc_bind(sc_sock_t s, const char *addr, int port);
int  sc_listen(sc_sock_t s, int backlog);
/* peer — строка "ip:port". Возврат SC_INVALID при ошибке. */
sc_sock_t sc_accept(sc_sock_t s, char *peer, size_t peersz);
int  sc_connect(sc_sock_t s, const char *host, int port);

long sc_send(sc_sock_t s, const char *data, size_t len);
/* 0 — соединение закрыто, -1 — ошибка. */
long sc_recv(sc_sock_t s, char *buf, size_t cap);

long sc_sendto(sc_sock_t s, const char *data, size_t len,
               const char *host, int port);
long sc_recvfrom(sc_sock_t s, char *buf, size_t cap,
                 char *host, size_t hsz, int *port);

void sc_close(sc_sock_t s);
int  sc_set_nonblock(sc_sock_t s, int nb);
/* локальный адрес сокета в "ip:port" */
void sc_local_addr(sc_sock_t s, char *buf, size_t sz);
void sc_peer_addr(sc_sock_t s, char *buf, size_t sz);

#endif /* GOIDA_SOCK_COMPAT_H */
