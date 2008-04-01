/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "WebKitDLL.h"
#include "WebArchive.h"

#include <WebCore/LegacyWebArchive.h>

using namespace WebCore;

// WebArchive ----------------------------------------------------------------
    
WebArchive* WebArchive::createInstance(PassRefPtr<LegacyWebArchive> coreArchive)
{
    WebArchive* instance = new WebArchive(coreArchive);

    instance->AddRef();
    return instance;
}

WebArchive::WebArchive(PassRefPtr<LegacyWebArchive> coreArchive)
    : m_refCount(0)
    , m_archive(coreArchive)
{
    gClassCount++;
}

WebArchive::~WebArchive()
{
    gClassCount--;
}

HRESULT STDMETHODCALLTYPE WebArchive::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebArchive*>(this);
    else if (IsEqualGUID(riid, __uuidof(IWebArchive)))
        *ppvObject = static_cast<IWebArchive*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebArchive::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebArchive::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT STDMETHODCALLTYPE WebArchive::initWithMainResource(
        /* [in] */ IWebResource*, 
        /* [in, size_is(cSubResources)] */ IWebResource**, 
        /* [in] */ int, 
        /* in, size_is(cSubFrameArchives)] */ IWebArchive**, 
        /* [in] */ int)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebArchive::initWithData(
        /* [in] */ IStream*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebArchive::mainResource(
        /* [out, retval] */ IWebResource**)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebArchive::subResources(
        /* [out, retval] */ IEnumVARIANT**)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebArchive::subframeArchives(
        /* [out, retval] */ IEnumVARIANT**)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebArchive::data(
        /* [out, retval] */ IStream**)
{
    return E_NOTIMPL;
}
