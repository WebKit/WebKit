/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorResource_h
#define InspectorResource_h

#include "HTTPHeaderMap.h"
#include "KURL.h"
#include "ScriptString.h"
#include "WebSocketHandshakeRequest.h"
#include "WebSocketHandshakeResponse.h"

#include <wtf/CurrentTime.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class CachedResource;
    class DocumentLoader;
    class Frame;
    class InspectorFrontend;
    class ResourceLoadTiming;
    class ResourceRequest;
    class ResourceResponse;

#if ENABLE(WEB_SOCKETS)
    class WebSocketHandshakeRequest;
    class WebSocketHandshakeResponse;
#endif

    class InspectorResource : public RefCounted<InspectorResource> {
    public:

        // Keep these in sync with WebInspector.Resource.Type
        enum Type {
            Doc,
            Stylesheet,
            Image,
            Font,
            Script,
            XHR,
            Media,
            WebSocket,
            Other
        };

        static PassRefPtr<InspectorResource> create(unsigned long identifier, DocumentLoader* loader, const KURL& requestURL);

        static PassRefPtr<InspectorResource> createCached(unsigned long identifier, DocumentLoader*, const CachedResource*);

#if ENABLE(WEB_SOCKETS)
        // WebSocket resource doesn't have its loader. For WebSocket resources, m_loader and m_frame will become null.
        static PassRefPtr<InspectorResource> createWebSocket(unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
#endif

        ~InspectorResource();

        PassRefPtr<InspectorResource> appendRedirect(unsigned long identifier, const KURL& redirectURL);
        void updateScriptObject(InspectorFrontend* frontend);
        void releaseScriptObject(InspectorFrontend* frontend);

        void updateRequest(const ResourceRequest&);
        void markAsCached();
        void updateResponse(const ResourceResponse&);

#if ENABLE(WEB_SOCKETS)
        void updateWebSocketRequest(const WebSocketHandshakeRequest&);
        void updateWebSocketResponse(const WebSocketHandshakeResponse&);
#endif

        void setOverrideContent(const ScriptString& data, Type);

        String sourceString() const;
        PassRefPtr<SharedBuffer> resourceData(String* textEncodingName) const;

        bool isSameLoader(DocumentLoader* loader) const { return loader == m_loader; }
        void markMainResource() { m_isMainResource = true; }
        unsigned long identifier() const { return m_identifier; }
        KURL requestURL() const { return m_requestURL; }
        Frame* frame() const { return m_frame.get(); }
        const String& mimeType() const { return m_mimeType; }
        const HTTPHeaderMap& requestHeaderFields() const { return m_requestHeaderFields; }
        const HTTPHeaderMap& responseHeaderFields() const { return m_responseHeaderFields; }
        int responseStatusCode() const { return m_responseStatusCode; }
        String requestMethod() const { return m_requestMethod; }
        String requestFormData() const { return m_requestFormData; }

        void startTiming();
        void markLoadEventTime();
        void markDOMContentEventTime();
        void endTiming(double actualEndTime);

        void markFailed();
        void addLength(int lengthReceived);

    private:
        enum ChangeType {
            NoChange = 0,
            RequestChange = 1,
            ResponseChange = 2,
            TypeChange = 4,
            LengthChange = 8,
            CompletionChange = 16,
            TimingChange = 32,
            RedirectsChange = 64
        };

        class Changes {
        public:
            Changes() : m_change(NoChange) {}

            inline bool hasChange(ChangeType change)
            {
                return m_change & change || (m_change == NoChange && change == NoChange);
            }
            inline void set(ChangeType change)
            {
                m_change = static_cast<ChangeType>(static_cast<unsigned>(m_change) | static_cast<unsigned>(change));
            }
            inline void clear(ChangeType change)
            {
                m_change = static_cast<ChangeType>(static_cast<unsigned>(m_change) & ~static_cast<unsigned>(change));
            }

            inline void setAll() { m_change = static_cast<ChangeType>(127); }
            inline void clearAll() { m_change = NoChange; }

        private:
            ChangeType m_change;
        };

        InspectorResource(unsigned long identifier, DocumentLoader*, const KURL& requestURL);
        Type type() const;

        Type cachedResourceType() const;
        CachedResource* cachedResource() const;

#if ENABLE(WEB_SOCKETS)
        void markWebSocket() { m_isWebSocket = true; }
#endif

        unsigned long m_identifier;
        RefPtr<DocumentLoader> m_loader;
        RefPtr<Frame> m_frame;
        KURL m_requestURL;
        KURL m_documentURL;
        HTTPHeaderMap m_requestHeaderFields;
        HTTPHeaderMap m_responseHeaderFields;
        String m_mimeType;
        String m_suggestedFilename;
        long long m_expectedContentLength;
        bool m_cached;
        bool m_finished;
        bool m_failed;
        int m_length;
        int m_responseStatusCode;
        String m_responseStatusText;
        double m_startTime;
        double m_responseReceivedTime;
        double m_endTime;
        double m_loadEventTime;
        double m_domContentEventTime;
        unsigned m_connectionID;
        bool m_connectionReused;
        RefPtr<ResourceLoadTiming> m_loadTiming;
        ScriptString m_overrideContent;
        Type m_overrideContentType;
        Changes m_changes;
        bool m_isMainResource;
        String m_requestMethod;
        String m_requestFormData;
        Vector<RefPtr<InspectorResource> > m_redirects;

#if ENABLE(WEB_SOCKETS)
        bool m_isWebSocket;

        // The following fields are not used for resources other than WebSocket.
        // We allocate them dynamically to reduce memory consumption for regular resources.
        OwnPtr<WebSocketHandshakeRequest::Key3> m_webSocketRequestKey3;
        OwnPtr<WebSocketHandshakeResponse::ChallengeResponse> m_webSocketChallengeResponse;
#endif
    };

} // namespace WebCore

#endif // InspectorResource_h
