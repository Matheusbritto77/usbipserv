cmake_minimum_required(VERSION 3.20)
project(usb_redirector_suite C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Includes
include_directories(
    src/shared
    external
)

# Compile flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -g)
endif()

# Source files
set(SHARED_SOURCES
    src/shared/usb_device.c
    src/shared/usb_device_win32.c
    src/shared/usbredir_protocol.c
    src/shared/network_socket.c
    src/shared/ipc_channel.c
    src/shared/web_ui.c
)

# 1. Server Component (Relay MITM Broker)
add_executable(usb_server
    src/server/server_main.c
    src/server/relay_server.c
    ${SHARED_SOURCES}
)

# 2. Client Component (Customer Wizard)
add_executable(usb_client
    src/client/client_main.c
    src/client/client_wizard.c
    ${SHARED_SOURCES}
)

# 3. Control Component (Technician Panel)
add_executable(usb_control
    src/control/control_main.c
    src/control/control_panel.c
    ${SHARED_SOURCES}
)

# Threading library
find_package(Threads REQUIRED)
target_link_libraries(usb_server PRIVATE Threads::Threads)
target_link_libraries(usb_client PRIVATE Threads::Threads)
target_link_libraries(usb_control PRIVATE Threads::Threads)

message(STATUS "Configured USB Redirector Suite (C / usbredir core)")
