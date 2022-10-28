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

#include "DataReference.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "NetworkResourceLoadIdentifier.h"
#include "PolicyDecision.h"
#include "ProcessThrottler.h"
#include "SandboxExtension.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageProxyMessagesReplies.h"
#include "WebsitePoliciesData.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/WeakPtr.h>

namespace API {
class Navigation;
}

namespace IPC {
class FormDataReference;
}

namespace WebCore {
class ResourceRequest;
struct BackForwardItemIdentifier;
enum class ShouldTreatAsContinuingLoad : uint8_t;
}

namespace WebKit {

class DrawingAreaProxy;
class SuspendedPageProxy;
class UserData;
class WebFrameProxy;
class WebPageProxy;
class WebProcessProxy;
class WebsiteDataStore;
struct FrameInfoData;
struct NavigationActionData;
struct URLSchemeTaskParameters;
struct WebNavigationDataStore;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
using LayerHostingContextID = uint32_t;
#endif

class ProvisionalPageProxy : public IPC::MessageReceiver, public IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ProvisionalPageProxy(WebPageProxy&, Ref<WebProcessProxy>&&, std::unique_ptr<SuspendedPageProxy>, uint64_t navigationID, bool isServerRedirect, const WebCore::ResourceRequest&, ProcessSwapRequestedByClient, bool isProcessSwappingOnNavigationResponse, API::WebsitePolicies*);
    ~ProvisionalPageProxy();

    WebPageProxy& page() const { return m_page; }
    WebCore::PageIdentifier webPageID() const { return m_webPageID; }
    WebFrameProxy* mainFrame() const { return m_mainFrame.get(); }
    WebProcessProxy& process() { return m_process.get(); }
    ProcessSwapRequestedByClient processSwapRequestedByClient() const { return m_processSwapRequestedByClient; }
    uint64_t navigationID() const { return m_navigationID; }
    const URL& provisionalURL() const { return m_provisionalLoadURL; }

    bool isProcessSwappingOnNavigationResponse() const { return m_isProcessSwappingOnNavigationResponse; }

    DrawingAreaProxy* drawingArea() const { return m_drawingArea.get(); }
    std::unique_ptr<DrawingAreaProxy> takeDrawingArea();

    void setNavigationID(uint64_t navigationID) { m_navigationID = navigationID; }

#if PLATFORM(COCOA)
    Vector<uint8_t> takeAccessibilityToken() { return WTFMove(m_accessibilityToken); }
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    const String& accessibilityPlugID() { return m_accessibilityPlugID; }
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID contextIDForVisibilityPropagationInWebProcess() const { return m_contextIDForVisibilityPropagationInWebProcess; }
#if ENABLE(GPU_PROCESS)
    void didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID);
    LayerHostingContextID contextIDForVisibilityPropagationInGPUProcess() const { return m_contextIDForVisibilityPropagationInGPUProcess; }
#endif
#endif

    void loadData(API::Navigation&, const IPC::DataReference&, const String& mimeType, const String& encoding, const String& baseURL, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain>, std::optional<WebsitePoliciesData>&&, WebCore::SubstituteData::SessionHistoryVisibility);
    void loadRequest(API::Navigation&, WebCore::ResourceRequest&&, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain>, std::optional<WebsitePoliciesData>&& = std::nullopt, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume = std::nullopt);
    void goToBackForwardItem(API::Navigation&, WebBackForwardListItem&, RefPtr<API::WebsitePolicies>&&, WebCore::ShouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume = std::nullopt);
    void cancel();

    void unfreezeLayerTreeDueToSwipeAnimation();

    void processDidTerminate();

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;
    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>) final;
    bool sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&&, AsyncReplyHandler, OptionSet<IPC::SendOption>) final;

    void decidePolicyForResponse(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::PolicyCheckIdentifier, uint64_t navigationID, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, const String& downloadAttribute, uint64_t listenerID, const UserData&);
    void didChangeProvisionalURLForFrame(WebCore::FrameIdentifier, uint64_t navigationID, URL&&);
    void didPerformServerRedirect(const String& sourceURLString, const String& destinationURLString, WebCore::FrameIdentifier);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebCore::FrameIdentifier, uint64_t navigationID, WebCore::ResourceRequest&&, const UserData&);
    void didNavigateWithNavigationData(const WebNavigationDataStore&, WebCore::FrameIdentifier);
    void didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, WebCore::FrameIdentifier);
    void didCreateMainFrame(WebCore::FrameIdentifier);
    void didStartProvisionalLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, URL&&, URL&& unreachableURL, const UserData&);
    void didCommitLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType, const WebCore::CertificateInfo&, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, std::optional<WebCore::HasInsecureContent> forcedHasInsecureContent, WebCore::MouseEventPolicy, const UserData&);
    void didFailProvisionalLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError&, WebCore::WillContinueLoading, const UserData&);
    void logDiagnosticMessageFromWebProcess(const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithEnhancedPrivacyFromWebProcess(const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithValueDictionaryFromWebProcess(const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary&, WebCore::ShouldSample);
    void startURLSchemeTask(URLSchemeTaskParameters&&);
    void backForwardGoToItem(const WebCore::BackForwardItemIdentifier&, CompletionHandler<void(const WebBackForwardListCounts&)>&&);
#if USE(QUICK_LOOK)
    void requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&&);
#endif
#if PLATFORM(COCOA)
    void registerWebProcessAccessibilityToken(const IPC::DataReference&);
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    void bindAccessibilityTree(const String&);
#endif
#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoadForFrame(const WebCore::ContentFilterUnblockHandler&, WebCore::FrameIdentifier);
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID);
#endif

    void initializeWebPage(RefPtr<API::WebsitePolicies>&&);
    bool validateInput(WebCore::FrameIdentifier, const std::optional<uint64_t>& navigationID = std::nullopt);

    WebPageProxy& m_page;
    WebCore::PageIdentifier m_webPageID;
    Ref<WebProcessProxy> m_process;
    // Keep WebsiteDataStore alive for provisional page load.
    RefPtr<WebsiteDataStore> m_websiteDataStore;
    std::unique_ptr<DrawingAreaProxy> m_drawingArea;
    RefPtr<WebFrameProxy> m_mainFrame;
    uint64_t m_navigationID;
    bool m_isServerRedirect;
    WebCore::ResourceRequest m_request;
    ProcessSwapRequestedByClient m_processSwapRequestedByClient;
    bool m_wasCommitted { false };
    bool m_isProcessSwappingOnNavigationResponse { false };
    URL m_provisionalLoadURL;

#if PLATFORM(COCOA)
    Vector<uint8_t> m_accessibilityToken;
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    String m_accessibilityPlugID;
    CompletionHandler<void(String&&)> m_accessibilityBindCompletionHandler;
#endif
#if USE(RUNNINGBOARD)
    UniqueRef<ProcessThrottler::ForegroundActivity> m_provisionalLoadActivity;
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID m_contextIDForVisibilityPropagationInWebProcess { 0 };
#if ENABLE(GPU_PROCESS)
    LayerHostingContextID m_contextIDForVisibilityPropagationInGPUProcess { 0 };
#endif
#endif
};

} // namespace WebKit
