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

#include "Dictionary.h"
#include "ExceptionCode.h"
#include "HTTPParsers.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

namespace WebCore {

static Optional<Exception> setReferrerPolicy(FetchOptions& options, const String& referrerPolicy)
{
    if (referrerPolicy.isEmpty())
        options.referrerPolicy = FetchOptions::ReferrerPolicy::EmptyString;
    else if (referrerPolicy == "no-referrer")
        options.referrerPolicy = FetchOptions::ReferrerPolicy::NoReferrer;
    else if (referrerPolicy == "no-referrer-when-downgrade")
        options.referrerPolicy = FetchOptions::ReferrerPolicy::NoReferrerWhenDowngrade;
    else if (referrerPolicy == "origin")
        options.referrerPolicy = FetchOptions::ReferrerPolicy::Origin;
    else if (referrerPolicy == "origin-when-cross-origin")
        options.referrerPolicy = FetchOptions::ReferrerPolicy::OriginWhenCrossOrigin;
    else if (referrerPolicy == "unsafe-url")
        options.referrerPolicy = FetchOptions::ReferrerPolicy::UnsafeUrl;
    else
        return Exception { TypeError, ASCIILiteral("Bad referrer policy value.") };
    return Nullopt;
}

static Optional<Exception> setMode(FetchOptions& options, const String& mode)
{
    if (mode == "navigate")
        options.mode = FetchOptions::Mode::Navigate;
    else if (mode == "same-origin")
        options.mode = FetchOptions::Mode::SameOrigin;
    else if (mode == "no-cors")
        options.mode = FetchOptions::Mode::NoCors;
    else if (mode == "cors")
        options.mode = FetchOptions::Mode::Cors;
    else
        return Exception { TypeError, ASCIILiteral("Bad fetch mode value.") };
    return Nullopt;
}

static Optional<Exception> setCredentials(FetchOptions& options, const String& credentials)
{
    if (credentials == "omit")
        options.credentials = FetchOptions::Credentials::Omit;
    else if (credentials == "same-origin")
        options.credentials = FetchOptions::Credentials::SameOrigin;
    else if (credentials == "include")
        options.credentials = FetchOptions::Credentials::Include;
    else
        return Exception { TypeError, ASCIILiteral("Bad credentials mode value.") };
    return Nullopt;
}

static Optional<Exception> setCache(FetchOptions& options, const String& cache)
{
    if (cache == "default")
        options.cache = FetchOptions::Cache::Default;
    else if (cache == "no-store")
        options.cache = FetchOptions::Cache::NoStore;
    else if (cache == "reload")
        options.cache = FetchOptions::Cache::Reload;
    else if (cache == "no-cache")
        options.cache = FetchOptions::Cache::NoCache;
    else if (cache == "force-cache")
        options.cache = FetchOptions::Cache::ForceCache;
    else if (cache == "only-if-cached")
        options.cache = FetchOptions::Cache::OnlyIfCached;
    else
        return Exception { TypeError, ASCIILiteral("Bad cache mode value.") };
    return Nullopt;
}

static Optional<Exception> setRedirect(FetchOptions& options, const String& redirect)
{
    if (redirect == "follow")
        options.redirect = FetchOptions::Redirect::Follow;
    else if (redirect == "error")
        options.redirect = FetchOptions::Redirect::Error;
    else if (redirect == "manual")
        options.redirect = FetchOptions::Redirect::Manual;
    else
        return Exception { TypeError, ASCIILiteral("Bad redirect mode value.") };
    return Nullopt;
}

static Optional<Exception> setMethod(ResourceRequest& request, const String& initMethod)
{
    if (!isValidHTTPToken(initMethod))
        return Exception { TypeError, ASCIILiteral("Method is not a valid HTTP token.") };

    String method = initMethod.convertToASCIIUppercase();
    if (method == "CONNECT" || method == "TRACE" || method == "TRACK")
        return Exception { TypeError, ASCIILiteral("Method is forbidden.") };

    request.setHTTPMethod((method == "DELETE" || method == "GET" || method == "HEAD" || method == "OPTIONS" || method == "POST" || method == "PUT") ? method : initMethod);

    return Nullopt;
}

static Optional<Exception> setReferrer(FetchRequest::InternalRequest& request, ScriptExecutionContext& context, const Dictionary& init)
{
    String referrer;
    if (!init.get("referrer", referrer))
        return Nullopt;
    if (referrer.isEmpty()) {
        request.referrer = ASCIILiteral("no-referrer");
        return Nullopt;
    }
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL referrerURL = context.completeURL(referrer);
    if (!referrerURL.isValid())
        return Exception { TypeError, ASCIILiteral("Referrer is not a valid URL.") };

    if (referrerURL.protocolIs("about") && referrerURL.path() == "client") {
        request.referrer = ASCIILiteral("client");
        return Nullopt;
    }

    if (!(context.securityOrigin() && context.securityOrigin()->canRequest(referrerURL)))
        return Exception { TypeError, ASCIILiteral("Referrer is not same-origin.") };

    request.referrer = referrerURL.string();
    return Nullopt;
}

static Optional<Exception> buildOptions(FetchRequest::InternalRequest& request, ScriptExecutionContext& context, const Dictionary& init)
{
    JSC::JSValue window;
    if (init.get("window", window)) {
        if (!window.isNull())
            return Exception { TypeError, ASCIILiteral("Window can only be null.") };
    }

    auto exception = setReferrer(request, context, init);
    if (exception)
        return exception;

    String value;
    if (init.get("referrerPolicy", value)) {
        exception = setReferrerPolicy(request.options, value);
        if (exception)
            return exception;
    }

    if (init.get("mode", value)) {
        exception = setMode(request.options, value);
        if (exception)
            return exception;
    }
    if (request.options.mode == FetchOptions::Mode::Navigate)
        return Exception { TypeError, ASCIILiteral("Request constructor does not accept navigate fetch mode.") };

    if (init.get("credentials", value)) {
        exception = setCredentials(request.options, value);
        if (exception)
            return exception;
    }

    if (init.get("cache", value)) {
        exception = setCache(request.options, value);
        if (exception)
            return exception;
    }

    if (request.options.cache == FetchOptions::Cache::OnlyIfCached && request.options.mode != FetchOptions::Mode::SameOrigin)
        return Exception { TypeError, ASCIILiteral("only-if-cached cache option requires fetch mode to be same-origin.")  };

    if (init.get("redirect", value)) {
        exception = setRedirect(request.options, value);
        if (exception)
            return exception;
    }

    init.get("integrity", request.integrity);

    if (init.get("method", value)) {
        exception = setMethod(request.request, value);
        if (exception)
            return exception;
    }
    return Nullopt;
}

static bool methodCanHaveBody(const FetchRequest::InternalRequest& internalRequest)
{
    return internalRequest.request.httpMethod() != "GET" && internalRequest.request.httpMethod() != "HEAD";
}

ExceptionOr<FetchHeaders&> FetchRequest::initializeOptions(const Dictionary& init)
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

ExceptionOr<FetchHeaders&> FetchRequest::initializeWith(const String& url, const Dictionary& init)
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
    m_internalRequest.request.setInitiatorIdentifier(scriptExecutionContext()->resourceRequestIdentifier());

    return initializeOptions(init);
}

ExceptionOr<FetchHeaders&> FetchRequest::initializeWith(FetchRequest& input, const Dictionary& init)
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

    auto clone = adoptRef(*new FetchRequest(context, Nullopt, FetchHeaders::create(m_headers.get()), FetchRequest::InternalRequest(m_internalRequest)));
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
