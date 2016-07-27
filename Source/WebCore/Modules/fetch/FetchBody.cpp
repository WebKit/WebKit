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
#include "FetchResponseSource.h"
#include "FormData.h"
#include "HTTPParsers.h"
#include "JSBlob.h"
#include "JSDOMFormData.h"
#include "JSReadableStream.h"
#include "ReadableStreamSource.h"

namespace WebCore {

FetchBody::FetchBody(Ref<Blob>&& blob)
    : m_type(Type::Blob)
    , m_mimeType(blob->type())
    , m_blob(WTFMove(blob))
{
}

FetchBody::FetchBody(Ref<DOMFormData>&& formData)
    : m_type(Type::FormData)
    , m_mimeType(ASCIILiteral("multipart/form-data"))
    , m_formData(WTFMove(formData))
{
    // FIXME: Handle the boundary parameter of multipart/form-data MIME type.
}

FetchBody::FetchBody(String&& text)
    : m_type(Type::Text)
    , m_mimeType(ASCIILiteral("text/plain;charset=UTF-8"))
    , m_text(WTFMove(text))
{
}

FetchBody FetchBody::extract(JSC::ExecState& state, JSC::JSValue value)
{
    if (value.inherits(JSBlob::info()))
        return FetchBody(*JSBlob::toWrapped(value));
    if (value.inherits(JSDOMFormData::info()))
        return FetchBody(*JSDOMFormData::toWrapped(value));
    if (value.isString())
        return FetchBody(value.toWTFString(&state));
    if (value.inherits(JSReadableStream::info()))
        return { Type::ReadableStream };
    return { };
}

FetchBody FetchBody::extractFromBody(FetchBody* body)
{
    if (!body)
        return { };

    return FetchBody(WTFMove(*body));
}

void FetchBody::arrayBuffer(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    ASSERT(m_type != Type::None);
    m_consumer.setType(FetchBodyConsumer::Type::ArrayBuffer);
    consume(owner, WTFMove(promise));
}

void FetchBody::blob(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    ASSERT(m_type != Type::None);
    m_consumer.setType(FetchBodyConsumer::Type::Blob);
    m_consumer.setContentType(Blob::normalizedContentType(extractMIMETypeFromMediaType(m_mimeType)));
    consume(owner, WTFMove(promise));
}

void FetchBody::json(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    ASSERT(m_type != Type::None);

    if (m_type == Type::Text) {
        fulfillPromiseWithJSON(promise, m_text);
        return;
    }
    m_consumer.setType(FetchBodyConsumer::Type::JSON);
    consume(owner, WTFMove(promise));
}

void FetchBody::text(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    ASSERT(m_type != Type::None);

    if (m_type == Type::Text) {
        promise.resolve(m_text);
        return;
    }
    m_consumer.setType(FetchBodyConsumer::Type::Text);
    consume(owner, WTFMove(promise));
}

void FetchBody::consume(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    // This should be handled by FetchBodyOwner
    ASSERT(m_type != Type::None);
    // This should be handled by JS built-ins
    ASSERT(m_type != Type::ReadableStream);

    switch (m_type) {
    case Type::ArrayBuffer:
        consumeArrayBuffer(promise);
        return;
    case Type::Text:
        consumeText(promise);
        return;
    case Type::Blob:
        consumeBlob(owner, WTFMove(promise));
        return;
    case Type::Loading:
        m_consumePromise = WTFMove(promise);
        return;
    case Type::Loaded:
        m_consumer.resolve(promise);
        return;
    default:
        // FIXME: Support other types.
        promise.reject(0);
    }
}

#if ENABLE(STREAMS_API)
void FetchBody::consumeAsStream(FetchBodyOwner& owner, FetchResponseSource& source)
{
    // This should be handled by FetchResponse
    ASSERT(m_type != Type::Loading);
    // This should be handled by JS built-ins
    ASSERT(m_type != Type::ReadableStream);

    bool closeStream = false;
    switch (m_type) {
    case Type::ArrayBuffer:
        ASSERT(m_data);
        closeStream = source.enqueue(RefPtr<JSC::ArrayBuffer>(m_data));
        break;
    case Type::Text: {
        Vector<uint8_t> data = extractFromText();
        closeStream = source.enqueue(ArrayBuffer::tryCreate(data.data(), data.size()));
        break;
    }
    case Type::Blob:
        ASSERT(m_blob);
        owner.loadBlob(*m_blob, nullptr);
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

void FetchBody::consumeArrayBuffer(DeferredWrapper& promise)
{
    m_consumer.resolveWithData(promise, static_cast<const uint8_t*>(m_data->data()), m_data->byteLength());
}

void FetchBody::consumeText(DeferredWrapper& promise)
{
    Vector<uint8_t> data = extractFromText();
    m_consumer.resolveWithData(promise, data.data(), data.size());
}

void FetchBody::consumeBlob(FetchBodyOwner& owner, DeferredWrapper&& promise)
{
    ASSERT(m_blob);

    m_consumePromise = WTFMove(promise);
    owner.loadBlob(*m_blob, &m_consumer);
}

Vector<uint8_t> FetchBody::extractFromText() const
{
    ASSERT(m_type == Type::Text);
    // FIXME: This double allocation is not efficient. Might want to fix that at WTFString level.
    CString data = m_text.utf8();
    Vector<uint8_t> value(data.length());
    memcpy(value.data(), data.data(), data.length());
    return value;
}

void FetchBody::loadingFailed()
{
    if (m_consumePromise) {
        m_consumePromise->reject(0);
        m_consumePromise = Nullopt;
    }
}

void FetchBody::loadingSucceeded()
{
    m_type = m_consumer.hasData() ? Type::Loaded : Type::None;
    if (m_consumePromise) {
        m_consumer.resolve(*m_consumePromise);
        m_consumePromise = Nullopt;
    }
}

RefPtr<FormData> FetchBody::bodyForInternalRequest() const
{
    if (m_type == Type::None)
        return nullptr;
    if (m_type == Type::Text)
        return FormData::create(UTF8Encoding().encode(m_text, EntitiesForUnencodables));
    if (m_type == Type::Blob) {
        RefPtr<FormData> body = FormData::create();
        body->appendBlob(m_blob->url());
        return body;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

}

#endif // ENABLE(FETCH_API)
