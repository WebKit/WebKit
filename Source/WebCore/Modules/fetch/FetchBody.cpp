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
    : m_type(Type::Blob)
    , m_data(WTFMove(blob))
    , m_contentType(this->blobBody().type())
{
}

FetchBody::FetchBody(DOMFormData& formData, Document& document)
    : m_type(Type::FormData)
    , m_data(FormData::createMultiPart(formData, formData.encoding(), &document))
    , m_contentType(makeString("multipart/form-data;boundary=", this->formDataBody().boundary().data()))
{
}

FetchBody::FetchBody(String&& text)
    : m_type(Type::Text)
    , m_data(WTFMove(text))
    , m_contentType(ASCIILiteral("text/plain;charset=UTF-8"))
{
}

FetchBody::FetchBody(Ref<const ArrayBuffer>&& data)
    : m_type(Type::ArrayBuffer)
    , m_data(WTFMove(data))
{
}

FetchBody::FetchBody(Ref<const ArrayBufferView>&& dataView)
    : m_type(Type::ArrayBufferView)
    , m_data(WTFMove(dataView))
{
}

FetchBody::FetchBody(Ref<const URLSearchParams>&& url)
    : m_type(Type::URLSeachParams)
    , m_data(WTFMove(url))
    , m_contentType(ASCIILiteral("application/x-www-form-urlencoded;charset=UTF-8"))
{
}

FetchBody::FetchBody(Type type, const String& contentType, const FetchBodyConsumer& consumer)
    : m_type(type)
    , m_contentType(contentType)
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
    if (value.inherits(JSReadableStream::info()))
        return { Type::ReadableStream };
    if (value.inherits(JSC::JSArrayBuffer::info())) {
        ArrayBuffer* data = toArrayBuffer(value);
        ASSERT(data);
        return { *data };
    }
    if (value.inherits(JSC::JSArrayBufferView::info())) {
        auto data = toArrayBufferView(value);
        ASSERT(data);
        // FIXME: We should be able to efficiently get a Ref<const T> from a RefPtr<T>.
        return { *data };
    }
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
    ASSERT(m_type != Type::None);
    m_consumer.setType(FetchBodyConsumer::Type::ArrayBuffer);
    consume(owner, WTFMove(promise));
}

void FetchBody::blob(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    ASSERT(m_type != Type::None);
    m_consumer.setType(FetchBodyConsumer::Type::Blob);
    m_consumer.setContentType(Blob::normalizedContentType(extractMIMETypeFromMediaType(m_contentType)));
    consume(owner, WTFMove(promise));
}

void FetchBody::json(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    ASSERT(m_type != Type::None);

    if (m_type == Type::Text) {
        fulfillPromiseWithJSON(WTFMove(promise), textBody());
        return;
    }
    m_consumer.setType(FetchBodyConsumer::Type::JSON);
    consume(owner, WTFMove(promise));
}

void FetchBody::text(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    ASSERT(m_type != Type::None);

    if (m_type == Type::Text) {
        promise->resolve(textBody());
        return;
    }
    m_consumer.setType(FetchBodyConsumer::Type::Text);
    consume(owner, WTFMove(promise));
}

void FetchBody::consume(FetchBodyOwner& owner, Ref<DeferredPromise>&& promise)
{
    // This should be handled by FetchBodyOwner
    ASSERT(m_type != Type::None);
    // This should be handled by JS built-ins
    ASSERT(m_type != Type::ReadableStream);

    switch (m_type) {
    case Type::ArrayBuffer:
        consumeArrayBuffer(WTFMove(promise));
        return;
    case Type::ArrayBufferView:
        consumeArrayBufferView(WTFMove(promise));
        return;
    case Type::Text:
        consumeText(WTFMove(promise), textBody());
        return;
    case Type::URLSeachParams:
        consumeText(WTFMove(promise), urlSearchParamsBody().toString());
        return;
    case Type::Blob:
        consumeBlob(owner, WTFMove(promise));
        return;
    case Type::Loading:
        m_consumePromise = WTFMove(promise);
        return;
    case Type::Loaded:
        m_consumer.resolve(WTFMove(promise));
        return;
    case Type::FormData:
        // FIXME: Support consuming FormData.
        promise->reject(0);
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

#if ENABLE(READABLE_STREAM_API)
void FetchBody::consumeAsStream(FetchBodyOwner& owner, FetchResponseSource& source)
{
    // This should be handled by FetchResponse
    ASSERT(m_type != Type::Loading);
    // This should be handled by JS built-ins
    ASSERT(m_type != Type::ReadableStream);

    bool closeStream = false;
    switch (m_type) {
    case Type::ArrayBuffer:
        closeStream = source.enqueue(ArrayBuffer::tryCreate(arrayBufferBody().data(), arrayBufferBody().byteLength()));
        m_data = nullptr;
        break;
    case Type::ArrayBufferView: {
        closeStream = source.enqueue(ArrayBuffer::tryCreate(arrayBufferViewBody().baseAddress(), arrayBufferViewBody().byteLength()));
        m_data = nullptr;
        break;
    }
    case Type::Text: {
        auto data = UTF8Encoding().encode(textBody(), EntitiesForUnencodables);
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.length()));
        m_data = nullptr;
        break;
    }
    case Type::URLSeachParams: {
        auto data = UTF8Encoding().encode(urlSearchParamsBody().toString(), EntitiesForUnencodables);
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.length()));
        m_data = nullptr;
        break;
    }
    case Type::Blob:
        owner.loadBlob(blobBody(), nullptr);
        m_data = nullptr;
        break;
    case Type::None:
        closeStream = true;
        break;
    case Type::Loaded: {
        closeStream = source.enqueue(m_consumer.takeAsArrayBuffer());
        break;
    }
    default:
        source.error(ASCIILiteral("not implemented"));
    }

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
    m_type = m_consumer.hasData() ? Type::Loaded : Type::None;
    if (m_consumePromise)
        m_consumer.resolve(m_consumePromise.releaseNonNull());
}

RefPtr<FormData> FetchBody::bodyForInternalRequest(ScriptExecutionContext& context) const
{
    switch (m_type) {
    case Type::None:
        return nullptr;
    case Type::Text:
        return FormData::create(UTF8Encoding().encode(textBody(), EntitiesForUnencodables));
    case Type::URLSeachParams:
        return FormData::create(UTF8Encoding().encode(urlSearchParamsBody().toString(), EntitiesForUnencodables));
    case Type::Blob: {
        RefPtr<FormData> body = FormData::create();
        body->appendBlob(blobBody().url());
        return body;
    }
    case Type::ArrayBuffer:
        return FormData::create(arrayBufferBody().data(), arrayBufferBody().byteLength());
    case Type::ArrayBufferView:
        return FormData::create(arrayBufferViewBody().baseAddress(), arrayBufferViewBody().byteLength());
    case Type::FormData: {
        ASSERT(!context.isWorkerGlobalScope());
        RefPtr<FormData> body = const_cast<FormData*>(&formDataBody());
        body->generateFiles(static_cast<Document*>(&context));
        return body;
    }
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

FetchBody FetchBody::clone() const
{
    ASSERT(!m_consumePromise);
    FetchBody clone(m_type, m_contentType, m_consumer);

    switch (m_type) {
    case Type::ArrayBuffer:
        clone.m_data = arrayBufferBody();
        break;
    case Type::ArrayBufferView:
        clone.m_data = arrayBufferViewBody();
        break;
    case Type::Blob:
        clone.m_data = blobBody();
        break;
    case Type::FormData:
        clone.m_data = const_cast<FormData&>(formDataBody());
        break;
    case Type::Text:
        clone.m_data = textBody();
        break;
    case Type::URLSeachParams:
        clone.m_data = urlSearchParamsBody();
        break;
    case Type::Loaded:
    case Type::None:
    case Type::Loading:
    case Type::ReadableStream:
        break;
    }
    return clone;
}

}

#endif // ENABLE(FETCH_API)
