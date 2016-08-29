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
        return false;
    return true;
}

static bool setMode(FetchOptions& options, const String& mode)
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
        return false;
    return true;
}

static bool setCredentials(FetchOptions& options, const String& credentials)
{
    if (credentials == "omit")
        options.credentials = FetchOptions::Credentials::Omit;
    else if (credentials == "same-origin")
        options.credentials = FetchOptions::Credentials::SameOrigin;
    else if (credentials == "include")
        options.credentials = FetchOptions::Credentials::Include;
    else
        return false;
    return true;
}

static bool setCache(FetchOptions& options, const String& cache)
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
    else
        return false;
    return true;
}

static bool setRedirect(FetchOptions& options, const String& redirect)
{
    if (redirect == "follow")
        options.redirect = FetchOptions::Redirect::Follow;
    else if (redirect == "error")
        options.redirect = FetchOptions::Redirect::Error;
    else if (redirect == "manual")
        options.redirect = FetchOptions::Redirect::Manual;
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
    if (request.options.mode == FetchOptions::Mode::Navigate)
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

static bool validateBodyAndMethod(const FetchBody& body, const FetchRequest::InternalRequest& internalRequest)
{
    if (body.isEmpty())
        return true;
    return internalRequest.request.httpMethod() != "GET" && internalRequest.request.httpMethod() != "HEAD";
}

void FetchRequest::initializeOptions(const Dictionary& init, ExceptionCode& ec)
{
    ASSERT(scriptExecutionContext());
    if (!buildOptions(m_internalRequest, *scriptExecutionContext(), init)) {
        ec = TypeError;
        return;
    }

    if (m_internalRequest.options.mode == FetchOptions::Mode::NoCors) {
        const String& method = m_internalRequest.request.httpMethod();
        if (method != "GET" && method != "POST" && method != "HEAD") {
            ec = TypeError;
            return;
        }
        if (!m_internalRequest.integrity.isEmpty()) {
            ec = TypeError;
            return;
        }
        m_headers->setGuard(FetchHeaders::Guard::RequestNoCors);
    }
}

FetchHeaders* FetchRequest::initializeWith(const String& url, const Dictionary& init, ExceptionCode& ec)
{
    ASSERT(scriptExecutionContext());
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL requestURL = scriptExecutionContext()->completeURL(url);
    if (!requestURL.isValid() || !requestURL.user().isEmpty() || !requestURL.pass().isEmpty()) {
        ec = TypeError;
        return nullptr;
    }

    m_internalRequest.options.mode = Mode::Cors;
    m_internalRequest.options.credentials = Credentials::Omit;
    m_internalRequest.referrer = ASCIILiteral("client");
    m_internalRequest.request.setURL(requestURL);

    initializeOptions(init, ec);
    return m_headers.ptr();
}

FetchHeaders* FetchRequest::initializeWith(FetchRequest& input, const Dictionary& init, ExceptionCode& ec)
{
    if (input.isDisturbed()) {
        ec = TypeError;
        return nullptr;
    }

    m_internalRequest = input.m_internalRequest;

    initializeOptions(init, ec);
    return m_headers.ptr();
}

void FetchRequest::setBody(JSC::ExecState& execState, JSC::JSValue body, FetchRequest* request, ExceptionCode& ec)
{
    if (!body.isNull()) {
        ASSERT(scriptExecutionContext());
        m_body = FetchBody::extract(*scriptExecutionContext(), execState, body);
        if (m_body.type() == FetchBody::Type::None) {
            ec = TypeError;
            return;
        }
    }
    else if (request && !request->m_body.isEmpty()) {
        m_body = FetchBody::extractFromBody(&request->m_body);
        request->setDisturbed();
    }

    m_body.updateContentType(m_headers);

    if (!validateBodyAndMethod(m_body, m_internalRequest))
        ec = TypeError;
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
    request.setHTTPBody(body().bodyForInternalRequest(*scriptExecutionContext()));

    // FIXME: Support no-referrer and client. Ensure this case-sensitive comparison is ok.
    if (m_internalRequest.referrer != "no-referrer" && m_internalRequest.referrer != "client")
        request.setHTTPReferrer(URL(URL(), m_internalRequest.referrer).strippedForUseAsReferrer());

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
