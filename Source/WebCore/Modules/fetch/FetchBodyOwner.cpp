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
#include "FetchBodyOwner.h"

#include "FetchLoader.h"
#include "HTTPParsers.h"
#include "JSBlob.h"
#include "ResourceError.h"
#include "ResourceResponse.h"

namespace WebCore {

FetchBodyOwner::FetchBodyOwner(ScriptExecutionContext& context, Optional<FetchBody>&& body, Ref<FetchHeaders>&& headers)
    : ActiveDOMObject(&context)
    , m_body(WTFMove(body))
    , m_headers(WTFMove(headers))
{
    suspendIfNeeded();
}

FetchBodyOwner::~FetchBodyOwner()
{
    if (m_readableStreamSource)
        m_readableStreamSource->detach();
}

void FetchBodyOwner::stop()
{
    if (m_body)
        m_body->cleanConsumer();

    if (m_blobLoader) {
        bool isUniqueReference = hasOneRef();
        if (m_blobLoader->loader)
            m_blobLoader->loader->stop();
        // After that point, 'this' may be destroyed, since unsetPendingActivity should have been called.
        ASSERT_UNUSED(isUniqueReference, isUniqueReference || !m_blobLoader);
    }
}

bool FetchBodyOwner::isDisturbed() const
{
    if (isBodyNull())
        return false;

    if (m_isDisturbed)
        return true;

#if ENABLE(STREAMS_API)
    if (body().readableStream())
        return body().readableStream()->isDisturbed();
#endif

    return false;
}

bool FetchBodyOwner::isDisturbedOrLocked() const
{
    if (isBodyNull())
        return false;

    if (m_isDisturbed)
        return true;

#if ENABLE(STREAMS_API)
    if (body().readableStream())
        return body().readableStream()->isDisturbed() || body().readableStream()->isLocked();
#endif

    return false;
}

void FetchBodyOwner::arrayBuffer(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        fulfillPromiseWithArrayBuffer(WTFMove(promise), nullptr, 0);
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->arrayBuffer(*this, WTFMove(promise));
}

void FetchBodyOwner::blob(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        promise->resolve<IDLInterface<Blob>>(Blob::create(Vector<uint8_t> { }, Blob::normalizedContentType(extractMIMETypeFromMediaType(m_contentType))));
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->blob(*this, WTFMove(promise), m_contentType);
}

void FetchBodyOwner::cloneBody(FetchBodyOwner& owner)
{
    m_loadingError = owner.m_loadingError;

    m_contentType = owner.m_contentType;
    if (owner.isBodyNull())
        return;
    m_body = owner.m_body->clone();
}

void FetchBodyOwner::extractBody(ScriptExecutionContext& context, FetchBody::Init&& value)
{
    m_body = FetchBody::extract(context, WTFMove(value), m_contentType);
}

void FetchBodyOwner::updateContentType()
{
    String contentType = m_headers->fastGet(HTTPHeaderName::ContentType);
    if (!contentType.isNull()) {
        m_contentType = WTFMove(contentType);
        return;
    }
    if (!m_contentType.isNull())
        m_headers->fastSet(HTTPHeaderName::ContentType, m_contentType);
}

void FetchBodyOwner::consumeOnceLoadingFinished(FetchBodyConsumer::Type type, Ref<DeferredPromise>&& promise)
{
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->consumeOnceLoadingFinished(type, WTFMove(promise), m_contentType);
}

void FetchBodyOwner::formData(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        promise->reject();
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->formData(*this, WTFMove(promise));
}

void FetchBodyOwner::json(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        promise->reject(SyntaxError);
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->json(*this, WTFMove(promise));
}

void FetchBodyOwner::text(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        promise->resolve<IDLDOMString>({ });
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->text(*this, WTFMove(promise));
}

void FetchBodyOwner::loadBlob(const Blob& blob, FetchBodyConsumer* consumer)
{
    // Can only be called once for a body instance.
    ASSERT(!m_blobLoader);
    ASSERT(!isBodyNull());

    if (!scriptExecutionContext()) {
        m_body->loadingFailed(Exception { TypeError, "Blob loading failed"_s});
        return;
    }

    m_blobLoader.emplace(*this);
    m_blobLoader->loader = std::make_unique<FetchLoader>(*m_blobLoader, consumer);

    m_blobLoader->loader->start(*scriptExecutionContext(), blob);
    if (!m_blobLoader->loader->isStarted()) {
        m_body->loadingFailed(Exception { TypeError, "Blob loading failed"_s});
        m_blobLoader = WTF::nullopt;
        return;
    }
    setPendingActivity(this);
}

void FetchBodyOwner::finishBlobLoading()
{
    ASSERT(m_blobLoader);

    m_blobLoader = WTF::nullopt;
    unsetPendingActivity(this);
}

void FetchBodyOwner::blobLoadingSucceeded()
{
    ASSERT(!isBodyNull());
#if ENABLE(STREAMS_API)
    if (m_readableStreamSource) {
        m_readableStreamSource->close();
        m_readableStreamSource = nullptr;
    }
#endif
    m_body->loadingSucceeded();
    finishBlobLoading();
}

void FetchBodyOwner::blobLoadingFailed()
{
    ASSERT(!isBodyNull());
#if ENABLE(STREAMS_API)
    if (m_readableStreamSource) {
        if (!m_readableStreamSource->isCancelling())
            m_readableStreamSource->error(Exception { TypeError, "Blob loading failed"_s});
        m_readableStreamSource = nullptr;
    } else
#endif
        m_body->loadingFailed(Exception { TypeError, "Blob loading failed"_s});

    finishBlobLoading();
}

void FetchBodyOwner::blobChunk(const char* data, size_t size)
{
    ASSERT(data);
#if ENABLE(STREAMS_API)
    ASSERT(m_readableStreamSource);
    if (!m_readableStreamSource->enqueue(ArrayBuffer::tryCreate(data, size)))
        stop();
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(size);
#endif
}

FetchBodyOwner::BlobLoader::BlobLoader(FetchBodyOwner& owner)
    : owner(owner)
{
}

void FetchBodyOwner::BlobLoader::didReceiveResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() != 200)
        didFail({ });
}

void FetchBodyOwner::BlobLoader::didFail(const ResourceError&)
{
    // didFail might be called within FetchLoader::start call.
    if (loader->isStarted())
        owner.blobLoadingFailed();
}

RefPtr<ReadableStream> FetchBodyOwner::readableStream(JSC::ExecState& state)
{
    if (isBodyNullOrOpaque())
        return nullptr;

    if (!m_body->hasReadableStream())
        createReadableStream(state);

    return m_body->readableStream();
}

void FetchBodyOwner::createReadableStream(JSC::ExecState& state)
{
    ASSERT(!m_readableStreamSource);
    if (isDisturbed()) {
        m_body->setReadableStream(ReadableStream::create(state, nullptr));
        m_body->readableStream()->lock();
    } else {
        m_readableStreamSource = adoptRef(*new FetchBodySource(*this));
        m_body->setReadableStream(ReadableStream::create(state, m_readableStreamSource));
    }
}

void FetchBodyOwner::consumeBodyAsStream()
{
    ASSERT(m_readableStreamSource);

    if (auto exception = loadingException()) {
        m_readableStreamSource->error(*exception);
        return;
    }

    body().consumeAsStream(*this, *m_readableStreamSource);
    if (!m_readableStreamSource->isPulling())
        m_readableStreamSource = nullptr;
}

ResourceError FetchBodyOwner::loadingError() const
{
    return WTF::switchOn(m_loadingError, [](const ResourceError& error) {
        return ResourceError { error };
    }, [](const Exception& exception) {
        return ResourceError { errorDomainWebKitInternal, 0, { }, exception.message() };
    }, [](auto&&) {
        return ResourceError { };
    });
}

Optional<Exception> FetchBodyOwner::loadingException() const
{
    return WTF::switchOn(m_loadingError, [](const ResourceError& error) {
        return Exception { TypeError, error.localizedDescription().isEmpty() ? "Loading failed"_s : error.localizedDescription() };
    }, [](const Exception& exception) {
        return Exception { exception };
    }, [](auto&&) -> Optional<Exception> {
        return WTF::nullopt;
    });
}

bool FetchBodyOwner::hasLoadingError() const
{
    return WTF::switchOn(m_loadingError, [](const ResourceError&) {
        return true;
    }, [](const Exception&) {
        return true;
    }, [](auto&&) {
        return false;
    });
}

void FetchBodyOwner::setLoadingError(Exception&& exception)
{
    if (hasLoadingError())
        return;

    m_loadingError = WTFMove(exception);
}

void FetchBodyOwner::setLoadingError(ResourceError&& error)
{
    if (hasLoadingError())
        return;

    m_loadingError = WTFMove(error);
}

} // namespace WebCore
