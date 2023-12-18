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
#include "FetchBody.h"

#include "Document.h"
#include "FetchBodyOwner.h"
#include "FetchBodySource.h"
#include "FetchHeaders.h"
#include "HTTPHeaderValues.h"
#include "HTTPParsers.h"
#include "JSDOMFormData.h"
#include "JSDOMPromiseDeferred.h"
#include "ReadableStreamSource.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <pal/text/TextCodecUTF8.h>

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
        contentType = makeString("multipart/form-data; boundary=", formData->boundary().data());
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
        body.m_consumer.setData(buffer.releaseNonNull());
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
    m_consumer.setType(FetchBodyConsumer::Type::ArrayBuffer);
    consume(owner, WTFMove(promise));
}

void FetchBody::blob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.setType(FetchBodyConsumer::Type::Blob);
    consume(owner, WTFMove(promise));
}

void FetchBody::json(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    if (isText()) {
        fulfillPromiseWithJSON(WTFMove(promise), textBody());
        return;
    }
    m_consumer.setType(FetchBodyConsumer::Type::JSON);
    consume(owner, WTFMove(promise));
}

void FetchBody::text(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    if (isText()) {
        promise->resolve<IDLDOMString>(textBody());
        return;
    }
    m_consumer.setType(FetchBodyConsumer::Type::Text);
    consume(owner, WTFMove(promise));
}

void FetchBody::formData(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.setType(FetchBodyConsumer::Type::FormData);
    consume(owner, WTFMove(promise));
}

void FetchBody::consumeOnceLoadingFinished(FetchBodyConsumer::Type type, Ref<DeferredPromise>&& promise)
{
    m_consumer.setType(type);
    m_consumer.setConsumePromise(WTFMove(promise));
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
        consumeText(owner, WTFMove(promise), urlSearchParamsBody().toString());
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

    m_consumer.resolve(WTFMove(promise), owner.contentType(), &owner, m_readableStream.get());
}

void FetchBody::consumeAsStream(FetchBodyOwner& owner, FetchBodySource& source)
{
    bool closeStream = false;
    if (isArrayBuffer())
        closeStream = source.enqueue(ArrayBuffer::tryCreate(arrayBufferBody().data(), arrayBufferBody().byteLength()));
    else if (isArrayBufferView())
        closeStream = source.enqueue(ArrayBuffer::tryCreate(arrayBufferViewBody().baseAddress(), arrayBufferViewBody().byteLength()));
    else if (isText()) {
        auto data = PAL::TextCodecUTF8::encodeUTF8(textBody());
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.size()));
    } else if (isURLSearchParams()) {
        auto data = PAL::TextCodecUTF8::encodeUTF8(urlSearchParamsBody().toString());
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.size()));
    } else if (isBlob())
        owner.loadBlob(blobBody(), nullptr);
    else if (isFormData())
        m_consumer.consumeFormDataAsStream(formDataBody(), source, owner.scriptExecutionContext());
    else if (m_consumer.hasData())
        closeStream = source.enqueue(m_consumer.takeAsArrayBuffer());
    else
        closeStream = true;

    if (closeStream)
        source.close();
}

void FetchBody::consumeArrayBuffer(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.resolveWithData(WTFMove(promise), owner.contentType(), static_cast<const uint8_t*>(arrayBufferBody().data()), arrayBufferBody().byteLength());
    m_data = nullptr;
}

void FetchBody::consumeArrayBufferView(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.resolveWithData(WTFMove(promise), owner.contentType(), static_cast<const uint8_t*>(arrayBufferViewBody().baseAddress()), arrayBufferViewBody().byteLength());
    m_data = nullptr;
}

void FetchBody::consumeText(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise, const String& text)
{
    auto data = PAL::TextCodecUTF8::encodeUTF8(text);
    m_consumer.resolveWithData(WTFMove(promise), owner.contentType(), data.data(), data.size());
    m_data = nullptr;
}

void FetchBody::consumeBlob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.setConsumePromise(WTFMove(promise));
    owner.loadBlob(blobBody(), &m_consumer);
    m_data = nullptr;
}

void FetchBody::consumeFormData(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.resolveWithFormData(WTFMove(promise), owner.contentType(), formDataBody(), owner.scriptExecutionContext());
    m_data = nullptr;
}

void FetchBody::loadingFailed(const Exception& exception)
{
    m_consumer.loadingFailed(exception);
}

void FetchBody::loadingSucceeded(const String& contentType)
{
    m_consumer.loadingSucceeded(contentType);
}

RefPtr<FormData> FetchBody::bodyAsFormData() const
{
    if (isText())
        return FormData::create(PAL::TextCodecUTF8::encodeUTF8(textBody()));
    if (isURLSearchParams())
        return FormData::create(PAL::TextCodecUTF8::encodeUTF8(urlSearchParamsBody().toString()));
    if (isBlob()) {
        auto body = FormData::create();
        body->appendBlob(blobBody().url());
        return body;
    }
    if (isArrayBuffer())
        return FormData::create(arrayBufferBody().data(), arrayBufferBody().byteLength());
    if (isArrayBufferView())
        return FormData::create(arrayBufferViewBody().baseAddress(), arrayBufferViewBody().byteLength());
    if (isFormData())
        return &const_cast<FormData&>(formDataBody());
    if (auto* data = m_consumer.data())
        return FormData::create(data->makeContiguous()->data(), data->size());

    ASSERT_NOT_REACHED();
    return nullptr;
}

FetchBody::TakenData FetchBody::take()
{
    if (m_consumer.hasData()) {
        auto buffer = m_consumer.takeData();
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
        return SharedBuffer::create(PAL::TextCodecUTF8::encodeUTF8(urlSearchParamsBody().toString()));

    if (isArrayBuffer())
        return SharedBuffer::create(static_cast<const char*>(arrayBufferBody().data()), arrayBufferBody().byteLength());
    if (isArrayBufferView())
        return SharedBuffer::create(static_cast<const uint8_t*>(arrayBufferViewBody().baseAddress()), arrayBufferViewBody().byteLength());

    return nullptr;
}

FetchBody FetchBody::clone()
{
    FetchBody clone(m_consumer.clone());

    if (isArrayBuffer())
        clone.m_data = arrayBufferBody();
    else if (isArrayBufferView())
        clone.m_data = arrayBufferViewBody();
    else if (isBlob())
        clone.m_data = blobBody();
    else if (isFormData())
        clone.m_data = const_cast<FormData&>(formDataBody());
    else if (isText())
        clone.m_data = textBody();
    else if (isURLSearchParams())
        clone.m_data = urlSearchParamsBody();
    else if (m_readableStream) {
        auto clones = m_readableStream->tee(true);
        ASSERT(!clones.hasException());
        if (!clones.hasException()) {
            auto pair = clones.releaseReturnValue();
            m_readableStream = WTFMove(pair[0]);
            clone.m_readableStream = WTFMove(pair[1]);
        }
    }
    return clone;
}

}
