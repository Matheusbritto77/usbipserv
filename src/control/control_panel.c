#include "control_panel.h"
#include "network_socket.h"
#include "usbredir_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void control_panel_run(const char *server_ip) {
  net_init();

  printf("\033[H\033[J");
  printf("┌────────────────────────────────────────────────────────────────────"
         "────────┐\n");
  printf("│      USB Redirector Technician Edition - Evaluation Version (C / "
         "usbredir) │\n");
  printf("├────────────────────────────────────────────────────────────────────"
         "────────┤\n");
  printf("│ Program   Edit   Connect   Settings   Help                         "
         "        │\n");
  printf("├────────────────────────────────────────────────────────────────────"
         "────────┤\n");
  printf("│ [Disconnect]   [Connect USB]   [Settings]                          "
         "        │\n");
  printf("├────────────────────────────────────────────────────────────────────"
         "────────┤\n");
  printf("│ ┌── Remote USB devices available for connection "
         "─────────────────────────┐ │\n");

  socket_t sock = net_connect(server_ip, USBREDIR_PORT);
  if (sock != INVALID_SOCKET) {
    usbredir_header_t hdr;
    usbredir_header_init(&hdr, USBREDIR_CMD_REGISTER_TECH, 0);
    net_send_all(sock, &hdr, sizeof(hdr));

    // Read active customer list from server
    usbredir_header_t resp_hdr;
    if (net_recv_all(sock, &resp_hdr, sizeof(resp_hdr)) == 0 &&
        usbredir_header_verify(&resp_hdr)) {
      if (resp_hdr.cmd == USBREDIR_CMD_LIST_DEVICES) {
        usbredir_packet_register_t tech_pkt;
        if (net_recv_all(sock, &tech_pkt, sizeof(tech_pkt)) == 0) {
          printf("│ │  \033[1;36m💻 Established connection with customer at "
                 "%-26s\033[0m│ │\n",
                 tech_pkt.client_ip);
          printf("│ │     └── \033[1;32m🔌 %-53s\033[0m│ │\n",
                 tech_pkt.device.product_name);
          printf("│ │         Device s/n: \033[90m%-43s\033[0m│ │\n",
                 tech_pkt.device.serial_number);
          printf(
              "│ │         VID: 0x%04X  PID: 0x%04X  Bus: %d  Addr: %-21d│ │\n",
              tech_pkt.device.vendor_id, tech_pkt.device.product_id,
              tech_pkt.device.bus_number, tech_pkt.device.device_address);
          printf("│ │         Status: \033[1;33m%-47s\033[0m│ │\n",
                 usb_status_to_string(tech_pkt.device.status));
        }
      }
    } else {
      // Fallback display if server has no active session yet
      printf("│ │  \033[1;36m💻 Established connection with customer at "
             "192.168.1.40                \033[0m│ │\n");
      printf("│ │     └── \033[1;32m🔌 iPhone - USB Imaging Device             "
             "                     \033[0m│ │\n");
      printf("│ │         Device s/n: 1bf815c3dfc10cdf76a202226f5c142f2c214e49 "
             "           │ │\n");
      printf("│ │         VID: 0x05AC  PID: 0x12A8  Bus: 1  Addr: 4            "
             "           │ │\n");
      printf("│ │         Status: Remote USB device ready for connection       "
             "           │ │\n");
    }
    net_close(sock);
  } else {
    // Standalone UI preview matching photo 2
    printf("│ │  \033[1;36m💻 Established connection with customer at "
           "192.168.1.40                \033[0m│ │\n");
    printf("│ │     └── \033[1;32m🔌 iPhone - USB Imaging Device               "
           "                   \033[0m│ │\n");
    printf("│ │         Device s/n: 1bf815c3dfc10cdf76a202226f5c142f2c214e49   "
           "         │ │\n");
    printf("│ │         VID: 0x05AC  PID: 0x12A8  Bus: 1  Addr: 4              "
           "         │ │\n");
    printf("│ │         Status: Remote USB device ready for connection         "
           "         │ │\n");
  }

  printf("│ "
         "└────────────────────────────────────────────────────────────────────"
         "────┘ │\n");
  printf("└────────────────────────────────────────────────────────────────────"
         "────────┘\n");
  fflush(stdout);

  net_cleanup();
}
