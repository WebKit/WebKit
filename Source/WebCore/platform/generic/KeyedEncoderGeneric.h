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

#pragma once

#include "KeyedCoding.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FragmentedSharedBuffer;

class KeyedEncoderGeneric final : public KeyedEncoder {
public:

    enum class Type : uint8_t {
        Bytes,
        Bool,
        UInt32,
        UInt64,
        Int32,
        Int64,
        Float,
        Double,
        String,
        BeginObject,
        EndObject,
        BeginArray,
        BeginArrayElement,
        EndArrayElement,
        EndArray,
    };

private:
    RefPtr<SharedBuffer> finishEncoding() override;

    void encodeBytes(const String& key, const uint8_t*, size_t) override;
    void encodeBool(const String& key, bool) override;
    void encodeUInt32(const String& key, uint32_t) override;
    void encodeUInt64(const String& key, uint64_t) override;
    void encodeInt32(const String& key, int32_t) override;
    void encodeInt64(const String& key, int64_t) override;
    void encodeFloat(const String& key, float) override;
    void encodeDouble(const String& key, double) override;
    void encodeString(const String& key, const String&) override;

    void beginObject(const String& key) override;
    void endObject() override;

    void beginArray(const String& key) override;
    void beginArrayElement() override;
    void endArrayElement() override;
    void endArray() override;

    void encodeString(const String&);

    WTF::Persistence::Encoder m_encoder;
};

} // namespace WebCore

namespace WTF {
template<> struct EnumTraitsForPersistence<WebCore::KeyedEncoderGeneric::Type> {
    using values = EnumValues<
        WebCore::KeyedEncoderGeneric::Type,
        WebCore::KeyedEncoderGeneric::Type::Bytes,
        WebCore::KeyedEncoderGeneric::Type::Bool,
        WebCore::KeyedEncoderGeneric::Type::UInt32,
        WebCore::KeyedEncoderGeneric::Type::UInt64,
        WebCore::KeyedEncoderGeneric::Type::Int32,
        WebCore::KeyedEncoderGeneric::Type::Int64,
        WebCore::KeyedEncoderGeneric::Type::Float,
        WebCore::KeyedEncoderGeneric::Type::Double,
        WebCore::KeyedEncoderGeneric::Type::String,
        WebCore::KeyedEncoderGeneric::Type::BeginObject,
        WebCore::KeyedEncoderGeneric::Type::EndObject,
        WebCore::KeyedEncoderGeneric::Type::BeginArray,
        WebCore::KeyedEncoderGeneric::Type::BeginArrayElement,
        WebCore::KeyedEncoderGeneric::Type::EndArrayElement,
        WebCore::KeyedEncoderGeneric::Type::EndArray
    >;
};

} // namespace WTF
