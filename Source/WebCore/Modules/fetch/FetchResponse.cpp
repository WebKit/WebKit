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
#include "FetchResponse.h"

#if ENABLE(FETCH_API)

#include "Dictionary.h"
#include "ExceptionCode.h"
#include "JSFetchResponse.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

static inline bool isRedirectStatus(int status)
{
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

static inline bool isNullBodyStatus(int status)
{
    return status == 101 || status == 204 || status == 205 || status == 304;
}

Ref<FetchResponse> FetchResponse::error(ScriptExecutionContext& context)
{
    return adoptRef(*new FetchResponse(context, Type::Error, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable), ResourceResponse()));
}

RefPtr<FetchResponse> FetchResponse::redirect(ScriptExecutionContext& context, const String& url, int status, ExceptionCode& ec)
{
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL requestURL = context.completeURL(url);
    if (!requestURL.isValid() || !requestURL.user().isEmpty() || !requestURL.pass().isEmpty()) {
        ec = TypeError;
        return nullptr;
    }
    if (!isRedirectStatus(status)) {
        ec = TypeError;
        return nullptr;
    }
    RefPtr<FetchResponse> redirectResponse = adoptRef(*new FetchResponse(context, Type::Default, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable), ResourceResponse()));
    redirectResponse->m_response.setHTTPStatusCode(status);
    redirectResponse->m_headers->fastSet(HTTPHeaderName::Location, requestURL.string());
    return redirectResponse;
}

void FetchResponse::initializeWith(const Dictionary& init, ExceptionCode& ec)
{
    int status;
    if (!init.get("status", status)) {
        ec = TypeError;
        return;
    }
    if (status < 200 || status > 599) {
        ec = RangeError;
        return;
    }

    // FIXME: Validate reason phrase (https://tools.ietf.org/html/rfc7230#section-3.1.2).
    String statusText;
    if (!init.get("statusText", statusText)) {
        ec = TypeError;
        return;
    }
    m_response.setHTTPStatusCode(status);
    m_response.setHTTPStatusText(statusText);

    RefPtr<FetchHeaders> initialHeaders;
    if (init.get("headers", initialHeaders))
        m_headers->fill(initialHeaders.get());

    JSC::JSValue body;
    if (init.get("body", body)) {
        if (isNullBodyStatus(status)) {
            ec = TypeError;
            return;
        }
        m_body = FetchBody::extract(*init.execState(), body);
        if (m_headers->fastGet(HTTPHeaderName::ContentType).isEmpty() && !m_body.mimeType().isEmpty())
            m_headers->fastSet(HTTPHeaderName::ContentType, m_body.mimeType());
    }
}

FetchResponse::FetchResponse(ScriptExecutionContext& context, Type type, FetchBody&& body, Ref<FetchHeaders>&& headers, ResourceResponse&& response)
    : FetchBodyOwner(context, WTFMove(body))
    , m_type(type)
    , m_response(WTFMove(response))
    , m_headers(WTFMove(headers))
{
}

RefPtr<FetchResponse> FetchResponse::clone(ScriptExecutionContext& context, ExceptionCode& ec)
{
    if (m_body.isDisturbed() || m_isLocked) {
        ec = TypeError;
        return nullptr;
    }
    RefPtr<FetchResponse> cloned = adoptRef(*new FetchResponse(context, m_type, FetchBody(m_body), FetchHeaders::create(headers()), ResourceResponse(m_response)));
    cloned->m_isRedirected = m_isRedirected;
    return cloned;
}

String FetchResponse::type() const
{
    switch (m_type) {
    case Type::Basic:
        return ASCIILiteral("basic");
    case Type::Cors:
        return ASCIILiteral("cors");
    case Type::Default:
        return ASCIILiteral("default");
    case Type::Error:
        return ASCIILiteral("error");
    case Type::Opaque:
        return ASCIILiteral("opaque");
    case Type::OpaqueRedirect:
        return ASCIILiteral("opaqueredirect");
    };
    ASSERT_NOT_REACHED();
    return String();
}

// FIXME: Implement this, as a custom or through binding generator.
JSC::JSValue JSFetchResponse::body(JSC::ExecState&) const
{
    return JSC::jsNull();
}

const char* FetchResponse::activeDOMObjectName() const
{
    return "Response";
}

bool FetchResponse::canSuspendForDocumentSuspension() const
{
    return true;
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)
