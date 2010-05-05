/*
 *  Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005-2007 Alexey Proskuryakov <ap@webkit.org>
 *  Copyright (C) 2007, 2008 Julien Chaffraix <jchaffraix@webkit.org>
 *  Copyright (C) 2008 David Levin <levin@chromium.org>
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

#include "Blob.h"
#include "Cache.h"
#include "CrossOriginAccessControl.h"
#include "DOMFormData.h"
#include "DOMImplementation.h"
#include "Document.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "HTTPParsers.h"
#include "InspectorController.h"
#include "InspectorTimelineAgent.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "TextResourceDecoder.h"
#include "ThreadableLoader.h"
#include "XMLHttpRequestException.h"
#include "XMLHttpRequestProgressEvent.h"
#include "XMLHttpRequestUpload.h"
#include "markup.h"
#include <wtf/text/CString.h>
#include <wtf/StdLibExtras.h>
#include <wtf/RefCountedLeakCounter.h>

#if USE(JSC)
#include "JSDOMBinding.h"
#include "JSDOMWindow.h"
#include <runtime/Protect.h>
#endif

namespace WebCore {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter xmlHttpRequestCounter("XMLHttpRequest");
#endif

struct XMLHttpRequestStaticData : Noncopyable {
    XMLHttpRequestStaticData();
    String m_proxyHeaderPrefix;
    String m_secHeaderPrefix;
    HashSet<String, CaseFoldingHash> m_forbiddenRequestHeaders;
};

XMLHttpRequestStaticData::XMLHttpRequestStaticData()
    : m_proxyHeaderPrefix("proxy-")
    , m_secHeaderPrefix("sec-")
{
    m_forbiddenRequestHeaders.add("accept-charset");
    m_forbiddenRequestHeaders.add("accept-encoding");
    m_forbiddenRequestHeaders.add("access-control-request-headers");
    m_forbiddenRequestHeaders.add("access-control-request-method");
    m_forbiddenRequestHeaders.add("connection");
    m_forbiddenRequestHeaders.add("content-length");
    m_forbiddenRequestHeaders.add("content-transfer-encoding");
    m_forbiddenRequestHeaders.add("cookie");
    m_forbiddenRequestHeaders.add("cookie2");
    m_forbiddenRequestHeaders.add("date");
    m_forbiddenRequestHeaders.add("expect");
    m_forbiddenRequestHeaders.add("host");
    m_forbiddenRequestHeaders.add("keep-alive");
    m_forbiddenRequestHeaders.add("origin");
    m_forbiddenRequestHeaders.add("referer");
    m_forbiddenRequestHeaders.add("te");
    m_forbiddenRequestHeaders.add("trailer");
    m_forbiddenRequestHeaders.add("transfer-encoding");
    m_forbiddenRequestHeaders.add("upgrade");
    m_forbiddenRequestHeaders.add("user-agent");
    m_forbiddenRequestHeaders.add("via");
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

static bool isSetCookieHeader(const AtomicString& name)
{
    return equalIgnoringCase(name, "set-cookie") || equalIgnoringCase(name, "set-cookie2");
}

static void setCharsetInMediaType(String& mediaType, const String& charsetValue)
{
    unsigned int pos = 0, len = 0;

    findCharsetInMediaType(mediaType, pos, len);

    if (!len) {
        // When no charset found, append new charset.
        mediaType.stripWhiteSpace();
        if (mediaType[mediaType.length() - 1] != ';')
            mediaType.append(";");
        mediaType.append(" charset=");
        mediaType.append(charsetValue);
    } else {
        // Found at least one existing charset, replace all occurrences with new charset.
        while (len) {
            mediaType.replace(pos, len, charsetValue);
            unsigned int start = pos + charsetValue.length();
            findCharsetInMediaType(mediaType, pos, len, start);
        }
    }
}

static const XMLHttpRequestStaticData* staticData = 0;

static const XMLHttpRequestStaticData* createXMLHttpRequestStaticData()
{
    staticData = new XMLHttpRequestStaticData;
    return staticData;
}

static const XMLHttpRequestStaticData* initializeXMLHttpRequestStaticData()
{
    // Uses dummy to avoid warnings about an unused variable.
    AtomicallyInitializedStatic(const XMLHttpRequestStaticData*, dummy = createXMLHttpRequestStaticData());
    return dummy;
}

XMLHttpRequest::XMLHttpRequest(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
    , m_async(true)
    , m_includeCredentials(false)
    , m_state(UNSENT)
    , m_responseText("")
    , m_createdDocument(false)
    , m_error(false)
    , m_uploadEventsAllowed(true)
    , m_uploadComplete(false)
    , m_sameOriginRequest(true)
    , m_didTellLoaderAboutRequest(false)
    , m_receivedLength(0)
    , m_lastSendLineNumber(0)
    , m_exceptionCode(0)
    , m_progressEventThrottle(this)
{
    initializeXMLHttpRequestStaticData();
#ifndef NDEBUG
    xmlHttpRequestCounter.increment();
#endif
}

XMLHttpRequest::~XMLHttpRequest()
{
    if (m_didTellLoaderAboutRequest) {
        cache()->loader()->nonCacheRequestComplete(m_url);
        m_didTellLoaderAboutRequest = false;
    }
    if (m_upload)
        m_upload->disconnectXMLHttpRequest();

#ifndef NDEBUG
    xmlHttpRequestCounter.decrement();
#endif
}

Document* XMLHttpRequest::document() const
{
    ASSERT(scriptExecutionContext()->isDocument());
    return static_cast<Document*>(scriptExecutionContext());
}

#if ENABLE(DASHBOARD_SUPPORT)
bool XMLHttpRequest::usesDashboardBackwardCompatibilityMode() const
{
    if (scriptExecutionContext()->isWorkerContext())
        return false;
    Settings* settings = document()->settings();
    return settings && settings->usesDashboardBackwardCompatibilityMode();
}
#endif

XMLHttpRequest::State XMLHttpRequest::readyState() const
{
    return m_state;
}

const ScriptString& XMLHttpRequest::responseText() const
{
    return m_responseText;
}

Document* XMLHttpRequest::responseXML() const
{
    if (m_state != DONE)
        return 0;

    if (!m_createdDocument) {
        if ((m_response.isHTTP() && !responseIsXML()) || scriptExecutionContext()->isWorkerContext()) {
            // The W3C spec requires this.
            m_responseXML = 0;
        } else {
            m_responseXML = Document::create(0);
            m_responseXML->open();
            m_responseXML->setURL(m_url);
            // FIXME: Set Last-Modified.
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

XMLHttpRequestUpload* XMLHttpRequest::upload()
{
    if (!m_upload)
        m_upload = XMLHttpRequestUpload::create(this);
    return m_upload.get();
}

void XMLHttpRequest::changeState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        callReadyStateChangeListener();
    }
}

void XMLHttpRequest::callReadyStateChangeListener()
{
    if (!scriptExecutionContext())
        return;

#if ENABLE(INSPECTOR)
    InspectorTimelineAgent* timelineAgent = InspectorTimelineAgent::retrieve(scriptExecutionContext());
    bool callTimelineAgentOnReadyStateChange = timelineAgent && hasEventListeners(eventNames().readystatechangeEvent);
    if (callTimelineAgentOnReadyStateChange)
        timelineAgent->willChangeXHRReadyState(m_url.string(), m_state);
#endif

    m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().readystatechangeEvent), m_state == DONE ? FlushProgressEvent : DoNotFlushProgressEvent);

#if ENABLE(INSPECTOR)
    if (callTimelineAgentOnReadyStateChange && (timelineAgent = InspectorTimelineAgent::retrieve(scriptExecutionContext())))
        timelineAgent->didChangeXHRReadyState();
#endif

    if (m_state == DONE && !m_error) {
#if ENABLE(INSPECTOR)
        timelineAgent = InspectorTimelineAgent::retrieve(scriptExecutionContext());
        bool callTimelineAgentOnLoad = timelineAgent && hasEventListeners(eventNames().loadEvent);
        if (callTimelineAgentOnLoad)
            timelineAgent->willLoadXHR(m_url.string());
#endif

        m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().loadEvent));

#if ENABLE(INSPECTOR)
        if (callTimelineAgentOnLoad && (timelineAgent = InspectorTimelineAgent::retrieve(scriptExecutionContext())))
            timelineAgent->didLoadXHR();
#endif
    }
}

void XMLHttpRequest::setWithCredentials(bool value, ExceptionCode& ec)
{
    if (m_state != OPENED || m_loader) {
        ec = INVALID_STATE_ERR;
        return;
    }

    m_includeCredentials = value;
}

void XMLHttpRequest::open(const String& method, const KURL& url, ExceptionCode& ec)
{
    open(method, url, true, ec);
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, ExceptionCode& ec)
{
    internalAbort();
    State previousState = m_state;
    m_state = UNSENT;
    m_error = false;

    m_uploadComplete = false;

    // clear stuff from possible previous load
    clearResponse();
    clearRequest();

    ASSERT(m_state == UNSENT);

    if (!isValidToken(method)) {
        ec = SYNTAX_ERR;
        return;
    }

    // Method names are case sensitive. But since Firefox uppercases method names it knows, we'll do the same.
    String methodUpper(method.upper());

    if (methodUpper == "TRACE" || methodUpper == "TRACK" || methodUpper == "CONNECT") {
        ec = SECURITY_ERR;
        return;
    }

    m_url = url;

    if (methodUpper == "COPY" || methodUpper == "DELETE" || methodUpper == "GET" || methodUpper == "HEAD"
        || methodUpper == "INDEX" || methodUpper == "LOCK" || methodUpper == "M-POST" || methodUpper == "MKCOL" || methodUpper == "MOVE"
        || methodUpper == "OPTIONS" || methodUpper == "POST" || methodUpper == "PROPFIND" || methodUpper == "PROPPATCH" || methodUpper == "PUT"
        || methodUpper == "UNLOCK")
        m_method = methodUpper;
    else
        m_method = method;

    m_async = async;

    ASSERT(!m_loader);

    // Check previous state to avoid dispatching readyState event
    // when calling open several times in a row.
    if (previousState != OPENED)
        changeState(OPENED);
    else
        m_state = OPENED;
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, ExceptionCode& ec)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user);

    open(method, urlWithCredentials, async, ec);
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, const String& password, ExceptionCode& ec)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user);
    urlWithCredentials.setPass(password);

    open(method, urlWithCredentials, async, ec);
}

bool XMLHttpRequest::initSend(ExceptionCode& ec)
{
    if (!scriptExecutionContext())
        return false;

    if (m_state != OPENED || m_loader) {
        ec = INVALID_STATE_ERR;
        return false;
    }

    m_error = false;
    return true;
}

void XMLHttpRequest::send(ExceptionCode& ec)
{
    send(String(), ec);
}

void XMLHttpRequest::send(Document* document, ExceptionCode& ec)
{
    ASSERT(document);

    if (!initSend(ec))
        return;

    if (m_method != "GET" && m_method != "HEAD" && m_url.protocolInHTTPFamily()) {
        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
#if ENABLE(DASHBOARD_SUPPORT)
            if (usesDashboardBackwardCompatibilityMode())
                setRequestHeaderInternal("Content-Type", "application/x-www-form-urlencoded");
            else
#endif
                // FIXME: this should include the charset used for encoding.
                setRequestHeaderInternal("Content-Type", "application/xml");
        }

        // FIXME: According to XMLHttpRequest Level 2, this should use the Document.innerHTML algorithm
        // from the HTML5 specification to serialize the document.
        String body = createMarkup(document);

        // FIXME: this should use value of document.inputEncoding to determine the encoding to use.
        TextEncoding encoding = UTF8Encoding();
        m_requestEntityBody = FormData::create(encoding.encode(body.characters(), body.length(), EntitiesForUnencodables));
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    createRequest(ec);
}

void XMLHttpRequest::send(const String& body, ExceptionCode& ec)
{
    if (!initSend(ec))
        return;

    if (!body.isNull() && m_method != "GET" && m_method != "HEAD" && m_url.protocolInHTTPFamily()) {
        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
#if ENABLE(DASHBOARD_SUPPORT)
            if (usesDashboardBackwardCompatibilityMode())
                setRequestHeaderInternal("Content-Type", "application/x-www-form-urlencoded");
            else
#endif
                setRequestHeaderInternal("Content-Type", "application/xml");
        } else {
            setCharsetInMediaType(contentType, "UTF-8");
            m_requestHeaders.set("Content-Type", contentType);
        }

        m_requestEntityBody = FormData::create(UTF8Encoding().encode(body.characters(), body.length(), EntitiesForUnencodables));
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    createRequest(ec);
}

void XMLHttpRequest::send(Blob* body, ExceptionCode& ec)
{
    if (!initSend(ec))
        return;

    if (m_method != "GET" && m_method != "HEAD" && m_url.protocolInHTTPFamily()) {
        // FIXME: Should we set a Content-Type if one is not set.
        // FIXME: add support for uploading bundles.
        m_requestEntityBody = FormData::create();
#if ENABLE(BLOB_SLICE)
        m_requestEntityBody->appendFileRange(body->path(), body->start(), body->length(), body->modificationTime());
#else
        m_requestEntityBody->appendFile(body->path(), false);
#endif
    }

    createRequest(ec);
}

void XMLHttpRequest::send(DOMFormData* body, ExceptionCode& ec)
{
    if (!initSend(ec))
        return;

    if (m_method != "GET" && m_method != "HEAD" && m_url.protocolInHTTPFamily()) {
        m_requestEntityBody = FormData::createMultiPart(*body, document());

        // We need to ask the client to provide the generated file names if needed. When FormData fills the element
        // for the file, it could set a flag to use the generated file name, i.e. a package file on Mac.
        m_requestEntityBody->generateFiles(document());

        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
            contentType = "multipart/form-data; boundary=";
            contentType += m_requestEntityBody->boundary().data();
            setRequestHeaderInternal("Content-Type", contentType);
        }
    }

    createRequest(ec);
}

void XMLHttpRequest::createRequest(ExceptionCode& ec)
{
    // The presence of upload event listeners forces us to use preflighting because POSTing to an URL that does not
    // permit cross origin requests should look exactly like POSTing to an URL that does not respond at all.
    // Also, only async requests support upload progress events.
    bool forcePreflight = false;
    if (m_async) {
        m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().loadstartEvent));
        if (m_requestEntityBody && m_upload) {
            forcePreflight = m_upload->hasEventListeners();
            m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().loadstartEvent));
        }
    }

    m_sameOriginRequest = scriptExecutionContext()->securityOrigin()->canRequest(m_url);

    // We also remember whether upload events should be allowed for this request in case the upload listeners are
    // added after the request is started.
    m_uploadEventsAllowed = m_sameOriginRequest || !isSimpleCrossOriginAccessRequest(m_method, m_requestHeaders);

    ResourceRequest request(m_url);
    request.setHTTPMethod(m_method);

    if (m_requestEntityBody) {
        ASSERT(m_method != "GET");
        ASSERT(m_method != "HEAD");
        request.setHTTPBody(m_requestEntityBody.release());
    }

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = true;
    options.sniffContent = false;
    options.forcePreflight = forcePreflight;
    options.allowCredentials = m_sameOriginRequest || m_includeCredentials;
    options.crossOriginRequestPolicy = UseAccessControl;

    m_exceptionCode = 0;
    m_error = false;

    if (m_async) {
        if (m_upload)
            request.setReportUploadProgress(true);

        // ThreadableLoader::create can return null here, for example if we're no longer attached to a page.
        // This is true while running onunload handlers.
        // FIXME: Maybe we need to be able to send XMLHttpRequests from onunload, <http://bugs.webkit.org/show_bug.cgi?id=10904>.
        // FIXME: Maybe create() can return null for other reasons too?
        m_loader = ThreadableLoader::create(scriptExecutionContext(), this, request, options);
        if (m_loader) {
            // Neither this object nor the JavaScript wrapper should be deleted while
            // a request is in progress because we need to keep the listeners alive,
            // and they are referenced by the JavaScript wrapper.
            setPendingActivity(this);

            // For now we should only balance the nonCached request count for main-thread XHRs and not
            // Worker XHRs, as the Cache is not thread-safe.
            // This will become irrelevant after https://bugs.webkit.org/show_bug.cgi?id=27165 is resolved.
            if (!scriptExecutionContext()->isWorkerContext()) {
                ASSERT(isMainThread());
                ASSERT(!m_didTellLoaderAboutRequest);
                cache()->loader()->nonCacheRequestInFlight(m_url);
                m_didTellLoaderAboutRequest = true;
            }
        }
    } else
        ThreadableLoader::loadResourceSynchronously(scriptExecutionContext(), request, *this, options);

    if (!m_exceptionCode && m_error)
        m_exceptionCode = XMLHttpRequestException::NETWORK_ERR;
    ec = m_exceptionCode;
}

void XMLHttpRequest::abort()
{
    // internalAbort() calls dropProtection(), which may release the last reference.
    RefPtr<XMLHttpRequest> protect(this);

    bool sendFlag = m_loader;

    internalAbort();

    m_responseText = "";
    m_createdDocument = false;
    m_responseXML = 0;

    // Clear headers as required by the spec
    m_requestHeaders.clear();

    if ((m_state <= OPENED && !sendFlag) || m_state == DONE)
        m_state = UNSENT;
    else {
        ASSERT(!m_loader);
        changeState(DONE);
        m_state = UNSENT;
    }

    m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().abortEvent));
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadEventsAllowed)
            m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().abortEvent));
    }
}

void XMLHttpRequest::internalAbort()
{
    bool hadLoader = m_loader;

    m_error = true;

    // FIXME: when we add the support for multi-part XHR, we will have to think be careful with this initialization.
    m_receivedLength = 0;

    if (hadLoader) {
        m_loader->cancel();
        m_loader = 0;
    }

    m_decoder = 0;

    if (hadLoader)
        dropProtection();
}

void XMLHttpRequest::clearResponse()
{
    m_response = ResourceResponse();
    m_responseText = "";
    m_createdDocument = false;
    m_responseXML = 0;
}

void XMLHttpRequest::clearRequest()
{
    m_requestHeaders.clear();
    m_requestEntityBody = 0;
}

void XMLHttpRequest::genericError()
{
    clearResponse();
    clearRequest();
    m_error = true;

    changeState(DONE);
}

void XMLHttpRequest::networkError()
{
    genericError();
    m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().errorEvent));
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadEventsAllowed)
            m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().errorEvent));
    }
    internalAbort();
}

void XMLHttpRequest::abortError()
{
    genericError();
    m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().abortEvent));
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadEventsAllowed)
            m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().abortEvent));
    }
}

void XMLHttpRequest::dropProtection()
{
#if USE(JSC)
    // The XHR object itself holds on to the responseText, and
    // thus has extra cost even independent of any
    // responseText or responseXML objects it has handed
    // out. But it is protected from GC while loading, so this
    // can't be recouped until the load is done, so only
    // report the extra cost at that point.
    JSC::JSGlobalData* globalData = scriptExecutionContext()->globalData();
    if (hasCachedDOMObjectWrapper(globalData, this))
        globalData->heap.reportExtraMemoryCost(m_responseText.size() * 2);
#endif

    unsetPendingActivity(this);
}

void XMLHttpRequest::overrideMimeType(const String& override)
{
    m_mimeTypeOverride = override;
}

static void reportUnsafeUsage(ScriptExecutionContext* context, const String& message)
{
    if (!context)
        return;
    // FIXME: It's not good to report the bad usage without indicating what source line it came from.
    // We should pass additional parameters so we can tell the console where the mistake occurred.
    context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, message, 1, String());
}

void XMLHttpRequest::setRequestHeader(const AtomicString& name, const String& value, ExceptionCode& ec)
{
    if (m_state != OPENED || m_loader) {
#if ENABLE(DASHBOARD_SUPPORT)
        if (usesDashboardBackwardCompatibilityMode())
            return;
#endif

        ec = INVALID_STATE_ERR;
        return;
    }

    if (!isValidToken(name) || !isValidHeaderValue(value)) {
        ec = SYNTAX_ERR;
        return;
    }

    // A privileged script (e.g. a Dashboard widget) can set any headers.
    if (!scriptExecutionContext()->securityOrigin()->canLoadLocalResources() && !isSafeRequestHeader(name)) {
        reportUnsafeUsage(scriptExecutionContext(), "Refused to set unsafe header \"" + name + "\"");
        return;
    }

    setRequestHeaderInternal(name, value);
}

void XMLHttpRequest::setRequestHeaderInternal(const AtomicString& name, const String& value)
{
    pair<HTTPHeaderMap::iterator, bool> result = m_requestHeaders.add(name, value);
    if (!result.second)
        result.first->second += ", " + value;
}

bool XMLHttpRequest::isSafeRequestHeader(const String& name) const
{
    return !staticData->m_forbiddenRequestHeaders.contains(name) && !name.startsWith(staticData->m_proxyHeaderPrefix, false)
        && !name.startsWith(staticData->m_secHeaderPrefix, false);
}

String XMLHttpRequest::getRequestHeader(const AtomicString& name) const
{
    return m_requestHeaders.get(name);
}

String XMLHttpRequest::getAllResponseHeaders(ExceptionCode& ec) const
{
    if (m_state < HEADERS_RECEIVED) {
        ec = INVALID_STATE_ERR;
        return "";
    }

    Vector<UChar> stringBuilder;

    HTTPHeaderMap::const_iterator end = m_response.httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = m_response.httpHeaderFields().begin(); it!= end; ++it) {
        // Hide Set-Cookie header fields from the XMLHttpRequest client for these reasons:
        //     1) If the client did have access to the fields, then it could read HTTP-only
        //        cookies; those cookies are supposed to be hidden from scripts.
        //     2) There's no known harm in hiding Set-Cookie header fields entirely; we don't
        //        know any widely used technique that requires access to them.
        //     3) Firefox has implemented this policy.
        if (isSetCookieHeader(it->first) && !scriptExecutionContext()->securityOrigin()->canLoadLocalResources())
            continue;

        if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(it->first))
            continue;

        stringBuilder.append(it->first.characters(), it->first.length());
        stringBuilder.append(':');
        stringBuilder.append(' ');
        stringBuilder.append(it->second.characters(), it->second.length());
        stringBuilder.append('\r');
        stringBuilder.append('\n');
    }

    return String::adopt(stringBuilder);
}

String XMLHttpRequest::getResponseHeader(const AtomicString& name, ExceptionCode& ec) const
{
    if (m_state < HEADERS_RECEIVED) {
        ec = INVALID_STATE_ERR;
        return String();
    }

    // See comment in getAllResponseHeaders above.
    if (isSetCookieHeader(name) && !scriptExecutionContext()->securityOrigin()->canLoadLocalResources()) {
        reportUnsafeUsage(scriptExecutionContext(), "Refused to get unsafe header \"" + name + "\"");
        return String();
    }

    if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(name)) {
        reportUnsafeUsage(scriptExecutionContext(), "Refused to get unsafe header \"" + name + "\"");
        return String();
    }
    return m_response.httpHeaderField(name);
}

String XMLHttpRequest::responseMIMEType() const
{
    String mimeType = extractMIMETypeFromMediaType(m_mimeTypeOverride);
    if (mimeType.isEmpty()) {
        if (m_response.isHTTP())
            mimeType = extractMIMETypeFromMediaType(m_response.httpHeaderField("Content-Type"));
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

int XMLHttpRequest::status(ExceptionCode& ec) const
{
    if (m_response.httpStatusCode())
        return m_response.httpStatusCode();

    if (m_state == OPENED) {
        // Firefox only raises an exception in this state; we match it.
        // Note the case of local file requests, where we have no HTTP response code! Firefox never raises an exception for those, but we match HTTP case for consistency.
        ec = INVALID_STATE_ERR;
    }

    return 0;
}

String XMLHttpRequest::statusText(ExceptionCode& ec) const
{
    if (!m_response.httpStatusText().isNull())
        return m_response.httpStatusText();

    if (m_state == OPENED) {
        // See comments in status() above.
        ec = INVALID_STATE_ERR;
    }

    return String();
}

void XMLHttpRequest::didFail(const ResourceError& error)
{
    if (m_didTellLoaderAboutRequest) {
        cache()->loader()->nonCacheRequestComplete(m_url);
        m_didTellLoaderAboutRequest = false;
    }

    // If we are already in an error state, for instance we called abort(), bail out early.
    if (m_error)
        return;

    if (error.isCancellation()) {
        m_exceptionCode = XMLHttpRequestException::ABORT_ERR;
        abortError();
        return;
    }

    m_exceptionCode = XMLHttpRequestException::NETWORK_ERR;
    networkError();
}

void XMLHttpRequest::didFailRedirectCheck()
{
    networkError();
}

void XMLHttpRequest::didFinishLoading(unsigned long identifier)
{
    if (m_didTellLoaderAboutRequest) {
        cache()->loader()->nonCacheRequestComplete(m_url);
        m_didTellLoaderAboutRequest = false;
    }

    if (m_error)
        return;

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    if (m_decoder)
        m_responseText += m_decoder->flush();

#if ENABLE(INSPECTOR)
    if (InspectorController* inspector = scriptExecutionContext()->inspectorController())
        inspector->resourceRetrievedByXMLHttpRequest(identifier, m_responseText);
#endif

    bool hadLoader = m_loader;
    m_loader = 0;

    changeState(DONE);
    m_decoder = 0;

    if (hadLoader)
        dropProtection();
}

void XMLHttpRequest::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (!m_upload)
        return;

    if (m_uploadEventsAllowed)
        m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().progressEvent, true, static_cast<unsigned>(bytesSent), static_cast<unsigned>(totalBytesToBeSent)));

    if (bytesSent == totalBytesToBeSent && !m_uploadComplete) {
        m_uploadComplete = true;
        if (m_uploadEventsAllowed)
            m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().loadEvent));
    }
}

void XMLHttpRequest::didReceiveResponse(const ResourceResponse& response)
{
    m_response = response;
    m_responseEncoding = extractCharsetFromMediaType(m_mimeTypeOverride);
    if (m_responseEncoding.isEmpty())
        m_responseEncoding = response.textEncodingName();
}

void XMLHttpRequest::didReceiveAuthenticationCancellation(const ResourceResponse& failureResponse)
{
    m_response = failureResponse;
}

void XMLHttpRequest::didReceiveData(const char* data, int len)
{
    if (m_error)
        return;

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    if (!m_decoder) {
        if (!m_responseEncoding.isEmpty())
            m_decoder = TextResourceDecoder::create("text/plain", m_responseEncoding);
        // allow TextResourceDecoder to look inside the m_response if it's XML or HTML
        else if (responseIsXML()) {
            m_decoder = TextResourceDecoder::create("application/xml");
            // Don't stop on encoding errors, unlike it is done for other kinds of XML resources. This matches the behavior of previous WebKit versions, Firefox and Opera.
            m_decoder->useLenientXMLDecoding();
        } else if (responseMIMEType() == "text/html")
            m_decoder = TextResourceDecoder::create("text/html", "UTF-8");
        else
            m_decoder = TextResourceDecoder::create("text/plain", "UTF-8");
    }

    if (!len)
        return;

    if (len == -1)
        len = strlen(data);

    m_responseText += m_decoder->decode(data, len);

    if (!m_error) {
        long long expectedLength = m_response.expectedContentLength();
        m_receivedLength += len;

        bool lengthComputable = expectedLength && m_receivedLength <= expectedLength;
        m_progressEventThrottle.dispatchProgressEvent(lengthComputable, static_cast<unsigned>(m_receivedLength), static_cast<unsigned>(expectedLength));

        if (m_state != LOADING)
            changeState(LOADING);
        else
            // Firefox calls readyStateChanged every time it receives data, 4449442
            callReadyStateChangeListener();
    }
}

bool XMLHttpRequest::canSuspend() const
{
    return !m_loader;
}

void XMLHttpRequest::suspend()
{
    m_progressEventThrottle.suspend();
}

void XMLHttpRequest::resume()
{
    m_progressEventThrottle.resume();
}

void XMLHttpRequest::stop()
{
    internalAbort();
}

void XMLHttpRequest::contextDestroyed()
{
    ASSERT(!m_loader);
    ActiveDOMObject::contextDestroyed();
}

ScriptExecutionContext* XMLHttpRequest::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

EventTargetData* XMLHttpRequest::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* XMLHttpRequest::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore
