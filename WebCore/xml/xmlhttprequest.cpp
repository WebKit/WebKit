/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#include "config.h"
#include "xmlhttprequest.h"

#include "CString.h"
#include "Cache.h"
#include "DOMImplementation.h"
#include "TextResourceDecoder.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FormData.h"
#include "HTMLDocument.h"
#include "LoaderFunctions.h"
#include "PlatformString.h"
#include "RegularExpression.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "TextEncoding.h"
#include "kjs_binding.h"
#include <kjs/protect.h>
#include <wtf/Vector.h>

namespace WebCore {

using namespace EventNames;

typedef HashSet<XMLHttpRequest*> RequestsSet;

static HashMap<Document*, RequestsSet*>& requestsByDocument()
{
    static HashMap<Document*, RequestsSet*> map;
    return map;
}

static void addToRequestsByDocument(Document* doc, XMLHttpRequest* req)
{
    ASSERT(doc);
    ASSERT(req);

    RequestsSet* requests = requestsByDocument().get(doc);
    if (!requests) {
        requests = new RequestsSet;
        requestsByDocument().set(doc, requests);
    }

    ASSERT(!requests->contains(req));
    requests->add(req);
}

static void removeFromRequestsByDocument(Document* doc, XMLHttpRequest* req)
{
    ASSERT(doc);
    ASSERT(req);

    RequestsSet* requests = requestsByDocument().get(doc);
    ASSERT(requests);
    ASSERT(requests->contains(req));
    requests->remove(req);
    if (requests->isEmpty()) {
        requestsByDocument().remove(doc);
        delete requests;
    }
}

static inline String getMIMEType(const String& contentTypeString)
{
    String mimeType;
    unsigned length = contentTypeString.length();
    for (unsigned offset = 0; offset < length; offset++) {
        UChar c = contentTypeString[offset];
        if (c == ';')
            break;
        else if (DeprecatedChar(c).isSpace()) // FIXME: This seems wrong, " " is an invalid MIME type character according to RFC 2045.  bug 8644
            continue;
        // FIXME: This is a very slow way to build a string, given WebCore::String's implementation.
        mimeType += String(&c, 1);
    }
    return mimeType;
}

static String getCharset(const String& contentTypeString)
{
    int pos = 0;
    int length = (int)contentTypeString.length();
    
    while (pos < length) {
        pos = contentTypeString.find("charset", pos, false);
        if (pos <= 0)
            return String();
        
        // is what we found a beginning of a word?
        if (contentTypeString[pos-1] > ' ' && contentTypeString[pos-1] != ';') {
            pos += 7;
            continue;
        }
        
        pos += 7;

        // skip whitespace
        while (pos != length && contentTypeString[pos] <= ' ')
            ++pos;
    
        if (contentTypeString[pos++] != '=') // this "charset" substring wasn't a parameter name, but there may be others
            continue;

        while (pos != length && (contentTypeString[pos] <= ' ' || contentTypeString[pos] == '"' || contentTypeString[pos] == '\''))
            ++pos;

        // we don't handle spaces within quoted parameter values, because charset names cannot have any
        int endpos = pos;
        while (pos != length && contentTypeString[endpos] > ' ' && contentTypeString[endpos] != '"' && contentTypeString[endpos] != '\'' && contentTypeString[endpos] != ';')
            ++endpos;
    
        return contentTypeString.substring(pos, endpos-pos);
    }
    
    return String();
}

XMLHttpRequestState XMLHttpRequest::getReadyState() const
{
    return m_state;
}

String XMLHttpRequest::getResponseText() const
{
    return m_responseText;
}

Document* XMLHttpRequest::getResponseXML() const
{
    if (m_state != Loaded)
        return 0;

    if (!m_createdDocument) {
        if (responseIsXML()) {
            m_responseXML = m_doc->implementation()->createDocument(0);
            m_responseXML->open();
            m_responseXML->write(m_responseText);
            m_responseXML->finishParsing();
            m_responseXML->close();
            
            if (!m_responseXML->wellFormed())
                m_responseXML = 0;
        }
        m_createdDocument = true;
    }

    return m_responseXML.get();
}

EventListener* XMLHttpRequest::onReadyStateChangeListener() const
{
    return m_onReadyStateChangeListener.get();
}

void XMLHttpRequest::setOnReadyStateChangeListener(EventListener* eventListener)
{
    m_onReadyStateChangeListener = eventListener;
}

EventListener* XMLHttpRequest::onLoadListener() const
{
    return m_onLoadListener.get();
}

void XMLHttpRequest::setOnLoadListener(EventListener* eventListener)
{
    m_onLoadListener = eventListener;
}

XMLHttpRequest::XMLHttpRequest(Document* d)
    : m_doc(d)
    , m_async(true)
    , m_loader(0)
    , m_state(Uninitialized)
    , m_responseText("", 0)
    , m_createdDocument(false)
    , m_aborted(false)
{
    ASSERT(m_doc);
    addToRequestsByDocument(m_doc, this);
}

XMLHttpRequest::~XMLHttpRequest()
{
    if (m_doc)
        removeFromRequestsByDocument(m_doc, this);
}

void XMLHttpRequest::changeState(XMLHttpRequestState newState)
{
    if (m_state != newState) {
        m_state = newState;
        callReadyStateChangeListener();
    }
}

void XMLHttpRequest::callReadyStateChangeListener()
{
    if (m_doc && m_doc->frame() && m_onReadyStateChangeListener)
        m_onReadyStateChangeListener->handleEvent(new Event(readystatechangeEvent, true, true), true);
    
    if (m_doc && m_doc->frame() && m_state == Loaded && m_onLoadListener)
        m_onLoadListener->handleEvent(new Event(loadEvent, true, true), true);
}

bool XMLHttpRequest::urlMatchesDocumentDomain(const KURL& url) const
{
    KURL documentURL(m_doc->URL());

    // a local file can load anything
    if (documentURL.protocol().lower() == "file" || documentURL.protocol().lower() == "applewebdata")
        return true;

    // but a remote document can only load from the same port on the server
    if (documentURL.protocol().lower() == url.protocol().lower()
            && documentURL.host().lower() == url.host().lower()
            && documentURL.port() == url.port())
        return true;

    return false;
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, const String& password, ExceptionCode& ec)
{
    abort();
    m_aborted = false;

    // clear stuff from possible previous load
    m_requestHeaders.clear();
    m_response = ResourceResponse();
    m_responseText = "";
    m_createdDocument = false;
    m_responseXML = 0;

    changeState(Uninitialized);

    if (!urlMatchesDocumentDomain(url)) {
        ec = PERMISSION_DENIED;
        return;
    }

    m_url = url;

    if (!user.isNull())
        m_url.setUser(user.deprecatedString());

    if (!password.isNull())
        m_url.setPass(password.deprecatedString());

    // Method names are case sensitive. But since Firefox uppercases method names it knows, we'll do the same.
    String methodUpper(method.upper());
    if (methodUpper == "CONNECT" || methodUpper == "COPY" || methodUpper == "DELETE" || methodUpper == "GET" || methodUpper == "HEAD"
        || methodUpper == "INDEX" || methodUpper == "LOCK" || methodUpper == "M-POST" || methodUpper == "MKCOL" || methodUpper == "MOVE" 
        || methodUpper == "OPTIONS" || methodUpper == "POST" || methodUpper == "PROPFIND" || methodUpper == "PROPPATCH" || methodUpper == "PUT" 
        || methodUpper == "TRACE" || methodUpper == "UNLOCK")
        m_method = methodUpper.deprecatedString();
    else
        m_method = method.deprecatedString();

    m_async = async;

    changeState(Open);
}

void XMLHttpRequest::send(const String& body, ExceptionCode& ec)
{
    if (!m_doc)
        return;

    if (m_state != Open) {
        ec = INVALID_STATE_ERR;
        return;
    }
  
    // FIXME: Should this abort or raise an exception instead if we already have a m_loader going?
    if (m_loader)
        return;

    m_aborted = false;

    ResourceRequest request(m_url);
    request.setHTTPMethod(m_method);
    
    if (!body.isNull() && m_method != "GET" && m_method != "HEAD" && (m_url.protocol().lower() == "http" || m_url.protocol().lower() == "https")) {
        String contentType = getRequestHeader("Content-Type");
        String charset;
        if (contentType.isEmpty()) {
            ExceptionCode ec = 0;
            setRequestHeader("Content-Type", "application/xml", ec);
            ASSERT(ec == 0);
        } else
            charset = getCharset(contentType);
      
        if (charset.isEmpty())
            charset = "UTF-8";
      
        TextEncoding m_encoding(charset);
        if (!m_encoding.isValid()) // FIXME: report an error?
            m_encoding = UTF8Encoding();

        request.setHTTPBody(PassRefPtr<FormData>(new FormData(m_encoding.encode(body.characters(), body.length()))));
    }

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    if (!m_async) {
        Vector<char> data;
        ResourceResponse response;

        {
            // avoid deadlock in case the loader wants to use JS on a background thread
            KJS::JSLock::DropAllLocks dropLocks;
            data = ServeSynchronousRequest(cache()->loader(), m_doc->docLoader(), request, response);
        }

        m_loader = 0;
        processSyncLoadResults(data, response);
    
        return;
    }

    // Neither this object nor the JavaScript wrapper should be deleted while
    // a request is in progress because we need to keep the listeners alive,
    // and they are referenced by the JavaScript wrapper.
    ref();
    {
        KJS::JSLock lock;
        gcProtectNullTolerant(KJS::ScriptInterpreter::getDOMObject(this));
    }
  
    // create can return null here, for example if we're no longer attached to a page.
    // this is true while running onunload handlers
    // FIXME: Maybe create can return false for other reasons too?
    m_loader = ResourceHandle::create(request, this, m_doc->docLoader());
}

void XMLHttpRequest::abort()
{
    bool hadLoader = m_loader;

    if (hadLoader) {
        m_loader->kill();
        m_loader = 0;
    }
    m_decoder = 0;
    m_aborted = true;

    if (hadLoader) {
        {
            KJS::JSLock lock;
            gcUnprotectNullTolerant(KJS::ScriptInterpreter::getDOMObject(this));
        }
        deref();
    }
}

void XMLHttpRequest::overrideMIMEType(const String& override)
{
    m_mimeTypeOverride = override;
}

void XMLHttpRequest::setRequestHeader(const String& name, const String& value, ExceptionCode& ec)
{
    if (m_state != Open)
        // rdar 4758577: XHR spec says an exception should be thrown here.  However, doing so breaks the Business and People widgets.
        return;

    if (!m_requestHeaders.contains(name)) {
        m_requestHeaders.set(name, value);
        return;
    }
    
    String oldValue = m_requestHeaders.get(name);
    m_requestHeaders.set(name, oldValue + ", " + value);
}

String XMLHttpRequest::getRequestHeader(const String& name) const
{
    return m_requestHeaders.get(name);
}

String XMLHttpRequest::getAllResponseHeaders() const
{
    Vector<UChar> stringBuilder;
    String separator(": ");

    HTTPHeaderMap::const_iterator end = m_response.httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = m_response.httpHeaderFields().begin(); it!= end; ++it) {
        stringBuilder.append(it->first.characters(), it->first.length());
        stringBuilder.append(separator.characters(), separator.length());
        stringBuilder.append(it->second.characters(), it->second.length());
        stringBuilder.append((UChar)'\n');
    }

    return String::adopt(stringBuilder);
}

String XMLHttpRequest::getResponseHeader(const String& name) const
{
    return m_response.httpHeaderField(name);
}

bool XMLHttpRequest::responseIsXML() const
{
    String mimeType = getMIMEType(m_mimeTypeOverride);
    if (mimeType.isEmpty())
        mimeType = getMIMEType(getResponseHeader("Content-Type"));
    if (mimeType.isEmpty())
        mimeType = "text/xml";
    return DOMImplementation::isXMLMIMEType(mimeType);
}

int XMLHttpRequest::getStatus(ExceptionCode& ec) const
{
    if (m_state == Uninitialized)
        return 0;
    
    if (m_response.httpStatusCode() == 0) {
        if (m_state != Receiving && m_state != Loaded)
            // status MUST be available in these states, but we don't get any headers from non-HTTP requests
            ec = INVALID_STATE_ERR;
    }

    return m_response.httpStatusCode();
}

String XMLHttpRequest::getStatusText(ExceptionCode& ec) const
{
    if (m_state == Uninitialized)
        return "";
    
    if (m_response.httpStatusCode() == 0) {
        if (m_state != Receiving && m_state != Loaded)
            // statusText MUST be available in these states, but we don't get any headers from non-HTTP requests
            ec = INVALID_STATE_ERR;
        return String();
    }

    // FIXME: should try to preserve status text in response
    return "OK";
}

void XMLHttpRequest::processSyncLoadResults(const Vector<char>& data, const ResourceResponse& response)
{
    if (!urlMatchesDocumentDomain(response.url())) {
        abort();
        return;
    }

    didReceiveResponse(0, response);
    changeState(Sent);
    if (m_aborted)
        return;

    const char* bytes = static_cast<const char*>(data.data());
    int len = static_cast<int>(data.size());

    didReceiveData(0, bytes, len);
    if (m_aborted)
        return;

    didFinishLoading(0);
}

void XMLHttpRequest::didFailWithError(ResourceHandle* handle, const ResourceError&)
{
    didFinishLoading(handle);
}

void XMLHttpRequest::didFinishLoading(ResourceHandle* handle)
{
    if (m_aborted)
        return;
        
    ASSERT(handle == m_loader);

    if (m_state < Sent)
        changeState(Sent);

    if (m_decoder)
        m_responseText += m_decoder->flush();

    bool hadLoader = m_loader;
    m_loader = 0;

    changeState(Loaded);
    m_decoder = 0;

    if (hadLoader) {
        {
            KJS::JSLock lock;
            gcUnprotectNullTolerant(KJS::ScriptInterpreter::getDOMObject(this));
        }
        deref();
    }
}

void XMLHttpRequest::willSendRequest(ResourceHandle*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!urlMatchesDocumentDomain(request.url()))
        abort();
}

void XMLHttpRequest::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    m_response = response;
    m_encoding = getCharset(m_mimeTypeOverride);
    if (m_encoding.isEmpty())
        m_encoding = response.textEncodingName();

}

void XMLHttpRequest::didReceiveData(ResourceHandle*, const char* data, int len)
{
    if (m_state < Sent)
        changeState(Sent);
  
    if (!m_decoder) {
        if (!m_encoding.isEmpty())
            m_decoder = new TextResourceDecoder("text/plain", m_encoding);
        else if (responseIsXML())
            // allow TextResourceDecoder to look inside the m_response if it's XML
            m_decoder = new TextResourceDecoder("application/xml");
        else
            m_decoder = new TextResourceDecoder("text/plain", "UTF-8");
    }
    if (len == 0)
        return;

    if (len == -1)
        len = strlen(data);

    String decoded = m_decoder->decode(data, len);

    m_responseText += decoded;

    if (!m_aborted) {
        if (m_state != Receiving)
            changeState(Receiving);
        else
            // Firefox calls readyStateChanged every time it receives data, 4449442
            callReadyStateChangeListener();
    }
}

void XMLHttpRequest::cancelRequests(Document* m_doc)
{
    RequestsSet* requests = requestsByDocument().get(m_doc);
    if (!requests)
        return;
    RequestsSet copy = *requests;
    RequestsSet::const_iterator end = copy.end();
    for (RequestsSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->abort();
}

void XMLHttpRequest::detachRequests(Document* m_doc)
{
    RequestsSet* requests = requestsByDocument().get(m_doc);
    if (!requests)
        return;
    requestsByDocument().remove(m_doc);
    RequestsSet::const_iterator end = requests->end();
    for (RequestsSet::const_iterator it = requests->begin(); it != end; ++it) {
        (*it)->m_doc = 0;
        (*it)->abort();
    }
    delete requests;
}

} // end namespace
