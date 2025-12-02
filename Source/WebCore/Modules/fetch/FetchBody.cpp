/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "FetchBody.h"

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "FetchBodyOwner.h"
#include "FetchBodySource.h"
#include "FetchHeaders.h"
#include "HTTPHeaderValues.h"
#include "HTTPParsers.h"
#include "JSDOMFormData.h"
#include "JSDOMPromiseDeferred.h"
#include "ReadableStreamSource.h"
#include "StreamPipeOptions.h"
#include "TransformStream.h"
#include "WritableStream.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <pal/text/TextCodecUTF8.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

FetchBody::~FetchBody() = default;

ExceptionOr<FetchBody> FetchBody::extract(Init&& value, String& contentType)
{
    return WTF::switchOn(value, [&](RefPtr<Blob>& value) mutable -> ExceptionOr<FetchBody> {
        Ref<const Blob> blob = value.releaseNonNull();
        if (!blob->type().isEmpty())
            contentType = blob->type();
        return FetchBody(WTFMove(blob));
    }, [&](RefPtr<DOMFormData>& value) mutable -> ExceptionOr<FetchBody> {
        Ref<DOMFormData> domFormData = value.releaseNonNull();
        auto formData = FormData::createMultiPart(domFormData.get());
        contentType = makeString("multipart/form-data; boundary="_s, formData->boundary());
        return FetchBody(WTFMove(formData));
    }, [&](RefPtr<URLSearchParams>& value) mutable -> ExceptionOr<FetchBody> {
        Ref<const URLSearchParams> params = value.releaseNonNull();
        contentType = HTTPHeaderValues::formURLEncodedContentType();
        return FetchBody(WTFMove(params));
    }, [&](RefPtr<ArrayBuffer>& value) mutable -> ExceptionOr<FetchBody> {
        Ref<const ArrayBuffer> buffer = value.releaseNonNull();
        return FetchBody(WTFMove(buffer));
    }, [&](RefPtr<ArrayBufferView>& value) mutable -> ExceptionOr<FetchBody> {
        Ref<const ArrayBufferView> buffer = value.releaseNonNull();
        return FetchBody(WTFMove(buffer));
    }, [&](RefPtr<ReadableStream>& stream) mutable -> ExceptionOr<FetchBody> {
        if (stream->isDisturbed())
            return Exception { ExceptionCode::TypeError, "Input body is disturbed."_s };
        if (stream->isLocked())
            return Exception { ExceptionCode::TypeError, "Input body is locked."_s };

        return FetchBody(stream.releaseNonNull());
    }, [&](String& value) -> ExceptionOr<FetchBody> {
        contentType = HTTPHeaderValues::textPlainContentType();
        return FetchBody(WTFMove(value));
    });
}

std::optional<FetchBody> FetchBody::fromFormData(ScriptExecutionContext& context, Ref<FormData>&& formData)
{
    ASSERT(!formData->isEmpty());

    if (auto buffer = formData->asSharedBuffer()) {
        FetchBody body;
        body.checkedConsumer()->setData(buffer.releaseNonNull());
        return body;
    }

    auto url = formData->asBlobURL();
    if (!url.isNull()) {
        // FIXME: Properly set mime type and size of the blob.
        Ref<const Blob> blob = Blob::deserialize(&context, url, { }, { }, 0, { });
        return FetchBody { WTFMove(blob) };
    }

    return FetchBody { WTFMove(formData) };
}

void FetchBody::arrayBuffer(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    checkedConsumer()->setType(FetchBodyConsumer::Type::ArrayBuffer);
    consume(owner, WTFMove(promise));
}

void FetchBody::blob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    checkedConsumer()->setType(FetchBodyConsumer::Type::Blob);
    consume(owner, WTFMove(promise));
}

void FetchBody::bytes(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    checkedConsumer()->setType(FetchBodyConsumer::Type::Bytes);
    consume(owner, WTFMove(promise));
}

void FetchBody::json(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    if (isText()) {
        fulfillPromiseWithJSON(WTFMove(promise), textBody());
        return;
    }
    checkedConsumer()->setType(FetchBodyConsumer::Type::JSON);
    consume(owner, WTFMove(promise));
}

void FetchBody::text(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    if (isText()) {
        promise->resolve<IDLDOMString>(textBody());
        return;
    }
    checkedConsumer()->setType(FetchBodyConsumer::Type::Text);
    consume(owner, WTFMove(promise));
}

void FetchBody::formData(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    checkedConsumer()->setType(FetchBodyConsumer::Type::FormData);
    consume(owner, WTFMove(promise));
}

void FetchBody::consumeOnceLoadingFinished(FetchBodyConsumer::Type type, Ref<DeferredPromise>&& promise)
{
    CheckedRef consumer = this->consumer();
    consumer->setType(type);
    consumer->setConsumePromise(WTFMove(promise));
}

void FetchBody::consume(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    if (isArrayBuffer()) {
        consumeArrayBuffer(owner, WTFMove(promise));
        return;
    }
    if (isArrayBufferView()) {
        consumeArrayBufferView(owner, WTFMove(promise));
        return;
    }
    if (isText()) {
        consumeText(owner, WTFMove(promise), textBody());
        return;
    }
    if (isURLSearchParams()) {
        consumeText(owner, WTFMove(promise), protectedURLSearchParamsBody()->toString());
        return;
    }
    if (isBlob()) {
        consumeBlob(owner, WTFMove(promise));
        return;
    }
    if (isFormData()) {
        consumeFormData(owner, WTFMove(promise));
        return;
    }

    checkedConsumer()->resolve(WTFMove(promise), owner.contentType(), &owner, m_readableStream.get());
}

void FetchBody::consumeAsStream(FetchBodyOwner& owner, FetchBodySource& source)
{
    bool closeStream = false;
    if (isArrayBuffer())
        closeStream = source.enqueue(ArrayBuffer::tryCreate(protectedArrayBufferBody()->span()));
    else if (isArrayBufferView())
        closeStream = source.enqueue(ArrayBuffer::tryCreate(protectedArrayBufferViewBody()->span()));
    else if (isText()) {
        auto data = PAL::TextCodecUTF8::encodeUTF8(textBody());
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data));
    } else if (isURLSearchParams()) {
        auto data = PAL::TextCodecUTF8::encodeUTF8(protectedURLSearchParamsBody()->toString());
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data));
    } else if (isBlob())
        owner.loadBlob(protectedBlobBody().get(), nullptr);
    else if (isFormData())
        checkedConsumer()->consumeFormDataAsStream(protectedFormDataBody().get(), source, owner.protectedScriptExecutionContext().get());
    else if (CheckedRef consumer = this->consumer(); consumer->hasData())
        closeStream = source.enqueue(consumer->asArrayBuffer());
    else
        closeStream = true;

    if (closeStream)
        source.close();
}

void FetchBody::consumeArrayBuffer(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    checkedConsumer()->resolveWithData(WTFMove(promise), owner.contentType(), protectedArrayBufferBody()->span());
    m_data = nullptr;
}

void FetchBody::consumeArrayBufferView(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    checkedConsumer()->resolveWithData(WTFMove(promise), owner.contentType(), protectedArrayBufferViewBody()->span());
    m_data = nullptr;
}

void FetchBody::consumeText(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise, const String& text)
{
    auto data = PAL::TextCodecUTF8::encodeUTF8(text);
    checkedConsumer()->resolveWithData(WTFMove(promise), owner.contentType(), data.span());
    m_data = nullptr;
}

void FetchBody::consumeBlob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    CheckedPtr consumer = m_consumer.get();
    RELEASE_ASSERT(consumer);
    consumer->setConsumePromise(WTFMove(promise));
    owner.loadBlob(protectedBlobBody().get(), consumer.get());
    m_data = nullptr;
}

void FetchBody::consumeFormData(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    checkedConsumer()->resolveWithFormData(WTFMove(promise), owner.contentType(), protectedFormDataBody().get(), owner.protectedScriptExecutionContext().get());
    m_data = nullptr;
}

void FetchBody::loadingFailed(const Exception& exception)
{
    checkedConsumer()->loadingFailed(exception);
}

void FetchBody::loadingSucceeded(const String& contentType)
{
    checkedConsumer()->loadingSucceeded(contentType);
}

RefPtr<FormData> FetchBody::bodyAsFormData() const
{
    if (isText())
        return FormData::create(PAL::TextCodecUTF8::encodeUTF8(textBody()));
    if (isURLSearchParams())
        return FormData::create(PAL::TextCodecUTF8::encodeUTF8(protectedURLSearchParamsBody()->toString()));
    if (isBlob()) {
        auto body = FormData::create();
        body->appendBlob(blobBody().url());
        return body;
    }
    if (isArrayBuffer())
        return FormData::create(protectedArrayBufferBody()->span());
    if (isArrayBufferView())
        return FormData::create(protectedArrayBufferViewBody()->span());
    if (isFormData())
        return &const_cast<FormData&>(formDataBody());
    if (RefPtr data = const_cast<FetchBody*>(this)->checkedConsumer()->data())
        return FormData::create(data->makeContiguous()->span());

    ASSERT_NOT_REACHED();
    return nullptr;
}

void FetchBody::convertReadableStreamToArrayBuffer(FetchBodyOwner& owner, CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    ASSERT(hasReadableStream());

    checkedConsumer()->extract(*protectedReadableStream(), [owner = Ref { owner }, data = SharedBufferBuilder(), completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        WTF::switchOn(WTFMove(result), [&](std::nullptr_t) {
            if (RefPtr arrayBuffer = data.takeAsArrayBuffer())
                owner->body().m_data = *arrayBuffer;
            completionHandler({ });
        }, [&](std::span<const uint8_t>&& chunk) {
            data.append(chunk);
        }, [&](JSC::JSValue) {
            completionHandler(Exception { ExceptionCode::TypeError, "Load failed"_s });
        }, [&](Exception&& error) {
            completionHandler(WTFMove(error));
        });
    });
}

FetchBody::TakenData FetchBody::take()
{
    if (m_consumer && m_consumer->hasData()) {
        auto buffer = checkedConsumer()->takeData();
        if (!buffer)
            return nullptr;
        return buffer->makeContiguous();
    }

    if (isBlob()) {
        auto body = FormData::create();
        body->appendBlob(blobBody().url());
        return TakenData { WTFMove(body) };
    }

    if (isFormData())
        return formDataBody();

    if (isText())
        return SharedBuffer::create(PAL::TextCodecUTF8::encodeUTF8(textBody()));
    if (isURLSearchParams())
        return SharedBuffer::create(PAL::TextCodecUTF8::encodeUTF8(protectedURLSearchParamsBody()->toString()));

    if (isArrayBuffer())
        return SharedBuffer::create(protectedArrayBufferBody()->span());
    if (isArrayBufferView())
        return SharedBuffer::create(protectedArrayBufferViewBody()->span());

    return nullptr;
}

FetchBodyConsumer& FetchBody::consumer()
{
    if (!m_consumer)
        m_consumer = makeUnique<FetchBodyConsumer>(FetchBodyConsumer::Type::None);
    return *m_consumer;
}

FetchBody FetchBody::clone(JSDOMGlobalObject& globalObject)
{
    FetchBody clone(checkedConsumer()->clone());

    if (isArrayBuffer())
        clone.m_data = protectedArrayBufferBody();
    else if (isArrayBufferView())
        clone.m_data = protectedArrayBufferViewBody();
    else if (isBlob())
        clone.m_data = protectedBlobBody();
    else if (isFormData())
        clone.m_data = Ref { const_cast<FormData&>(formDataBody()) };
    else if (isText())
        clone.m_data = textBody();
    else if (isURLSearchParams())
        clone.m_data = protectedURLSearchParamsBody();
    else if (RefPtr readableStream = m_readableStream) {
        auto clones = readableStream->tee(globalObject, true);
        ASSERT(!clones.hasException());
        if (!clones.hasException()) {
            auto pair = clones.releaseReturnValue();
            m_readableStream = WTFMove(pair[0]);
            clone.m_readableStream = WTFMove(pair[1]);
        }
    }
    return clone;
}

FetchBody FetchBody::createProxy(JSDOMGlobalObject& globalObject)
{
    FetchBody proxy;

    proxy.m_consumer = std::exchange(m_consumer, { });
    proxy.m_data = std::exchange(m_data, { });

    if (!proxy.isReadableStream())
        return proxy;

    auto identityTransformOrException = TransformStream::create(globalObject, { }, { }, { });
    ASSERT(!identityTransformOrException.hasException());
    if (identityTransformOrException.hasException())
        return proxy;

    Ref identityTransform = identityTransformOrException.releaseReturnValue();
    auto proxyStreamOrException = Ref { *m_readableStream }->pipeThrough(globalObject, { &identityTransform->readable(), &identityTransform->writable() }, { });
    ASSERT(!proxyStreamOrException.hasException());
    if (proxyStreamOrException.hasException())
        return proxy;

    Ref proxyStream = proxyStreamOrException.releaseReturnValue();
    proxy.m_data = proxyStream.get();
    proxy.m_readableStream = WTFMove(proxyStream);

    return proxy;
}

}
