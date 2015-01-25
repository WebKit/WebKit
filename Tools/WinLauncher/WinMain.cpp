/*
 * Copyright (C) 2006, 2008, 2013, 2014 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2011 Brent Fulgham.  All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Appcelerator, Inc. All rights reserved.
 * Copyright (C) 2013 Alex Christensen. All rights reserved.
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
#include "WinLauncherLibResource.h"
#include "WinLauncherWebHost.h"
#include "Common.cpp"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif

    MSG msg = {0};
    HACCEL hAccelTable;

    INITCOMMONCONTROLSEX InitCtrlEx;

    InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    InitCtrlEx.dwICC  = 0x00004000; // ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&InitCtrlEx);

    bool usesLayeredWebView = false;
    bool useFullDesktop = false;
    bool pageLoadTesting = false;
    _bstr_t requestedURL;

    parseCommandLine(usesLayeredWebView, useFullDesktop, pageLoadTesting, requestedURL);

    // Initialize global strings
    LoadString(hInst, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInst, IDC_WINLAUNCHER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInst);

    if (useFullDesktop)
        computeFullDesktopFrame();

    // Init COM
    OleInitialize(nullptr);

    if (usesLayeredWebView) {
        hURLBarWnd = CreateWindow(L"EDIT", L"Type URL Here",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, 
            s_windowPosition.x, s_windowPosition.y + s_windowSize.cy, s_windowSize.cx, URLBAR_HEIGHT,
            0, 0, hInst, 0);
    } else {
        hMainWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInst, 0);

        if (!hMainWnd)
            return FALSE;

        hBackButtonWnd = CreateWindow(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE  | BS_TEXT, 0, 0, 0, 0, hMainWnd, 0, hInst, 0);
        hForwardButtonWnd = CreateWindow(L"BUTTON", L">", WS_CHILD | WS_VISIBLE  | BS_TEXT, CONTROLBUTTON_WIDTH, 0, 0, 0, hMainWnd, 0, hInst, 0);
        hURLBarWnd = CreateWindow(L"EDIT", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, CONTROLBUTTON_WIDTH * 2, 0, 0, 0, hMainWnd, 0, hInst, 0);

        ShowWindow(hMainWnd, nCmdShow);
        UpdateWindow(hMainWnd);
    }

    DefEditProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hURLBarWnd, GWLP_WNDPROC));
    DefButtonProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hBackButtonWnd, GWLP_WNDPROC));
    SetWindowLongPtr(hURLBarWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));
    SetWindowLongPtr(hBackButtonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BackButtonProc));
    SetWindowLongPtr(hForwardButtonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ForwardButtonProc));

    SetFocus(hURLBarWnd);

    RECT clientRect = { s_windowPosition.x, s_windowPosition.y, s_windowPosition.x + s_windowSize.cx, s_windowPosition.y + s_windowSize.cy };

    WinLauncherWebHost* webHost = nullptr;

    gWinLauncher = new WinLauncher(hMainWnd, hURLBarWnd, usesLayeredWebView, pageLoadTesting);
    if (!gWinLauncher)
        goto exit;

    if (!gWinLauncher->seedInitialDefaultPreferences())
        goto exit;

    if (!gWinLauncher->setToDefaultPreferences())
        goto exit;

    HRESULT hr = gWinLauncher->init();
    if (FAILED(hr))
        goto exit;

    if (!setCacheFolder())
        goto exit;

    webHost = new WinLauncherWebHost(gWinLauncher, hURLBarWnd);

    hr = gWinLauncher->setFrameLoadDelegate(webHost);
    if (FAILED(hr))
        goto exit;

    hr = gWinLauncher->setFrameLoadDelegatePrivate(webHost);
    if (FAILED(hr))
        goto exit;

    hr = gWinLauncher->setUIDelegate(new PrintWebUIDelegate());
    if (FAILED (hr))
        goto exit;

    hr = gWinLauncher->setAccessibilityDelegate(new AccessibilityDelegate());
    if (FAILED (hr))
        goto exit;

    hr = gWinLauncher->setResourceLoadDelegate(new ResourceLoadDelegate(gWinLauncher));
    if (FAILED(hr))
        goto exit;

    hr = gWinLauncher->prepareViews(hMainWnd, clientRect, requestedURL.GetBSTR(), gViewWindow);
    if (FAILED(hr) || !gViewWindow)
        goto exit;

    if (gWinLauncher->usesLayeredWebView())
        subclassForLayeredWindow();

    resizeSubViews();

    ShowWindow(gViewWindow, nCmdShow);
    UpdateWindow(gViewWindow);

    hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_WINLAUNCHER));

    if (requestedURL.length())
        loadURL(requestedURL.GetBSTR());

#pragma warning(disable:4509)

    // Main message loop:
    __try {
        _com_ptr_t<_com_IIID<IWebKitMessageLoop, &__uuidof(IWebKitMessageLoop)>> messageLoop;

        hr = WebKitCreateInstance(CLSID_WebKitMessageLoop, 0, IID_IWebKitMessageLoop, reinterpret_cast<void**>(&messageLoop.GetInterfacePtr()));
        if (FAILED(hr))
            goto exit;

        messageLoop->run(hAccelTable);

    } __except(createCrashReport(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) { }

exit:
    shutDownWebKit();
#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif

    // Shut down COM.
    OleUninitialize();

    delete gWinLauncher;
    
    return static_cast<int>(msg.wParam);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINLAUNCHER));
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_WINLAUNCHER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}
