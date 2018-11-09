/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdafx.h"
#include "MainWindow.h"

#include "Common.h"
#include "MiniBrowserLibResource.h"
#include "WebKitLegacyBrowserWindow.h"

#if ENABLE(WEBKIT)
#include "WebKitBrowserWindow.h"
#endif

namespace WebCore {
float deviceScaleFactorForWindow(HWND);
}

static constexpr int controlButtonWidth = 24;
static constexpr int urlBarHeight = 24;

static WNDPROC DefEditProc = nullptr;

static LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

std::wstring MainWindow::s_windowClass;
size_t MainWindow::s_numInstances;

static std::wstring loadString(int id)
{
    constexpr size_t length = 100;
    wchar_t buff[length];
    LoadString(hInst, id, buff, length);
    return buff;
}

void MainWindow::registerClass(HINSTANCE hInstance)
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    s_windowClass = loadString(IDC_MINIBROWSER);

    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MINIBROWSER));
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_MINIBROWSER);
    wcex.lpszClassName  = s_windowClass.c_str();
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassEx(&wcex);
}

MainWindow::MainWindow(BrowserWindowType type)
    : m_browserWindowType(type)
{
    s_numInstances++;
}

MainWindow::~MainWindow()
{
    s_numInstances--;
}

Ref<MainWindow> MainWindow::create(BrowserWindowType type)
{
    return adoptRef(*new MainWindow(type));
}

bool MainWindow::init(HINSTANCE hInstance, bool usesLayeredWebView, bool pageLoadTesting)
{
    registerClass(hInstance);

    auto title = loadString(IDS_APP_TITLE);

    m_hMainWnd = CreateWindow(s_windowClass.c_str(), title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInstance, this);

    if (!m_hMainWnd)
        return false;

#if !ENABLE(WEBKIT)
    EnableMenuItem(GetMenu(m_hMainWnd), IDM_NEW_WEBKIT_WINDOW, MF_GRAYED);
#endif

    float scaleFactor = WebCore::deviceScaleFactorForWindow(nullptr);
    m_hBackButtonWnd = CreateWindow(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE  | BS_TEXT, 0, 0, 0, 0, m_hMainWnd, reinterpret_cast<HMENU>(IDM_HISTORY_BACKWARD), hInstance, 0);
    m_hForwardButtonWnd = CreateWindow(L"BUTTON", L">", WS_CHILD | WS_VISIBLE | BS_TEXT, scaleFactor * controlButtonWidth, 0, 0, 0, m_hMainWnd, reinterpret_cast<HMENU>(IDM_HISTORY_FORWARD), hInstance, 0);
    m_hURLBarWnd = CreateWindow(L"EDIT", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, scaleFactor * controlButtonWidth * 2, 0, 0, 0, m_hMainWnd, 0, hInstance, 0);

    DefEditProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(m_hURLBarWnd, GWLP_WNDPROC));
    SetWindowLongPtr(m_hURLBarWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));

    auto factory = WebKitLegacyBrowserWindow::create;
#if ENABLE(WEBKIT)
    if (m_browserWindowType == BrowserWindowType::WebKit)
        factory = WebKitBrowserWindow::create;
#endif
    m_browserWindow = factory(m_hMainWnd, m_hURLBarWnd, usesLayeredWebView, pageLoadTesting);
    if (!m_browserWindow)
        return false;
    HRESULT hr = m_browserWindow->init();
    if (FAILED(hr))
        return false;

    updateDeviceScaleFactor();
    resizeSubViews();
    SetFocus(m_hURLBarWnd);
    return true;
}

void MainWindow::resizeSubViews()
{
    float scaleFactor = WebCore::deviceScaleFactorForWindow(m_hMainWnd);

    RECT rcClient;
    GetClientRect(m_hMainWnd, &rcClient);

    int height = scaleFactor * urlBarHeight;
    int width = scaleFactor * controlButtonWidth;

    MoveWindow(m_hBackButtonWnd, 0, 0, width, height, TRUE);
    MoveWindow(m_hForwardButtonWnd, width, 0, width, height, TRUE);
    MoveWindow(m_hURLBarWnd, width * 2, 0, rcClient.right, height, TRUE);

    if (m_browserWindow->usesLayeredWebView() || !m_browserWindow->hwnd())
        return;
    MoveWindow(m_browserWindow->hwnd(), 0, height, rcClient.right, rcClient.bottom - height, TRUE);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RefPtr<MainWindow> thisWindow = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE:
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
        break;
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        switch (wmEvent) {
        case 0: // Menu or BN_CLICKED
        case 1: // Accelerator
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        if (wmId >= IDM_HISTORY_LINK0 && wmId <= IDM_HISTORY_LINK9) {
            thisWindow->browserWindow()->navigateToHistory(wmId);
            break;
        }
        // Parse the menu selections:
        switch (wmId) {
        case IDC_URL_BAR:
            thisWindow->onURLBarEnter();
            break;
        case IDM_NEW_WEBKIT_WINDOW: {
            auto& newWindow = MainWindow::create(BrowserWindowType::WebKit).leakRef();
            newWindow.init(hInst);
            ShowWindow(newWindow.hwnd(), SW_SHOW);
            break;
        }
        case IDM_NEW_WEBKITLEGACY_WINDOW: {
            auto& newWindow = MainWindow::create(BrowserWindowType::WebKitLegacy).leakRef();
            newWindow.init(hInst);
            ShowWindow(newWindow.hwnd(), SW_SHOW);
            break;
        }
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDM_PRINT:
            thisWindow->browserWindow()->print();
            break;
        case IDM_WEB_INSPECTOR:
            thisWindow->browserWindow()->launchInspector();
            break;
        case IDM_PROXY_SETTINGS:
            thisWindow->browserWindow()->openProxySettings();
            break;
        case IDM_CACHES:
            if (!::IsWindow(thisWindow->m_hCacheWnd)) {
                thisWindow->m_hCacheWnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_CACHES), hWnd, cachesDialogProc, reinterpret_cast<LPARAM>(thisWindow.get()));
                ::ShowWindow(thisWindow->m_hCacheWnd, SW_SHOW);
            }
            break;
        case IDM_HISTORY_BACKWARD:
        case IDM_HISTORY_FORWARD:
            thisWindow->browserWindow()->navigateForwardOrBackward(wmId);
            break;
        case IDM_UA_OTHER:
            DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_USER_AGENT), hWnd, customUserAgentDialogProc, reinterpret_cast<LPARAM>(thisWindow.get()));
            break;
        case IDM_ACTUAL_SIZE:
            thisWindow->browserWindow()->resetZoom();
            break;
        case IDM_ZOOM_IN:
            thisWindow->browserWindow()->zoomIn();
            break;
        case IDM_ZOOM_OUT:
            thisWindow->browserWindow()->zoomOut();
            break;
        case IDM_SHOW_LAYER_TREE:
            thisWindow->browserWindow()->showLayerTree();
            break;
        default:
            if (!thisWindow->toggleMenuItem(wmId))
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        }
        break;
    case WM_DESTROY:
        SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
        thisWindow->deref();
        if (s_numInstances > 1)
            return 0;
#if USE(CF)
        CFRunLoopStop(CFRunLoopGetMain());
#endif
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        thisWindow->resizeSubViews();
        break;
    case WM_DPICHANGED:
        thisWindow->updateDeviceScaleFactor();
        return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

static bool menuItemIsChecked(const MENUITEMINFO& info)
{
    return info.fState & MFS_CHECKED;
}

static void turnOffOtherUserAgents(HMENU menu)
{
    MENUITEMINFO info;
    ::memset(&info, 0x00, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STATE;

    // Must unset the other menu items:
    for (UINT menuToClear = IDM_UA_DEFAULT; menuToClear <= IDM_UA_OTHER; ++menuToClear) {
        if (!::GetMenuItemInfo(menu, menuToClear, FALSE, &info))
            continue;
        if (!menuItemIsChecked(info))
            continue;

        info.fState = MFS_UNCHECKED;
        ::SetMenuItemInfo(menu, menuToClear, FALSE, &info);
    }
}

bool MainWindow::toggleMenuItem(UINT menuID)
{
    HMENU menu = ::GetMenu(hwnd());

    switch (menuID) {
    case IDM_UA_DEFAULT:
    case IDM_UA_SAFARI:
    case IDM_UA_SAFARI_IOS_IPHONE:
    case IDM_UA_SAFARI_IOS_IPAD:
    case IDM_UA_EDGE:
    case IDM_UA_IE_11:
    case IDM_UA_CHROME_MAC:
    case IDM_UA_CHROME_WIN:
    case IDM_UA_FIREFOX_MAC:
    case IDM_UA_FIREFOX_WIN:
        m_browserWindow->setUserAgent(menuID);
        turnOffOtherUserAgents(menu);
        break;
    case IDM_UA_OTHER:
        // The actual user agent string will be set by the custom user agent dialog
        turnOffOtherUserAgents(menu);
        break;
    }

    MENUITEMINFO info = { };
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STATE;

    if (!::GetMenuItemInfo(menu, menuID, FALSE, &info))
        return false;

    BOOL newState = !menuItemIsChecked(info);
    info.fState = (newState) ? MFS_CHECKED : MFS_UNCHECKED;
    ::SetMenuItemInfo(menu, menuID, FALSE, &info);

    m_browserWindow->setPreference(menuID, newState);

    return true;
}

LRESULT CALLBACK EditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CHAR:
        if (wParam == 13) {
            // Enter Key
            ::PostMessage(GetParent(hDlg), static_cast<UINT>(WM_COMMAND), MAKELPARAM(IDC_URL_BAR, 0), 0);
            return 0;
        }
    default:
        return CallWindowProc(DefEditProc, hDlg, message, wParam, lParam);
    }
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK MainWindow::cachesDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainWindow& thisWindow = *reinterpret_cast<MainWindow*>(GetWindowLongPtr(hDlg, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, DWLP_USER, lParam);
        ::SetTimer(hDlg, IDT_UPDATE_STATS, 1000, nullptr);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            ::KillTimer(hDlg, IDT_UPDATE_STATS);
            ::DestroyWindow(hDlg);
            thisWindow.m_hCacheWnd = 0;
            return (INT_PTR)TRUE;
        }
        break;

    case IDT_UPDATE_STATS:
        ::InvalidateRect(hDlg, nullptr, FALSE);
        return (INT_PTR)TRUE;

    case WM_PAINT:
        thisWindow.browserWindow()->updateStatistics(hDlg);
        break;
    }

    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK MainWindow::customUserAgentDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainWindow& thisWindow = *reinterpret_cast<MainWindow*>(GetWindowLongPtr(hDlg, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG: {
        MainWindow& thisWindow = *reinterpret_cast<MainWindow*>(lParam);
        SetWindowLongPtr(hDlg, DWLP_USER, lParam);
        HWND edit = ::GetDlgItem(hDlg, IDC_USER_AGENT_INPUT);
        _bstr_t userAgent;
        userAgent = thisWindow.browserWindow()->userAgent();

        ::SetWindowText(edit, static_cast<LPCTSTR>(userAgent));
        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            HWND edit = ::GetDlgItem(hDlg, IDC_USER_AGENT_INPUT);

            TCHAR buffer[1024];
            int strLen = ::GetWindowText(edit, buffer, 1024);
            buffer[strLen] = 0;

            _bstr_t bstr(buffer);
            if (bstr.length()) {
                thisWindow.browserWindow()->setUserAgent(bstr);
                thisWindow.toggleMenuItem(IDM_UA_OTHER);
            }
        }

        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            ::EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void MainWindow::loadURL(BSTR url)
{
    if (FAILED(m_browserWindow->loadURL(url)))
        return;

    SetFocus(m_browserWindow->hwnd());
}

void MainWindow::onURLBarEnter()
{
    wchar_t strPtr[INTERNET_MAX_URL_LENGTH];
    GetWindowText(m_hURLBarWnd, strPtr, INTERNET_MAX_URL_LENGTH);
    strPtr[INTERNET_MAX_URL_LENGTH - 1] = 0;
    _bstr_t bstr(strPtr);
    loadURL(bstr.GetBSTR());
}

void MainWindow::updateDeviceScaleFactor()
{
    if (m_hURLBarFont)
        ::DeleteObject(m_hURLBarFont);
    auto scaleFactor = WebCore::deviceScaleFactorForWindow(m_hMainWnd);
    int fontHeight = scaleFactor * urlBarHeight * 3 / 4;
    m_hURLBarFont = ::CreateFont(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, L"Times New Roman");
    ::SendMessage(m_hURLBarWnd, static_cast<UINT>(WM_SETFONT), reinterpret_cast<WPARAM>(m_hURLBarFont), TRUE);
}
