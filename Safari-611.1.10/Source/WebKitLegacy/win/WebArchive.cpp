/*
 * Copyright (C) 2008, 2015 Apple Inc. All Rights Reserved.
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
#include "WebArchive.h"

#include "DOMCoreClasses.h"
#include "MemoryStream.h"

#if USE(CF)
#include <WebCore/LegacyWebArchive.h>
#endif

using namespace WebCore;

// WebArchive ----------------------------------------------------------------

WebArchive* WebArchive::createInstance()
{
#if USE(CF)
    WebArchive* instance = new WebArchive(0);
    instance->AddRef();
    return instance;
#else
    return nullptr;
#endif
}

WebArchive* WebArchive::createInstance(RefPtr<LegacyWebArchive>&& coreArchive)
{
    WebArchive* instance = new WebArchive(WTFMove(coreArchive));

    instance->AddRef();
    return instance;
}

WebArchive::WebArchive(RefPtr<LegacyWebArchive>&& coreArchive)
#if USE(CF)
    : m_archive(WTFMove(coreArchive))
#endif
{
    gClassCount++;
    gClassNameCount().add("WebArchive");
}

WebArchive::~WebArchive()
{
    gClassCount--;
    gClassNameCount().remove("WebArchive");
}

HRESULT WebArchive::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebArchive*>(this);
    else if (IsEqualGUID(riid, __uuidof(IWebArchive)))
        *ppvObject = static_cast<IWebArchive*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebArchive::AddRef()
{
    return ++m_refCount;
}

ULONG WebArchive::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT WebArchive::initWithMainResource(_In_opt_ IWebResource*, 
    __inout_ecount_full(cSubResources) IWebResource**, int cSubResources,
    __inout_ecount_full(cSubFrameArchives) IWebArchive**, int cSubFrameArchives)
{
    return E_NOTIMPL;
}

HRESULT WebArchive::initWithData(_In_opt_ IStream*)
{
    return E_NOTIMPL;
}

HRESULT WebArchive::initWithNode(_In_opt_ IDOMNode* node)
{
    if (!node)
        return E_POINTER;

    COMPtr<DOMNode> domNode(Query, node);
    if (!domNode)
        return E_NOINTERFACE;

#if USE(CF)
    m_archive = LegacyWebArchive::create(*domNode->node());
#endif

    return S_OK;
}

HRESULT WebArchive::mainResource(_COM_Outptr_opt_ IWebResource** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT WebArchive::subResources(_COM_Outptr_opt_ IEnumVARIANT** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT WebArchive::subframeArchives(_COM_Outptr_opt_ IEnumVARIANT** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT WebArchive::data(_COM_Outptr_opt_ IStream** stream)
{
    if (!stream)
        return E_POINTER;

#if USE(CF)
    *stream = nullptr;
    RetainPtr<CFDataRef> cfData = m_archive->rawDataRepresentation();
    if (!cfData)
        return E_FAIL;

    auto buffer = SharedBuffer::create(CFDataGetBytePtr(cfData.get()), CFDataGetLength(cfData.get()));

    return MemoryStream::createInstance(WTFMove(buffer)).copyRefTo(stream);
#else
    UNUSED_PARAM(stream);
    return E_FAIL;
#endif
}
