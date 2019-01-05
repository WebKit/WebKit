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
#include "ReadableStreamSource.h"
#include <JavaScriptCore/ArrayBufferView.h>

namespace WebCore {

FetchBody FetchBody::extract(ScriptExecutionContext& context, Init&& value, String& contentType)
{
    return WTF::switchOn(value, [&](RefPtr<Blob>& value) mutable {
        Ref<const Blob> blob = value.releaseNonNull();
        if (!blob->type().isEmpty())
            contentType = blob->type();
        return FetchBody(WTFMove(blob));
    }, [&](RefPtr<DOMFormData>& value) mutable {
        Ref<DOMFormData> domFormData = value.releaseNonNull();
        auto formData = FormData::createMultiPart(domFormData.get(), &downcast<Document>(context));
        contentType = makeString("multipart/form-data; boundary=", formData->boundary().data());
        return FetchBody(WTFMove(formData));
    }, [&](RefPtr<URLSearchParams>& value) mutable {
        Ref<const URLSearchParams> params = value.releaseNonNull();
        contentType = HTTPHeaderValues::formURLEncodedContentType();
        return FetchBody(WTFMove(params));
    }, [&](RefPtr<ArrayBuffer>& value) mutable {
        Ref<const ArrayBuffer> buffer = value.releaseNonNull();
        return FetchBody(WTFMove(buffer));
    }, [&](RefPtr<ArrayBufferView>& value) mutable {
        Ref<const ArrayBufferView> buffer = value.releaseNonNull();
        return FetchBody(WTFMove(buffer));
    }, [&](RefPtr<ReadableStream>& stream) mutable {
        return FetchBody(stream.releaseNonNull());
    }, [&](String& value) {
        contentType = HTTPHeaderValues::textPlainContentType();
        return FetchBody(WTFMove(value));
    });
}

Optional<FetchBody> FetchBody::fromFormData(FormData& formData)
{
    ASSERT(!formData.isEmpty());

    if (auto buffer = formData.asSharedBuffer()) {
        FetchBody body;
        body.m_consumer.setData(buffer.releaseNonNull());
        return WTFMove(body);
    }

    auto url = formData.asBlobURL();
    if (!url.isNull()) {
        // FIXME: Properly set mime type and size of the blob.
        Ref<const Blob> blob = Blob::deserialize(url, { }, 0, { });
        return FetchBody { WTFMove(blob) };
    }

    // FIXME: Support form data bodies.
    return WTF::nullopt;
}

void FetchBody::arrayBuffer(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.setType(FetchBodyConsumer::Type::ArrayBuffer);
    consume(owner, WTFMove(promise));
}

void FetchBody::blob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise, const String& contentType)
{
    m_consumer.setType(FetchBodyConsumer::Type::Blob);
    m_consumer.setContentType(Blob::normalizedContentType(extractMIMETypeFromMediaType(contentType)));
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

void FetchBody::consumeOnceLoadingFinished(FetchBodyConsumer::Type type, Ref<DeferredPromise>&& promise, const String& contentType)
{
    m_consumer.setType(type);
    m_consumer.setConsumePromise(WTFMove(promise));
    if (type == FetchBodyConsumer::Type::Blob)
        m_consumer.setContentType(Blob::normalizedContentType(extractMIMETypeFromMediaType(contentType)));
}

void FetchBody::consume(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    if (isArrayBuffer()) {
        consumeArrayBuffer(WTFMove(promise));
        return;
    }
    if (isArrayBufferView()) {
        consumeArrayBufferView(WTFMove(promise));
        return;
    }
    if (isText()) {
        consumeText(WTFMove(promise), textBody());
        return;
    }
    if (isURLSearchParams()) {
        consumeText(WTFMove(promise), urlSearchParamsBody().toString());
        return;
    }
    if (isBlob()) {
        consumeBlob(owner, WTFMove(promise));
        return;
    }
    if (isFormData()) {
        // FIXME: Support consuming FormData.
        promise->reject(NotSupportedError);
        return;
    }

    m_consumer.resolve(WTFMove(promise), m_readableStream.get());
}

#if ENABLE(STREAMS_API)

void FetchBody::consumeAsStream(FetchBodyOwner& owner, FetchBodySource& source)
{
    bool closeStream = false;
    if (isArrayBuffer()) {
        closeStream = source.enqueue(ArrayBuffer::tryCreate(arrayBufferBody().data(), arrayBufferBody().byteLength()));
        m_data = nullptr;
    } else if (isArrayBufferView()) {
        closeStream = source.enqueue(ArrayBuffer::tryCreate(arrayBufferViewBody().baseAddress(), arrayBufferViewBody().byteLength()));
        m_data = nullptr;
    } else if (isText()) {
        auto data = UTF8Encoding().encode(textBody(), UnencodableHandling::Entities);
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.size()));
        m_data = nullptr;
    } else if (isURLSearchParams()) {
        auto data = UTF8Encoding().encode(urlSearchParamsBody().toString(), UnencodableHandling::Entities);
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.size()));
        m_data = nullptr;
    } else if (isBlob()) {
        owner.loadBlob(blobBody(), nullptr);
        m_data = nullptr;
    } else if (isFormData())
        source.error(Exception { NotSupportedError, "Not implemented"_s });
    else if (m_consumer.hasData())
        closeStream = source.enqueue(m_consumer.takeAsArrayBuffer());
    else
        closeStream = true;

    if (closeStream)
        source.close();
}

#endif

void FetchBody::consumeArrayBuffer(Ref<DeferredPromise>&& promise)
{
    m_consumer.resolveWithData(WTFMove(promise), static_cast<const uint8_t*>(arrayBufferBody().data()), arrayBufferBody().byteLength());
    m_data = nullptr;
}

void FetchBody::consumeArrayBufferView(Ref<DeferredPromise>&& promise)
{
    m_consumer.resolveWithData(WTFMove(promise), static_cast<const uint8_t*>(arrayBufferViewBody().baseAddress()), arrayBufferViewBody().byteLength());
    m_data = nullptr;
}

void FetchBody::consumeText(Ref<DeferredPromise>&& promise, const String& text)
{
    auto data = UTF8Encoding().encode(text, UnencodableHandling::Entities);
    m_consumer.resolveWithData(WTFMove(promise), data.data(), data.size());
    m_data = nullptr;
}

void FetchBody::consumeBlob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.setConsumePromise(WTFMove(promise));
    owner.loadBlob(blobBody(), &m_consumer);
    m_data = nullptr;
}

void FetchBody::loadingFailed(const Exception& exception)
{
    m_consumer.loadingFailed(exception);
}

void FetchBody::loadingSucceeded()
{
    m_consumer.loadingSucceeded();
}

RefPtr<FormData> FetchBody::bodyAsFormData(ScriptExecutionContext& context) const
{
    if (isText())
        return FormData::create(UTF8Encoding().encode(textBody(), UnencodableHandling::Entities));
    if (isURLSearchParams())
        return FormData::create(UTF8Encoding().encode(urlSearchParamsBody().toString(), UnencodableHandling::Entities));
    if (isBlob()) {
        auto body = FormData::create();
        body->appendBlob(blobBody().url());
        return WTFMove(body);
    }
    if (isArrayBuffer())
        return FormData::create(arrayBufferBody().data(), arrayBufferBody().byteLength());
    if (isArrayBufferView())
        return FormData::create(arrayBufferViewBody().baseAddress(), arrayBufferViewBody().byteLength());
    if (isFormData()) {
        ASSERT(!context.isWorkerGlobalScope());
        auto body = makeRef(const_cast<FormData&>(formDataBody()));
        body->generateFiles(&downcast<Document>(context));
        return WTFMove(body);
    }
    if (auto* data = m_consumer.data())
        return FormData::create(data->data(), data->size());

    ASSERT_NOT_REACHED();
    return nullptr;
}

FetchBody::TakenData FetchBody::take()
{
    if (m_consumer.hasData()) {
        auto buffer = m_consumer.takeData();
        if (!buffer)
            return nullptr;
        return buffer.releaseNonNull();
    }

    if (isBlob()) {
        auto body = FormData::create();
        body->appendBlob(blobBody().url());
        return WTFMove(body);
    }

    if (isFormData())
        return formDataBody();

    if (isText())
        return SharedBuffer::create(UTF8Encoding().encode(textBody(), UnencodableHandling::Entities));
    if (isURLSearchParams())
        return SharedBuffer::create(UTF8Encoding().encode(urlSearchParamsBody().toString(), UnencodableHandling::Entities));

    if (isArrayBuffer())
        return SharedBuffer::create(reinterpret_cast<const char*>(arrayBufferBody().data()), arrayBufferBody().byteLength());
    if (isArrayBufferView())
        return SharedBuffer::create(reinterpret_cast<const uint8_t*>(arrayBufferViewBody().baseAddress()), arrayBufferViewBody().byteLength());

    return nullptr;
}

FetchBody FetchBody::clone()
{
    FetchBody clone(m_consumer);

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

    if (m_readableStream) {
        auto clones = m_readableStream->tee();
        m_readableStream = WTFMove(clones.first);
        clone.m_readableStream = WTFMove(clones.second);
    }
    return clone;
}

}
