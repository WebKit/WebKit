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

#ifndef WebFramePolicyListener_h
#define WebFramePolicyListener_h

#include "WebKit.h"
#include <wtf/RefPtr.h>

#include <WebCore/FrameLoaderTypes.h>

namespace WebCore {
    class Frame;
}

class WebFramePolicyListener final : public IWebPolicyDecisionListener, public IWebFormSubmissionListener {
public:
    static WebFramePolicyListener* createInstance(RefPtr<WebCore::Frame>&&);
protected:
    WebFramePolicyListener(RefPtr<WebCore::Frame>&&);
    ~WebFramePolicyListener();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebPolicyDecisionListener
    virtual HRESULT STDMETHODCALLTYPE use();
    virtual HRESULT STDMETHODCALLTYPE download();
    virtual HRESULT STDMETHODCALLTYPE ignore();

    // IWebFormSubmissionListener
    virtual HRESULT STDMETHODCALLTYPE continueSubmit();

    // WebFramePolicyListener
    void receivedPolicyDecision(WebCore::PolicyAction);
    void invalidate();

private:
    ULONG m_refCount { 0 };
    RefPtr<WebCore::Frame> m_frame;
};

#endif
