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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef XMLHTTPREQUEST_H_
#define XMLHTTPREQUEST_H_

#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include "KURL.h"
#include "ResourceResponse.h"
#include "PlatformString.h"
#include "HTTPHeaderMap.h"
#include "StringHash.h"
#include "SubresourceLoaderClient.h"

namespace WebCore {

class TextResourceDecoder;
class Document;
class EventListener;
class String;

typedef int ExceptionCode;

const int XMLHttpRequestExceptionOffset = 500;
const int XMLHttpRequestExceptionMax = 599;
enum XMLHttpRequestExceptionCode { PERMISSION_DENIED = XMLHttpRequestExceptionOffset };

// these exact numeric values are important because JS expects them
enum XMLHttpRequestState {
    Uninitialized = 0,  // The initial value.
    Open = 1,           // The open() method has been successfully called.
    Sent = 2,           // The user agent successfully completed the request, but no data has yet been received.
    Receiving = 3,      // Immediately before receiving the message body (if any). All HTTP headers have been received.
    Loaded = 4          // The data transfer has been completed.
};

class XMLHttpRequest : public Shared<XMLHttpRequest>, private SubresourceLoaderClient {
public:
    XMLHttpRequest(Document*);
    ~XMLHttpRequest();

    static void detachRequests(Document*);
    static void cancelRequests(Document*);

    String getStatusText(ExceptionCode&) const;
    int getStatus(ExceptionCode&) const;
    XMLHttpRequestState getReadyState() const;
    void open(const String& method, const KURL& url, bool async, const String& user, const String& password, ExceptionCode& ec);
    void send(const String& body, ExceptionCode&);
    void abort();
    void setRequestHeader(const String& name, const String& value, ExceptionCode&);
    void overrideMIMEType(const String& override);
    String getAllResponseHeaders() const;
    String getResponseHeader(const String& name) const;
    String getResponseText() const;
    Document* getResponseXML() const;

    void setOnReadyStateChangeListener(EventListener*);
    EventListener* onReadyStateChangeListener() const;
    void setOnLoadListener(EventListener*);
    EventListener* onLoadListener() const;

private:
    bool urlMatchesDocumentDomain(const KURL&) const;

    virtual void willSendRequest(SubresourceLoader*, ResourceRequest& request, const ResourceResponse& redirectResponse);
    virtual void didReceiveResponse(SubresourceLoader*, const ResourceResponse&);
    virtual void didReceiveData(SubresourceLoader*, const char* data, int size);
    virtual void didFail(SubresourceLoader*, const ResourceError&);
    virtual void didFinishLoading(SubresourceLoader*);

    void processSyncLoadResults(const Vector<char>& data, const ResourceResponse&);

    bool responseIsXML() const;

    String getRequestHeader(const String& name) const;

    void changeState(XMLHttpRequestState newState);
    void callReadyStateChangeListener();

    Document* m_doc;
    RefPtr<EventListener> m_onReadyStateChangeListener;
    RefPtr<EventListener> m_onLoadListener;

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
    String m_responseText;
    mutable bool m_createdDocument;
    mutable RefPtr<Document> m_responseXML;

    bool m_aborted;
};

} // namespace

#endif
