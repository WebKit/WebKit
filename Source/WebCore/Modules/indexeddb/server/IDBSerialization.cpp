/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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
#include "IDBSerialization.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "KeyedCoding.h"

namespace WebCore {

enum class KeyPathType { Null, String, Array };

RefPtr<SharedBuffer> serializeIDBKeyPath(const Optional<IDBKeyPath>& keyPath)
{
    auto encoder = KeyedEncoder::encoder();

    if (keyPath) {
        auto visitor = WTF::makeVisitor([&](const String& string) {
            encoder->encodeEnum("type", KeyPathType::String);
            encoder->encodeString("string", string);
        }, [&](const Vector<String>& vector) {
            encoder->encodeEnum("type", KeyPathType::Array);
            encoder->encodeObjects("array", vector.begin(), vector.end(), [](WebCore::KeyedEncoder& encoder, const String& string) {
                encoder.encodeString("string", string);
            });
        });
        WTF::visit(visitor, keyPath.value());
    } else
        encoder->encodeEnum("type", KeyPathType::Null);

    return encoder->finishEncoding();
}

bool deserializeIDBKeyPath(const uint8_t* data, size_t size, Optional<IDBKeyPath>& result)
{
    if (!data || !size)
        return false;

    auto decoder = KeyedDecoder::decoder(data, size);

    KeyPathType type;
    bool succeeded = decoder->decodeEnum("type", type, [](KeyPathType value) {
        return value == KeyPathType::Null || value == KeyPathType::String || value == KeyPathType::Array;
    });
    if (!succeeded)
        return false;

    switch (type) {
    case KeyPathType::Null:
        break;
    case KeyPathType::String: {
        String string;
        if (!decoder->decodeString("string", string))
            return false;
        result = IDBKeyPath(WTFMove(string));
        break;
    }
    case KeyPathType::Array: {
        Vector<String> vector;
        succeeded = decoder->decodeObjects("array", vector, [](KeyedDecoder& decoder, String& result) {
            return decoder.decodeString("string", result);
        });
        if (!succeeded)
            return false;
        result = IDBKeyPath(WTFMove(vector));
        break;
    }
    }
    return true;
}

RefPtr<SharedBuffer> serializeIDBKeyData(const IDBKeyData& key)
{
    auto encoder = KeyedEncoder::encoder();
    key.encode(*encoder);
    return encoder->finishEncoding();
}

bool deserializeIDBKeyData(const uint8_t* data, size_t size, IDBKeyData& result)
{
    if (!data || !size)
        return false;

    auto decoder = KeyedDecoder::decoder(data, size);
    return IDBKeyData::decode(*decoder, result);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
