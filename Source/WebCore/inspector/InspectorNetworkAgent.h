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

#ifndef InspectorNetworkAgent_h
#define InspectorNetworkAgent_h

#include "InspectorWebAgentBase.h"
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>
#include <yarr/RegularExpression.h>

namespace Inspector {
class InspectorObject;
}

namespace WebCore {

class CachedResource;
class Document;
class DocumentLoader;
class InspectorPageAgent;
class NetworkResourcesData;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class ThreadableLoaderClient;
class URL;

#if ENABLE(WEB_SOCKETS)
struct WebSocketFrame;
#endif

typedef String ErrorString;

class InspectorNetworkAgent final : public InspectorAgentBase, public Inspector::NetworkBackendDispatcherHandler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorNetworkAgent(WebAgentContext&, InspectorPageAgent*);
    virtual ~InspectorNetworkAgent();

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // InspectorInstrumentation callbacks.
    void willRecalculateStyle();
    void didRecalculateStyle();
    void willSendRequest(unsigned long identifier, DocumentLoader&, ResourceRequest&, const ResourceResponse& redirectResponse);
    void markResourceAsCached(unsigned long identifier);
    void didReceiveResponse(unsigned long identifier, DocumentLoader&, const ResourceResponse&, ResourceLoader*);
    void didReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    void didFinishLoading(unsigned long identifier, DocumentLoader&, double finishTime);
    void didFailLoading(unsigned long identifier, DocumentLoader&, const ResourceError&);
    void didLoadResourceFromMemoryCache(DocumentLoader&, CachedResource&);
    void didFinishXHRLoading(ThreadableLoaderClient*, unsigned long identifier, const String& sourceString);
    void didReceiveXHRResponse(unsigned long identifier);
    void willLoadXHRSynchronously();
    void didLoadXHRSynchronously();
    void didReceiveScriptResponse(unsigned long identifier);
    void willDestroyCachedResource(CachedResource&);
#if ENABLE(WEB_SOCKETS)
    void didCreateWebSocket(unsigned long identifier, const URL& requestURL);
    void willSendWebSocketHandshakeRequest(unsigned long identifier, const ResourceRequest&);
    void didReceiveWebSocketHandshakeResponse(unsigned long identifier, const ResourceResponse&);
    void didCloseWebSocket(unsigned long identifier);
    void didReceiveWebSocketFrame(unsigned long identifier, const WebSocketFrame&);
    void didSendWebSocketFrame(unsigned long identifier, const WebSocketFrame&);
    void didReceiveWebSocketFrameError(unsigned long identifier, const String&);
#endif
    void mainFrameNavigated(DocumentLoader&);
    void setInitialScriptContent(unsigned long identifier, const String& sourceString);
    void didScheduleStyleRecalculation(Document&);

    void searchOtherRequests(const JSC::Yarr::RegularExpression&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Page::SearchResult>>&);
    void searchInRequest(ErrorString&, const String& requestId, const String& query, bool caseSensitive, bool isRegex, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::GenericTypes::SearchMatch>>&);

    RefPtr<Inspector::Protocol::Network::Initiator> buildInitiatorObject(Document*);

    // Called from frontend.
    void enable(ErrorString&) override;
    void disable(ErrorString&) override;
    void setExtraHTTPHeaders(ErrorString&, const Inspector::InspectorObject& headers) override;
    void getResponseBody(ErrorString&, const String& requestId, String* content, bool* base64Encoded) override;
    void setCacheDisabled(ErrorString&, bool cacheDisabled) override;
    void loadResource(ErrorString&, const String& frameId, const String& url, Ref<LoadResourceCallback>&&) override;

private:
    void enable();

    double timestamp();

    std::unique_ptr<Inspector::NetworkFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::NetworkBackendDispatcher> m_backendDispatcher;
    InspectorPageAgent* m_pageAgent { nullptr };

    // FIXME: InspectorNetworkAgent should not be aware of style recalculation.
    RefPtr<Inspector::Protocol::Network::Initiator> m_styleRecalculationInitiator;
    bool m_isRecalculatingStyle { false };

    std::unique_ptr<NetworkResourcesData> m_resourcesData;
    bool m_enabled { false };
    bool m_cacheDisabled { false };
    bool m_loadingXHRSynchronously { false };
    HashMap<String, String> m_extraRequestHeaders;
    HashSet<unsigned long> m_hiddenRequestIdentifiers;
};

} // namespace WebCore

#endif // !defined(InspectorNetworkAgent_h)
