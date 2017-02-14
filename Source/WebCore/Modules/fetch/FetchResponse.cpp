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

ExceptionOr<Ref<FetchResponse>> FetchResponse::redirect(ScriptExecutionContext& context, const String& url, int status)
{
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL requestURL = context.completeURL(url);
    if (!requestURL.isValid() || !requestURL.user().isEmpty() || !requestURL.pass().isEmpty())
        return Exception { TypeError };
    if (!isRedirectStatus(status))
        return Exception { RangeError };
    auto redirectResponse = adoptRef(*new FetchResponse(context, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));
    redirectResponse->m_response.setHTTPStatusCode(status);
    redirectResponse->m_headers->fastSet(HTTPHeaderName::Location, requestURL.string());
    return WTFMove(redirectResponse);
}

ExceptionOr<void> FetchResponse::setStatus(int status, const String& statusText)
{
    if (!isValidReasonPhrase(statusText))
        return Exception { TypeError };
    m_response.setHTTPStatusCode(status);
    m_response.setHTTPStatusText(statusText);
    return { };
}

void FetchResponse::initializeWith(JSC::ExecState& execState, JSC::JSValue body)
{
    ASSERT(scriptExecutionContext());
    extractBody(*scriptExecutionContext(), execState, body);
    updateContentType();
}

FetchResponse::FetchResponse(ScriptExecutionContext& context, std::optional<FetchBody>&& body, Ref<FetchHeaders>&& headers, ResourceResponse&& response)
    : FetchBodyOwner(context, WTFMove(body), WTFMove(headers))
    , m_response(WTFMove(response))
{
}

Ref<FetchResponse> FetchResponse::cloneForJS()
{
    ASSERT(scriptExecutionContext());
    ASSERT(!isDisturbedOrLocked());

    auto clone = adoptRef(*new FetchResponse(*scriptExecutionContext(), std::nullopt, FetchHeaders::create(headers()), ResourceResponse(m_response)));
    clone->cloneBody(*this);
    return clone;
}

void FetchResponse::fetch(ScriptExecutionContext& context, FetchRequest& request, FetchPromise&& promise)
{
    if (request.isBodyReadableStream()) {
        promise.reject(TypeError, "ReadableStream uploading is not supported");
        return;
    }
    auto response = adoptRef(*new FetchResponse(context, FetchBody::loadingBody(), FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));

    response->m_bodyLoader.emplace(response.get(), WTFMove(promise));
    if (!response->m_bodyLoader->start(context, request))
        response->m_bodyLoader = std::nullopt;
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
    m_response.m_body->loadingSucceeded();

#if ENABLE(READABLE_STREAM_API)
    if (m_response.m_readableStreamSource && !m_response.body().consumer().hasData())
        m_response.closeStream();
#endif

    if (m_loader->isStarted()) {
        Ref<FetchResponse> protector(m_response);
        m_response.m_bodyLoader = std::nullopt;
    }
}

void FetchResponse::BodyLoader::didFail()
{
    ASSERT(m_response.hasPendingActivity());
    if (m_promise)
        std::exchange(m_promise, std::nullopt)->reject(TypeError);

#if ENABLE(READABLE_STREAM_API)
    if (m_response.m_readableStreamSource) {
        if (!m_response.m_readableStreamSource->isCancelling())
            m_response.m_readableStreamSource->error(ASCIILiteral("Loading failed"));
        m_response.m_readableStreamSource = nullptr;
    }
#endif

    // Check whether didFail is called as part of FetchLoader::start.
    if (m_loader->isStarted()) {
        Ref<FetchResponse> protector(m_response);
        m_response.m_bodyLoader = std::nullopt;
    }
}

FetchResponse::BodyLoader::BodyLoader(FetchResponse& response, FetchPromise&& promise)
    : m_response(response)
    , m_promise(WTFMove(promise))
{
    m_response.setPendingActivity(&m_response);
}

FetchResponse::BodyLoader::~BodyLoader()
{
    m_response.unsetPendingActivity(&m_response);
}

void FetchResponse::BodyLoader::didReceiveResponse(const ResourceResponse& resourceResponse)
{
    ASSERT(m_promise);

    m_response.m_response = resourceResponse;
    m_response.m_headers->filterAndFill(resourceResponse.httpHeaderFields(), FetchHeaders::Guard::Response);

    std::exchange(m_promise, std::nullopt)->resolve(m_response);
}

void FetchResponse::BodyLoader::didReceiveData(const char* data, size_t size)
{
#if ENABLE(READABLE_STREAM_API)
    ASSERT(m_response.m_readableStreamSource);
    auto& source = *m_response.m_readableStreamSource;

    if (!source.isPulling()) {
        m_response.body().consumer().append(data, size);
        return;
    }

    if (m_response.body().consumer().hasData() && !source.enqueue(m_response.body().consumer().takeAsArrayBuffer())) {
        stop();
        return;
    }
    if (!source.enqueue(ArrayBuffer::tryCreate(data, size))) {
        stop();
        return;
    }
    source.resolvePullPromise();
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(size);
#endif
}

bool FetchResponse::BodyLoader::start(ScriptExecutionContext& context, const FetchRequest& request)
{
    m_loader = std::make_unique<FetchLoader>(*this, &m_response.m_body->consumer());
    m_loader->start(context, request);
    return m_loader->isStarted();
}

void FetchResponse::BodyLoader::stop()
{
    m_promise = std::nullopt;
    if (m_loader)
        m_loader->stop();
}

void FetchResponse::consume(unsigned type, Ref<DeferredPromise>&& wrapper)
{
    ASSERT(type <= static_cast<unsigned>(FetchBodyConsumer::Type::Text));
    auto consumerType = static_cast<FetchBodyConsumer::Type>(type);

    if (isLoading()) {
        consumeOnceLoadingFinished(consumerType, WTFMove(wrapper));
        return;
    }

    switch (consumerType) {
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

#if ENABLE(READABLE_STREAM_API)
void FetchResponse::startConsumingStream(unsigned type)
{
    m_isDisturbed = true;
    m_consumer.setType(static_cast<FetchBodyConsumer::Type>(type));
}

void FetchResponse::consumeChunk(Ref<JSC::Uint8Array>&& chunk)
{
    m_consumer.append(chunk->data(), chunk->byteLength());
}

void FetchResponse::finishConsumingStream(Ref<DeferredPromise>&& promise)
{
    m_consumer.resolve(WTFMove(promise));
}

void FetchResponse::consumeBodyAsStream()
{
    ASSERT(m_readableStreamSource);
    m_isDisturbed = true;
    if (!isLoading()) {
        body().consumeAsStream(*this, *m_readableStreamSource);
        if (!m_readableStreamSource->isPulling())
            m_readableStreamSource = nullptr;
        return;
    }

    ASSERT(m_bodyLoader);

    RefPtr<SharedBuffer> data = m_bodyLoader->startStreaming();
    if (data) {
        if (!m_readableStreamSource->enqueue(data->createArrayBuffer())) {
            stop();
            return;
        }
        m_readableStreamSource->resolvePullPromise();
    }
}

void FetchResponse::closeStream()
{
    ASSERT(m_readableStreamSource);
    m_readableStreamSource->close();
    m_readableStreamSource = nullptr;
}

void FetchResponse::feedStream()
{
    ASSERT(m_readableStreamSource);
    bool shouldCloseStream = !m_bodyLoader;

    if (body().consumer().hasData()) {
        if (!m_readableStreamSource->enqueue(body().consumer().takeAsArrayBuffer())) {
            stop();
            return;
        }
        if (!shouldCloseStream) {
            m_readableStreamSource->resolvePullPromise();
            return;
        }
    } else if (!shouldCloseStream)
        return;

    closeStream();
}

ReadableStreamSource* FetchResponse::createReadableStreamSource()
{
    ASSERT(!m_readableStreamSource);
    ASSERT(!m_isDisturbed);

    if (isBodyNull())
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
        m_bodyLoader = std::nullopt;
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
