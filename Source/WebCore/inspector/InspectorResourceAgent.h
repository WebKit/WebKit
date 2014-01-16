/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorResourceAgent_h
#define InspectorResourceAgent_h

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INSPECTOR)

namespace Inspector {
class InspectorArray;
class InspectorObject;
}

namespace WebCore {

class CachedResource;
class Document;
class DocumentLoader;
class FormData;
class Frame;
class HTTPHeaderMap;
class InspectorClient;
class InspectorPageAgent;
class InstrumentingAgents;
class URL;
class NetworkResourcesData;
class Page;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;
class ThreadableLoaderClient;
class XHRReplayData;
class XMLHttpRequest;

#if ENABLE(WEB_SOCKETS)
struct WebSocketFrame;
#endif

typedef String ErrorString;

class InspectorResourceAgent : public InspectorAgentBase, public Inspector::InspectorNetworkBackendDispatcherHandler {
public:
    InspectorResourceAgent(InstrumentingAgents*, InspectorPageAgent*, InspectorClient*);
    ~InspectorResourceAgent();

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend() override;

    void willSendRequest(unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    void markResourceAsCached(unsigned long identifier);
    void didReceiveResponse(unsigned long identifier, DocumentLoader* laoder, const ResourceResponse&, ResourceLoader*);
    void didReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    void didFinishLoading(unsigned long identifier, DocumentLoader*, double finishTime);
    void didFailLoading(unsigned long identifier, DocumentLoader*, const ResourceError&);
    void didLoadResourceFromMemoryCache(DocumentLoader*, CachedResource*);
    void mainFrameNavigated(DocumentLoader*);
    void setInitialScriptContent(unsigned long identifier, const String& sourceString);
    void didReceiveScriptResponse(unsigned long identifier);

    void documentThreadableLoaderStartedLoadingForClient(unsigned long identifier, ThreadableLoaderClient*);
    void willLoadXHR(ThreadableLoaderClient*, const String& method, const URL&, bool async, PassRefPtr<FormData> body, const HTTPHeaderMap& headers, bool includeCrendentials);
    void didFailXHRLoading(ThreadableLoaderClient*);
    void didFinishXHRLoading(ThreadableLoaderClient*, unsigned long identifier, const String& sourceString);
    void didReceiveXHRResponse(unsigned long identifier);
    void willLoadXHRSynchronously();
    void didLoadXHRSynchronously();

    void willDestroyCachedResource(CachedResource*);

    // FIXME: InspectorResourceAgent should now be aware of style recalculation.
    void willRecalculateStyle();
    void didRecalculateStyle();
    void didScheduleStyleRecalculation(Document*);

    PassRefPtr<Inspector::TypeBuilder::Network::Initiator> buildInitiatorObject(Document*);

#if ENABLE(WEB_SOCKETS)
    void didCreateWebSocket(unsigned long identifier, const URL& requestURL);
    void willSendWebSocketHandshakeRequest(unsigned long identifier, const ResourceRequest&);
    void didReceiveWebSocketHandshakeResponse(unsigned long identifier, const ResourceResponse&);
    void didCloseWebSocket(unsigned long identifier);
    void didReceiveWebSocketFrame(unsigned long identifier, const WebSocketFrame&);
    void didSendWebSocketFrame(unsigned long identifier, const WebSocketFrame&);
    void didReceiveWebSocketFrameError(unsigned long identifier, const String&);
#endif

    // called from Internals for layout test purposes.
    void setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize);

    // Called from frontend
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void setExtraHTTPHeaders(ErrorString*, const RefPtr<Inspector::InspectorObject>&);
    virtual void getResponseBody(ErrorString*, const String& requestId, String* content, bool* base64Encoded);

    virtual void replayXHR(ErrorString*, const String& requestId);

    virtual void canClearBrowserCache(ErrorString*, bool*);
    virtual void clearBrowserCache(ErrorString*);
    virtual void canClearBrowserCookies(ErrorString*, bool*);
    virtual void clearBrowserCookies(ErrorString*);
    virtual void setCacheDisabled(ErrorString*, bool cacheDisabled);

private:
    void enable();

    InspectorPageAgent* m_pageAgent;
    InspectorClient* m_client;
    std::unique_ptr<Inspector::InspectorNetworkFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorNetworkBackendDispatcher> m_backendDispatcher;
    OwnPtr<NetworkResourcesData> m_resourcesData;
    bool m_enabled;
    bool m_cacheDisabled;
    bool m_loadingXHRSynchronously;
    RefPtr<Inspector::InspectorObject> m_extraRequestHeaders;

    typedef HashMap<ThreadableLoaderClient*, RefPtr<XHRReplayData>> PendingXHRReplayDataMap;
    PendingXHRReplayDataMap m_pendingXHRReplayData;
    // FIXME: InspectorResourceAgent should now be aware of style recalculation.
    RefPtr<Inspector::TypeBuilder::Network::Initiator> m_styleRecalculationInitiator;
    bool m_isRecalculatingStyle;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorResourceAgent_h)
