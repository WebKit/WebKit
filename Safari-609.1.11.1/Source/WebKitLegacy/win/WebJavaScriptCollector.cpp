/*
 * Copyright (C) 2007-2008, 2015 Apple Inc. All rights reserved.
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

#include "WebKitDLL.h"
#include "WebJavaScriptCollector.h"

#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VM.h>
#include <WebCore/CommonVM.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/GCController.h>
#include <WebCore/JSDOMWindow.h>

using namespace JSC;
using namespace WebCore;

// WebJavaScriptCollector ---------------------------------------------------------------------------

WebJavaScriptCollector::WebJavaScriptCollector()
{
    gClassCount++;
    gClassNameCount().add("WebJavaScriptCollector");
}

WebJavaScriptCollector::~WebJavaScriptCollector()
{
    gClassCount--;
    gClassNameCount().remove("WebJavaScriptCollector");
}

WebJavaScriptCollector* WebJavaScriptCollector::createInstance()
{
    WebJavaScriptCollector* instance = new WebJavaScriptCollector();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebJavaScriptCollector::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebJavaScriptCollector*>(this);
    else if (IsEqualGUID(riid, IID_IWebJavaScriptCollector))
        *ppvObject = static_cast<IWebJavaScriptCollector*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebJavaScriptCollector::AddRef()
{
    return ++m_refCount;
}

ULONG WebJavaScriptCollector::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebJavaScriptCollector ------------------------------------------------------------------------------

HRESULT WebJavaScriptCollector::collect()
{
    GCController::singleton().garbageCollectNow();
    return S_OK;
}

HRESULT WebJavaScriptCollector::collectOnAlternateThread(BOOL waitUntilDone)
{
    GCController::singleton().garbageCollectOnAlternateThreadForDebugging(!!waitUntilDone);
    return S_OK;
}

HRESULT WebJavaScriptCollector::objectCount(_Out_ UINT* count)
{
    if (!count) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    JSLockHolder lock(commonVM());
    *count = (UINT)commonVM().heap.objectCount();
    return S_OK;
}
