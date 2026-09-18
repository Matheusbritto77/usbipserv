#include "client_wizard.h"
#include "usbredir_protocol.h"
#include "network_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define sleep_ms(ms) usleep((ms) * 1000)
#endif

static void print_step_ui(usb_service_status_t step, int progress, const usb_device_info_t *dev) {
    // Clear screen
    printf("\033[H\033[J");

    printf("┌────────────────────────────────────────────────────────────────────────────┐\n");
    printf("│                      USB Redirector - Customer Module                      │\n");
    printf("│                      Ready to Service Your Device                          │\n");
    printf("│                      Please follow instructions below                      │\n");
    printf("├────────────────────────────────────────────────────────────────────────────┤\n");

    // Step 1
    if (step >= USB_STATUS_PLUGGED) {
        printf("│  \033[1;32m1  Plug your USB device\033[0m                                                  │\n");
        printf("│     Device detected: \033[36m%-45s\033[0m │\n", dev->product_name);
        printf("│     Serial: \033[90m%-54s\033[0m │\n", dev->serial_number);
    } else {
        printf("│  \033[90m1  Plug your USB device\033[0m                                                  │\n");
        printf("│     Please plug your device into a USB port.                               │\n");
    }
    printf("│                                                                            │\n");

    // Step 2
    if (step >= USB_STATUS_WAITING_TECH) {
        printf("│  \033[1;32m2  Waiting for technician to start servicing your device\033[0m                 │\n");
        printf("│     Technician is getting ready to service your device...                  │\n");
    } else {
        printf("│  \033[90m2  Waiting for technician to start servicing your device\033[0m                 │\n");
        printf("│     Technician is getting ready to service your device.                    │\n");
    }
    printf("│                                                                            │\n");

    // Step 3
    if (step == USB_STATUS_SERVICING) {
        printf("│  \033[1;32m3  Servicing your device\033[0m   [");
        int filled = progress / 5;
        for (int i = 0; i < 20; i++) {
            if (i < filled) printf("█");
            else printf("░");
        }
        printf("] %3d%%                               │\n", progress);
        printf("│     Technician is servicing your device, this may take awhile.             │\n");
    } else if (step > USB_STATUS_SERVICING) {
        printf("│  \033[1;32m3  Servicing your device\033[0m   [████████████████████] 100%%                              │\n");
    } else {
        printf("│  \033[90m3  Servicing your device\033[0m                                                  │\n");
    }
    printf("│                                                                            │\n");

    // Step 4
    if (step == USB_STATUS_FINISHED) {
        printf("│  \033[1;32m4  Servicing of your device has been finished\033[0m                            │\n");
        printf("│     \033[1;33mPlease unplug the device from USB port and click Finish to close.\033[0m     │\n");
    } else {
        printf("│  \033[90m4  Servicing of your device has been finished\033[0m                            │\n");
    }

    printf("└────────────────────────────────────────────────────────────────────────────┘\n");
    fflush(stdout);
}

void client_wizard_run(const char *server_ip) {
    net_init();

    usb_device_info_t dev;
    usb_device_init(&dev);

    char tech_id[32] = "7891";
    printf("\033[H\033[J");
    printf("========================================================\n");
    printf("          USB Redirector Customer Module (macOS)         \n");
    printf("========================================================\n");
    printf("Enter Numeric Technician ID [default: 7891]: ");
    fflush(stdout);

    char input_buf[64] = {0};
    if (fgets(input_buf, sizeof(input_buf), stdin)) {
        input_buf[strcspn(input_buf, "\r\n")] = '\0';
        if (strlen(input_buf) > 0) {
            strncpy(tech_id, input_buf, sizeof(tech_id) - 1);
        }
    }

    // Step 1: Detect plugged USB
    dev.status = USB_STATUS_PLUGGED;
    print_step_ui(dev.status, 0, &dev);
    sleep_ms(1500);

    // Step 2: Register with Server Relay
    dev.status = USB_STATUS_WAITING_TECH;
    print_step_ui(dev.status, 0, &dev);

    socket_t sock = net_connect(server_ip, USBREDIR_PORT);
    if (sock != INVALID_SOCKET) {
        usbredir_header_t hdr;
        usbredir_header_init(&hdr, USBREDIR_CMD_REGISTER_CLIENT, sizeof(usbredir_packet_register_t));

        usbredir_packet_register_t reg_pkt;
        memset(&reg_pkt, 0, sizeof(reg_pkt));
        strncpy(reg_pkt.client_ip, "192.168.10.50 (macOS)", sizeof(reg_pkt.client_ip) - 1);
        strncpy(reg_pkt.target_tech_id, tech_id, sizeof(reg_pkt.target_tech_id) - 1);
        reg_pkt.device = dev;

        net_send_all(sock, &hdr, sizeof(hdr));
        net_send_all(sock, &reg_pkt, sizeof(reg_pkt));

        while (1) {
            usbredir_header_t rx_hdr;
            if (net_recv_all(sock, &rx_hdr, sizeof(rx_hdr)) < 0) break;

            if (usbredir_header_verify(&rx_hdr)) {
                if (rx_hdr.cmd == USBREDIR_CMD_START_SERVICE) {
                    // Step 3: Servicing device
                    dev.status = USB_STATUS_SERVICING;
                    for (int p = 0; p <= 100; p += 10) {
                        dev.progress_percent = p;
                        print_step_ui(dev.status, p, &dev);
                        sleep_ms(250);
                    }
                } else if (rx_hdr.cmd == USBREDIR_CMD_FINISH_SERVICE) {
                    // Step 4: Servicing finished
                    dev.status = USB_STATUS_FINISHED;
                    print_step_ui(dev.status, 100, &dev);
                    break;
                }
            }
        }
        net_close(sock);
    } else {
        printf("\nFailed to connect to relay server at %s:%d\n", server_ip, USBREDIR_PORT);
    }
    net_cleanup();
}
