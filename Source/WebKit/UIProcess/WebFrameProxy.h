/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "MessageReceiver.h"
#include "ProvisionalFrameCreationParameters.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntSize.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/ProcessID.h>
#include <wtf/RetainReleaseSwift.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(CONTENT_FILTERING)
#include <WebCore/ContentFilterUnblockHandler.h>
#endif

namespace API {
class Data;
class Navigation;
class URL;
class WebsitePolicies;
}

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class FrameTreeSyncData;
class ResourceRequest;
class SecurityOriginData;
class ShareableBitmapHandle;

struct FocusEventData;
struct FrameIdentifierType;
struct JSHandleIdentifierType;
struct NavigationIdentifierType;

enum class FocusDirection : uint8_t;
enum class FoundElementInRemoteFrame : bool;
enum class MouseEventPolicy : uint8_t;
enum class ResourceResponseSource : uint8_t;
enum class SandboxFlag : uint16_t;
enum class ScrollbarMode : uint8_t;

using FrameIdentifier = ObjectIdentifier<FrameIdentifierType>;
using NavigationIdentifier = ObjectIdentifier<NavigationIdentifierType, uint64_t>;
using SandboxFlags = OptionSet<SandboxFlag>;
using WebProcessJSHandleIdentifier = ObjectIdentifier<JSHandleIdentifierType>;
using JSHandleIdentifier = ProcessQualified<WebProcessJSHandleIdentifier>;
}

namespace WebKit {

class BrowsingContextGroup;
class FrameProcess;
class ProvisionalFrameProxy;
class BrowsingWarning;
class UserData;
class WebBackForwardListFrameItem;
class WebFramePolicyListenerProxy;
class WebPageProxy;
class WebProcessProxy;
class WebsiteDataStore;

enum class CanWrap : bool { No, Yes };
enum class DidWrap : bool { No, Yes };
enum class IsMainFrame : bool { No, Yes };
enum class NavigatingToAppBoundDomain : bool;
enum class ShouldExpectSafeBrowsingResult : bool;
enum class ShouldExpectAppBoundDomainResult : bool;
enum class ShouldWaitForInitialLinkDecorationFilteringData : bool;
enum class ProcessSwapRequestedByClient : bool;
enum class WasNavigationIntercepted : bool;

struct FrameInfoData;
struct FrameTreeCreationParameters;
struct FrameTreeNodeData;
struct SharedPreferencesForWebProcess;
struct WebsitePoliciesData;

class WebFrameProxy : public API::ObjectImpl<API::Object::Type::Frame>, public IPC::MessageReceiver {
public:
    static Ref<WebFrameProxy> create(WebPageProxy& page, FrameProcess& process, WebCore::FrameIdentifier frameID, WebCore::SandboxFlags sandboxFlags, WebCore::ReferrerPolicy referrerPolicy, WebCore::ScrollbarMode scrollingMode, WebFrameProxy* opener, IsMainFrame isMainFrame)
    {
        return adoptRef(*new WebFrameProxy(page, process, frameID, sandboxFlags, referrerPolicy, scrollingMode, opener, isMainFrame));
    }

    void ref() const final { API::ObjectImpl<API::Object::Type::Frame>::ref(); }
    void deref() const final { API::ObjectImpl<API::Object::Type::Frame>::deref(); }

    static WebFrameProxy* webFrame(std::optional<WebCore::FrameIdentifier>);
    static RefPtr<WebFrameProxy> protectedWebFrame(std::optional<WebCore::FrameIdentifier> identifier) { return webFrame(identifier); }

    static bool canCreateFrame(WebCore::FrameIdentifier);

    virtual ~WebFrameProxy();

    WebCore::FrameIdentifier frameID() const { return m_frameID; }
    WebPageProxy* page() const;
    RefPtr<WebPageProxy> protectedPage() const;

    bool pageIsClosed() const { return !m_page; } // Needs to be thread-safe.

    void webProcessWillShutDown();

    bool isMainFrame() const;

    FrameLoadState& frameLoadState() { return m_frameLoadState; }

    void navigateServiceWorkerClient(WebCore::ScriptExecutionContextIdentifier, const URL&, CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)>&&);

    void loadURL(const URL&, const String& referrer = String());
    // Sub frames only. For main frames, use WebPageProxy::loadData.
    void loadData(std::span<const uint8_t>, const String& MIMEType, const String& encodingName, const URL& baseURL);

    const URL& url() const { return m_frameLoadState.url(); }
    const URL& provisionalURL() const { return m_frameLoadState.provisionalURL(); }

    void setUnreachableURL(const URL&);
    const URL& unreachableURL() const { return m_frameLoadState.unreachableURL(); }

    const String& mimeType() const { return m_MIMEType; }
    bool containsPluginDocument() const { return m_containsPluginDocument; }

    const String& title() const { return m_title; }

    const WebCore::CertificateInfo& certificateInfo() const { return m_certificateInfo; }

    bool canProvideSource() const;

    bool isDisplayingStandaloneImageDocument() const;
    bool isDisplayingStandaloneMediaDocument() const;
    bool isDisplayingMarkupDocument() const;
    bool isDisplayingPDFDocument() const;

    void getWebArchive(CompletionHandler<void(API::Data*)>&&);
    void getMainResourceData(CompletionHandler<void(API::Data*)>&&);
    void getResourceData(API::URL*, CompletionHandler<void(API::Data*)>&&);

    void didStartProvisionalLoad(URL&&);
    void didExplicitOpen(URL&&, String&& mimeType);
    void didReceiveServerRedirectForProvisionalLoad(URL&&);
    void didFailProvisionalLoad();
    void didCommitLoad(const String& contentType, const WebCore::CertificateInfo&, bool containsPluginDocument);
    void didFinishLoad();
    void didFailLoad();
    void didSameDocumentNavigation(URL&&); // eg. anchor navigation, session state change.
    void didChangeTitle(String&&);

    WebFramePolicyListenerProxy& setUpPolicyListenerProxy(CompletionHandler<void(WebCore::PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted)>&&, ShouldExpectSafeBrowsingResult, ShouldExpectAppBoundDomainResult, ShouldWaitForInitialLinkDecorationFilteringData);

#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler contentFilterUnblockHandler) { m_contentFilterUnblockHandler = WTFMove(contentFilterUnblockHandler); }
    bool didHandleContentFilterUnblockNavigation(const WebCore::ResourceRequest&);
#endif

#if PLATFORM(GTK)
    void collapseSelection();
#endif

    void transferNavigationCallbackToFrame(WebFrameProxy&);
    void setNavigationCallback(CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)>&&);

    void disconnect();
    bool isConnected() const;
    void didCreateSubframe(WebCore::FrameIdentifier, String&& frameName, WebCore::SandboxFlags, WebCore::ReferrerPolicy, WebCore::ScrollbarMode);
    ProcessID processID() const;
    void prepareForProvisionalLoadInProcess(WebProcessProxy&, API::Navigation&, BrowsingContextGroup&, std::optional<WebCore::SecurityOriginData>, CompletionHandler<void(WebCore::PageIdentifier)>&&);

    void commitProvisionalFrame(IPC::Connection&, WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, std::optional<WebCore::NavigationIdentifier>, String&& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType, const WebCore::CertificateInfo&, bool usedLegacyTLS, bool privateRelayed, String&& proxyName, WebCore::ResourceResponseSource, bool containsPluginDocument, WebCore::HasInsecureContent, WebCore::MouseEventPolicy, const UserData&);

    void getFrameTree(CompletionHandler<void(std::optional<FrameTreeNodeData>&&)>&&);
    void getFrameInfo(CompletionHandler<void(std::optional<FrameInfoData>&&)>&&);
    FrameTreeCreationParameters frameTreeCreationParameters() const;

    WebFrameProxy* parentFrame() const { return m_parentFrame.get(); }
    WebFrameProxy& rootFrame();
    WebProcessProxy& process() const;
    Ref<WebProcessProxy> protectedProcess() const;
    void setProcess(FrameProcess&);
    const FrameProcess& frameProcess() const { return m_frameProcess.get(); }
    FrameProcess& frameProcess() { return m_frameProcess.get(); }
    void removeChildFrames();
    ProvisionalFrameProxy* provisionalFrame() { return m_provisionalFrame.get(); }
    RefPtr<ProvisionalFrameProxy> takeProvisionalFrame();
    WebProcessProxy& provisionalLoadProcess();
    std::optional<WebCore::PageIdentifier> webPageIDInCurrentProcess();
    void notifyParentOfLoadCompletion(WebProcessProxy&);

    enum class ClearFrameTreeSyncData : bool {
        No,
        Yes
    };
    void remoteProcessDidTerminate(WebProcessProxy&, ClearFrameTreeSyncData);

    Ref<WebCore::FrameTreeSyncData> calculateFrameTreeSyncData() const;
    void broadcastFrameTreeSyncData(Ref<WebCore::FrameTreeSyncData>&&);

    void removeRemotePagesForSuspension();
    void bindAccessibilityFrameWithData(std::span<const uint8_t>);

    bool isFocused() const;

    struct TraversalResult {
        RefPtr<WebFrameProxy> frame;
        DidWrap didWrap { DidWrap::No };
    };
    TraversalResult traverseNext() const;
    TraversalResult traverseNext(CanWrap) const;
    TraversalResult traversePrevious(CanWrap);

    void setIsPendingInitialHistoryItem(bool isPending) { m_isPendingInitialHistoryItem = isPending; }
    bool isPendingInitialHistoryItem() const { return m_isPendingInitialHistoryItem; }

    WebCore::LayerHostingContextIdentifier layerHostingContextIdentifier() const { return m_layerHostingContextIdentifier; }
    void updateRemoteFrameSize(WebCore::IntSize);
    void setAppBadge(const WebCore::SecurityOriginData&, std::optional<uint64_t> badge);
    void findFocusableElementDescendingIntoRemoteFrame(WebCore::FocusDirection, const WebCore::FocusEventData&, CompletionHandler<void(WebCore::FoundElementInRemoteFrame)>&&);

    WebCore::SandboxFlags effectiveSandboxFlags() const { return m_effectiveSandboxFlags; }
    void updateSandboxFlags(WebCore::SandboxFlags sandboxFlags) { m_effectiveSandboxFlags = sandboxFlags; }

    WebCore::ReferrerPolicy effectiveReferrerPolicy() const { return m_effectiveReferrerPolicy; }
    void updateReferrerPolicy(WebCore::ReferrerPolicy referrerPolicy) { m_effectiveReferrerPolicy = referrerPolicy; }

    WebCore::ScrollbarMode scrollingMode() const { return m_scrollingMode; }
    void updateScrollingMode(WebCore::ScrollbarMode);

    void updateOpener(WebCore::FrameIdentifier);
    WebFrameProxy* opener() { return m_opener.get(); }
    void disownOpener() { m_opener = nullptr; }

    std::optional<WebCore::IntSize> remoteFrameSize() const { return m_remoteFrameSize; }

    void takeSnapshotOfNode(WebCore::JSHandleIdentifier, CompletionHandler<void(std::optional<WebCore::ShareableBitmapHandle>&&)>&&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    static void sendCancelReply(IPC::Connection&, IPC::Decoder&);
    template<typename M, typename C> void sendWithAsyncReply(M&&, C&&);
    template<typename M> void send(M&&);

    void sendMessageToInspectorFrontend(const String& targetId, const String& message);

    ProvisionalFrameCreationParameters provisionalFrameCreationParameters(std::optional<WebCore::FrameIdentifier>, CommitTiming);

private:
    WebFrameProxy(WebPageProxy&, FrameProcess&, WebCore::FrameIdentifier, WebCore::SandboxFlags, WebCore::ReferrerPolicy, WebCore::ScrollbarMode, WebFrameProxy*, IsMainFrame);

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    std::optional<WebCore::PageIdentifier> pageIdentifier() const;

    RefPtr<WebFrameProxy> deepLastChild();
    WebFrameProxy* firstChild() const;
    WebFrameProxy* lastChild() const;
    WebFrameProxy* nextSibling() const;
    WebFrameProxy* previousSibling() const;

    WeakPtr<WebPageProxy> m_page;
    Ref<FrameProcess> m_frameProcess;
    WeakPtr<WebFrameProxy> m_opener;

    FrameLoadState m_frameLoadState;

    String m_MIMEType;
    String m_title;
    String m_frameName;
    bool m_containsPluginDocument { false };
    WebCore::CertificateInfo m_certificateInfo;
    RefPtr<WebFramePolicyListenerProxy> m_activeListener;
    WebCore::FrameIdentifier m_frameID;
    ListHashSet<Ref<WebFrameProxy>> m_childFrames;
    WeakPtr<WebFrameProxy> m_parentFrame;
    RefPtr<ProvisionalFrameProxy> m_provisionalFrame;
#if ENABLE(CONTENT_FILTERING)
    WebCore::ContentFilterUnblockHandler m_contentFilterUnblockHandler;
#endif
    CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)> m_navigateCallback;
    const WebCore::LayerHostingContextIdentifier m_layerHostingContextIdentifier;
    bool m_isPendingInitialHistoryItem { false };
    std::optional<WebCore::IntSize> m_remoteFrameSize;
    WebCore::SandboxFlags m_effectiveSandboxFlags;
    WebCore::ReferrerPolicy m_effectiveReferrerPolicy { WebCore::ReferrerPolicy::EmptyString };
    WebCore::ScrollbarMode m_scrollingMode;
} SWIFT_SHARED_REFERENCE(refWebFrameProxy, derefWebFrameProxy);

} // namespace WebKit

inline void refWebFrameProxy(WebKit::WebFrameProxy* WTF_NONNULL obj)
{
    WTF::ref(obj);
}

inline void derefWebFrameProxy(WebKit::WebFrameProxy* WTF_NONNULL obj)
{
    WTF::deref(obj);
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebFrameProxy)
    static bool isType(const API::Object& object) { return object.type() == API::Object::Type::Frame; }
SPECIALIZE_TYPE_TRAITS_END()
