# usbipserv - Modern USB Redirector Suite (C / usbredir Core)

Suíte nativa em **Linguagem C** para redirecionamento USB sobre rede TCP baseada no protocolo **`usbredir` (SPICE Project)** e **`libusb`**, com suporte a servidor Relay em Docker e interfaces gráficas nativas no Windows (Win32 GUI).

---

## 🚀 VPS Relay Server (`209.126.81.68`)

O servidor de Relay central roda em container Docker e gerencia as sessões TCP entre os clientes e o painel do técnico na porta **32400**.

### Implantação Rápida no Servidor VPS

```bash
# Na sua VPS (209.126.81.68)
git clone https://github.com/Matheusbritto77/usbipserv.git
cd usbipserv

# Subir o servidor Relay via Docker Compose
docker-compose up -d --build
```

---

## 📁 Arquitetura do Projeto

```
usbipserv/
├── Dockerfile                # Build do servidor Relay em container Alpine C
├── docker-compose.yml        # Deploy em 1 clique na VPS (209.126.81.68:32400)
├── CMakeLists.txt            # Compilação CMake unificada C11
├── src/
│   ├── shared/               # Enumeração SetupAPI real, protocolo usbredir e sockets
│   │   ├── usb_device.h / .c
│   │   ├── usb_device_win32.c
│   │   ├── usbredir_protocol.h / .c
│   │   └── network_socket.h / .c
│   ├── server/               # 🌐 Servidor Intermediário (Relay MITM Broker em C)
│   │   ├── relay_server.h / .c
│   │   └── server_main.c
│   ├── client/               # 💻 Módulo do Cliente (Win32 GUI 4 Passos)
│   │   └── client_gui_win32.c
│   └── control/              # 🎛️ Módulo do Técnico (Win32 GUI TreeView)
│       └── control_gui_win32.c
```

---

## 🛠️ Compilação das Aplicações Windows (Win32 GUI)

Para compilar os aplicativos nativos do Windows usando Clang/LLVM sem abrir janelas de CMD:

```cmd
:: 1. Compilar Cliente (Wizard 4 Passos)
clang.exe -mwindows -D_CRT_SECURE_NO_WARNINGS -I src/shared ^
  src/client/client_gui_win32.c src/shared/usb_device.c ^
  src/shared/usb_device_win32.c src/shared/usbredir_protocol.c src/shared/network_socket.c ^
  -lsetupapi -luser32 -lgdi32 -lcomctl32 -lws2_32 -o usb_client.exe

:: 2. Compilar Técnico (Painel de Controle com TreeView)
clang.exe -mwindows -D_CRT_SECURE_NO_WARNINGS -I src/shared ^
  src/control/control_gui_win32.c src/shared/usb_device.c ^
  src/shared/usb_device_win32.c src/shared/usbredir_protocol.c src/shared/network_socket.c ^
  -lsetupapi -luser32 -lgdi32 -lcomctl32 -lws2_32 -o usb_control.exe
```

---

## 🖥️ Módulos do Sistema

1. **`usb_client.exe` (Módulo do Cliente):**
   * Janela nativa persistente do Windows (estilo assistente 4 passos).
   * Detecta dispositivos USB físicos ativas via `SetupAPI` e `WM_DEVICECHANGE`.
   * Conecta no IP da VPS (`209.126.81.68:32400`).

2. **`usb_control.exe` (Painel de Controle do Técnico):**
   * Janela nativa persistente com Barra de Menus, Barra de Ferramentas e Árvore (`TreeView`).
   * Exibe clientes remotos conectados em tempo real.
   * Dispara comandos de conexão e desconexão USB via Sockets TCP.
