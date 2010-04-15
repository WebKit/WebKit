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

#ifndef WebFrameProxy_h
#define WebFrameProxy_h

#include "WebFramePolicyListenerProxy.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/PlatformString.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class String;
}

namespace WebKit {

class WebPageProxy;

class WebFrameProxy : public RefCounted<WebFrameProxy> {
public:
    static PassRefPtr<WebFrameProxy> create(WebPageProxy* page, uint64_t frameID)
    {
        return adoptRef(new WebFrameProxy(page, frameID));
    }
    ~WebFrameProxy();

    enum LoadState {
        LoadStateProvisional,
        LoadStateCommitted,
        LoadStateFinished
    };

    uint64_t frameID() const { return m_frameID; }

    void disconnect();

    bool isMainFrame() const;
    LoadState loadState() const { return m_loadState; }

    const WebCore::String& url() const { return m_url; }
    const WebCore::String& provisionalURL() const { return m_provisionalURL; }

    void didStartProvisionalLoad(const WebCore::String& url);
    void didCommitLoad();
    void didFinishLoad();
    void didReceiveTitle(const WebCore::String&);

    void receivedPolicyDecision(WebCore::PolicyAction, uint64_t listenerID);
    WebFramePolicyListenerProxy* setUpPolicyListenerProxy(uint64_t listenerID);

private:
    WebFrameProxy(WebPageProxy* page, uint64_t frameID);

    WebPageProxy* m_page;
    LoadState m_loadState;
    WebCore::String m_url;
    WebCore::String m_provisionalURL;
    RefPtr<WebFramePolicyListenerProxy> m_policyListener;
    uint64_t m_frameID;
};

} // namespace WebKit

#endif // WebFrameProxy_h
