#include "relay_server.h"
#include <stdio.h>

int main(void) {
    relay_server_start(USBREDIR_PORT);
    return 0;
}
