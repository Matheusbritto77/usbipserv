#ifndef WEB_UI_H
#define WEB_UI_H

#include "ipc_channel.h"
#include <stdbool.h>

void web_ui_start_client(int port);
void web_ui_start_control(int port);

#endif // WEB_UI_H
