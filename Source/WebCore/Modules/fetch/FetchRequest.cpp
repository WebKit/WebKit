/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchRequest.h"

#include "Document.h"
#include "HTTPParsers.h"
#include "JSAbortSignal.h"
#include "Logging.h"
#include "Quirks.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "Settings.h"

namespace WebCore {

static Optional<Exception> setMethod(ResourceRequest& request, const String& initMethod)
{
    if (!isValidHTTPToken(initMethod))
        return Exception { TypeError, "Method is not a valid HTTP token."_s };
    if (isForbiddenMethod(initMethod))
        return Exception { TypeError, "Method is forbidden."_s };
    request.setHTTPMethod(normalizeHTTPMethod(initMethod));
    return WTF::nullopt;
}

static ExceptionOr<String> computeReferrer(ScriptExecutionContext& context, const String& referrer)
{
    if (referrer.isEmpty())
        return "no-referrer"_str;

    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL referrerURL = context.completeURL(referrer);
    if (!referrerURL.isValid())
        return Exception { TypeError, "Referrer is not a valid URL."_s };

    if (referrerURL.protocolIs("about") && referrerURL.path() == "client")
        return "client"_str;

    if (!(context.securityOrigin() && context.securityOrigin()->canRequest(referrerURL)))
        return "client"_str;

    return String { referrerURL.string() };
}

static Optional<Exception> buildOptions(FetchOptions& options, ResourceRequest& request, String& referrer, ScriptExecutionContext& context, const FetchRequest::Init& init)
{
    if (!init.window.isUndefinedOrNull() && !init.window.isEmpty())
        return Exception { TypeError, "Window can only be null."_s };

    if (init.hasMembers()) {
        if (options.mode == FetchOptions::Mode::Navigate)
            options.mode = FetchOptions::Mode::SameOrigin;
        referrer = "client"_s;
        options.referrerPolicy = { };
    }

    if (!init.referrer.isNull()) {
        auto result = computeReferrer(context, init.referrer);
        if (result.hasException())
            return result.releaseException();
        referrer = result.releaseReturnValue();
    }

    if (init.referrerPolicy)
        options.referrerPolicy = init.referrerPolicy.value();

    if (init.mode) {
        options.mode = init.mode.value();
        if (options.mode == FetchOptions::Mode::Navigate)
            return Exception { TypeError, "Request constructor does not accept navigate fetch mode."_s };
    }

    if (init.credentials)
        options.credentials = init.credentials.value();

    if (init.cache)
        options.cache = init.cache.value();
    if (options.cache == FetchOptions::Cache::OnlyIfCached && options.mode != FetchOptions::Mode::SameOrigin)
        return Exception { TypeError, "only-if-cached cache option requires fetch mode to be same-origin."_s  };

    if (init.redirect)
        options.redirect = init.redirect.value();

    if (!init.integrity.isNull())
        options.integrity = init.integrity;

    if (init.keepalive && init.keepalive.value())
        options.keepAlive = true;

    if (!init.method.isNull()) {
        if (auto exception = setMethod(request, init.method))
            return exception;
    }

    return WTF::nullopt;
}

static bool methodCanHaveBody(const ResourceRequest& request)
{
    return request.httpMethod() != "GET" && request.httpMethod() != "HEAD";
}

ExceptionOr<void> FetchRequest::initializeOptions(const Init& init)
{
    ASSERT(scriptExecutionContext());

    auto exception = buildOptions(m_options, m_request, m_referrer, *scriptExecutionContext(), init);
    if (exception)
        return WTFMove(exception.value());

    if (m_options.mode == FetchOptions::Mode::NoCors) {
        const String& method = m_request.httpMethod();
        if (method != "GET" && method != "POST" && method != "HEAD")
            return Exception { TypeError, "Method must be GET, POST or HEAD in no-cors mode."_s };
        m_headers->setGuard(FetchHeaders::Guard::RequestNoCors);
    }
    
    return { };
}

static inline Optional<Exception> processInvalidSignal(ScriptExecutionContext& context)
{
    ASCIILiteral message { "FetchRequestInit.signal should be undefined, null or an AbortSignal object."_s };
    context.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, message);

    if (is<Document>(context) && downcast<Document>(context).quirks().shouldIgnoreInvalidSignal())
        return { };

    RELEASE_LOG_ERROR(ResourceLoading, "FetchRequestInit.signal should be undefined, null or an AbortSignal object.");
    return Exception { TypeError, message };
}

ExceptionOr<void> FetchRequest::initializeWith(const String& url, Init&& init)
{
    ASSERT(scriptExecutionContext());
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL requestURL = scriptExecutionContext()->completeURL(url);
    if (!requestURL.isValid() || !requestURL.user().isEmpty() || !requestURL.pass().isEmpty())
        return Exception { TypeError, "URL is not valid or contains user credentials."_s };

    m_options.mode = Mode::Cors;
    m_options.credentials = Credentials::SameOrigin;
    m_referrer = "client"_s;
    m_request.setURL(requestURL);
    m_request.setRequester(ResourceRequest::Requester::Fetch);
    m_request.setInitiatorIdentifier(scriptExecutionContext()->resourceRequestIdentifier());

    auto optionsResult = initializeOptions(init);
    if (optionsResult.hasException())
        return optionsResult.releaseException();

    if (init.signal) {
        if (auto* signal = JSAbortSignal::toWrapped(scriptExecutionContext()->vm(), init.signal))
            m_signal->follow(*signal);
        else if (!init.signal.isUndefinedOrNull())  {
            if (auto exception = processInvalidSignal(*scriptExecutionContext()))
                return WTFMove(*exception);
        }
    }

    if (init.headers) {
        auto fillResult = m_headers->fill(*init.headers);
        if (fillResult.hasException())
            return fillResult.releaseException();
    }

    if (init.body) {
        auto setBodyResult = setBody(WTFMove(*init.body));
        if (setBodyResult.hasException())
            return setBodyResult.releaseException();
    }

    updateContentType();
    return { };
}

ExceptionOr<void> FetchRequest::initializeWith(FetchRequest& input, Init&& init)
{
    if (input.isDisturbedOrLocked())
        return Exception {TypeError, "Request input is disturbed or locked."_s };

    m_request = input.m_request;
    m_options = input.m_options;
    m_referrer = input.m_referrer;

    auto optionsResult = initializeOptions(init);
    if (optionsResult.hasException())
        return optionsResult.releaseException();

    if (init.signal && !init.signal.isUndefined()) {
        if (auto* signal = JSAbortSignal::toWrapped(scriptExecutionContext()->vm(), init.signal))
            m_signal->follow(*signal);
        else if (!init.signal.isNull()) {
            if (auto exception = processInvalidSignal(*scriptExecutionContext()))
                return WTFMove(*exception);
        }

    } else
        m_signal->follow(input.m_signal.get());

    if (init.headers) {
        auto fillResult = m_headers->fill(*init.headers);
        if (fillResult.hasException())
            return fillResult.releaseException();
    } else {
        auto fillResult = m_headers->fill(input.headers());
        if (fillResult.hasException())
            return fillResult.releaseException();
    }

    if (init.body) {
        auto setBodyResult = setBody(WTFMove(*init.body));
        if (setBodyResult.hasException())
            return setBodyResult.releaseException();
    } else {
        auto setBodyResult = setBody(input);
        if (setBodyResult.hasException())
            return setBodyResult.releaseException();
    }

    updateContentType();
    return { };
}

ExceptionOr<void> FetchRequest::setBody(FetchBody::Init&& body)
{
    if (!methodCanHaveBody(m_request))
        return Exception { TypeError, makeString("Request has method '", m_request.httpMethod(), "' and cannot have a body") };

    ASSERT(scriptExecutionContext());
    extractBody(*scriptExecutionContext(), WTFMove(body));

    if (m_options.keepAlive && hasReadableStreamBody())
        return Exception { TypeError, "Request cannot have a ReadableStream body and keepalive set to true"_s };
    return { };
}

ExceptionOr<void> FetchRequest::setBody(FetchRequest& request)
{
    if (!request.isBodyNull()) {
        if (!methodCanHaveBody(m_request))
            return Exception { TypeError, makeString("Request has method '", m_request.httpMethod(), "' and cannot have a body") };
        // FIXME: If body has a readable stream, we should pipe it to this new body stream.
        m_body = WTFMove(*request.m_body);
        request.setDisturbed();
    }

    if (m_options.keepAlive && hasReadableStreamBody())
        return Exception { TypeError, "Request cannot have a ReadableStream body and keepalive set to true"_s };
    return { };
}

ExceptionOr<Ref<FetchRequest>> FetchRequest::create(ScriptExecutionContext& context, Info&& input, Init&& init)
{
    auto request = adoptRef(*new FetchRequest(context, WTF::nullopt, FetchHeaders::create(FetchHeaders::Guard::Request), { }, { }, { }));

    if (WTF::holds_alternative<String>(input)) {
        auto result = request->initializeWith(WTF::get<String>(input), WTFMove(init));
        if (result.hasException())
            return result.releaseException();
    } else {
        auto result = request->initializeWith(*WTF::get<RefPtr<FetchRequest>>(input), WTFMove(init));
        if (result.hasException())
            return result.releaseException();
    }

    return WTFMove(request);
}

String FetchRequest::referrer() const
{
    if (m_referrer == "no-referrer")
        return String();
    if (m_referrer == "client")
        return "about:client"_s;
    return m_referrer;
}

const String& FetchRequest::urlString() const
{
    if (m_requestURL.isNull())
        m_requestURL = m_request.url();
    return m_requestURL;
}

ResourceRequest FetchRequest::resourceRequest() const
{
    ASSERT(scriptExecutionContext());

    ResourceRequest request = m_request;
    request.setHTTPHeaderFields(m_headers->internalHeaders());

    if (!isBodyNull())
        request.setHTTPBody(body().bodyAsFormData(*scriptExecutionContext()));

    return request;
}

ExceptionOr<Ref<FetchRequest>> FetchRequest::clone(ScriptExecutionContext& context)
{
    if (isDisturbedOrLocked())
        return Exception { TypeError, "Body is disturbed or locked"_s };

    auto clone = adoptRef(*new FetchRequest(context, WTF::nullopt, FetchHeaders::create(m_headers.get()), ResourceRequest { m_request }, FetchOptions { m_options}, String { m_referrer }));
    clone->cloneBody(*this);
    clone->m_signal->follow(m_signal);
    return WTFMove(clone);
}

const char* FetchRequest::activeDOMObjectName() const
{
    return "Request";
}

bool FetchRequest::canSuspendForDocumentSuspension() const
{
    // FIXME: We can probably do the same strategy as XHR.
    return !isActive();
}

} // namespace WebCore

