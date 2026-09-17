#include "usbredir_protocol.h"
#include <stdio.h>

void usbredir_header_init(usbredir_header_t *hdr, usbredir_cmd_type_t cmd, uint32_t payload_len) {
    if (!hdr) return;
    hdr->magic = USBREDIR_MAGIC;
    hdr->cmd = (uint32_t)cmd;
    hdr->payload_len = payload_len;
}

int usbredir_header_verify(const usbredir_header_t *hdr) {
    if (!hdr) return 0;
    return (hdr->magic == USBREDIR_MAGIC);
}
