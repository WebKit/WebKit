/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include "WebFramePolicyListenerProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/ListHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(CONTENT_FILTERING)
#include <WebCore/ContentFilterUnblockHandler.h>
#endif

namespace API {
class Data;
class Navigation;
class URL;
}

namespace IPC {
class Connection;
class Decoder;
using DataReference = std::span<const uint8_t>;
}

namespace WebKit {

class ProvisionalFrameProxy;
class RemotePageProxy;
class SafeBrowsingWarning;
class UserData;
class WebFramePolicyListenerProxy;
class WebPageProxy;
class WebProcessProxy;
class WebsiteDataStore;

enum class CanWrap : bool { No, Yes };
enum class DidWrap : bool { No, Yes };
enum class ShouldExpectSafeBrowsingResult : bool;
enum class ProcessSwapRequestedByClient : bool;

struct FrameInfoData;
struct FrameTreeCreationParameters;
struct FrameTreeNodeData;
struct WebsitePoliciesData;

class WebFrameProxy : public API::ObjectImpl<API::Object::Type::Frame>, public CanMakeWeakPtr<WebFrameProxy> {
public:
    static Ref<WebFrameProxy> create(WebPageProxy& page, WebProcessProxy& process, WebCore::FrameIdentifier frameID)
    {
        return adoptRef(*new WebFrameProxy(page, process, frameID));
    }

    static WebFrameProxy* webFrame(WebCore::FrameIdentifier);
    static bool canCreateFrame(WebCore::FrameIdentifier);

    virtual ~WebFrameProxy();

    WebCore::FrameIdentifier frameID() const { return m_frameID; }
    WebPageProxy* page() const;

    bool pageIsClosed() const { return !m_page; } // Needs to be thread-safe.

    void webProcessWillShutDown();

    bool isMainFrame() const;

    FrameLoadState& frameLoadState() { return m_frameLoadState; }

    void navigateServiceWorkerClient(WebCore::ScriptExecutionContextIdentifier, const URL&, CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)>&&);

    void loadURL(const URL&, const String& referrer = String());
    // Sub frames only. For main frames, use WebPageProxy::loadData.
    void loadData(const IPC::DataReference&, const String& MIMEType, const String& encodingName, const URL& baseURL);

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

    void didStartProvisionalLoad(const URL&);
    void didExplicitOpen(URL&&, String&& mimeType);
    void didReceiveServerRedirectForProvisionalLoad(const URL&);
    void didFailProvisionalLoad();
    void didCommitLoad(const String& contentType, const WebCore::CertificateInfo&, bool containsPluginDocument);
    void didFinishLoad();
    void didFailLoad();
    void didSameDocumentNavigation(const URL&); // eg. anchor navigation, session state change.
    void didChangeTitle(const String&);

    WebFramePolicyListenerProxy& setUpPolicyListenerProxy(CompletionHandler<void(WebCore::PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&&, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted)>&&, ShouldExpectSafeBrowsingResult, ShouldExpectAppBoundDomainResult, ShouldWaitForInitialLinkDecorationFilteringData);

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
    void didCreateSubframe(WebCore::FrameIdentifier, const String& frameName);
    ProcessID processID() const;
    void prepareForProvisionalNavigationInProcess(WebProcessProxy&, const API::Navigation&, CompletionHandler<void()>&&);

    void commitProvisionalFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType, const WebCore::CertificateInfo&, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent, WebCore::MouseEventPolicy, const UserData&);

    void getFrameInfo(CompletionHandler<void(FrameTreeNodeData&&)>&&);
    FrameTreeCreationParameters frameTreeCreationParameters() const;

    WebFrameProxy* parentFrame() const { return m_parentFrame.get(); }
    Ref<WebFrameProxy> rootFrame();
    WebProcessProxy& process() const { return m_process.get(); }
    Ref<WebProcessProxy> protectedProcess() const { return process(); }
    void setProcess(WebProcessProxy& process) { m_process = process; }
    ProvisionalFrameProxy* provisionalFrame() { return m_provisionalFrame.get(); }
    std::unique_ptr<ProvisionalFrameProxy> takeProvisionalFrame();
    RefPtr<RemotePageProxy> remotePageProxy();
    void remoteProcessDidTerminate();
    std::optional<WebCore::PageIdentifier> webPageIDInCurrentProcess();
    void notifyParentOfLoadCompletion(WebProcessProxy&);

    bool isFocused() const;

    struct TraversalResult {
        RefPtr<WebFrameProxy> frame;
        DidWrap didWrap { DidWrap::No };
    };
    TraversalResult traverseNext() const;
    TraversalResult traverseNext(CanWrap) const;
    TraversalResult traversePrevious(CanWrap);

private:
    WebFrameProxy(WebPageProxy&, WebProcessProxy&, WebCore::FrameIdentifier);

    std::optional<WebCore::PageIdentifier> pageIdentifier() const;

    RefPtr<WebFrameProxy> deepLastChild();
    RefPtr<WebFrameProxy> firstChild() const;
    RefPtr<WebFrameProxy> lastChild() const;
    RefPtr<WebFrameProxy> nextSibling() const;
    RefPtr<WebFrameProxy> previousSibling() const;

    WeakPtr<WebPageProxy> m_page;
    Ref<WebProcessProxy> m_process;

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
    std::unique_ptr<ProvisionalFrameProxy> m_provisionalFrame;
    RefPtr<RemotePageProxy> m_remotePageProxy;
#if ENABLE(CONTENT_FILTERING)
    WebCore::ContentFilterUnblockHandler m_contentFilterUnblockHandler;
#endif
    CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)> m_navigateCallback;
};

} // namespace WebKit
