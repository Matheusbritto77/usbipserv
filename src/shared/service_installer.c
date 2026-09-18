#include "service_installer.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

bool service_install(const char *service_name, const char *display_name, const char *exe_path) {
    SC_HANDLE schSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) {
        printf("[Service] Failed to open Service Control Manager (error %ld)\n", GetLastError());
        return false;
    }

    SC_HANDLE schService = CreateServiceA(
        schSCManager,
        service_name,
        display_name,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        exe_path,
        NULL, NULL, NULL, NULL, NULL
    );

    if (!schService) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            printf("[Service] Service '%s' is already registered.\n", service_name);
        } else {
            printf("[Service] CreateService failed (error %ld)\n", err);
        }
        CloseServiceHandle(schSCManager);
        return false;
    }

    printf("[Service] Service '%s' registered successfully.\n", service_name);
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

bool service_uninstall(const char *service_name) {
    SC_HANDLE schSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) return false;

    SC_HANDLE schService = OpenServiceA(schSCManager, service_name, DELETE);
    if (!schService) {
        CloseServiceHandle(schSCManager);
        return false;
    }

    bool ret = DeleteService(schService);
    if (ret) {
        printf("[Service] Service '%s' removed successfully.\n", service_name);
    }
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return ret;
}

bool service_start(const char *service_name) {
    SC_HANDLE schSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) return false;

    SC_HANDLE schService = OpenServiceA(schSCManager, service_name, SERVICE_START);
    if (!schService) {
        CloseServiceHandle(schSCManager);
        return false;
    }

    bool ret = StartServiceA(schService, 0, NULL);
    if (ret) {
        printf("[Service] Service '%s' started successfully.\n", service_name);
    }
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return ret;
}

bool service_stop(const char *service_name) {
    SC_HANDLE schSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) return false;

    SC_HANDLE schService = OpenServiceA(schSCManager, service_name, SERVICE_STOP);
    if (!schService) {
        CloseServiceHandle(schSCManager);
        return false;
    }

    SERVICE_STATUS status;
    bool ret = ControlService(schService, SERVICE_CONTROL_STOP, &status);
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return ret;
}

#else

bool service_install(const char *service_name, const char *display_name, const char *exe_path) {
    (void)service_name; (void)display_name; (void)exe_path;
    return true;
}
bool service_uninstall(const char *service_name) { (void)service_name; return true; }
bool service_start(const char *service_name) { (void)service_name; return true; }
bool service_stop(const char *service_name) { (void)service_name; return true; }

#endif
