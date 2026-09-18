#ifndef SERVICE_INSTALLER_H
#define SERVICE_INSTALLER_H

#include <stdbool.h>

bool service_install(const char *service_name, const char *display_name, const char *exe_path);
bool service_uninstall(const char *service_name);
bool service_start(const char *service_name);
bool service_stop(const char *service_name);

#endif // SERVICE_INSTALLER_H
