/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InspectorInstrumentation.h"
#include "InspectorPageAgent.h"
#include "InspectorWebAgentBase.h"
#include "NetworkResourcesData.h"
#include "WebSocket.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/JSONValues.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/TZoneMalloc.h>

namespace Inspector {
class ConsoleMessage;
class InjectedScriptManager;
class PendingInterceptRequest;
class PendingInterceptResponse;
enum class ResourceType;
struct Intercept;
}

namespace WebCore {

class CachedResource;
class Document;
class DocumentLoader;
class DocumentThreadableLoader;
class NetworkLoadMetrics;
class NetworkResourcesData;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class TextResourceDecoder;
class WebSocket;

struct WebSocketFrame;

class InspectorNetworkAgent : public InspectorAgentBase, public Inspector::NetworkBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(InspectorNetworkAgent);
    WTF_MAKE_NONCOPYABLE(InspectorNetworkAgent);
public:
    ~InspectorNetworkAgent() override;

    static constexpr ASCIILiteral errorDomain() { return "InspectorNetworkAgent"_s; }

    // InspectorAgentBase
    void didCreateFrontendAndBackend() final;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) final;

    // NetworkBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable() final;
    Inspector::Protocol::ErrorStringOr<void> disable() final;
    Inspector::Protocol::ErrorStringOr<void> setExtraHTTPHeaders(Ref<JSON::Object>&&) final;
    Inspector::Protocol::ErrorStringOr<std::tuple<String, bool /* base64Encoded */>> getResponseBody(const Inspector::Protocol::Network::RequestId&) final;
    Inspector::Protocol::ErrorStringOr<void> setResourceCachingDisabled(bool) final;
    void loadResource(const Inspector::Protocol::Network::FrameId&, const String& url, Ref<LoadResourceCallback>&&) final;
    Inspector::Protocol::ErrorStringOr<String> getSerializedCertificate(const Inspector::Protocol::Network::RequestId&) final;
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> resolveWebSocket(const Inspector::Protocol::Network::RequestId&, const String& objectGroup) final;
    Inspector::Protocol::ErrorStringOr<void> setInterceptionEnabled(bool) final;
    Inspector::Protocol::ErrorStringOr<void> addInterception(const String& url, Inspector::Protocol::Network::NetworkStage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex) final;
    Inspector::Protocol::ErrorStringOr<void> removeInterception(const String& url, Inspector::Protocol::Network::NetworkStage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex) final;
    Inspector::Protocol::ErrorStringOr<void> interceptContinue(const Inspector::Protocol::Network::RequestId&, Inspector::Protocol::Network::NetworkStage) final;
    Inspector::Protocol::ErrorStringOr<void> interceptWithRequest(const Inspector::Protocol::Network::RequestId&, const String& url, const String& method, RefPtr<JSON::Object>&& headers, const String& postData) final;
    Inspector::Protocol::ErrorStringOr<void> interceptWithResponse(const Inspector::Protocol::Network::RequestId&, const String& content, bool base64Encoded, const String& mimeType, std::optional<int>&& status, const String& statusText, RefPtr<JSON::Object>&& headers) final;
    Inspector::Protocol::ErrorStringOr<void> interceptRequestWithResponse(const Inspector::Protocol::Network::RequestId&, const String& content, bool base64Encoded, const String& mimeType, int status, const String& statusText, Ref<JSON::Object>&& headers) final;
    Inspector::Protocol::ErrorStringOr<void> interceptRequestWithError(const Inspector::Protocol::Network::RequestId&, Inspector::Protocol::Network::ResourceErrorType) final;
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    Inspector::Protocol::ErrorStringOr<void> setEmulatedConditions(std::optional<int>&& bytesPerSecondLimit) final;
#endif

    // InspectorInstrumentation
    void willRecalculateStyle();
    void didRecalculateStyle();
    void willSendRequest(ResourceLoaderIdentifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse, const CachedResource*, ResourceLoader*);
    void willSendRequestOfType(ResourceLoaderIdentifier, DocumentLoader*, ResourceRequest&, InspectorInstrumentation::LoadType);
    void didReceiveResponse(ResourceLoaderIdentifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didReceiveData(ResourceLoaderIdentifier, const SharedBuffer*, int expectedDataLength, int encodedDataLength);
    void didFinishLoading(ResourceLoaderIdentifier, DocumentLoader*, const NetworkLoadMetrics&, ResourceLoader*);
    void didFailLoading(ResourceLoaderIdentifier, DocumentLoader*, const ResourceError&);
    void didLoadResourceFromMemoryCache(DocumentLoader*, CachedResource&);
    void didReceiveThreadableLoaderResponse(ResourceLoaderIdentifier, DocumentThreadableLoader&);
    void willLoadXHRSynchronously();
    void didLoadXHRSynchronously();
    void didReceiveScriptResponse(ResourceLoaderIdentifier);
    void willDestroyCachedResource(CachedResource&);
    void didCreateWebSocket(WebSocketChannelIdentifier, const URL& requestURL);
    void willSendWebSocketHandshakeRequest(WebSocketChannelIdentifier, const ResourceRequest&);
    void didReceiveWebSocketHandshakeResponse(WebSocketChannelIdentifier, const ResourceResponse&);
    void didCloseWebSocket(WebSocketChannelIdentifier);
    void didReceiveWebSocketFrame(WebSocketChannelIdentifier, const WebSocketFrame&);
    void didSendWebSocketFrame(WebSocketChannelIdentifier, const WebSocketFrame&);
    void didReceiveWebSocketFrameError(WebSocketChannelIdentifier, const String&);
    void mainFrameNavigated(DocumentLoader&);
    void setInitialScriptContent(ResourceLoaderIdentifier, const String& sourceString);
    void didScheduleStyleRecalculation(Document&);
    bool willIntercept(const ResourceRequest&);
    bool shouldInterceptRequest(const ResourceLoader&);
    bool shouldInterceptResponse(const ResourceResponse&);
    void interceptResponse(const ResourceResponse&, ResourceLoaderIdentifier, CompletionHandler<void(const ResourceResponse&, RefPtr<FragmentedSharedBuffer>)>&&);
    void interceptRequest(ResourceLoader&, Function<void(const ResourceRequest&)>&&);

    void searchOtherRequests(const JSC::Yarr::RegularExpression&, Ref<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>&);
    void searchInRequest(Inspector::Protocol::ErrorString&, const Inspector::Protocol::Network::RequestId&, const String& query, bool caseSensitive, bool isRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>&);

protected:
    InspectorNetworkAgent(WebAgentContext&, const NetworkResourcesData::Settings&);

    virtual Inspector::Protocol::Network::LoaderId loaderIdentifier(DocumentLoader*) = 0;
    virtual Inspector::Protocol::Network::FrameId frameIdentifier(DocumentLoader*) = 0;
    virtual Vector<Ref<WebSocket>> activeWebSockets() WTF_REQUIRES_LOCK(WebSocket::allActiveWebSocketsLock()) = 0;
    virtual void setResourceCachingDisabledInternal(bool) = 0;
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    virtual bool setEmulatedConditionsInternal(std::optional<int>&& bytesPerSecondLimit) = 0;
#endif
    virtual ScriptExecutionContext* scriptExecutionContext(Inspector::Protocol::ErrorString&, const Inspector::Protocol::Network::FrameId&) = 0;
    virtual void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) = 0;
    virtual bool shouldForceBufferingNetworkResourceData() const = 0;

private:
    void willSendRequest(ResourceLoaderIdentifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse, Inspector::ResourceType, ResourceLoader*);

    bool shouldIntercept(URL, Inspector::Protocol::Network::NetworkStage);
    void continuePendingRequests();
    void continuePendingResponses();

    RefPtr<WebSocket> webSocketForRequestId(const Inspector::Protocol::Network::RequestId&);

    Ref<Inspector::Protocol::Network::Initiator> buildInitiatorObject(Document*, const ResourceRequest* = nullptr);
    Ref<Inspector::Protocol::Network::ResourceTiming> buildObjectForTiming(const NetworkLoadMetrics&, ResourceLoader&);
    Ref<Inspector::Protocol::Network::Metrics> buildObjectForMetrics(const NetworkLoadMetrics&);
    RefPtr<Inspector::Protocol::Network::Response> buildObjectForResourceResponse(const ResourceResponse&, ResourceLoader*);
    Ref<Inspector::Protocol::Network::CachedResource> buildObjectForCachedResource(CachedResource*);

    double timestamp();

    const UniqueRef<Inspector::NetworkFrontendDispatcher> m_frontendDispatcher;
    const Ref<Inspector::NetworkBackendDispatcher> m_backendDispatcher;
    const CheckedRef<Inspector::InjectedScriptManager> m_injectedScriptManager;

    const UniqueRef<NetworkResourcesData> m_resourcesData;

    MemoryCompactRobinHoodHashMap<String, String> m_extraRequestHeaders;
    HashSet<ResourceLoaderIdentifier> m_hiddenRequestIdentifiers;

    Vector<Inspector::Intercept> m_intercepts;
    MemoryCompactRobinHoodHashMap<String, std::unique_ptr<Inspector::PendingInterceptRequest>> m_pendingInterceptRequests;
    MemoryCompactRobinHoodHashMap<String, std::unique_ptr<Inspector::PendingInterceptResponse>> m_pendingInterceptResponses;

    // FIXME: InspectorNetworkAgent should not be aware of style recalculation.
    RefPtr<Inspector::Protocol::Network::Initiator> m_styleRecalculationInitiator;
    bool m_isRecalculatingStyle { false };

    bool m_enabled { false };
    bool m_loadingXHRSynchronously { false };
    bool m_interceptionEnabled { false };
};

} // namespace WebCore
