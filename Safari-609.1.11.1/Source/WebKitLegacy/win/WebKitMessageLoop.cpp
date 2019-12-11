/*
* Copyright (C) 2015 Apple Inc.  All rights reserved.
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

#include "WebKitMessageLoop.h"

#include "WebKitDLL.h"

#if USE(GLIB)
#include <glib.h>
#endif

WebKitMessageLoop::WebKitMessageLoop()
{
    gClassCount++;
    gClassNameCount().add("WebKitMessageLoop");
}

WebKitMessageLoop::~WebKitMessageLoop()
{
    gClassCount--;
    gClassNameCount().remove("WebKitMessageLoop");
}

WebKitMessageLoop* WebKitMessageLoop::createInstance()
{
    WebKitMessageLoop* instance = new WebKitMessageLoop();
    instance->AddRef();
    return instance;
}

HRESULT WebKitMessageLoop::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebKitMessageLoop*>(this);
    else if (IsEqualGUID(riid, CLSID_WebKitMessageLoop))
        *ppvObject = static_cast<WebKitMessageLoop*>(this);
    else if (IsEqualGUID(riid, IID_IWebKitMessageLoop))
        *ppvObject = static_cast<IWebKitMessageLoop*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebKitMessageLoop::AddRef()
{
    return ++m_refCount;
}

ULONG WebKitMessageLoop::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT WebKitMessageLoop::run(_In_ HACCEL hAccelTable)
{
    MSG msg { };

    while (GetMessage(&msg, 0, 0, 0)) {
        performMessageLoopTasks();

        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return S_OK;
}

HRESULT WebKitMessageLoop::performMessageLoopTasks()
{
#if USE(CF)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
#endif
#if USE(GLIB)
    g_main_context_iteration(0, false);
#endif
    return S_OK;
}

