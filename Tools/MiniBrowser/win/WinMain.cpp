/*
 * Copyright (C) 2006, 2008, 2013-2015 Apple Inc.  All rights reserved.
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

#pragma warning(disable: 4091)

#include "stdafx.h"
#include "Common.h"
#include "MiniBrowserLibResource.h"
#include "MiniBrowserReplace.h"
#include <wtf/win/SoftLinking.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if ENABLE(WEBKIT)
#include "WebKitBrowserWindow.h"
#endif

#if ENABLE(WEBKIT_LEGACY)
#include "WebKitLegacyBrowserWindow.h"
#include <WebKitLegacy/WebKitCOMAPI.h>
#endif

SOFT_LINK_LIBRARY(user32);
SOFT_LINK_OPTIONAL(user32, SetProcessDpiAwarenessContext, BOOL, STDAPICALLTYPE, (DPI_AWARENESS_CONTEXT));

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpstrCmdLine, _In_ int nCmdShow)
{
#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif

    MSG msg { };
    HACCEL hAccelTable;

    INITCOMMONCONTROLSEX InitCtrlEx;

    InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    InitCtrlEx.dwICC  = 0x00004000; // ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&InitCtrlEx);

    auto options = parseCommandLine();

    if (options.useFullDesktop)
        computeFullDesktopFrame();

    // Init COM
    OleInitialize(nullptr);

    if (SetProcessDpiAwarenessContextPtr())
        SetProcessDpiAwarenessContextPtr()(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else
        ::SetProcessDPIAware();

#if !ENABLE(WEBKIT_LEGACY)
    auto factory = WebKitBrowserWindow::create;
#elif !ENABLE(WEBKIT)
    auto factory = WebKitLegacyBrowserWindow::create;
#else
    auto factory = options.windowType == BrowserWindowType::WebKit ? WebKitBrowserWindow::create : WebKitLegacyBrowserWindow::create;
#endif
    auto& mainWindow = MainWindow::create().leakRef();
    HRESULT hr = mainWindow.init(factory, hInst, options.usesLayeredWebView);
    if (FAILED(hr))
        goto exit;

    ShowWindow(mainWindow.hwnd(), nCmdShow);

    hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_MINIBROWSER));

    if (options.requestedURL.length())
        mainWindow.loadURL(options.requestedURL.GetBSTR());
    else
        mainWindow.browserWindow()->loadURL(_bstr_t(defaultURL).GetBSTR());

#pragma warning(disable:4509)

    // Main message loop:
    __try {
#if ENABLE(WEBKIT)
        while (GetMessage(&msg, nullptr, 0, 0)) {
#if USE(CF)
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
#endif
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
#else
        IWebKitMessageLoopPtr messageLoop;

        hr = WebKitCreateInstance(CLSID_WebKitMessageLoop, 0, IID_IWebKitMessageLoop, reinterpret_cast<void**>(&messageLoop.GetInterfacePtr()));
        if (FAILED(hr))
            goto exit;

        messageLoop->run(hAccelTable);
#endif
    } __except(createCrashReport(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) { }

exit:
#if !ENABLE(WEBKIT)
    shutDownWebKit();
#endif
#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif

    // Shut down COM.
    OleUninitialize();

    return static_cast<int>(msg.wParam);
}

extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpstrCmdLine, int nCmdShow)
{
    return wWinMain(hInstance, hPrevInstance, lpstrCmdLine, nCmdShow);
}
