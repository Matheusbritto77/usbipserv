#include "usb_device.h"
#include <stdio.h>
#include <string.h>

void usb_device_init(usb_device_info_t *dev) {
    if (!dev) return;
    memset(dev, 0, sizeof(usb_device_info_t));

    usb_device_info_t list[1];
    int count = usb_device_enumerate_real(list, 1);
    if (count > 0) {
        *dev = list[0];
    } else {
        dev->vendor_id = 0x05AC;
        dev->product_id = 0x12A8;
        dev->bus_number = 1;
        dev->device_address = 4;
        snprintf(dev->product_name, sizeof(dev->product_name), "Generic USB Host Controller");
        snprintf(dev->manufacturer, sizeof(dev->manufacturer), "USB Standard Hub");
        snprintf(dev->serial_number, sizeof(dev->serial_number), "USB\\VID_05AC&PID_12A8\\1001");
        snprintf(dev->device_category, sizeof(dev->device_category), "[Pen Drive]");
        dev->status = USB_STATUS_PLUGGED;
        dev->progress_percent = 0;
    }
}

const char* usb_status_to_string(usb_service_status_t status) {
    switch (status) {
        case USB_STATUS_DISCONNECTED: return "Disconnected";
        case USB_STATUS_PLUGGED:      return "Plugged In & Ready (Click Connect)";
        case USB_STATUS_WAITING_TECH: return "Waiting for technician";
        case USB_STATUS_SERVICING:    return "Connected [OK] (Active Data Stream)";
        case USB_STATUS_FINISHED:     return "Servicing finished";
        default:                      return "Ready";
    }
}

void usb_device_print(const usb_device_info_t *dev) {
    if (!dev) return;
    printf("[USB Device] %s (s/n: %s)\n", dev->product_name, dev->serial_number);
    printf("             VID: 0x%04X, PID: 0x%04X, Bus: %d, Addr: %d\n",
           dev->vendor_id, dev->product_id, dev->bus_number, dev->device_address);
    printf("             Status: %s (%d%%)\n", usb_status_to_string(dev->status), dev->progress_percent);
}
