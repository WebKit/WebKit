/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "FetchLoader.h"
#include "HTTPParsers.h"
#include "HTTPStatusCodes.h"
#include "JSBlob.h"
#include "JSDOMFormData.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "Settings.h"
#include "WindowEventLoop.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(FetchBodyOwner);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FetchBodyOwner::BlobLoader);

FetchBodyOwner::FetchBodyOwner(ScriptExecutionContext* context, std::optional<FetchBody>&& body, Ref<FetchHeaders>&& headers)
    : ActiveDOMObject(context)
    , m_body(WTF::move(body))
    , m_headers(WTF::move(headers))
{
}

FetchBodyOwner::~FetchBodyOwner()
{
    if (RefPtr readableStreamSource = m_readableStreamSource)
        readableStreamSource->detach();
}

void FetchBodyOwner::stop()
{
    m_readableStreamSource = nullptr;
    if (m_body)
        m_body->cleanConsumer();

    if (m_blobLoader) {
        bool isUniqueReference = hasOneRef();
        if (RefPtr loader = m_blobLoader->loader.get())
            loader->stop();
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

    if (RefPtr readableStream = body().readableStream())
        return readableStream->isDisturbed();

    return false;
}

bool FetchBodyOwner::isDisturbedOrLocked() const
{
    if (isBodyNull())
        return false;

    if (m_isDisturbed)
        return true;

    if (RefPtr readableStream = body().readableStream())
        return readableStream->isDisturbed() || readableStream->isLocked();

    return false;
}

void FetchBodyOwner::arrayBuffer(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        fulfillPromiseWithArrayBufferFromSpan(WTF::move(promise), { });
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->arrayBuffer(*this, WTF::move(promise));
}

void FetchBodyOwner::blob(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        promise->resolveCallbackValueWithNewlyCreated<IDLInterface<Blob>>([this](auto& context) {
            return Blob::create(&context, Vector<uint8_t> { }, Blob::normalizedContentType(extractMIMETypeFromMediaType(contentType())));
        });
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->blob(*this, WTF::move(promise));
}

void FetchBodyOwner::bytes(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        fulfillPromiseWithUint8ArrayFromSpan(WTF::move(promise), { });
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->bytes(*this, WTF::move(promise));
}

void FetchBodyOwner::cloneBody(JSDOMGlobalObject& globalObject, FetchBodyOwner& owner)
{
    m_loadingError = owner.m_loadingError;
    if (owner.isBodyNull())
        return;
    m_body = owner.m_body->clone(globalObject);
}

ExceptionOr<void> FetchBodyOwner::extractBody(FetchBody::Init&& value)
{
    auto currentContentType = contentType();
    bool isContentTypeSet = !currentContentType.isNull();
    auto result = FetchBody::extract(WTF::move(value), currentContentType);

    // Initialize the Content-Type header if it didn't exist.
    if (!isContentTypeSet && !currentContentType.isNull())
        m_headers->fastSet(HTTPHeaderName::ContentType, currentContentType);

    if (result.hasException())
        return result.releaseException();
    m_body = result.releaseReturnValue();
    return { };
}

void FetchBodyOwner::consumeOnceLoadingFinished(FetchBodyConsumer::Type type, Ref<DeferredPromise>&& promise)
{
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->consumeOnceLoadingFinished(type, WTF::move(promise));
}

void FetchBodyOwner::formData(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isDisturbedOrLocked()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Body is disturbed or locked"_s });
        return;
    }

    if (isBodyNullOrOpaque()) {
        if (isBodyNull()) {
            // If the content-type is 'application/x-www-form-urlencoded', a body is not required and we should package an empty byte sequence as per the specification.
            if (auto formData = FetchBodyConsumer::packageFormData(promise->protectedScriptExecutionContext().get(), contentType(), { })) {
                promise->resolve<IDLInterface<DOMFormData>>(*formData);
                return;
            }
        }

        promise->reject(ExceptionCode::TypeError);
        return;
    }

    m_isDisturbed = true;
    m_body->formData(*this, WTF::move(promise));
}

void FetchBodyOwner::json(Ref<DeferredPromise>&& promise)
{
    if (auto exception = loadingException()) {
        promise->reject(*exception);
        return;
    }

    if (isBodyNullOrOpaque()) {
        promise->reject(ExceptionCode::SyntaxError);
        return;
    }
    if (isDisturbedOrLocked()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->json(*this, WTF::move(promise));
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
        promise->reject(Exception { ExceptionCode::TypeError, "Body is disturbed or locked"_s });
        return;
    }
    m_isDisturbed = true;
    m_body->text(*this, WTF::move(promise));
}

void FetchBodyOwner::loadBlob(const Blob& blob, FetchBodyConsumer* consumer)
{
    // Can only be called once for a body instance.
    ASSERT(!m_blobLoader);
    ASSERT(!isBodyNull());

    if (!scriptExecutionContext()) {
        m_body->loadingFailed(Exception { ExceptionCode::TypeError, "Blob loading failed"_s });
        return;
    }

    Ref blobLoader = BlobLoader::create(*this);
    m_blobLoader = blobLoader.copyRef();
    Ref loader = FetchLoader::create(blobLoader.get(), consumer);
    blobLoader->loader = loader.copyRef();

    loader->start(*protectedScriptExecutionContext(), blob);
    if (!loader->isStarted()) {
        m_body->loadingFailed(Exception { ExceptionCode::TypeError, "Blob loading failed"_s });
        m_blobLoader = nullptr;
        return;
    }
}

void FetchBodyOwner::finishBlobLoading()
{
    ASSERT(m_blobLoader);

    m_blobLoader = nullptr;
}

void FetchBodyOwner::blobLoadingSucceeded()
{
    ASSERT(!isBodyNull());
    if (RefPtr readableStreamSource = std::exchange(m_readableStreamSource, nullptr))
        readableStreamSource->close();

    m_body->loadingSucceeded(contentType());
    if (!m_blobLoader)
        return;

    finishBlobLoading();
}

void FetchBodyOwner::blobLoadingFailed()
{
    ASSERT(!isBodyNull());
    if (RefPtr readableStreamSource = std::exchange(m_readableStreamSource, nullptr)) {
        if (!readableStreamSource->isCancelling())
            readableStreamSource->error(Exception { ExceptionCode::TypeError, "Blob loading failed"_s });
    } else
        m_body->loadingFailed(Exception { ExceptionCode::TypeError, "Blob loading failed"_s });
    finishBlobLoading();
}

void FetchBodyOwner::blobChunk(const SharedBuffer& buffer)
{
    RefPtr readableStreamSource = m_readableStreamSource;
    ASSERT(readableStreamSource);
    if (!readableStreamSource->enqueue(buffer.tryCreateArrayBuffer()))
        stop();
}

Ref<FetchBodyOwner::BlobLoader> FetchBodyOwner::BlobLoader::create(FetchBodyOwner& owner)
{
    return adoptRef(*new BlobLoader(owner));
}

FetchBodyOwner::BlobLoader::BlobLoader(FetchBodyOwner& owner)
    : m_owner(owner)
{
}

FetchBodyOwner::BlobLoader::~BlobLoader() = default;

void FetchBodyOwner::BlobLoader::didReceiveResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() != httpStatus200OK)
        didFail({ });
}

void FetchBodyOwner::BlobLoader::didFail(const ResourceError&)
{
    // didFail might be called within FetchLoader::start call.
    if (loader->isStarted()) {
        if (RefPtr owner = m_owner.get())
            owner->blobLoadingFailed();
    }
}

void FetchBodyOwner::BlobLoader::didSucceed(const NetworkLoadMetrics&)
{
    if (RefPtr owner = m_owner.get())
        owner->blobLoadingSucceeded();
}

void FetchBodyOwner::BlobLoader::didReceiveData(const SharedBuffer& buffer)
{
    if (RefPtr owner = m_owner.get())
        owner->blobChunk(buffer);
}

ExceptionOr<RefPtr<ReadableStream>> FetchBodyOwner::readableStream(JSC::JSGlobalObject& state)
{
    if (isBodyNullOrOpaque())
        return nullptr;

    if (!m_body->hasReadableStream()) {
        auto voidOrException = createReadableStream(state);
        if (voidOrException.hasException()) [[unlikely]]
            return voidOrException.releaseException();
    }

    return m_body->readableStream();
}

ExceptionOr<void> FetchBodyOwner::createReadableStream(JSC::JSGlobalObject& state)
{
    ASSERT(!m_readableStreamSource);

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(&state);
    if (isDisturbed()) {
        auto streamOrException = ReadableStream::create(globalObject, { }, { });
        if (streamOrException.hasException()) [[unlikely]]
            return streamOrException.releaseException();
        m_body->setReadableStream(streamOrException.releaseReturnValue());
        protect(m_body->readableStream())->lock();
        return { };
    }

    RefPtr context = scriptExecutionContext();
    if (context && context->settingsValues().readableByteStreamFetchSourceEnabled) {
        Ref readableStreamSource = FetchBodySource::createByteSource(*this);
        Ref readableStream = ReadableStream::createReadableByteStream(globalObject, [readableStreamSource](auto& globalObject, auto& controller) {
            return readableStreamSource->pull(globalObject, controller);
        }, [readableStreamSource](auto& globalObject, auto& controller, auto&& value) {
            return readableStreamSource->cancel(globalObject, controller, WTF::move(value));
        }, {
            .highwaterMark = 1,
            .startSynchronously = ReadableStream::StartSynchronously::Yes,
            .isSourceReachableFromOpaqueRoot = ReadableStream::IsSourceReachableFromOpaqueRoot::Yes
        });

        m_readableStreamSource = readableStreamSource.ptr();
        readableStreamSource->setByteController(*readableStream->controller());
        m_body->setReadableStream(WTF::move(readableStream));

        return { };
    }

    auto [fetchBodySource, readableStreamSource] = FetchBodySource::createNonByteSource(*this);
    m_readableStreamSource = WTF::move(fetchBodySource);

    auto streamOrException = ReadableStream::create(*JSC::jsCast<JSDOMGlobalObject*>(&state), readableStreamSource);
    if (streamOrException.hasException()) [[unlikely]] {
        m_readableStreamSource = nullptr;
        return streamOrException.releaseException();
    }
    m_body->setReadableStream(streamOrException.releaseReturnValue());
    return { };
}

void FetchBodyOwner::consumeBodyAsStream()
{
    RefPtr readableStreamSource = m_readableStreamSource;
    ASSERT(readableStreamSource);

    if (auto exception = loadingException()) {
        readableStreamSource->error(*exception);
        return;
    }

    body().consumeAsStream(*this, *readableStreamSource);
    if (!readableStreamSource->isPulling())
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

std::optional<Exception> FetchBodyOwner::loadingException() const
{
    return WTF::switchOn(m_loadingError, [](const ResourceError& error) -> std::optional<Exception> {
        return Exception { ExceptionCode::TypeError, error.sanitizedDescription() };
    }, [](const Exception& exception) -> std::optional<Exception> {
        return Exception { exception };
    }, [](auto&&) -> std::optional<Exception> {
        return std::nullopt;
    });
}

bool FetchBodyOwner::virtualHasPendingActivity() const
{
    return !!m_blobLoader || (m_body && m_body->hasConsumerPendingActivity());
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

    m_loadingError = WTF::move(exception);
}

void FetchBodyOwner::setLoadingError(ResourceError&& error)
{
    if (hasLoadingError())
        return;

    m_loadingError = WTF::move(error);
}

} // namespace WebCore
