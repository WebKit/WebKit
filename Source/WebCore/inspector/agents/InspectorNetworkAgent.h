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
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/Forward.h>
#include <wtf/JSONValues.h>

namespace Inspector {
class InjectedScriptManager;
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
    WTF_MAKE_NONCOPYABLE(InspectorNetworkAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~InspectorNetworkAgent() override;

    static bool shouldTreatAsText(const String& mimeType);
    static Ref<TextResourceDecoder> createTextDecoder(const String& mimeType, const String& textEncodingName);
    static Optional<String> textContentForCachedResource(CachedResource&);
    static bool cachedResourceContent(CachedResource&, String* result, bool* base64Encoded);

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) final;
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
    Inspector::Protocol::ErrorStringOr<void> addInterception(const String& url, Inspector::Protocol::Network::NetworkStage, Optional<bool>&& caseSensitive, Optional<bool>&& isRegex) final;
    Inspector::Protocol::ErrorStringOr<void> removeInterception(const String& url, Inspector::Protocol::Network::NetworkStage, Optional<bool>&& caseSensitive, Optional<bool>&& isRegex) final;
    Inspector::Protocol::ErrorStringOr<void> interceptContinue(const Inspector::Protocol::Network::RequestId&, Inspector::Protocol::Network::NetworkStage) final;
    Inspector::Protocol::ErrorStringOr<void> interceptWithRequest(const Inspector::Protocol::Network::RequestId&, const String& url, const String& method, RefPtr<JSON::Object>&& headers, const String& postData) final;
    Inspector::Protocol::ErrorStringOr<void> interceptWithResponse(const Inspector::Protocol::Network::RequestId&, const String& content, bool base64Encoded, const String& mimeType, Optional<int>&& status, const String& statusText, RefPtr<JSON::Object>&& headers) final;
    Inspector::Protocol::ErrorStringOr<void> interceptRequestWithResponse(const Inspector::Protocol::Network::RequestId&, const String& content, bool base64Encoded, const String& mimeType, int status, const String& statusText, Ref<JSON::Object>&& headers) final;
    Inspector::Protocol::ErrorStringOr<void> interceptRequestWithError(const Inspector::Protocol::Network::RequestId&, Inspector::Protocol::Network::ResourceErrorType) final;

    // InspectorInstrumentation
    void willRecalculateStyle();
    void didRecalculateStyle();
    void willSendRequest(unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    void willSendRequestOfType(unsigned long identifier, DocumentLoader*, ResourceRequest&, InspectorInstrumentation::LoadType);
    void didReceiveResponse(unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    void didFinishLoading(unsigned long identifier, DocumentLoader*, const NetworkLoadMetrics&, ResourceLoader*);
    void didFailLoading(unsigned long identifier, DocumentLoader*, const ResourceError&);
    void didLoadResourceFromMemoryCache(DocumentLoader*, CachedResource&);
    void didReceiveThreadableLoaderResponse(unsigned long identifier, DocumentThreadableLoader&);
    void willLoadXHRSynchronously();
    void didLoadXHRSynchronously();
    void didReceiveScriptResponse(unsigned long identifier);
    void willDestroyCachedResource(CachedResource&);
    void didCreateWebSocket(unsigned long identifier, const URL& requestURL);
    void willSendWebSocketHandshakeRequest(unsigned long identifier, const ResourceRequest&);
    void didReceiveWebSocketHandshakeResponse(unsigned long identifier, const ResourceResponse&);
    void didCloseWebSocket(unsigned long identifier);
    void didReceiveWebSocketFrame(unsigned long identifier, const WebSocketFrame&);
    void didSendWebSocketFrame(unsigned long identifier, const WebSocketFrame&);
    void didReceiveWebSocketFrameError(unsigned long identifier, const String&);
    void mainFrameNavigated(DocumentLoader&);
    void setInitialScriptContent(unsigned long identifier, const String& sourceString);
    void didScheduleStyleRecalculation(Document&);
    bool willIntercept(const ResourceRequest&);
    bool shouldInterceptRequest(const ResourceRequest&);
    bool shouldInterceptResponse(const ResourceResponse&);
    void interceptResponse(const ResourceResponse&, unsigned long identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&&);
    void interceptRequest(ResourceLoader&, Function<void(const ResourceRequest&)>&&);

    void searchOtherRequests(const JSC::Yarr::RegularExpression&, Ref<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>&);
    void searchInRequest(Inspector::Protocol::ErrorString&, const Inspector::Protocol::Network::RequestId&, const String& query, bool caseSensitive, bool isRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>&);

protected:
    InspectorNetworkAgent(WebAgentContext&);

    virtual Inspector::Protocol::Network::LoaderId loaderIdentifier(DocumentLoader*) = 0;
    virtual Inspector::Protocol::Network::FrameId frameIdentifier(DocumentLoader*) = 0;
    virtual Vector<WebSocket*> activeWebSockets(const LockHolder&) = 0;
    virtual void setResourceCachingDisabledInternal(bool) = 0;
    virtual ScriptExecutionContext* scriptExecutionContext(Inspector::Protocol::ErrorString&, const Inspector::Protocol::Network::FrameId&) = 0;
    virtual bool shouldForceBufferingNetworkResourceData() const = 0;

private:
    void willSendRequest(unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse, InspectorPageAgent::ResourceType);

    bool shouldIntercept(URL, Inspector::Protocol::Network::NetworkStage);
    void continuePendingRequests();
    void continuePendingResponses();

    WebSocket* webSocketForRequestId(const Inspector::Protocol::Network::RequestId&);

    Ref<Inspector::Protocol::Network::Initiator> buildInitiatorObject(Document*, const ResourceRequest* = nullptr);
    Ref<Inspector::Protocol::Network::ResourceTiming> buildObjectForTiming(const NetworkLoadMetrics*, ResourceLoader&);
    Ref<Inspector::Protocol::Network::Metrics> buildObjectForMetrics(const NetworkLoadMetrics&);
    RefPtr<Inspector::Protocol::Network::Response> buildObjectForResourceResponse(const ResourceResponse&, ResourceLoader*);
    Ref<Inspector::Protocol::Network::CachedResource> buildObjectForCachedResource(CachedResource*);

    double timestamp();

    class PendingInterceptRequest {
        WTF_MAKE_NONCOPYABLE(PendingInterceptRequest);
        WTF_MAKE_FAST_ALLOCATED;
    public:
        PendingInterceptRequest(RefPtr<ResourceLoader> loader, Function<void(const ResourceRequest&)>&& callback)
            : m_loader(loader)
            , m_completionCallback(WTFMove(callback))
        { }

        void continueWithOriginalRequest()
        {
            if (!m_loader->reachedTerminalState())
                m_completionCallback(m_loader->request());
        }

        void continueWithRequest(const ResourceRequest& request)
        {
            m_completionCallback(request);
        }

        PendingInterceptRequest() = default;
        RefPtr<ResourceLoader> m_loader;
        Function<void(const ResourceRequest&)> m_completionCallback;
    };

    class PendingInterceptResponse {
        WTF_MAKE_NONCOPYABLE(PendingInterceptResponse);
        WTF_MAKE_FAST_ALLOCATED;
    public:
        PendingInterceptResponse(const ResourceResponse& originalResponse, CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)>&& completionHandler)
            : m_originalResponse(originalResponse)
            , m_completionHandler(WTFMove(completionHandler))
        { }

        ~PendingInterceptResponse()
        {
            ASSERT(m_responded);
        }

        ResourceResponse originalResponse() { return m_originalResponse; }

        void respondWithOriginalResponse()
        {
            respond(m_originalResponse, nullptr);
        }

        void respond(const ResourceResponse& response, RefPtr<SharedBuffer> data)
        {
            ASSERT(!m_responded);
            if (m_responded)
                return;

            m_responded = true;

            m_completionHandler(response, data);
        }

    private:
        ResourceResponse m_originalResponse;
        CompletionHandler<void(const ResourceResponse&, RefPtr<SharedBuffer>)> m_completionHandler;
        bool m_responded { false };
    };

    std::unique_ptr<Inspector::NetworkFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::NetworkBackendDispatcher> m_backendDispatcher;
    Inspector::InjectedScriptManager& m_injectedScriptManager;

    std::unique_ptr<NetworkResourcesData> m_resourcesData;

    HashMap<String, String> m_extraRequestHeaders;
    HashSet<unsigned long> m_hiddenRequestIdentifiers;

    struct Intercept {
        String url;
        bool caseSensitive { true };
        bool isRegex { false };
        Inspector::Protocol::Network::NetworkStage networkStage { Inspector::Protocol::Network::NetworkStage::Response };

        inline bool operator==(const Intercept& other) const
        {
            return url == other.url
                && caseSensitive == other.caseSensitive
                && isRegex == other.isRegex
                && networkStage == other.networkStage;
        }
    };
    Vector<Intercept> m_intercepts;
    HashMap<String, std::unique_ptr<PendingInterceptRequest>> m_pendingInterceptRequests;
    HashMap<String, std::unique_ptr<PendingInterceptResponse>> m_pendingInterceptResponses;

    // FIXME: InspectorNetworkAgent should not be aware of style recalculation.
    RefPtr<Inspector::Protocol::Network::Initiator> m_styleRecalculationInitiator;
    bool m_isRecalculatingStyle { false };

    bool m_enabled { false };
    bool m_loadingXHRSynchronously { false };
    bool m_interceptionEnabled { false };
};

} // namespace WebCore
