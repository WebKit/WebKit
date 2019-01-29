/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include <wtf/WeakPtr.h>

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

class DrawingAreaProxy;
class SuspendedPageProxy;
class WebFrameProxy;
struct WebNavigationDataStore;
class WebPageProxy;
class WebProcessProxy;

class ProvisionalPageProxy : public IPC::MessageReceiver, public CanMakeWeakPtr<ProvisionalPageProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ProvisionalPageProxy(WebPageProxy&, Ref<WebProcessProxy>&&, std::unique_ptr<SuspendedPageProxy>, uint64_t navigationID, bool isServerRedirect, const WebCore::ResourceRequest&, ProcessSwapRequestedByClient);
    ~ProvisionalPageProxy();

    WebPageProxy& page() { return m_page; }
    WebFrameProxy* mainFrame() const { return m_mainFrame.get(); }
    WebProcessProxy& process() { return m_process.get(); }
    ProcessSwapRequestedByClient processSwapRequestedByClient() const { return m_processSwapRequestedByClient; }
    uint64_t navigationID() const { return m_navigationID; }

    DrawingAreaProxy* drawingArea() const { return m_drawingArea.get(); }
    std::unique_ptr<DrawingAreaProxy> takeDrawingArea();

#if PLATFORM(COCOA)
    Vector<uint8_t> takeAccessibilityToken() { return WTFMove(m_accessibilityToken); }
#endif

    void loadData(API::Navigation&, const IPC::DataReference&, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, Optional<WebsitePoliciesData>&& = WTF::nullopt);
    void loadRequest(API::Navigation&, WebCore::ResourceRequest&&, WebCore::ShouldOpenExternalURLsPolicy, API::Object* userData, Optional<WebsitePoliciesData>&& = WTF::nullopt);
    void goToBackForwardItem(API::Navigation&, WebBackForwardListItem&, Optional<WebsitePoliciesData>&&);
    void cancel();

    void processDidFinishLaunching();
    void processDidTerminate();

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    void decidePolicyForNavigationActionAsync(uint64_t frameID, WebCore::SecurityOriginData&& frameSecurityOrigin, uint64_t navigationID, NavigationActionData&&, FrameInfoData&&, uint64_t originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&&, IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse, const UserData&, uint64_t listenerID);
    void decidePolicyForResponse(uint64_t frameID, const WebCore::SecurityOriginData& frameSecurityOrigin, uint64_t navigationID, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, uint64_t listenerID, const UserData&);
    void didChangeProvisionalURLForFrame(uint64_t frameID, uint64_t navigationID, URL&&);
    void didNavigateWithNavigationData(const WebNavigationDataStore&, uint64_t frameID);
    void didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, uint64_t frameID);
    void didCreateMainFrame(uint64_t frameID);
    void didStartProvisionalLoadForFrame(uint64_t frameID, uint64_t navigationID, URL&&, URL&& unreachableURL, const UserData&);
    void didCommitLoadForFrame(uint64_t frameID, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, uint32_t frameLoadType, const WebCore::CertificateInfo&, bool containsPluginDocument, Optional<WebCore::HasInsecureContent> forcedHasInsecureContent, const UserData&);
    void didFailProvisionalLoadForFrame(uint64_t frameID, const WebCore::SecurityOriginData& frameSecurityOrigin, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError&, const UserData&);
    void startURLSchemeTask(URLSchemeTaskParameters&&);
    void backForwardGoToItem(const WebCore::BackForwardItemIdentifier&, SandboxExtension::Handle&);
#if PLATFORM(COCOA)
    void registerWebProcessAccessibilityToken(const IPC::DataReference&);
#endif

    void initializeWebPage();
    void finishInitializingWebPageAfterProcessLaunch();

    WebPageProxy& m_page;
    Ref<WebProcessProxy> m_process;
    std::unique_ptr<DrawingAreaProxy> m_drawingArea;
    RefPtr<WebFrameProxy> m_mainFrame;
    uint64_t m_navigationID;
    bool m_isServerRedirect;
    WebCore::ResourceRequest m_request;
    ProcessSwapRequestedByClient m_processSwapRequestedByClient;
    bool m_wasCommitted { false };
    URL m_provisionalLoadURL;

#if PLATFORM(COCOA)
    Vector<uint8_t> m_accessibilityToken;
#endif
#if PLATFORM(IOS_FAMILY)
    ProcessThrottler::ForegroundActivityToken m_suspensionToken;
#endif
};

} // namespace WebKit
