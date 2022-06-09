/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebKitDLL.h"
#include "WebWorkersPrivate.h"

#include <WebCore/WorkerThread.h>

// IUnknown -------------------------------------------------------------------

HRESULT WebWorkersPrivate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, __uuidof(IWebWorkersPrivate)))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebWorkersPrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebWorkersPrivate))
        *ppvObject = static_cast<IWebWorkersPrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebWorkersPrivate::AddRef()
{
    return ++m_refCount;
}

ULONG WebWorkersPrivate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebWorkersPrivate ---------------------------------------------------------

HRESULT WebWorkersPrivate::workerThreadCount(_Out_ UINT* number)
{
    if (!number)
        return E_POINTER;

    *number = WebCore::WorkerThread::workerThreadCount();
    return S_OK;
}

// WebWorkersPrivate ----------------------------------------------------------

WebWorkersPrivate* WebWorkersPrivate::createInstance()
{
    WebWorkersPrivate* instance = new WebWorkersPrivate();
    instance->AddRef();
    return instance;
}

WebWorkersPrivate::WebWorkersPrivate()
{
    gClassCount++;
    gClassNameCount().add("WebWorkersPrivate");
}

WebWorkersPrivate::~WebWorkersPrivate()
{
    gClassCount--;
    gClassNameCount().remove("WebWorkersPrivate");
}
