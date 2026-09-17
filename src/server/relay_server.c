#include "relay_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE thread_t;
    typedef CRITICAL_SECTION mutex_t;
    #define MUTEX_INIT(m) InitializeCriticalSection(m)
    #define MUTEX_LOCK(m) EnterCriticalSection(m)
    #define MUTEX_UNLOCK(m) LeaveCriticalSection(m)
    #define THREAD_ROUTINE DWORD WINAPI
#else
    #include <pthread.h>
    typedef pthread_t thread_t;
    typedef pthread_mutex_t mutex_t;
    #define MUTEX_INIT(m) pthread_mutex_init(m, NULL)
    #define MUTEX_LOCK(m) pthread_mutex_lock(m)
    #define MUTEX_UNLOCK(m) pthread_mutex_unlock(m)
    #define THREAD_ROUTINE void*
#endif

#define MAX_SESSIONS 16

static relay_session_t g_sessions[MAX_SESSIONS];
static mutex_t g_session_lock;
static bool g_mutex_initialized = false;

static void init_sessions(void) {
    if (!g_mutex_initialized) {
        MUTEX_INIT(&g_session_lock);
        g_mutex_initialized = true;
    }
    memset(g_sessions, 0, sizeof(g_sessions));
    for (int i = 0; i < MAX_SESSIONS; i++) {
        g_sessions[i].client_sock = INVALID_SOCKET;
        g_sessions[i].tech_sock = INVALID_SOCKET;
    }
}

static THREAD_ROUTINE handle_client_connection(void *arg) {
    socket_t client_sock = (socket_t)(intptr_t)arg;
    char client_ip[64] = "192.168.1.40";

    usbredir_header_t hdr;
    if (net_recv_all(client_sock, &hdr, sizeof(hdr)) < 0) {
        net_close(client_sock);
        return 0;
    }

    if (!usbredir_header_verify(&hdr)) {
        printf("[Server] Invalid header magic received!\n");
        net_close(client_sock);
        return 0;
    }

    if (hdr.cmd == USBREDIR_CMD_REGISTER_CLIENT) {
        usbredir_packet_register_t reg_pkt;
        if (net_recv_all(client_sock, &reg_pkt, sizeof(reg_pkt)) < 0) {
            net_close(client_sock);
            return 0;
        }

        MUTEX_LOCK(&g_session_lock);
        int session_idx = -1;
        for (int i = 0; i < MAX_SESSIONS; i++) {
            if (!g_sessions[i].is_active) {
                session_idx = i;
                break;
            }
        }

        if (session_idx >= 0) {
            g_sessions[session_idx].client_sock = client_sock;
            strncpy(g_sessions[session_idx].client_ip, client_ip, sizeof(g_sessions[session_idx].client_ip));
            g_sessions[session_idx].device = reg_pkt.device;
            g_sessions[session_idx].is_active = true;

            printf("[Server] Registered new Customer Client at %s\n", client_ip);
            printf("[Server] USB Device: %s (s/n: %s)\n",
                   reg_pkt.device.product_name, reg_pkt.device.serial_number);
        } else {
            printf("[Server] Max sessions reached!\n");
            net_close(client_sock);
        }
        MUTEX_UNLOCK(&g_session_lock);
    } else if (hdr.cmd == USBREDIR_CMD_REGISTER_TECH) {
        printf("[Server] Technician Control Panel connected!\n");
        MUTEX_LOCK(&g_session_lock);
        for (int i = 0; i < MAX_SESSIONS; i++) {
            if (g_sessions[i].is_active) {
                usbredir_header_t resp_hdr;
                usbredir_header_init(&resp_hdr, USBREDIR_CMD_LIST_DEVICES, sizeof(usbredir_packet_register_t));
                usbredir_packet_register_t tech_pkt;
                strncpy(tech_pkt.client_ip, g_sessions[i].client_ip, sizeof(tech_pkt.client_ip));
                tech_pkt.device = g_sessions[i].device;

                net_send_all(client_sock, &resp_hdr, sizeof(resp_hdr));
                net_send_all(client_sock, &tech_pkt, sizeof(tech_pkt));
            }
        }
        MUTEX_UNLOCK(&g_session_lock);
    }

    return 0;
}

void relay_server_start(int port) {
    net_init();
    init_sessions();

    socket_t server_fd = net_listen(port);
    if (server_fd == INVALID_SOCKET) {
        fprintf(stderr, "[Server Error] Failed to bind to port %d\n", port);
        return;
    }

    printf("=====================================================\n");
    printf("   USB REDIRECTOR MITM RELAY SERVER (usbredir Core)  \n");
    printf("=====================================================\n");
    printf("[Server] Listening for Clients & Technicians on 0.0.0.0:%d...\n", port);

    while (1) {
        char client_ip[64];
        socket_t conn_fd = net_accept(server_fd, client_ip, sizeof(client_ip));
        if (conn_fd != INVALID_SOCKET) {
#ifdef _WIN32
            HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)handle_client_connection, (void*)(intptr_t)conn_fd, 0, NULL);
            if (hThread) CloseHandle(hThread);
#else
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client_connection, (void*)(intptr_t)conn_fd);
            pthread_detach(tid);
#endif
        }
    }

    net_close(server_fd);
    net_cleanup();
}
