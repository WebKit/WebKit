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

#include "APIObject.h"
#include "WebFrameListenerProxy.h"
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebKit {

class PlatformCertificateInfo;
class WebCertificateInfo;
class WebFormSubmissionListenerProxy;
class WebFramePolicyListenerProxy;
class WebPageProxy;

class WebFrameProxy : public APIObject {
public:
    static const Type APIType = TypeFrame;

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
    WebPageProxy* page() { return m_page; }

    void disconnect();

    bool isMainFrame() const;
    LoadState loadState() const { return m_loadState; }

    const WTF::String& url() const { return m_url; }
    const WTF::String& provisionalURL() const { return m_provisionalURL; }

    void setCertificateInfo(PassRefPtr<WebCertificateInfo>);
    WebCertificateInfo* certificateInfo() const { return m_certificateInfo.get(); }

    void didStartProvisionalLoad(const String& url);
    void didReceiveServerRedirectForProvisionalLoad(const String& url);
    void didCommitLoad();
    void didFinishLoad();
    void didReceiveTitle(const WTF::String&);

    void receivedPolicyDecision(WebCore::PolicyAction, uint64_t listenerID);
    WebFramePolicyListenerProxy* setUpPolicyListenerProxy(uint64_t listenerID);
    WebFormSubmissionListenerProxy* setUpFormSubmissionListenerProxy(uint64_t listenerID);

private:
    WebFrameProxy(WebPageProxy* page, uint64_t frameID);

    virtual Type type() const { return APIType; }

    WebPageProxy* m_page;
    LoadState m_loadState;
    WTF::String m_url;
    WTF::String m_provisionalURL;
    RefPtr<WebCertificateInfo> m_certificateInfo;
    RefPtr<WebFrameListenerProxy> m_activeListener;
    uint64_t m_frameID;
};

} // namespace WebKit

#endif // WebFrameProxy_h
