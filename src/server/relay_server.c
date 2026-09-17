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

#define MAX_SESSIONS 32

typedef struct {
    socket_t client_sock;
    char client_ip[64];
    char target_tech_id[32];
    usb_device_info_t device;
    bool is_active;
} client_session_t;

typedef struct {
    socket_t tech_sock;
    char tech_id[32];
    bool is_active;
} tech_session_t;

static client_session_t g_clients[MAX_SESSIONS];
static tech_session_t g_techs[MAX_SESSIONS];
static mutex_t g_session_lock;
static bool g_mutex_initialized = false;
static int g_tech_counter = 7890;

static void init_sessions(void) {
    if (!g_mutex_initialized) {
        MUTEX_INIT(&g_session_lock);
        g_mutex_initialized = true;
    }
    memset(g_clients, 0, sizeof(g_clients));
    memset(g_techs, 0, sizeof(g_techs));
    for (int i = 0; i < MAX_SESSIONS; i++) {
        g_clients[i].client_sock = INVALID_SOCKET;
        g_techs[i].tech_sock = INVALID_SOCKET;
    }
}

static THREAD_ROUTINE handle_client_connection(void *arg) {
    socket_t conn_sock = (socket_t)(intptr_t)arg;
    char conn_ip[64] = "192.168.10.25";

    usbredir_header_t hdr;
    if (net_recv_all(conn_sock, &hdr, sizeof(hdr)) < 0) {
        net_close(conn_sock);
        return 0;
    }

    if (!usbredir_header_verify(&hdr)) {
        printf("[Server] Invalid header magic received!\n");
        net_close(conn_sock);
        return 0;
    }

    if (hdr.cmd == USBREDIR_CMD_REGISTER_CLIENT) {
        usbredir_packet_register_t reg_pkt;
        if (net_recv_all(conn_sock, &reg_pkt, sizeof(reg_pkt)) < 0) {
            net_close(conn_sock);
            return 0;
        }

        MUTEX_LOCK(&g_session_lock);
        int session_idx = -1;
        for (int i = 0; i < MAX_SESSIONS; i++) {
            if (!g_clients[i].is_active) {
                session_idx = i;
                break;
            }
        }

        if (session_idx >= 0) {
            g_clients[session_idx].client_sock = conn_sock;
            strncpy(g_clients[session_idx].client_ip, conn_ip, sizeof(g_clients[session_idx].client_ip));
            strncpy(g_clients[session_idx].target_tech_id, reg_pkt.target_tech_id, sizeof(g_clients[session_idx].target_tech_id));
            g_clients[session_idx].device = reg_pkt.device;
            g_clients[session_idx].is_active = true;

            printf("[Server] Registered Customer Client at %s -> Target Tech ID: [%s]\n",
                   conn_ip, reg_pkt.target_tech_id);
            printf("[Server] USB Device: %s (s/n: %s)\n",
                   reg_pkt.device.product_name, reg_pkt.device.serial_number);

            // Forward client registration to matching active technician
            for (int t = 0; t < MAX_SESSIONS; t++) {
                if (g_techs[t].is_active && strcasecmp(g_techs[t].tech_id, reg_pkt.target_tech_id) == 0) {
                    usbredir_header_t resp_hdr;
                    usbredir_header_init(&resp_hdr, USBREDIR_CMD_LIST_DEVICES, sizeof(usbredir_packet_register_t));
                    net_send_all(g_techs[t].tech_sock, &resp_hdr, sizeof(resp_hdr));
                    net_send_all(g_techs[t].tech_sock, &reg_pkt, sizeof(reg_pkt));
                    printf("[Server] Routed USB device to Technician %s\n", g_techs[t].tech_id);
                }
            }
        }
        MUTEX_UNLOCK(&g_session_lock);
    } else if (hdr.cmd == USBREDIR_CMD_REGISTER_TECH) {
        MUTEX_LOCK(&g_session_lock);
        int tech_idx = -1;
        for (int i = 0; i < MAX_SESSIONS; i++) {
            if (!g_techs[i].is_active) {
                tech_idx = i;
                break;
            }
        }

        if (tech_idx >= 0) {
            g_techs[tech_idx].tech_sock = conn_sock;
            snprintf(g_techs[tech_idx].tech_id, sizeof(g_techs[tech_idx].tech_id), "TECH-%d", ++g_tech_counter);
            g_techs[tech_idx].is_active = true;

            printf("[Server] Registered Technician Control Panel. Assigned ID: [%s]\n", g_techs[tech_idx].tech_id);

            // Send assigned Technician ID back to Technician GUI
            usbredir_header_t init_hdr;
            usbredir_header_init(&init_hdr, USBREDIR_CMD_REGISTER_TECH, sizeof(usbredir_packet_tech_init_t));
            usbredir_packet_tech_init_t init_pkt;
            strncpy(init_pkt.tech_id, g_techs[tech_idx].tech_id, sizeof(init_pkt.tech_id));

            net_send_all(conn_sock, &init_hdr, sizeof(init_hdr));
            net_send_all(conn_sock, &init_pkt, sizeof(init_pkt));

            // Forward any existing clients mapped to this tech
            for (int c = 0; c < MAX_SESSIONS; c++) {
                if (g_clients[c].is_active && strcasecmp(g_clients[c].target_tech_id, g_techs[tech_idx].tech_id) == 0) {
                    usbredir_header_t list_hdr;
                    usbredir_header_init(&list_hdr, USBREDIR_CMD_LIST_DEVICES, sizeof(usbredir_packet_register_t));
                    usbredir_packet_register_t reg_pkt;
                    strncpy(reg_pkt.client_ip, g_clients[c].client_ip, sizeof(reg_pkt.client_ip));
                    strncpy(reg_pkt.target_tech_id, g_clients[c].target_tech_id, sizeof(reg_pkt.target_tech_id));
                    reg_pkt.device = g_clients[c].device;

                    net_send_all(conn_sock, &list_hdr, sizeof(list_hdr));
                    net_send_all(conn_sock, &reg_pkt, sizeof(reg_pkt));
                }
            }
        }
        MUTEX_UNLOCK(&g_session_lock);
    } else if (hdr.cmd == USBREDIR_CMD_START_SERVICE || hdr.cmd == USBREDIR_CMD_FINISH_SERVICE) {
        // Forward start/finish service command to client
        MUTEX_LOCK(&g_session_lock);
        for (int c = 0; c < MAX_SESSIONS; c++) {
            if (g_clients[c].is_active) {
                net_send_all(g_clients[c].client_sock, &hdr, sizeof(hdr));
                printf("[Server] Broadcasted service command 0x%02X to Customer Client\n", hdr.cmd);
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
    printf("   Target Technician ID Session Router Active        \n");
    printf("=====================================================\n");
    printf("[Server] Listening on 0.0.0.0:%d...\n", port);

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
