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

#include <shlwapi.h>
#include <windows.h>

#if defined _M_IX86
#define PROCESSORARCHITECTURE "x86"
#elif defined _M_IA64
#define PROCESSORARCHITECTURE "ia64"
#elif defined _M_X64
#define PROCESSORARCHITECTURE "amd64"
#else
#define PROCESSORARCHITECTURE "*"
#endif

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='" PROCESSORARCHITECTURE "' publicKeyToken='6595b64144ccf1df' language='*'\"")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpstrCmdLine, int nCmdShow)
{
#ifndef DEBUG_ALL
    LPCWSTR webKitDLLName = L"WebKit.dll";
#else
    LPCTSTR webKitDLLName = L"WebKit_debug.dll";
#endif

    WCHAR webKitPath[MAX_PATH];
    ::GetModuleFileNameW(0, webKitPath, ARRAYSIZE(webKitPath));
    ::PathRemoveFileSpecW(webKitPath);

    // Look for DLLs in the same directory as WebKit2WebProcess.exe. This is not in the search
    // path already, since we launch WebKit2WebProcess.exe via CreateProcess with lpCurrentDirectory
    // set to 0. We want both the WebKit client app DLL path and the WebKit directory DLL path in
    // the DLL search order, and we want the current directory set to the WebKit client app path.
    ::SetDllDirectoryW(webKitPath);

    ::PathAppendW(webKitPath, webKitDLLName);
    HMODULE module = ::LoadLibraryW(webKitPath);
    typedef int (__cdecl* WebKitMainProcPtr)(HINSTANCE, HINSTANCE, LPTSTR, int);
    WebKitMainProcPtr mainProc = reinterpret_cast<WebKitMainProcPtr>(GetProcAddress(module, "WebKitMain"));
    if (!mainProc)
        return 0;

    return mainProc(hInstance, hPrevInstance, lpstrCmdLine, nCmdShow);
}
