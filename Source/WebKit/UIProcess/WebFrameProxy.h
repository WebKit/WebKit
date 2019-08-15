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

#pragma once

#include "APIObject.h"
#include "FrameLoadState.h"
#include "GenericCallback.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageProxy.h"
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/text/WTFString.h>

#if ENABLE(CONTENT_FILTERING)
#include <WebCore/ContentFilterUnblockHandler.h>
#endif

namespace API {
class Navigation;
}

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {
class SafeBrowsingWarning;
class WebCertificateInfo;
class WebFramePolicyListenerProxy;
class WebsiteDataStore;
enum class ShouldExpectSafeBrowsingResult;
enum class ProcessSwapRequestedByClient;
struct WebsitePoliciesData;

class WebFrameProxy : public API::ObjectImpl<API::Object::Type::Frame> {
public:
    static Ref<WebFrameProxy> create(WebPageProxy& page, WebCore::FrameIdentifier frameID)
    {
        return adoptRef(*new WebFrameProxy(page, frameID));
    }

    virtual ~WebFrameProxy();

    WebCore::FrameIdentifier frameID() const { return m_frameID; }
    WebPageProxy* page() const { return m_page.get(); }

    void webProcessWillShutDown();

    bool isMainFrame() const;

    void setIsFrameSet(bool value) { m_isFrameSet = value; }
    bool isFrameSet() const { return m_isFrameSet; }

    FrameLoadState& frameLoadState() { return m_frameLoadState; }

    void loadURL(const URL&, const String& referrer = String());
    // Sub frames only. For main frames, use WebPageProxy::loadData.
    void loadData(const IPC::DataReference&, const String& MIMEType, const String& encodingName, const URL& baseURL);
    void stopLoading() const;

    const URL& url() const { return m_frameLoadState.url(); }
    const URL& provisionalURL() const { return m_frameLoadState.provisionalURL(); }

    void setUnreachableURL(const URL&);
    const URL& unreachableURL() const { return m_frameLoadState.unreachableURL(); }

    const String& mimeType() const { return m_MIMEType; }
    bool containsPluginDocument() const { return m_containsPluginDocument; }

    const String& title() const { return m_title; }

    WebCertificateInfo* certificateInfo() const { return m_certificateInfo.get(); }

    bool canProvideSource() const;
    bool canShowMIMEType(const String& mimeType) const;

    bool isDisplayingStandaloneImageDocument() const;
    bool isDisplayingStandaloneMediaDocument() const;
    bool isDisplayingMarkupDocument() const;
    bool isDisplayingPDFDocument() const;

    void getWebArchive(Function<void (API::Data*, CallbackBase::Error)>&&);
    void getMainResourceData(Function<void (API::Data*, CallbackBase::Error)>&&);
    void getResourceData(API::URL*, Function<void (API::Data*, CallbackBase::Error)>&&);

    void didStartProvisionalLoad(const URL&);
    void didExplicitOpen(const URL&);
    void didReceiveServerRedirectForProvisionalLoad(const URL&);
    void didFailProvisionalLoad();
    void didCommitLoad(const String& contentType, WebCertificateInfo&, bool containsPluginDocument);
    void didFinishLoad();
    void didFailLoad();
    void didSameDocumentNavigation(const URL&); // eg. anchor navigation, session state change.
    void didChangeTitle(const String&);

    WebFramePolicyListenerProxy& setUpPolicyListenerProxy(CompletionHandler<void(WebCore::PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&&)>&&, ShouldExpectSafeBrowsingResult);

#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler contentFilterUnblockHandler) { m_contentFilterUnblockHandler = WTFMove(contentFilterUnblockHandler); }
    bool didHandleContentFilterUnblockNavigation(const WebCore::ResourceRequest&);
#endif

#if PLATFORM(GTK)
    void collapseSelection();
#endif

private:
    WebFrameProxy(WebPageProxy&, WebCore::FrameIdentifier);

    WeakPtr<WebPageProxy> m_page;

    FrameLoadState m_frameLoadState;

    String m_MIMEType;
    String m_title;
    bool m_isFrameSet { false };
    bool m_containsPluginDocument { false };
    RefPtr<WebCertificateInfo> m_certificateInfo;
    RefPtr<WebFramePolicyListenerProxy> m_activeListener;
    WebCore::FrameIdentifier m_frameID;
#if ENABLE(CONTENT_FILTERING)
    WebCore::ContentFilterUnblockHandler m_contentFilterUnblockHandler;
#endif
};

} // namespace WebKit
