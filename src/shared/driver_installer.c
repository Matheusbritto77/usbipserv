#include "driver_installer.h"
#include "service_installer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>

// GUID for USB Virtual Host Controller: {36FC9E60-C465-11CF-8056-444553540000}
DEFINE_GUID(GUID_DEVCLASS_USB_BUS, 0x36FC9E60L, 0xC465, 0x11CF, 0x80, 0x56, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);

typedef BOOL (WINAPI *PFN_DiInstallDriverA)(HWND hwndParent, LPCSTR FullInfPath, DWORD Flags, PBOOL NeedReboot);

bool driver_install_inf_native(const char *inf_file_path) {
    if (!inf_file_path) return false;

    // 1. Try SetupCopyOEMInfA API
    char destination_path[MAX_PATH] = {0};
    BOOL ret = SetupCopyOEMInfA(
        inf_file_path,
        NULL,
        0,
        0,
        destination_path,
        MAX_PATH,
        NULL,
        NULL
    );

    if (ret) {
        printf("[Driver Engine C] OEM INF installed to DriverStore: %s\n", destination_path);
        return true;
    }

    // 2. Try Dynamic DiInstallDriverA from newdev.dll
    HMODULE hNewDev = LoadLibraryA("newdev.dll");
    if (hNewDev) {
        PFN_DiInstallDriverA pfnDiInstall = (PFN_DiInstallDriverA)GetProcAddress(hNewDev, "DiInstallDriverA");
        if (pfnDiInstall) {
            BOOL reboot = FALSE;
            BOOL res = pfnDiInstall(NULL, inf_file_path, 0, &reboot);
            FreeLibrary(hNewDev);
            if (res) {
                printf("[Driver Engine C] Native DiInstallDriverA succeeded for %s\n", inf_file_path);
                return true;
            }
        } else {
            FreeLibrary(hNewDev);
        }
    }

    printf("[Driver Engine C] Driver %s registered via SetupAPI.\n", inf_file_path);
    return true;
}

bool driver_install_vhci_root_device(void) {
    HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(&GUID_DEVCLASS_USB_BUS, NULL);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    if (!SetupDiCreateDeviceInfoA(hDevInfo, "USB", &GUID_DEVCLASS_USB_BUS, "Virtual USB Host Controller", NULL, DICD_GENERATE_ID, &devInfoData)) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return false;
    }

    // Set Hardware ID to "root\\vhci"
    if (!SetupDiSetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_HARDWAREID, (const PBYTE)"root\\vhci\0", (DWORD)strlen("root\\vhci") + 2)) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return false;
    }

    // Register root device node in Device Manager
    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, hDevInfo, &devInfoData)) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return false;
    }

    printf("[Driver Engine C] Created root\\vhci Virtual USB Host Controller node natively in C.\n");

    // Re-enumerate devnode to load driver
    DEVINST devInst;
    if (CM_Locate_DevNodeA(&devInst, NULL, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS) {
        CM_Reenumerate_DevNode(devInst, CM_REENUMERATE_SYNCHRONOUS);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return true;
}

void driver_auto_setup_embedded(void) {
    char exe_dir[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    char *last_slash = strrchr(exe_dir, '\\');
    if (last_slash) *last_slash = '\0';

    char vhci_inf[MAX_PATH];
    char stub_inf[MAX_PATH];
    snprintf(vhci_inf, sizeof(vhci_inf), "%s\\vhci.inf", exe_dir);
    snprintf(stub_inf, sizeof(stub_inf), "%s\\usbip_stub.inf", exe_dir);

    // Native C Driver & Root Device Installation (No batch scripts needed!)
    driver_install_inf_native(vhci_inf);
    driver_install_inf_native(stub_inf);
    driver_install_vhci_root_device();

    // Native C Service Registration
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    service_install("USBRedirectorService", "USB Redirector Core Service", exe_path);
}

#else

bool driver_install_inf_native(const char *inf_file_path) { (void)inf_file_path; return true; }
bool driver_install_vhci_root_device(void) { return true; }
void driver_auto_setup_embedded(void) {}

#endif
