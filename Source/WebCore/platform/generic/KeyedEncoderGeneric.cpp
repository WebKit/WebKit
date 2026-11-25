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
#include <wtf/StdLibExtras.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WebCore {

std::unique_ptr<KeyedEncoder> KeyedEncoder::encoder()
{
    return makeUnique<KeyedEncoderGeneric>();
}

void KeyedEncoderGeneric::encodeString(StringImpl* key)
{
    auto result = tryGetUTF8(key, [&](std::span<const char8_t> span) -> bool {
        m_encoder << span.size();
        m_encoder.encodeFixedLengthData(byteCast<uint8_t>(span));
        return true;
    });
    RELEASE_ASSERT(result);
}

void KeyedEncoderGeneric::encodeBytes(StringImpl* key, std::span<const uint8_t> bytes)
{
    m_encoder << Type::Bytes;
    encodeString(key);
    m_encoder << bytes.size();
    m_encoder.encodeFixedLengthData(bytes);
}

void KeyedEncoderGeneric::encodeBool(StringImpl* key, bool value)
{
    m_encoder << Type::Bool;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeUInt32(StringImpl* key, uint32_t value)
{
    m_encoder << Type::UInt32;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeUInt64(StringImpl* key, uint64_t value)
{
    m_encoder << Type::UInt64;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeInt32(StringImpl* key, int32_t value)
{
    m_encoder << Type::Int32;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeInt64(StringImpl* key, int64_t value)
{
    m_encoder << Type::Int64;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeFloat(StringImpl* key, float value)
{
    m_encoder << Type::Float;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeDouble(StringImpl* key, double value)
{
    m_encoder << Type::Double;
    encodeString(key);
    m_encoder << value;
}

void KeyedEncoderGeneric::encodeString(StringImpl* key, StringImpl* value)
{
    m_encoder << Type::String;
    encodeString(key);
    encodeString(value);
}

void KeyedEncoderGeneric::beginObject(StringImpl* key)
{
    m_encoder << Type::BeginObject;
    encodeString(key);
}

void KeyedEncoderGeneric::endObject()
{
    m_encoder << Type::EndObject;
}

void KeyedEncoderGeneric::beginArray(StringImpl* key)
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
    return SharedBuffer::create(m_encoder.span());
}

} // namespace WebCore
