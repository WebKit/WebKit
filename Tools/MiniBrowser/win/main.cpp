/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdafx.h"

#include "BrowserWindow.h"
#include "MiniBrowser.h"
#include <string>

static bool shouldTranslateMessage(const MSG& msg)
{
    // Only these four messages are actually translated by ::TranslateMessage or ::TranslateAccelerator.
    // It's useless (though harmless) to call those functions for other messages, so we always allow other messages to be translated.
    if (msg.message != WM_KEYDOWN && msg.message != WM_SYSKEYDOWN && msg.message != WM_KEYUP && msg.message != WM_SYSKEYUP)
        return true;
    
    wchar_t className[256];
    if (!::GetClassNameW(msg.hwnd, className, ARRAYSIZE(className)))
        return true;

    // Don't call TranslateMessage() on key events destined for a WebKit2 view, WebKit will do this if it doesn't handle the message.
    // It would be nice to use some API here instead of hard-coding the window class name.
    return wcscmp(className, L"WebKit2WebViewWindowClass");
}

BOOL WINAPI DllMain(HINSTANCE dllInstance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        MiniBrowser::shared().initialize(dllInstance);

    return TRUE;
}

extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpstrCmdLine, int nCmdShow)
{
    // Create and show our initial window.
    MiniBrowser::shared().createNewWindow();

    MSG message;
    while (BOOL result = ::GetMessage(&message, 0, 0, 0)) {
        if (result == -1)
            break;
        
        if (shouldTranslateMessage(message))
            ::TranslateMessage(&message);

        if (!MiniBrowser::shared().handleMessage(&message))
            ::DispatchMessage(&message);
    }

    return 0;
}
