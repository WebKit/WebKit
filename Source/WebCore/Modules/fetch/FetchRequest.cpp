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

static bool setReferrerPolicy(FetchOptions& options, const String& referrerPolicy)
{
    if (referrerPolicy.isEmpty())
        options.setReferrerPolicy(FetchOptions::ReferrerPolicy::Empty);
    else if (referrerPolicy == "no-referrer")
        options.setReferrerPolicy(FetchOptions::ReferrerPolicy::NoReferrer);
    else if (referrerPolicy == "no-referrer-when-downgrade")
        options.setReferrerPolicy(FetchOptions::ReferrerPolicy::NoReferrerWhenDowngrade);
    else if (referrerPolicy == "origin-only")
        options.setReferrerPolicy(FetchOptions::ReferrerPolicy::OriginOnly);
    else if (referrerPolicy == "origin-when-cross-origin")
        options.setReferrerPolicy(FetchOptions::ReferrerPolicy::OriginWhenCrossOrigin);
    else if (referrerPolicy == "unsafe-url")
        options.setReferrerPolicy(FetchOptions::ReferrerPolicy::UnsafeURL);
    else
        return false;
    return true;
}

static bool setMode(FetchOptions& options, const String& mode)
{
    if (mode == "navigate")
        options.setMode(FetchOptions::Mode::Navigate);
    else if (mode == "same-origin")
        options.setMode(FetchOptions::Mode::SameOrigin);
    else if (mode == "no-cors")
        options.setMode(FetchOptions::Mode::NoCors);
    else if (mode == "cors")
        options.setMode(FetchOptions::Mode::Cors);
    else
        return false;
    return true;
}

static bool setCredentials(FetchOptions& options, const String& credentials)
{
    if (credentials == "omit")
        options.setCredentials(FetchOptions::Credentials::Omit);
    else if (credentials == "same-origin")
        options.setCredentials(FetchOptions::Credentials::SameOrigin);
    else if (credentials == "include")
        options.setCredentials(FetchOptions::Credentials::Include);
    else
        return false;
    return true;
}

static bool setCache(FetchOptions& options, const String& cache)
{
    if (cache == "default")
        options.setCache(FetchOptions::Cache::Default);
    else if (cache == "no-store")
        options.setCache(FetchOptions::Cache::NoStore);
    else if (cache == "reload")
        options.setCache(FetchOptions::Cache::Reload);
    else if (cache == "no-cache")
        options.setCache(FetchOptions::Cache::NoCache);
    else if (cache == "force-cache")
        options.setCache(FetchOptions::Cache::ForceCache);
    else
        return false;
    return true;
}

static bool setRedirect(FetchOptions& options, const String& redirect)
{
    if (redirect == "follow")
        options.setRedirect(FetchOptions::Redirect::Follow);
    else if (redirect == "error")
        options.setRedirect(FetchOptions::Redirect::Error);
    else if (redirect == "manual")
        options.setRedirect(FetchOptions::Redirect::Manual);
    else
        return false;
    return true;
}

static bool setMethod(ResourceRequest& request, const String& initMethod)
{
    if (!isValidHTTPToken(initMethod))
        return false;

    String method = initMethod.convertToASCIIUppercase();
    if (method == "CONNECT" || method == "TRACE" || method == "TRACK")
        return false;

    request.setHTTPMethod((method == "DELETE" || method == "GET" || method == "HEAD" || method == "OPTIONS" || method == "POST" || method == "PUT") ? method : initMethod);

    return true;
}

static bool setReferrer(FetchRequest::InternalRequest& request, ScriptExecutionContext& context, const Dictionary& init)
{
    String referrer;
    if (!init.get("referrer", referrer))
        return true;
    if (referrer.isEmpty()) {
        request.referrer = ASCIILiteral("no-referrer");
        return true;
    }
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL referrerURL = context.completeURL(referrer);
    if (!referrerURL.isValid())
        return false;

    if (referrerURL.protocolIs("about") && referrerURL.path() == "client") {
        request.referrer = ASCIILiteral("client");
        return true;
    }

    if (!(context.securityOrigin() && context.securityOrigin()->canRequest(referrerURL)))
        return false;

    request.referrer = referrerURL.string();
    return true;
}

static bool buildOptions(FetchRequest::InternalRequest& request, ScriptExecutionContext& context, const Dictionary& init)
{
    JSC::JSValue window;
    if (init.get("window", window)) {
        if (!window.isNull())
            return false;
    }

    if (!setReferrer(request, context, init))
        return false;

    String value;
    if (init.get("referrerPolicy", value) && !setReferrerPolicy(request.options, value))
        return false;

    if (init.get("mode", value) && !setMode(request.options, value))
        return false;
    if (request.options.mode() == FetchOptions::Mode::Navigate)
        return false;

    if (init.get("credentials", value) && !setCredentials(request.options, value))
        return false;

    if (init.get("cache", value) && !setCache(request.options, value))
        return false;

    if (init.get("redirect", value) && !setRedirect(request.options, value))
        return false;

    init.get("integrity", request.integrity);

    if (init.get("method", value) && !setMethod(request.request, value))
        return false;

    return true;
}

static RefPtr<FetchHeaders> buildHeaders(const Dictionary& init, const FetchRequest::InternalRequest& request, const FetchHeaders* inputHeaders = nullptr)
{
    FetchHeaders::Guard guard = FetchHeaders::Guard::Request;
    if (request.options.mode() == FetchOptions::Mode::NoCors) {
        const String& method = request.request.httpMethod();
        if (method != "GET" && method != "POST" && method != "HEAD")
            return nullptr;
        if (!request.integrity.isEmpty())
            return nullptr;
        guard = FetchHeaders::Guard::RequestNoCors;
    }

    RefPtr<FetchHeaders> initialHeaders;
    RefPtr<FetchHeaders> headers = FetchHeaders::create(guard);
    headers->fill(init.get("headers", initialHeaders) ? initialHeaders.get() : inputHeaders);

    return headers;
}

static FetchBody buildBody(const Dictionary& init, FetchHeaders& headers, FetchBody* inputBody = nullptr)
{
    JSC::JSValue value;
    bool hasInitBody = init.get("body", value);
    FetchBody body = hasInitBody ? FetchBody::extract(*init.execState(), value) : FetchBody::extractFromBody(inputBody);

    String type = headers.fastGet(HTTPHeaderName::ContentType);
    if (hasInitBody && type.isEmpty() && !body.mimeType().isEmpty()) {
        type = body.mimeType();
        headers.fastSet(HTTPHeaderName::ContentType, type);
    }
    body.setMimeType(type);
    return body;
}

static bool validateBodyAndMethod(const FetchBody& body, const FetchRequest::InternalRequest& internalRequest)
{
    if (body.isEmpty())
        return true;
    return internalRequest.request.httpMethod() != "GET" && internalRequest.request.httpMethod() != "HEAD";
}

RefPtr<FetchRequest> FetchRequest::create(ScriptExecutionContext& context, const String& url, const Dictionary& init, ExceptionCode& ec)
{
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL requestURL = context.completeURL(url);
    if (!requestURL.isValid() || !requestURL.user().isEmpty() || !requestURL.pass().isEmpty()) {
        ec = TypeError;
        return nullptr;
    }

    FetchRequest::InternalRequest internalRequest;
    internalRequest.options.setMode(FetchOptions::Mode::Cors);
    internalRequest.options.setCredentials(FetchOptions::Credentials::Omit);
    internalRequest.referrer = ASCIILiteral("client");
    internalRequest.request.setURL(requestURL);

    if (!buildOptions(internalRequest, context, init)) {
        ec = TypeError;
        return nullptr;
    }

    RefPtr<FetchHeaders> headers = buildHeaders(init, internalRequest);
    if (!headers) {
        ec = TypeError;
        return nullptr;
    }

    FetchBody body = buildBody(init, *headers);
    if (!validateBodyAndMethod(body, internalRequest)) {
        ec = TypeError;
        return nullptr;
    }

    return adoptRef(*new FetchRequest(context, WTFMove(body), headers.releaseNonNull(), WTFMove(internalRequest)));
}

RefPtr<FetchRequest> FetchRequest::create(ScriptExecutionContext& context, FetchRequest& input, const Dictionary& init, ExceptionCode& ec)
{
    if (input.isDisturbed()) {
        ec = TypeError;
        return nullptr;
    }

    FetchRequest::InternalRequest internalRequest(input.m_internalRequest);

    if (!buildOptions(internalRequest, context, init)) {
        ec = TypeError;
        return nullptr;
    }

    RefPtr<FetchHeaders> headers = buildHeaders(init, internalRequest, input.m_headers.ptr());
    if (!headers) {
        ec = TypeError;
        return nullptr;
    }

    FetchBody body = buildBody(init, *headers, &input.m_body);
    if (!validateBodyAndMethod(body, internalRequest)) {
        ec = TypeError;
        return nullptr;
    }

    return adoptRef(*new FetchRequest(context, WTFMove(body), headers.releaseNonNull(), WTFMove(internalRequest)));
}

String FetchRequest::type() const
{
    switch (m_internalRequest.options.type()) {
    case FetchOptions::Type::Default:
        return String();
    case FetchOptions::Type::Audio :
        return ASCIILiteral("audio");
    case FetchOptions::Type::Font :
        return ASCIILiteral("font");
    case FetchOptions::Type::Image :
        return ASCIILiteral("image");
    case FetchOptions::Type::Script :
        return ASCIILiteral("script");
    case FetchOptions::Type::Style :
        return ASCIILiteral("style");
    case FetchOptions::Type::Track :
        return ASCIILiteral("track");
    case FetchOptions::Type::Video :
        return ASCIILiteral("video");
    };
    ASSERT_NOT_REACHED();
    return String();
}

String FetchRequest::destination() const
{
    switch (m_internalRequest.options.destination()) {
    case FetchOptions::Destination::Default:
        return String();
    case FetchOptions::Destination::Document:
        return ASCIILiteral("document");
    case FetchOptions::Destination::SharedWorker:
        return ASCIILiteral("sharedworker");
    case FetchOptions::Destination::Subresource:
        return ASCIILiteral("subresource");
    case FetchOptions::Destination::Unknown:
        return ASCIILiteral("unknown");
    case FetchOptions::Destination::Worker:
        return ASCIILiteral("worker");
    }
    ASSERT_NOT_REACHED();
    return String();
}

String FetchRequest::referrerPolicy() const
{
    switch (m_internalRequest.options.referrerPolicy()) {
    case FetchOptions::ReferrerPolicy::Empty:
        return String();
    case FetchOptions::ReferrerPolicy::NoReferrer:
        return ASCIILiteral("no-referrer");
    case FetchOptions::ReferrerPolicy::NoReferrerWhenDowngrade:
        return ASCIILiteral("no-referrer-when-downgrade");
    case FetchOptions::ReferrerPolicy::OriginOnly:
        return ASCIILiteral("origin-only");
    case FetchOptions::ReferrerPolicy::OriginWhenCrossOrigin:
        return ASCIILiteral("origin-when-cross-origin");
    case FetchOptions::ReferrerPolicy::UnsafeURL:
        return ASCIILiteral("unsafe-url");
    }
    ASSERT_NOT_REACHED();
    return String();
}

String FetchRequest::referrer() const
{
    if (m_internalRequest.referrer == "no-referrer")
        return String();
    if (m_internalRequest.referrer == "client")
        return ASCIILiteral("about:client");
    return m_internalRequest.referrer;
}

String FetchRequest::mode() const
{
    switch (m_internalRequest.options.mode()) {
    case FetchOptions::Mode::Navigate:
        return ASCIILiteral("navigate");
    case FetchOptions::Mode::SameOrigin:
        return ASCIILiteral("same-origin");
    case FetchOptions::Mode::NoCors:
        return ASCIILiteral("no-cors");
    case FetchOptions::Mode::Cors:
        return ASCIILiteral("cors");
    };
    ASSERT_NOT_REACHED();
    return String();
}

String FetchRequest::credentials() const
{
    switch (m_internalRequest.options.credentials()) {
    case FetchOptions::Credentials::Omit:
        return ASCIILiteral("omit");
    case FetchOptions::Credentials::SameOrigin:
        return ASCIILiteral("same-origin");
    case FetchOptions::Credentials::Include:
        return ASCIILiteral("include");
    };
    ASSERT_NOT_REACHED();
    return String();
}

String FetchRequest::cache() const
{
    switch (m_internalRequest.options.cache()) {
    case FetchOptions::Cache::Default:
        return ASCIILiteral("default");
    case FetchOptions::Cache::NoStore:
        return ASCIILiteral("no-store");
    case FetchOptions::Cache::Reload:
        return ASCIILiteral("reload");
    case FetchOptions::Cache::NoCache:
        return ASCIILiteral("no-cache");
    case FetchOptions::Cache::ForceCache:
        return ASCIILiteral("force-cache");
    }
    ASSERT_NOT_REACHED();
    return String();
}

String FetchRequest::redirect() const
{
    switch (m_internalRequest.options.redirect()) {
    case FetchOptions::Redirect::Follow:
        return ASCIILiteral("follow");
    case FetchOptions::Redirect::Error:
        return ASCIILiteral("error");
    case FetchOptions::Redirect::Manual:
        return ASCIILiteral("manual");
    }
    ASSERT_NOT_REACHED();
    return String();
}

ResourceRequest FetchRequest::internalRequest() const
{
    ResourceRequest request = m_internalRequest.request;
    request.setHTTPHeaderFields(m_headers->internalHeaders());
    request.setHTTPBody(body().bodyForInternalRequest());
    return request;
}

RefPtr<FetchRequest> FetchRequest::clone(ScriptExecutionContext& context, ExceptionCode& ec)
{
    if (isDisturbed()) {
        ec = TypeError;
        return nullptr;
    }

    // FIXME: Validate body teeing.
    return adoptRef(*new FetchRequest(context, FetchBody(m_body), FetchHeaders::create(m_headers.get()), FetchRequest::InternalRequest(m_internalRequest)));
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
