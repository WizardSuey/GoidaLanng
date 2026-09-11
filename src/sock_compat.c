/* sock_compat.c — реализация прослойки сокетов. */
#include "sock_compat.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32

int sc_init(void)
{
    WSADATA wd;
    return WSAStartup(MAKEWORD(2, 2), &wd) == 0 ? 0 : -1;
}

sc_sock_t sc_socket(int type)
{
    return socket(AF_INET, type == 1 ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int sc_bind(sc_sock_t s, const char *addr, int port)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((u_short)port);
    if (!addr || !*addr || strcmp(addr, "0.0.0.0") == 0)
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
    else if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1)
        return -1;
    return bind(s, (struct sockaddr *)&sa, sizeof sa);
}

int sc_listen(sc_sock_t s, int backlog) { return listen(s, backlog); }

sc_sock_t sc_accept(sc_sock_t s, char *peer, size_t peersz)
{
    struct sockaddr_in sa;
    int sl = sizeof sa;
    sc_sock_t c = accept(s, (struct sockaddr *)&sa, &sl);
    if (c != SC_INVALID && peer) {
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
        snprintf(peer, peersz, "%s:%d", ip, ntohs(sa.sin_port));
    }
    return c;
}

int sc_connect(sc_sock_t s, const char *host, int port)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((u_short)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) return -1;
        sa.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }
    return connect(s, (struct sockaddr *)&sa, sizeof sa);
}

long sc_send(sc_sock_t s, const char *data, size_t len)
{
    return (long)send(s, data, (int)len, 0);
}

long sc_recv(sc_sock_t s, char *buf, size_t cap)
{
    long r = (long)recv(s, buf, (int)cap, 0);
    return r;
}

long sc_sendto(sc_sock_t s, const char *data, size_t len, const char *host, int port)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((u_short)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) return -1;
    return (long)sendto(s, data, (int)len, 0, (struct sockaddr *)&sa, sizeof sa);
}

long sc_recvfrom(sc_sock_t s, char *buf, size_t cap, char *host, size_t hsz, int *port)
{
    struct sockaddr_in sa;
    int sl = sizeof sa;
    long r = (long)recvfrom(s, buf, (int)cap, 0, (struct sockaddr *)&sa, &sl);
    if (r >= 0) {
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
        if (host) snprintf(host, hsz, "%s", ip);
        if (port) *port = ntohs(sa.sin_port);
    }
    return r;
}

void sc_close(sc_sock_t s) { closesocket(s); }

int sc_set_nonblock(sc_sock_t s, int nb)
{
    u_long v = nb ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &v) == 0 ? 0 : -1;
}

static void addr_str(sc_sock_t s, char *buf, size_t sz, int local)
{
    struct sockaddr_in sa;
    int sl = sizeof sa;
    int rc = local ? getsockname(s, (struct sockaddr *)&sa, &sl)
                   : getpeername(s, (struct sockaddr *)&sa, &sl);
    if (rc == 0) {
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
        snprintf(buf, sz, "%s:%d", ip, ntohs(sa.sin_port));
    } else {
        snprintf(buf, sz, "?");
    }
}

void sc_local_addr(sc_sock_t s, char *buf, size_t sz) { addr_str(s, buf, sz, 1); }
void sc_peer_addr(sc_sock_t s, char *buf, size_t sz)  { addr_str(s, buf, sz, 0); }

#else /* ------------------------- POSIX ------------------------- */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int sc_init(void) { return 0; }

sc_sock_t sc_socket(int type)
{
    return socket(AF_INET, type == 1 ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int sc_bind(sc_sock_t s, const char *addr, int port)
{
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    if (!addr || !*addr || strcmp(addr, "0.0.0.0") == 0)
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
    else if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1)
        return -1;
    return bind(s, (struct sockaddr *)&sa, sizeof sa);
}

int sc_listen(sc_sock_t s, int backlog) { return listen(s, backlog); }

sc_sock_t sc_accept(sc_sock_t s, char *peer, size_t peersz)
{
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    sc_sock_t c = accept(s, (struct sockaddr *)&sa, &sl);
    if (c != SC_INVALID && peer) {
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
        snprintf(peer, peersz, "%s:%d", ip, ntohs(sa.sin_port));
    }
    return c;
}

int sc_connect(sc_sock_t s, const char *host, int port)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) return -1;
        sa.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }
    return connect(s, (struct sockaddr *)&sa, sizeof sa);
}

long sc_send(sc_sock_t s, const char *data, size_t len)
{
    return (long)send(s, data, len, MSG_NOSIGNAL);
}

long sc_recv(sc_sock_t s, char *buf, size_t cap)
{
    return (long)recv(s, buf, cap, 0);
}

long sc_sendto(sc_sock_t s, const char *data, size_t len, const char *host, int port)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) return -1;
    return (long)sendto(s, data, len, 0, (struct sockaddr *)&sa, sizeof sa);
}

long sc_recvfrom(sc_sock_t s, char *buf, size_t cap, char *host, size_t hsz, int *port)
{
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    long r = (long)recvfrom(s, buf, cap, 0, (struct sockaddr *)&sa, &sl);
    if (r >= 0) {
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
        if (host) snprintf(host, hsz, "%s", ip);
        if (port) *port = ntohs(sa.sin_port);
    }
    return r;
}

void sc_close(sc_sock_t s) { close(s); }

int sc_set_nonblock(sc_sock_t s, int nb)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    if (nb) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return fcntl(s, F_SETFL, flags);
}

static void addr_str(sc_sock_t s, char *buf, size_t sz, int local)
{
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    int rc = local ? getsockname(s, (struct sockaddr *)&sa, &sl)
                   : getpeername(s, (struct sockaddr *)&sa, &sl);
    if (rc == 0) {
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
        snprintf(buf, sz, "%s:%d", ip, ntohs(sa.sin_port));
    } else {
        snprintf(buf, sz, "?");
    }
}

void sc_local_addr(sc_sock_t s, char *buf, size_t sz) { addr_str(s, buf, sz, 1); }
void sc_peer_addr(sc_sock_t s, char *buf, size_t sz)  { addr_str(s, buf, sz, 0); }

#endif /* _WIN32 */
