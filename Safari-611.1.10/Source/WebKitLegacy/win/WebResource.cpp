/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WebResource.h"

#include "MarshallingHelpers.h"
#include "MemoryStream.h"
#include <WebCore/BString.h>

using namespace WebCore;

// WebResource ---------------------------------------------------------------------

WebResource::WebResource(IStream* data, const WTF::URL& url, const WTF::String& mimeType, const WTF::String& textEncodingName, const WTF::String& frameName)
    : m_data(data)
    , m_url(url)
    , m_mimeType(mimeType)
    , m_textEncodingName(textEncodingName)
    , m_frameName(frameName)
{
    gClassCount++;
    gClassNameCount().add("WebResource");
}

WebResource::~WebResource()
{
    gClassCount--;
    gClassNameCount().remove("WebResource");
}

WebResource* WebResource::createInstance(RefPtr<WebCore::SharedBuffer>&& data, const WebCore::ResourceResponse& response)
{
    COMPtr<MemoryStream> memoryStream = MemoryStream::createInstance(WTFMove(data));

    WebResource* instance = new WebResource(memoryStream.get(), response.url(), response.mimeType(), response.textEncodingName(), String());
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebResource::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, IID_IWebResource))
        *ppvObject = static_cast<IWebResource*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebResource::AddRef()
{
    return ++m_refCount;
}

ULONG WebResource::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// WebResource ------------------------------------------------------------------

HRESULT WebResource::initWithData(_In_opt_ IStream* data, _In_ BSTR url, _In_ BSTR mimeType, _In_ BSTR textEncodingName, _In_ BSTR frameName)
{
    m_data = data;
    m_url = MarshallingHelpers::BSTRToKURL(url);
    m_mimeType = String(mimeType);
    m_textEncodingName = String(textEncodingName);
    m_frameName = String(frameName);

    return S_OK;
}

HRESULT WebResource::data(_COM_Outptr_opt_ IStream** data)
{
    if (!data)
        return E_POINTER;
    *data = nullptr;
    return m_data.copyRefTo(data);
}
   
HRESULT WebResource::URL(__deref_opt_out BSTR* url)
{
    if (!url) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *url = BString(String(m_url.string())).release();
    return S_OK;
}
    
HRESULT WebResource::MIMEType(__deref_opt_out BSTR* mime)
{
    if (!mime) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *mime = BString(m_mimeType).release();
    return S_OK;
}
   
HRESULT WebResource::textEncodingName(__deref_opt_out BSTR* encodingName)
{
    if (!encodingName) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *encodingName = BString(m_textEncodingName).release();
    return S_OK;
}
    
HRESULT WebResource::frameName(__deref_opt_out BSTR* name)
{
    if (!name) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *name = BString(m_frameName).release();
    return S_OK;
}
