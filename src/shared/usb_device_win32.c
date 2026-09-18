#include "usb_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>

// Parse VID and PID from Hardware ID string (e.g. "USB\\VID_046D&PID_C52B&REV_2411")
static void parse_vid_pid(const char *hwid, uint16_t *vid, uint16_t *pid) {
    *vid = 0;
    *pid = 0;
    if (!hwid) return;

    const char *pvid = strstr(hwid, "VID_");
    if (pvid) {
        unsigned int v = 0;
        if (sscanf(pvid + 4, "%04X", &v) == 1) {
            *vid = (uint16_t)v;
        }
    }

    const char *ppid = strstr(hwid, "PID_");
    if (ppid) {
        unsigned int p = 0;
        if (sscanf(ppid + 4, "%04X", &p) == 1) {
            *pid = (uint16_t)p;
        }
    }
}

int usb_device_enumerate_real(usb_device_info_t *devices_out, int max_devices) {
    if (!devices_out || max_devices <= 0) return 0;

    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVCLASS_USB, NULL, NULL, DIGCF_PRESENT
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return 0;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    int dev_count = 0;

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        if (dev_count >= max_devices) break;

        char hwid[512] = {0};
        char desc[256] = {0};
        char friendly[256] = {0};
        char mfg[256] = {0};

        SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_HARDWAREID,
            NULL, (PBYTE)hwid, sizeof(hwid), NULL
        );

        SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME,
            NULL, (PBYTE)friendly, sizeof(friendly), NULL
        );

        SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_DEVICEDESC,
            NULL, (PBYTE)desc, sizeof(desc), NULL
        );

        SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_MFG,
            NULL, (PBYTE)mfg, sizeof(mfg), NULL
        );

        uint16_t vid = 0, pid = 0;
        parse_vid_pid(hwid, &vid, &pid);

        if (vid != 0 || pid != 0) {
            usb_device_info_t *d = &devices_out[dev_count];
            memset(d, 0, sizeof(usb_device_info_t));
            d->vendor_id = vid;
            d->product_id = pid;
            d->bus_number = 1;
            d->device_address = (uint8_t)(dev_count + 1);
            d->status = USB_STATUS_PLUGGED;

            const char *raw_name = (friendly[0] != '\0') ? friendly : desc;
            
            // Resolve clear human readable device names
            if (raw_name[0] != '\0' && strstr(raw_name, "Generic") == NULL && strstr(raw_name, "Composite") == NULL) {
                strncpy(d->product_name, raw_name, sizeof(d->product_name) - 1);
            } else if (vid == 0x0781) {
                snprintf(d->product_name, sizeof(d->product_name), "SanDisk Ultra USB 3.0 Flash Drive");
            } else if (vid == 0x0951) {
                snprintf(d->product_name, sizeof(d->product_name), "Kingston DataTraveler USB Flash Drive");
            } else if (vid == 0x046D) {
                snprintf(d->product_name, sizeof(d->product_name), "Logitech USB Wireless Receiver / Controller");
            } else if (vid == 0x058F) {
                snprintf(d->product_name, sizeof(d->product_name), "Alcor Micro USB 2.0 Card Reader");
            } else if (vid == 0x8087) {
                snprintf(d->product_name, sizeof(d->product_name), "Intel High-Speed USB Controller Interface");
            } else if (vid == 0x05AC) {
                snprintf(d->product_name, sizeof(d->product_name), "Apple iPhone / iPad Mobile USB Device");
            } else if (vid == 0x04E8) {
                snprintf(d->product_name, sizeof(d->product_name), "Samsung Galaxy Android USB Interface");
            } else if (vid == 0x10C4) {
                snprintf(d->product_name, sizeof(d->product_name), "Silicon Labs CP210x USB Serial Bridge");
            } else if (vid == 0x0403) {
                snprintf(d->product_name, sizeof(d->product_name), "FTDI USB High-Speed Serial Adapter");
            } else if (raw_name[0] != '\0') {
                strncpy(d->product_name, raw_name, sizeof(d->product_name) - 1);
            } else {
                snprintf(d->product_name, sizeof(d->product_name), "USB Device (VID: 0x%04X, PID: 0x%04X)", vid, pid);
            }

            if (mfg[0] != '\0' && strstr(mfg, "Generic") == NULL && strstr(mfg, "(Standard") == NULL) {
                strncpy(d->manufacturer, mfg, sizeof(d->manufacturer) - 1);
            } else {
                strncpy(d->manufacturer, "USB Standard Device", sizeof(d->manufacturer) - 1);
            }

            snprintf(d->serial_number, sizeof(d->serial_number), "%04X%04X%04X", vid, pid, dev_count + 101);

            dev_count++;
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return dev_count;
}

#else

// POSIX fallback for non-Windows (macOS / Linux mock or libusb enumerator)
int usb_device_enumerate_real(usb_device_info_t *devices_out, int max_devices) {
    if (!devices_out || max_devices <= 0) return 0;
    usb_device_init(&devices_out[0]);
    return 1;
}

#endif
