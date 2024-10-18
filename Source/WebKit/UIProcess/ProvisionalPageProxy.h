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
#include "MessageSender.h"
#include "NetworkResourceLoadIdentifier.h"
#include "PolicyDecision.h"
#include "ProcessThrottler.h"
#include "SandboxExtension.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageProxyIdentifier.h"
#include "WebPageProxyMessageReceiverRegistration.h"
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace API {
class Navigation;
}

namespace IPC {
class FormDataReference;
}

namespace WebCore {
class RegistrableDomain;
class ResourceRequest;
enum class CrossOriginOpenerPolicyValue : uint8_t;
enum class ShouldTreatAsContinuingLoad : uint8_t;
}

namespace WebKit {

class BrowsingContextGroup;
class DrawingAreaProxy;
class FrameProcess;
class FrameState;
class RemotePageProxy;
class SuspendedPageProxy;
class UserData;
class WebBackForwardListItem;
class WebFrameProxy;
class WebPageProxy;
class WebProcessProxy;
class WebsiteDataStore;

struct FrameInfoData;
struct NavigationActionData;
struct URLSchemeTaskParameters;
struct WebsitePoliciesData;
struct WebBackForwardListCounts;
struct WebNavigationDataStore;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
using LayerHostingContextID = uint32_t;
#endif

class ProvisionalPageProxy final : public IPC::MessageReceiver, public IPC::MessageSender, public RefCounted<ProvisionalPageProxy> {
    WTF_MAKE_TZONE_ALLOCATED(ProvisionalPageProxy);
public:
    template<typename... Args>
    static Ref<ProvisionalPageProxy> create(Args&&... args)
    {
        return adoptRef(*new ProvisionalPageProxy(std::forward<Args>(args)...));
    }
    ~ProvisionalPageProxy();

    WebPageProxy* page() { return m_page.get(); }
    const WebPageProxy* page() const { return m_page.get(); }
    RefPtr<WebPageProxy> protectedPage() const;

    WebCore::PageIdentifier webPageID() const { return m_webPageID; }
    WebFrameProxy* mainFrame() const { return m_mainFrame.get(); }
    BrowsingContextGroup& browsingContextGroup() { return m_browsingContextGroup.get(); }
    Ref<BrowsingContextGroup> protectedBrowsingContextGroup();
    WebProcessProxy& process();
    Ref<WebProcessProxy> protectedProcess();
    ProcessSwapRequestedByClient processSwapRequestedByClient() const { return m_processSwapRequestedByClient; }
    WebCore::NavigationIdentifier navigationID() const { return m_navigationID; }
    const URL& provisionalURL() const { return m_provisionalLoadURL; }
    RefPtr<WebsiteDataStore> replacedDataStoreForWebArchiveLoad() const { return m_replacedDataStoreForWebArchiveLoad; }

    bool isProcessSwappingOnNavigationResponse() const { return m_isProcessSwappingOnNavigationResponse; }

    DrawingAreaProxy* drawingArea() const { return m_drawingArea.get(); }
    std::unique_ptr<DrawingAreaProxy> takeDrawingArea();

    void setNavigation(API::Navigation&);

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
#if ENABLE(MODEL_PROCESS)
    void didCreateContextInModelProcessForVisibilityPropagation(LayerHostingContextID);
    LayerHostingContextID contextIDForVisibilityPropagationInModelProcess() const { return m_contextIDForVisibilityPropagationInModelProcess; }
#endif
#endif

    void loadData(API::Navigation&, Ref<WebCore::SharedBuffer>&&, const String& mimeType, const String& encoding, const String& baseURL, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain>, std::optional<WebsitePoliciesData>&&, WebCore::SubstituteData::SessionHistoryVisibility);
    void loadRequest(API::Navigation&, WebCore::ResourceRequest&&, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain>, std::optional<WebsitePoliciesData>&& = std::nullopt, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume = std::nullopt, WebCore::IsPerformingHTTPFallback = WebCore::IsPerformingHTTPFallback::No);
    void goToBackForwardItem(API::Navigation&, WebBackForwardListItem&, RefPtr<API::WebsitePolicies>&&, WebCore::ShouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume = std::nullopt);
    void cancel();

    void swipeAnimationDidEnd();

    void processDidTerminate();

    bool needsCookieAccessAddedInNetworkProcess() const { return m_needsCookieAccessAddedInNetworkProcess; }

    WebsitePoliciesData* mainFrameWebsitePoliciesData() const { return m_mainFrameWebsitePoliciesData.get(); }

    WebPageProxyMessageReceiverRegistration& messageReceiverRegistration() { return m_messageReceiverRegistration; }

private:
    ProvisionalPageProxy(WebPageProxy&, Ref<FrameProcess>&&, BrowsingContextGroup&, std::unique_ptr<SuspendedPageProxy>, API::Navigation&, bool isServerRedirect, const WebCore::ResourceRequest&, ProcessSwapRequestedByClient, bool isProcessSwappingOnNavigationResponse, API::WebsitePolicies*, WebsiteDataStore* replacedDataStoreForWebArchiveLoad = nullptr);

    RefPtr<WebFrameProxy> protectedMainFrame() const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;
    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>) final;
    bool sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&&, AsyncReplyHandler, OptionSet<IPC::SendOption>) final;

    void decidePolicyForNavigationActionAsync(NavigationActionData&&, CompletionHandler<void(PolicyDecision&&)>&&);
    void decidePolicyForResponse(FrameInfoData&&, std::optional<WebCore::NavigationIdentifier>, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, const String& downloadAttribute, bool isShowingInitialAboutBlank, WebCore::CrossOriginOpenerPolicyValue activeDocumentCOOPValue, CompletionHandler<void(PolicyDecision&&)>&&);
    void didChangeProvisionalURLForFrame(WebCore::FrameIdentifier, std::optional<WebCore::NavigationIdentifier>, URL&&);
    void didPerformServerRedirect(const String& sourceURLString, const String& destinationURLString, WebCore::FrameIdentifier);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebCore::FrameIdentifier, std::optional<WebCore::NavigationIdentifier>, WebCore::ResourceRequest&&, const UserData&);
    void didNavigateWithNavigationData(const WebNavigationDataStore&, WebCore::FrameIdentifier);
    void didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, WebCore::FrameIdentifier);
    void didStartProvisionalLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, std::optional<WebCore::NavigationIdentifier>, URL&&, URL&& unreachableURL, const UserData&, WallTime);
    void didCommitLoadForFrame(IPC::Connection&, WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, std::optional<WebCore::NavigationIdentifier>, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType, const WebCore::CertificateInfo&, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent, WebCore::MouseEventPolicy, const UserData&);
    void didFailProvisionalLoadForFrame(FrameInfoData&&, WebCore::ResourceRequest&&, std::optional<WebCore::NavigationIdentifier>, const String& provisionalURL, const WebCore::ResourceError&, WebCore::WillContinueLoading, const UserData&, WebCore::WillInternallyHandleFailure);
    void logDiagnosticMessageFromWebProcess(const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithEnhancedPrivacyFromWebProcess(const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithValueDictionaryFromWebProcess(const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary&, WebCore::ShouldSample);
    void startURLSchemeTask(IPC::Connection&, URLSchemeTaskParameters&&);
    void backForwardGoToItem(const WebCore::BackForwardItemIdentifier&, CompletionHandler<void(const WebBackForwardListCounts&)>&&);
    void decidePolicyForNavigationActionSync(NavigationActionData&&, CompletionHandler<void(PolicyDecision&&)>&&);
    void backForwardAddItem(IPC::Connection&, WebCore::FrameIdentifier, Ref<FrameState>&&);
    void didDestroyNavigation(WebCore::NavigationIdentifier);
#if USE(QUICK_LOOK)
    void requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&&);
#endif
#if PLATFORM(COCOA)
    void registerWebProcessAccessibilityToken(std::span<const uint8_t>);
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
    bool validateInput(WebCore::FrameIdentifier, const std::optional<WebCore::NavigationIdentifier>& = std::nullopt);

    WeakPtr<WebPageProxy> m_page;
    WebCore::PageIdentifier m_webPageID;
    Ref<FrameProcess> m_frameProcess;
    Ref<BrowsingContextGroup> m_browsingContextGroup;

    // Keep WebsiteDataStore alive for provisional page load.
    RefPtr<WebsiteDataStore> m_websiteDataStore;

    std::unique_ptr<DrawingAreaProxy> m_drawingArea;
    RefPtr<WebFrameProxy> m_mainFrame;
    RefPtr<WebsiteDataStore> m_replacedDataStoreForWebArchiveLoad;
    WebCore::NavigationIdentifier m_navigationID;
    bool m_isServerRedirect;
    WebCore::ResourceRequest m_request;
    ProcessSwapRequestedByClient m_processSwapRequestedByClient;
    bool m_wasCommitted { false };
    bool m_isProcessSwappingOnNavigationResponse { false };
    bool m_needsCookieAccessAddedInNetworkProcess { false };
    bool m_needsDidStartProvisionalLoad { true };
    bool m_shouldClosePage { true };
    URL m_provisionalLoadURL;
    WebPageProxyMessageReceiverRegistration m_messageReceiverRegistration;
    std::unique_ptr<WebsitePoliciesData> m_mainFrameWebsitePoliciesData;

#if PLATFORM(COCOA)
    Vector<uint8_t> m_accessibilityToken;
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    String m_accessibilityPlugID;
    CompletionHandler<void(String&&)> m_accessibilityBindCompletionHandler;
#endif
#if USE(RUNNINGBOARD)
    Ref<ProcessThrottler::ForegroundActivity> m_provisionalLoadActivity;
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID m_contextIDForVisibilityPropagationInWebProcess { 0 };
#if ENABLE(GPU_PROCESS)
    LayerHostingContextID m_contextIDForVisibilityPropagationInGPUProcess { 0 };
#endif
#if ENABLE(MODEL_PROCESS)
    LayerHostingContextID m_contextIDForVisibilityPropagationInModelProcess { 0 };
#endif
#endif
};

} // namespace WebKit
