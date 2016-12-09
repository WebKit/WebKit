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

#if ENABLE(FETCH_API)

#include "ExceptionCode.h"
#include "HTTPParsers.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

namespace WebCore {

static std::optional<Exception> setMethod(ResourceRequest& request, const String& initMethod)
{
    if (!isValidHTTPToken(initMethod))
        return Exception { TypeError, ASCIILiteral("Method is not a valid HTTP token.") };

    String method = initMethod.convertToASCIIUppercase();
    if (method == "CONNECT" || method == "TRACE" || method == "TRACK")
        return Exception { TypeError, ASCIILiteral("Method is forbidden.") };

    request.setHTTPMethod((method == "DELETE" || method == "GET" || method == "HEAD" || method == "OPTIONS" || method == "POST" || method == "PUT") ? method : initMethod);

    return std::nullopt;
}

static std::optional<Exception> setReferrer(FetchRequest::InternalRequest& request, ScriptExecutionContext& context, const String& referrer)
{
    if (referrer.isEmpty()) {
        request.referrer = ASCIILiteral("no-referrer");
        return std::nullopt;
    }
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL referrerURL = context.completeURL(referrer);
    if (!referrerURL.isValid())
        return Exception { TypeError, ASCIILiteral("Referrer is not a valid URL.") };

    if (referrerURL.protocolIs("about") && referrerURL.path() == "client") {
        request.referrer = ASCIILiteral("client");
        return std::nullopt;
    }

    if (!(context.securityOrigin() && context.securityOrigin()->canRequest(referrerURL)))
        return Exception { TypeError, ASCIILiteral("Referrer is not same-origin.") };

    request.referrer = referrerURL.string();
    return std::nullopt;
}

static std::optional<Exception> buildOptions(FetchRequest::InternalRequest& request, ScriptExecutionContext& context, const FetchRequest::Init& init)
{
    if (!init.window.isUndefinedOrNull())
        return Exception { TypeError, ASCIILiteral("Window can only be null.") };

    if (!init.referrer.isNull()) {
        if (auto exception = setReferrer(request, context, init.referrer))
            return exception;
    }

    if (init.referrerPolicy)
        request.options.referrerPolicy = init.referrerPolicy.value();

    if (init.mode)
        request.options.mode = init.mode.value();
    if (request.options.mode == FetchOptions::Mode::Navigate)
        return Exception { TypeError, ASCIILiteral("Request constructor does not accept navigate fetch mode.") };

    if (init.credentials)
        request.options.credentials = init.credentials.value();

    if (init.cache)
        request.options.cache = init.cache.value();
    if (request.options.cache == FetchOptions::Cache::OnlyIfCached && request.options.mode != FetchOptions::Mode::SameOrigin)
        return Exception { TypeError, ASCIILiteral("only-if-cached cache option requires fetch mode to be same-origin.")  };

    if (init.redirect)
        request.options.redirect = init.redirect.value();

    if (!init.integrity.isNull())
        request.integrity = init.integrity;

    if (!init.method.isNull()) {
        if (auto exception = setMethod(request.request, init.method))
            return exception;
    }

    return std::nullopt;
}

static bool methodCanHaveBody(const FetchRequest::InternalRequest& internalRequest)
{
    return internalRequest.request.httpMethod() != "GET" && internalRequest.request.httpMethod() != "HEAD";
}

ExceptionOr<FetchHeaders&> FetchRequest::initializeOptions(const Init& init)
{
    ASSERT(scriptExecutionContext());

    auto exception = buildOptions(m_internalRequest, *scriptExecutionContext(), init);
    if (exception)
        return WTFMove(exception.value());

    if (m_internalRequest.options.mode == FetchOptions::Mode::NoCors) {
        const String& method = m_internalRequest.request.httpMethod();
        if (method != "GET" && method != "POST" && method != "HEAD")
            return Exception { TypeError, ASCIILiteral("Method must be GET, POST or HEAD in no-cors mode.") };
        if (!m_internalRequest.integrity.isEmpty())
            return Exception { TypeError, ASCIILiteral("There cannot be an integrity in no-cors mode.") };
        m_headers->setGuard(FetchHeaders::Guard::RequestNoCors);
    }
    return m_headers.get();
}

ExceptionOr<FetchHeaders&> FetchRequest::initializeWith(const String& url, const Init& init)
{
    ASSERT(scriptExecutionContext());
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL requestURL = scriptExecutionContext()->completeURL(url);
    if (!requestURL.isValid() || !requestURL.user().isEmpty() || !requestURL.pass().isEmpty())
        return Exception { TypeError, ASCIILiteral("URL is not valid or contains user credentials.") };

    m_internalRequest.options.mode = Mode::Cors;
    m_internalRequest.options.credentials = Credentials::Omit;
    m_internalRequest.referrer = ASCIILiteral("client");
    m_internalRequest.request.setURL(requestURL);
    m_internalRequest.request.setRequester(ResourceRequest::Requester::Fetch);
    m_internalRequest.request.setInitiatorIdentifier(scriptExecutionContext()->resourceRequestIdentifier());

    return initializeOptions(init);
}

ExceptionOr<FetchHeaders&> FetchRequest::initializeWith(FetchRequest& input, const Init& init)
{
    if (input.isDisturbedOrLocked())
        return Exception {TypeError, ASCIILiteral("Request input is disturbed or locked.") };

    m_internalRequest = input.m_internalRequest;

    return initializeOptions(init);
}

ExceptionOr<void> FetchRequest::setBody(JSC::ExecState& execState, JSC::JSValue body, FetchRequest* request)
{
    if (!body.isNull()) {
        if (!methodCanHaveBody(m_internalRequest))
            return Exception { TypeError };
        ASSERT(scriptExecutionContext());
        extractBody(*scriptExecutionContext(), execState, body);
        if (isBodyNull())
            return Exception { TypeError };
    } else if (request && !request->isBodyNull()) {
        if (!methodCanHaveBody(m_internalRequest))
            return Exception { TypeError };
        m_body = WTFMove(request->m_body);
        request->setDisturbed();
    }
    updateContentType();
    return { };
}

String FetchRequest::referrer() const
{
    if (m_internalRequest.referrer == "no-referrer")
        return String();
    if (m_internalRequest.referrer == "client")
        return ASCIILiteral("about:client");
    return m_internalRequest.referrer;
}

const String& FetchRequest::url() const
{
    if (m_requestURL.isNull())
        m_requestURL = m_internalRequest.request.url().serialize();
    return m_requestURL;
}

ResourceRequest FetchRequest::internalRequest() const
{
    ASSERT(scriptExecutionContext());

    ResourceRequest request = m_internalRequest.request;
    request.setHTTPHeaderFields(m_headers->internalHeaders());

    if (!isBodyNull())
        request.setHTTPBody(body().bodyForInternalRequest(*scriptExecutionContext()));

    return request;
}

ExceptionOr<Ref<FetchRequest>> FetchRequest::clone(ScriptExecutionContext& context)
{
    if (isDisturbedOrLocked())
        return Exception { TypeError };

    auto clone = adoptRef(*new FetchRequest(context, std::nullopt, FetchHeaders::create(m_headers.get()), FetchRequest::InternalRequest(m_internalRequest)));
    clone->cloneBody(*this);
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

#endif // ENABLE(FETCH_API)
