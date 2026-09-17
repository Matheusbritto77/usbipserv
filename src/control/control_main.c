#include "control_panel.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    const char *server_ip = "127.0.0.1";
    if (argc > 1) {
        server_ip = argv[1];
    }
    control_panel_run(server_ip);
    return 0;
}
