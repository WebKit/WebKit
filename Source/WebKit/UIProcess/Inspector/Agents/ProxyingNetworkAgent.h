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
#include <WebCore/InspectorResourceType.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {
class WebProcessProxy;
}

namespace Inspector {

using ResourceID = WebCore::ScopedResourceLoaderIdentifier;
using FrameID = WebCore::FrameIdentifier;
using ContextID = WebCore::ScriptExecutionContextIdentifier;

class ProxyingNetworkAgent : public RefCounted<ProxyingNetworkAgent>, public WebKit::InspectorAgentBase, public NetworkBackendDispatcherHandler, public IPC::MessageReceiver, public CanMakeCheckedPtr<ProxyingNetworkAgent> {
    WTF_MAKE_TZONE_ALLOCATED(ProxyingNetworkAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProxyingNetworkAgent);
    WTF_MAKE_NONCOPYABLE(ProxyingNetworkAgent);
public:
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
    void getResponseBody(const Protocol::Network::RequestId&, Ref<GetResponseBodyCallback>&&) final;
    CommandResult<void> setResourceCachingDisabled(bool) final;
    CommandResult<void> setClearResourceDataOnNavigate(bool) final;
    void loadResource(const Protocol::Network::FrameId&, const String& url, Ref<LoadResourceCallback>&&) final;
    CommandResult<String> getSerializedCertificate(const Protocol::Network::RequestId&) final;
    CommandResult<Ref<Protocol::Runtime::RemoteObject>> resolveWebSocket(const Protocol::Network::RequestId&, const String& objectGroup) final;
    CommandResult<void> setInterceptionEnabled(bool) final;
    CommandResult<void> addInterception(const String& url, Protocol::Network::NetworkStage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex) final;
    CommandResult<void> removeInterception(const String& url, Protocol::Network::NetworkStage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex) final;
    CommandResult<void> interceptContinue(const Protocol::Network::RequestId&, Protocol::Network::NetworkStage) final;
    CommandResult<void> interceptWithRequest(const Protocol::Network::RequestId&, const String& url, const String& method, RefPtr<JSON::Object>&& headers, const String& postData) final;
    CommandResult<void> interceptWithResponse(const Protocol::Network::RequestId&, const String& content, bool base64Encoded, const String& mimeType, std::optional<int>&& status, const String& statusText, RefPtr<JSON::Object>&& headers) final;
    CommandResult<void> interceptRequestWithResponse(const Protocol::Network::RequestId&, const String& content, bool base64Encoded, const String& mimeType, int status, const String& statusText, Ref<JSON::Object>&& headers) final;
    CommandResult<void> interceptRequestWithError(const Protocol::Network::RequestId&, Protocol::Network::ResourceErrorType) final;
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    CommandResult<void> setEmulatedConditions(std::optional<int>&& bytesPerSecondLimit) final;
#endif

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC message handlers from WebProcess FrameNetworkAgentProxy
    void requestWillBeSent(ResourceID, FrameID, ContextID, const String& targetID, const String& documentURL, const WebCore::ResourceRequest&, std::optional<WebCore::ResourceResponse>&&, ResourceType, double timestamp, double walltime);
    void responseReceived(ResourceID, FrameID, ContextID, const WebCore::ResourceResponse&, ResourceType, double timestamp);
    void dataReceived(ResourceID, int dataLength, int encodedDataLength, double timestamp);
    void loadingFinished(ResourceID, double timestamp, const String& sourceMapURL);
    void loadingFailed(ResourceID, double timestamp, const String& errorText, bool canceled);
    void requestServedFromMemoryCache(ResourceID, FrameID, ContextID, const String& documentURL, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, ResourceType, double timestamp);

    void removeAllRegisteredReceivers();

    const UniqueRef<NetworkFrontendDispatcher> m_frontendDispatcher;
    const Ref<NetworkBackendDispatcher> m_backendDispatcher;
    WeakRef<WebKit::WebPageProxy> m_inspectedPage;

    bool m_enabled { false };
    HashMap<std::pair<WebCore::ProcessIdentifier, WebCore::PageIdentifier>, unsigned> m_instrumentedProcessPageCounts;

    // Pin each instrumented WebProcessProxy alive while we hold an IPC message
    // receiver registration on it. Without this, the process can be destructed
    // before ~ProxyingNetworkAgent runs, leaving the receiver registered against
    // a map that has already gone away. The receiver's m_messageReceiverMapCount
    // then stays nonzero and ~MessageReceiver fires its debug ASSERT.
    HashMap<WebCore::ProcessIdentifier, Ref<WebKit::WebProcessProxy>> m_pinnedInstrumentedProcesses;
};

} // namespace Inspector
