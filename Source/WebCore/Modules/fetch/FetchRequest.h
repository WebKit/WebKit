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

#include "AbortSignal.h"
#include "ExceptionOr.h"
#include "FetchBodyOwner.h"
#include "FetchIdentifier.h"
#include "FetchOptions.h"
#include "FetchRequestDestination.h"
#include "FetchRequestInit.h"
#include "ResourceRequest.h"
#include "URLKeepingBlobAlive.h"

namespace WebCore {

class Blob;
class ScriptExecutionContext;
class URLSearchParams;
class WebCoreOpaqueRoot;

class FetchRequest final : public FetchBodyOwner {
public:
    using Init = FetchRequestInit;
    using Info = std::variant<RefPtr<FetchRequest>, String>;

    using Cache = FetchOptions::Cache;
    using Credentials = FetchOptions::Credentials;
    using Destination = FetchOptions::Destination;
    using Mode = FetchOptions::Mode;
    using Redirect = FetchOptions::Redirect;

    static ExceptionOr<Ref<FetchRequest>> create(ScriptExecutionContext&, Info&&, Init&&);
    static Ref<FetchRequest> create(ScriptExecutionContext&, std::optional<FetchBody>&&, Ref<FetchHeaders>&&, ResourceRequest&&, FetchOptions&&, String&& referrer);

    const String& method() const { return m_request.httpMethod(); }
    const String& urlString() const;
    FetchHeaders& headers() { return m_headers.get(); }
    const FetchHeaders& headers() const { return m_headers.get(); }

    Destination destination() const { return m_options.destination; }
    String referrer() const;
    ReferrerPolicy referrerPolicy() const { return m_options.referrerPolicy; }
    Mode mode() const { return m_options.mode; }
    Credentials credentials() const { return m_options.credentials; }
    Cache cache() const { return m_options.cache; }
    Redirect redirect() const { return m_options.redirect; }
    bool keepalive() const { return m_options.keepAlive; };
    AbortSignal& signal() { return m_signal.get(); }

    const String& integrity() const { return m_options.integrity; }

    ExceptionOr<Ref<FetchRequest>> clone();

    const FetchOptions& fetchOptions() const { return m_options; }
    const ResourceRequest& internalRequest() const { return m_request; }
    const String& internalRequestReferrer() const { return m_referrer; }
    const URL& url() const { return m_request.url(); }

    ResourceRequest resourceRequest() const;
    FetchIdentifier navigationPreloadIdentifier() const { return m_navigationPreloadIdentifier; }
    void setNavigationPreloadIdentifier(FetchIdentifier identifier) { m_navigationPreloadIdentifier = identifier; }

private:
    FetchRequest(ScriptExecutionContext*, std::optional<FetchBody>&&, Ref<FetchHeaders>&&, ResourceRequest&&, FetchOptions&&, String&& referrer);

    ExceptionOr<void> initializeOptions(const Init&);
    ExceptionOr<void> initializeWith(FetchRequest&, Init&&);
    ExceptionOr<void> initializeWith(const String&, Init&&);
    ExceptionOr<void> setBody(FetchBody::Init&&);
    ExceptionOr<void> setBody(FetchRequest&);

    void stop() final;
    const char* activeDOMObjectName() const final;

    ResourceRequest m_request;
    URLKeepingBlobAlive m_requestURL;
    FetchOptions m_options;
    String m_referrer;
    Ref<AbortSignal> m_signal;
    FetchIdentifier m_navigationPreloadIdentifier;
};

inline FetchRequest::FetchRequest(ScriptExecutionContext* context, std::optional<FetchBody>&& body, Ref<FetchHeaders>&& headers, ResourceRequest&& request, FetchOptions&& options, String&& referrer)
    : FetchBodyOwner(context, WTFMove(body), WTFMove(headers))
    , m_request(WTFMove(request))
    , m_requestURL(m_request.url())
    , m_options(WTFMove(options))
    , m_referrer(WTFMove(referrer))
    , m_signal(AbortSignal::create(context))
{
    m_request.setRequester(ResourceRequestRequester::Fetch);
    updateContentType();
}

WebCoreOpaqueRoot root(FetchRequest*);

} // namespace WebCore
