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
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/InspectorResourceType.h>
#include <WebCore/InspectorResourceUtilities.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <optional>
#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class NetworkLoadMetrics;
}

namespace WebKit {
class WebProcessProxy;
}

namespace Inspector {

using ResourceID = WebCore::ScopedResourceLoaderIdentifier;
using FrameID = WebCore::FrameIdentifier;

class ProxyingNetworkAgent : public RefCounted<ProxyingNetworkAgent>, public WebKit::InspectorAgentBase, public NetworkBackendDispatcherHandler, public IPC::MessageReceiver, public CanMakeCheckedPtr<ProxyingNetworkAgent> {
    WTF_MAKE_TZONE_ALLOCATED(ProxyingNetworkAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProxyingNetworkAgent);
    WTF_MAKE_NONCOPYABLE(ProxyingNetworkAgent);
public:
    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);

    ProxyingNetworkAgent(WebKit::WebPageAgentContext&);
    ~ProxyingNetworkAgent() override;

    // RefCounted
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    // InspectorAgentBase
    void didCreateFrontendAndBackend() final;
    void willDestroyFrontendAndBackend(DisconnectReason) final;

    bool isEnabled() const { return m_enabled; }
    void enableInstrumentationForProcess(WebKit::WebProcessProxy&, WebCore::PageIdentifier);
    void disableInstrumentationForProcess(WebKit::WebProcessProxy&, WebCore::PageIdentifier);

    // NetworkBackendDispatcherHandler
    CommandResult<void> enable() final;
    CommandResult<void> disable() final;
    CommandResult<void> setExtraHTTPHeaders(Ref<JSON::Object>&&) final;
    void getResponseBody(StringImpl*, Ref<GetResponseBodyCallback>&&) final;
    CommandResult<void> setResourceCachingDisabled(bool) final;
    CommandResult<void> setClearResourceDataOnNavigate(bool) final;
    void loadResource(StringImpl*, StringImpl* url, Ref<LoadResourceCallback>&&) final;
    void getSerializedCertificate(StringImpl*, Ref<GetSerializedCertificateCallback>&&) final;
    CommandResult<Ref<Protocol::Runtime::RemoteObject>> resolveWebSocket(StringImpl*, StringImpl* objectGroup) final;
    CommandResult<void> setInterceptionEnabled(bool) final;
    CommandResult<void> addInterception(StringImpl* url, Protocol::Network::NetworkStage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex) final;
    CommandResult<void> removeInterception(StringImpl* url, Protocol::Network::NetworkStage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex) final;
    CommandResult<void> interceptContinue(StringImpl*, Protocol::Network::NetworkStage) final;
    CommandResult<void> interceptWithRequest(StringImpl*, StringImpl* url, StringImpl* method, RefPtr<JSON::Object>&& headers, StringImpl* postData) final;
    CommandResult<void> interceptWithResponse(StringImpl*, StringImpl* content, bool base64Encoded, StringImpl* mimeType, std::optional<int>&& status, StringImpl* statusText, RefPtr<JSON::Object>&& headers) final;
    CommandResult<void> interceptRequestWithResponse(StringImpl*, StringImpl* content, bool base64Encoded, StringImpl* mimeType, int status, StringImpl* statusText, Ref<JSON::Object>&& headers) final;
    CommandResult<void> interceptRequestWithError(StringImpl*, Protocol::Network::ResourceErrorType) final;
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    CommandResult<void> setEmulatedConditions(std::optional<int>&& bandwidth, std::optional<int>&& latency) final;
#endif

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC message handlers from WebProcess FrameNetworkAgentProxy
    void requestWillBeSent(ResourceID, FrameID, StringImpl* loaderId, StringImpl* targetID, StringImpl* documentURL, const WebCore::ResourceRequest&, const RequestExtras&, std::optional<WebCore::ResourceResponse>&&, ResourceType, double timestamp, double walltime, InitiatorData&&);
    void responseReceived(ResourceID, FrameID, StringImpl* loaderId, const WebCore::ResourceResponse&, ResourceType, double timestamp, std::optional<MonotonicTime> resourceLoadStartTime);
    void dataReceived(ResourceID, int dataLength, int encodedDataLength, double timestamp);
    void loadingFinished(ResourceID, double timestamp, StringImpl* sourceMapURL, WebCore::NetworkLoadMetrics&&);
    void loadingFailed(ResourceID, double timestamp, StringImpl* errorText, bool canceled);
    void requestServedFromMemoryCache(ResourceID, FrameID, StringImpl* loaderId, StringImpl* documentURL, const URL&, const WebCore::ResourceResponse&, ResourceType, StringImpl* sourceMapURL, uint64_t bodySize, double timestamp, InitiatorData&&);

    void removeAllRegisteredReceivers();

    const UniqueRef<NetworkFrontendDispatcher> m_frontendDispatcher;
    const Ref<NetworkBackendDispatcher> m_backendDispatcher;
    WeakRef<WebKit::WebPageProxy> m_inspectedPage;

    bool m_enabled { false };
    HashMap<std::pair<WebCore::ProcessIdentifier, WebCore::PageIdentifier>, unsigned> m_instrumentedProcessPageCounts;

    WebCore::HTTPHeaderMap m_extraRequestHeaders;
    bool m_resourceCachingDisabled { false };

    // Pin each instrumented WebProcessProxy alive while we hold an IPC message
    // receiver registration on it. Without this, the process can be destructed
    // before ~ProxyingNetworkAgent runs, leaving the receiver registered against
    // a map that has already gone away. The receiver's m_messageReceiverMapCount
    // then stays nonzero and ~MessageReceiver fires its debug ASSERT.
    HashMap<WebCore::ProcessIdentifier, Ref<WebKit::WebProcessProxy>> m_pinnedInstrumentedProcesses;
};

} // namespace Inspector
