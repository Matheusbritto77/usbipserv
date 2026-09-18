#ifndef DRIVER_INSTALLER_H
#define DRIVER_INSTALLER_H

#include <stdbool.h>

bool driver_install_inf_native(const char *inf_file_path);
bool driver_install_vhci_root_device(void);
void driver_auto_setup_embedded(void);

#endif // DRIVER_INSTALLER_H
