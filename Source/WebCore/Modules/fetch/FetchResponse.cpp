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

#include "ExceptionCode.h"
#include "FetchRequest.h"
#include "HTTPParsers.h"
#include "JSBlob.h"
#include "JSFetchResponse.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

static inline bool isRedirectStatus(int status)
{
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
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

void FetchResponse::setStatus(int status, const String& statusText, ExceptionCode& ec)
{
    if (!isValidReasonPhrase(statusText)) {
        ec = TypeError;
        return;
    }
    m_response.setHTTPStatusCode(status);
    m_response.setHTTPStatusText(statusText);
}

void FetchResponse::initializeWith(JSC::ExecState& execState, JSC::JSValue body)
{
    ASSERT(scriptExecutionContext());
    m_body = FetchBody::extract(*scriptExecutionContext(), execState, body);
    m_body.updateContentType(m_headers);
}

FetchResponse::FetchResponse(ScriptExecutionContext& context, FetchBody&& body, Ref<FetchHeaders>&& headers, ResourceResponse&& response)
    : FetchBodyOwner(context, WTFMove(body))
    , m_response(WTFMove(response))
    , m_headers(WTFMove(headers))
{
}

Ref<FetchResponse> FetchResponse::cloneForJS()
{
    ASSERT(scriptExecutionContext());
    ASSERT(!isDisturbed());
    return adoptRef(*new FetchResponse(*scriptExecutionContext(), FetchBody(m_body), FetchHeaders::create(headers()), ResourceResponse(m_response)));
}

void FetchResponse::fetch(ScriptExecutionContext& context, FetchRequest& request, FetchPromise&& promise)
{
    auto response = adoptRef(*new FetchResponse(context, FetchBody::loadingBody(), FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));

    // Setting pending activity until BodyLoader didFail or didSucceed callback is called.
    response->setPendingActivity(response.ptr());

    response->m_bodyLoader = BodyLoader(response.get(), WTFMove(promise));
    if (!response->m_bodyLoader->start(context, request))
        response->m_bodyLoader = Nullopt;
}

const String& FetchResponse::url() const
{
    if (m_responseURL.isNull())
        m_responseURL = m_response.url().serialize(true);
    return m_responseURL;
}

void FetchResponse::BodyLoader::didSucceed()
{
    ASSERT(m_response.hasPendingActivity());
    m_response.m_body.loadingSucceeded();

#if ENABLE(STREAMS_API)
    if (m_response.m_readableStreamSource && m_response.m_body.type() != FetchBody::Type::Loaded) {
        // We only close the stream if FetchBody already enqueued data.
        // Otherwise, FetchBody will close the stream when enqueuing data.
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
    m_response.m_body.setContentType(m_response.m_headers->fastGet(HTTPHeaderName::ContentType));

    std::exchange(m_promise, Nullopt)->resolve(m_response);
}

void FetchResponse::BodyLoader::didReceiveData(const char* data, size_t size)
{
#if ENABLE(STREAMS_API)
    ASSERT(m_response.m_readableStreamSource);

    if (!m_response.m_readableStreamSource->enqueue(ArrayBuffer::tryCreate(data, size)))
        stop();
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(size);
#endif
}

bool FetchResponse::BodyLoader::start(ScriptExecutionContext& context, const FetchRequest& request)
{
    m_loader = std::make_unique<FetchLoader>(*this, &m_response.m_body.consumer());
    m_loader->start(context, request);
    return m_loader->isStarted();
}

void FetchResponse::BodyLoader::stop()
{
    m_promise = Nullopt;
    if (m_loader)
        m_loader->stop();
}

void FetchResponse::consume(unsigned type, DeferredWrapper&& wrapper)
{
    ASSERT(type <= static_cast<unsigned>(FetchBodyConsumer::Type::Text));

    switch (static_cast<FetchBodyConsumer::Type>(type)) {
    case FetchBodyConsumer::Type::ArrayBuffer:
        arrayBuffer(WTFMove(wrapper));
        return;
    case FetchBodyConsumer::Type::Blob:
        blob(WTFMove(wrapper));
        return;
    case FetchBodyConsumer::Type::JSON:
        json(WTFMove(wrapper));
        return;
    case FetchBodyConsumer::Type::Text:
        text(WTFMove(wrapper));
        return;
    case FetchBodyConsumer::Type::None:
        ASSERT_NOT_REACHED();
        return;
    }
}

#if ENABLE(STREAMS_API)
void FetchResponse::startConsumingStream(unsigned type)
{
    m_isDisturbed = true;
    m_consumer.setType(static_cast<FetchBodyConsumer::Type>(type));
}

void FetchResponse::consumeChunk(Ref<JSC::Uint8Array>&& chunk)
{
    m_consumer.append(chunk->data(), chunk->byteLength());
}

void FetchResponse::finishConsumingStream(DeferredWrapper&& promise)
{
    m_consumer.resolve(promise);
}

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
        if (!m_readableStreamSource->enqueue(data->createArrayBuffer()))
            stop();
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
