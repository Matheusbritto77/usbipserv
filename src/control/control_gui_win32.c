#ifdef _WIN32
#include "network_socket.h"
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <process.h>
#include "usb_device.h"
#include "usbredir_protocol.h"

#pragma comment(lib, "comctl32.lib")

#define IDM_PROGRAM_EXIT 1001
#define IDM_CONNECT_DEVICE 1002
#define IDM_DISCONNECT_DEVICE 1003
#define IDM_REFRESH_LIST 1004
#define IDM_SETTINGS 1005

#define WM_USER_REFRESH_TREE (WM_USER + 200)

static HWND g_hwndMain = NULL;
static HWND g_hwndTree = NULL;
static socket_t g_tech_sock = INVALID_SOCKET;

static char g_assigned_tech_id[32] = "7891";
static usbredir_packet_register_t g_remote_devices[16];
static int g_remote_count = 0;

// GDI Vector Icon Helper Functions for Modern Single-Tab UI
static void draw_modern_usb_icon(HDC hdc, int x, int y, COLORREF color) {
    // Metal Plug
    RECT rcMetal = { x + 2, y + 5, x + 8, y + 13 };
    HBRUSH hbMetal = CreateSolidBrush(RGB(210, 215, 220));
    FillRect(hdc, &rcMetal, hbMetal);
    DeleteObject(hbMetal);

    // Plug Holes
    RECT rcH1 = { x + 4, y + 7, x + 6, y + 9 };
    RECT rcH2 = { x + 4, y + 10, x + 6, y + 12 };
    HBRUSH hbBlack = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rcH1, hbBlack);
    FillRect(hdc, &rcH2, hbBlack);
    DeleteObject(hbBlack);

    // Plug Body
    RECT rcBody = { x + 8, y + 3, x + 18, y + 15 };
    HBRUSH hbBody = CreateSolidBrush(color);
    FillRect(hdc, &rcBody, hbBody);
    DeleteObject(hbBody);
}

static void draw_modern_refresh_icon(HDC hdc, int x, int y) {
    HPEN hp = CreatePen(PS_SOLID, 2, RGB(33, 150, 243));
    HPEN hpOld = (HPEN)SelectObject(hdc, hp);

    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, x + 2, y + 2, x + 16, y + 16);
    SelectObject(hdc, hbOld);

    MoveToEx(hdc, x + 12, y + 2, NULL); LineTo(hdc, x + 17, y + 2);
    MoveToEx(hdc, x + 16, y + 1, NULL); LineTo(hdc, x + 16, y + 6);

    SelectObject(hdc, hpOld);
    DeleteObject(hp);
}

static void draw_modern_settings_icon(HDC hdc, int x, int y) {
    HPEN hp = CreatePen(PS_SOLID, 2, RGB(100, 105, 115));
    HPEN hpOld = (HPEN)SelectObject(hdc, hp);

    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, x + 4, y + 4, x + 14, y + 14);
    SelectObject(hdc, hbOld);

    MoveToEx(hdc, x + 9, y + 1, NULL); LineTo(hdc, x + 9, y + 17);
    MoveToEx(hdc, x + 1, y + 9, NULL); LineTo(hdc, x + 17, y + 9);

    SelectObject(hdc, hpOld);
    DeleteObject(hp);
}

static void update_tree_view(void) {
    if (!g_hwndTree) return;

    TreeView_DeleteAllItems(g_hwndTree);

    if (g_remote_count == 0) {
        TVINSERTSTRUCT tvis;
        memset(&tvis, 0, sizeof(tvis));
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        char emptyLabel[256];
        snprintf(emptyLabel, sizeof(emptyLabel), "No remote customer USB devices connected yet. (Share Technician ID [%s] with customer)", g_assigned_tech_id);
        tvis.item.pszText = emptyLabel;
        TreeView_InsertItem(g_hwndTree, &tvis);
        return;
    }

    for (int i = 0; i < g_remote_count; i++) {
        char customerLabel[256];
        snprintf(customerLabel, sizeof(customerLabel), "Established direct connection to Customer at %s ( TCP port:32400 )",
                 g_remote_devices[i].client_ip);

        TVINSERTSTRUCT tvis;
        memset(&tvis, 0, sizeof(tvis));
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        tvis.item.pszText = customerLabel;
        HTREEITEM hCustomer = TreeView_InsertItem(g_hwndTree, &tvis);

        char devLabel[512];
        snprintf(devLabel, sizeof(devLabel), "%s  (s/n: %s)",
                 g_remote_devices[i].device.product_name, g_remote_devices[i].device.serial_number);

        memset(&tvis, 0, sizeof(tvis));
        tvis.hParent = hCustomer;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        tvis.item.pszText = devLabel;
        HTREEITEM hDevice = TreeView_InsertItem(g_hwndTree, &tvis);

        char propLabel[512];
        snprintf(propLabel, sizeof(propLabel), "Status: %s (VID: 0x%04X, PID: 0x%04X, Bus: %d, Addr: %d)",
                 usb_status_to_string(g_remote_devices[i].device.status),
                 g_remote_devices[i].device.vendor_id, g_remote_devices[i].device.product_id,
                 g_remote_devices[i].device.bus_number, g_remote_devices[i].device.device_address);

        memset(&tvis, 0, sizeof(tvis));
        tvis.hParent = hDevice;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        tvis.item.pszText = propLabel;
        TreeView_InsertItem(g_hwndTree, &tvis);

        TreeView_Expand(g_hwndTree, hCustomer, TVE_EXPAND);
        TreeView_Expand(g_hwndTree, hDevice, TVE_EXPAND);
    }
}

static unsigned __stdcall tech_network_thread(void *arg) {
    net_init();

    g_tech_sock = net_connect("209.126.81.68", USBREDIR_PORT);
    if (g_tech_sock != INVALID_SOCKET) {
        usbredir_header_t hdr;
        usbredir_header_init(&hdr, USBREDIR_CMD_REGISTER_TECH, 0);
        net_send_all(g_tech_sock, &hdr, sizeof(hdr));

        while (1) {
            usbredir_header_t rx_hdr;
            if (net_recv_all(g_tech_sock, &rx_hdr, sizeof(rx_hdr)) < 0) break;

            if (usbredir_header_verify(&rx_hdr)) {
                if (rx_hdr.cmd == USBREDIR_CMD_REGISTER_TECH) {
                    usbredir_packet_tech_init_t init_pkt;
                    if (net_recv_all(g_tech_sock, &init_pkt, sizeof(init_pkt)) == 0) {
                        strncpy(g_assigned_tech_id, init_pkt.tech_id, sizeof(g_assigned_tech_id));
                        PostMessage(g_hwndMain, WM_USER_REFRESH_TREE, 0, 0);
                    }
                } else if (rx_hdr.cmd == USBREDIR_CMD_LIST_DEVICES) {
                    usbredir_packet_register_t tech_pkt;
                    if (net_recv_all(g_tech_sock, &tech_pkt, sizeof(tech_pkt)) == 0) {
                        if (g_remote_count < 16) {
                            g_remote_devices[g_remote_count++] = tech_pkt;
                            PostMessage(g_hwndMain, WM_USER_REFRESH_TREE, 0, 0);
                        }
                    }
                }
            }
        }
    }
    return 0;
}

LRESULT CALLBACK ControlPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_hwndMain = hwnd;

        // Clean Menu Bar
        HMENU hMenuBar = CreateMenu();
        HMENU hMenuProgram = CreatePopupMenu();
        HMENU hMenuConnect = CreatePopupMenu();
        HMENU hMenuSettings = CreatePopupMenu();
        HMENU hMenuHelp = CreatePopupMenu();

        AppendMenu(hMenuProgram, MF_STRING, IDM_PROGRAM_EXIT, "Exit");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuProgram, "Program");

        AppendMenu(hMenuConnect, MF_STRING, IDM_CONNECT_DEVICE, "Connect Remote USB");
        AppendMenu(hMenuConnect, MF_STRING, IDM_DISCONNECT_DEVICE, "Disconnect Remote USB");
        AppendMenu(hMenuConnect, MF_STRING, IDM_REFRESH_LIST, "Refresh Device List");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuConnect, "Connect");

        AppendMenu(hMenuSettings, MF_STRING, IDM_SETTINGS, "Server Settings");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuSettings, "Settings");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp, "Help");

        SetMenu(hwnd, hMenuBar);

        // Single TreeView Control directly embedded (Single Tab / View)
        g_hwndTree = CreateWindowEx(
            WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL,
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
            12, 120, 756, 320,
            hwnd, (HMENU)2002, GetModuleHandle(NULL), NULL
        );

        update_tree_view();

        _beginthreadex(NULL, 0, tech_network_thread, NULL, 0, NULL);
        return 0;
    }

    case WM_USER_REFRESH_TREE: {
        update_tree_view();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDM_PROGRAM_EXIT) {
            DestroyWindow(hwnd);
        } else if (LOWORD(wParam) == IDM_CONNECT_DEVICE) {
            if (g_tech_sock != INVALID_SOCKET) {
                usbredir_header_t hdr;
                usbredir_header_init(&hdr, USBREDIR_CMD_START_SERVICE, 0);
                net_send_all(g_tech_sock, &hdr, sizeof(hdr));
                MessageBox(hwnd, "Sent 'Connect USB' command to remote Customer Client via usbredir.", "USB Redirector Control", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBox(hwnd, "Simulated 'Connect USB' command sent to remote Customer Client.", "USB Redirector Control", MB_OK | MB_ICONINFORMATION);
            }
        } else if (LOWORD(wParam) == IDM_DISCONNECT_DEVICE) {
            if (g_tech_sock != INVALID_SOCKET) {
                usbredir_header_t hdr;
                usbredir_header_init(&hdr, USBREDIR_CMD_FINISH_SERVICE, 0);
                net_send_all(g_tech_sock, &hdr, sizeof(hdr));
            }
            MessageBox(hwnd, "Disconnected remote USB device.", "USB Redirector Control", MB_OK | MB_ICONINFORMATION);
        } else if (LOWORD(wParam) == IDM_REFRESH_LIST) {
            update_tree_view();
        } else if (LOWORD(wParam) == IDM_SETTINGS) {
            MessageBox(hwnd, "Server Relay VPS: 209.126.81.68:32400 (Active)", "USB Redirector Settings", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // Header Background Banner
        RECT rcHeader = { 0, 0, rcClient.right, 50 };
        HBRUSH hhdrBrush = CreateSolidBrush(RGB(250, 251, 253));
        FillRect(hdc, &rcHeader, hhdrBrush);
        DeleteObject(hhdrBrush);

        // Header Separator Line
        HPEN hPenLine = CreatePen(PS_SOLID, 1, RGB(220, 224, 230));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPenLine);
        MoveToEx(hdc, 0, 50, NULL);
        LineTo(hdc, rcClient.right, 50);

        // Header Title
        SetBkMode(hdc, TRANSPARENT);
        HFONT hFontTitle = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFontTitle);

        SetTextColor(hdc, RGB(30, 35, 45));
        TextOut(hdc, 16, 14, "USB Redirector Technician Control Panel", 39);

        // Technician ID Badge Box on Top Right
        RECT rcIdBox = { rcClient.right - 220, 10, rcClient.right - 16, 40 };
        HBRUSH hbIdBg = CreateSolidBrush(RGB(232, 240, 254));
        FillRect(hdc, &rcIdBox, hbIdBg);
        DeleteObject(hbIdBg);
        FrameRect(hdc, &rcIdBox, (HBRUSH)GetStockObject(BLACK_BRUSH));

        HFONT hFontId = CreateFont(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        SelectObject(hdc, hFontId);
        SetTextColor(hdc, RGB(25, 103, 210));
        char techIdBanner[128];
        snprintf(techIdBanner, sizeof(techIdBanner), "Technician ID: [ %s ]", g_assigned_tech_id);
        DrawText(hdc, techIdBanner, -1, &rcIdBox, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Toolbar Background Banner (Y: 50..95)
        RECT rcToolbar = { 0, 50, rcClient.right, 95 };
        HBRUSH htbBrush = CreateSolidBrush(RGB(242, 244, 247));
        FillRect(hdc, &rcToolbar, htbBrush);
        DeleteObject(htbBrush);

        MoveToEx(hdc, 0, 95, NULL);
        LineTo(hdc, rcClient.right, 95);

        // Render Modern Clean 4 Toolbar Buttons
        HFONT hFontBtn = CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        SelectObject(hdc, hFontBtn);

        // Button 1: Connect USB (Green)
        RECT rcB1 = { 12, 58, 140, 88 };
        HBRUSH hbB1 = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcB1, hbB1);
        DeleteObject(hbB1);
        FrameRect(hdc, &rcB1, (HBRUSH)GetStockObject(BLACK_BRUSH));
        draw_modern_usb_icon(hdc, 20, 64, RGB(46, 125, 50));
        SetTextColor(hdc, RGB(46, 125, 50));
        RECT rcT1 = { 42, 58, 136, 88 };
        DrawText(hdc, "Connect USB", -1, &rcT1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Button 2: Disconnect (Red)
        RECT rcB2 = { 148, 58, 266, 88 };
        HBRUSH hbB2 = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcB2, hbB2);
        DeleteObject(hbB2);
        FrameRect(hdc, &rcB2, (HBRUSH)GetStockObject(BLACK_BRUSH));
        draw_modern_usb_icon(hdc, 156, 64, RGB(211, 47, 47));
        SetTextColor(hdc, RGB(211, 47, 47));
        RECT rcT2 = { 178, 58, 262, 88 };
        DrawText(hdc, "Disconnect", -1, &rcT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Button 3: Refresh (Blue)
        RECT rcB3 = { 274, 58, 380, 88 };
        HBRUSH hbB3 = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcB3, hbB3);
        DeleteObject(hbB3);
        FrameRect(hdc, &rcB3, (HBRUSH)GetStockObject(BLACK_BRUSH));
        draw_modern_refresh_icon(hdc, 282, 65);
        SetTextColor(hdc, RGB(33, 150, 243));
        RECT rcT3 = { 306, 58, 376, 88 };
        DrawText(hdc, "Refresh", -1, &rcT3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Button 4: Settings (Gray)
        RECT rcB4 = { 388, 58, 494, 88 };
        HBRUSH hbB4 = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcB4, hbB4);
        DeleteObject(hbB4);
        FrameRect(hdc, &rcB4, (HBRUSH)GetStockObject(BLACK_BRUSH));
        draw_modern_settings_icon(hdc, 396, 64);
        SetTextColor(hdc, RGB(80, 85, 95));
        RECT rcT4 = { 420, 58, 490, 88 };
        DrawText(hdc, "Settings", -1, &rcT4, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Single View Title Header
        HFONT hFontTabHead = CreateFont(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        SelectObject(hdc, hFontTabHead);
        SetTextColor(hdc, RGB(50, 55, 65));
        TextOut(hdc, 14, 98, "Remote USB Devices Shared With Us:", 34);

        SelectObject(hdc, hOldFont);
        SelectObject(hdc, hOldPen);
        DeleteObject(hFontTitle);
        DeleteObject(hFontId);
        DeleteObject(hFontBtn);
        DeleteObject(hFontTabHead);
        DeleteObject(hPenLine);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // Toolbar Buttons (Y: 58..88)
        if (y >= 58 && y <= 88) {
            if (x >= 12 && x <= 140) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_CONNECT_DEVICE, 0), 0);
            } else if (x >= 148 && x <= 266) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_DISCONNECT_DEVICE, 0), 0);
            } else if (x >= 274 && x <= 380) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_REFRESH_LIST, 0), 0);
            } else if (x >= 388 && x <= 494) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_SETTINGS, 0), 0);
            }
        }
        return 0;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (g_hwndTree) {
            SetWindowPos(g_hwndTree, NULL, 12, 120, width - 24, height - 132, SWP_NOZORDER);
        }
        return 0;
    }

    case WM_DESTROY:
        if (g_tech_sock != INVALID_SOCKET) {
            net_close(g_tech_sock);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    const char CLASS_NAME[] = "UsbRedirectorControlPanel";
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = ControlPanelProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME,
        "USB Redirector Technician Control Panel",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 780, 500,
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
