#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
#endif

void net_init(void);
void net_cleanup(void);
socket_t net_connect(const char *host, int port);
socket_t net_listen(int port);
socket_t net_accept(socket_t server_fd, char *client_ip_out, size_t ip_len);
int net_send_all(socket_t sock, const void *buf, size_t len);
int net_recv_all(socket_t sock, void *buf, size_t len);
void net_close(socket_t sock);

#endif // NETWORK_SOCKET_H
