/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef XMLHttpRequest_h
#define XMLHttpRequest_h

#include "AccessControlList.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "FormData.h"
#include "ResourceResponse.h"
#include "SubresourceLoaderClient.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class Document;
class File;
class TextResourceDecoder;

// these exact numeric values are important because JS expects them
enum XMLHttpRequestState {
    UNSENT = 0,             // The object has been constructed.
    OPENED = 1,             // The open() method has been successfully invoked. During this state request headers can be set using setRequestHeader() and the request can be made using the send() method.
    HEADERS_RECEIVED = 2,   // All HTTP headers have been received. Several response members of the object are now available.
    LOADING = 3,            // The response entity body is being received.
    DONE = 4                // The data transfer has been completed or something went wrong during the transfer (such as infinite redirects)..
};

class XMLHttpRequest : public RefCounted<XMLHttpRequest>, public EventTarget, private SubresourceLoaderClient {
public:
    static PassRefPtr<XMLHttpRequest> create(Document *document) { return adoptRef(new XMLHttpRequest(document)); }
    ~XMLHttpRequest();

    virtual XMLHttpRequest* toXMLHttpRequest() { return this; }

    static void detachRequests(Document*);
    static void cancelRequests(Document*);

    String statusText(ExceptionCode&) const;
    int status(ExceptionCode&) const;
    XMLHttpRequestState readyState() const;
    void open(const String& method, const KURL&, bool async, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, const String& user, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, const String& user, const String& password, ExceptionCode&);
    void send(ExceptionCode&);
    void send(Document*, ExceptionCode&);
    void send(const String&, ExceptionCode&);
    void abort();
    void setRequestHeader(const String& name, const String& value, ExceptionCode&);
    void overrideMimeType(const String& override);
    String getAllResponseHeaders(ExceptionCode&) const;
    String getResponseHeader(const String& name, ExceptionCode&) const;
    const KJS::UString& responseText() const;
    Document* responseXML() const;

    void setOnReadyStateChangeListener(PassRefPtr<EventListener> eventListener) { m_onReadyStateChangeListener = eventListener; }
    EventListener* onReadyStateChangeListener() const { return m_onReadyStateChangeListener.get(); }

    void setOnLoadListener(PassRefPtr<EventListener> eventListener) { m_onLoadListener = eventListener; }
    EventListener* onLoadListener() const { return m_onLoadListener.get(); }

    void setOnProgressListener(PassRefPtr<EventListener> eventListener) { m_onProgressListener = eventListener; }
    EventListener* onProgressListener() const { return m_onProgressListener.get(); }

    typedef Vector<RefPtr<EventListener> > ListenerVector;
    typedef HashMap<AtomicStringImpl*, ListenerVector> EventListenersMap;

    // useCapture is not used, even for add/remove pairing (for Firefox compatibility).
    virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);
    EventListenersMap& eventListeners() { return m_eventListeners; }

    void dispatchProgressEvent(long long expectedLength);

    Document* document() const { return m_doc; }

    using RefCounted<XMLHttpRequest>::ref;
    using RefCounted<XMLHttpRequest>::deref;

private:
    XMLHttpRequest(Document*);
    
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    virtual void willSendRequest(SubresourceLoader*, ResourceRequest& request, const ResourceResponse& redirectResponse);
    virtual void didReceiveResponse(SubresourceLoader*, const ResourceResponse&);
    virtual void didReceiveData(SubresourceLoader*, const char* data, int size);
    virtual void didFail(SubresourceLoader*, const ResourceError&);
    virtual void didFinishLoading(SubresourceLoader*);
    virtual void receivedCancellation(SubresourceLoader*, const AuthenticationChallenge&);

    // Special versions for the method check preflight
    void didReceiveResponseMethodCheck(SubresourceLoader*, const ResourceResponse&);
    void didFinishLoadingMethodCheck(SubresourceLoader*);

    void processSyncLoadResults(const Vector<char>& data, const ResourceResponse&, ExceptionCode&);
    void updateAndDispatchOnProgress(unsigned int len);

    String responseMIMEType() const;
    bool responseIsXML() const;

    bool initSend(ExceptionCode&);

    String getRequestHeader(const String& name) const;
    void setRequestHeaderInternal(const String& name, const String& value);

    void changeState(XMLHttpRequestState newState);
    void callReadyStateChangeListener();
    void dropProtection();
    void internalAbort();
    void clearResponse();
    void clearRequest();

    void createRequest(ExceptionCode&);

    void sameOriginRequest(ResourceRequest&);
    void crossSiteAccessRequest(ResourceRequest&, ExceptionCode&);

    void loadRequestSynchronously(ResourceRequest&, ExceptionCode&);
    void loadRequestAsynchronously(ResourceRequest&);

    void handleAsynchronousMethodCheckResult();

    String accessControlOrigin() const;

    void genericError();
    void networkError();
    void abortError();

    Document* m_doc;

    RefPtr<EventListener> m_onReadyStateChangeListener;
    RefPtr<EventListener> m_onLoadListener;
    RefPtr<EventListener> m_onProgressListener;
    EventListenersMap m_eventListeners;

    KURL m_url;
    String m_method;
    HTTPHeaderMap m_requestHeaders;
    RefPtr<FormData> m_requestEntityBody;
    String m_mimeTypeOverride;
    bool m_async;

    RefPtr<SubresourceLoader> m_loader;
    XMLHttpRequestState m_state;

    ResourceResponse m_response;
    String m_responseEncoding;

    RefPtr<TextResourceDecoder> m_decoder;
    unsigned long m_identifier;

    // Unlike most strings in the DOM, we keep this as a KJS::UString, not a WebCore::String.
    // That's because these strings can easily get huge (they are filled from the network with
    // no parsing) and because JS can easily observe many intermediate states, so it's very useful
    // to be able to share the buffer with JavaScript versions of the whole or partial string.
    // In contrast, this string doesn't interact much with the rest of the engine so it's not that
    // big a cost that it isn't a String.
    KJS::UString m_responseText;
    mutable bool m_createdDocument;
    mutable RefPtr<Document> m_responseXML;

    bool m_error;

    bool m_sameOriginRequest;
    bool m_allowAccess;
    bool m_inMethodCheck;

    // FIXME: Add support for AccessControlList in a PI in an XML document in addition to the http header.
    OwnPtr<AccessControlList> m_httpAccessControlList;

    // Used for onprogress tracking
    long long m_receivedLength;
};

} // namespace WebCore

#endif // XMLHttpRequest_h
