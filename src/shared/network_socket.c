#include "network_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void net_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

socket_t net_connect(const char *host, int port) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        net_close(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

socket_t net_listen(int port) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        net_close(sock);
        return INVALID_SOCKET;
    }

    if (listen(sock, 10) < 0) {
        net_close(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

socket_t net_accept(socket_t server_fd, char *client_ip_out, size_t ip_len) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    socket_t client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_sock != INVALID_SOCKET && client_ip_out) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_out, (socklen_t)ip_len);
    }
    return client_sock;
}

int net_send_all(socket_t sock, const void *buf, size_t len) {
    size_t total_sent = 0;
    const char *p = (const char *)buf;
    while (total_sent < len) {
        int sent = send(sock, p + total_sent, (int)(len - total_sent), 0);
        if (sent <= 0) return -1;
        total_sent += (size_t)sent;
    }
    return 0;
}

int net_recv_all(socket_t sock, void *buf, size_t len) {
    size_t total_recvd = 0;
    char *p = (char *)buf;
    while (total_recvd < len) {
        int recvd = recv(sock, p + total_recvd, (int)(len - total_recvd), 0);
        if (recvd <= 0) return -1;
        total_recvd += (size_t)recvd;
    }
    return 0;
}

void net_close(socket_t sock) {
    if (sock != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
}
