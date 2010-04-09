/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFrame_h
#define WebFrame_h

#include "WebFrameLoaderClient.h"
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/PolicyChecker.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
    class Frame;
    class HTMLFrameOwnerElement;
    class String;
}

namespace WebKit {

class WebPage;

class WebFrame : public RefCounted<WebFrame> {
public:
    static PassRefPtr<WebFrame> createMainFrame(WebPage*);
    static PassRefPtr<WebFrame> createSubframe(WebPage*, const WebCore::String& frameName, WebCore::HTMLFrameOwnerElement*);
    ~WebFrame();

    // Called when the FrameLoaderClient (and therefore the WebCore::Frame) is being torn down.
    void invalidate();

    WebPage* page() const { return m_page; }
    WebCore::Frame* coreFrame() const { return m_coreFrame; }

    uint64_t frameID() const { return m_frameID; }

    uint64_t setUpPolicyListener(WebCore::FramePolicyFunction);
    void invalidatePolicyListener();
    void didReceivePolicyDecision(uint64_t listenerID, WebCore::PolicyAction);

private:
    WebFrame(WebPage*, const WebCore::String& frameName, WebCore::HTMLFrameOwnerElement*);

    WebPage* m_page;
    WebCore::Frame* m_coreFrame;

    uint64_t m_policyListenerID;
    WebCore::FramePolicyFunction m_policyFunction;

    WebFrameLoaderClient m_frameLoaderClient;

    uint64_t m_frameID;
};

} // namespace WebKit

#endif // WebFrame_h
