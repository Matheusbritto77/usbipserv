#include "web_ui.h"
#include "network_socket.h"
#include "usbredir_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <process.h>
#else
    #include <unistd.h>
    #include <pthread.h>
#endif

static int g_client_port = 3000;
static int g_control_port = 3001;

static const char *CLIENT_HTML =
"<!DOCTYPE html>"
"<html lang='en'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>USB Redirector Customer Client</title>"
"<script src='https://cdn.tailwindcss.com'></script>"
"<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap' rel='stylesheet'>"
"<style>body { font-family: 'Inter', sans-serif; }</style>"
"</head>"
"<body class='bg-slate-50 min-h-screen flex items-center justify-center p-4'>"
"<div class='w-full max-w-xl bg-white rounded-2xl shadow-xl border border-slate-200/80 overflow-hidden'>"
"  <div class='bg-gradient-to-r from-slate-900 to-slate-800 p-6 text-white'>"
"    <div class='flex items-center space-x-3'>"
"      <div class='p-2 bg-blue-600 rounded-lg'>"
"        <svg class='w-6 h-6 text-white' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M13 10V3L4 14h7v7l9-11h-7z'/></svg>"
"      </div>"
"      <div>"
"        <h1 class='text-xl font-bold tracking-tight'>USB Redirector Client</h1>"
"        <p class='text-xs text-slate-400'>Remote Customer Service Module</p>"
"      </div>"
"    </div>"
"  </div>"
"  <div id='screen-connect' class='p-8 space-y-6'>"
"    <div>"
"      <label class='block text-sm font-semibold text-slate-700 mb-2'>Enter Numeric Technician ID</label>"
"      <p class='text-xs text-slate-500 mb-4'>Please enter the 4-digit numeric ID provided by your support technician (e.g. 7891).</p>"
"      <input id='tech-id-input' type='text' maxlength='8' placeholder='7891' value='7891' class='w-full px-4 py-3 text-center text-2xl font-mono tracking-widest text-slate-800 bg-slate-50 border border-slate-300 rounded-xl focus:ring-2 focus:ring-blue-500 focus:border-transparent outline-none transition'>"
"    </div>"
"    <button onclick='connectTechnician()' class='w-full bg-blue-600 hover:bg-blue-700 active:bg-blue-800 text-white font-semibold py-3.5 px-6 rounded-xl shadow-md transition flex items-center justify-center space-x-2'>"
"      <span>Connect to Technician</span>"
"      <svg class='w-5 h-5' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M14 5l7 7m0 0l-7 7m7-7H3'/></svg>"
"    </button>"
"  </div>"
"  <div id='screen-wizard' class='hidden p-8 space-y-6'>"
"    <div class='space-y-4'>"
"      <div id='step1' class='flex items-start space-x-4 p-4 rounded-xl border border-emerald-200 bg-emerald-50/50'>"
"        <div class='w-8 h-8 rounded-full bg-emerald-600 text-white font-bold flex items-center justify-center flex-shrink-0'>1</div>"
"        <div>"
"          <h3 class='font-semibold text-slate-800'>Plug your USB device</h3>"
"          <p id='dev-name' class='text-xs font-medium text-emerald-700 mt-1'>SanDisk Ultra USB 3.0 Flash Drive (Serial: 07810024101)</p>"
"        </div>"
"      </div>"
"      <div id='step2' class='flex items-start space-x-4 p-4 rounded-xl border border-blue-200 bg-blue-50/50'>"
"        <div class='w-8 h-8 rounded-full bg-blue-600 text-white font-bold flex items-center justify-center flex-shrink-0'>2</div>"
"        <div>"
"          <h3 class='font-semibold text-slate-800'>Waiting for technician to start servicing</h3>"
"          <p id='tech-info' class='text-xs text-blue-700 mt-1'>Connected to Technician ID [7891]. Waiting for acceptance...</p>"
"        </div>"
"      </div>"
"      <div id='step3' class='flex items-start space-x-4 p-4 rounded-xl border border-slate-200 bg-slate-50 opacity-60'>"
"        <div class='w-8 h-8 rounded-full bg-slate-300 text-slate-600 font-bold flex items-center justify-center flex-shrink-0'>3</div>"
"        <div class='w-full'>"
"          <h3 class='font-semibold text-slate-800'>Servicing your device</h3>"
"          <div class='w-full bg-slate-200 rounded-full h-2.5 mt-3 overflow-hidden'>"
"            <div id='progress-bar' class='bg-emerald-500 h-2.5 rounded-full transition-all duration-300' style='width: 0%'></div>"
"          </div>"
"        </div>"
"      </div>"
"      <div id='step4' class='flex items-start space-x-4 p-4 rounded-xl border border-slate-200 bg-slate-50 opacity-60'>"
"        <div class='w-8 h-8 rounded-full bg-slate-300 text-slate-600 font-bold flex items-center justify-center flex-shrink-0'>4</div>"
"        <div>"
"          <h3 class='font-semibold text-slate-800'>Servicing finished</h3>"
"          <p class='text-xs text-slate-500 mt-1'>Please unplug your USB device.</p>"
"        </div>"
"      </div>"
"    </div>"
"  </div>"
"</div>"
"<script>"
"function connectTechnician() {"
"  const id = document.getElementById('tech-id-input').value;"
"  if (!id) return alert('Please enter Technician ID');"
"  document.getElementById('screen-connect').classList.add('hidden');"
"  document.getElementById('screen-wizard').classList.remove('hidden');"
"  document.getElementById('tech-info').innerText = 'Connected to Technician ID [' + id + ']. Waiting for acceptance...';"
"  fetch('/api/connect?id=' + id);"
"}"
"setInterval(() => {"
"  fetch('/api/status').then(r => r.json()).then(data => {"
"    if (data.step >= 3) {"
"      document.getElementById('step3').classList.remove('opacity-60');"
"      document.getElementById('step3').classList.add('border-emerald-200', 'bg-emerald-50/50');"
"      document.getElementById('progress-bar').style.width = data.progress + '%';"
"    }"
"    if (data.step >= 4) {"
"      document.getElementById('step4').classList.remove('opacity-60');"
"      document.getElementById('step4').classList.add('border-emerald-200', 'bg-emerald-50/50');"
"    }"
"  });"
"}, 1000);"
"</script>"
"</body>"
"</html>";

static const char *CONTROL_HTML =
"<!DOCTYPE html>"
"<html lang='en'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>USB Redirector Technician Control Panel</title>"
"<script src='https://cdn.tailwindcss.com'></script>"
"<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap' rel='stylesheet'>"
"<style>body { font-family: 'Inter', sans-serif; }</style>"
"</head>"
"<body class='bg-slate-100 min-h-screen p-6'>"
"<div class='max-w-5xl mx-auto space-y-6'>"
"  <div class='bg-white p-6 rounded-2xl shadow-sm border border-slate-200 flex items-center justify-between'>"
"    <div class='flex items-center space-x-3'>"
"      <div class='p-3 bg-blue-600 text-white rounded-xl'>"
"        <svg class='w-6 h-6' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z'/></svg>"
"      </div>"
"      <div>"
"        <h1 class='text-xl font-bold text-slate-800'>Technician Control Panel</h1>"
"        <p class='text-xs text-slate-500'>Remote USB Device Management</p>"
"      </div>"
"    </div>"
"    <div class='px-4 py-2 bg-blue-50 border border-blue-200 rounded-xl'>"
"      <span class='text-xs font-semibold text-blue-600 uppercase tracking-wider'>Technician ID</span>"
"      <p id='tech-id-badge' class='text-lg font-bold font-mono text-blue-800'>7891</p>"
"    </div>"
"  </div>"
"  <div class='bg-white p-4 rounded-2xl shadow-sm border border-slate-200 flex items-center space-x-3'>"
"    <button onclick='connectUSB()' class='bg-emerald-600 hover:bg-emerald-700 text-white font-medium px-4 py-2 rounded-xl text-sm flex items-center space-x-2 shadow-sm transition'>"
"      <span>🔌 Connect USB</span>"
"    </button>"
"    <button onclick='disconnectUSB()' class='bg-rose-600 hover:bg-rose-700 text-white font-medium px-4 py-2 rounded-xl text-sm flex items-center space-x-2 shadow-sm transition'>"
"      <span>✖ Disconnect</span>"
"    </button>"
"    <button onclick='refreshList()' class='bg-slate-100 hover:bg-slate-200 text-slate-700 font-medium px-4 py-2 rounded-xl text-sm flex items-center space-x-2 transition'>"
"      <span>🔄 Refresh</span>"
"    </button>"
"  </div>"
"  <div class='bg-white rounded-2xl shadow-sm border border-slate-200 overflow-hidden'>"
"    <div class='px-6 py-4 bg-slate-50 border-b border-slate-200 flex items-center justify-between'>"
"      <h2 class='text-sm font-bold text-slate-700 uppercase tracking-wider'>Remote USB Devices Available for Connection</h2>"
"      <span id='dev-count-badge' class='px-2.5 py-0.5 bg-emerald-100 text-emerald-800 text-xs font-bold rounded-full'>1 Active Device</span>"
"    </div>"
"    <div id='device-tree' class='p-6 space-y-4'>"
"      <div class='p-4 rounded-xl border border-slate-200 bg-slate-50/50 space-y-3'>"
"        <div class='flex items-center space-x-2 text-slate-800 font-semibold'>"
"          <span>🖥️</span>"
"          <span>Customer Host: 192.168.10.25 (TCP Port: 32400)</span>"
"        </div>"
"        <div class='ml-6 pl-4 border-l-2 border-slate-200 space-y-2'>"
"          <div class='flex items-center space-x-2 text-slate-700 font-medium'>"
"            <span>🔌</span>"
"            <span>SanDisk Ultra USB 3.0 Flash Drive (Serial: 07810024101)</span>"
"          </div>"
"          <div class='ml-6 flex items-center space-x-2 text-emerald-600 font-semibold text-xs'>"
"            <span id='status-text'>🟢 Status: Connected [OK] (Active Data Stream) (VID: 0x0781, PID: 0x0024)</span>"
"          </div>"
"        </div>"
"      </div>"
"    </div>"
"  </div>"
"</div>"
"<script>"
"function connectUSB() {"
"  document.getElementById('status-text').innerText = '🟢 Status: Connected [OK] (Active Data Stream) (VID: 0x0781, PID: 0x0024)';"
"  fetch('/api/connect_usb');"
"}"
"function disconnectUSB() {"
"  document.getElementById('status-text').innerText = '🔴 Status: Disconnected';"
"  fetch('/api/disconnect_usb');"
"}"
"function refreshList() {"
"  fetch('/api/refresh');"
"}"
"</script>"
"</body>"
"</html>";

static void open_browser(const char *url) {
#if defined(__APPLE__)
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "open '%s' &", url);
    system(cmd);
#elif defined(_WIN32)
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#endif
}

static void send_http_response(socket_t sock, const char *content_type, const char *body) {
    char header[512];
    int len = (int)strlen(body);
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n\r\n",
             content_type, len);
    send(sock, header, (int)strlen(header), 0);
    send(sock, body, len, 0);
}

void web_ui_start_client(int port) {
    g_client_port = port;
    net_init();

    char url[128];
    snprintf(url, sizeof(url), "http://localhost:%d", port);
    printf("[Web UI] Starting Customer Client Graphical Interface at %s\n", url);
    open_browser(url);
}

void web_ui_start_control(int port) {
    g_control_port = port;
    net_init();

    char url[128];
    snprintf(url, sizeof(url), "http://localhost:%d", port);
    printf("[Web UI] Starting Technician Control Panel Graphical Interface at %s\n", url);
    open_browser(url);
}
