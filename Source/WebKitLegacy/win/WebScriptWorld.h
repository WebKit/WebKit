/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebScriptWorld_h
#define WebScriptWorld_h

#include "WebKit.h"
#include <WebCore/COMPtr.h>

namespace WebCore {
    class DOMWrapperWorld;
}

class WebScriptWorld final : public IWebScriptWorld {
    WTF_MAKE_NONCOPYABLE(WebScriptWorld);
public:
    static WebScriptWorld* standardWorld();
    static COMPtr<WebScriptWorld> createInstance();

    static COMPtr<WebScriptWorld> findOrCreateWorld(WebCore::DOMWrapperWorld&);

    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    WebCore::DOMWrapperWorld& world() const { return *m_world; }

private:
    static COMPtr<WebScriptWorld> createInstance(RefPtr<WebCore::DOMWrapperWorld>&&);

    WebScriptWorld(RefPtr<WebCore::DOMWrapperWorld>&&);
    ~WebScriptWorld();

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID, _COM_Outptr_ void** ppvObject);
    virtual HRESULT STDMETHODCALLTYPE standardWorld(_COM_Outptr_opt_ IWebScriptWorld**);
    virtual HRESULT STDMETHODCALLTYPE scriptWorldForGlobalContext(JSGlobalContextRef, IWebScriptWorld**);
    virtual HRESULT STDMETHODCALLTYPE unregisterWorld();

    ULONG m_refCount { 0 };
    RefPtr<WebCore::DOMWrapperWorld> m_world;
};

#endif // WebScriptWorld_h
