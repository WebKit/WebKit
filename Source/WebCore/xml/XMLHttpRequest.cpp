/*
 *  Copyright (C) 2004-2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2005-2007 Alexey Proskuryakov <ap@webkit.org>
 *  Copyright (C) 2007, 2008 Julien Chaffraix <jchaffraix@webkit.org>
 *  Copyright (C) 2008, 2011 Google Inc. All rights reserved.
 *  Copyright (C) 2012 Intel Corporation
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
#include "CachedResourceRequestInitiators.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginAccessControl.h"
#include "DOMFormData.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventNames.h"
#include "File.h"
#include "HTMLDocument.h"
#include "HTMLIFrameElement.h"
#include "HTTPHeaderNames.h"
#include "HTTPHeaderValues.h"
#include "HTTPParsers.h"
#include "InspectorInstrumentation.h"
#include "JSDOMBinding.h"
#include "JSDOMWindow.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "ParsedContentType.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "RuntimeApplicationChecks.h"
#include "SecurityOriginPolicy.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "StringAdaptors.h"
#include "TextResourceDecoder.h"
#include "ThreadableLoader.h"
#include "XMLDocument.h"
#include "XMLHttpRequestProgressEvent.h"
#include "XMLHttpRequestUpload.h"
#include "markup.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(XMLHttpRequest);

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, xmlHttpRequestCounter, ("XMLHttpRequest"));

// Histogram enum to see when we can deprecate xhr.send(ArrayBuffer).
enum XMLHttpRequestSendArrayBufferOrView {
    XMLHttpRequestSendArrayBuffer,
    XMLHttpRequestSendArrayBufferView,
    XMLHttpRequestSendArrayBufferOrViewMax,
};

static void replaceCharsetInMediaTypeIfNeeded(String& mediaType)
{
    auto parsedContentType = ParsedContentType::create(mediaType);
    if (!parsedContentType || parsedContentType->charset().isEmpty() || equalIgnoringASCIICase(parsedContentType->charset(), "UTF-8"))
        return;

    parsedContentType->setCharset("UTF-8");
    mediaType = parsedContentType->serialize();
}

static void logConsoleError(ScriptExecutionContext* context, const String& message)
{
    if (!context)
        return;
    // FIXME: It's not good to report the bad usage without indicating what source line it came from.
    // We should pass additional parameters so we can tell the console where the mistake occurred.
    context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, message);
}

Ref<XMLHttpRequest> XMLHttpRequest::create(ScriptExecutionContext& context)
{
    auto xmlHttpRequest = adoptRef(*new XMLHttpRequest(context));
    xmlHttpRequest->suspendIfNeeded();
    return xmlHttpRequest;
}

XMLHttpRequest::XMLHttpRequest(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_async(true)
    , m_includeCredentials(false)
    , m_sendFlag(false)
    , m_createdDocument(false)
    , m_error(false)
    , m_uploadListenerFlag(false)
    , m_uploadComplete(false)
    , m_wasAbortedByClient(false)
    , m_responseCacheIsValid(false)
    , m_readyState(static_cast<unsigned>(UNSENT))
    , m_responseType(static_cast<unsigned>(ResponseType::EmptyString))
    , m_progressEventThrottle(*this)
    , m_timeoutTimer(*this, &XMLHttpRequest::didReachTimeout)
{
#ifndef NDEBUG
    xmlHttpRequestCounter.increment();
#endif
}

XMLHttpRequest::~XMLHttpRequest()
{
#ifndef NDEBUG
    xmlHttpRequestCounter.decrement();
#endif
}

Document* XMLHttpRequest::document() const
{
    ASSERT(scriptExecutionContext());
    return downcast<Document>(scriptExecutionContext());
}

SecurityOrigin* XMLHttpRequest::securityOrigin() const
{
    return scriptExecutionContext()->securityOrigin();
}

ExceptionOr<OwnedString> XMLHttpRequest::responseText()
{
    if (responseType() != ResponseType::EmptyString && responseType() != ResponseType::Text)
        return Exception { InvalidStateError };
    return OwnedString { responseTextIgnoringResponseType() };
}

void XMLHttpRequest::didCacheResponse()
{
    ASSERT(doneWithoutErrors());
    m_responseCacheIsValid = true;
    m_responseBuilder.clear();
}

ExceptionOr<Document*> XMLHttpRequest::responseXML()
{
    ASSERT(scriptExecutionContext()->isDocument());
    
    if (responseType() != ResponseType::EmptyString && responseType() != ResponseType::Document)
        return Exception { InvalidStateError };

    if (!doneWithoutErrors())
        return nullptr;

    if (!m_createdDocument) {
        auto& context = downcast<Document>(*scriptExecutionContext());

        String mimeType = responseMIMEType();
        bool isHTML = equalLettersIgnoringASCIICase(mimeType, "text/html");

        // The W3C spec requires the final MIME type to be some valid XML type, or text/html.
        // If it is text/html, then the responseType of "document" must have been supplied explicitly.
        if ((m_response.isInHTTPFamily() && !responseIsXML() && !isHTML)
            || (isHTML && responseType() == ResponseType::EmptyString)) {
            m_responseDocument = nullptr;
        } else {
            if (isHTML)
                m_responseDocument = HTMLDocument::create(nullptr, m_response.url());
            else
                m_responseDocument = XMLDocument::create(nullptr, m_response.url());
            m_responseDocument->overrideLastModified(m_response.lastModified());
            m_responseDocument->setContextDocument(context);
            m_responseDocument->setSecurityOriginPolicy(context.securityOriginPolicy());
            m_responseDocument->overrideMIMEType(mimeType);
            m_responseDocument->setContent(m_responseBuilder.toStringPreserveCapacity());

            if (!m_responseDocument->wellFormed())
                m_responseDocument = nullptr;
        }
        m_createdDocument = true;
    }

    return m_responseDocument.get();
}

Ref<Blob> XMLHttpRequest::createResponseBlob()
{
    ASSERT(responseType() == ResponseType::Blob);
    ASSERT(doneWithoutErrors());

    // FIXME: We just received the data from NetworkProcess, and are sending it back. This is inefficient.
    Vector<uint8_t> data;
    if (m_binaryResponseBuilder)
        data.append(m_binaryResponseBuilder->data(), m_binaryResponseBuilder->size());
    m_binaryResponseBuilder = nullptr;
    String normalizedContentType = Blob::normalizedContentType(responseMIMEType()); // responseMIMEType defaults to text/xml which may be incorrect.
    return Blob::create(scriptExecutionContext(), WTFMove(data), normalizedContentType);
}

RefPtr<ArrayBuffer> XMLHttpRequest::createResponseArrayBuffer()
{
    ASSERT(responseType() == ResponseType::Arraybuffer);
    ASSERT(doneWithoutErrors());

    auto result = m_binaryResponseBuilder ? m_binaryResponseBuilder->tryCreateArrayBuffer() : ArrayBuffer::create(nullptr, 0);
    m_binaryResponseBuilder = nullptr;
    return result;
}

ExceptionOr<void> XMLHttpRequest::setTimeout(unsigned timeout)
{
    if (scriptExecutionContext()->isDocument() && !m_async) {
        logConsoleError(scriptExecutionContext(), "XMLHttpRequest.timeout cannot be set for synchronous HTTP(S) requests made from the window context.");
        return Exception { InvalidAccessError };
    }
    m_timeoutMilliseconds = timeout;
    if (!m_timeoutTimer.isActive())
        return { };

    // If timeout is zero, we should use the default network timeout. But we disabled it so let's mimic it with a 60 seconds timeout value.
    Seconds interval = Seconds { m_timeoutMilliseconds ? m_timeoutMilliseconds / 1000. : 60. } - (MonotonicTime::now() - m_sendingTime);
    m_timeoutTimer.startOneShot(std::max(interval, 0_s));
    return { };
}

ExceptionOr<void> XMLHttpRequest::setResponseType(ResponseType type)
{
    if (!scriptExecutionContext()->isDocument() && type == ResponseType::Document)
        return { };

    if (readyState() >= LOADING)
        return Exception { InvalidStateError };

    // Newer functionality is not available to synchronous requests in window contexts, as a spec-mandated
    // attempt to discourage synchronous XHR use. responseType is one such piece of functionality.
    // We'll only disable this functionality for HTTP(S) requests since sync requests for local protocols
    // such as file: and data: still make sense to allow.
    if (!m_async && scriptExecutionContext()->isDocument() && m_url.protocolIsInHTTPFamily()) {
        logConsoleError(scriptExecutionContext(), "XMLHttpRequest.responseType cannot be changed for synchronous HTTP(S) requests made from the window context.");
        return Exception { InvalidAccessError };
    }

    m_responseType = static_cast<unsigned>(type);
    return { };
}

String XMLHttpRequest::responseURL() const
{
    URL responseURL(m_response.url());
    responseURL.removeFragmentIdentifier();

    return responseURL.string();
}

XMLHttpRequestUpload& XMLHttpRequest::upload()
{
    if (!m_upload)
        m_upload = makeUnique<XMLHttpRequestUpload>(*this);
    return *m_upload;
}

void XMLHttpRequest::changeState(State newState)
{
    if (readyState() != newState) {
        // Setting the readyState to DONE could get the wrapper collected before we get a chance to fire the JS
        // events in callReadyStateChangeListener() below so we extend the lifetime of the JS wrapper until the
        // of this scope.
        auto eventFiringActivity = makePendingActivity(*this);

        m_readyState = static_cast<State>(newState);
        if (readyState() == DONE) {
            // The XHR object itself holds on to the responseText, and
            // thus has extra cost even independent of any
            // responseText or responseXML objects it has handed
            // out. But it is protected from GC while loading, so this
            // can't be recouped until the load is done, so only
            // report the extra cost at that point.
            if (auto* context = scriptExecutionContext()) {
                JSC::VM& vm = context->vm();
                JSC::JSLockHolder lock(vm);
                vm.heap.reportExtraMemoryAllocated(memoryCost());
            }
        }
        callReadyStateChangeListener();
    }
}

void XMLHttpRequest::callReadyStateChangeListener()
{
    if (!scriptExecutionContext())
        return;

    // Check whether sending load and loadend events before sending readystatechange event, as it may change m_error/m_readyState values.
    bool shouldSendLoadEvent = (readyState() == DONE && !m_error);

    if (m_async || (readyState() <= OPENED || readyState() == DONE)) {
        m_progressEventThrottle.dispatchReadyStateChangeEvent(Event::create(eventNames().readystatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No),
            readyState() == DONE ? FlushProgressEvent : DoNotFlushProgressEvent);
    }

    if (shouldSendLoadEvent) {
        m_progressEventThrottle.dispatchProgressEvent(eventNames().loadEvent);
        m_progressEventThrottle.dispatchProgressEvent(eventNames().loadendEvent);
    }
}

ExceptionOr<void> XMLHttpRequest::setWithCredentials(bool value)
{
    if (readyState() > OPENED || m_sendFlag)
        return Exception { InvalidStateError };

    m_includeCredentials = value;
    return { };
}

ExceptionOr<void> XMLHttpRequest::open(const String& method, const String& url)
{
    // If the async argument is omitted, set async to true.
    return open(method, scriptExecutionContext()->completeURL(url), true);
}

ExceptionOr<void> XMLHttpRequest::open(const String& method, const URL& url, bool async)
{
    auto* context = scriptExecutionContext();
    bool contextIsDocument = is<Document>(*context);
    if (contextIsDocument && !downcast<Document>(*context).isFullyActive())
        return Exception { InvalidStateError, "Document is not fully active"_s };

    if (!isValidHTTPToken(method))
        return Exception { SyntaxError };

    if (isForbiddenMethod(method))
        return Exception { SecurityError };

    if (!url.isValid())
        return Exception { SyntaxError };

    if (!async && contextIsDocument) {
        // Newer functionality is not available to synchronous requests in window contexts, as a spec-mandated
        // attempt to discourage synchronous XHR use. responseType is one such piece of functionality.
        // We'll only disable this functionality for HTTP(S) requests since sync requests for local protocols
        // such as file: and data: still make sense to allow.
        if (url.protocolIsInHTTPFamily() && responseType() != ResponseType::EmptyString) {
            logConsoleError(context, "Synchronous HTTP(S) requests made from the window context cannot have XMLHttpRequest.responseType set.");
            return Exception { InvalidAccessError };
        }

        // Similarly, timeouts are disabled for synchronous requests as well.
        if (m_timeoutMilliseconds > 0) {
            logConsoleError(context, "Synchronous XMLHttpRequests must not have a timeout value set.");
            return Exception { InvalidAccessError };
        }
    }

    if (!internalAbort())
        return { };

    m_sendFlag = false;
    m_uploadListenerFlag = false;
    m_method = normalizeHTTPMethod(method);
    m_error = false;
    m_uploadComplete = false;
    m_wasAbortedByClient = false;

    // clear stuff from possible previous load
    clearResponse();
    clearRequest();

    m_url = url;
    context->contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(m_url, ContentSecurityPolicy::InsecureRequestType::Load);

    m_async = async;

    ASSERT(!m_loadingActivity);

    changeState(OPENED);

    return { };
}

ExceptionOr<void> XMLHttpRequest::open(const String& method, const String& url, bool async, const String& user, const String& password)
{
    URL urlWithCredentials = scriptExecutionContext()->completeURL(url);
    if (!user.isNull())
        urlWithCredentials.setUser(user);
    if (!password.isNull())
        urlWithCredentials.setPassword(password);

    return open(method, urlWithCredentials, async);
}

Optional<ExceptionOr<void>> XMLHttpRequest::prepareToSend()
{
    // A return value other than WTF::nullopt means we should not try to send, and we should return that value to the caller.
    // WTF::nullopt means we are ready to send and should continue with the send algorithm.

    if (!scriptExecutionContext())
        return ExceptionOr<void> { };

    auto& context = *scriptExecutionContext();

    if (is<Document>(context) && downcast<Document>(context).shouldIgnoreSyncXHRs()) {
        logConsoleError(scriptExecutionContext(), makeString("Ignoring XMLHttpRequest.send() call for '", m_url.string(), "' because the maximum number of synchronous failures was reached."));
        return ExceptionOr<void> { };
    }

    if (readyState() != OPENED || m_sendFlag)
        return ExceptionOr<void> { Exception { InvalidStateError } };
    ASSERT(!m_loadingActivity);

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    if (!context.shouldBypassMainWorldContentSecurityPolicy() && !context.contentSecurityPolicy()->allowConnectToSource(m_url)) {
        if (!m_async)
            return ExceptionOr<void> { Exception { NetworkError } };
        m_timeoutTimer.stop();
        queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this] {
            networkError();
        });
        return ExceptionOr<void> { };
    }

    m_error = false;
    return WTF::nullopt;
}

ExceptionOr<void> XMLHttpRequest::send(Optional<SendTypes>&& sendType)
{
    InspectorInstrumentation::willSendXMLHttpRequest(scriptExecutionContext(), url().string());
    m_userGestureToken = UserGestureIndicator::currentUserGesture();

    ExceptionOr<void> result;
    if (!sendType)
        result = send();
    else {
        result = WTF::switchOn(sendType.value(),
            [this] (const RefPtr<Document>& document) -> ExceptionOr<void> { return send(*document); },
            [this] (const RefPtr<Blob>& blob) -> ExceptionOr<void> { return send(*blob); },
            [this] (const RefPtr<JSC::ArrayBufferView>& arrayBufferView) -> ExceptionOr<void> { return send(*arrayBufferView); },
            [this] (const RefPtr<JSC::ArrayBuffer>& arrayBuffer) -> ExceptionOr<void> { return send(*arrayBuffer); },
            [this] (const RefPtr<DOMFormData>& formData) -> ExceptionOr<void> { return send(*formData); },
            [this] (const String& string) -> ExceptionOr<void> { return send(string); }
        );
    }

    return result;
}

ExceptionOr<void> XMLHttpRequest::send(Document& document)
{
    if (auto result = prepareToSend())
        return WTFMove(result.value());

    if (m_method != "GET" && m_method != "HEAD") {
        if (!m_requestHeaders.contains(HTTPHeaderName::ContentType)) {
            // FIXME: this should include the charset used for encoding.
            m_requestHeaders.set(HTTPHeaderName::ContentType, document.isHTMLDocument() ? "text/html;charset=UTF-8"_s : "application/xml;charset=UTF-8"_s);
        } else {
            String contentType = m_requestHeaders.get(HTTPHeaderName::ContentType);
            replaceCharsetInMediaTypeIfNeeded(contentType);
            m_requestHeaders.set(HTTPHeaderName::ContentType, contentType);
        }

        // FIXME: According to XMLHttpRequest Level 2, this should use the Document.innerHTML algorithm
        // from the HTML5 specification to serialize the document.
        m_requestEntityBody = FormData::create(UTF8Encoding().encode(serializeFragment(document, SerializedNodes::SubtreeIncludingNode), UnencodableHandling::Entities));
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    return createRequest();
}

ExceptionOr<void> XMLHttpRequest::send(const String& body)
{
    if (auto result = prepareToSend())
        return WTFMove(result.value());

    if (!body.isNull() && m_method != "GET" && m_method != "HEAD") {
        String contentType = m_requestHeaders.get(HTTPHeaderName::ContentType);
        if (contentType.isNull()) {
            m_requestHeaders.set(HTTPHeaderName::ContentType, HTTPHeaderValues::textPlainContentType());
        } else {
            replaceCharsetInMediaTypeIfNeeded(contentType);
            m_requestHeaders.set(HTTPHeaderName::ContentType, contentType);
        }

        m_requestEntityBody = FormData::create(UTF8Encoding().encode(body, UnencodableHandling::Entities));
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    return createRequest();
}

ExceptionOr<void> XMLHttpRequest::send(Blob& body)
{
    if (auto result = prepareToSend())
        return WTFMove(result.value());

    if (m_method != "GET" && m_method != "HEAD") {
        if (!m_url.protocolIsInHTTPFamily()) {
            // FIXME: We would like to support posting Blobs to non-http URLs (e.g. custom URL schemes)
            // but because of the architecture of blob-handling that will require a fair amount of work.
            
            ASCIILiteral consoleMessage { "POST of a Blob to non-HTTP protocols in XMLHttpRequest.send() is currently unsupported."_s };
            scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, consoleMessage);
            
            return createRequest();
        }
        
        if (!m_requestHeaders.contains(HTTPHeaderName::ContentType)) {
            const String& blobType = body.type();
            if (!blobType.isEmpty() && isValidContentType(blobType))
                m_requestHeaders.set(HTTPHeaderName::ContentType, blobType);
        }

        m_requestEntityBody = FormData::create();
        m_requestEntityBody->appendBlob(body.url());
    }

    return createRequest();
}

ExceptionOr<void> XMLHttpRequest::send(DOMFormData& body)
{
    if (auto result = prepareToSend())
        return WTFMove(result.value());

    if (m_method != "GET" && m_method != "HEAD") {
        m_requestEntityBody = FormData::createMultiPart(body);
        if (!m_requestHeaders.contains(HTTPHeaderName::ContentType))
            m_requestHeaders.set(HTTPHeaderName::ContentType, makeString("multipart/form-data; boundary=", m_requestEntityBody->boundary().data()));
    }

    return createRequest();
}

ExceptionOr<void> XMLHttpRequest::send(ArrayBuffer& body)
{
    ASCIILiteral consoleMessage { "ArrayBuffer is deprecated in XMLHttpRequest.send(). Use ArrayBufferView instead."_s };
    scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, consoleMessage);
    return sendBytesData(body.data(), body.byteLength());
}

ExceptionOr<void> XMLHttpRequest::send(ArrayBufferView& body)
{
    return sendBytesData(body.baseAddress(), body.byteLength());
}

ExceptionOr<void> XMLHttpRequest::sendBytesData(const void* data, size_t length)
{
    if (auto result = prepareToSend())
        return WTFMove(result.value());

    if (m_method != "GET" && m_method != "HEAD") {
        m_requestEntityBody = FormData::create(data, length);
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    return createRequest();
}

ExceptionOr<void> XMLHttpRequest::createRequest()
{
    // Only GET request is supported for blob URL.
    if (!m_async && m_url.protocolIsBlob() && m_method != "GET")
        return Exception { NetworkError };

    if (m_async && m_upload && m_upload->hasEventListeners())
        m_uploadListenerFlag = true;

    ResourceRequest request(m_url);
    request.setRequester(ResourceRequest::Requester::XHR);
    request.setInitiatorIdentifier(scriptExecutionContext()->resourceRequestIdentifier());
    request.setHTTPMethod(m_method);

    if (m_requestEntityBody) {
        ASSERT(m_method != "GET");
        ASSERT(m_method != "HEAD");
        request.setHTTPBody(WTFMove(m_requestEntityBody));
    }

    if (!m_requestHeaders.isEmpty())
        request.setHTTPHeaderFields(m_requestHeaders);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    // The presence of upload event listeners forces us to use preflighting because POSTing to an URL that does not
    // permit cross origin requests should look exactly like POSTing to an URL that does not respond at all.
    options.preflightPolicy = m_uploadListenerFlag ? PreflightPolicy::Force : PreflightPolicy::Consider;
    options.credentials = m_includeCredentials ? FetchOptions::Credentials::Include : FetchOptions::Credentials::SameOrigin;
    options.mode = FetchOptions::Mode::Cors;
    options.contentSecurityPolicyEnforcement = scriptExecutionContext()->shouldBypassMainWorldContentSecurityPolicy() ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceConnectSrcDirective;
    options.initiator = cachedResourceRequestInitiators().xmlhttprequest;
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
    options.filteringPolicy = ResponseFilteringPolicy::Enable;
    options.sniffContentEncoding = ContentEncodingSniffingPolicy::DoNotSniff;

    if (m_timeoutMilliseconds) {
        if (!m_async)
            request.setTimeoutInterval(m_timeoutMilliseconds / 1000.0);
        else {
            request.setTimeoutInterval(std::numeric_limits<double>::infinity());
            m_sendingTime = MonotonicTime::now();
            m_timeoutTimer.startOneShot(1_ms * m_timeoutMilliseconds);
        }
    }

    m_exceptionCode = WTF::nullopt;
    m_error = false;
    m_uploadComplete = !request.httpBody();
    m_sendFlag = true;

    if (m_async) {
        m_progressEventThrottle.dispatchProgressEvent(eventNames().loadstartEvent);
        if (!m_uploadComplete && m_uploadListenerFlag)
            m_upload->dispatchProgressEvent(eventNames().loadstartEvent, 0, request.httpBody()->lengthInBytes());

        if (readyState() != OPENED || !m_sendFlag || m_loadingActivity)
            return { };

        // ThreadableLoader::create can return null here, for example if we're no longer attached to a page or if a content blocker blocks the load.
        // This is true while running onunload handlers.
        // FIXME: Maybe we need to be able to send XMLHttpRequests from onunload, <http://bugs.webkit.org/show_bug.cgi?id=10904>.
        auto loader = ThreadableLoader::create(*scriptExecutionContext(), *this, WTFMove(request), options);
        if (loader)
            m_loadingActivity = LoadingActivity { makeRef(*this), loader.releaseNonNull() };

        // Either loader is null or some error was synchronously sent to us.
        ASSERT(m_loadingActivity || !m_sendFlag);
    } else {
        if (scriptExecutionContext()->isDocument() && !isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::SyncXHR, *document()))
            return Exception { NetworkError };

        request.setDomainForCachePartition(scriptExecutionContext()->domainForCachePartition());
        InspectorInstrumentation::willLoadXHRSynchronously(scriptExecutionContext());
        ThreadableLoader::loadResourceSynchronously(*scriptExecutionContext(), WTFMove(request), *this, options);
        InspectorInstrumentation::didLoadXHRSynchronously(scriptExecutionContext());
    }

    if (m_exceptionCode)
        return Exception { m_exceptionCode.value() };
    if (m_error)
        return Exception { NetworkError };
    return { };
}

void XMLHttpRequest::abort()
{
    Ref<XMLHttpRequest> protectedThis(*this);

    m_wasAbortedByClient = true;
    if (!internalAbort())
        return;

    clearResponseBuffers();

    m_requestHeaders.clear();
    if ((readyState() == OPENED && m_sendFlag) || readyState() == HEADERS_RECEIVED || readyState() == LOADING) {
        ASSERT(!m_loadingActivity);
        m_sendFlag = false;
        changeState(DONE);
        dispatchErrorEvents(eventNames().abortEvent);
    }
    if (readyState() == DONE)
        m_readyState = static_cast<State>(UNSENT);
}

bool XMLHttpRequest::internalAbort()
{
    m_error = true;

    // FIXME: when we add the support for multi-part XHR, we will have to think be careful with this initialization.
    m_receivedLength = 0;

    m_decoder = nullptr;

    m_timeoutTimer.stop();

    if (!m_loadingActivity)
        return true;

    // Cancelling m_loadingActivity may trigger a window.onload callback which can call open() on the same xhr.
    // This would create internalAbort reentrant call.
    // m_loadingActivity is set to WTF::nullopt before being cancelled to exit early in any reentrant internalAbort() call.
    auto loadingActivity = std::exchange(m_loadingActivity, WTF::nullopt);
    loadingActivity->loader->cancel();

    // If window.onload callback calls open() and send() on the same xhr, m_loadingActivity is now set to a new value.
    // The function calling internalAbort() should abort to let the open() and send() calls continue properly.
    // We ask the function calling internalAbort() to exit by returning false.
    // Save this information to a local variable since we are going to drop protection.
    bool newLoadStarted = !!m_loadingActivity;

    return !newLoadStarted;
}

void XMLHttpRequest::clearResponse()
{
    m_response = ResourceResponse();
    clearResponseBuffers();
}

void XMLHttpRequest::clearResponseBuffers()
{
    m_responseBuilder.clear();
    m_responseEncoding = String();
    m_createdDocument = false;
    m_responseDocument = nullptr;
    m_binaryResponseBuilder = nullptr;
    m_responseCacheIsValid = false;
}

void XMLHttpRequest::clearRequest()
{
    m_requestHeaders.clear();
    m_requestEntityBody = nullptr;
}

void XMLHttpRequest::genericError()
{
    clearResponse();
    clearRequest();
    m_sendFlag = false;
    m_error = true;

    changeState(DONE);
}

void XMLHttpRequest::networkError()
{
    genericError();
    dispatchErrorEvents(eventNames().errorEvent);
    internalAbort();
}
    
void XMLHttpRequest::abortError()
{
    ASSERT(m_wasAbortedByClient);
    genericError();
    dispatchErrorEvents(eventNames().abortEvent);
}

size_t XMLHttpRequest::memoryCost() const
{
    if (readyState() == DONE)
        return m_responseBuilder.length() * 2;
    return 0;
}

ExceptionOr<void> XMLHttpRequest::overrideMimeType(const String& mimeType)
{
    if (readyState() == LOADING || readyState() == DONE)
        return Exception { InvalidStateError };

    m_mimeTypeOverride = "application/octet-stream"_s;
    if (isValidContentType(mimeType))
        m_mimeTypeOverride = mimeType;

    return { };
}

ExceptionOr<void> XMLHttpRequest::setRequestHeader(const String& name, const String& value)
{
    if (readyState() != OPENED || m_sendFlag)
        return Exception { InvalidStateError };

    String normalizedValue = stripLeadingAndTrailingHTTPSpaces(value);
    if (!isValidHTTPToken(name) || !isValidHTTPHeaderValue(normalizedValue))
        return Exception { SyntaxError };

    bool allowUnsafeHeaderField = false;
    // FIXME: The allowSettingAnyXHRHeaderFromFileURLs setting currently only applies to Documents, not workers.
    if (securityOrigin()->canLoadLocalResources() && scriptExecutionContext()->isDocument() && document()->settings().allowSettingAnyXHRHeaderFromFileURLs())
        allowUnsafeHeaderField = true;
    if (!allowUnsafeHeaderField && isForbiddenHeaderName(name)) {
        logConsoleError(scriptExecutionContext(), "Refused to set unsafe header \"" + name + "\"");
        return { };
    }

    m_requestHeaders.add(name, normalizedValue);
    return { };
}

String XMLHttpRequest::getAllResponseHeaders() const
{
    if (readyState() < HEADERS_RECEIVED || m_error)
        return emptyString();

    if (!m_allResponseHeaders) {
        Vector<std::pair<String, String>> headers;
        headers.reserveInitialCapacity(m_response.httpHeaderFields().size());

        for (auto& header : m_response.httpHeaderFields())
            headers.uncheckedAppend(std::make_pair(header.key, header.value));

        std::sort(headers.begin(), headers.end(), [] (const std::pair<String, String>& x, const std::pair<String, String>& y) {
            unsigned xLength = x.first.length();
            unsigned yLength = y.first.length();
            unsigned commonLength = std::min(xLength, yLength);
            for (unsigned i = 0; i < commonLength; ++i) {
                auto xCharacter = toASCIIUpper(x.first[i]);
                auto yCharacter = toASCIIUpper(y.first[i]);
                if (xCharacter != yCharacter)
                    return xCharacter < yCharacter;
            }
            return xLength < yLength;
        });

        StringBuilder stringBuilder;
        for (auto& header : headers)
            stringBuilder.append(lowercase(header.first), ": ", header.second, "\r\n");

        m_allResponseHeaders = stringBuilder.toString();
    }

    return m_allResponseHeaders;
}

String XMLHttpRequest::getResponseHeader(const String& name) const
{
    if (readyState() < HEADERS_RECEIVED || m_error)
        return String();

    return m_response.httpHeaderField(name);
}

String XMLHttpRequest::responseMIMEType() const
{
    String mimeType = extractMIMETypeFromMediaType(m_mimeTypeOverride);
    if (mimeType.isEmpty()) {
        // Same logic as externalEntityMimeTypeAllowed() in XMLDocumentParserLibxml2.cpp. Keep them in sync.
        String contentType;
        if (m_response.isInHTTPFamily())
            contentType = m_response.httpHeaderField(HTTPHeaderName::ContentType);
        else
            contentType = m_response.mimeType();
        if (auto parsedContentType = ParsedContentType::create(contentType))
            return parsedContentType->mimeType();
        return "text/xml"_s;
    }
    return mimeType;
}

bool XMLHttpRequest::responseIsXML() const
{
    return MIMETypeRegistry::isXMLMIMEType(responseMIMEType());
}

int XMLHttpRequest::status() const
{
    if (readyState() == UNSENT || readyState() == OPENED || m_error)
        return 0;

    return m_response.httpStatusCode();
}

String XMLHttpRequest::statusText() const
{
    if (readyState() == UNSENT || readyState() == OPENED || m_error)
        return String();

    return m_response.httpStatusText();
}

void XMLHttpRequest::didFail(const ResourceError& error)
{
    auto protectedThis = makeRef(*this);

    // If we are already in an error state, for instance we called abort(), bail out early.
    if (m_error)
        return;

    // The XHR specification says we should only fire an abort event if the cancelation was requested by the client.
    if (m_wasAbortedByClient && error.isCancellation()) {
        m_exceptionCode = AbortError;
        abortError();
        return;
    }

    // In case of worker sync timeouts.
    if (error.isTimeout()) {
        didReachTimeout();
        return;
    }

    // In case didFail is called synchronously on an asynchronous XHR call, let's dispatch network error asynchronously
    if (m_async && m_sendFlag && !m_loadingActivity) {
        m_sendFlag = false;
        m_timeoutTimer.stop();
        queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this] {
            networkError();
        });
        return;
    }
    m_exceptionCode = NetworkError;
    networkError();
}

void XMLHttpRequest::didFinishLoading(unsigned long)
{
    auto protectedThis = makeRef(*this);

    if (m_error)
        return;

    if (readyState() < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    if (m_decoder)
        m_responseBuilder.append(m_decoder->flush());

    m_responseBuilder.shrinkToFit();

    m_loadingActivity = WTF::nullopt;

    m_sendFlag = false;
    changeState(DONE);
    m_responseEncoding = String();
    m_decoder = nullptr;

    m_timeoutTimer.stop();
}

void XMLHttpRequest::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (!m_upload)
        return;

    if (m_uploadListenerFlag)
        m_upload->dispatchProgressEvent(eventNames().progressEvent, bytesSent, totalBytesToBeSent);

    if (bytesSent == totalBytesToBeSent && !m_uploadComplete) {
        m_uploadComplete = true;
        if (m_uploadListenerFlag) {
            m_upload->dispatchProgressEvent(eventNames().loadEvent, bytesSent, totalBytesToBeSent);
            m_upload->dispatchProgressEvent(eventNames().loadendEvent, bytesSent, totalBytesToBeSent);
        }
    }
}

void XMLHttpRequest::didReceiveResponse(unsigned long, const ResourceResponse& response)
{
    m_response = response;
}

static inline bool shouldDecodeResponse(XMLHttpRequest::ResponseType type)
{
    switch (type) {
    case XMLHttpRequest::ResponseType::EmptyString:
    case XMLHttpRequest::ResponseType::Document:
    case XMLHttpRequest::ResponseType::Json:
    case XMLHttpRequest::ResponseType::Text:
        return true;
    case XMLHttpRequest::ResponseType::Arraybuffer:
    case XMLHttpRequest::ResponseType::Blob:
        return false;
    }
    ASSERT_NOT_REACHED();
    return true;
}

// https://xhr.spec.whatwg.org/#final-charset
TextEncoding XMLHttpRequest::finalResponseCharset() const
{
    String label = m_responseEncoding;

    String overrideResponseCharset = extractCharsetFromMediaType(m_mimeTypeOverride);
    if (!overrideResponseCharset.isEmpty())
        label = overrideResponseCharset;

    return TextEncoding(label);
}

Ref<TextResourceDecoder> XMLHttpRequest::createDecoder() const
{
    TextEncoding finalResponseCharset = this->finalResponseCharset();
    if (finalResponseCharset.isValid())
        return TextResourceDecoder::create("text/plain", finalResponseCharset);

    switch (responseType()) {
    case ResponseType::EmptyString:
        if (responseIsXML()) {
            auto decoder = TextResourceDecoder::create("application/xml");
            // Don't stop on encoding errors, unlike it is done for other kinds of XML resources. This matches the behavior of previous WebKit versions, Firefox and Opera.
            decoder->useLenientXMLDecoding();
            return decoder;
        }
        FALLTHROUGH;
    case ResponseType::Text:
    case ResponseType::Json:
        return TextResourceDecoder::create("text/plain", "UTF-8");
    case ResponseType::Document: {
        if (equalLettersIgnoringASCIICase(responseMIMEType(), "text/html"))
            return TextResourceDecoder::create("text/html", "UTF-8");
        auto decoder = TextResourceDecoder::create("application/xml");
        // Don't stop on encoding errors, unlike it is done for other kinds of XML resources. This matches the behavior of previous WebKit versions, Firefox and Opera.
        decoder->useLenientXMLDecoding();
        return decoder;
    }
    case ResponseType::Arraybuffer:
    case ResponseType::Blob:
        ASSERT_NOT_REACHED();
        break;
    }
    return TextResourceDecoder::create("text/plain", "UTF-8");
}

void XMLHttpRequest::didReceiveData(const char* data, int len)
{
    if (m_error)
        return;

    if (readyState() < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    if (!m_mimeTypeOverride.isEmpty())
        m_responseEncoding = extractCharsetFromMediaType(m_mimeTypeOverride);
    if (m_responseEncoding.isEmpty())
        m_responseEncoding = m_response.textEncodingName();

    bool useDecoder = shouldDecodeResponse(responseType());

    if (useDecoder && !m_decoder)
        m_decoder = createDecoder();

    if (!len)
        return;

    if (len == -1)
        len = strlen(data);

    if (useDecoder)
        m_responseBuilder.append(m_decoder->decode(data, len));
    else {
        // Buffer binary data.
        if (!m_binaryResponseBuilder)
            m_binaryResponseBuilder = SharedBuffer::create();
        m_binaryResponseBuilder->append(data, len);
    }

    if (!m_error) {
        m_receivedLength += len;

        if (readyState() != LOADING)
            changeState(LOADING);
        else {
            // Firefox calls readyStateChanged every time it receives data, 4449442
            callReadyStateChangeListener();
        }

        if (m_async) {
            long long expectedLength = m_response.expectedContentLength();
            bool lengthComputable = expectedLength > 0 && m_receivedLength <= expectedLength;
            unsigned long long total = lengthComputable ? expectedLength : 0;
            m_progressEventThrottle.dispatchThrottledProgressEvent(lengthComputable, m_receivedLength, total);
        }
    }
}

void XMLHttpRequest::dispatchEvent(Event& event)
{
    RELEASE_ASSERT(!scriptExecutionContext()->activeDOMObjectsAreSuspended());

    if (m_userGestureToken && m_userGestureToken->hasExpired(UserGestureToken::maximumIntervalForUserGestureForwardingForFetch()))
        m_userGestureToken = nullptr;

    if (readyState() != DONE || !m_userGestureToken || !m_userGestureToken->processingUserGesture()) {
        EventTarget::dispatchEvent(event);
        return;
    }

    UserGestureIndicator gestureIndicator(m_userGestureToken, UserGestureToken::GestureScope::MediaOnly);
    EventTarget::dispatchEvent(event);
}

void XMLHttpRequest::dispatchErrorEvents(const AtomString& type)
{
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadListenerFlag) {
            m_upload->dispatchProgressEvent(type, 0, 0);
            m_upload->dispatchProgressEvent(eventNames().loadendEvent, 0, 0);
        }
    }
    m_progressEventThrottle.dispatchProgressEvent(type);
    m_progressEventThrottle.dispatchProgressEvent(eventNames().loadendEvent);
}

void XMLHttpRequest::didReachTimeout()
{
    Ref<XMLHttpRequest> protectedThis(*this);
    if (!internalAbort())
        return;

    clearResponse();
    clearRequest();

    m_sendFlag = false;
    m_error = true;
    m_exceptionCode = TimeoutError;

    if (!m_async) {
        m_readyState = static_cast<State>(DONE);
        m_exceptionCode = TimeoutError;
        return;
    }

    changeState(DONE);

    dispatchErrorEvents(eventNames().timeoutEvent);
}

const char* XMLHttpRequest::activeDOMObjectName() const
{
    return "XMLHttpRequest";
}

void XMLHttpRequest::suspend(ReasonForSuspension)
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
    ASSERT(!m_loadingActivity);
    ActiveDOMObject::contextDestroyed();
}

void XMLHttpRequest::eventListenersDidChange()
{
    m_hasRelevantEventListener = hasEventListeners(eventNames().abortEvent)
        || hasEventListeners(eventNames().errorEvent)
        || hasEventListeners(eventNames().loadEvent)
        || hasEventListeners(eventNames().loadendEvent)
        || hasEventListeners(eventNames().progressEvent)
        || hasEventListeners(eventNames().readystatechangeEvent)
        || hasEventListeners(eventNames().timeoutEvent);
}

// An XMLHttpRequest object must not be garbage collected if its state is either opened with the send() flag set, headers received, or loading, and
// it has one or more event listeners registered whose type is one of readystatechange, progress, abort, error, load, timeout, and loadend.
bool XMLHttpRequest::virtualHasPendingActivity() const
{
    if (!m_hasRelevantEventListener && !(m_upload && m_upload->hasRelevantEventListener()))
        return false;

    switch (readyState()) {
    case OPENED:
        return m_sendFlag;
    case HEADERS_RECEIVED:
    case LOADING:
        return true;
    case UNSENT:
    case DONE:
        break;
    }
    return false;
}

} // namespace WebCore
