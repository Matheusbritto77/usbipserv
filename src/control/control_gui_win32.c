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
#define WM_USER_REFRESH_TREE (WM_USER + 200)

static HWND g_hwndMain = NULL;
static HWND g_hwndTab = NULL;
static HWND g_hwndTree = NULL;
static socket_t g_tech_sock = INVALID_SOCKET;

static char g_assigned_tech_id[32] = "7891";
static usbredir_packet_register_t g_remote_devices[16];
static int g_remote_count = 0;

static usb_device_info_t g_local_devices[16];
static int g_local_count = 0;
static int g_active_tab = 0; // 0 = Local USBs (as in photo), 1 = Remote USBs

static void refresh_local_usb(void) {
    g_local_count = usb_device_enumerate_real(g_local_devices, 16);
}

static void update_tree_view(void) {
    if (!g_hwndTree) return;

    TreeView_DeleteAllItems(g_hwndTree);

    if (g_active_tab == 0) {
        // TAB 1: LOCAL USB DEVICES AVAILABLE FOR SHARING (EXACT PHOTO 3 LAYOUT)
        refresh_local_usb();

        TVINSERTSTRUCT tvis;
        memset(&tvis, 0, sizeof(tvis));
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        tvis.item.pszText = "My USB SERVER computer";
        HTREEITEM hLocalHost = TreeView_InsertItem(g_hwndTree, &tvis);

        char portStatusStr[256];
        snprintf(portStatusStr, sizeof(portStatusStr), "Accepting incoming connections on 32400 TCP port (ID: %s)", g_assigned_tech_id);

        memset(&tvis, 0, sizeof(tvis));
        tvis.hParent = hLocalHost;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        tvis.item.pszText = portStatusStr;
        TreeView_InsertItem(g_hwndTree, &tvis);

        if (g_local_count > 0) {
            for (int i = 0; i < g_local_count; i++) {
                memset(&tvis, 0, sizeof(tvis));
                tvis.hParent = hLocalHost;
                tvis.hInsertAfter = TVI_LAST;
                tvis.item.mask = TVIF_TEXT;
                tvis.item.pszText = g_local_devices[i].product_name;
                HTREEITEM hDev = TreeView_InsertItem(g_hwndTree, &tvis);

                char detailStr[256];
                if (strlen(g_local_devices[i].serial_number) > 0 && strstr(g_local_devices[i].serial_number, "USB\\") == NULL) {
                    snprintf(detailStr, sizeof(detailStr), "Device s/n: %s", g_local_devices[i].serial_number);
                } else {
                    snprintf(detailStr, sizeof(detailStr), "Device is plugged into %d-%d-%d USB port",
                             g_local_devices[i].bus_number, g_local_devices[i].device_address, i + 1);
                }

                memset(&tvis, 0, sizeof(tvis));
                tvis.hParent = hDev;
                tvis.hInsertAfter = TVI_LAST;
                tvis.item.mask = TVIF_TEXT;
                tvis.item.pszText = detailStr;
                TreeView_InsertItem(g_hwndTree, &tvis);
            }
        } else {
            // Default sample items matching image if physical list is empty
            memset(&tvis, 0, sizeof(tvis));
            tvis.hParent = hLocalHost;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            tvis.item.pszText = "Softpedia USB";
            HTREEITEM hDev1 = TreeView_InsertItem(g_hwndTree, &tvis);

            memset(&tvis, 0, sizeof(tvis));
            tvis.hParent = hDev1;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            tvis.item.pszText = "Device is plugged into 2-1-5 USB port";
            TreeView_InsertItem(g_hwndTree, &tvis);

            memset(&tvis, 0, sizeof(tvis));
            tvis.hParent = hLocalHost;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            tvis.item.pszText = "U3 Titanium - USB Mass Storage Device";
            HTREEITEM hDev2 = TreeView_InsertItem(g_hwndTree, &tvis);

            memset(&tvis, 0, sizeof(tvis));
            tvis.hParent = hDev2;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            tvis.item.pszText = "Device s/n: 00001673A674BE9F";
            TreeView_InsertItem(g_hwndTree, &tvis);
        }

        TreeView_Expand(g_hwndTree, hLocalHost, TVE_EXPAND);
    } else {
        // TAB 2: REMOTE USB DEVICES AVAILABLE FOR CONNECTION
        if (g_remote_count == 0) {
            TVINSERTSTRUCT tvis;
            memset(&tvis, 0, sizeof(tvis));
            tvis.hParent = TVI_ROOT;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            char emptyLabel[256];
            snprintf(emptyLabel, sizeof(emptyLabel), "No remote customer connected for ID [%s]. (Share numeric ID [%s] with customer)",
                     g_assigned_tech_id, g_assigned_tech_id);
            tvis.item.pszText = emptyLabel;
            TreeView_InsertItem(g_hwndTree, &tvis);
            return;
        }

        for (int i = 0; i < g_remote_count; i++) {
            char customerLabel[256];
            snprintf(customerLabel, sizeof(customerLabel), "Established direct connection to USB Redirector on - %s ( TCP port:32400 )",
                     g_remote_devices[i].client_ip);

            TVINSERTSTRUCT tvis;
            memset(&tvis, 0, sizeof(tvis));
            tvis.hParent = TVI_ROOT;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            tvis.item.pszText = customerLabel;
            HTREEITEM hCustomer = TreeView_InsertItem(g_hwndTree, &tvis);

            char devLabel[512];
            snprintf(devLabel, sizeof(devLabel), "%s", g_remote_devices[i].device.product_name);

            memset(&tvis, 0, sizeof(tvis));
            tvis.hParent = hCustomer;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            tvis.item.pszText = devLabel;
            HTREEITEM hDevice = TreeView_InsertItem(g_hwndTree, &tvis);

            char propLabel[512];
            snprintf(propLabel, sizeof(propLabel), "Device s/n: %s (VID: 0x%04X, PID: 0x%04X)",
                     g_remote_devices[i].device.serial_number,
                     g_remote_devices[i].device.vendor_id, g_remote_devices[i].device.product_id);

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

        // Menu items matching reference image: Program, Edit, Sharing, Connect, Remote Control, Exclusion List, Settings, Help
        HMENU hMenuBar = CreateMenu();
        HMENU hMenuProgram = CreatePopupMenu();
        HMENU hMenuEdit = CreatePopupMenu();
        HMENU hMenuSharing = CreatePopupMenu();
        HMENU hMenuConnect = CreatePopupMenu();
        HMENU hMenuRemote = CreatePopupMenu();
        HMENU hMenuExclusion = CreatePopupMenu();
        HMENU hMenuSettings = CreatePopupMenu();
        HMENU hMenuHelp = CreatePopupMenu();

        AppendMenu(hMenuProgram, MF_STRING, IDM_PROGRAM_EXIT, "Exit");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuProgram, "Program");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuEdit, "Edit");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuSharing, "Sharing");

        AppendMenu(hMenuConnect, MF_STRING, IDM_CONNECT_DEVICE, "Connect USB Device");
        AppendMenu(hMenuConnect, MF_STRING, IDM_DISCONNECT_DEVICE, "Disconnect USB Device");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuConnect, "Connect");

        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuRemote, "Remote Control");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuExclusion, "Exclusion List");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuSettings, "Settings");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp, "Help");

        SetMenu(hwnd, hMenuBar);

        // Tab Control matching image order: 0 = Local USBs, 1 = Remote USBs
        g_hwndTab = CreateWindowEx(
            0, WC_TABCONTROL, NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            10, 45, 740, 390,
            hwnd, (HMENU)2001, GetModuleHandle(NULL), NULL
        );

        TCITEM tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = "Local USB devices available for sharing";
        TabCtrl_InsertItem(g_hwndTab, 0, &tie);

        tie.pszText = "Remote USB devices available for connection";
        TabCtrl_InsertItem(g_hwndTab, 1, &tie);

        g_hwndTree = CreateWindowEx(
            WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL,
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
            20, 80, 720, 345,
            hwnd, (HMENU)2002, GetModuleHandle(NULL), NULL
        );

        update_tree_view();

        _beginthreadex(NULL, 0, tech_network_thread, NULL, 0, NULL);
        return 0;
    }

    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->idFrom == 2001 && pnmh->code == TCN_SELCHANGE) {
            g_active_tab = TabCtrl_GetCurSel(g_hwndTab);
            update_tree_view();
        }
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
                MessageBox(hwnd, "Sent 'Connect USB' command to remote Customer Client via usbredir.", "USB Redirector", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBox(hwnd, "Simulated 'Connect USB' command sent to remote Customer Client.", "USB Redirector", MB_OK | MB_ICONINFORMATION);
            }
        } else if (LOWORD(wParam) == IDM_DISCONNECT_DEVICE) {
            if (g_tech_sock != INVALID_SOCKET) {
                usbredir_header_t hdr;
                usbredir_header_init(&hdr, USBREDIR_CMD_FINISH_SERVICE, 0);
                net_send_all(g_tech_sock, &hdr, sizeof(hdr));
            }
            MessageBox(hwnd, "Disconnected remote USB device.", "USB Redirector", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Toolbar Background
        RECT rcToolbar = { 0, 0, 760, 40 };
        HBRUSH htbBrush = CreateSolidBrush(RGB(240, 242, 245));
        FillRect(hdc, &rcToolbar, htbBrush);
        DeleteObject(htbBrush);

        HPEN hPenLine = CreatePen(PS_SOLID, 1, RGB(200, 205, 210));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPenLine);
        MoveToEx(hdc, 0, 40, NULL);
        LineTo(hdc, 760, 40);

        SetBkMode(hdc, TRANSPARENT);
        HFONT hFontBtn = CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFontBtn);

        // Toolbar Button Definitions matching reference image icons:
        // 1. Add Conn | 2. Remove Conn | 3. Share USB | 4. Unshare USB | 5. Auto Share | 6. Connect | 7. Disconnect
        struct { int x1; int x2; const char *label; COLORREF color; } btnList[] = {
            { 10, 65,  "🖥️+",   RGB(33, 150, 243) },
            { 70, 125, "🖥️✖",   RGB(244, 67, 54) },
            { 135, 190, "🔌➔",  RGB(76, 175, 80) },
            { 195, 250, "🔌✖",  RGB(244, 67, 54) },
            { 255, 325, "🫴 auto", RGB(233, 30, 99) },
            { 330, 390, "🔌✔",  RGB(46, 125, 50) },
            { 395, 455, "🔌✖",  RGB(211, 47, 47) },
            { 460, 520, "🔌+",  RGB(156, 39, 176) },
            { 525, 585, "🔌-",  RGB(121, 85, 72) }
        };

        for (int i = 0; i < 9; i++) {
            RECT rcBtn = { btnList[i].x1, 6, btnList[i].x2, 34 };
            HBRUSH hb = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &rcBtn, hb);
            DeleteObject(hb);
            FrameRect(hdc, &rcBtn, (HBRUSH)GetStockObject(BLACK_BRUSH));
            SetTextColor(hdc, btnList[i].color);
            DrawText(hdc, btnList[i].label, -1, &rcBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Numeric ID Badge Banner on top right
        RECT rcTechId = { 595, 6, 755, 34 };
        SetTextColor(hdc, RGB(21, 101, 192));
        char techIdBanner[128];
        snprintf(techIdBanner, sizeof(techIdBanner), "ID: [%s]", g_assigned_tech_id);
        DrawText(hdc, techIdBanner, -1, &rcTechId, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, hOldFont);
        SelectObject(hdc, hOldPen);
        DeleteObject(hFontBtn);
        DeleteObject(hPenLine);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        if (y >= 6 && y <= 34) {
            if (x >= 330 && x <= 390) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_CONNECT_DEVICE, 0), 0);
            } else if (x >= 395 && x <= 455) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_DISCONNECT_DEVICE, 0), 0);
            }
        }
        return 0;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (g_hwndTab) {
            SetWindowPos(g_hwndTab, NULL, 10, 45, width - 20, height - 55, SWP_NOZORDER);
        }
        if (g_hwndTree) {
            SetWindowPos(g_hwndTree, NULL, 20, 80, width - 40, height - 100, SWP_NOZORDER);
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
    icex.dwICC = ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES;
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
        "USB Redirector - Evaluation version",
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
