#pragma once
#include <windows.h>

#define WM_UAHDESTROY           0x0090
#define WM_UAHDRAWMENU          0x0091
#define WM_UAHDRAWMENUITEM      0x0092
#define WM_UAHMEASUREMENUITEM   0x0093

typedef struct tagUAHMENU {
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
} UAHMENU;

typedef struct tagUAHDRAWMENUITEM {
    DRAWITEMSTRUCT dis;
    UAHMENU um;
    struct {
        int iPosition;
        int iItemID;
        UINT uItemState;
    } umi;
} UAHDRAWMENUITEM;

typedef struct tagUAHMEASUREMENUITEM {
    MEASUREITEMSTRUCT mis;
    UAHMENU um;
    struct {
        int iPosition;
        int iItemID;
        UINT uItemState;
    } umi;
} UAHMEASUREMENUITEM;

bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr);