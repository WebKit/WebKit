/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004, 2006 Apple Computer, Inc.
 *  Copyright (C) 2005-2007 Alexey Proskuryakov <ap@webkit.org>
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

#include "config.h"
#include "XMLHttpRequest.h"

#include "CString.h"
#include "Cache.h"
#include "DOMImplementation.h"
#include "TextResourceDecoder.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FormData.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTTPParsers.h"
#include "Page.h"
#include "PlatformString.h"
#include "RegularExpression.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "Settings.h"
#include "SubresourceLoader.h"
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

static bool canSetRequestHeader(const String& name)
{
    static HashSet<String, CaseInsensitiveHash<String> > forbiddenHeaders;
    
    if (forbiddenHeaders.isEmpty()) {
        forbiddenHeaders.add("accept-charset");
        forbiddenHeaders.add("accept-encoding");
        forbiddenHeaders.add("content-length");
        forbiddenHeaders.add("expect");
        forbiddenHeaders.add("date");
        forbiddenHeaders.add("host");
        forbiddenHeaders.add("keep-alive");
        forbiddenHeaders.add("referer");
        forbiddenHeaders.add("te");
        forbiddenHeaders.add("trailer");
        forbiddenHeaders.add("transfer-encoding");
        forbiddenHeaders.add("upgrade");
        forbiddenHeaders.add("via");
    }
    
    return !forbiddenHeaders.contains(name);
}

// Determines if a string is a valid token, as defined by
// "token" in section 2.2 of RFC 2616.
static bool isValidToken(const String& name)
{
    unsigned length = name.length();
    for (unsigned i = 0; i < length; i++) {
        UChar c = name[i];
        
        if (c >= 127 || c <= 32)
            return false;
        
        if (c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
            c == ',' || c == ';' || c == ':' || c == '\\' || c == '\"' ||
            c == '/' || c == '[' || c == ']' || c == '?' || c == '=' ||
            c == '{' || c == '}')
            return false;
    }
    
    return true;
}
    
static bool isValidHeaderValue(const String& name)
{
    // FIXME: This should really match name against 
    // field-value in section 4.2 of RFC 2616.
        
    return !name.contains('\r') && !name.contains('\n');
}
    
XMLHttpRequestState XMLHttpRequest::getReadyState() const
{
    return m_state;
}

const KJS::UString& XMLHttpRequest::getResponseText() const
{
    return m_responseText;
}

Document* XMLHttpRequest::getResponseXML() const
{
    if (m_state != Loaded)
        return 0;

    if (!m_createdDocument) {
        if (m_response.isHTTP() && !responseIsXML()) {
            // The W3C spec requires this.
            m_responseXML = 0;
        } else {
            m_responseXML = m_doc->implementation()->createDocument(0);
            m_responseXML->open();
            m_responseXML->setURL(m_url.url());
            // FIXME: set Last-Modified and cookies (currently, those are only available for HTMLDocuments).
            m_responseXML->write(String(m_responseText));
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

void XMLHttpRequest::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType.impl());
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType.impl(), listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter)
            if (*listenerIter == eventListener)
                return;
        
        listeners.append(eventListener);
        m_eventListeners.add(eventType.impl(), listeners);
    }
}

void XMLHttpRequest::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType.impl());
    if (iter == m_eventListeners.end())
        return;

    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter)
        if (*listenerIter == eventListener) {
            listeners.remove(listenerIter - listeners.begin());
            return;
        }
}

bool XMLHttpRequest::dispatchEvent(PassRefPtr<Event> evt, ExceptionCode& ec, bool /*tempEvent*/)
{
    // FIXME: check for other error conditions enumerated in the spec.
    if (evt->type().isEmpty()) {
        ec = UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }

    ListenerVector listenersCopy = m_eventListeners.get(evt->type().impl());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        listenerIter->get()->handleEvent(evt.get(), false);
    }

    return !evt->defaultPrevented();
}

XMLHttpRequest::XMLHttpRequest(Document* d)
    : m_doc(d)
    , m_async(true)
    , m_loader(0)
    , m_state(Uninitialized)
    , m_responseText("")
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
    if (m_doc && m_doc->frame() && m_onReadyStateChangeListener) {
        RefPtr<Event> evt = new Event(readystatechangeEvent, true, true);
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onReadyStateChangeListener->handleEvent(evt.get(), false);
    }
    
    if (m_doc && m_doc->frame() && m_state == Loaded) {
        if (m_onLoadListener) {
            RefPtr<Event> evt = new Event(loadEvent, true, true);
            evt->setTarget(this);
            evt->setCurrentTarget(this);
            m_onLoadListener->handleEvent(evt.get(), false);
        }
        
        ListenerVector listenersCopy = m_eventListeners.get(loadEvent.impl());
        for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
            RefPtr<Event> evt = new Event(loadEvent, true, true);
            evt->setTarget(this);
            evt->setCurrentTarget(this);
            listenerIter->get()->handleEvent(evt.get(), false);
        }
    }
}

bool XMLHttpRequest::urlMatchesDocumentDomain(const KURL& url) const
{
    // a local file can load anything
    if (m_doc->isAllowedToLoadLocalResources())
        return true;

    // but a remote document can only load from the same port on the server
    KURL documentURL = m_doc->URL();
    if (documentURL.protocol().lower() == url.protocol().lower()
            && documentURL.host().lower() == url.host().lower()
            && documentURL.port() == url.port())
        return true;

    return false;
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, ExceptionCode& ec)
{
    abort();
    m_aborted = false;

    // clear stuff from possible previous load
    m_requestHeaders.clear();
    m_response = ResourceResponse();
    {
        KJS::JSLock lock;
        m_responseText = "";
    }
    m_createdDocument = false;
    m_responseXML = 0;

    changeState(Uninitialized);

    if (!urlMatchesDocumentDomain(url)) {
        ec = PERMISSION_DENIED;
        return;
    }

    if (!isValidToken(method)) {
        ec = SYNTAX_ERR;
        return;
    }
    
    m_url = url;

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

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, ExceptionCode& ec)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user.deprecatedString());
    
    open(method, urlWithCredentials, async, ec);
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, const String& password, ExceptionCode& ec)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user.deprecatedString());
    urlWithCredentials.setPass(password.deprecatedString());
    
    open(method, urlWithCredentials, async, ec);
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
        if (contentType.isEmpty()) {
            ExceptionCode ec = 0;
            Settings* settings = m_doc->settings();
            if (settings && settings->usesDashboardBackwardCompatibilityMode())
                setRequestHeader("Content-Type", "application/x-www-form-urlencoded", ec);
            else
                setRequestHeader("Content-Type", "application/xml", ec);
            ASSERT(ec == 0);
        }

        // FIXME: must use xmlEncoding for documents.
        String charset = "UTF-8";
      
        TextEncoding m_encoding(charset);
        if (!m_encoding.isValid()) // FIXME: report an error?
            m_encoding = UTF8Encoding();

        request.setHTTPBody(PassRefPtr<FormData>(new FormData(m_encoding.encode(body.characters(), body.length()))));
    }

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    if (!m_async) {
        Vector<char> data;
        ResourceError error;
        ResourceResponse response;

        {
            // avoid deadlock in case the loader wants to use JS on a background thread
            KJS::JSLock::DropAllLocks dropLocks;
            if (m_doc->frame()) 
                m_doc->frame()->loader()->loadResourceSynchronously(request, error, response, data);
        }

        m_loader = 0;
        
        // No exception for file:/// resources, see <rdar://problem/4962298>.
        // Also, if we have an HTTP response, then it wasn't a network error in fact.
        if (error.isNull() || request.url().isLocalFile() || response.httpStatusCode() > 0)
            processSyncLoadResults(data, response);
        else
            ec = NETWORK_ERR;

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
    m_loader = SubresourceLoader::create(m_doc->frame(), this, request, false, true, false);
}

void XMLHttpRequest::abort()
{
    bool hadLoader = m_loader;

    m_aborted = true;
    
    if (hadLoader) {
        m_loader->cancel();
        m_loader = 0;
    }

    m_decoder = 0;

    if (hadLoader)
        dropProtection();
}

void XMLHttpRequest::dropProtection()        
{
    {
        KJS::JSLock lock;
        KJS::JSValue* wrapper = KJS::ScriptInterpreter::getDOMObject(this);
        KJS::gcUnprotectNullTolerant(wrapper);
    
        // the XHR object itself holds on to the responseText, and
        // thus has extra cost even independent of any
        // responseText or responseXML objects it has handed
        // out. But it is protected from GC while loading, so this
        // can't be recouped until the load is done, so only
        // report the extra cost at that point.
    
        if (wrapper)
            KJS::Collector::reportExtraMemoryCost(m_responseText.size() * 2);
    }

    deref();
}

void XMLHttpRequest::overrideMIMEType(const String& override)
{
    m_mimeTypeOverride = override;
}
    
void XMLHttpRequest::setRequestHeader(const String& name, const String& value, ExceptionCode& ec)
{
    if (m_state != Open) {
        Settings* settings = m_doc ? m_doc->settings() : 0;
        if (settings && settings->usesDashboardBackwardCompatibilityMode())
            return;

        ec = INVALID_STATE_ERR;
        return;
    }

    if (!isValidToken(name) || !isValidHeaderValue(value)) {
        ec = SYNTAX_ERR;
        return;
    }
        
    if (!canSetRequestHeader(name)) {
        if (m_doc && m_doc->frame() && m_doc->frame()->page())
            m_doc->frame()->page()->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, "Refused to set unsafe header " + name, 1, String());
        return;
    }

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

String XMLHttpRequest::responseMIMEType() const
{
    String mimeType = extractMIMETypeFromMediaType(m_mimeTypeOverride);
    if (mimeType.isEmpty()) {
        if (m_response.isHTTP())
            mimeType = extractMIMETypeFromMediaType(getResponseHeader("Content-Type"));
        else
            mimeType = m_response.mimeType();
    }
    if (mimeType.isEmpty())
        mimeType = "text/xml";
    
    return mimeType;
}

bool XMLHttpRequest::responseIsXML() const
{
    return DOMImplementation::isXMLMIMEType(responseMIMEType());
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

void XMLHttpRequest::didFail(SubresourceLoader* loader, const ResourceError&)
{
    didFinishLoading(loader);
}

void XMLHttpRequest::didFinishLoading(SubresourceLoader* loader)
{
    if (m_aborted)
        return;
        
    ASSERT(loader == m_loader);

    if (m_state < Sent)
        changeState(Sent);

    {
        KJS::JSLock lock;
        if (m_decoder)
            m_responseText += m_decoder->flush();
    }

    bool hadLoader = m_loader;
    m_loader = 0;

    changeState(Loaded);
    m_decoder = 0;

    if (hadLoader)
        dropProtection();
}

void XMLHttpRequest::willSendRequest(SubresourceLoader*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!urlMatchesDocumentDomain(request.url()))
        abort();
}

void XMLHttpRequest::didReceiveResponse(SubresourceLoader*, const ResourceResponse& response)
{
    m_response = response;
    m_encoding = extractCharsetFromMediaType(m_mimeTypeOverride);
    if (m_encoding.isEmpty())
        m_encoding = response.textEncodingName();

}

void XMLHttpRequest::receivedCancellation(SubresourceLoader*, const AuthenticationChallenge& challenge)
{
    m_response = challenge.failureResponse();
}

void XMLHttpRequest::didReceiveData(SubresourceLoader*, const char* data, int len)
{
    if (m_state < Sent)
        changeState(Sent);
  
    if (!m_decoder) {
        if (!m_encoding.isEmpty())
            m_decoder = new TextResourceDecoder("text/plain", m_encoding);
        // allow TextResourceDecoder to look inside the m_response if it's XML or HTML
        else if (responseIsXML())
            m_decoder = new TextResourceDecoder("application/xml");
        else if (responseMIMEType() == "text/html")
            m_decoder = new TextResourceDecoder("text/html");
        else
            m_decoder = new TextResourceDecoder("text/plain", "UTF-8");
    }
    if (len == 0)
        return;

    if (len == -1)
        len = strlen(data);

    String decoded = m_decoder->decode(data, len);

    {
        KJS::JSLock lock;
        m_responseText += decoded;
    }

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
