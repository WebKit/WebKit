/*
 * Copyright (C) 2007, 2015 Apple Inc.  All rights reserved.
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

#ifndef WebResource_h
#define WebResource_h

#include "WebKit.h"
#include <WebCore/COMPtr.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

class WebResource : public IWebResource {
public:
    static WebResource* createInstance(RefPtr<WebCore::SharedBuffer>&&, const WebCore::ResourceResponse&);
protected:
    WebResource(IStream* data, const WTF::URL& url, const WTF::String& mimeType, const WTF::String& textEncodingName, const WTF::String& frameName);
    ~WebResource();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebResource
    virtual HRESULT STDMETHODCALLTYPE initWithData(_In_opt_ IStream* data, _In_ BSTR url, _In_ BSTR mimeType, _In_ BSTR textEncodingName, _In_ BSTR frameName);
    virtual HRESULT STDMETHODCALLTYPE data(_COM_Outptr_opt_ IStream** data);
    virtual HRESULT STDMETHODCALLTYPE URL(__deref_opt_out BSTR* url);
    virtual HRESULT STDMETHODCALLTYPE MIMEType(__deref_opt_out BSTR* mime);
    virtual HRESULT STDMETHODCALLTYPE textEncodingName(__deref_opt_out BSTR* encodingName);
    virtual HRESULT STDMETHODCALLTYPE frameName(__deref_opt_out BSTR* name);

private:
    ULONG m_refCount { 0 };
    COMPtr<IStream> m_data;
    WTF::URL m_url;
    WTF::String m_mimeType;
    WTF::String m_textEncodingName;
    WTF::String m_frameName;
};

#endif
