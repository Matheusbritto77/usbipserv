#ifndef USBREDIR_PROTOCOL_H
#define USBREDIR_PROTOCOL_H

#include "usb_device.h"
#include <stdint.h>
#include <stddef.h>

#define USBREDIR_MAGIC 0x55534252 // "USBR"
#define USBREDIR_PORT  32400

typedef enum {
    USBREDIR_CMD_REGISTER_CLIENT = 0x01,
    USBREDIR_CMD_REGISTER_TECH   = 0x02,
    USBREDIR_CMD_LIST_DEVICES    = 0x03,
    USBREDIR_CMD_START_SERVICE   = 0x04,
    USBREDIR_CMD_UPDATE_PROGRESS = 0x05,
    USBREDIR_CMD_FINISH_SERVICE  = 0x06,
    USBREDIR_CMD_DATA_FRAME      = 0x07
} usbredir_cmd_type_t;

typedef struct {
    uint32_t magic;
    uint32_t cmd;
    uint32_t payload_len;
} usbredir_header_t;

typedef struct {
    char client_ip[64];
    char target_tech_id[32];
    int device_count;
    usb_device_info_t devices[8];
} usbredir_packet_register_t;

typedef struct {
    char tech_id[32];
} usbredir_packet_tech_init_t;

typedef struct {
    uint32_t progress;
    usb_service_status_t status;
} usbredir_packet_progress_t;

// Functions
void usbredir_header_init(usbredir_header_t *hdr, usbredir_cmd_type_t cmd, uint32_t payload_len);
int usbredir_header_verify(const usbredir_header_t *hdr);

#endif // USBREDIR_PROTOCOL_H
