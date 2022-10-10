/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "KeyedEncoderGeneric.h"

#include "SharedBuffer.h"
#include <wtf/persistence/PersistentEncoder.h>

namespace WebCore {

std::unique_ptr<KeyedEncoder> KeyedEncoder::encoder()
{
    return makeUnique<KeyedEncoderGeneric>();
}

void KeyedEncoderGeneric::encodeString(const String& key)
{
    auto result = key.tryGetUTF8([&](Span<const char> span) -> bool {
        m_encoder << span.size();
        m_encoder.encodeFixedLengthData({ bitwise_cast<const uint8_t*>(span.data()), span.size() });
        return true;
    });
    RELEASE_ASSERT(result);
}

void KeyedEncoderGeneric::encodeBytes(const String& key, const uint8_t* bytes, size_t size)
{
    m_encoder << Type::Bytes;
    encodeString(key);
    m_encoder << size;
    m_encoder.encodeFixedLengthData({ bytes, size });
}

void KeyedEncoderGeneric::encodeBool(const String& key, bool value)
{
    m_encoder << Type::Bool;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeUInt32(const String& key, uint32_t value)
{
    m_encoder << Type::UInt32;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeUInt64(const String& key, uint64_t value)
{
    m_encoder << Type::UInt64;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeInt32(const String& key, int32_t value)
{
    m_encoder << Type::Int32;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeInt64(const String& key, int64_t value)
{
    m_encoder << Type::Int64;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeFloat(const String& key, float value)
{
    m_encoder << Type::Float;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeDouble(const String& key, double value)
{
    m_encoder << Type::Double;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeString(const String& key, const String& value)
{
    m_encoder << Type::String;
    encodeString(key);
    encodeString(value);
}

void KeyedEncoderGeneric::beginObject(const String& key)
{
    m_encoder << Type::BeginObject;
    encodeString(key);
}

void KeyedEncoderGeneric::endObject()
{
    m_encoder << Type::EndObject;
}

void KeyedEncoderGeneric::beginArray(const String& key)
{
    m_encoder << Type::BeginArray;
    encodeString(key);
}

void KeyedEncoderGeneric::beginArrayElement()
{
    m_encoder << Type::BeginArrayElement;
}

void KeyedEncoderGeneric::endArrayElement()
{
    m_encoder << Type::EndArrayElement;
}

void KeyedEncoderGeneric::endArray()
{
    m_encoder << Type::EndArray;
}

RefPtr<SharedBuffer> KeyedEncoderGeneric::finishEncoding()
{
    return SharedBuffer::create(m_encoder.buffer(), m_encoder.bufferSize());
}

} // namespace WebCore
