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

#if USE(GLIB)
#include <glib.h>
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebCore {

enum class KeyPathType { Null, String, Array };

RefPtr<SharedBuffer> serializeIDBKeyPath(const std::optional<IDBKeyPath>& keyPath)
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

bool deserializeIDBKeyPath(const uint8_t* data, size_t size, std::optional<IDBKeyPath>& result)
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

static bool isLegacySerializedIDBKeyData(const uint8_t* data, size_t size)
{
#if USE(CF)
    UNUSED_PARAM(size);

    // This is the magic character that begins serialized PropertyLists, and tells us whether
    // the key we're looking at is an old-style key.
    static const uint8_t legacySerializedKeyVersion = 'b';
    if (data[0] == legacySerializedKeyVersion)
        return true;
#elif USE(GLIB)
    // KeyedEncoderGLib uses a GVariant dictionary, so check if the given data is a valid GVariant dictionary.
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(data, size));
    GRefPtr<GVariant> variant = g_variant_new_from_bytes(G_VARIANT_TYPE("a{sv}"), bytes.get(), FALSE);
    return g_variant_is_normal_form(variant.get());
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(size);
#endif
    return false;
}


/*
The IDBKeyData serialization format is as follows:
[1 byte version header][Key Buffer]

The Key Buffer serialization format is as follows:
[1 byte key type][Type specific data]

Type specific serialization formats are as follows for each of the types:
Min:
[0 bytes]

Number:
[8 bytes representing a double encoded in little endian]

Date:
[8 bytes representing a double encoded in little endian]

String:
[4 bytes representing string "length" in little endian]["length" number of 2-byte pairs representing ECMAScript 16-bit code units]

Binary:
[8 bytes representing the "size" of the binary blob]["size" bytes]

Array:
[8 bytes representing the "length" of the key array]["length" individual Key Buffer entries]

Max:
[0 bytes]
*/

static const uint8_t SIDBKeyVersion = 0x00;
enum class SIDBKeyType : uint8_t {
    Min = 0x00,
    Number = 0x20,
    Date = 0x40,
    String = 0x60,
    Binary = 0x80,
    Array = 0xA0,
    Max = 0xFF,
};

static SIDBKeyType serializedTypeForKeyType(IndexedDB::KeyType type)
{
    switch (type) {
    case IndexedDB::KeyType::Min:
        return SIDBKeyType::Min;
    case IndexedDB::KeyType::Number:
        return SIDBKeyType::Number;
    case IndexedDB::KeyType::Date:
        return SIDBKeyType::Date;
    case IndexedDB::KeyType::String:
        return SIDBKeyType::String;
    case IndexedDB::KeyType::Binary:
        return SIDBKeyType::Binary;
    case IndexedDB::KeyType::Array:
        return SIDBKeyType::Array;
    case IndexedDB::KeyType::Max:
        return SIDBKeyType::Max;
    case IndexedDB::KeyType::Invalid:
        RELEASE_ASSERT_NOT_REACHED();
    };

    RELEASE_ASSERT_NOT_REACHED();
}

#if CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN) || CPU(NEEDS_ALIGNED_ACCESS)
template <typename T> static void writeLittleEndian(Vector<char>& buffer, T value)
{
    for (unsigned i = 0; i < sizeof(T); i++) {
        buffer.append(value & 0xFF);
        value >>= 8;
    }
}

template <typename T> static bool readLittleEndian(const uint8_t*& ptr, const uint8_t* end, T& value)
{
    if (ptr > end - sizeof(value))
        return false;

    value = 0;
    for (size_t i = 0; i < sizeof(T); i++)
        value += ((T)*ptr++) << (i * 8);
    return true;
}
#else
template <typename T> static void writeLittleEndian(Vector<char>& buffer, T value)
{
    buffer.append(reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

template <typename T> static bool readLittleEndian(const uint8_t*& ptr, const uint8_t* end, T& value)
{
    if (ptr > end - sizeof(value))
        return false;

    value = *reinterpret_cast<const T*>(ptr);
    ptr += sizeof(T);

    return true;
}
#endif

static void writeDouble(Vector<char>& data, double d)
{
    writeLittleEndian(data, *reinterpret_cast<uint64_t*>(&d));
}

static bool readDouble(const uint8_t*& data, const uint8_t* end, double& d)
{
    return readLittleEndian(data, end, *reinterpret_cast<uint64_t*>(&d));
}

static void encodeKey(Vector<char>& data, const IDBKeyData& key)
{
    SIDBKeyType type = serializedTypeForKeyType(key.type());
    data.append(static_cast<char>(type));

    switch (type) {
    case SIDBKeyType::Number:
        writeDouble(data, key.number());
        break;
    case SIDBKeyType::Date:
        writeDouble(data, key.date());
        break;
    case SIDBKeyType::String: {
        auto string = key.string();
        uint32_t length = string.length();
        writeLittleEndian(data, length);

        for (size_t i = 0; i < length; ++i)
            writeLittleEndian(data, string[i]);

        break;
    }
    case SIDBKeyType::Binary: {
        auto& buffer = key.binary();
        uint64_t size = buffer.size();
        writeLittleEndian(data, size);

        auto* bufferData = buffer.data();
        ASSERT(bufferData || !size);
        if (bufferData)
            data.append(bufferData->data(), bufferData->size());

        break;
    }
    case SIDBKeyType::Array: {
        auto& array = key.array();
        uint64_t size = array.size();
        writeLittleEndian(data, size);
        for (auto& key : array)
            encodeKey(data, key);

        break;
    }
    case SIDBKeyType::Min:
    case SIDBKeyType::Max:
        break;
    }
}

RefPtr<SharedBuffer> serializeIDBKeyData(const IDBKeyData& key)
{
    Vector<char> data;
    data.append(SIDBKeyVersion);

    encodeKey(data, key);
    return SharedBuffer::create(WTFMove(data));
}

static bool decodeKey(const uint8_t*& data, const uint8_t* end, IDBKeyData& result)
{
    if (!data || data >= end)
        return false;

    SIDBKeyType type = static_cast<SIDBKeyType>(data++[0]);
    switch (type) {
    case SIDBKeyType::Min:
        result = IDBKeyData::minimum();
        return true;
    case SIDBKeyType::Max:
        result = IDBKeyData::maximum();
        return true;
    case SIDBKeyType::Number: {
        double d;
        if (!readDouble(data, end, d))
            return false;

        result.setNumberValue(d);
        return true;
    }
    case SIDBKeyType::Date: {
        double d;
        if (!readDouble(data, end, d))
            return false;

        result.setDateValue(d);
        return true;
    }
    case SIDBKeyType::String: {
        uint32_t length;
        if (!readLittleEndian(data, end, length))
            return false;

        if (static_cast<uint64_t>(end - data) < length * 2)
            return false;

        Vector<UChar> buffer;
        buffer.reserveInitialCapacity(length);
        for (size_t i = 0; i < length; i++) {
            uint16_t ch;
            if (!readLittleEndian(data, end, ch))
                return false;
            buffer.uncheckedAppend(ch);
        }

        result.setStringValue(String::adopt(WTFMove(buffer)));

        return true;
    }
    case SIDBKeyType::Binary: {
        uint64_t size64;
        if (!readLittleEndian(data, end, size64))
            return false;

        if (static_cast<uint64_t>(end - data) < size64)
            return false;

        if (size64 > std::numeric_limits<size_t>::max())
            return false;

        size_t size = static_cast<size_t>(size64);
        Vector<uint8_t> dataVector;

        dataVector.append(data, size);
        data += size;

        result.setBinaryValue(ThreadSafeDataBuffer::create(WTFMove(dataVector)));
        return true;
    }
    case SIDBKeyType::Array: {
        uint64_t size64;
        if (!readLittleEndian(data, end, size64))
            return false;

        if (size64 > std::numeric_limits<size_t>::max())
            return false;

        size_t size = static_cast<size_t>(size64);
        Vector<IDBKeyData> array;
        array.reserveInitialCapacity(size);

        for (size_t i = 0; i < size; ++i) {
            IDBKeyData keyData;
            if (!decodeKey(data, end, keyData))
                return false;

            ASSERT(keyData.isValid());
            array.uncheckedAppend(WTFMove(keyData));
        }

        result.setArrayValue(array);

        return true;
    }
    default:
        LOG_ERROR("decodeKey encountered unexpected type: %i", (int)type);
        return false;
    }
}

bool deserializeIDBKeyData(const uint8_t* data, size_t size, IDBKeyData& result)
{
    if (!data || !size)
        return false;

    if (isLegacySerializedIDBKeyData(data, size)) {
        auto decoder = KeyedDecoder::decoder(data, size);
        return IDBKeyData::decode(*decoder, result);
    }

    // Verify this is a SerializedIDBKey version we understand.
    const uint8_t* current = data;
    const uint8_t* end = data + size;
    if (current++[0] != SIDBKeyVersion)
        return false;

    if (decodeKey(current, end, result)) {
        // Even if we successfully decoded a key, the deserialize is only successful
        // if we actually consumed all input data.
        return current == end;
    }

    return false;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
