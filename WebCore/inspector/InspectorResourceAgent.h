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

#include "PlatformString.h"

#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

#if ENABLE(INSPECTOR)

namespace WTF {
class String;
}

namespace WebCore {

class CachedResource;
class Document;
class DocumentLoader;
class Frame;
class InspectorArray;
class InspectorObject;
class InspectorFrontend;
class KURL;
class Page;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;

#if ENABLE(WEB_SOCKETS)
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
#endif

class InspectorResourceAgent : public RefCounted<InspectorResourceAgent> {
public:
    static PassRefPtr<InspectorResourceAgent> create(Page* page, InspectorFrontend* frontend)
    {
        return adoptRef(new InspectorResourceAgent(page, frontend));
    }

    static bool resourceContent(Frame*, const KURL&, String* result);
    static bool resourceContentBase64(Frame*, const KURL&, String* result);
    static PassRefPtr<SharedBuffer> resourceData(Frame*, const KURL&, String* textEncodingName);
    static CachedResource* cachedResource(Frame*, const KURL&);

    ~InspectorResourceAgent();

    void identifierForInitialRequest(unsigned long identifier, const KURL&, DocumentLoader*);
    void willSendRequest(unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    void markResourceAsCached(unsigned long identifier);
    void didReceiveResponse(unsigned long identifier, DocumentLoader* laoder, const ResourceResponse&);
    void didReceiveContentLength(unsigned long identifier, int lengthReceived);
    void didFinishLoading(unsigned long identifier, double finishTime);
    void didFailLoading(unsigned long identifier, const ResourceError&);
    void didLoadResourceFromMemoryCache(DocumentLoader*, const CachedResource*);
    void setInitialContent(unsigned long identifier, const String& sourceString, const String& type);
    void didCommitLoad(DocumentLoader*);
    void frameDetachedFromParent(Frame*);

#if ENABLE(WEB_SOCKETS)
    void didCreateWebSocket(unsigned long identifier, const KURL& requestURL);
    void willSendWebSocketHandshakeRequest(unsigned long identifier, const WebSocketHandshakeRequest&);
    void didReceiveWebSocketHandshakeResponse(unsigned long identifier, const WebSocketHandshakeResponse&);
    void didCloseWebSocket(unsigned long identifier);
#endif

    Frame* frameForId(unsigned long);

    // Called from frontend 
    void cachedResources(RefPtr<InspectorObject>*);
    void resourceContent(unsigned long frameID, const String& url, bool base64Encode, String* content);

private:
    InspectorResourceAgent(Page* page, InspectorFrontend* frontend);

    Page* m_page;
    InspectorFrontend* m_frontend;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorResourceAgent_h)
