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

#pragma once

#if ENABLE(FETCH_API)

#include "ExceptionOr.h"
#include "FetchBodyOwner.h"
#include "FetchOptions.h"
#include "ResourceRequest.h"
#include <wtf/Optional.h>

namespace WebCore {

class Blob;
class ScriptExecutionContext;
class URLSearchParams;

class FetchRequest final : public FetchBodyOwner {
public:
    static Ref<FetchRequest> create(ScriptExecutionContext& context) { return adoptRef(*new FetchRequest(context, std::nullopt, FetchHeaders::create(FetchHeaders::Guard::Request), { })); }

    using Cache = FetchOptions::Cache;
    using Credentials = FetchOptions::Credentials;
    using Destination = FetchOptions::Destination;
    using Mode = FetchOptions::Mode;
    using Redirect = FetchOptions::Redirect;
    using ReferrerPolicy = FetchOptions::ReferrerPolicy;
    using Type = FetchOptions::Type;

    struct Init {
        String method;
        String referrer;
        std::optional<ReferrerPolicy> referrerPolicy;
        std::optional<Mode> mode;
        std::optional<Credentials> credentials;
        std::optional<Cache> cache;
        std::optional<Redirect> redirect;
        String integrity;
        JSC::JSValue window;
    };

    ExceptionOr<FetchHeaders&> initializeWith(FetchRequest&, const Init&);
    ExceptionOr<FetchHeaders&> initializeWith(const String&, const Init&);
    ExceptionOr<void> setBody(JSC::ExecState&, JSC::JSValue, FetchRequest*);

    const String& method() const { return m_internalRequest.request.httpMethod(); }
    const String& url() const;
    FetchHeaders& headers() { return m_headers.get(); }

    Type type() const;
    Destination destination() const;
    String referrer() const;
    ReferrerPolicy referrerPolicy() const;
    Mode mode() const;
    Credentials credentials() const;
    Cache cache() const;
    Redirect redirect() const;

    const String& integrity() const { return m_internalRequest.integrity; }

    ExceptionOr<Ref<FetchRequest>> clone(ScriptExecutionContext&);

    struct InternalRequest {
        ResourceRequest request;
        FetchOptions options;
        String referrer;
        String integrity;
    };

    const FetchOptions& fetchOptions() const { return m_internalRequest.options; }
    ResourceRequest internalRequest() const;
    bool isBodyReadableStream() const { return !isBodyNull() && body().isReadableStream(); }

    const String& internalRequestReferrer() const { return m_internalRequest.referrer; }

private:
    FetchRequest(ScriptExecutionContext&, std::optional<FetchBody>&&, Ref<FetchHeaders>&&, InternalRequest&&);

    ExceptionOr<FetchHeaders&> initializeOptions(const Init&);

    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    InternalRequest m_internalRequest;
    mutable String m_requestURL;
};

inline FetchRequest::FetchRequest(ScriptExecutionContext& context, std::optional<FetchBody>&& body, Ref<FetchHeaders>&& headers, InternalRequest&& internalRequest)
    : FetchBodyOwner(context, WTFMove(body), WTFMove(headers))
    , m_internalRequest(WTFMove(internalRequest))
{
}

inline auto FetchRequest::cache() const -> Cache
{
    return m_internalRequest.options.cache;
}

inline auto FetchRequest::credentials() const -> Credentials
{
    return m_internalRequest.options.credentials;
}

inline auto FetchRequest::destination() const -> Destination
{
    return m_internalRequest.options.destination;
}

inline auto FetchRequest::mode() const -> Mode
{
    return m_internalRequest.options.mode;
}

inline auto FetchRequest::redirect() const -> Redirect
{
    return m_internalRequest.options.redirect;
}

inline auto FetchRequest::referrerPolicy() const -> ReferrerPolicy
{
    return m_internalRequest.options.referrerPolicy;
}

inline auto FetchRequest::type() const -> Type
{
    return m_internalRequest.options.type;
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)
