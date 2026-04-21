/*
==============================================================================
Project:        REAPER Dark Mode UI

Description:    Provides a full dark mode implementation for Reaper (Windows platform)

Author:         Copyright (c) 2026 Rob Kor (Wormhole Labs)
Created:        2026
Version:        Release v1.0.7

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

// Minimal REAPER plugin info structure for API access
typedef struct REAPER_PLUGIN_INFO {
    int caller_version;
    HWND hwnd_main;
    int (*Register)(const char* name, void* infostruct);
    void* (*GetFunc)(const char* name);
} REAPER_PLUGIN_INFO;

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
static bool g_bGlobalPinEnabled = true;
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
static COLORREF g_MainWindowBorder = RGB(60, 60, 90);
static COLORREF g_BorderColor = RGB(70, 70, 70);
static COLORREF g_GroupBoxColor = RGB(160, 160, 160);
static COLORREF g_HeaderBackground = RGB(80, 80, 80);
static COLORREF g_HeaderTextColor = RGB(240, 240, 240);
static COLORREF g_TreeSelectionTextColor = RGB(255, 255, 255);
static COLORREF g_TabBackground = RGB(56, 56, 56);
static COLORREF g_TabSelected = RGB(32, 32, 32);
static COLORREF g_SystemWindowsColor = RGB(75, 75, 75);
static COLORREF g_TreeSelectionBgColor = RGB(90, 90, 90);
static COLORREF g_TreeUnfocusedSelectionBgColor = RGB(60, 60, 60);

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
        mii.fMask = MIIM_FTYPE | MIIM_DATA;
        GetMenuItemInfoW(hMenu, i, TRUE, &mii);

        if (isFullscreen) {
            if (!(mii.fType & MFT_OWNERDRAW) || mii.dwItemData != (ULONG_PTR)(0xDEAD0000 | i)) {
                mii.fType |= MFT_OWNERDRAW;
                mii.dwItemData = (ULONG_PTR)(0xDEAD0000 | i);
                SetMenuItemInfoW(hMenu, i, TRUE, &mii);
                changed = true;
            }
        }
        else {
            if (mii.fType & MFT_OWNERDRAW) {
                mii.fType &= ~MFT_OWNERDRAW;
                mii.dwItemData = 0;

                wchar_t text[256] = { 0 };
                GetMenuStringW(hMenu, i, text, 256, MF_BYPOSITION);
                mii.fMask |= MIIM_STRING;
                mii.dwTypeData = text;

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

// Safe window text retrieval with timeout to prevent deadlocks
static bool SafeGetWindowText(HWND hWnd, wchar_t* buffer, int maxCount) {
    if (!buffer || maxCount <= 0) return false;
    buffer[0] = L'\0';
    DWORD pid = 0;
    if (GetWindowThreadProcessId(hWnd, &pid) == 0) return false;

    DWORD_PTR res = 0;
    if (SendMessageTimeoutW(hWnd, WM_GETTEXT, maxCount, (LPARAM)buffer, SMTO_ABORTIFHUNG | SMTO_NORMAL, 20, &res)) {
        return true;
    }
    return false;
}

// Window validation helper
static BOOL IsWindowValid(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;
    return (GetWindowThreadProcessId(hwnd, NULL) != 0);
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
    WND_REAPERWINDOW,
    WND_CLICKPATTERN
};

enum class PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
using fnAllowDarkModeForWindow = bool(WINAPI*)(HWND hWnd, bool allow);
using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode);
using fnFlushMenuThemes = void(WINAPI*)();
static fnAllowDarkModeForWindow AllowDarkModeForWindow = nullptr;
static fnSetPreferredAppMode SetPreferredAppMode = nullptr;
static fnFlushMenuThemes FlushMenuThemes = nullptr;
static HHOOK g_hHook = NULL;
static void UnstyleWindow(HWND hwnd);
static void StyleWindow(HWND hwnd);
static BOOL CALLBACK EnumAllChildren(HWND hwnd, LPARAM lParam);
static BOOL IsChildOfTabControl(HWND hWnd);

static COLORREF GetWindowBgColor(HWND hWnd) {
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root) {
        wchar_t title[256] = { 0 };

        // Use the structural property assigned by StyleWindow for system dialogs
        if (GetPropW(root, L"IsFileDialog") != NULL) {

            wchar_t cls[256] = { 0 };
            GetClassNameW(hWnd, cls, 256);

            if (wcscmp(cls, CLASSNAME_TREEVIEW) == 0 || wcscmp(cls, CLASSNAME_LISTVIEW) == 0) {
                return RGB(25, 25, 25);
            }

            if (wcscmp(cls, CLASSNAME_DIALOG) == 0 && hWnd != root) {
                return RGB(56, 56, 56);
            }

            HWND parent = GetParent(hWnd);
            if (parent != NULL && parent != root) {
                wchar_t pCls[256] = { 0 }; GetClassNameW(parent, pCls, 256);
                if (wcscmp(pCls, CLASSNAME_DIALOG) == 0) return RGB(56, 56, 56);
            }

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

        // Use safe retrieval instead of GetWindowTextW
        SafeGetWindowText(root, title, 256);

        // Use the structural property assigned by StyleWindow for system dialogs
        if (GetPropW(root, L"IsFileDialog") != NULL) {

            wchar_t cls[256] = { 0 };
            GetClassNameW(hWnd, cls, 256);

            if (wcscmp(cls, CLASSNAME_TREEVIEW) == 0 || wcscmp(cls, CLASSNAME_LISTVIEW) == 0) {
                static HBRUSH hbrExplorer = CreateSolidBrush(RGB(25, 25, 25));
                return hbrExplorer;
            }

            static HBRUSH hbrFixedTest = CreateSolidBrush(RGB(56, 56, 56));
            if (wcscmp(cls, CLASSNAME_DIALOG) == 0 && hWnd != root) return hbrFixedTest;

            HWND parent = GetParent(hWnd);
            if (parent != NULL && parent != root) {
                wchar_t pCls[256] = { 0 };
                GetClassNameW(parent, pCls, 256);
                if (wcscmp(pCls, CLASSNAME_DIALOG) == 0) return hbrFixedTest;
            }

            return g_hbrSystemWindows;
        }
        return g_hbrChildBackground;
    }
    if ((GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD) != 0) return g_hbrChildBackground;
    return g_hbrMainBackground;
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

// Helper for the horizontal white line underneath the menu bar
static void DrawMenuBarSeparator(HWND hWnd) {
    HDC hdc = GetWindowDC(hWnd);
    if (hdc) {
        RECT rcWin;
        GetWindowRect(hWnd, &rcWin);
        MENUBARINFO mbi = { sizeof(mbi) };
        if (GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi)) {
            RECT rcBar = mbi.rcBar;
            OffsetRect(&rcBar, -rcWin.left, -rcWin.top);

            if (rcBar.bottom > rcBar.top) {
                // Draw a 1px line at the bottom of the menu bar to cover the white separator
                RECT rcLine = { rcBar.left, rcBar.bottom, rcBar.right, rcBar.bottom + 1 };
                HBRUSH hbr = CreateSolidBrush(g_menuBgColor);
                FillRect(hdc, &rcLine, hbr);
                DeleteObject(hbr);
            }
        }
        ReleaseDC(hWnd, hdc);
    }
}

// -----------------------------------------------------------------------------
// CUSTOM PAINT SUBCLASS FOR FAKE SYSLINKS (Reaper Link Buttons)
// -----------------------------------------------------------------------------
static LRESULT CALLBACK FakeSysLinkSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(hWnd, &rc);

        // Dynamically match the parent's background color for a seamless blend
        HWND hParent = GetParent(hWnd);
        COLORREF bgColor = GetWindowBgColor(hParent);
        HBRUSH bgBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        int len = GetWindowTextLengthW(hWnd);
        if (len > 0) {
            wchar_t* text = new wchar_t[len + 1];
            GetWindowTextW(hWnd, text, len + 1);

            SetBkMode(hdc, TRANSPARENT);
            // Inject our readable light blue color for Dark Mode
            SetTextColor(hdc, RGB(0, 150, 255));
            // Ensure we use REAPER's UI font
            HFONT hFont = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            // Draw text aligned to the left and vertically centered
            DrawTextW(hdc, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOldFont);
            delete[] text;
        }

        EndPaint(hWnd, &ps);
        return 0; // Signal that we completely handled the painting
    }
    else if (uMsg == WM_NCDESTROY) {
        RemoveWindowSubclass(hWnd, FakeSysLinkSubclassProc, uIdSubclass);
        RemovePropW(hWnd, L"SubclassedAsLink");
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
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

    // --- AVTOMATIC PINNING OF NON-MIXER WINDOWS (CONFIG IN INI) ---
    if (uMsg == WM_SHOWWINDOW || uMsg == WM_ACTIVATE || uMsg == WM_ACTIVATEAPP) {
        HWND rootTemp = GetAncestor(hWnd, GA_ROOT);

        if (rootTemp && hWnd == rootTemp && rootTemp != g_ReaperMainWindow) {

            wchar_t rootTitle[256] = { 0 };
            SafeGetWindowText(rootTemp, rootTitle, 256);

            if (wcslen(rootTitle) > 0 && wcsstr(rootTitle, L"Mixer") == NULL) {

                if (g_bGlobalPinEnabled) {

                    // Standardni flagi (Brez NOREDRAW, da se okno lahko normalno osveži)
                    UINT uFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;

                    // 1. Ko se okno PRIKAŽE -> Pripni na vrh
                    if (uMsg == WM_SHOWWINDOW && wParam == TRUE) {
                        SetWindowPos(rootTemp, HWND_TOPMOST, 0, 0, 0, 0, uFlags);
                    }
                    // 2. Ko okno DOBI FOKUS -> Pripni na vrh
                    else if (uMsg == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE) {
                        SetWindowPos(rootTemp, HWND_TOPMOST, 0, 0, 0, 0, uFlags);
                    }
                    // 3. Ko preklapljamo med Reaperjem in drugimi programi (npr. Chrome)
                    else if (uMsg == WM_ACTIVATEAPP) {
                        if (wParam == TRUE) {
                            SetWindowPos(rootTemp, HWND_TOPMOST, 0, 0, 0, 0, uFlags); // Nazaj v Reaperju
                        }
                        else {
                            SetWindowPos(rootTemp, HWND_NOTOPMOST, 0, 0, 0, 0, uFlags); // Alt+Tab ven iz Reaperja
                        }
                    }
                    // OPOMBA: WM_SHOWWINDOW z wParam == FALSE namenoma IGNORIRAMO. 
                    // Reaper ga bo skril sam in s tem preprečimo "Ghost Pin" hrošča!
                }
            }
        }
    }
    // ----------------------------------------------------------------

    /// --- CUSTOM DRAW for TREEVIEW and LISTVIEW ---
    if (uMsg == WM_NOTIFY) {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->code == NM_CUSTOMDRAW) {
            wchar_t clsName[256] = { 0 };
            GetClassNameW(nmhdr->hwndFrom, clsName, 256);

            // FOCUS DETECTION: Check if the specific list currently has keyboard/mouse focus
            bool hasFocus = (GetFocus() == nmhdr->hwndFrom);

            // --- LEFT SIDE (TreeView - FX) ---
            if (wcscmp(clsName, CLASSNAME_TREEVIEW) == 0) {
                LPNMTVCUSTOMDRAW ptvcd = (LPNMTVCUSTOMDRAW)lParam;

                if (ptvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                    // Hide native dotted focus lines
                    SendMessage(nmhdr->hwndFrom, 0x0127 /*WM_CHANGEUISTATE*/, MAKEWPARAM(1, 1), 0);

                    LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                    return res | CDRF_NOTIFYITEMDRAW;
                }
                else if (ptvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    bool isActuallySelected = (SendMessage(nmhdr->hwndFrom, 4391, ptvcd->nmcd.dwItemSpec, 2) & 2) != 0;

                    if (isActuallySelected) {
                        COLORREF normalBg = GetWindowBgColor(nmhdr->hwndFrom);
                        ptvcd->clrText = normalBg;
                        ptvcd->clrTextBk = normalBg;

                        ptvcd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS | CDIS_HOT);

                        return CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
                    }
                    else {
                        // Normal not-selected element
                        ptvcd->clrText = g_TextColor;
                        ptvcd->clrTextBk = GetWindowBgColor(nmhdr->hwndFrom);
                        return CDRF_NEWFONT;
                    }
                }
                else if (ptvcd->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT) {
                    bool isActuallySelected = (SendMessage(nmhdr->hwndFrom, 4391, ptvcd->nmcd.dwItemSpec, 2) & 2) != 0;

                    if (isActuallySelected) {
                        HDC hdc = ptvcd->nmcd.hdc;
                        HWND hTree = nmhdr->hwndFrom;
                        HTREEITEM hItem = (HTREEITEM)ptvcd->nmcd.dwItemSpec;

                        RECT rcText;
                        *(HTREEITEM*)&rcText = hItem;
                        if (SendMessage(hTree, 4356, TRUE, (LPARAM)&rcText)) {

                            // BACKGROUND SELECTION: Draw focused or unfocused selection color
                            COLORREF bgColor = hasFocus ? g_TreeSelectionBgColor : g_TreeUnfocusedSelectionBgColor;
                            HBRUSH hBrush = CreateSolidBrush(bgColor);
                            FillRect(hdc, &rcText, hBrush);
                            DeleteObject(hBrush);

                            wchar_t text[256] = { 0 };
                            TVITEMEXW item = { 0 };
                            item.mask = TVIF_TEXT | TVIF_HANDLE;
                            item.hItem = hItem;
                            item.pszText = text;
                            item.cchTextMax = 256;
                            SendMessage(hTree, 4414, 0, (LPARAM)&item);

                            // Keep text color highlighted even without background box
                            SetTextColor(hdc, g_TreeSelectionTextColor);
                            SetBkMode(hdc, TRANSPARENT);

                            RECT rcDraw = rcText;
                            rcDraw.left += 2;
                            DrawText(hdc, text, -1, &rcDraw, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
                        }
                        return CDRF_DODEFAULT;
                    }
                }
            }
            // --- RIGHT SIDE: General lists (ListView - Actions, FX lists) ---
            else if (wcscmp(clsName, CLASSNAME_LISTVIEW) == 0) {
                LPNMLVCUSTOMDRAW plvcd = (LPNMLVCUSTOMDRAW)lParam;

                if (plvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                    SendMessage(nmhdr->hwndFrom, 0x0127 /*WM_CHANGEUISTATE*/, MAKEWPARAM(1, 1), 0);

                    LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                    return res | CDRF_NOTIFYITEMDRAW;
                }
                else if (plvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    bool isSelected = (SendMessage(nmhdr->hwndFrom, 4140, plvcd->nmcd.dwItemSpec, 2) & 2) != 0;

                    if (isSelected) {
                        HDC hdc = plvcd->nmcd.hdc;
                        RECT rc = plvcd->nmcd.rc;

                        // BACKGROUND SELECTION: Draw focused or unfocused selection color
                        COLORREF bgColor = hasFocus ? g_TreeSelectionBgColor : g_TreeUnfocusedSelectionBgColor;
                        HBRUSH hBrush = CreateSolidBrush(bgColor);
                        FillRect(hdc, &rc, hBrush);
                        DeleteObject(hBrush);

                        // Keep text color highlighted even without background box
                        plvcd->clrText = g_TreeSelectionTextColor;
                        plvcd->clrTextBk = bgColor;

                        plvcd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS | CDIS_HOT);

                        return CDRF_NEWFONT;
                    }
                    else {
                        COLORREF bgColor = GetWindowBgColor(nmhdr->hwndFrom);
                        plvcd->clrTextBk = bgColor;
                        plvcd->clrText = g_TextColor;
                        return CDRF_NEWFONT;
                    }
                }
            }
        }
    }

    wchar_t className[256] = { 0 };
    GetClassName(hWnd, className, 256);

    WndClass classType = (WndClass)dwRefData;

    // --- IMMEDIATE LIST REFRESH ON FOCUS CHANGE ---
    // Forces TreeViews and ListViews to redraw immediately when they gain or lose focus,
    // ensuring our custom unfocused selection color updates instantly.
    if (uMsg == WM_SETFOCUS || uMsg == WM_KILLFOCUS) {
        if (classType == WND_TREEVIEW || classType == WND_LISTVIEW) {
            InvalidateRect(hWnd, NULL, FALSE);
            UpdateWindow(hWnd);
        }
    }
    // ----------------------------------------------------------

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

    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root) {
        if (GetPropW(root, L"IsColorDlg") != NULL) isColorDlg = true;
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
            bool foundText = false;

            if ((mis->itemData & 0xFFFF0000) == 0xDEAD0000) {
                int pos = mis->itemData & 0xFFFF;
                HMENU hMenu = GetMenu(hWnd);
                GetMenuStringW(hMenu, pos, text, 256, MF_BYPOSITION);
                foundText = true;
            }
            else {
                foundText = GetMenuTextFromItemStruct(hWnd, mis->itemID, text, 256);
            }

            if (foundText) {
                HDC hdc = GetDC(hWnd);
                NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
                SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);

                HFONT hSysFont = CreateFontIndirectW(&ncm.lfMenuFont);
                HFONT hOldFont = (HFONT)SelectObject(hdc, hSysFont);

                SIZE size = { 0 };
                GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &size);

                SelectObject(hdc, hOldFont);
                DeleteObject(hSysFont);
                ReleaseDC(hWnd, hdc);

                UINT dpi = GetDpiForWindow(hWnd);
                if (dpi == 0) dpi = 96;
                float dpiScale = (float)dpi / 96.0f;


                mis->itemWidth = size.cx + (int)(5.0f * dpiScale);
                mis->itemHeight = size.cy + (int)(6.0f * dpiScale);
                // ----------------------------

                return TRUE;
            }
        }
    }

    // Buttons in menu bar - draw
    if (uMsg == WM_DRAWITEM) {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlType == ODT_MENU) {
            wchar_t text[256] = { 0 };
            HMENU hMenu = GetMenu(hWnd);

            int count = GetMenuItemCount(hMenu);
            if (dis->itemData >= 0 && dis->itemData < (ULONG_PTR)count) {
                GetMenuStringW(hMenu, (UINT)dis->itemData, text, 256, MF_BYPOSITION);
            }
            else {
                GetMenuTextFromItemStruct(hWnd, dis->itemID, text, 256); // Fallback
            }

            if (wcslen(text) > 0) {
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
        // --- THEME TWEAKER (LISTBOX GRID) ---
        else if (dis->CtlType == ODT_LISTBOX) {
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            HPEN hPen = CreatePen(PS_SOLID, 1, g_BorderColor);

            HBRUSH hOldBrush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            HPEN hOldPen = (HPEN)SelectObject(dis->hDC, hPen);

            Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);

            SelectObject(dis->hDC, hOldPen);
            SelectObject(dis->hDC, hOldBrush);
            DeleteObject(hPen);

            return res;
        }
    }

    // TEST FOR COLOR PICKER AND SENSITIVE WINDOWS
    if (uMsg == WM_INITDIALOG || uMsg == WM_SHOWWINDOW) {
        if (wcscmp(className, CLASSNAME_DIALOG) == 0) {
            wchar_t title[256] = { 0 };
            GetWindowTextW(hWnd, title, 256);

            // Structural detection heuristic for Color Picker
            bool isColorStruct = (GetDlgItem(hWnd, 712) != NULL);


            // Structural detection heuristic for "Customize menus"
            bool isCustomizeMenus = (GetDlgItem(hWnd, 0x05FE) != NULL &&
                GetDlgItem(hWnd, 0x05FA) != NULL &&
                GetDlgItem(hWnd, 0x060D) != NULL);

            // Structural detection heuristic for "Select toolbar icon"
            bool isToolbarIcon = (GetDlgItem(hWnd, 0x03EF) != NULL &&
                GetDlgItem(hWnd, 0x03F1) != NULL &&
                GetDlgItem(hWnd, 0x0420) != NULL &&
                GetDlgItem(hWnd, 0x0710) == NULL &&
                GetDlgItem(hWnd, 0x0685) == NULL);

            // Structural detection heuristic for Metronomome
            bool hasClickPattern = (FindWindowExW(hWnd, NULL, L"REAPERClickPatternEdit", NULL) != NULL);

            // Protect sensitive windows from being fully painted over (exclude Metronome!)
            if ((isColorStruct || isCustomizeMenus || isToolbarIcon) && !hasClickPattern) {
                SetPropW(hWnd, L"IsColorDlg", (HANDLE)1);
                isColorDlg = true;
            }
        }
    }

    if (uMsg == WM_SHOWWINDOW && wParam == TRUE) {
        if (wcscmp(className, CLASSNAME_DIALOG) == 0) {
            if (!isColorDlg) {
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

        // Prevent a full refresh of all elements when the MIDI editor is displayed
        wchar_t wndClass[256] = { 0 };
        GetClassNameW(hWnd, wndClass, 256);

        bool isMidiEditor = (wcscmp(wndClass, L"REAPERmidieditorwnd") == 0);

        // Refresh elements natively if THIS IS NOT the MIDI editor window
        if (!isMidiEditor) {
            EnumChildWindows(hWnd, EnumAllChildren, 0);
        }
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

    // --- CUSTOM BORDERS AND MENU SEPARATOR ---
    if (uMsg == WM_NCPAINT || uMsg == WM_NCACTIVATE) {
        if (classType == WND_LISTVIEW || classType == WND_TREEVIEW) {
            // Let Windows draw the default 3D scrollbars and edges first
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            // Get the Device Context for the ENTIRE window (including non-client area)
            HDC hdc = GetWindowDC(hWnd);
            RECT rc;
            GetWindowRect(hWnd, &rc);
            OffsetRect(&rc, -rc.left, -rc.top); // Normalize coordinates to 0,0

            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH)); // Hollow brush

            // 1. Outer border (1px) - apply our custom border color
            HPEN hPenBorder = CreatePen(PS_SOLID, 1, g_BorderColor);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPenBorder);
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

            // 2. Inner border (1px) - blend with the control's background to make the border look 1px thin
            COLORREF bgColor = GetWindowBgColor(hWnd);
            HPEN hPenBg = CreatePen(PS_SOLID, 1, bgColor);
            SelectObject(hdc, hPenBg);
            Rectangle(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);

            // Cleanup
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPenBorder);
            DeleteObject(hPenBg);
            ReleaseDC(hWnd, hdc);

            return res;
        }
        else if (GetMenu(hWnd) != NULL) {
            // --- FIX: Cover the white separator line under the main menu ---
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            DrawMenuBarSeparator(hWnd);
            return res;
        }
    }
    // ------------------------------------------------------

    if (uMsg == WM_PAINT) {
        HWND rootTemp = GetAncestor(hWnd, GA_ROOT);

        // --- OPTIMIZATION: Check for "Confirm Save As" ONLY on dialogs and ONLY ONCE ---
        // Calling SafeGetWindowText on every WM_PAINT tick causes severe UI lag (e.g., tracklist scrolling).
        if (classType == WND_DIALOG) {
            // Check if we have already tested this specific window to avoid redundant polling
            if (GetPropW(hWnd, L"HybridChecked") == NULL) {
                SetPropW(hWnd, L"HybridChecked", (HANDLE)1); // Mark as checked

                wchar_t wndTitle[256] = { 0 };
                SafeGetWindowText(hWnd, wndTitle, 256);

                if (wcscmp(wndTitle, L"Confirm Save As") == 0) {
                    if (GetPropW(hWnd, L"HybridStyled") == NULL) {
                        SetPropW(hWnd, L"HybridStyled", (HANDLE)1);

                        // 1. Remove the custom Dark Mode theme from the window content
                        SetWindowTheme(hWnd, NULL, NULL);

                        // 2. Remove our subclassing from child buttons to make them light again
                        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lp) -> BOOL {
                            RemoveWindowSubclass(hChild, UniversalSubclassProc, 1);
                            RemoveWindowSubclass(hChild, UniversalSubclassProc, 3);
                            SetWindowTheme(hChild, NULL, NULL);
                            return TRUE;
                            }, 0);

                        // 3. Re-force the dark title bar via DWM (crucial after theme reset)
                        DarkenTitleBar(hWnd);

                        // Refresh to apply changes immediately
                        InvalidateRect(hWnd, NULL, TRUE);
                    }
                }
            }

            // If the window is flagged as Hybrid, return native drawing immediately
            if (GetPropW(hWnd, L"HybridStyled") != NULL) {
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }
        }
        // ------------------------------------------------------------------------------------

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
            PAINTSTRUCT ps = { 0 };
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
                    // Let the OS draw the native control (glyph + native text)
                    LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);

                    HDC hdc = GetDC(hWnd);
                    RECT rc; GetClientRect(hWnd, &rc);

                    // Calculate accurate DPI scale factor
                    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
                    if (dpiX == 0) dpiX = 96; // Fallback to 100%
                    float scale = (float)dpiX / 96.0f;

                    // THE SWEET SPOT (Pixel-perfect cut)
                    // At 100%, the box ends ~14px, native text starts ~16px.
                    // We slice exactly at 15px and scale it perfectly.
                    int clearOffset = (int)(15.0f * scale);
                    int textOffset = (int)(19.0f * scale);

                    // Erase the native text exactly between the box and the text
                    RECT rcClear = rc;
                    rcClear.left += clearOffset;
                    HBRUSH bg = GetWindowBgBrush(hWnd);
                    FillRect(hdc, &rcClear, bg);

                    // Draw the custom text
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
            PAINTSTRUCT ps = { 0 };
            HDC hdc = BeginPaint(hWnd, &ps);
            HBRUSH bgBrush = GetWindowBgBrush(hWnd);
            FillRect(hdc, &ps.rcPaint, bgBrush);
            EndPaint(hWnd, &ps);
            return 0;
        }
        else if (classType == WND_CLICKPATTERN) {
            // Let REAPER draw its original white grid and black dots first
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            // Grab the drawn canvas and invert its colors instantly
            HDC hdc = GetDC(hWnd);
            RECT rc = { 0 };
            GetClientRect(hWnd, &rc);

            // DSTINVERT turns white into black, and black into white
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdc, 0, 0, DSTINVERT);

            ReleaseDC(hWnd, hdc);
            return res;
        }
    }

    if (uMsg == WM_CTLCOLORDLG || uMsg == WM_CTLCOLORMSGBOX) {
        HWND hDlg = (HWND)lParam;

        // LET THE DIALOG BACKGROUND BE DARK
        return (LRESULT)(GetWindowBgBrush(hDlg));
    }
    else if (uMsg == WM_CTLCOLORSTATIC || uMsg == WM_CTLCOLORBTN) {
        // KEEP COLOR SWATCHES NATIVE IN COLOR PICKER! (Prevents them from turning gray)
        if (isColorDlg && uMsg == WM_CTLCOLORSTATIC) return DefSubclassProc(hWnd, uMsg, wParam, lParam);

        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        COLORREF bgColor = GetWindowBgColor(hWnd);
        HBRUSH bgBrush = GetWindowBgBrush(hWnd);

        wchar_t childClass[256];
        GetClassNameW(hCtrl, childClass, 256);

        DWORD btnStyle = GetWindowLong(hCtrl, GWL_STYLE) & BS_TYPEMASK;
        bool isStandardButton = (wcscmp(childClass, L"Button") == 0 &&
            (btnStyle == BS_PUSHBUTTON || btnStyle == BS_DEFPUSHBUTTON));

        // --- FAKE LINK BACKGROUND HANDLING ---
        bool isCheckBox = (wcscmp(childClass, L"Button") == 0 && (btnStyle == BS_CHECKBOX || btnStyle == BS_AUTOCHECKBOX || btnStyle == BS_3STATE || btnStyle == BS_AUTO3STATE));
        bool isRadio = (wcscmp(childClass, L"Button") == 0 && (btnStyle == BS_RADIOBUTTON || btnStyle == BS_AUTORADIOBUTTON));
        bool isGroupBox = (wcscmp(childClass, L"Button") == 0 && (btnStyle == BS_GROUPBOX));

        if (wcscmp(childClass, L"Button") == 0 && !isStandardButton && !isCheckBox && !isRadio && !isGroupBox) {
            // Already subclassed in StyleWindow, just return transparent background.
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)bgBrush;
        }
        // ----------------------------------------------------

        // Protection against embossing only for genuine Static text and checkboxes/radio buttons!
        if ((!IsWindowEnabled(hCtrl) || GetPropW(hCtrl, L"FakeDisabled") != NULL) && !isStandardButton) {
            SetTextColor(hdc, g_DisabledTextColor);
            SetBkColor(hdc, bgColor);
            SetBkMode(hdc, OPAQUE);
            return (LRESULT)bgBrush;
        }

        if (wcscmp(childClass, L"SysLink") == 0) {
            SetTextColor(hdc, RGB(0, 150, 255));
        }
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
    if (wcscmp(className, L"REAPERClickPatternEdit") == 0) return WND_CLICKPATTERN;
    if (wcsncmp(className, CLASSNAME_REAPER_PREFIX, 6) == 0) return WND_REAPERWINDOW;
    return WND_UNKNOWN;
}

static void StyleWindow(HWND hwnd) {
    // Validation at the very start
    if (!IsWindowValid(hwnd)) {
        return;  // Exit if window is invalid
    }

    wchar_t className[256] = { 0 };
    if (!GetClassNameW(hwnd, className, 256)) return;

    // --- IGNORE MENUS ---
    if (wcscmp(className, CLASSNAME_MENU) == 0) return;

    // --- IGNORE PYTHON TCL/TK WINDOWS (CRASH FIX) ---
    // Tcl/Tkinter uses its own custom rendering engine. Subclassing these will crash the tcl86t.dll module.
    if (wcsncmp(className, L"TkTopLevel", 10) == 0 || wcsncmp(className, L"TkChild", 7) == 0) {
        return;
    }

    // --- PROTECT FLOATING OVERLAYS (MIDI Razor Edit) ---
    LONG wndStyle = GetWindowLong(hwnd, GWL_STYLE);
    if ((wndStyle & WS_POPUP) && !(wndStyle & WS_CAPTION)) {
        wchar_t wndTitle[256] = { 0 };

        // Safe title check for floating windows
        SafeGetWindowText(hwnd, wndTitle, 256);

        if (wcslen(wndTitle) == 0) {
            return;
        }
    }
    // ---------------------------------------------------

    HWND root = GetAncestor(hwnd, GA_ROOT);

    // --- HYBRID DIALOG CHECK (Confirm Save As) ---
    // If the root window is tagged for hybrid styling, skip dark mode for content
    if (root && GetPropW(root, L"IsHybridWindow") != NULL) {
        // We still allow the title bar to be darkened in StyleWindow logic
        if (hwnd == root) {
            DarkenTitleBar(hwnd);
        }
        return; // Skip further dark subclassing for the window and its children
    }

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

        // Force UAH dark menubar for specific floating REAPER windows
        if (wcscmp(className, L"REAPERmidieditorwnd") == 0 || wcscmp(className, L"REAPERMediaExplorerMainwnd") == 0) {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
            SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
        }
    }

    if (GetPropW(hwnd, L"IsColorDlg") != NULL) {
        SetWindowSubclass(hwnd, UniversalSubclassProc, 1, (DWORD_PTR)GetWindowClassType(className));
    }

    if (AllowDarkModeForWindow) {
        AllowDarkModeForWindow(hwnd, true);
    }

    // Capture "REAPERwnd" as well as floating windows ("REAPERdocker")
    if (wcsncmp(className, CLASSNAME_REAPER_PREFIX, 6) == 0) {
        // --- SHIELD FOR MIDI CHILD WINDOWS ---
        if (root) {
            wchar_t rootClass[256] = { 0 };
            GetClassNameW(root, rootClass, 256);

            // Skip custom styling for internal MIDI editor elements to preserve native piano roll drawing
            if (wcscmp(rootClass, L"REAPERmidieditorwnd") == 0 && hwnd != root) {
                return;
            }
        }
        // -------------------------------------

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

        bool isStandardButton = (style == BS_PUSHBUTTON || style == BS_DEFPUSHBUTTON);
        bool isCheckBox = (style == BS_CHECKBOX || style == BS_AUTOCHECKBOX || style == BS_3STATE || style == BS_AUTO3STATE);
        bool isRadio = (style == BS_RADIOBUTTON || style == BS_AUTORADIOBUTTON);
        bool isGroupBox = (style == BS_GROUPBOX);

        // --- FAKE LINK RECOGNITION (Early Subclassing) ---
        // Catch the fake links immediately when the window is styled, before any painting occurs.
        if (!isStandardButton && !isCheckBox && !isRadio && !isGroupBox) {
            if (!GetPropW(hwnd, L"SubclassedAsLink")) {
                SetPropW(hwnd, L"SubclassedAsLink", (HANDLE)1);
                SetWindowSubclass(hwnd, FakeSysLinkSubclassProc, 10101, 0);
            }
        }
        else if (isGroupBox) {
            HWND rootWindow = GetAncestor(hwnd, GA_ROOT);

            // Structural detection for Preferences/Settings window
            bool isPreferencesWindow = (GetDlgItem(rootWindow, 0x0456) != NULL &&
                GetDlgItem(rootWindow, 0x0520) != NULL &&
                GetDlgItem(rootWindow, 0x051F) != NULL);

            if (isPreferencesWindow) {
                return; // Skip styling GroupBoxes inside the Preferences window
            }
            SetWindowTheme(hwnd, L"", L"");
            SetWindowSubclass(hwnd, UniversalSubclassProc, 2, (DWORD_PTR)GetWindowClassType(className));
        }
        else if (isCheckBox || isRadio) {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
            SetWindowSubclass(hwnd, UniversalSubclassProc, 3, (DWORD_PTR)GetWindowClassType(className));
        }
        else if (isStandardButton) {
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

        // --- FX BROWSER SEARCH AUTO-SELECT FIX ---
        // We detect the FX Filter search box using its Control ID (0x473).
        // If it's the search box, we SKIP the de-selection so users can type immediately.
        // For all other ComboBoxes, we clear the selection to keep the UI clean.
        int ctrlId = GetDlgCtrlID(hwnd);
        if (ctrlId != 0x473) {
            PostMessage(hwnd, CB_SETEDITSEL, 0, MAKELPARAM(-1, 0));
        }
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
    g_TreeSelectionBgColor = ReadColorFromIni(g_IniPath.c_str(), L"TreeSelectionBgColor", g_TreeSelectionBgColor);
    g_TreeUnfocusedSelectionBgColor = ReadColorFromIni(g_IniPath.c_str(), L"TreeUnfocusedSelectionBgColor", g_TreeUnfocusedSelectionBgColor);
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

    // Read global pin status
    int globalPinInt = GetPrivateProfileIntW(L"Settings", L"GlobalPin", 0, g_IniPath.c_str());
    g_bGlobalPinEnabled = (globalPinInt != 0);
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

        if (stateChanged) {
            if (g_bIsEnabled) {
                if (SetPreferredAppMode) SetPreferredAppMode(PreferredAppMode::ForceDark);
                if (FlushMenuThemes) FlushMenuThemes();
                if (!g_hHook) g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, NULL, GetCurrentThreadId());
            }
            else {
                if (SetPreferredAppMode) SetPreferredAppMode(PreferredAppMode::Default);
                if (FlushMenuThemes) FlushMenuThemes();
                if (g_hHook) {
                    UnhookWindowsHookEx(g_hHook);
                    g_hHook = NULL;
                }
            }
        }

        DWORD pid = GetCurrentProcessId();
        HWND topHwnd = GetTopWindow(GetDesktopWindow());
        while (topHwnd) {
            // Validate window handle before processing
            if (!IsWindowValid(topHwnd)) {
                topHwnd = GetWindow(topHwnd, GW_HWNDNEXT);
                continue;  // Skip invalid windows
            }

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
                    DarkenTitleBar(topHwnd);
                }

                RedrawWindow(topHwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
            }
            topHwnd = GetWindow(topHwnd, GW_HWNDNEXT);
        }
    }
}

extern "C" __declspec(dllexport) int ReaperPluginEntry(void* r, void* v) {
    if (v && r) {
        REAPER_PLUGIN_INFO* rec = (REAPER_PLUGIN_INFO*)v;

        // Load specific REAPER API functions
        void (*SetThemeColor)(const char*, int, int) = (void (*)(const char*, int, int))rec->GetFunc("SetThemeColor");
        void (*ThemeLayout_RefreshAll)() = (void (*)())rec->GetFunc("ThemeLayout_RefreshAll");

        LoadConfig();

        // SYNC STATE: Ensure timer logic knows the correct initial state
        g_LastEnabledState = g_bIsEnabled;

        InitDarkApi();

        // --- SILENTLY APPLY THEME COLORS ON STARTUP ---
        // If the plugin is enabled and we have the REAPER API functions loaded, apply the RAM color overrides
        int syncInt = GetPrivateProfileIntW(L"Settings", L"SyncThemeColors", 0, g_IniPath.c_str());
        if (g_bIsEnabled && syncInt == 1 && SetThemeColor && ThemeLayout_RefreshAll) {

            // Note: COLORREF on Windows perfectly matches REAPER's native color integer
            // Sync Backgrounds (ColorChild)
            SetThemeColor("col_main_bg", (int)g_colorChild, 0);
            SetThemeColor("window_bg", (int)g_colorChild, 0);
            SetThemeColor("col_main_editbk", (int)g_colorChild, 0);

            // Sync Gridlines
            COLORREF gridColor = ReadColorFromIni(g_IniPath.c_str(), L"GridLinesColor", RGB(60, 60, 60));
            SetThemeColor("genlist_grid", (int)gridColor, 0);
            SetThemeColor("midieditorlist_grid", (int)gridColor, 0);
            SetThemeColor("explorer_grid", (int)gridColor, 0);

            // Sync Text Color
            COLORREF textColor = ReadColorFromIni(g_IniPath.c_str(), L"ThemedWindowText", RGB(160, 160, 160));
            SetThemeColor("col_main_text", (int)textColor, 0);

            ThemeLayout_RefreshAll();
        }
        // ----------------------------------------------

        // Only apply initial styling and hooks if enabled in INI
        if (g_bIsEnabled) {
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
        }

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
        // Doing so may cause a deadlock and leaves REAPER as a zombie process in the Task Manager.
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