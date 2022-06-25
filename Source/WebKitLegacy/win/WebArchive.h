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

#ifndef WebArchive_h
#define WebArchive_h

#include "WebKit.h"

#include <wtf/RefPtr.h>

namespace WebCore {
    class LegacyWebArchive;
}

class WebArchive final : public IWebArchive
{
public:
    static WebArchive* createInstance();
    static WebArchive* createInstance(RefPtr<WebCore::LegacyWebArchive>&&);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebArchive
    virtual HRESULT STDMETHODCALLTYPE initWithMainResource(_In_opt_ IWebResource* mainResource, 
        __inout_ecount_full(cSubResources) IWebResource** subResources, int cSubResources, 
        __inout_ecount_full(cSubFrameArchives) IWebArchive** subFrameArchives, int cSubFrameArchives);
    virtual HRESULT STDMETHODCALLTYPE  initWithData(_In_opt_ IStream*);
    virtual HRESULT STDMETHODCALLTYPE  initWithNode(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE  mainResource(_COM_Outptr_opt_ IWebResource**);
    virtual HRESULT STDMETHODCALLTYPE  subResources(_COM_Outptr_opt_ IEnumVARIANT**);
    virtual HRESULT STDMETHODCALLTYPE  subframeArchives(_COM_Outptr_opt_ IEnumVARIANT**);
    virtual HRESULT STDMETHODCALLTYPE  data(_COM_Outptr_opt_ IStream**);

private:
    WebArchive(RefPtr<WebCore::LegacyWebArchive>&&);
    ~WebArchive();

    ULONG m_refCount { 0 };
#if USE(CF)
    RefPtr<WebCore::LegacyWebArchive> m_archive;
#endif
};

#endif // WebArchive_h
