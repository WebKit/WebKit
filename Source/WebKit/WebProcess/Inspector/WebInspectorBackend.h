/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "BackendResourceDataStore.h"
#include "Connection.h"
#include "MessageReceiver.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/InspectorBackendClient.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <utility>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class EmulationManager;
struct EmulationOverrides;
struct FrameResourceData;
struct SearchMatch;
struct SearchResult;
}

namespace WebKit {

class FrameNetworkAgentProxy;
class PageAgentProxy;
class WebPage;

// IPC::Connection::Client publicly derives CanMakeThreadSafeCheckedPtr, which is the
// checked-pointer capability this class exposes. The base must be public so the
// CheckedRef<WebInspectorBackend> member in EmulationManager can reach
// increment/decrementCheckedPtrCount(); private inheritance hides them and fails to compile.
class WebInspectorBackend : public ThreadSafeRefCounted<WebInspectorBackend>, public IPC::Connection::Client {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WebInspectorBackend);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebInspectorBackend);
public:
    static Ref<WebInspectorBackend> create(WebPage&);
    ~WebInspectorBackend();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    WebPage* NODELETE page() const;

    void updateDockingAvailability();

    // Implemented in generated WebInspectorBackendMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override { close(); }
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding) override { close(); }

    void show(CompletionHandler<void(bool success)>&&);
    void close();

    void canAttachWindow(bool& result);

    void showConsole();
    void showResources();

    void showMainResourceForFrame(WebCore::FrameIdentifier);

    void setAttached(bool attached) { m_attached = attached; }

    void evaluateScriptForTest(const String& script);

    void startPageProfiling();
    void stopPageProfiling();

    void startElementSelection();
    void stopElementSelection();
    void elementSelectionChanged(bool);
    void timelineRecordingChanged(bool);
    void showPaintRectsChanged(bool);

    void setDeveloperPreferenceOverride(WebCore::InspectorBackendClient::DeveloperPreference, std::optional<bool>);
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    void setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit);
#endif

    void enableNetworkInstrumentation();
    void disableNetworkInstrumentation();
    void getResponseBody(WebCore::ResourceLoaderIdentifier, CompletionHandler<void(std::expected<std::pair<String, bool>, String>&&)>&&);
    void getSerializedCertificate(WebCore::ResourceLoaderIdentifier, CompletionHandler<void(std::expected<String, String>&&)>&&);
    void loadResource(WebCore::FrameIdentifier, const String& url, CompletionHandler<void(std::expected<std::tuple<String, String, int>, String>&&)>&&);

    void setExtraHTTPHeaders(WebCore::HTTPHeaderMap&&);
    void setResourceCachingDisabled(bool);

    void enablePageInstrumentation();
    void disablePageInstrumentation();
    void getFrameResourceData(Vector<WebCore::FrameIdentifier>&& frameIDs, CompletionHandler<void(Vector<std::pair<WebCore::FrameIdentifier, Inspector::FrameResourceData>>&&)>&&);
    void getFrameResourceContent(WebCore::FrameIdentifier, String url, CompletionHandler<void(String content, bool base64Encoded, String errorString)>&&);

    void searchInRequest(WebCore::ResourceLoaderIdentifier, const String& query, bool caseSensitive, bool isRegex, CompletionHandler<void(Vector<Inspector::SearchMatch>&&, String errorString)>&&);
    void searchInFrameResource(WebCore::FrameIdentifier, const String& url, const String& query, bool caseSensitive, bool isRegex, CompletionHandler<void(Vector<Inspector::SearchMatch>&&, String errorString)>&&);
    void searchInFramesAndRequests(Vector<WebCore::FrameIdentifier>&& frameIDs, const String& query, bool caseSensitive, bool isRegex, CompletionHandler<void(Vector<Inspector::SearchResult>&&)>&&);

    // Fan the paint-rects toggle out to every per-frame PageAgentProxy this process hosts.
    void setShowPaintRects(bool);

    // Apply the typed emulation overrides forwarded from the UIProcess InspectorBackendSyncState.
    // Only emulatedMedia is materialized today; it is applied through the EmulationManager so a
    // process joining under Site Isolation inherits the config. See webkit.org/b/308897.
    void setEmulationOverrides(Inspector::EmulationOverrides&&);

    Inspector::EmulationManager& emulationManager() { return m_emulationManager.get(); }

    // Set up / tear down every per-frame instrumentation agent for a frame. Callers
    // don't need to know which agents are frame-scoped; each helper no-ops unless its
    // domain is enabled.
    void ensureInstrumentationForFrame(WebCore::LocalFrame&);
    void removeInstrumentationForFrame(WebCore::FrameIdentifier);

    void setFrontendConnection(IPC::Connection::Handle&&);

    void disconnectFromPage() { close(); }

private:
    friend class WebInspectorBackendClient;

    explicit WebInspectorBackend(WebPage&);

    bool canAttachWindow();

    // Called from WebInspectorBackendClient
    void openLocalInspectorFrontend();
    void closeFrontendConnection();

    void bringToFront();

    void whenFrontendConnectionEstablished(Function<void(IPC::Connection&)>&&);

    void ensureNetworkInstrumentationForFrame(WebCore::LocalFrame&);
    void ensurePageInstrumentationForFrame(WebCore::LocalFrame&);

    // Connect the page's remote instrumentation (bumping the per-process frontend counter and
    // registering the page's instrumenting agents) on the first enabled domain, and disconnect on
    // the last. Network and page instrumentation share one connection so the counter stays balanced.
    void connectRemoteInstrumentationIfNeeded();
    void disconnectRemoteInstrumentationIfNeeded();

    WeakPtr<WebPage> m_page;

    // Owned eagerly (created in the ctor): the backend's lifetime is the manager's lifetime, and the
    // manager reaches the page back through this backend. See webkit.org/b/308897.
    const Ref<Inspector::EmulationManager> m_emulationManager;

    RefPtr<IPC::Connection> m_frontendConnection;
    Vector<Function<void(IPC::Connection&)>> m_frontendConnectionActions;

    bool m_attached { false };
    bool m_previousCanAttach { false };

    // Must outlive m_frameNetworkAgentProxies below: each proxy holds a reference to
    // m_extraRequestHeaders and reads it in willSendRequest.
    WebCore::HTTPHeaderMap m_extraRequestHeaders;
    bool m_resourceCachingDisabled { false };

    HashMap<WebCore::FrameIdentifier, std::unique_ptr<FrameNetworkAgentProxy>> m_frameNetworkAgentProxies;
    UniqueRef<BackendResourceDataStore> m_resourceDataStore;
    bool m_networkInstrumentationEnabled { false };

    HashMap<WebCore::FrameIdentifier, std::unique_ptr<PageAgentProxy>> m_framePageAgentProxies;
    bool m_pageInstrumentationEnabled { false };

    // Latest paint-rects toggle for this process, remembered so a proxy created later by
    // ensurePageInstrumentationForFrame starts in the correct state (the UIProcess replays state
    // only on the first (process, page) registration).
    bool m_showPaintRects { false };
};

} // namespace WebKit
