#ifndef IPC_CHANNEL_H
#define IPC_CHANNEL_H

#include "usbredir_protocol.h"
#include <stdbool.h>

typedef enum {
    IPC_EVENT_NONE = 0,
    IPC_EVENT_CONNECTED_SERVER,
    IPC_EVENT_DISCONNECTED_SERVER,
    IPC_EVENT_TECH_ID_ASSIGNED,
    IPC_EVENT_DEVICE_UPDATED,
    IPC_EVENT_PROGRESS_UPDATED,
    IPC_EVENT_REMOTE_DEVICE_LIST
} ipc_event_type_t;

typedef enum {
    IPC_CMD_NONE = 0,
    IPC_CMD_CONNECT_TECH,
    IPC_CMD_DISCONNECT_USB,
    IPC_CMD_REFRESH_LIST,
    IPC_CMD_START_SERVICE
} ipc_cmd_type_t;

typedef struct {
    ipc_event_type_t type;
    char tech_id[32];
    uint32_t progress_percent;
    usb_service_status_t status;
    usb_device_info_t device;
    usbredir_packet_register_t remote_devices[16];
    int remote_count;
} ipc_event_t;

typedef struct {
    ipc_cmd_type_t type;
    char target_tech_id[32];
} ipc_cmd_t;

typedef void (*ipc_event_callback_t)(const ipc_event_t *event, void *user_data);

typedef struct {
    ipc_event_callback_t on_event;
    void *user_data;
} ipc_channel_t;

void ipc_channel_init(ipc_channel_t *chan, ipc_event_callback_t cb, void *user_data);
void ipc_emit_event(ipc_channel_t *chan, const ipc_event_t *event);
void ipc_send_cmd(ipc_channel_t *chan, const ipc_cmd_t *cmd);

#endif // IPC_CHANNEL_H
