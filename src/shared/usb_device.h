#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_USB_DEVICES 32
#define MAX_STR_LEN 256

typedef enum {
    USB_STATUS_DISCONNECTED = 0,
    USB_STATUS_PLUGGED = 1,
    USB_STATUS_WAITING_TECH = 2,
    USB_STATUS_SERVICING = 3,
    USB_STATUS_FINISHED = 4
} usb_service_status_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t bus_number;
    uint8_t device_address;
    char serial_number[MAX_STR_LEN];
    char product_name[MAX_STR_LEN];
    char manufacturer[MAX_STR_LEN];
    char device_category[64];
    usb_service_status_t status;
    int progress_percent;
} usb_device_info_t;

// Functions
void usb_device_init(usb_device_info_t *dev);
void usb_device_print(const usb_device_info_t *dev);
const char* usb_status_to_string(usb_service_status_t status);
int usb_device_enumerate_real(usb_device_info_t *devices_out, int max_devices);

#endif // USB_DEVICE_H
