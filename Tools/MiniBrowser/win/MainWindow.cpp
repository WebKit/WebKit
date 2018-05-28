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
#include "MiniBrowser.h"
#include "MiniBrowserLibResource.h"

namespace WebCore {
float deviceScaleFactorForWindow(HWND);
}

static constexpr int controlButtonWidth = 24;

extern MainWindow* gMainWindow;
extern MiniBrowser* gMiniBrowser;
extern WNDPROC DefEditProc;
extern WNDPROC DefButtonProc;
extern HINSTANCE hInst;
extern HWND hCacheWnd;

LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK BackButtonProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ForwardButtonProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ReloadButtonProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK CustomUserAgent(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Caches(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AuthDialogProc(HWND, UINT, WPARAM, LPARAM);
void PrintView(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool ToggleMenuItem(HWND hWnd, UINT menuID);

std::wstring MainWindow::s_windowClass;

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

bool MainWindow::init(HINSTANCE hInstance, bool usesLayeredWebView, bool pageLoadTesting)
{
    registerClass(hInstance);

    auto title = loadString(IDS_APP_TITLE);

    m_hMainWnd = CreateWindow(s_windowClass.c_str(), title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInstance, 0);

    if (!m_hMainWnd)
        return false;

    float scaleFactor = WebCore::deviceScaleFactorForWindow(nullptr);
    m_hBackButtonWnd = CreateWindow(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE  | BS_TEXT, 0, 0, 0, 0, m_hMainWnd, 0, hInstance, 0);
    m_hForwardButtonWnd = CreateWindow(L"BUTTON", L">", WS_CHILD | WS_VISIBLE | BS_TEXT, scaleFactor * controlButtonWidth, 0, 0, 0, m_hMainWnd, 0, hInstance, 0);
    m_hURLBarWnd = CreateWindow(L"EDIT", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, scaleFactor * controlButtonWidth * 2, 0, 0, 0, m_hMainWnd, 0, hInstance, 0);

    DefEditProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(m_hURLBarWnd, GWLP_WNDPROC));
    DefButtonProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(m_hBackButtonWnd, GWLP_WNDPROC));
    SetWindowLongPtr(m_hURLBarWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));
    SetWindowLongPtr(m_hBackButtonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BackButtonProc));
    SetWindowLongPtr(m_hForwardButtonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ForwardButtonProc));

    SetFocus(m_hURLBarWnd);

    m_browserWindow = std::make_unique<MiniBrowser>(m_hMainWnd, m_hURLBarWnd, usesLayeredWebView, pageLoadTesting);
    if (!m_browserWindow)
        return false;
    HRESULT hr = m_browserWindow->init();
    if (FAILED(hr))
        return false;

    resizeSubViews();
    return true;
}

void MainWindow::resizeSubViews()
{
    float scaleFactor = WebCore::deviceScaleFactorForWindow(m_hMainWnd);

    RECT rcClient;
    GetClientRect(m_hMainWnd, &rcClient);

    constexpr int urlBarHeight = 24;
    int height = scaleFactor * urlBarHeight;
    int width = scaleFactor * controlButtonWidth;

    MoveWindow(m_hBackButtonWnd, 0, 0, width, height, TRUE);
    MoveWindow(m_hForwardButtonWnd, width, 0, width, height, TRUE);
    MoveWindow(m_hURLBarWnd, width * 2, 0, rcClient.right, height, TRUE);

    ::SendMessage(m_hURLBarWnd, static_cast<UINT>(WM_SETFONT), reinterpret_cast<WPARAM>(m_browserWindow->urlBarFont()), TRUE);

    if (m_browserWindow->usesLayeredWebView() || !m_browserWindow->hwnd())
        return;
    MoveWindow(m_browserWindow->hwnd(), 0, height, rcClient.right, rcClient.bottom - height, TRUE);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        if (wmId >= IDM_HISTORY_LINK0 && wmId <= IDM_HISTORY_LINK9) {
            if (gMiniBrowser)
                gMiniBrowser->navigateToHistory(hWnd, wmId);
            break;
        }
        // Parse the menu selections:
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDM_PRINT:
            PrintView(hWnd, message, wParam, lParam);
            break;
        case IDM_WEB_INSPECTOR:
            if (gMiniBrowser)
                gMiniBrowser->launchInspector();
            break;
        case IDM_CACHES:
            if (!::IsWindow(hCacheWnd)) {
                hCacheWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_CACHES), hWnd, Caches);
                ::ShowWindow(hCacheWnd, SW_SHOW);
            }
            break;
        case IDM_HISTORY_BACKWARD:
        case IDM_HISTORY_FORWARD:
            if (gMiniBrowser)
                gMiniBrowser->navigateForwardOrBackward(hWnd, wmId);
            break;
        case IDM_UA_OTHER:
            if (wmEvent)
                ToggleMenuItem(hWnd, wmId);
            else
                DialogBox(hInst, MAKEINTRESOURCE(IDD_USER_AGENT), hWnd, CustomUserAgent);
            break;
        case IDM_ACTUAL_SIZE:
            if (gMiniBrowser)
                gMiniBrowser->resetZoom();
            break;
        case IDM_ZOOM_IN:
            if (gMiniBrowser)
                gMiniBrowser->zoomIn();
            break;
        case IDM_ZOOM_OUT:
            if (gMiniBrowser)
                gMiniBrowser->zoomOut();
            break;
        case IDM_SHOW_LAYER_TREE:
            if (gMiniBrowser)
                gMiniBrowser->showLayerTree();
            break;
        default:
            if (!ToggleMenuItem(hWnd, wmId))
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        }
        break;
    case WM_DESTROY:
#if USE(CF)
        CFRunLoopStop(CFRunLoopGetMain());
#endif
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (!gMiniBrowser || !gMiniBrowser->hasWebView())
            return DefWindowProc(hWnd, message, wParam, lParam);

        gMainWindow->resizeSubViews();
        break;
    case WM_DPICHANGED:
        if (gMiniBrowser)
            gMiniBrowser->updateDeviceScaleFactor();
        return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
