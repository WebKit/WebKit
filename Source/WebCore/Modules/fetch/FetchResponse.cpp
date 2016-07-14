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
#include "FetchRequest.h"
#include "HTTPParsers.h"
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
    auto response = adoptRef(*new FetchResponse(context, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));
    response->m_response.setType(Type::Error);
    return response;
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
        ec = RangeError;
        return nullptr;
    }
    auto redirectResponse = adoptRef(*new FetchResponse(context, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));
    redirectResponse->m_response.setHTTPStatusCode(status);
    redirectResponse->m_headers->fastSet(HTTPHeaderName::Location, requestURL.string());
    return WTFMove(redirectResponse);
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

    String statusText;
    if (!init.get("statusText", statusText) || !isValidReasonPhrase(statusText)) {
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

FetchResponse::FetchResponse(ScriptExecutionContext& context, FetchBody&& body, Ref<FetchHeaders>&& headers, ResourceResponse&& response)
    : FetchBodyOwner(context, WTFMove(body))
    , m_response(WTFMove(response))
    , m_headers(WTFMove(headers))
{
}

RefPtr<FetchResponse> FetchResponse::clone(ScriptExecutionContext& context, ExceptionCode& ec)
{
    if (isDisturbed()) {
        ec = TypeError;
        return nullptr;
    }
    return adoptRef(*new FetchResponse(context, FetchBody(m_body), FetchHeaders::create(headers()), ResourceResponse(m_response)));
}

void FetchResponse::startFetching(ScriptExecutionContext& context, const FetchRequest& request, FetchPromise&& promise)
{
    auto response = adoptRef(*new FetchResponse(context, FetchBody::loadingBody(), FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));

    // Setting pending activity until BodyLoader didFail or didSucceed callback is called.
    response->setPendingActivity(response.ptr());

    response->m_bodyLoader = BodyLoader(response.get(), WTFMove(promise));
    if (!response->m_bodyLoader->start(context, request))
        response->m_bodyLoader = Nullopt;
}

void FetchResponse::fetch(ScriptExecutionContext& context, FetchRequest& input, const Dictionary& dictionary, FetchPromise&& promise)
{
    ExceptionCode ec = 0;
    RefPtr<FetchRequest> fetchRequest = FetchRequest::create(context, input, dictionary, ec);
    if (ec) {
        promise.reject(ec);
        return;
    }
    ASSERT(fetchRequest);
    startFetching(context, *fetchRequest, WTFMove(promise));
}

const String& FetchResponse::url() const
{
    if (m_responseURL.isNull())
        m_responseURL = m_response.url().serialize(true);
    return m_responseURL;
}

void FetchResponse::fetch(ScriptExecutionContext& context, const String& url, const Dictionary& dictionary, FetchPromise&& promise)
{
    ExceptionCode ec = 0;
    RefPtr<FetchRequest> fetchRequest = FetchRequest::create(context, url, dictionary, ec);
    if (ec) {
        promise.reject(ec);
        return;
    }
    ASSERT(fetchRequest);
    startFetching(context, *fetchRequest, WTFMove(promise));
}

void FetchResponse::BodyLoader::didSucceed()
{
    ASSERT(m_response.hasPendingActivity());
#if ENABLE(STREAMS_API)
    if (m_response.m_readableStreamSource) {
        m_response.m_readableStreamSource->close();
        m_response.m_readableStreamSource = nullptr;
    }
#endif
    if (m_loader->isStarted())
        m_response.m_bodyLoader = Nullopt;
    m_response.unsetPendingActivity(&m_response);
}

void FetchResponse::BodyLoader::didFail()
{
    ASSERT(m_response.hasPendingActivity());
    if (m_promise)
        std::exchange(m_promise, Nullopt)->reject(TypeError);

#if ENABLE(STREAMS_API)
    if (m_response.m_readableStreamSource) {
        if (!m_response.m_readableStreamSource->isCancelling())
            m_response.m_readableStreamSource->error(ASCIILiteral("Loading failed"));
        m_response.m_readableStreamSource = nullptr;
    }
#endif

    // Check whether didFail is called as part of FetchLoader::start.
    if (m_loader->isStarted())
        m_response.m_bodyLoader = Nullopt;

    // FIXME: Handle the case of failing after didReceiveResponse is called.

    m_response.unsetPendingActivity(&m_response);
}

FetchResponse::BodyLoader::BodyLoader(FetchResponse& response, FetchPromise&& promise)
    : m_response(response)
    , m_promise(WTFMove(promise))
{
}

void FetchResponse::BodyLoader::didReceiveResponse(const ResourceResponse& resourceResponse)
{
    ASSERT(m_promise);

    m_response.m_response = resourceResponse;
    m_response.m_headers->filterAndFill(resourceResponse.httpHeaderFields(), FetchHeaders::Guard::Response);

    std::exchange(m_promise, Nullopt)->resolve(m_response);
}

void FetchResponse::BodyLoader::didReceiveData(const char* data, size_t size)
{
#if ENABLE(STREAMS_API)
    ASSERT(m_response.m_readableStreamSource);

    // FIXME: If ArrayBuffer::tryCreate returns null, we should probably cancel the load.
    m_response.m_readableStreamSource->enqueue(ArrayBuffer::tryCreate(data, size));
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(size);
#endif
}

void FetchResponse::BodyLoader::didFinishLoadingAsArrayBuffer(RefPtr<ArrayBuffer>&& buffer)
{
    m_response.body().loadedAsArrayBuffer(WTFMove(buffer));
}

bool FetchResponse::BodyLoader::start(ScriptExecutionContext& context, const FetchRequest& request)
{
    m_loader = std::make_unique<FetchLoader>(FetchLoader::Type::ArrayBuffer, *this);
    m_loader->start(context, request);
    return m_loader->isStarted();
}

void FetchResponse::BodyLoader::stop()
{
    if (m_loader)
        m_loader->stop();
}

#if ENABLE(STREAMS_API)
void FetchResponse::consumeBodyAsStream()
{
    ASSERT(m_readableStreamSource);
    m_isDisturbed = true;
    if (body().type() != FetchBody::Type::Loading) {
        body().consumeAsStream(*this, *m_readableStreamSource);
        if (!m_readableStreamSource->isStarting())
            m_readableStreamSource = nullptr;        
        return;
    }

    ASSERT(m_bodyLoader);

    RefPtr<SharedBuffer> data = m_bodyLoader->startStreaming();
    if (data) {
        // FIXME: We might want to enqueue each internal SharedBuffer chunk as an individual ArrayBuffer.
        // Also, createArrayBuffer might return nullptr which will lead to erroring the stream.
        // We might want to cancel the load and rename createArrayBuffer to tryCreateArrayBuffer.
        m_readableStreamSource->enqueue(data->createArrayBuffer());
    }
}

ReadableStreamSource* FetchResponse::createReadableStreamSource()
{
    ASSERT(!m_readableStreamSource);
    ASSERT(!isDisturbed());

    if (body().isEmpty())
        return nullptr;

    m_readableStreamSource = adoptRef(*new FetchResponseSource(*this));
    return m_readableStreamSource.get();
}

RefPtr<SharedBuffer> FetchResponse::BodyLoader::startStreaming()
{
    ASSERT(m_loader);
    return m_loader->startStreaming();
}

void FetchResponse::cancel()
{
    m_isDisturbed = true;
    stop();
}

#endif

void FetchResponse::stop()
{
    RefPtr<FetchResponse> protectedThis(this);
    FetchBodyOwner::stop();
    if (m_bodyLoader) {
        m_bodyLoader->stop();
        ASSERT(!m_bodyLoader);
    }
}

const char* FetchResponse::activeDOMObjectName() const
{
    return "Response";
}

bool FetchResponse::canSuspendForDocumentSuspension() const
{
    // FIXME: We can probably do the same strategy as XHR.
    return !isActive();
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)
