/*
 * Copyright (C) 2016 Apple Inc.
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
 * 3.  Neither the name of Apple Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchBodyConsumer.h"

#include "JSBlob.h"
#include "TextResourceDecoder.h"

namespace WebCore {

static inline Ref<Blob> blobFromData(const unsigned char* data, unsigned length, const String& contentType)
{
    Vector<uint8_t> value(length);
    memcpy(value.data(), data, length);
    return Blob::create(WTFMove(value), contentType);
}

static inline bool shouldPrependBOM(const unsigned char* data, unsigned length)
{
    if (length < 3)
        return true;
    return data[0] != 0xef || data[1] != 0xbb || data[2] != 0xbf;
}

static String textFromUTF8(const unsigned char* data, unsigned length)
{
    auto decoder = TextResourceDecoder::create("text/plain", "UTF-8");
    if (shouldPrependBOM(data, length))
        decoder->decode("\xef\xbb\xbf", 3);
    return decoder->decodeAndFlush(reinterpret_cast<const char*>(data), length);
}

void FetchBodyConsumer::resolveWithData(Ref<DeferredPromise>&& promise, const unsigned char* data, unsigned length)
{
    switch (m_type) {
    case Type::ArrayBuffer:
        fulfillPromiseWithArrayBuffer(WTFMove(promise), data, length);
        return;
    case Type::Blob:
        promise->resolveWithNewlyCreated<IDLInterface<Blob>>(blobFromData(data, length, m_contentType).get());
        return;
    case Type::JSON:
        fulfillPromiseWithJSON(WTFMove(promise), textFromUTF8(data, length));
        return;
    case Type::Text:
        promise->resolve<IDLDOMString>(textFromUTF8(data, length));
        return;
    case Type::None:
        ASSERT_NOT_REACHED();
        return;
    }
}

void FetchBodyConsumer::resolve(Ref<DeferredPromise>&& promise)
{
    ASSERT(m_type != Type::None);
    switch (m_type) {
    case Type::ArrayBuffer:
        fulfillPromiseWithArrayBuffer(WTFMove(promise), takeAsArrayBuffer().get());
        return;
    case Type::Blob:
        promise->resolveWithNewlyCreated<IDLInterface<Blob>>(takeAsBlob().get());
        return;
    case Type::JSON:
        fulfillPromiseWithJSON(WTFMove(promise), takeAsText());
        return;
    case Type::Text:
        promise->resolve<IDLDOMString>(takeAsText());
        return;
    case Type::None:
        ASSERT_NOT_REACHED();
        return;
    }
}

void FetchBodyConsumer::append(const char* data, unsigned length)
{
    if (!m_buffer) {
        m_buffer = SharedBuffer::create(data, length);
        return;
    }
    m_buffer->append(data, length);
}

void FetchBodyConsumer::append(const unsigned char* data, unsigned length)
{
    append(reinterpret_cast<const char*>(data), length);
}

RefPtr<SharedBuffer> FetchBodyConsumer::takeData()
{
    return WTFMove(m_buffer);
}

RefPtr<JSC::ArrayBuffer> FetchBodyConsumer::takeAsArrayBuffer()
{
    if (!m_buffer)
        return ArrayBuffer::tryCreate(nullptr, 0);

    auto arrayBuffer = m_buffer->tryCreateArrayBuffer();
    m_buffer = nullptr;
    return arrayBuffer;
}

Ref<Blob> FetchBodyConsumer::takeAsBlob()
{
    if (!m_buffer)
        return Blob::create(Vector<uint8_t>(), m_contentType);

    // FIXME: We should try to move m_buffer to Blob without doing extra copy.
    return blobFromData(reinterpret_cast<const unsigned char*>(m_buffer->data()), m_buffer->size(), m_contentType);
}

String FetchBodyConsumer::takeAsText()
{
    // FIXME: We could probably text decode on the fly as soon as m_type is set to JSON or Text.
    if (!m_buffer)
        return String();

    auto text = textFromUTF8(reinterpret_cast<const unsigned char*>(m_buffer->data()), m_buffer->size());
    m_buffer = nullptr;
    return text;
}

} // namespace WebCore
