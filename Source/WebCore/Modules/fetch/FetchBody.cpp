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

#if ENABLE(FETCH_API)

#include "DOMRequestState.h"
#include "Dictionary.h"
#include "FetchBodyOwner.h"
#include "FetchHeaders.h"
#include "FetchResponseSource.h"
#include "FormData.h"
#include "HTTPParsers.h"
#include "JSBlob.h"
#include "JSDOMFormData.h"
#include "JSReadableStream.h"
#include "JSURLSearchParams.h"
#include "ReadableStreamSource.h"
#include <runtime/ArrayBufferView.h>

namespace WebCore {

FetchBody::FetchBody(Ref<const Blob>&& blob)
    : m_data(WTFMove(blob))
    , m_contentType(this->blobBody().type())
{
}

FetchBody::FetchBody(DOMFormData& formData, Document& document)
    : m_data(FormData::createMultiPart(formData, formData.encoding(), &document))
    , m_contentType(makeString("multipart/form-data;boundary=", this->formDataBody().boundary().data()))
{
}

FetchBody::FetchBody(String&& text)
    : m_data(WTFMove(text))
    , m_contentType(ASCIILiteral("text/plain;charset=UTF-8"))
{
}

FetchBody::FetchBody(Ref<const ArrayBuffer>&& data)
    : m_data(WTFMove(data))
{
}

FetchBody::FetchBody(Ref<const ArrayBufferView>&& dataView)
    : m_data(WTFMove(dataView))
{
}

FetchBody::FetchBody(Ref<const URLSearchParams>&& url)
    : m_data(WTFMove(url))
    , m_contentType(ASCIILiteral("application/x-www-form-urlencoded;charset=UTF-8"))
{
}

FetchBody::FetchBody(const String& contentType, const FetchBodyConsumer& consumer)
    : m_contentType(contentType)
    , m_consumer(consumer)
{
}

FetchBody FetchBody::extract(ScriptExecutionContext& context, JSC::ExecState& state, JSC::JSValue value)
{
    if (value.inherits(JSBlob::info()))
        return FetchBody(*JSBlob::toWrapped(value));
    if (value.inherits(JSDOMFormData::info())) {
        ASSERT(!context.isWorkerGlobalScope());
        return FetchBody(*JSDOMFormData::toWrapped(value), static_cast<Document&>(context));
    }
    if (value.isString())
        return FetchBody(value.toWTFString(&state));
    if (value.inherits(JSURLSearchParams::info()))
        return FetchBody(*JSURLSearchParams::toWrapped(value));
    if (value.inherits(JSReadableStream::info())) {
        FetchBody body;
        body.m_isEmpty = false;
        return body;
    } if (value.inherits(JSC::JSArrayBuffer::info())) {
        ArrayBuffer* data = toArrayBuffer(value);
        ASSERT(data);
        return { *data };
    }
    if (value.inherits(JSC::JSArrayBufferView::info()))
        return { toArrayBufferView(value).releaseConstNonNull() };

    return { };
}

FetchBody FetchBody::extractFromBody(FetchBody* body)
{
    if (!body)
        return { };

    return FetchBody(WTFMove(*body));
}

void FetchBody::updateContentType(FetchHeaders& headers)
{
    String contentType = headers.fastGet(HTTPHeaderName::ContentType);
    if (!contentType.isNull()) {
        m_contentType = contentType;
        return;
    }
    if (!m_contentType.isNull())
        headers.fastSet(HTTPHeaderName::ContentType, m_contentType);
}

void FetchBody::arrayBuffer(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.setType(FetchBodyConsumer::Type::ArrayBuffer);
    consume(owner, WTFMove(promise));
}

void FetchBody::blob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumer.setType(FetchBodyConsumer::Type::Blob);
    m_consumer.setContentType(Blob::normalizedContentType(extractMIMETypeFromMediaType(m_contentType)));
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
        promise->resolve(textBody());
        return;
    }
    m_consumer.setType(FetchBodyConsumer::Type::Text);
    consume(owner, WTFMove(promise));
}

void FetchBody::consumeOnceLoadingFinished(FetchBodyConsumer::Type type, Ref<DeferredPromise>&& promise)
{
    m_consumer.setType(type);
    m_consumePromise = WTFMove(promise);
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
    if (m_consumer.hasData()) {
        m_consumer.resolve(WTFMove(promise));
        return;
    }
    if (isFormData()) {
        // FIXME: Support consuming FormData.
        promise->reject(0);
        return;
    }
    ASSERT_NOT_REACHED();
}

#if ENABLE(READABLE_STREAM_API)
void FetchBody::consumeAsStream(FetchBodyOwner& owner, FetchResponseSource& source)
{
    bool closeStream = false;
    if (isArrayBuffer()) {
        closeStream = source.enqueue(ArrayBuffer::tryCreate(arrayBufferBody().data(), arrayBufferBody().byteLength()));
        m_data = nullptr;
    } else if (isArrayBufferView()) {
        closeStream = source.enqueue(ArrayBuffer::tryCreate(arrayBufferViewBody().baseAddress(), arrayBufferViewBody().byteLength()));
        m_data = nullptr;
    } else if (isText()) {
        auto data = UTF8Encoding().encode(textBody(), EntitiesForUnencodables);
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.length()));
        m_data = nullptr;
    } else if (isURLSearchParams()) {
        auto data = UTF8Encoding().encode(urlSearchParamsBody().toString(), EntitiesForUnencodables);
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.length()));
        m_data = nullptr;
    } else if (isBlob()) {
        owner.loadBlob(blobBody(), nullptr);
        m_data = nullptr;
    } else if (m_consumer.hasData())
        closeStream = source.enqueue(m_consumer.takeAsArrayBuffer());
    else if (isEmpty())
        closeStream = true;
    else
        source.error(ASCIILiteral("not implemented"));

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
    auto data = UTF8Encoding().encode(text, EntitiesForUnencodables);
    m_consumer.resolveWithData(WTFMove(promise), reinterpret_cast<const uint8_t*>(data.data()), data.length());
    m_data = nullptr;
}

void FetchBody::consumeBlob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    m_consumePromise = WTFMove(promise);
    owner.loadBlob(blobBody(), &m_consumer);
    m_data = nullptr;
}

void FetchBody::loadingFailed()
{
    if (m_consumePromise) {
        m_consumePromise->reject(0);
        m_consumePromise = nullptr;
    }
}

void FetchBody::loadingSucceeded()
{
    m_isEmpty = !m_consumer.hasData();
    if (m_consumePromise)
        m_consumer.resolve(m_consumePromise.releaseNonNull());
}

RefPtr<FormData> FetchBody::bodyForInternalRequest(ScriptExecutionContext& context) const
{
    if (isEmpty())
        return nullptr;
    if (isText())
        return FormData::create(UTF8Encoding().encode(textBody(), EntitiesForUnencodables));
    if (isURLSearchParams())
        return FormData::create(UTF8Encoding().encode(urlSearchParamsBody().toString(), EntitiesForUnencodables));
    if (isBlob()) {
        RefPtr<FormData> body = FormData::create();
        body->appendBlob(blobBody().url());
        return body;
    }
    if (isArrayBuffer())
        return FormData::create(arrayBufferBody().data(), arrayBufferBody().byteLength());
    if (isArrayBufferView())
        return FormData::create(arrayBufferViewBody().baseAddress(), arrayBufferViewBody().byteLength());
    if (isFormData()) {
        ASSERT(!context.isWorkerGlobalScope());
        RefPtr<FormData> body = const_cast<FormData*>(&formDataBody());
        body->generateFiles(static_cast<Document*>(&context));
        return body;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

FetchBody FetchBody::clone() const
{
    ASSERT(!m_consumePromise);
    FetchBody clone(m_contentType, m_consumer);
    clone.m_isEmpty = m_isEmpty;

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
    return clone;
}

}

#endif // ENABLE(FETCH_API)
