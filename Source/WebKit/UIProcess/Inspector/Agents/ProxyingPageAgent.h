/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WebPageInspectorAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/InspectorResourceUtilities.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebFrameProxy;
class WebProcessProxy;
}

namespace Inspector {

class ProxyingPageAgent final : public RefCounted<ProxyingPageAgent>, public WebKit::InspectorAgentBase, public PageBackendDispatcherHandler, public IPC::MessageReceiver, public CanMakeCheckedPtr<ProxyingPageAgent> {
    WTF_MAKE_NONCOPYABLE(ProxyingPageAgent);
    WTF_MAKE_TZONE_ALLOCATED(ProxyingPageAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProxyingPageAgent);
public:
    ProxyingPageAgent(WebKit::WebPageAgentContext&);
    ~ProxyingPageAgent();

    // RefCounted
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    // InspectorAgentBase
    void didCreateFrontendAndBackend() override;
    void willDestroyFrontendAndBackend(DisconnectReason) override;

    bool isEnabled() const { return m_enabled; }
    void enableInstrumentationForProcess(WebKit::WebProcessProxy&, WebCore::PageIdentifier);
    void disableInstrumentationForProcess(WebKit::WebProcessProxy&, WebCore::PageIdentifier);

    // Authoritative "frame is genuinely gone" signal, reported from the WebFrameProxy destruction
    // path. Unlike the WebContent-process frameDetached IPC (which also fires on a process swap),
    // a WebFrameProxy is destroyed only when the frame is truly removed. See webkit.org/b/308896.
    void frameDestroyed(WebCore::FrameIdentifier);

    // PageBackendDispatcherHandler
    CommandResult<void> enable() final;
    CommandResult<void> disable() final;
    CommandResult<void> reload(std::optional<bool>&& ignoreCache, std::optional<bool>&& revalidateAllResources) final;
    CommandResult<void> overrideUserAgent(const String&) final;
    CommandResult<void> overrideSetting(Protocol::Page::Setting, std::optional<bool>&& value) final;
    CommandResult<void> overrideUserPreference(Protocol::Page::UserPreferenceName, std::optional<Protocol::Page::UserPreferenceValue>&&) final;
    CommandResult<Ref<JSON::ArrayOf<Protocol::Page::Cookie>>> getCookies() final;
    CommandResult<void> setCookie(Ref<JSON::Object>&&, std::optional<bool>&& shouldPartition) final;
    CommandResult<void> deleteCookie(const String& cookieName, const String& url) final;
    void getResourceTree(Ref<GetResourceTreeCallback>&&) final;
    CommandResultOf<String, bool /* base64Encoded */> getResourceContent(const Protocol::Network::FrameId&, const String& url) final;
    CommandResult<void> setBootstrapScript(const String& source) final;
    CommandResult<Ref<JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>>> searchInResource(const Protocol::Network::FrameId&, const String& url, const String& query, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex, const Protocol::Network::RequestId&) final;
    CommandResult<Ref<JSON::ArrayOf<Protocol::Page::SearchResult>>> searchInResources(const String&, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex) final;
#if !PLATFORM(IOS_FAMILY)
    CommandResult<void> setShowRulers(bool) final;
#endif
    CommandResult<void> setShowPaintRects(bool) final;
    CommandResult<void> setEmulatedMedia(const String&) final;
    CommandResult<String> snapshotNode(Protocol::DOM::NodeId) final;
    CommandResult<String> snapshotRect(int x, int y, int width, int height, Protocol::Page::CoordinateSystem) final;
#if ENABLE(WEB_ARCHIVE) && USE(CF)
    CommandResult<String> archive() final;
#endif
#if !PLATFORM(COCOA)
    CommandResult<void> setScreenSizeOverride(std::optional<int>&& width, std::optional<int>&& height) final;
#endif

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC message handlers from WebProcess PageAgentProxy
    void frameNavigated(WebCore::FrameIdentifier, const URL&, const String& mimeType, WebCore::SecurityOriginData&&, std::optional<WebCore::FrameIdentifier> parentFrameID, const String& name, WebCore::ScriptExecutionContextIdentifier loaderId);
    void domContentEventFired(double timestamp);
    void loadEventFired(double timestamp);
    void frameDetached(WebCore::FrameIdentifier);

    void removeAllRegisteredReceivers();

    Ref<Protocol::Page::FrameResourceTree> buildFrameTree(const WebKit::WebFrameProxy&, const String* parentProtocolId, const HashMap<WebCore::FrameIdentifier, FrameResourceData>& resourcesByFrame) const;

    const UniqueRef<PageFrontendDispatcher> m_frontendDispatcher;
    const Ref<PageBackendDispatcher> m_backendDispatcher;
    WeakRef<WebKit::WebPageProxy> m_inspectedPage;

    bool m_enabled { false };
    HashMap<std::pair<WebCore::ProcessIdentifier, WebCore::PageIdentifier>, unsigned> m_instrumentedProcessPageCounts;

    // Pin each instrumented WebProcessProxy alive while we hold an IPC message
    // receiver registration on it. Without this, the process can be destructed
    // before ~ProxyingPageAgent runs, leaving the receiver registered against a
    // map that has already gone away. The receiver's m_messageReceiverMapCount
    // then stays nonzero and ~MessageReceiver fires its debug ASSERT.
    HashMap<WebCore::ProcessIdentifier, Ref<WebKit::WebProcessProxy>> m_pinnedInstrumentedProcesses;

    // Per-frame committed document info, cached from the cross-process frameNavigated
    // events. buildFrameTree() prefers this over the inspectedPage's WebFrameProxy state,
    // which is stale for cross-origin children whose commit the UIProcess never observes.
    // See webkit.org/b/308896.
    struct CachedFrameDocumentInfo {
        URL url;
        String mimeType;
        WebCore::SecurityOriginData securityOrigin;
        std::optional<WebCore::ScriptExecutionContextIdentifier> loaderId;
    };
    HashMap<WebCore::FrameIdentifier, CachedFrameDocumentInfo> m_cachedFrameDocumentInfo;
};

} // namespace Inspector
