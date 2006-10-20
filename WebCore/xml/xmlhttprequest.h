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
#include "ResourceLoaderClient.h"
#include "PlatformString.h"
#include "StringHash.h"

namespace WebCore {

class Decoder;
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

class XMLHttpRequest : public Shared<XMLHttpRequest>, ResourceLoaderClient {
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

    virtual void receivedRedirect(ResourceLoader*, const KURL&);
    virtual void receivedData(ResourceLoader*, const char* data, int size);
    virtual void receivedAllData(ResourceLoader*);

    void processSyncLoadResults(const Vector<char>& data, const KURL& finalURL, const DeprecatedString& headers);

    bool responseIsXML() const;

    String getRequestHeader(const String& name) const;
    static DeprecatedString getSpecificHeader(const DeprecatedString& headers, const DeprecatedString& name);

    void changeState(XMLHttpRequestState newState);
    void callReadyStateChangeListener();

    Document* m_doc;
    RefPtr<EventListener> m_onReadyStateChangeListener;
    RefPtr<EventListener> m_onLoadListener;

    KURL m_url;
    DeprecatedString m_method;
    bool m_async;
    HashMap<String, String> m_requestHeaders;

    RefPtr<ResourceLoader> m_loader;

    XMLHttpRequestState m_state;

    RefPtr<Decoder> m_decoder;
    String m_encoding;
    String m_responseHeaders;
    String m_mimeTypeOverride;

    String m_response;
    mutable bool m_createdDocument;
    mutable RefPtr<Document> m_responseXML;

    bool m_aborted;
};

} // namespace

#endif
