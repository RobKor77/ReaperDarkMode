/*
==============================================================================
Project:        REAPER Dark Mode UI

Description:    Provides a full dark mode implementation for Reaper (Windows platform)

Author:         Copyright (c) 2026 Rob Kor (Wormhole Labs)
Created:        2026
Version:        Release v1.0.2

Platform:       Windows 10, Windows 11
Compiler:       MSVC / Visual Studio

Based on:
win32-custom-menubar-aero-theme
https://github.com/jjYBdx4IL/win32-custom-menubar-aero-theme

Notes:
- Uses WinAPI hooks to modify UI behavior.
- Designed for REAPER DAW integration.
- Tested on Windows 11 Pro, Windows 11 Home, Windows 10 Home.

License:
This code is provided as-is without warranty.
You may use, modify, and distribute it freely.
==============================================================================
*/

#include "framework.h"
#include "UAHMenuBar.h"
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <string>
#include <shlwapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib")

// ============ CLASS NAME CONSTANTS ============
static const wchar_t CLASSNAME_DIALOG[] = L"#32770";
static const wchar_t CLASSNAME_BUTTON[] = L"Button";
static const wchar_t CLASSNAME_STATIC[] = L"Static";
static const wchar_t CLASSNAME_TREEVIEW[] = L"SysTreeView32";
static const wchar_t CLASSNAME_LISTVIEW[] = L"SysListView32";
static const wchar_t CLASSNAME_HEADER[] = L"SysHeader32";
static const wchar_t CLASSNAME_TABCTRL[] = L"SysTabControl32";
static const wchar_t CLASSNAME_EDIT[] = L"Edit";
static const wchar_t CLASSNAME_COMBOBOX[] = L"ComboBox";
static const wchar_t CLASSNAME_REBAR[] = L"ReBarWindow32";
static const wchar_t CLASSNAME_TOOLBAR[] = L"ToolbarWindow32";
static const wchar_t CLASSNAME_SCROLLBAR[] = L"ScrollBar";
static const wchar_t CLASSNAME_LISTBOX[] = L"ListBox";
static const wchar_t CLASSNAME_SYSLINK[] = L"SysLink";
static const wchar_t CLASSNAME_TRACKBAR[] = L"msctls_trackbar32";
static const wchar_t CLASSNAME_MENU[] = L"#32768";
static const wchar_t CLASSNAME_COMBOBOXEX[] = L"ComboBoxEx32";
static const wchar_t CLASSNAME_REAPER_PREFIX[] = L"REAPER";
static const wchar_t CLASSNAME_DUI_VIEW[] = L"DUIViewWndClassName";
// ================================================

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

// --- COLOR SETTINGS (Default before INI) ---
static COLORREF g_TitleBarColor = RGB(22, 22, 22);
static COLORREF g_colorMain = RGB(48, 48, 48);
static COLORREF g_colorChild = RGB(32, 32, 32);
static COLORREF g_colorEdit = RGB(35, 35, 35);
static COLORREF g_TextColor = RGB(160, 160, 160);
static COLORREF g_DisabledTextColor = RGB(120, 120, 120);
static COLORREF g_MainWindowBorder = RGB(90, 90, 90);
static COLORREF g_BorderColor = RGB(70, 70, 70);
static COLORREF g_GroupBoxColor = RGB(160, 160, 160);
static COLORREF g_HeaderBackground = RGB(80, 80, 80);
static COLORREF g_HeaderTextColor = RGB(240, 240, 240);
static COLORREF g_TreeSelectionTextColor = RGB(255, 255, 255);
static COLORREF g_TabBackground = RGB(56, 56, 56);
static COLORREF g_TabSelected = RGB(32, 32, 32);
static COLORREF g_SystemWindowsColor = RGB(75, 75, 75);

static HBRUSH g_hbrTabBackground = NULL;
static HBRUSH g_hbrTabSelected = NULL;
static HBRUSH g_hbrSystemWindows = NULL;

// Import from UAHMenuBar
extern COLORREF g_menuBgColor;
extern COLORREF g_menuHoverColor;
extern COLORREF g_menuTextColor;
extern COLORREF g_menuTextDisabledColor;
extern bool g_menuConfigLoaded;

// Text helper
static bool GetMenuTextFromItemStruct(HWND hWnd, UINT itemID, wchar_t* outText, int maxLen) {
    HMENU hMenu = GetMenu(hWnd);
    if (!hMenu) return false;

    int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; i++) {
        MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
        mii.fMask = MIIM_SUBMENU | MIIM_ID;
        GetMenuItemInfoW(hMenu, i, TRUE, &mii);

        if (mii.hSubMenu == (HMENU)(UINT_PTR)itemID || mii.wID == itemID) {
            GetMenuStringW(hMenu, i, outText, maxLen, MF_BYPOSITION);
            return true;
        }
    }
    return false;
}

// Fullscreen Owner-Draw - menu background color
static HBRUSH g_menuBarBgBrush = NULL;

static void UpdateMainMenuOwnerDraw(HWND hWnd) {
    HMENU hMenu = GetMenu(hWnd);
    if (!hMenu) return;

    LONG style = GetWindowLong(hWnd, GWL_STYLE);
    bool isFullscreen = ((style & WS_CAPTION) != WS_CAPTION);

    int count = GetMenuItemCount(hMenu);
    bool changed = false;

    for (int i = 0; i < count; i++) {
        MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
        mii.fMask = MIIM_FTYPE;
        GetMenuItemInfoW(hMenu, i, TRUE, &mii);

        if (isFullscreen) {
            if (!(mii.fType & MFT_OWNERDRAW)) {
                mii.fType |= MFT_OWNERDRAW;
                SetMenuItemInfoW(hMenu, i, TRUE, &mii);
                changed = true;
            }
        }
        else {
            if (mii.fType & MFT_OWNERDRAW) {
                mii.fType &= ~MFT_OWNERDRAW;
                SetMenuItemInfoW(hMenu, i, TRUE, &mii);
                changed = true;
            }
        }
    }

    if (isFullscreen) {
        if (g_menuBarBgBrush) DeleteObject(g_menuBarBgBrush);
        g_menuBarBgBrush = CreateSolidBrush(g_menuBgColor);

        MENUINFO mi = { sizeof(MENUINFO) };
        mi.fMask = MIM_BACKGROUND;
        mi.hbrBack = g_menuBarBgBrush;
        SetMenuInfo(hMenu, &mi);
    }
    else {
        MENUINFO mi = { sizeof(MENUINFO) };
        mi.fMask = MIM_BACKGROUND;
        mi.hbrBack = NULL;
        SetMenuInfo(hMenu, &mi);
    }

    if (changed) DrawMenuBar(hWnd);
}

static HBRUSH g_hbrMainBackground = NULL;
static HBRUSH g_hbrChildBackground = NULL;
static HBRUSH g_hbrEdit = NULL;

static std::wstring g_IniPath = L"";
static FILETIME g_LastIniTime = { 0 };
static UINT_PTR g_IniTimerID = 0;
static HWND g_ReaperMainWindow = NULL;
static bool g_bIsEnabled = true;
static bool g_LastEnabledState = true;

// Enum for windows type  - class caching
enum WndClass {
    WND_UNKNOWN = 0,
    WND_DIALOG,
    WND_BUTTON,
    WND_STATIC,
    WND_TREEVIEW,
    WND_LISTVIEW,
    WND_HEADER,
    WND_TABCTRL,
    WND_EDIT,
    WND_COMBOBOX,
    WND_REBAR,
    WND_TOOLBAR,
    WND_SCROLLBAR,
    WND_LISTBOX,
    WND_SYSLINK,
    WND_TRACKBAR,
    WND_MENU,
    WND_COMBOBOXEX,
    WND_REAPERWINDOW
};
enum class PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
using fnAllowDarkModeForWindow = bool (WINAPI*)(HWND hWnd, bool allow);
using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode);
using fnFlushMenuThemes = void (WINAPI*)();
static fnAllowDarkModeForWindow AllowDarkModeForWindow = nullptr;
static fnSetPreferredAppMode SetPreferredAppMode = nullptr;
static fnFlushMenuThemes FlushMenuThemes = nullptr;
static HHOOK g_hHook = NULL;
static void StyleWindow(HWND hwnd);
static BOOL CALLBACK EnumAllChildren(HWND hwnd, LPARAM lParam);
static BOOL IsChildOfTabControl(HWND hWnd);

static COLORREF GetWindowBgColor(HWND hWnd) {
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root) {
        wchar_t title[256] = { 0 };
        GetWindowTextW(root, title, 256);
        if ((wcsstr(title, L"Save") && wcsstr(title, L"bounce") == NULL) ||
            wcsstr(title, L"Open") || wcsstr(title, L"Import") ||
            wcsstr(title, L"Export") || wcsstr(title, L"Browse") ||
            wcsstr(title, L"Select")) {

            wchar_t cls[256] = { 0 };
            GetClassNameW(hWnd, cls, 256);

            // TreeView and ListView controls (retain native Explorer dark theme)
            if (wcscmp(cls, CLASSNAME_TREEVIEW) == 0 || wcscmp(cls, CLASSNAME_LISTVIEW) == 0) {
                return RGB(25, 25, 25);
            }

            // REAPER checkbox panel (hardcoded color RGB(56,56,56))
            if (wcscmp(cls, CLASSNAME_DIALOG) == 0 && hWnd != root) {
                return RGB(56, 56, 56);
            }

            // Checkbox controls inside panel (hardcoded RGB(56,56,56))
            HWND parent = GetParent(hWnd);
            if (parent != NULL && parent != root) {
                wchar_t pCls[256]; GetClassNameW(parent, pCls, 256);
                if (wcscmp(pCls, CLASSNAME_DIALOG) == 0) return RGB(56, 56, 56);
            }

            // Bottom area (Save/Cancel buttons): controlled by SystemWindowsColor
            return g_SystemWindowsColor;
        }
        return g_colorChild;
    }
    if ((GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD) != 0) return g_colorChild;
    return g_colorMain;
}

static HBRUSH GetWindowBgBrush(HWND hWnd) {
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root) {
        wchar_t title[256] = { 0 };
        GetWindowTextW(root, title, 256);
        if ((wcsstr(title, L"Save") && wcsstr(title, L"bounce") == NULL) ||
            wcsstr(title, L"Open") ||
            wcsstr(title, L"Create new MIDI") ||
            wcsstr(title, L"Render to file") ||//watch out for file-File!
            wcsstr(title, L"Select replacement file") ||
            wcsstr(title, L"Select new filename") ||
            wcsstr(title, L"Import") ||
            wcsstr(title, L"Export")) {

            wchar_t cls[256] = { 0 };
            GetClassNameW(hWnd, cls, 256);

            // Explorer brush
            if (wcscmp(cls, CLASSNAME_TREEVIEW) == 0 || wcscmp(cls, CLASSNAME_LISTVIEW) == 0) {
                static HBRUSH hbrExplorer = CreateSolidBrush(RGB(25, 25, 25));
                return hbrExplorer;
            }

            // HARD-CODED BRUSH (RGB(56,56,56))
            static HBRUSH hbrFixedTest = CreateSolidBrush(RGB(56, 56, 56));

            if (wcscmp(cls, CLASSNAME_DIALOG) == 0 && hWnd != root) return hbrFixedTest;

            HWND parent = GetParent(hWnd);
            if (parent != NULL && parent != root) {
                wchar_t pCls[256]; GetClassNameW(parent, pCls, 256);
                if (wcscmp(pCls, CLASSNAME_DIALOG) == 0) return hbrFixedTest;
            }
            // --------------------------------------

            return g_hbrSystemWindows;
        }
        return g_hbrChildBackground;
    }
    if ((GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD) != 0) return g_hbrChildBackground;
    return g_hbrMainBackground;
}

// Check if we are inside FX window (VST, AU, JS, ...)
static BOOL IsInFXWindow(HWND hWnd) {
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (!root) return FALSE;

    wchar_t rootTitle[256] = { 0 };
    GetWindowTextW(root, rootTitle, 256);

    // If the root window title contains these keywords, it is a plugin
    if (wcsstr(rootTitle, L"VST:") ||
        wcsstr(rootTitle, L"VST3:") ||
        wcsstr(rootTitle, L"JS:") ||
        wcsstr(rootTitle, L"AU:") ||
        wcsstr(rootTitle, L"FX: ")) {
        return TRUE;
    }
    return FALSE;
}

static VOID CALLBACK UncloakTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    KillTimer(hwnd, idEvent);

    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    LONG currentEx = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, currentEx & ~WS_EX_LAYERED);

    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static void InitDarkApi() {
    HMODULE hUxtheme = GetModuleHandle(L"uxtheme.dll");
    if (hUxtheme) {
        AllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133));
        SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
        FlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));
        if (SetPreferredAppMode) SetPreferredAppMode(g_bIsEnabled ? PreferredAppMode::ForceDark : PreferredAppMode::Default);
        if (FlushMenuThemes) FlushMenuThemes();
    }
}

static void DarkenTitleBar(HWND hwnd) {
    if (!IsWindow(hwnd)) return;
    BOOL dark = TRUE;

    // System Dark Mode setting
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));

    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &g_TitleBarColor, sizeof(g_TitleBarColor));

    // 1px window border
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &g_MainWindowBorder, sizeof(g_MainWindowBorder));
}

static BOOL IsChildOfTabControl(HWND hWnd) {
    HWND parent = GetParent(hWnd);
    while (parent) {
        wchar_t cls[256];
        GetClassName(parent, cls, 256);
        if (wcscmp(cls, CLASSNAME_TABCTRL) == 0) return TRUE;
        parent = GetParent(parent);
    }
    return FALSE;
}

// HELPER FUNCTION TO PROTECT CHILD CONTROLS FROM BEING PAINTED OVER
static void ClipChildWindows(HWND hWnd, HDC hdc) {
    HWND hChild = GetWindow(hWnd, GW_CHILD);
    while (hChild) {
        if (IsWindowVisible(hChild)) {
            RECT rc;
            GetWindowRect(hChild, &rc);
            MapWindowPoints(HWND_DESKTOP, hWnd, (LPPOINT)&rc, 2);
            ExcludeClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

// BRUSH RECOVERY: Protection against rogue plugins invalidating global brushes
static void EnsureBrushesAreAlive() {
    LOGBRUSH lb;
    bool systemThemeResetNeeded = false;

    // Restore window brushes
    if (g_hbrMainBackground != NULL && GetObject(g_hbrMainBackground, sizeof(LOGBRUSH), &lb) == 0) {
        g_hbrMainBackground = CreateSolidBrush(g_colorMain);
        systemThemeResetNeeded = true;
    }
    if (g_hbrChildBackground != NULL && GetObject(g_hbrChildBackground, sizeof(LOGBRUSH), &lb) == 0) {
        g_hbrChildBackground = CreateSolidBrush(g_colorChild);
        systemThemeResetNeeded = true;
    }
    if (g_hbrEdit != NULL && GetObject(g_hbrEdit, sizeof(LOGBRUSH), &lb) == 0) {
        g_hbrEdit = CreateSolidBrush(g_colorEdit);
        systemThemeResetNeeded = true;
    }

    // Restore tab brushes (custom brushes)
    if (g_hbrTabBackground != NULL && GetObject(g_hbrTabBackground, sizeof(LOGBRUSH), &lb) == 0) {
        g_hbrTabBackground = CreateSolidBrush(g_TabBackground);
        systemThemeResetNeeded = true;
    }
    if (g_hbrTabSelected != NULL && GetObject(g_hbrTabSelected, sizeof(LOGBRUSH), &lb) == 0) {
        g_hbrTabSelected = CreateSolidBrush(g_TabSelected);
        systemThemeResetNeeded = true;
    }
    if (g_hbrSystemWindows != NULL && GetObject(g_hbrSystemWindows, sizeof(LOGBRUSH), &lb) == 0) {
        g_hbrSystemWindows = CreateSolidBrush(g_SystemWindowsColor);
        systemThemeResetNeeded = true;
    }

    // Restore main menu bar brush
    if (g_menuBarBgBrush != NULL && GetObject(g_menuBarBgBrush, sizeof(LOGBRUSH), &lb) == 0) {
        g_menuBarBgBrush = CreateSolidBrush(g_menuBgColor);
        systemThemeResetNeeded = true;

        // Immediately reassign restored brush to REAPER main menu
        HWND reaperMain = FindWindow(L"REAPERwnd", NULL);
        if (reaperMain) {
            HMENU hMenu = GetMenu(reaperMain);
            if (hMenu) {
                MENUINFO mi = { sizeof(MENUINFO) };
                mi.fMask = MIM_BACKGROUND;
                mi.hbrBack = g_menuBarBgBrush;
                SetMenuInfo(hMenu, &mi);
            }
        }
    }

    // force reapply dark menu mode for rogue plugins stealing (erasing) colors
    if (systemThemeResetNeeded) {
        if (SetPreferredAppMode) SetPreferredAppMode(PreferredAppMode::ForceDark);
        if (FlushMenuThemes) FlushMenuThemes();
    }
}


static LRESULT CALLBACK UniversalSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    LRESULT lr = 0;
    if (UAHWndProc(hWnd, uMsg, wParam, lParam, &lr)) return lr;

    // --- SAFETY GUARD ---
    // Validate brush handles only on paint-related messages
    if (uMsg == WM_PAINT || uMsg == WM_ERASEBKGND || uMsg == WM_CTLCOLORDLG ||
        uMsg == WM_CTLCOLORSTATIC || uMsg == WM_CTLCOLORBTN ||
        uMsg == WM_CTLCOLOREDIT || uMsg == WM_CTLCOLORLISTBOX) {
        EnsureBrushesAreAlive();
    }

    /// --- CUSTOM DRAW for TREEVIEW and LISTVIEW ---
    if (uMsg == WM_NOTIFY) {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->code == NM_CUSTOMDRAW) {
            wchar_t clsName[256];
            GetClassNameW(nmhdr->hwndFrom, clsName, 256);

            if (wcscmp(clsName, CLASSNAME_TREEVIEW) == 0) {
                LPNMTVCUSTOMDRAW ptvcd = (LPNMTVCUSTOMDRAW)lParam;
                if (ptvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                    LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                    return res | CDRF_NOTIFYITEMDRAW;
                }
                else if (ptvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    bool isSelected = (ptvcd->nmcd.uItemState & (CDIS_SELECTED | CDIS_FOCUS | CDIS_DROPHILITED)) != 0;
                    if (isSelected) {
                        ptvcd->clrText = g_TreeSelectionTextColor;
                    }
                    else {
                        ptvcd->clrText = g_TextColor;
                    }
                    return CDRF_NEWFONT;
                }
            }
            else if (wcscmp(clsName, CLASSNAME_LISTVIEW) == 0) {
                LPNMLVCUSTOMDRAW plvcd = (LPNMLVCUSTOMDRAW)lParam;

                if (plvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                    LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                    return res | CDRF_NOTIFYITEMDRAW;
                }
                else if (plvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    bool isSelected = (plvcd->nmcd.uItemState & (CDIS_SELECTED | CDIS_FOCUS | CDIS_DROPHILITED)) != 0;

                    COLORREF bgColor = GetWindowBgColor(nmhdr->hwndFrom);
                    plvcd->clrTextBk = bgColor;

                    if (isSelected) {
                        plvcd->clrText = g_TreeSelectionTextColor;
                    }
                    else {
                        plvcd->clrText = g_TextColor;
                    }

                    return CDRF_NEWFONT;
                }
            }
        }
    }

    wchar_t className[256];
    GetClassName(hWnd, className, 256);

    WndClass classType = (WndClass)dwRefData;

    // --- SAFE SHUTDOWN CLEANUP ---
    if (uMsg == WM_DESTROY && hWnd == g_ReaperMainWindow) {
        if (g_IniTimerID) {
            KillTimer(hWnd, g_IniTimerID);
            g_IniTimerID = 0;
        }
        if (g_hHook) {
            UnhookWindowsHookEx(g_hHook);
            g_hHook = NULL;
        }
    }



    if (classType == WND_BUTTON) {
        if (uMsg == WM_SETTEXT || uMsg == BM_SETCHECK || uMsg == BM_SETSTATE) {
            // CHECKBOX PROTECTION BLOCK
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            // Force full redraw via WM_PAINT invalidation
            InvalidateRect(hWnd, NULL, TRUE);
            UpdateWindow(hWnd);

            return res;
        }
    }

    bool isColorDlg = false;
    bool isPinConnector = false;

    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root) {
        if (GetPropW(root, L"IsColorDlg") != NULL) isColorDlg = true;
        if (GetPropW(root, L"IsPinConnector") != NULL) isPinConnector = true;
    }

    // FULLSCREEN MENU
    // Entering Fullscreen modifies the window style, causing Windows to reset the theme.
    // That is why we immediately refresh and reapply the dark theme upon any size or style change!
    if (uMsg == WM_SIZE || uMsg == WM_STYLECHANGED) {
        if (root && hWnd == root) {
            SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);
            DarkenTitleBar(hWnd);
            UpdateMainMenuOwnerDraw(hWnd);
        }
    }

    if (uMsg == WM_MEASUREITEM) {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        if (mis->CtlType == ODT_MENU) {
            wchar_t text[256] = { 0 };
            if (GetMenuTextFromItemStruct(hWnd, mis->itemID, text, 256)) {
                HDC hdc = GetDC(hWnd);
                NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
                SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
                HFONT hSysFont = CreateFontIndirectW(&ncm.lfMenuFont);
                HFONT hOldFont = (HFONT)SelectObject(hdc, hSysFont);

                SIZE size;
                GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &size);

                SelectObject(hdc, hOldFont);
                DeleteObject(hSysFont);
                ReleaseDC(hWnd, hdc);

                mis->itemWidth = size.cx + 5; // padding modification
                mis->itemHeight = size.cy + 6;
                return TRUE;
            }
        }
    }

    // Buttons in menu bar - draw
    if (uMsg == WM_DRAWITEM) {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlType == ODT_MENU) {
            wchar_t text[256] = { 0 };
            if (GetMenuTextFromItemStruct(hWnd, dis->itemID, text, 256)) {
                bool isSelected = (dis->itemState & ODS_SELECTED) || (dis->itemState & ODS_HOTLIGHT);

                COLORREF bgColor = isSelected ? g_menuHoverColor : g_menuBgColor;
                COLORREF txtColor = (dis->itemState & ODS_GRAYED) ? g_menuTextDisabledColor : g_menuTextColor;

                HBRUSH hbrBg = CreateSolidBrush(bgColor);
                FillRect(dis->hDC, &dis->rcItem, hbrBg);
                DeleteObject(hbrBg);

                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, txtColor);

                NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
                SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
                HFONT hSysFont = CreateFontIndirectW(&ncm.lfMenuFont);
                HFONT hOldFont = (HFONT)SelectObject(dis->hDC, hSysFont);

                DrawTextW(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SelectObject(dis->hDC, hOldFont);
                DeleteObject(hSysFont);
                return TRUE;
            }
        }
    }

    // 1. TEST FOR COLOR PICKER
    if (uMsg == WM_INITDIALOG || uMsg == WM_SHOWWINDOW) {
        if (wcscmp(className, CLASSNAME_DIALOG) == 0) {
            wchar_t title[256] = { 0 };
            GetWindowTextW(hWnd, title, 256);

            // Structural detection heuristic for Color Picker and similar dialogs
            bool isColorStruct = (GetDlgItem(hWnd, 712) != NULL);
            if (isColorStruct ||
                wcsstr(title, L"Notes") || wcsstr(title, L"Screenset") ||
                wcsstr(title, L"Customize menus") || wcsstr(title, L"Customise menus") ||
                wcsstr(title, L"toolbar icon") || wcsstr(title, L"Toolbar Icon") ||
                wcsstr(title, L"Icon")) {

                SetPropW(hWnd, L"IsColorDlg", (HANDLE)1);
                isColorDlg = true;
            }

            if (wcsstr(title, L"pin connector") || wcsstr(title, L"Pin connector") || wcsstr(title, L"Plug-in pin")) {
                SetPropW(hWnd, L"IsPinConnector", (HANDLE)1);
                isPinConnector = true;
            }
        }
    }

    if (uMsg == WM_SHOWWINDOW && wParam == TRUE) {
        if (wcscmp(className, CLASSNAME_DIALOG) == 0) {
            if (!isColorDlg && !isPinConnector) {
                EnableThemeDialogTexture(hWnd, ETDT_DISABLE);
            }
        }

        HWND root = GetAncestor(hWnd, GA_ROOT);
        if (hWnd == root) {

            int curtainDelay = 1;// global minimal delay to prevent system dialog flicker

            wchar_t rootTitle[256] = { 0 };
            GetWindowTextW(hWnd, rootTitle, 256);

            if (wcsstr(rootTitle, L"FX:") != NULL ||
                wcsstr(rootTitle, L"Chain") != NULL ||
                wcsstr(rootTitle, L"Container") != NULL ||
                wcsstr(rootTitle, L"VST:") != NULL ||
                wcsstr(rootTitle, L"VST3:") != NULL ||
                wcsstr(rootTitle, L"AU:") != NULL ||
                wcsstr(rootTitle, L"JS:") != NULL ||
                wcsstr(rootTitle, L"CLAP:") != NULL) {

                curtainDelay = 0; // no LAG for these windows
            }

            if (curtainDelay > 0) {
                LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
                if ((exStyle & WS_EX_LAYERED) == 0) {
                    SetWindowLong(hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
                    SetLayeredWindowAttributes(hWnd, 0, 0, LWA_ALPHA);

                    SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                }

                SetTimer(hWnd, 8888, curtainDelay, UncloakTimerProc);
            }
        }

        // Prevent a full refresh of all elements when the MIDI editor is displayed.
        // NOTE: Envelope tables are themable via Theme Tweaker
        // Exception for "Envelopes", so the window doesn't stay white if the take (Take) has "MIDI" in its name!
        wchar_t wndTitle[256] = { 0 };
        GetWindowTextW(hWnd, wndTitle, 256);

        bool hasMidi = (wcsstr(wndTitle, L"MIDI") != NULL);
        bool isEnvelopesWindow = (wcsstr(wndTitle, L"Envelopes") != NULL);

        //Refresh elements if THIS IS NOT a Midi window, OR if it is specifically the Envelopes window
        if (!hasMidi || isEnvelopesWindow) {
            EnumChildWindows(hWnd, EnumAllChildren, 0);
        }
    }

    if (isPinConnector) {
        if (uMsg == WM_NCDESTROY) {
            RemoveWindowSubclass(hWnd, UniversalSubclassProc, uIdSubclass);
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_ENABLE) {
        if (wcscmp(className, CLASSNAME_STATIC) == 0 || wcscmp(className, CLASSNAME_BUTTON) == 0) {
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
    if (uMsg == WM_CTLCOLORSTATIC) {
        HDC hdc = (HDC)wParam;

        // Return transparent to prevent drawing ugly rectangles behind the text.
        SetTextColor(hdc, g_TextColor);
        SetBkMode(hdc, TRANSPARENT);

        // Check if we are in an inner or main window and return the appropriate brush
        HBRUSH bgBrush = GetWindowBgBrush(hWnd);
        return (LRESULT)bgBrush;
    }
    if (uMsg == WM_ERASEBKGND) {
        HWND root = GetAncestor(hWnd, GA_ROOT);

        // PROTECTION: If we are painting IN the Color Picker, DO NOT ERASE THE BACKGROUND!
        // Exit if "Root" is flagged, regardless of which internal window we are in.
        if (root && GetPropW(root, L"IsColorDlg") != NULL) {
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        // PROTECTION: STRUCTURAL TOOLBAR CHECK
        if (root && classType == WND_DIALOG) {
            bool hasToolbar = (FindWindowExW(hWnd, NULL, CLASSNAME_TOOLBAR, NULL) != NULL);
            bool noControls = (FindWindowExW(hWnd, NULL, CLASSNAME_COMBOBOX, NULL) == NULL &&
                FindWindowExW(hWnd, NULL, CLASSNAME_BUTTON, NULL) == NULL);

            bool isPureToolbarWindow = hasToolbar && noControls;

            if (isPureToolbarWindow) {
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }
        }
    }

    if (uMsg == WM_PAINT) {
        HWND rootTemp = GetAncestor(hWnd, GA_ROOT);

        // PROTECTION: If we are in the Color Picker, the OS MUST draw the squares!
        // Ignore all elements except lists.
        if (rootTemp && GetPropW(rootTemp, L"IsColorDlg") != NULL) {
            if (classType != WND_TREEVIEW &&
                classType != WND_LISTVIEW &&
                classType != WND_LISTBOX &&
                classType != WND_HEADER) {
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }
        }

        // PROTECTION: TOOLBAR CHECK AND WM_PAINT (Must be Here!)
        if (rootTemp && classType == WND_DIALOG) {
            bool hasToolbar = (FindWindowExW(hWnd, NULL, CLASSNAME_TOOLBAR, NULL) != NULL);
            bool noControls = (FindWindowExW(hWnd, NULL, CLASSNAME_COMBOBOX, NULL) == NULL &&
                FindWindowExW(hWnd, NULL, CLASSNAME_BUTTON, NULL) == NULL);

            if (hasToolbar || noControls) {
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }
        }
        // For TABLES AND TREES (Envelopes window): REAPER frequently resets table colors during operation.
        // We check and force them back to dark ONLY if they have changed to white.
        if (classType == WND_TREEVIEW) {
            COLORREF bgColor = GetWindowBgColor(hWnd);
            COLORREF curBk = (COLORREF)SendMessage(hWnd, 4383, 0, 0); // TVM_GETBKCOLOR
            if (curBk != bgColor) {
                SendMessage(hWnd, 4381, 0, (LPARAM)bgColor); // TVM_SETBKCOLOR
                SendMessage(hWnd, 4382, 0, (LPARAM)g_TextColor); // TVM_SETTEXTCOLOR
            }
        }
        else if (classType == WND_LISTVIEW) {
            static int paintCountList = 0;
            COLORREF bgColor = GetWindowBgColor(hWnd);
            COLORREF curBk = (COLORREF)SendMessage(hWnd, 4096, 0, 0); // LVM_GETBKCOLOR

            if (curBk != bgColor) {
                SendMessage(hWnd, 4097, 0, (LPARAM)bgColor); // LVM_SETBKCOLOR
                SendMessage(hWnd, 4134, 0, (LPARAM)bgColor); // LVM_SETTEXTBKCOLOR
                SendMessage(hWnd, 4132, 0, (LPARAM)g_TextColor); // LVM_SETTEXTCOLOR

                // Trigger refresh every 10 calls to prevent infinite repaint loop
                if ((paintCountList++ % 10) == 0) {
                    InvalidateRect(hWnd, NULL, TRUE);
                }
            }
        }

        // STRUCTURAL PROTECTION FOR ALL TOOLBARS (FLOATING AND DOCKED)
        if (root && classType == WND_DIALOG) {
            //if (root && wcscmp(className, CLASSNAME_DIALOG) == 0) {

            bool hasToolbar = (FindWindowExW(hWnd, NULL, CLASSNAME_TOOLBAR, NULL) != NULL);
            bool noControls = (FindWindowExW(hWnd, NULL, CLASSNAME_COMBOBOX, NULL) == NULL &&
                FindWindowExW(hWnd, NULL, CLASSNAME_BUTTON, NULL) == NULL);

            bool isPureToolbarOnlyWindow = hasToolbar && noControls;   // pure toolbar-only windows (e.g., Media Explorer, Actions)

            if (isPureToolbarOnlyWindow) {
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }
        }

        if (classType == WND_HEADER) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            HBRUSH bgBrush = CreateSolidBrush(g_HeaderBackground);
            FillRect(hdc, &rcClient, bgBrush);
            DeleteObject(bgBrush);

            int count = (int)SendMessageW(hWnd, HDM_GETITEMCOUNT, 0, 0);
            HFONT hFont = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            SetTextColor(hdc, g_HeaderTextColor);
            SetBkMode(hdc, TRANSPARENT);

            HPEN hPen = CreatePen(PS_SOLID, 1, g_BorderColor);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

            for (int i = 0; i < count; i++) {
                RECT rcItem;
                SendMessageW(hWnd, HDM_GETITEMRECT, i, (LPARAM)&rcItem);

                wchar_t text[256] = { 0 };
                HDITEMW hdi = { 0 };
                hdi.mask = HDI_TEXT;
                hdi.pszText = text;
                hdi.cchTextMax = 256;
                SendMessageW(hWnd, HDM_GETITEMW, i, (LPARAM)&hdi);

                RECT rcText = rcItem;
                rcText.left += 6;
                DrawTextW(hdc, text, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

                MoveToEx(hdc, rcItem.right - 1, rcItem.top + 3, NULL);
                LineTo(hdc, rcItem.right - 1, rcItem.bottom - 3);
            }

            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
            SelectObject(hdc, hOldFont);
            EndPaint(hWnd, &ps);
            return 0;
        }

        if (classType == WND_STATIC && !IsWindowEnabled(hWnd)) {
            wchar_t text[256] = { 0 };
            GetWindowTextW(hWnd, text, 256);

            if ((int)wcslen(text) > 0) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hWnd, &ps);
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);

                HBRUSH bgBrush = GetWindowBgBrush(hWnd);
                FillRect(hdc, &rcClient, bgBrush);

                HFONT hFont = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
                HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

                // Render text once with flat gray color (no emboss effect)
                SetTextColor(hdc, g_DisabledTextColor);
                SetBkMode(hdc, TRANSPARENT);

                // Dynamic alignment to prevent text shifting
                DWORD style = GetWindowLong(hWnd, GWL_STYLE);
                UINT textFormat = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;

                if ((style & SS_TYPEMASK) == SS_RIGHT) textFormat |= DT_RIGHT;
                else if ((style & SS_TYPEMASK) == SS_CENTER) textFormat |= DT_CENTER;
                else textFormat |= DT_LEFT;

                if (textFormat & DT_RIGHT) rcClient.right -= 2;
                else rcClient.left += 2;

                DrawTextW(hdc, text, -1, &rcClient, textFormat);

                SelectObject(hdc, hOldFont);
                EndPaint(hWnd, &ps);
                return 0;
            }
        }

        if (classType == WND_BUTTON) {
            DWORD style = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK;

            // Handle DPI-aware checkbox and radio button rendering
            if (style == BS_CHECKBOX || style == BS_AUTOCHECKBOX ||
                style == BS_RADIOBUTTON || style == BS_AUTORADIOBUTTON ||
                style == BS_3STATE || style == BS_AUTO3STATE) {

                wchar_t text[256] = { 0 };
                GetWindowTextW(hWnd, text, 256);

                if ((int)wcslen(text) > 0) {
                    // 1. Let the OS draw the native control (glyph + native text)
                    LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);

                    HDC hdc = GetDC(hWnd);
                    RECT rc; GetClientRect(hWnd, &rc);

                    // 2. Calculate accurate DPI scale factor
                    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
                    if (dpiX == 0) dpiX = 96; // Fallback to 100%
                    float scale = (float)dpiX / 96.0f;

                    // 3. THE SWEET SPOT (Pixel-perfect cut)
                    // At 100%, the box ends ~14px, native text starts ~16px.
                    // We slice exactly at 15px and scale it perfectly.
                    int clearOffset = (int)(15.0f * scale);
                    int textOffset = (int)(19.0f * scale);

                    // 4. Erase the native text exactly between the box and the text
                    RECT rcClear = rc;
                    rcClear.left += clearOffset;
                    HBRUSH bg = GetWindowBgBrush(hWnd);
                    FillRect(hdc, &rcClear, bg);

                    // 5. Draw the custom text
                    HFONT hFont = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

                    bool isDisabled = (!IsWindowEnabled(hWnd) || GetPropW(hWnd, L"FakeDisabled") != NULL);
                    SetTextColor(hdc, isDisabled ? g_DisabledTextColor : g_TextColor);
                    SetBkMode(hdc, TRANSPARENT);

                    RECT rcText = rc;
                    rcText.left += textOffset;

                    DrawTextW(hdc, text, -1, &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

                    SelectObject(hdc, hOldFont);
                    ReleaseDC(hWnd, hdc);
                    return result;
                }
            }
            // GROUPBOX frame rendering (native style)
            else if (style == BS_GROUPBOX) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hWnd, &ps);
                RECT rc; GetClientRect(hWnd, &rc);
                LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                wchar_t text[256] = { 0 };
                GetWindowTextW(hWnd, text, 256);
                HFONT hFont = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
                HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                SIZE textSize;
                GetTextExtentPointW(hdc, text, (int)wcslen(text), &textSize);
                SetTextColor(hdc, g_GroupBoxColor);
                SetBkMode(hdc, TRANSPARENT);
                RECT rcText = rc; rcText.left += 8; rcText.top += 2;
                DrawTextW(hdc, text, -1, &rcText, DT_SINGLELINE | DT_TOP | DT_LEFT);
                HPEN hPen = CreatePen(PS_SOLID, 1, g_BorderColor);
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                int borderY = rc.top + 8;
                MoveToEx(hdc, rc.left + 2, borderY, NULL);
                LineTo(hdc, rc.left + 6, borderY);
                MoveToEx(hdc, rc.left + 6 + textSize.cx + 6, borderY, NULL);
                LineTo(hdc, rc.right - 2, borderY);
                LineTo(hdc, rc.right - 2, rc.bottom - 2);
                LineTo(hdc, rc.left + 2, rc.bottom - 2);
                LineTo(hdc, rc.left + 2, borderY);
                SelectObject(hdc, hOldPen); SelectObject(hdc, hOldFont);
                DeleteObject(hPen);
                EndPaint(hWnd, &ps);
                return 0;
            }
        }

        if (classType == WND_TABCTRL) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            FillRect(hdc, &rcClient, g_hbrTabBackground);

            int count = (int)SendMessageW(hWnd, TCM_GETITEMCOUNT, 0, 0);
            int curSel = (int)SendMessageW(hWnd, TCM_GETCURSEL, 0, 0);
            HFONT hFont = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            SetBkMode(hdc, TRANSPARENT);
            for (int i = 0; i < count; i++) {
                RECT rcItem;
                SendMessageW(hWnd, TCM_GETITEMRECT, i, (LPARAM)&rcItem);
                wchar_t text[256] = { 0 };
                TCITEMW tci = { 0 };
                tci.mask = TCIF_TEXT;
                tci.pszText = text;
                tci.cchTextMax = 256;
                SendMessageW(hWnd, TCM_GETITEMW, i, (LPARAM)&tci);
                bool isSelected = (i == curSel);

                HBRUSH bgBrush = isSelected ? g_hbrTabSelected : g_hbrTabBackground;

                COLORREF txtColor = isSelected ? RGB(255, 255, 255) : RGB(140, 140, 140);
                FillRect(hdc, &rcItem, bgBrush);
                SetTextColor(hdc, txtColor);
                DrawTextW(hdc, text, -1, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            SelectObject(hdc, hOldFont);
            EndPaint(hWnd, &ps);
            return 0;
        }
        else if (classType == WND_DIALOG || IsChildOfTabControl(hWnd)) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            HBRUSH bgBrush = GetWindowBgBrush(hWnd);
            FillRect(hdc, &ps.rcPaint, bgBrush);
            EndPaint(hWnd, &ps);
            return 0;
        }
    }

    if (uMsg == WM_CTLCOLORDLG || uMsg == WM_CTLCOLORMSGBOX) {
        HWND hDlg = (HWND)lParam;
        // LET THE DIALOG BACKGROUND BE DARK
        return (LRESULT)(GetWindowBgBrush(hDlg));
    }
    else if (uMsg == WM_CTLCOLORSTATIC || uMsg == WM_CTLCOLORBTN) {

        // KEEP COLOR SWATCHES NATIVE IN COLOR PICKER! (Prevents them from turning gray)
        //if (isColorDlg) return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (isColorDlg && uMsg == WM_CTLCOLORSTATIC) return DefSubclassProc(hWnd, uMsg, wParam, lParam);

        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        COLORREF bgColor = GetWindowBgColor(hWnd);
        HBRUSH bgBrush = GetWindowBgBrush(hWnd);

        wchar_t childClass[256];
        GetClassName(hCtrl, childClass, 256);

        DWORD btnStyle = GetWindowLong(hCtrl, GWL_STYLE) & BS_TYPEMASK;
        bool isStandardButton = (wcscmp(childClass, L"Button") == 0 &&
            (btnStyle == BS_PUSHBUTTON || btnStyle == BS_DEFPUSHBUTTON));

        // Protection against embossing only for genuine Static text and checkboxes/radio buttons!
        // Do not interfere with manual painting of standard buttons (e.g., ReaInsert).
        if ((!IsWindowEnabled(hCtrl) || GetProp(hCtrl, L"FakeDisabled")) && !isStandardButton) {
            SetTextColor(hdc, g_DisabledTextColor);
            SetBkColor(hdc, bgColor);
            SetBkMode(hdc, OPAQUE);
            return (LRESULT)bgBrush;
        }

        if (wcscmp(childClass, L"SysLink") == 0) {
            SetTextColor(hdc, RGB(100, 180, 255));
        }
        // Do not force white text if it's a standard button (e.g., ReaInsert).
        else if (!isStandardButton) {
            SetTextColor(hdc, g_TextColor);
        }

        SetBkColor(hdc, bgColor);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)bgBrush;
    }
    else if (uMsg == WM_CTLCOLORLISTBOX) {
        // For lists in Reaper (Media Explorer, Actions, etc.):
        HDC hdc = (HDC)wParam;
        COLORREF bgColor = GetWindowBgColor(hWnd);
        HBRUSH bgBrush = GetWindowBgBrush(hWnd);
        SetTextColor(hdc, g_TextColor);
        SetBkColor(hdc, bgColor);
        SetBkMode(hdc, OPAQUE);
        return (LRESULT)bgBrush;
    }
    else if (uMsg == WM_CTLCOLOREDIT) {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, g_TextColor);
        SetBkColor(hdc, g_colorEdit);
        SetBkMode(hdc, OPAQUE);
        return (LRESULT)g_hbrEdit;
    }
    if (uMsg == WM_NCDESTROY) {
        RemovePropW(hWnd, L"FakeDisabled");
        RemovePropW(hWnd, L"IsColorDlg");
        RemovePropW(hWnd, L"IsPinConnector");
        RemoveWindowSubclass(hWnd, UniversalSubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
static WndClass GetWindowClassType(const wchar_t* className) {
    if (wcscmp(className, CLASSNAME_DIALOG) == 0) return WND_DIALOG;
    if (wcscmp(className, CLASSNAME_BUTTON) == 0) return WND_BUTTON;
    if (wcscmp(className, CLASSNAME_STATIC) == 0) return WND_STATIC;
    if (wcscmp(className, CLASSNAME_TREEVIEW) == 0) return WND_TREEVIEW;
    if (wcscmp(className, CLASSNAME_LISTVIEW) == 0) return WND_LISTVIEW;
    if (wcscmp(className, CLASSNAME_HEADER) == 0) return WND_HEADER;
    if (wcscmp(className, CLASSNAME_TABCTRL) == 0) return WND_TABCTRL;
    if (wcscmp(className, CLASSNAME_EDIT) == 0) return WND_EDIT;
    if (wcscmp(className, CLASSNAME_COMBOBOX) == 0) return WND_COMBOBOX;
    if (wcscmp(className, CLASSNAME_REBAR) == 0) return WND_REBAR;
    if (wcscmp(className, CLASSNAME_TOOLBAR) == 0) return WND_TOOLBAR;
    if (wcscmp(className, CLASSNAME_SCROLLBAR) == 0) return WND_SCROLLBAR;
    if (wcscmp(className, CLASSNAME_LISTBOX) == 0) return WND_LISTBOX;
    if (wcscmp(className, CLASSNAME_SYSLINK) == 0) return WND_SYSLINK;
    if (wcscmp(className, CLASSNAME_TRACKBAR) == 0) return WND_TRACKBAR;
    if (wcscmp(className, CLASSNAME_MENU) == 0) return WND_MENU;
    if (wcscmp(className, CLASSNAME_COMBOBOXEX) == 0) return WND_COMBOBOXEX;
    if (wcsncmp(className, CLASSNAME_REAPER_PREFIX, 6) == 0) return WND_REAPERWINDOW;
    return WND_UNKNOWN;
}

static void StyleWindow(HWND hwnd) {
    wchar_t className[256] = { 0 };
    if (!GetClassName(hwnd, className, 256)) return;


    // --- IGNORE MENUS ---
    if (wcscmp(className, CLASSNAME_MENU) == 0) return;

    HWND root = GetAncestor(hwnd, GA_ROOT);

    // --- SMART DETECTION "SAVE/OPEN" WND ---
    bool isFileDialog = false;
    if (root) {
        if (GetPropW(root, L"IsFileDialog") != NULL) {
            isFileDialog = true;
        }
        else {
            if (FindWindowExW(root, NULL, CLASSNAME_DUI_VIEW, NULL) != NULL) {
                SetPropW(root, L"IsFileDialog", (HANDLE)1);
                isFileDialog = true;
            }
        }
    }

    if (isFileDialog) {
        // 1. Root window and the main title bar remain Native to avoid breaking the gradient!
        if (hwnd == root) {
            if (AllowDarkModeForWindow) AllowDarkModeForWindow(hwnd, true);
            DarkenTitleBar(hwnd);
            return;
        }

        // The only part of the window we subclass is the REAPER panel with checkboxes!
        if (wcscmp(className, CLASSNAME_DIALOG) == 0) {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
            SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
            return;
        }

        // First check if buttons and labels belong to the REAPER panel.
        bool isReaperPanelElement = false;
        HWND parent = GetParent(hwnd);
        while (parent && parent != root) {
            wchar_t pClass[256] = { 0 };
            GetClassName(parent, pClass, 256);
            if (wcscmp(pClass, CLASSNAME_DIALOG) == 0) {
                isReaperPanelElement = true;
                break;
            }
            parent = GetParent(parent);
        }

        // If it's not a Reaper button, but a native Windows window element (e.g., Hide Folders), skip it!
        if (!isReaperPanelElement) {
            return;
        }
    }

    // --- DARKEN TITLEBAR ---
    if (root && hwnd == root) {
        DarkenTitleBar(hwnd);

        // UNIVERSAL FIX FOR ALL MENUS (Including Media Explorer):
        // If Windows reports that this window has a system menu (GetMenu), 
        // we must subclass it to trigger UAHMenuBar for the dark theme!
        if (GetMenu(hwnd) != NULL) {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
            SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
            UpdateMainMenuOwnerDraw(hwnd);
        }

        // Additional protection for REAPER-specific windows
        wchar_t title[256] = { 0 };
        GetWindowTextW(hwnd, title, 256);
        if (wcsstr(title, L"MIDI") != NULL || wcsstr(title, L"Media Explorer") != NULL || wcsstr(title, L"Media explorer") != NULL) {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
            SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
        }
    }

    bool isPin = false;
    if (root) {
        wchar_t rootTitle[256] = { 0 };
        GetWindowTextW(root, rootTitle, 256);
        if (wcsstr(rootTitle, L"pin connector") || wcsstr(rootTitle, L"Pin connector") || wcsstr(rootTitle, L"Plug-in pin")) {
            SetPropW(root, L"IsPinConnector", (HANDLE)1);
            isPin = true;
        }
        else if (GetPropW(root, L"IsPinConnector") != NULL) {
            isPin = true;
        }
    }

    if (isPin) {
        return;
    }

    if (GetPropW(hwnd, L"IsColorDlg") != NULL) {
        SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
    }

    if (AllowDarkModeForWindow) {
        AllowDarkModeForWindow(hwnd, true);
    }

    // Capture "REAPERwnd" as well as floating windows ("REAPERdocker")
    if (wcsncmp(className, CLASSNAME_REAPER_PREFIX, 6) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
    }
    else if (wcscmp(className, CLASSNAME_DIALOG) == 0) {
        if (GetPropW(hwnd, L"IsColorDlg") == NULL) {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        }
        SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
    }
    else if (wcscmp(className, CLASSNAME_BUTTON) == 0) {
        DWORD style = GetWindowLong(hwnd, GWL_STYLE) & BS_TYPEMASK;
        if (style == BS_GROUPBOX) {
            HWND rootWindow = GetAncestor(hwnd, GA_ROOT);
            wchar_t windowTitle[256] = { 0 };
            GetWindowTextW(rootWindow, windowTitle, 256);

            if (wcsstr(windowTitle, L"Preferences") ||
                wcsstr(windowTitle, L"Audio") ||
                wcsstr(windowTitle, L"Device") ||
                wcsstr(windowTitle, L"Settings")) {
                return;
            }
            SetWindowTheme(hwnd, L"", L"");
            SetWindowSubclass(hwnd, UniversalSubclassProc, 2, (DWORD_PTR)GetWindowClassType(className));
        }
        else if (style == BS_RADIOBUTTON || style == BS_AUTORADIOBUTTON ||
            style == BS_CHECKBOX || style == BS_AUTOCHECKBOX ||
            style == BS_3STATE || style == BS_AUTO3STATE) {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
            SetWindowSubclass(hwnd, UniversalSubclassProc, 3, (DWORD_PTR)GetWindowClassType(className));
        }

        else if (style == BS_PUSHBUTTON || style == BS_DEFPUSHBUTTON) {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
            // VERY IMPORTANT: DO NOT call SubclassProc here!
            // This prevents our WM_PAINT function from breaking the rendering of these buttons.
        }
    }
    else if (wcscmp(className, CLASSNAME_STATIC) == 0) {
        // Prevent interference with the Color Dialog so it remains intact.
        HWND hRoot = GetAncestor(hwnd, GA_ROOT);
        if (hRoot && GetPropW(hRoot, L"IsColorDlg") != NULL) return;

        SetWindowTheme(hwnd, L"", L"");

        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        if (style & WS_DISABLED) {
            SetWindowLong(hwnd, GWL_STYLE, style & ~WS_DISABLED);
            SetProp(hwnd, L"FakeDisabled", (HANDLE)1);
        }
        SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
    }
    else if (wcscmp(className, CLASSNAME_SYSLINK) == 0) {
        SetWindowTheme(hwnd, L"", L"");
        LITEM item;
        ZeroMemory(&item, sizeof(item));
        item.mask = 0x0001 | 0x0002;
        item.stateMask = 0x0010;
        item.state = 0;
        for (int i = 0; i < 3; i++) {
            item.iLink = i;
            SendMessage(hwnd, WM_USER + 0x302, 0, (LPARAM)&item);
        }
    }
    // Standalone sliders get the dark theme.
    else if (wcscmp(className, CLASSNAME_SCROLLBAR) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
    }
    else if (wcscmp(className, CLASSNAME_TOOLBAR) == 0 || wcscmp(className, CLASSNAME_COMBOBOXEX) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        SetWindowSubclass(hwnd, UniversalSubclassProc, 6, (DWORD_PTR)GetWindowClassType(className));
    }
    else if (wcscmp(className, L"ToolbarWindow32") == 0 || wcscmp(className, L"ComboBoxEx32") == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        SetWindowSubclass(hwnd, UniversalSubclassProc, 6, (DWORD_PTR)GetWindowClassType(className));
    }
    // ComboBoxes retain the old theme (this prevents those white borders in FX windows and settings!)
    else if (wcscmp(className, CLASSNAME_COMBOBOX) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_CFD", NULL);
        // Prevent automatic text selection in comboboxes on window open
        PostMessage(hwnd, CB_SETEDITSEL, 0, MAKELPARAM(-1, 0));
    }
    // Smartly separate Edit fields
    else if (wcscmp(className, CLASSNAME_EDIT) == 0) {
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);

        if ((style & WS_VSCROLL) || (style & ES_MULTILINE)) {

            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);

            // remove WS_EX_CLIENTEDGE thick border
            DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            if (exStyle & WS_EX_CLIENTEDGE) {
                SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_CLIENTEDGE);
                SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
        else {
            SetWindowTheme(hwnd, L"DarkMode_CFD", NULL);
        }
    }
    // extension for Theme Tweaker (ListBox support)
    else if (wcscmp(className, CLASSNAME_LISTBOX) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);

        DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_CLIENTEDGE) {
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_CLIENTEDGE);
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
    }
    else if (wcscmp(className, CLASSNAME_TRACKBAR) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
    }
    else if (wcscmp(className, CLASSNAME_TREEVIEW) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);

        // CRITICAL: apply colors immediately before first paint
        COLORREF bgColor = GetWindowBgColor(hwnd);
        SendMessage(hwnd, 4381, 0, (LPARAM)bgColor); // TVM_SETBKCOLOR
        SendMessage(hwnd, 4382, 0, (LPARAM)g_TextColor); // TVM_SETTEXTCOLOR

        SetWindowSubclass(hwnd, UniversalSubclassProc, 7, (DWORD_PTR)GetWindowClassType(className));
    }
    else if (wcscmp(className, CLASSNAME_LISTVIEW) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);

        // CRITICAL: initialize ListView colors immediately (Media Explorer, Open/Save)
        COLORREF bgColor = GetWindowBgColor(hwnd);
        SendMessage(hwnd, 4097, 0, (LPARAM)bgColor); // LVM_SETBKCOLOR
        SendMessage(hwnd, 4134, 0, (LPARAM)bgColor); // LVM_SETTEXTBKCOLOR
        SendMessage(hwnd, 4132, 0, (LPARAM)g_TextColor); // LVM_SETTEXTCOLOR

        DWORD exStyle = (DWORD)SendMessage(hwnd, 4151, 0, 0);
        exStyle |= 0x00010000; // LVS_EX_DOUBLEBUFFER

        exStyle &= ~0x00000001; // remove LVS_EX_GRIDLINES

        SendMessage(hwnd, 4150, 0, exStyle);

        SetWindowSubclass(hwnd, UniversalSubclassProc, 8, (DWORD_PTR)GetWindowClassType(className));
    }
    else if (wcscmp(className, CLASSNAME_HEADER) == 0) {
        SetWindowTheme(hwnd, L"", L"");
        SetWindowSubclass(hwnd, UniversalSubclassProc, 5, (DWORD_PTR)GetWindowClassType(className));
    }
    else if (wcscmp(className, CLASSNAME_TABCTRL) == 0) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
        SetWindowSubclass(hwnd, UniversalSubclassProc, 4, (DWORD_PTR)GetWindowClassType(className));
    }
    if (wcscmp(className, CLASSNAME_EDIT) != 0 &&
        wcscmp(className, CLASSNAME_COMBOBOX) != 0 &&
        wcscmp(className, CLASSNAME_COMBOBOXEX) != 0) {

        SendMessage(hwnd, WM_THEMECHANGED, 0, 0);
    }
}

static BOOL CALLBACK EnumAllChildren(HWND hwnd, LPARAM lParam) {
    StyleWindow(hwnd);
    return TRUE;
}

static LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (!g_bIsEnabled) return CallNextHookEx(g_hHook, nCode, wParam, lParam); // Skip if the plugin is disabled

    if (nCode == HCBT_CREATEWND || nCode == HCBT_ACTIVATE) {
        HWND hwnd = (HWND)wParam;
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId()) {
            StyleWindow(hwnd);
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// Helper functions for the INI file
static COLORREF ParseColor(const std::wstring& colorStr, COLORREF defaultColor) {
    int r, g, b;
    if (swscanf_s(colorStr.c_str(), L"%d, %d, %d", &r, &g, &b) == 3) return RGB(r, g, b);
    if (swscanf_s(colorStr.c_str(), L"%d,%d,%d", &r, &g, &b) == 3) return RGB(r, g, b);
    return defaultColor;
}

static void WriteColorToIni(const wchar_t* iniPath, const wchar_t* key, COLORREF color) {
    wchar_t colorStr[32];
    swprintf_s(colorStr, L"%d, %d, %d", GetRValue(color), GetGValue(color), GetBValue(color));
    WritePrivateProfileStringW(L"Colors", key, colorStr, iniPath);
}

static COLORREF ReadColorFromIni(const wchar_t* iniPath, const wchar_t* key, COLORREF defaultColor) {
    wchar_t buffer[64];
    GetPrivateProfileStringW(L"Colors", key, L"", buffer, 64, iniPath);
    if (wcslen(buffer) > 0) return ParseColor(buffer, defaultColor);

    WriteColorToIni(iniPath, key, defaultColor);
    return defaultColor;
}

// Function for reading the last modified time of a file
static FILETIME GetFileWriteTime(const std::wstring& path) {
    FILETIME ft = { 0 };
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, NULL, NULL, &ft);
        CloseHandle(hFile);
    }
    return ft;
}

static void LoadConfig() {
    if (g_IniPath.empty()) {
        wchar_t dllPath[MAX_PATH];
        HMODULE hModule = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&LoadConfig, &hModule);
        GetModuleFileNameW(hModule, dllPath, MAX_PATH);
        PathRemoveFileSpecW(dllPath);
        g_IniPath = std::wstring(dllPath) + L"\\reaper_darkmode.ini";
    }
    if (g_menuBarBgBrush) {
        DeleteObject(g_menuBarBgBrush);
        g_menuBarBgBrush = NULL;
    }
    g_menuBgColor = ReadColorFromIni(g_IniPath.c_str(), L"MenuBarBackground", g_menuBgColor);
    g_menuTextColor = ReadColorFromIni(g_IniPath.c_str(), L"MenuTextColor", g_menuTextColor);
    g_menuTextDisabledColor = ReadColorFromIni(g_IniPath.c_str(), L"DisabledTextColor", g_menuTextDisabledColor);
    g_menuHoverColor = ReadColorFromIni(g_IniPath.c_str(), L"MenuBarHover", g_menuHoverColor);
    g_SystemWindowsColor = ReadColorFromIni(g_IniPath.c_str(), L"SystemWindowsColor", g_SystemWindowsColor);

    g_menuConfigLoaded = false;

    g_TitleBarColor = ReadColorFromIni(g_IniPath.c_str(), L"TitleBarColor", g_TitleBarColor);
    g_colorMain = ReadColorFromIni(g_IniPath.c_str(), L"ColorMain", g_colorMain);
    g_colorChild = ReadColorFromIni(g_IniPath.c_str(), L"ColorChild", g_colorChild);
    g_colorEdit = ReadColorFromIni(g_IniPath.c_str(), L"ColorEditBackground", g_colorEdit);
    g_TextColor = ReadColorFromIni(g_IniPath.c_str(), L"TextColor", g_TextColor);
    g_DisabledTextColor = ReadColorFromIni(g_IniPath.c_str(), L"DisabledTextColor", g_DisabledTextColor);
    g_MainWindowBorder = ReadColorFromIni(g_IniPath.c_str(), L"MainWindowBorder", g_MainWindowBorder);
    g_BorderColor = ReadColorFromIni(g_IniPath.c_str(), L"BorderColor", g_BorderColor);
    g_GroupBoxColor = ReadColorFromIni(g_IniPath.c_str(), L"GroupBoxColor", g_GroupBoxColor);
    g_HeaderBackground = ReadColorFromIni(g_IniPath.c_str(), L"HeaderBackground", g_HeaderBackground);
    g_HeaderTextColor = ReadColorFromIni(g_IniPath.c_str(), L"HeaderTextColor", g_HeaderTextColor);
    g_TreeSelectionTextColor = ReadColorFromIni(g_IniPath.c_str(), L"TreeSelectionTextColor", g_TreeSelectionTextColor);
    g_TabBackground = ReadColorFromIni(g_IniPath.c_str(), L"TabBackground", g_TabBackground);
    g_TabSelected = ReadColorFromIni(g_IniPath.c_str(), L"TabSelected", g_TabSelected);

    // Delete old brushes before creating new ones (Memory Leak prevention!)
    if (g_hbrMainBackground) DeleteObject(g_hbrMainBackground);
    if (g_hbrChildBackground) DeleteObject(g_hbrChildBackground);
    if (g_hbrEdit) DeleteObject(g_hbrEdit);

    if (g_menuBarBgBrush) {
        DeleteObject(g_menuBarBgBrush);
        g_menuBarBgBrush = NULL; // set to NULL to force recreation on next paint
    }
    if (g_hbrTabBackground) DeleteObject(g_hbrTabBackground);
    if (g_hbrTabSelected) DeleteObject(g_hbrTabSelected);
    if (g_hbrSystemWindows) DeleteObject(g_hbrSystemWindows);

    g_hbrMainBackground = CreateSolidBrush(g_colorMain);
    g_hbrChildBackground = CreateSolidBrush(g_colorChild);
    g_hbrEdit = CreateSolidBrush(g_colorEdit);
    g_hbrTabBackground = CreateSolidBrush(g_TabBackground);
    g_hbrTabSelected = CreateSolidBrush(g_TabSelected);
    g_hbrSystemWindows = CreateSolidBrush(g_SystemWindowsColor);

    // Read the enable/disable state
    int enabledInt = GetPrivateProfileIntW(L"Settings", L"Enabled", 1, g_IniPath.c_str());
    g_bIsEnabled = (enabledInt != 0);

    // Save the current file time
    g_LastIniTime = GetFileWriteTime(g_IniPath);
}

// Helper function for removing the dark theme
static BOOL CALLBACK EnumUnstyleChildren(HWND hwnd, LPARAM lParam) {
    for (int i = 1; i <= 8; i++) {
        RemoveWindowSubclass(hwnd, UniversalSubclassProc, i);
    }
    SetWindowTheme(hwnd, NULL, NULL);
    RemovePropW(hwnd, L"FakeDisabled");
    return TRUE;
}

static void UnstyleWindow(HWND hwnd) {
    BOOL dark = FALSE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    COLORREF defaultColor = 0xFFFFFFFF;
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &defaultColor, sizeof(defaultColor));
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &defaultColor, sizeof(defaultColor));

    // If this is the main window, forcibly remove Owner-Draw from the menu!
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        int count = GetMenuItemCount(hMenu);
        bool menuChanged = false;
        for (int i = 0; i < count; i++) {
            MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
            mii.fMask = MIIM_FTYPE;
            GetMenuItemInfoW(hMenu, i, TRUE, &mii);
            if (mii.fType & MFT_OWNERDRAW) {
                mii.fType &= ~MFT_OWNERDRAW;
                SetMenuItemInfoW(hMenu, i, TRUE, &mii);
                menuChanged = true;
            }
        }
        // Remove the background color
        MENUINFO mi = { sizeof(MENUINFO) };
        mi.fMask = MIM_BACKGROUND;
        mi.hbrBack = NULL;
        SetMenuInfo(hMenu, &mi);

        if (menuChanged) DrawMenuBar(hwnd);
    }

    EnumUnstyleChildren(hwnd, 0);
    EnumChildWindows(hwnd, EnumUnstyleChildren, 0);
}

// Timer that checks if the Lua script has saved a new INI
static VOID CALLBACK CheckIniTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (g_IniPath.empty()) return;

    FILETIME newTime = GetFileWriteTime(g_IniPath);
    if (CompareFileTime(&newTime, &g_LastIniTime) > 0) {

        LoadConfig();

        bool stateChanged = (g_bIsEnabled != g_LastEnabledState);
        g_LastEnabledState = g_bIsEnabled;

        // Enable or disable global APIs and Hooks
        if (stateChanged) {
            if (g_bIsEnabled) {
                // Enable
                if (SetPreferredAppMode) SetPreferredAppMode(PreferredAppMode::ForceDark);
                if (FlushMenuThemes) FlushMenuThemes();
                if (!g_hHook) g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, NULL, GetCurrentThreadId());
            }
            else {
                // Disable
                if (SetPreferredAppMode) SetPreferredAppMode(PreferredAppMode::Default);
                if (FlushMenuThemes) FlushMenuThemes();
                if (g_hHook) {
                    UnhookWindowsHookEx(g_hHook);
                    g_hHook = NULL;
                }
            }
        }

        // Iterate through all windows to update the interface
        DWORD pid = GetCurrentProcessId();
        HWND topHwnd = GetTopWindow(GetDesktopWindow());
        while (topHwnd) {
            DWORD wpid;
            GetWindowThreadProcessId(topHwnd, &wpid);
            if (wpid == pid) {
                if (stateChanged) {
                    if (g_bIsEnabled) {
                        StyleWindow(topHwnd);
                        EnumChildWindows(topHwnd, EnumAllChildren, 0);
                    }
                    else {
                        UnstyleWindow(topHwnd);
                    }
                }
                else if (g_bIsEnabled) {
                    // Only refresh colors if the window remains in Dark Mode
                    DarkenTitleBar(topHwnd);
                }

                // Forcibly repaint the window, including the frame
                RedrawWindow(topHwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
            }
            topHwnd = GetWindow(topHwnd, GW_HWNDNEXT);
        }
    }
}

extern "C" __declspec(dllexport) int ReaperPluginEntry(void* r, void* v) {
    if (r) {
        LoadConfig();
        InitDarkApi();
        DWORD pid = GetCurrentProcessId();
        HWND hwnd = GetTopWindow(GetDesktopWindow());
        while (hwnd) {
            DWORD wpid;
            GetWindowThreadProcessId(hwnd, &wpid);
            if (wpid == pid) {
                StyleWindow(hwnd);
                EnumChildWindows(hwnd, EnumAllChildren, 0);
            }
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
        }
        g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, NULL, GetCurrentThreadId());

        // Save the main window handle to a global variable
        g_ReaperMainWindow = FindWindow(L"REAPERwnd", NULL);
        if (g_ReaperMainWindow) {
            SendMessage(g_ReaperMainWindow, WM_THEMECHANGED, 0, 0);
            g_IniTimerID = SetTimer(g_ReaperMainWindow, 8899, 1000, CheckIniTimerProc);
        }
        return 1;
    }
    return 0;
}

// Function for safely removing the library from memory
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_PROCESS_DETACH:
        // WARNING: Do not call FindWindow, KillTimer, or UnhookWindowsHookEx here!
        // Doing so causes a deadlock and leaves REAPER as a zombie process in the Task Manager.
        // Cleanup is now safely moved to WM_DESTROY inside UniversalSubclassProc.

        // Deleting GDI objects (colors and brushes) is safe here.
        if (g_hbrMainBackground) DeleteObject(g_hbrMainBackground);
        if (g_hbrChildBackground) DeleteObject(g_hbrChildBackground);
        if (g_hbrEdit) DeleteObject(g_hbrEdit);
        if (g_menuBarBgBrush) DeleteObject(g_menuBarBgBrush);
        if (g_hbrTabBackground) DeleteObject(g_hbrTabBackground);
        if (g_hbrTabSelected) DeleteObject(g_hbrTabSelected);
        if (g_hbrSystemWindows) DeleteObject(g_hbrSystemWindows);
        break;
    }
    return TRUE;
}