#ifndef RELAY_SERVER_H
#define RELAY_SERVER_H

#include "network_socket.h"
#include "usbredir_protocol.h"
#include "usb_device.h"

typedef struct {
    socket_t client_sock;
    socket_t tech_sock;
    char client_ip[64];
    usb_device_info_t device;
    bool is_active;
} relay_session_t;

void relay_server_start(int port);

#endif // RELAY_SERVER_H
