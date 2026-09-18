#ifdef _WIN32
#include "network_socket.h"
#include <windows.h>
#include <commctrl.h>
#include <dbt.h>
#include <stdio.h>
#include <process.h>
#include "usb_device.h"
#include "usbredir_protocol.h"
#include "ipc_channel.h"

#pragma comment(lib, "comctl32.lib")

#define TIMER_ID_PROGRESS 101
#define WM_USER_SERVER_CMD (WM_USER + 100)
#define WM_USER_IPC_EVENT   (WM_USER + 101)

#define IDC_EDIT_TECH_ID 301
#define IDC_BTN_CONNECT_TECH 302
#define IDC_BTN_CANCEL 303

typedef enum {
    GUI_STATE_CONNECT_DIALOG = 0,
    GUI_STATE_WIZARD_STEPS   = 1
} gui_state_t;

static HWND g_hwndMain = NULL;
static HWND g_hwndEditTech = NULL;
static HWND g_hwndBtnConnect = NULL;
static HWND g_hwndProgress = NULL;
static HWND g_hwndButton = NULL;

static gui_state_t g_gui_state = GUI_STATE_CONNECT_DIALOG;
static int g_current_step = 1;
static int g_progress_val = 0;
static usb_device_info_t g_devices[MAX_USB_DEVICES];
static int g_num_devices = 0;
static socket_t g_client_sock = INVALID_SOCKET;
static char g_entered_tech_id[32] = "";

static void refresh_usb_hardware(void) {
    g_num_devices = usb_device_enumerate_real(g_devices, MAX_USB_DEVICES);
}

static unsigned __stdcall client_network_thread(void *arg) {
    net_init();

    g_client_sock = net_connect("209.126.81.68", USBREDIR_PORT);
    if (g_client_sock != INVALID_SOCKET) {
        usbredir_header_t hdr;
        usbredir_header_init(&hdr, USBREDIR_CMD_REGISTER_CLIENT, sizeof(usbredir_packet_register_t));

        usbredir_packet_register_t reg_pkt;
        memset(&reg_pkt, 0, sizeof(reg_pkt));
        strncpy(reg_pkt.client_ip, "192.168.10.25", sizeof(reg_pkt.client_ip) - 1);
        strncpy(reg_pkt.target_tech_id, g_entered_tech_id, sizeof(reg_pkt.target_tech_id) - 1);

        if (g_num_devices > 0) {
            reg_pkt.device = g_devices[0];
        } else {
            usb_device_init(&reg_pkt.device);
        }

        net_send_all(g_client_sock, &hdr, sizeof(hdr));
        net_send_all(g_client_sock, &reg_pkt, sizeof(reg_pkt));

        PostMessage(g_hwndMain, WM_USER_SERVER_CMD, 2, 0);

        while (1) {
            usbredir_header_t rx_hdr;
            if (net_recv_all(g_client_sock, &rx_hdr, sizeof(rx_hdr)) < 0) break;
            if (usbredir_header_verify(&rx_hdr)) {
                if (rx_hdr.cmd == USBREDIR_CMD_START_SERVICE) {
                    PostMessage(g_hwndMain, WM_USER_SERVER_CMD, 3, 0);
                } else if (rx_hdr.cmd == USBREDIR_CMD_FINISH_SERVICE) {
                    PostMessage(g_hwndMain, WM_USER_SERVER_CMD, 4, 0);
                }
            }
        }
    } else {
        Sleep(1000);
        PostMessage(g_hwndMain, WM_USER_SERVER_CMD, 2, 0);
    }
    return 0;
}

LRESULT CALLBACK ClientWizardProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_hwndMain = hwnd;
        refresh_usb_hardware();

        // Enforce Numeric-Only Input (ES_NUMBER)
        g_hwndEditTech = CreateWindowEx(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
            160, 180, 260, 28,
            hwnd, (HMENU)IDC_EDIT_TECH_ID, GetModuleHandle(NULL), NULL
        );

        g_hwndBtnConnect = CreateWindowEx(
            0, "BUTTON", "Connect",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            430, 180, 100, 28,
            hwnd, (HMENU)IDC_BTN_CONNECT_TECH, GetModuleHandle(NULL), NULL
        );

        g_hwndProgress = CreateWindowEx(
            0, PROGRESS_CLASS, NULL,
            WS_CHILD | PBS_SMOOTH,
            240, 260, 280, 20,
            hwnd, (HMENU)201, GetModuleHandle(NULL), NULL
        );
        SendMessage(g_hwndProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessage(g_hwndProgress, PBM_SETPOS, 0, 0);

        g_hwndButton = CreateWindowEx(
            0, "BUTTON", "Cancel",
            WS_CHILD | BS_PUSHBUTTON,
            460, 370, 100, 32,
            hwnd, (HMENU)IDC_BTN_CANCEL, GetModuleHandle(NULL), NULL
        );

        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_BTN_CONNECT_TECH) {
            GetWindowText(g_hwndEditTech, g_entered_tech_id, sizeof(g_entered_tech_id));
            if (strlen(g_entered_tech_id) == 0) {
                MessageBox(hwnd, "Please enter the numeric Technician ID (e.g. 7891) provided by your support technician.", "USB Redirector Client", MB_OK | MB_ICONWARNING);
                return 0;
            }

            g_gui_state = GUI_STATE_WIZARD_STEPS;
            g_current_step = 1;

            ShowWindow(g_hwndEditTech, SW_HIDE);
            ShowWindow(g_hwndBtnConnect, SW_HIDE);

            ShowWindow(g_hwndProgress, SW_SHOW);
            ShowWindow(g_hwndButton, SW_SHOW);

            InvalidateRect(hwnd, NULL, TRUE);

            _beginthreadex(NULL, 0, client_network_thread, NULL, 0, NULL);
        } else if (LOWORD(wParam) == IDC_BTN_CANCEL || LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IDOK) {
            if (g_client_sock != INVALID_SOCKET) {
                net_close(g_client_sock);
            }
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DEVICECHANGE: {
        if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
            refresh_usb_hardware();
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_USER_SERVER_CMD: {
        int next_step = (int)wParam;
        g_current_step = next_step;

        if (g_current_step == 3) {
            SetTimer(hwnd, TIMER_ID_PROGRESS, 200, NULL);
        } else if (g_current_step == 4) {
            SetWindowText(g_hwndButton, "Finish");
            KillTimer(hwnd, TIMER_ID_PROGRESS);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_TIMER: {
        if (wParam == TIMER_ID_PROGRESS) {
            g_progress_val += 10;
            SendMessage(g_hwndProgress, PBM_SETPOS, g_progress_val, 0);
            if (g_progress_val >= 100) {
                g_current_step = 4;
                SetWindowText(g_hwndButton, "Finish");
                KillTimer(hwnd, TIMER_ID_PROGRESS);
            }
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        HBRUSH hbgBrush = CreateSolidBrush(RGB(245, 246, 248));
        FillRect(hdc, &rcClient, hbgBrush);
        DeleteObject(hbgBrush);

        RECT rcHeader = { 0, 0, rcClient.right, 70 };
        HBRUSH hhdrBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcHeader, hhdrBrush);
        DeleteObject(hhdrBrush);

        HPEN hPenLine = CreatePen(PS_SOLID, 1, RGB(218, 220, 224));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPenLine);
        MoveToEx(hdc, 0, 70, NULL);
        LineTo(hdc, rcClient.right, 70);

        SetBkMode(hdc, TRANSPARENT);

        HFONT hFontTitle = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFontTitle);

        if (g_gui_state == GUI_STATE_CONNECT_DIALOG) {
            SetTextColor(hdc, RGB(30, 30, 30));
            TextOut(hdc, 24, 14, "USB Redirector Client", 21);

            HFONT hFontSub = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            SelectObject(hdc, hFontSub);
            SetTextColor(hdc, RGB(110, 115, 125));
            TextOut(hdc, 24, 40, "Connect to Remote Technician Server", 35);

            HFONT hFontLabel = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            SelectObject(hdc, hFontLabel);
            SetTextColor(hdc, RGB(40, 40, 40));
            TextOut(hdc, 50, 120, "Enter Numeric Technician ID:", 28);

            HFONT hFontHint = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            SelectObject(hdc, hFontHint);
            SetTextColor(hdc, RGB(120, 120, 120));
            TextOut(hdc, 50, 148, "Please type the numeric Technician ID (e.g. 7891) provided by your technician.", 78);

            TextOut(hdc, 50, 184, "Technician ID:", 14);

            DeleteObject(hFontSub);
            DeleteObject(hFontLabel);
            DeleteObject(hFontHint);
        } else {
            SetTextColor(hdc, RGB(30, 30, 30));
            TextOut(hdc, 24, 14, "Ready to Service Your Device", 28);

            HFONT hFontSub = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            SelectObject(hdc, hFontSub);
            SetTextColor(hdc, RGB(110, 115, 125));
            TextOut(hdc, 24, 40, "Please follow instructions below", 32);

            HFONT hFontNumActive = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            HFONT hFontTextActive = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                               CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            HFONT hFontTextInactive = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            HFONT hFontDetail = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");

            int y_positions[4] = { 90, 170, 240, 310 };

            // STEP 1
            SelectObject(hdc, hFontNumActive);
            SetTextColor(hdc, (g_current_step >= 1) ? RGB(76, 175, 80) : RGB(180, 180, 180));
            TextOut(hdc, 40, y_positions[0], "1", 1);

            SelectObject(hdc, (g_current_step >= 1) ? hFontTextActive : hFontTextInactive);
            SetTextColor(hdc, (g_current_step >= 1) ? RGB(30, 30, 30) : RGB(150, 150, 150));
            TextOut(hdc, 70, y_positions[0] + 4, "Plug your USB device", 20);

            SelectObject(hdc, hFontDetail);
            SetTextColor(hdc, RGB(100, 100, 100));

            char devStr[256];
            if (g_num_devices > 0) {
                snprintf(devStr, sizeof(devStr), "Detected USB: %s (VID: 0x%04X, PID: 0x%04X)",
                         g_devices[0].product_name, g_devices[0].vendor_id, g_devices[0].product_id);
            } else {
                snprintf(devStr, sizeof(devStr), "Scanning USB ports... Please plug your device into a USB port.");
            }
            TextOut(hdc, 70, y_positions[0] + 28, devStr, (int)strlen(devStr));

            // STEP 2
            SelectObject(hdc, hFontNumActive);
            SetTextColor(hdc, (g_current_step >= 2) ? RGB(76, 175, 80) : RGB(180, 180, 180));
            TextOut(hdc, 40, y_positions[1], "2", 1);

            SelectObject(hdc, (g_current_step >= 2) ? hFontTextActive : hFontTextInactive);
            SetTextColor(hdc, (g_current_step >= 2) ? RGB(30, 30, 30) : RGB(150, 150, 150));
            TextOut(hdc, 70, y_positions[1] + 4, "Waiting for technician to start servicing your device", 53);

            SelectObject(hdc, hFontDetail);
            SetTextColor(hdc, RGB(100, 100, 100));
            char techStatusStr[256];
            snprintf(techStatusStr, sizeof(techStatusStr), "Connected to Technician ID [%s]. Waiting for technician to accept...", g_entered_tech_id);
            TextOut(hdc, 70, y_positions[1] + 28, techStatusStr, (int)strlen(techStatusStr));

            // STEP 3
            SelectObject(hdc, hFontNumActive);
            SetTextColor(hdc, (g_current_step >= 3) ? RGB(76, 175, 80) : RGB(180, 180, 180));
            TextOut(hdc, 40, y_positions[2], "3", 1);

            SelectObject(hdc, (g_current_step >= 3) ? hFontTextActive : hFontTextInactive);
            SetTextColor(hdc, (g_current_step >= 3) ? RGB(30, 30, 30) : RGB(150, 150, 150));
            TextOut(hdc, 70, y_positions[2] + 4, "Servicing your device", 21);

            SelectObject(hdc, hFontDetail);
            SetTextColor(hdc, RGB(100, 100, 100));
            TextOut(hdc, 70, y_positions[2] + 48, "Technician is servicing your device, this may take awhile. Please be patient.", 77);

            // STEP 4
            SelectObject(hdc, hFontNumActive);
            SetTextColor(hdc, (g_current_step >= 4) ? RGB(76, 175, 80) : RGB(180, 180, 180));
            TextOut(hdc, 40, y_positions[3], "4", 1);

            SelectObject(hdc, (g_current_step >= 4) ? hFontTextActive : hFontTextInactive);
            SetTextColor(hdc, (g_current_step >= 4) ? RGB(30, 30, 30) : RGB(150, 150, 150));
            TextOut(hdc, 70, y_positions[3] + 4, "Servicing of your device has been finished", 42);

            if (g_current_step == 4) {
                SelectObject(hdc, hFontDetail);
                SetTextColor(hdc, RGB(211, 47, 47));
                TextOut(hdc, 70, y_positions[3] + 28, "Please unplug the device from USB port and click Finish to close this program.", 78);
            }

            DeleteObject(hFontSub);
            DeleteObject(hFontNumActive);
            DeleteObject(hFontTextActive);
            DeleteObject(hFontTextInactive);
            DeleteObject(hFontDetail);
        }

        SelectObject(hdc, hOldFont);
        SelectObject(hdc, hOldPen);
        DeleteObject(hFontTitle);
        DeleteObject(hPenLine);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    const char CLASS_NAME[] = "UsbRedirectorClientWizard";
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = ClientWizardProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME,
        "USB Redirector Client",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 460,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
#endif // _WIN32
