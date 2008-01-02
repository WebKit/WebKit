// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#include "EventTarget.h"
#include "HTTPHeaderMap.h"
#include "KURL.h"
#include "PlatformString.h"
#include "ResourceResponse.h"
#include "StringHash.h"
#include "SubresourceLoaderClient.h"
#include <kjs/ustring.h>

#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class TextResourceDecoder;
class Document;
class Event;
class EventListener;
class String;

typedef int ExceptionCode;

// these exact numeric values are important because JS expects them
enum XMLHttpRequestState {
    Uninitialized = 0,  // The initial value.
    Open = 1,           // The open() method has been successfully called.
    Sent = 2,           // The user agent successfully completed the request, but no data has yet been received.
    Receiving = 3,      // Immediately before receiving the message body (if any). All HTTP headers have been received.
    Loaded = 4          // The data transfer has been completed.
};

class XMLHttpRequest : public RefCounted<XMLHttpRequest>, public EventTarget, private SubresourceLoaderClient {
public:
    XMLHttpRequest(Document*);
    ~XMLHttpRequest();

    virtual XMLHttpRequest* toXMLHttpRequest() { return this; }

    static void detachRequests(Document*);
    static void cancelRequests(Document*);

    String getStatusText(ExceptionCode&) const;
    int getStatus(ExceptionCode&) const;
    XMLHttpRequestState getReadyState() const;
    void open(const String& method, const KURL&, bool async, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, const String& user, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, const String& user, const String& password, ExceptionCode&);
    void send(const String& body, ExceptionCode&);
    void abort();
    void setRequestHeader(const String& name, const String& value, ExceptionCode&);
    void overrideMIMEType(const String& override);
    String getAllResponseHeaders(ExceptionCode&) const;
    String getResponseHeader(const String& name, ExceptionCode&) const;
    const KJS::UString& getResponseText(ExceptionCode&) const;
    Document* getResponseXML(ExceptionCode&) const;

    void setOnReadyStateChangeListener(EventListener*);
    EventListener* onReadyStateChangeListener() const;
    void setOnLoadListener(EventListener*);
    EventListener* onLoadListener() const;

    typedef Vector<RefPtr<EventListener> > ListenerVector;
    typedef HashMap<AtomicStringImpl*, ListenerVector> EventListenersMap;

    // useCapture is not used, even for add/remove pairing (for Firefox compatibility).
    virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);
    EventListenersMap& eventListeners() { return m_eventListeners; }

    Document* document() const { return m_doc; }

    using RefCounted<XMLHttpRequest>::ref;
    using RefCounted<XMLHttpRequest>::deref;

private:
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    bool urlMatchesDocumentDomain(const KURL&) const;

    virtual void willSendRequest(SubresourceLoader*, ResourceRequest& request, const ResourceResponse& redirectResponse);
    virtual void didReceiveResponse(SubresourceLoader*, const ResourceResponse&);
    virtual void didReceiveData(SubresourceLoader*, const char* data, int size);
    virtual void didFail(SubresourceLoader*, const ResourceError&);
    virtual void didFinishLoading(SubresourceLoader*);
    virtual void receivedCancellation(SubresourceLoader*, const AuthenticationChallenge&);

    void processSyncLoadResults(const Vector<char>& data, const ResourceResponse&);

    String responseMIMEType() const;
    bool responseIsXML() const;

    String getRequestHeader(const String& name) const;

    void changeState(XMLHttpRequestState newState);
    void callReadyStateChangeListener();
    void dropProtection();

    Document* m_doc;

    RefPtr<EventListener> m_onReadyStateChangeListener;
    RefPtr<EventListener> m_onLoadListener;
    EventListenersMap m_eventListeners;

    KURL m_url;
    DeprecatedString m_method;
    HTTPHeaderMap m_requestHeaders;
    String m_mimeTypeOverride;
    bool m_async;

    RefPtr<SubresourceLoader> m_loader;
    XMLHttpRequestState m_state;

    ResourceResponse m_response;
    String m_encoding;

    RefPtr<TextResourceDecoder> m_decoder;

    // Unlike most strings in the DOM, we keep this as a KJS::UString, not a WebCore::String.
    // That's because these strings can easily get huge (they are filled from the network with
    // no parsing) and because JS can easily observe many intermediate states, so it's very useful
    // to be able to share the buffer with JavaScript versions of the whole or partial string.
    // In contrast, this string doesn't interact much with the rest of the engine so it's not that
    // big a cost that it isn't a String.
    KJS::UString m_responseText;
    mutable bool m_createdDocument;
    mutable RefPtr<Document> m_responseXML;

    bool m_aborted;
};

} // namespace WebCore

#endif // XMLHttpRequest_h
