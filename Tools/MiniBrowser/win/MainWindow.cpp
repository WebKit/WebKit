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
#include "WebKitBrowserWindow.h"
#include <sstream>

namespace WebCore {
float deviceScaleFactorForWindow(HWND);
}

static const wchar_t* kMiniBrowserRegistryKey = L"Software\\WebKit\\MiniBrowser";

static constexpr int kToolbarImageSize = 32;
static constexpr int kToolbarURLBarIndex = 4;
static constexpr int kToolbarProgressIndicatorIndex = 5;

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

bool MainWindow::isInstance(HWND hwnd)
{
    wchar_t buff[64];
    if (!GetClassName(hwnd, buff, _countof(buff)))
        return false;
    return s_windowClass == buff;
}

MainWindow::MainWindow()
{
    s_numInstances++;
}

MainWindow::~MainWindow()
{
    s_numInstances--;
}

Ref<MainWindow> MainWindow::create()
{
    return adoptRef(*new MainWindow());
}

void MainWindow::resetFeatureMenu(BrowserWindow::FeatureType featureType, bool resetsSettingsToDefaults)
{
    auto settingMenu = GetSubMenu(GetMenu(m_hMainWnd), 3);
    int index = featureType == BrowserWindow::FeatureType::Experimental ? 0 : 1;
    auto featureMenu = GetSubMenu(settingMenu, index);
    assert(GetMenuItemID(featureMenu, 0) == (featureType == BrowserWindow::FeatureType::Experimental ? IDM_EXPERIMENTAL_FEATURES_RESET_ALL_TO_DEFAULTS : IDM_INTERNAL_DEBUG_FEATURES_RESET_ALL_TO_DEFAULTS));
    m_browserWindow->resetFeatureMenu(featureType, featureMenu, resetsSettingsToDefaults);
}

void MainWindow::createToolbar(HINSTANCE hInstance)
{
    m_hToolbarWnd = CreateWindowEx(0, TOOLBARCLASSNAME, nullptr, 
        WS_CHILD | WS_BORDER | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS, 0, 0, 0, 0, 
        m_hMainWnd, nullptr, hInstance, nullptr);
        
    if (!m_hToolbarWnd)
        return;

    const int ImageListID = 0;

    HIMAGELIST hImageList;
    hImageList = ImageList_LoadImage(hInstance, MAKEINTRESOURCE(IDB_TOOLBAR), kToolbarImageSize, 0, CLR_DEFAULT, IMAGE_BITMAP, 0);

    SendMessage(m_hToolbarWnd, TB_SETIMAGELIST, ImageListID, reinterpret_cast<LPARAM>(hImageList));
    SendMessage(m_hToolbarWnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS);

    const DWORD buttonStyles = BTNS_AUTOSIZE;

    TBBUTTON tbButtons[] = {
        { MAKELONG(0, ImageListID), IDM_HISTORY_BACKWARD, TBSTATE_ENABLED, buttonStyles | BTNS_DROPDOWN, { }, 0, (INT_PTR)L"Back" },
        { MAKELONG(1, ImageListID), IDM_HISTORY_FORWARD, TBSTATE_ENABLED, buttonStyles | BTNS_DROPDOWN, { }, 0, (INT_PTR)L"Forward"},
        { MAKELONG(2, ImageListID), IDM_RELOAD, TBSTATE_ENABLED, buttonStyles, { }, 0, (INT_PTR)L"Reload"},
        { MAKELONG(3, ImageListID), IDM_GO_HOME, TBSTATE_ENABLED, buttonStyles, { }, 0, (INT_PTR)L"Home"},
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, { }, 0, 0}, // URL bar
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, { }, 0, 0}, // Progress indicator
    };
    ASSERT(tbButtons[kToolbarURLBarIndex].fsStyle == BTNS_SEP);
    ASSERT(tbButtons[kToolbarProgressIndicatorIndex].fsStyle == BTNS_SEP);

    SendMessage(m_hToolbarWnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(m_hToolbarWnd, TB_ADDBUTTONS, _countof(tbButtons), reinterpret_cast<LPARAM>(&tbButtons));
    ShowWindow(m_hToolbarWnd, true);

    m_hURLBarWnd = CreateWindow(L"EDIT", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hToolbarWnd, 0, hInstance, 0);
    m_hProgressIndicator = CreateWindow(L"STATIC", 0, WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 0, 0, 0, 0, m_hToolbarWnd, 0, hInstance, 0);

    DefEditProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(m_hURLBarWnd, GWLP_WNDPROC));
    SetWindowLongPtr(m_hURLBarWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));
}

void MainWindow::resizeToolbar(int parentWidth)
{
    TBBUTTONINFO info { };
    info.cbSize = sizeof(TBBUTTONINFO);
    info.dwMask = TBIF_BYINDEX | TBIF_SIZE;
    info.cx = parentWidth - m_toolbarItemsWidth;
    SendMessage(m_hToolbarWnd, TB_SETBUTTONINFO, kToolbarURLBarIndex, reinterpret_cast<LPARAM>(&info));

    SendMessage(m_hToolbarWnd, TB_AUTOSIZE, 0, 0);

    RECT rect;
    SendMessage(m_hToolbarWnd, TB_GETITEMRECT, kToolbarURLBarIndex, reinterpret_cast<LPARAM>(&rect));
    MoveWindow(m_hURLBarWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, true);

    SendMessage(m_hToolbarWnd, TB_GETITEMRECT, kToolbarProgressIndicatorIndex, reinterpret_cast<LPARAM>(&rect));
    MoveWindow(m_hProgressIndicator, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, true);
}

void MainWindow::rescaleToolbar()
{
    const float scaleFactor = WebCore::deviceScaleFactorForWindow(m_hMainWnd);
    const int scaledImageSize = kToolbarImageSize * scaleFactor;

    TBBUTTONINFO info { };
    info.cbSize = sizeof(TBBUTTONINFO);
    info.dwMask = TBIF_BYINDEX | TBIF_SIZE;
    info.cx = 0;
    SendMessage(m_hToolbarWnd, TB_SETBUTTONINFO, kToolbarURLBarIndex, reinterpret_cast<LPARAM>(&info));
    info.cx = scaledImageSize * 2;
    SendMessage(m_hToolbarWnd, TB_SETBUTTONINFO, kToolbarProgressIndicatorIndex, reinterpret_cast<LPARAM>(&info));

    SendMessage(m_hToolbarWnd, TB_AUTOSIZE, 0, 0);

    int numItems = SendMessage(m_hToolbarWnd, TB_BUTTONCOUNT, 0, 0);

    RECT rect;
    SendMessage(m_hToolbarWnd, TB_GETITEMRECT, numItems-1, reinterpret_cast<LPARAM>(&rect));
    m_toolbarItemsWidth = rect.right;
}

bool MainWindow::init(BrowserWindowFactory factory, HINSTANCE hInstance, bool usesLayeredWebView)
{
    registerClass(hInstance);

    auto title = loadString(IDS_APP_TITLE);

    m_hMainWnd = CreateWindow(s_windowClass.c_str(), title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInstance, this);

    if (!m_hMainWnd)
        return false;

    createToolbar(hInstance);
    if (!m_hToolbarWnd)
        return false;

    m_browserWindow = factory(*this, m_hMainWnd, usesLayeredWebView);
    if (!m_browserWindow)
        return false;
    HRESULT hr = m_browserWindow->init();
    if (FAILED(hr))
        return false;

    resetFeatureMenu(BrowserWindow::FeatureType::Experimental);
    resetFeatureMenu(BrowserWindow::FeatureType::InternalDebug);
    
    updateDeviceScaleFactor();
    resizeSubViews();
    SetFocus(m_hURLBarWnd);
    return true;
}

void MainWindow::resizeSubViews()
{
    RECT rcClient;
    GetClientRect(m_hMainWnd, &rcClient);

    resizeToolbar(rcClient.right);

    if (m_browserWindow->usesLayeredWebView() || !m_browserWindow->hwnd())
        return;

    RECT rect;
    GetWindowRect(m_hToolbarWnd, &rect);
    POINT toolbarBottom = { 0, rect.bottom };
    ScreenToClient(m_hMainWnd, &toolbarBottom);
    auto height = toolbarBottom.y;
    MoveWindow(m_browserWindow->hwnd(), 0, height, rcClient.right, rcClient.bottom - height, true);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    RefPtr<MainWindow> thisWindow = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    switch (message) {
    case WM_ACTIVATE:
        switch (LOWORD(wParam)) {
        case WA_ACTIVE:
        case WA_CLICKACTIVE:
            SetFocus(thisWindow->browserWindow()->hwnd());
        }
        break;
    case WM_CREATE:
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
        break;
    case WM_APPCOMMAND: {
        auto cmd = GET_APPCOMMAND_LPARAM(lParam);
        switch (cmd) {
        case APPCOMMAND_BROWSER_BACKWARD:
            thisWindow->browserWindow()->navigateForwardOrBackward(false);
            result = 1;
            break;
        case APPCOMMAND_BROWSER_FORWARD:
            thisWindow->browserWindow()->navigateForwardOrBackward(true);
            result = 1;
            break;
        case APPCOMMAND_BROWSER_HOME:
            thisWindow->goHome();
            break;
        case APPCOMMAND_BROWSER_REFRESH:
            thisWindow->browserWindow()->reload();
            result = 1;
            break;
        case APPCOMMAND_BROWSER_STOP:
            break;
        }
        break;
    }
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
            auto& newWindow = MainWindow::create().leakRef();
            newWindow.init(WebKitBrowserWindow::create, hInst);
            ShowWindow(newWindow.hwnd(), SW_SHOW);
            break;
        }
        case IDM_CLOSE_WINDOW:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_GO_HOME:
            thisWindow->goHome();
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
        case IDM_EXPERIMENTAL_FEATURES_RESET_ALL_TO_DEFAULTS:
            thisWindow->resetFeatureMenu(BrowserWindow::FeatureType::Experimental, true);
            break;
        case IDM_INTERNAL_DEBUG_FEATURES_RESET_ALL_TO_DEFAULTS:
            thisWindow->resetFeatureMenu(BrowserWindow::FeatureType::InternalDebug, true);
            break;
        case IDM_SET_DEFAULT_URL:
            thisWindow->setDefaultURLToCurrentURL();
            break;
        case IDM_CACHES:
            if (!::IsWindow(thisWindow->m_hCacheWnd)) {
                thisWindow->m_hCacheWnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_CACHES), hWnd, cachesDialogProc, reinterpret_cast<LPARAM>(thisWindow.get()));
                ::ShowWindow(thisWindow->m_hCacheWnd, SW_SHOW);
            }
            break;
        case IDM_HISTORY_BACKWARD:
        case IDM_HISTORY_FORWARD:
            thisWindow->browserWindow()->navigateForwardOrBackward(wmId == IDM_HISTORY_FORWARD);
            break;
        case IDM_UA_OTHER:
            DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_USER_AGENT), hWnd, customUserAgentDialogProc, reinterpret_cast<LPARAM>(thisWindow.get()));
            break;
        case IDM_ACTUAL_SIZE:
            thisWindow->browserWindow()->resetZoom();
            break;
        case IDM_RELOAD:
            thisWindow->browserWindow()->reload();
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
        case IDM_CLEAR_COOKIES:
            thisWindow->browserWindow()->clearCookies();
            break;
        case IDM_CLEAR_WEBSITE_DATA:
            thisWindow->browserWindow()->clearWebsiteData();
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
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        thisWindow->resizeSubViews();
        break;
    case WM_DPICHANGED: {
        thisWindow->updateDeviceScaleFactor();
        thisWindow->browserWindow()->adjustScaleFactors();
        auto& rect = *reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hWnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return result;
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

void MainWindow::setDefaultURLToCurrentURL()
{
    wchar_t url[INTERNET_MAX_URL_LENGTH];
    GetWindowText(m_hURLBarWnd, url, INTERNET_MAX_URL_LENGTH);
    auto length = wcslen(url);
    if (!length)
        return;
    RegSetKeyValue(HKEY_CURRENT_USER, kMiniBrowserRegistryKey, L"DefaultURL", REG_SZ, url, (length + 1) * sizeof(wchar_t));
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

LRESULT CALLBACK EditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_SETFOCUS:
        PostMessage(hWnd, EM_SETSEL, 0, -1);
        break;
    case WM_CHAR:
        if (wParam == 13) {
            // Enter Key
            ::PostMessage(GetParent(hWnd), static_cast<UINT>(WM_COMMAND), MAKELPARAM(IDC_URL_BAR, 0), 0);
            return 0;
        }
        break;
    }
    return CallWindowProc(DefEditProc, hWnd, message, wParam, lParam);
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

void MainWindow::loadURL(std::wstring url)
{
    if (::PathFileExists(url.c_str()) || ::PathIsUNC(url.c_str())) {
        wchar_t fileURL[INTERNET_MAX_URL_LENGTH];
        DWORD fileURLLength = _countof(fileURL);

        if (SUCCEEDED(::UrlCreateFromPath(url.c_str(), fileURL, &fileURLLength, 0)))
            url = fileURL;
    }
    if (url.find(L"://") == url.npos)
        url = L"http://" + url;

    if (FAILED(m_browserWindow->loadURL(_bstr_t(url.c_str()))))
        return;

    SetFocus(m_browserWindow->hwnd());
}

void MainWindow::goHome()
{
    std::wstring defaultURL = L"https://www.webkit.org/";
    wchar_t url[INTERNET_MAX_URL_LENGTH];
    DWORD urlLength = sizeof(url);
    if (!RegGetValue(HKEY_CURRENT_USER, kMiniBrowserRegistryKey, L"DefaultURL", RRF_RT_REG_SZ, nullptr, &url, &urlLength))
        defaultURL = url;
    loadURL(defaultURL);
}

void MainWindow::onURLBarEnter()
{
    wchar_t url[INTERNET_MAX_URL_LENGTH];
    GetWindowText(m_hURLBarWnd, url, INTERNET_MAX_URL_LENGTH);
    loadURL(url);
}

void MainWindow::updateDeviceScaleFactor()
{
    if (m_hURLBarFont)
        ::DeleteObject(m_hURLBarFont);

    rescaleToolbar();

    RECT rect;
    GetClientRect(m_hToolbarWnd, &rect);
    int fontHeight = rect.bottom * 3. / 4;

    m_hURLBarFont = ::CreateFont(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"Tahoma");
    ::SendMessage(m_hURLBarWnd, static_cast<UINT>(WM_SETFONT), reinterpret_cast<WPARAM>(m_hURLBarFont), TRUE);
}

void MainWindow::progressChanged(double progress)
{
    std::wostringstream text;
    text << static_cast<int>(progress * 100) << L'%';
    SetWindowText(m_hProgressIndicator, text.str().c_str());
}

void MainWindow::progressFinished()
{
    SetWindowText(m_hProgressIndicator, L"");
}

void MainWindow::activeURLChanged(std::wstring url)
{
    SetWindowText(m_hURLBarWnd, url.c_str());
}
