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

#include "FetchRequest.h"
#include "HTTPParsers.h"
#include "InspectorInstrumentation.h"
#include "JSBlob.h"
#include "MIMETypeRegistry.h"
#include "ReadableStreamSink.h"
#include "ResourceError.h"
#include "ScriptExecutionContext.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

// https://fetch.spec.whatwg.org/#null-body-status
static inline bool isNullBodyStatus(int status)
{
    return status == 101 || status == 204 || status == 205 || status == 304;
}

Ref<FetchResponse> FetchResponse::create(ScriptExecutionContext& context, Optional<FetchBody>&& body, FetchHeaders::Guard guard, ResourceResponse&& response)
{
    bool isSynthetic = response.type() == ResourceResponse::Type::Default || response.type() == ResourceResponse::Type::Error;
    bool isOpaque = response.tainting() == ResourceResponse::Tainting::Opaque;
    auto headers = isOpaque ? FetchHeaders::create(guard) : FetchHeaders::create(guard, HTTPHeaderMap { response.httpHeaderFields() });

    auto fetchResponse = adoptRef(*new FetchResponse(context, WTFMove(body), WTFMove(headers), WTFMove(response)));
    if (!isSynthetic)
        fetchResponse->m_filteredResponse = ResourceResponseBase::filter(fetchResponse->m_internalResponse);
    if (isOpaque)
        fetchResponse->setBodyAsOpaque();
    return fetchResponse;
}

ExceptionOr<Ref<FetchResponse>> FetchResponse::create(ScriptExecutionContext& context, Optional<FetchBody::Init>&& body, Init&& init)
{
    // 1. If init’s status member is not in the range 200 to 599, inclusive, then throw a RangeError.
    if (init.status < 200  || init.status > 599)
        return Exception { RangeError, "Status must be between 200 and 599"_s };

    // 2. If init’s statusText member does not match the reason-phrase token production, then throw a TypeError.
    if (!isValidReasonPhrase(init.statusText))
        return Exception { TypeError, "Status text must be a valid reason-phrase."_s };

    // 3. Let r be a new Response object associated with a new response.
    // NOTE: Creation of the Response object is delayed until all potential exceptional cases are handled.
    
    // 4. Set r’s headers to a new Headers object, whose header list is r’s response’s header list, and guard is "response".
    auto headers = FetchHeaders::create(FetchHeaders::Guard::Response);

    // 5. Set r’s response’s status to init’s status member.
    auto status = init.status;
    
    // 6. Set r’s response’s status message to init’s statusText member.
    auto statusText = init.statusText;
    
    // 7. If init’s headers member is present, then fill r’s headers with init’s headers member.
    if (init.headers) {
        auto result = headers->fill(*init.headers);
        if (result.hasException())
            return result.releaseException();
    }

    Optional<FetchBody> extractedBody;

    // 8. If body is non-null, run these substeps:
    if (body) {
        // 8.1 If init’s status member is a null body status, then throw a TypeError.
        //     (NOTE: 101 is included in null body status due to its use elsewhere. It does not affect this step.)
        if (isNullBodyStatus(init.status))
            return Exception { TypeError, "Response cannot have a body with the given status."_s };

        // 8.2 Let Content-Type be null.
        String contentType;

        // 8.3 Set r’s response’s body and Content-Type to the result of extracting body.
        extractedBody = FetchBody::extract(context, WTFMove(*body), contentType);

        // 8.4 If Content-Type is non-null and r’s response’s header list does not contain `Content-Type`, then append
        //     `Content-Type`/Content-Type to r’s response’s header list.
        if (!contentType.isNull() && !headers->fastHas(HTTPHeaderName::ContentType))
            headers->fastSet(HTTPHeaderName::ContentType, contentType);
    }

    // 9. Set r’s MIME type to the result of extracting a MIME type from r’s response’s header list.
    auto contentType = headers->fastGet(HTTPHeaderName::ContentType);

    // 10. Set r’s response’s HTTPS state to current settings object’s HTTPS state.
    // FIXME: Implement.

    // 11. Resolve r’s trailer promise with a new Headers object whose guard is "immutable".
    // FIXME: Implement.
    
    // 12. Return r.
    auto r = adoptRef(*new FetchResponse(context, WTFMove(extractedBody), WTFMove(headers), { }));

    r->m_contentType = contentType;
    auto mimeType = extractMIMETypeFromMediaType(contentType);
    r->m_internalResponse.setMimeType(mimeType.isEmpty() ? defaultMIMEType() : mimeType);
    r->m_internalResponse.setTextEncodingName(extractCharsetFromMediaType(contentType));

    r->m_internalResponse.setHTTPStatusCode(status);
    r->m_internalResponse.setHTTPStatusText(statusText);

    return WTFMove(r);
}

Ref<FetchResponse> FetchResponse::error(ScriptExecutionContext& context)
{
    auto response = adoptRef(*new FetchResponse(context, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));
    response->m_internalResponse.setType(Type::Error);
    return response;
}

ExceptionOr<Ref<FetchResponse>> FetchResponse::redirect(ScriptExecutionContext& context, const String& url, int status)
{
    // FIXME: Tighten the URL parsing algorithm according https://url.spec.whatwg.org/#concept-url-parser.
    URL requestURL = context.completeURL(url);
    if (!requestURL.isValid())
        return Exception { TypeError, makeString("Redirection URL '", requestURL.string(), "' is invalid") };
    if (!requestURL.user().isEmpty() || !requestURL.pass().isEmpty())
        return Exception { TypeError, "Redirection URL contains credentials"_s };
    if (!ResourceResponse::isRedirectionStatusCode(status))
        return Exception { RangeError, makeString("Status code ", status, "is not a redirection status code") };
    auto redirectResponse = adoptRef(*new FetchResponse(context, { }, FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));
    redirectResponse->m_internalResponse.setHTTPStatusCode(status);
    redirectResponse->m_internalResponse.setHTTPHeaderField(HTTPHeaderName::Location, requestURL.string());
    redirectResponse->m_headers->fastSet(HTTPHeaderName::Location, requestURL.string());
    return WTFMove(redirectResponse);
}

FetchResponse::FetchResponse(ScriptExecutionContext& context, Optional<FetchBody>&& body, Ref<FetchHeaders>&& headers, ResourceResponse&& response)
    : FetchBodyOwner(context, WTFMove(body), WTFMove(headers))
    , m_internalResponse(WTFMove(response))
{
}

ExceptionOr<Ref<FetchResponse>> FetchResponse::clone(ScriptExecutionContext& context)
{
    if (isDisturbedOrLocked())
        return Exception { TypeError, "Body is disturbed or locked"_s };

    ASSERT(scriptExecutionContext());

    // If loading, let's create a stream so that data is teed on both clones.
    if (isLoading() && !m_readableStreamSource)
        createReadableStream(*context.execState());

    // Synthetic responses do not store headers in m_internalResponse.
    if (m_internalResponse.type() == ResourceResponse::Type::Default)
        m_internalResponse.setHTTPHeaderFields(HTTPHeaderMap { headers().internalHeaders() });

    auto clone = FetchResponse::create(context, WTF::nullopt, headers().guard(), ResourceResponse { m_internalResponse });
    clone->cloneBody(*this);
    clone->m_opaqueLoadIdentifier = m_opaqueLoadIdentifier;
    clone->m_bodySizeWithPadding = m_bodySizeWithPadding;
    return WTFMove(clone);
}

void FetchResponse::addAbortSteps(Ref<AbortSignal>&& signal)
{
    m_abortSignal = WTFMove(signal);
    m_abortSignal->addAlgorithm([this, weakThis = makeWeakPtr(this)] {
        // FIXME: Cancel request body if it is a stream.
        if (!weakThis)
            return;

        m_abortSignal = nullptr;

        setLoadingError(Exception { AbortError, "Fetch is aborted"_s });

        if (m_bodyLoader) {
            if (auto callback = m_bodyLoader->takeNotificationCallback())
                callback(Exception { AbortError, "Fetch is aborted"_s });
        }

        if (m_readableStreamSource) {
            if (!m_readableStreamSource->isCancelling())
                m_readableStreamSource->error(*loadingException());
            m_readableStreamSource = nullptr;
        }
        if (m_body)
            m_body->loadingFailed(*loadingException());

        if (m_bodyLoader) {
            m_bodyLoader->stop();
            m_bodyLoader = WTF::nullopt;
        }
    });
}

void FetchResponse::fetch(ScriptExecutionContext& context, FetchRequest& request, NotificationCallback&& responseCallback)
{
    if (request.signal().aborted()) {
        responseCallback(Exception { AbortError, "Request signal is aborted"_s });
        // FIXME: Cancel request body if it is a stream.
        return;
    }

    if (request.hasReadableStreamBody()) {
        responseCallback(Exception { NotSupportedError, "ReadableStream uploading is not supported"_s });
        return;
    }

    InspectorInstrumentation::willFetch(context, request.url());

    auto response = adoptRef(*new FetchResponse(context, FetchBody { }, FetchHeaders::create(FetchHeaders::Guard::Immutable), { }));

    response->body().consumer().setAsLoading();

    response->addAbortSteps(request.signal());

    response->m_bodyLoader.emplace(response.get(), WTFMove(responseCallback));
    if (!response->m_bodyLoader->start(context, request))
        response->m_bodyLoader = WTF::nullopt;
}

const String& FetchResponse::url() const
{
    if (m_responseURL.isNull()) {
        URL url = filteredResponse().url();
        url.removeFragmentIdentifier();
        m_responseURL = url.string();
    }
    return m_responseURL;
}

const ResourceResponse& FetchResponse::filteredResponse() const
{
    if (m_filteredResponse)
        return m_filteredResponse.value();
    return m_internalResponse;
}

void FetchResponse::BodyLoader::didSucceed()
{
    ASSERT(m_response.hasPendingActivity());
    m_response.m_body->loadingSucceeded();

#if ENABLE(STREAMS_API)
    if (m_response.m_readableStreamSource) {
        if (m_response.body().consumer().hasData())
            m_response.m_readableStreamSource->enqueue(m_response.body().consumer().takeAsArrayBuffer());

        m_response.closeStream();
    }
#endif
    if (auto consumeDataCallback = WTFMove(m_consumeDataCallback))
        consumeDataCallback(nullptr);

    if (m_loader->isStarted()) {
        Ref<FetchResponse> protector(m_response);
        m_response.m_bodyLoader = WTF::nullopt;
    }
}

void FetchResponse::BodyLoader::didFail(const ResourceError& error)
{
    ASSERT(m_response.hasPendingActivity());

    m_response.setLoadingError(ResourceError { error });

    if (auto responseCallback = WTFMove(m_responseCallback))
        responseCallback(Exception { TypeError, error.localizedDescription() });

    if (auto consumeDataCallback = WTFMove(m_consumeDataCallback))
        consumeDataCallback(Exception { TypeError, error.localizedDescription() });

#if ENABLE(STREAMS_API)
    if (m_response.m_readableStreamSource) {
        if (!m_response.m_readableStreamSource->isCancelling())
            m_response.m_readableStreamSource->error(*m_response.loadingException());
        m_response.m_readableStreamSource = nullptr;
    }
#endif

    // Check whether didFail is called as part of FetchLoader::start.
    if (m_loader->isStarted()) {
        Ref<FetchResponse> protector(m_response);
        m_response.m_bodyLoader = WTF::nullopt;
    }
}

FetchResponse::BodyLoader::BodyLoader(FetchResponse& response, NotificationCallback&& responseCallback)
    : m_response(response)
    , m_responseCallback(WTFMove(responseCallback))
{
    m_response.setPendingActivity(m_response);
}

FetchResponse::BodyLoader::~BodyLoader()
{
    m_response.unsetPendingActivity(m_response);
}

static uint64_t nextOpaqueLoadIdentifier { 0 };
void FetchResponse::BodyLoader::didReceiveResponse(const ResourceResponse& resourceResponse)
{
    m_response.m_filteredResponse = ResourceResponseBase::filter(resourceResponse);
    m_response.m_internalResponse = resourceResponse;
    m_response.m_internalResponse.setType(m_response.m_filteredResponse->type());
    if (resourceResponse.tainting() == ResourceResponse::Tainting::Opaque) {
        m_response.m_opaqueLoadIdentifier = ++nextOpaqueLoadIdentifier;
        m_response.setBodyAsOpaque();
    }

    m_response.m_headers->filterAndFill(m_response.m_filteredResponse->httpHeaderFields(), FetchHeaders::Guard::Response);
    m_response.updateContentType();

    if (auto responseCallback = WTFMove(m_responseCallback))
        responseCallback(m_response);
}

void FetchResponse::BodyLoader::didReceiveData(const char* data, size_t size)
{
#if ENABLE(STREAMS_API)
    ASSERT(m_response.m_readableStreamSource || m_consumeDataCallback);
#else
    ASSERT(m_consumeDataCallback);
#endif

    if (m_consumeDataCallback) {
        ReadableStreamChunk chunk { reinterpret_cast<const uint8_t*>(data), size };
        m_consumeDataCallback(&chunk);
        return;
    }

#if ENABLE(STREAMS_API)
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
    m_responseCallback = { };
    if (m_loader)
        m_loader->stop();
}

void FetchResponse::BodyLoader::consumeDataByChunk(ConsumeDataByChunkCallback&& consumeDataCallback)
{
    ASSERT(!m_consumeDataCallback);
    m_consumeDataCallback = WTFMove(consumeDataCallback);
    auto data = m_loader->startStreaming();
    if (!data)
        return;

    ReadableStreamChunk chunk { reinterpret_cast<const uint8_t*>(data->data()), data->size() };
    m_consumeDataCallback(&chunk);
}

FetchResponse::ResponseData FetchResponse::consumeBody()
{
    ASSERT(!isBodyReceivedByChunk());

    if (isBodyNull())
        return nullptr;

    ASSERT(!m_isDisturbed);
    m_isDisturbed = true;

    return body().take();
}

void FetchResponse::consumeBodyReceivedByChunk(ConsumeDataByChunkCallback&& callback)
{
    ASSERT(isBodyReceivedByChunk());
    ASSERT(!isDisturbed());
    m_isDisturbed = true;

    if (hasReadableStreamBody()) {
        m_body->consumer().extract(*m_body->readableStream(), WTFMove(callback));
        return;
    }

    ASSERT(isLoading());
    m_bodyLoader->consumeDataByChunk(WTFMove(callback));
}

void FetchResponse::setBodyData(ResponseData&& data, uint64_t bodySizeWithPadding)
{
    m_bodySizeWithPadding = bodySizeWithPadding;
    WTF::switchOn(data,
        [this](Ref<FormData>& formData) {
            if (isBodyNull())
                setBody({ });
            body().setAsFormData(WTFMove(formData));
        },
        [this](Ref<SharedBuffer>& buffer) {
            if (isBodyNull())
                setBody({ });
            body().consumer().setData(WTFMove(buffer));
        },
        [](std::nullptr_t&) {
        }
    );
}

#if ENABLE(STREAMS_API)
void FetchResponse::consumeChunk(Ref<JSC::Uint8Array>&& chunk)
{
    body().consumer().append(chunk->data(), chunk->byteLength());
}

void FetchResponse::consumeBodyAsStream()
{
    ASSERT(m_readableStreamSource);
    if (!isLoading()) {
        FetchBodyOwner::consumeBodyAsStream();
        return;
    }

    ASSERT(m_bodyLoader);

    auto data = m_bodyLoader->startStreaming();
    if (data) {
        if (!m_readableStreamSource->enqueue(data->tryCreateArrayBuffer())) {
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
        m_bodyLoader = WTF::nullopt;
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

ResourceResponse FetchResponse::resourceResponse() const
{
    auto response = m_internalResponse;

    if (headers().guard() != FetchHeaders::Guard::Immutable) {
        // FIXME: Add a setHTTPHeaderFields on ResourceResponseBase.
        for (auto& header : headers().internalHeaders())
            response.setHTTPHeaderField(header.key, header.value);
    }

    return response;
}

} // namespace WebCore
