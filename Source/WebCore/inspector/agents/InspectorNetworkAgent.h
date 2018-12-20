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
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/text/WTFString.h>

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

typedef String ErrorString;

class InspectorNetworkAgent : public InspectorAgentBase, public Inspector::NetworkBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorNetworkAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InspectorNetworkAgent(WebAgentContext&);
    virtual ~InspectorNetworkAgent();

    static bool shouldTreatAsText(const String& mimeType);
    static Ref<TextResourceDecoder> createTextDecoder(const String& mimeType, const String& textEncodingName);
    static Optional<String> textContentForCachedResource(CachedResource&);
    static bool cachedResourceContent(CachedResource&, String* result, bool* base64Encoded);

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

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

    void searchOtherRequests(const JSC::Yarr::RegularExpression&, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>&);
    void searchInRequest(ErrorString&, const String& requestId, const String& query, bool caseSensitive, bool isRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>&);

    // Called from frontend.
    void enable(ErrorString&) final;
    void disable(ErrorString&) final;
    void setExtraHTTPHeaders(ErrorString&, const JSON::Object& headers) final;
    void getResponseBody(ErrorString&, const String& requestId, String* content, bool* base64Encoded) final;
    void setResourceCachingDisabled(ErrorString&, bool disabled) final;
    void loadResource(const String& frameId, const String& url, Ref<LoadResourceCallback>&&) final;
    void getSerializedCertificate(ErrorString&, const String& requestId, String* serializedCertificate) final;
    void resolveWebSocket(ErrorString&, const String& requestId, const String* objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>&) final;

    virtual String loaderIdentifier(DocumentLoader*) = 0;
    virtual String frameIdentifier(DocumentLoader*) = 0;
    virtual Vector<WebSocket*> activeWebSockets(const LockHolder&) = 0;
    virtual void setResourceCachingDisabled(bool) = 0;
    virtual ScriptExecutionContext* scriptExecutionContext(ErrorString&, const String& frameId) = 0;
    virtual bool shouldForceBufferingNetworkResourceData() const = 0;

private:
    void enable();

    void willSendRequest(unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse, InspectorPageAgent::ResourceType);

    WebSocket* webSocketForRequestId(const String& requestId);

    RefPtr<Inspector::Protocol::Network::Initiator> buildInitiatorObject(Document*, Optional<const ResourceRequest&> = WTF::nullopt);
    Ref<Inspector::Protocol::Network::ResourceTiming> buildObjectForTiming(const NetworkLoadMetrics&, ResourceLoader&);
    Ref<Inspector::Protocol::Network::Metrics> buildObjectForMetrics(const NetworkLoadMetrics&);
    RefPtr<Inspector::Protocol::Network::Response> buildObjectForResourceResponse(const ResourceResponse&, ResourceLoader*);
    Ref<Inspector::Protocol::Network::CachedResource> buildObjectForCachedResource(CachedResource*);

    double timestamp();

    std::unique_ptr<Inspector::NetworkFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::NetworkBackendDispatcher> m_backendDispatcher;
    Inspector::InjectedScriptManager& m_injectedScriptManager;

    // FIXME: InspectorNetworkAgent should not be aware of style recalculation.
    RefPtr<Inspector::Protocol::Network::Initiator> m_styleRecalculationInitiator;
    bool m_isRecalculatingStyle { false };

    std::unique_ptr<NetworkResourcesData> m_resourcesData;
    bool m_enabled { false };
    bool m_loadingXHRSynchronously { false };
    HashMap<String, String> m_extraRequestHeaders;
    HashSet<unsigned long> m_hiddenRequestIdentifiers;
};

} // namespace WebCore
