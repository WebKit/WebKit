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

#include "HTTPParsers.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

namespace WebCore {

static std::optional<Exception> setMethod(ResourceRequest& request, const String& initMethod)
{
    if (!isValidHTTPToken(initMethod))
        return Exception { TypeError, ASCIILiteral("Method is not a valid HTTP token.") };
    if (isForbiddenMethod(initMethod))
        return Exception { TypeError, ASCIILiteral("Method is forbidden.") };
    request.setHTTPMethod(normalizeHTTPMethod(initMethod));
    return std::nullopt;
}

static ExceptionOr<String> computeReferrer(ScriptExecutionContext& context, const String& referrer)
{
    if (referrer.isEmpty())
        return String { "no-referrer" };

    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL referrerURL = context.completeURL(referrer);
    if (!referrerURL.isValid())
        return Exception { TypeError, ASCIILiteral("Referrer is not a valid URL.") };

    if (referrerURL.protocolIs("about") && referrerURL.path() == "client")
        return String { "client" };

    if (!(context.securityOrigin() && context.securityOrigin()->canRequest(referrerURL)))
        return Exception { TypeError, ASCIILiteral("Referrer is not same-origin.") };

    return String { referrerURL.string() };
}

static std::optional<Exception> buildOptions(FetchOptions& options, ResourceRequest& request, String& referrer, ScriptExecutionContext& context, const FetchRequest::Init& init)
{
    if (!init.window.isUndefinedOrNull() && !init.window.isEmpty())
        return Exception { TypeError, ASCIILiteral("Window can only be null.") };

    if (init.hasMembers()) {
        if (options.mode == FetchOptions::Mode::Navigate)
            options.mode = FetchOptions::Mode::SameOrigin;
        referrer = ASCIILiteral("client");
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
            return Exception { TypeError, ASCIILiteral("Request constructor does not accept navigate fetch mode.") };
    }

    if (init.credentials)
        options.credentials = init.credentials.value();

    if (init.cache)
        options.cache = init.cache.value();
    if (options.cache == FetchOptions::Cache::OnlyIfCached && options.mode != FetchOptions::Mode::SameOrigin)
        return Exception { TypeError, ASCIILiteral("only-if-cached cache option requires fetch mode to be same-origin.")  };

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

    return std::nullopt;
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
            return Exception { TypeError, ASCIILiteral("Method must be GET, POST or HEAD in no-cors mode.") };
        if (!m_options.integrity.isEmpty())
            return Exception { TypeError, ASCIILiteral("There cannot be an integrity in no-cors mode.") };
        m_headers->setGuard(FetchHeaders::Guard::RequestNoCors);
    }
    
    return { };
}

ExceptionOr<void> FetchRequest::initializeWith(const String& url, Init&& init)
{
    ASSERT(scriptExecutionContext());
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL requestURL = scriptExecutionContext()->completeURL(url);
    if (!requestURL.isValid() || !requestURL.user().isEmpty() || !requestURL.pass().isEmpty())
        return Exception { TypeError, ASCIILiteral("URL is not valid or contains user credentials.") };

    m_options.mode = Mode::Cors;
    m_options.credentials = Credentials::Omit;
    m_referrer = ASCIILiteral("client");
    m_request.setURL(requestURL);
    m_request.setRequester(ResourceRequest::Requester::Fetch);
    m_request.setInitiatorIdentifier(scriptExecutionContext()->resourceRequestIdentifier());

    auto optionsResult = initializeOptions(init);
    if (optionsResult.hasException())
        return optionsResult.releaseException();

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
        return Exception {TypeError, ASCIILiteral("Request input is disturbed or locked.") };

    m_request = input.m_request;
    m_options = input.m_options;
    m_referrer = input.m_referrer;

    auto optionsResult = initializeOptions(init);
    if (optionsResult.hasException())
        return optionsResult.releaseException();

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
        return Exception { TypeError };

    ASSERT(scriptExecutionContext());
    extractBody(*scriptExecutionContext(), WTFMove(body));

    if (m_options.keepAlive && hasReadableStreamBody())
        return Exception { TypeError, ASCIILiteral("Request cannot have a ReadableStream body and keepalive set to true") };
    return { };
}

ExceptionOr<void> FetchRequest::setBody(FetchRequest& request)
{
    if (!request.isBodyNull()) {
        if (!methodCanHaveBody(m_request))
            return Exception { TypeError };
        // FIXME: If body has a readable stream, we should pipe it to this new body stream.
        m_body = WTFMove(request.m_body);
        request.setDisturbed();
    }

    if (m_options.keepAlive && hasReadableStreamBody())
        return Exception { TypeError, ASCIILiteral("Request cannot have a ReadableStream body and keepalive set to true") };
    return { };
}

ExceptionOr<Ref<FetchRequest>> FetchRequest::create(ScriptExecutionContext& context, Info&& input, Init&& init)
{
    auto request = adoptRef(*new FetchRequest(context, std::nullopt, FetchHeaders::create(FetchHeaders::Guard::Request), { }, { }, { }));

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
        return ASCIILiteral("about:client");
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
        return Exception { TypeError };

    auto clone = adoptRef(*new FetchRequest(context, std::nullopt, FetchHeaders::create(m_headers.get()), ResourceRequest { m_request }, FetchOptions { m_options}, String { m_referrer }));
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

