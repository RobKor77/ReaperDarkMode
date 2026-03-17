#include "framework.h"
#include "UAHMenuBar.h"
#include <Uxtheme.h>
#include <vsstyle.h>
#include <string>
#include <shlwapi.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shlwapi.lib")

static HTHEME g_menuTheme = nullptr;

// --- MENU COLORS ---
COLORREF g_menuBgColor = RGB(22, 22, 22);
COLORREF g_menuHoverColor = RGB(62, 62, 62);
COLORREF g_menuTextColor = RGB(220, 220, 220);
COLORREF g_menuTextDisabledColor = RGB(120, 120, 120);

static HBRUSH g_brBarBackground = NULL;
static HBRUSH g_hB = NULL; // Normal button background
static HBRUSH g_hH = NULL; // Hover button background
bool g_menuConfigLoaded = false;

// INI helper
static COLORREF ParseMenuColor(const std::wstring& colorStr, COLORREF defaultColor) {
    int r, g, b;
    if (swscanf_s(colorStr.c_str(), L"%d, %d, %d", &r, &g, &b) == 3) return RGB(r, g, b);
    if (swscanf_s(colorStr.c_str(), L"%d,%d,%d", &r, &g, &b) == 3) return RGB(r, g, b);
    return defaultColor;
}

static void WriteMenuColorToIni(const wchar_t* iniPath, const wchar_t* key, COLORREF color) {
    wchar_t colorStr[32];
    swprintf_s(colorStr, L"%d, %d, %d", GetRValue(color), GetGValue(color), GetBValue(color));
    WritePrivateProfileStringW(L"Colors", key, colorStr, iniPath);
}

static COLORREF ReadMenuColorFromIni(const wchar_t* iniPath, const wchar_t* key, COLORREF defaultColor) {
    wchar_t buffer[64];
    GetPrivateProfileStringW(L"Colors", key, L"", buffer, 64, iniPath);
    if (wcslen(buffer) > 0) return ParseMenuColor(buffer, defaultColor);

    // If the key does not exist, write it to the file
    WriteMenuColorToIni(iniPath, key, defaultColor);
    return defaultColor;
}

// Function that reads or initializes INI colors upon the first menu paint
static void LoadMenuConfig() {
    if (g_menuConfigLoaded) return; // Prevent loading if already loaded

    wchar_t dllPath[MAX_PATH];
    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&LoadMenuConfig, &hModule);
    GetModuleFileNameW(hModule, dllPath, MAX_PATH);
    PathRemoveFileSpecW(dllPath);

    // Check buffer overflow
    if (wcslen(dllPath) + 25 > MAX_PATH) {
        return;
    }
    std::wstring iniPath = std::wstring(dllPath) + L"\\reaper_darkmode.ini";


    g_menuBgColor = ReadMenuColorFromIni(iniPath.c_str(), L"MenuBarBackground", g_menuBgColor);
    g_menuHoverColor = ReadMenuColorFromIni(iniPath.c_str(), L"MenuBarHover", g_menuHoverColor);
    g_menuTextColor = ReadMenuColorFromIni(iniPath.c_str(), L"MenuTextColor", g_menuTextColor);
    g_menuTextDisabledColor = ReadMenuColorFromIni(iniPath.c_str(), L"MenuTextDisabled", g_menuTextDisabledColor);

    // Delete old brushes (prevent memory leak)
    if (g_brBarBackground) DeleteObject(g_brBarBackground);
    if (g_hB) DeleteObject(g_hB);
    if (g_hH) DeleteObject(g_hH);

    // 3. Create new brushes with new colors
    g_brBarBackground = CreateSolidBrush(g_menuBgColor);
    g_hB = CreateSolidBrush(g_menuBgColor);
    g_hH = CreateSolidBrush(g_menuHoverColor);
    // Ensure brushes were successfully created
    if (!g_brBarBackground || !g_hB || !g_hH) {
        return; // Err creating brushes
    }

    g_menuConfigLoaded = true; // Mark configuration as loaded
}

bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr) {
    if (!g_menuConfigLoaded) LoadMenuConfig(); // Load only once

    switch (message) {
    case WM_UAHDRAWMENU: {
        UAHMENU* pUDM = (UAHMENU*)lParam;
        RECT rc; MENUBARINFO mbi = { sizeof(mbi) };
        GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);
        RECT rcWin; GetWindowRect(hWnd, &rcWin);
        rc = mbi.rcBar; OffsetRect(&rc, -rcWin.left, -rcWin.top);

        // Color menu background
        FillRect(pUDM->hdc, &rc, g_brBarBackground);

        return true;
    }
    case WM_UAHDRAWMENUITEM: {
        UAHDRAWMENUITEM* pI = (UAHDRAWMENUITEM*)lParam;
        if (!pI) return false; // Check null pointer

        HBRUSH* pB = (pI->dis.itemState & ODS_HOTLIGHT) ? &g_hH : &g_hB;

        wchar_t s[256] = { 0 };
        MENUITEMINFO m = { sizeof(m), MIIM_STRING | MIIM_FTYPE | MIIM_BITMAP };
        m.dwTypeData = s; m.cch = 255;
        if (pI->um.hmenu == NULL) return false; // Safety check

        GetMenuItemInfo(pI->um.hmenu, pI->umi.iPosition, TRUE, &m);
        if ((m.fType & MFT_OWNERDRAW) || (m.hbmpItem != NULL)) {
            return false;
        }

        if (!g_menuTheme) {
            g_menuTheme = OpenThemeData(hWnd, L"Menu");
            if (!g_menuTheme) return false; // CRITICAL - don't continue!
        }

        // Dyn. text color
        COLORREF tC = (pI->dis.itemState & ODS_GRAYED) ? g_menuTextDisabledColor : g_menuTextColor;
        DTTOPTS o = { sizeof(o), DTT_TEXTCOLOR, tC };

        FillRect(pI->um.hdc, &pI->dis.rcItem, *pB);
        DrawThemeTextEx(g_menuTheme, pI->um.hdc, MENU_BARITEM, MBI_NORMAL, s, -1, DT_CENTER | DT_SINGLELINE | DT_VCENTER, &pI->dis.rcItem, &o);
        return true;
    }
    case WM_UAHMEASUREMENUITEM:
        return false;
    default:
        return false;
    }
}