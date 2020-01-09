/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"
#include <wtf/StdSet.h>
#include <wtf/Variant.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class KeyedDecoder;
class KeyedEncoder;

class IDBKeyData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IDBKeyData()
        : m_type(IndexedDB::KeyType::Invalid)
        , m_isNull(true)
    {
    }

    WEBCORE_EXPORT IDBKeyData(const IDBKey*);

    enum IsolatedCopyTag { IsolatedCopy };
    IDBKeyData(const IDBKeyData&, IsolatedCopyTag);

    static IDBKeyData minimum()
    {
        IDBKeyData result;
        result.m_type = IndexedDB::KeyType::Min;
        result.m_isNull = false;
        return result;
    }

    static IDBKeyData maximum()
    {
        IDBKeyData result;
        result.m_type = IndexedDB::KeyType::Max;
        result.m_isNull = false;
        return result;
    }

    WEBCORE_EXPORT RefPtr<IDBKey> maybeCreateIDBKey() const;

    WEBCORE_EXPORT IDBKeyData isolatedCopy() const;

    WEBCORE_EXPORT void encode(KeyedEncoder&) const;
    WEBCORE_EXPORT static bool decode(KeyedDecoder&, IDBKeyData&);

    // compare() has the same semantics as strcmp().
    //   - Returns negative if this IDBKeyData is less than other.
    //   - Returns positive if this IDBKeyData is greater than other.
    //   - Returns zero if this IDBKeyData is equal to other.
    WEBCORE_EXPORT int compare(const IDBKeyData& other) const;

    void setArrayValue(const Vector<IDBKeyData>&);
    void setBinaryValue(const ThreadSafeDataBuffer&);
    void setStringValue(const String&);
    void setDateValue(double);
    WEBCORE_EXPORT void setNumberValue(double);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<IDBKeyData> decode(Decoder&);
    
#if !LOG_DISABLED
    WEBCORE_EXPORT String loggingString() const;
#endif

    bool isNull() const { return m_isNull; }
    bool isValid() const;
    IndexedDB::KeyType type() const { return m_type; }

    bool operator<(const IDBKeyData&) const;
    bool operator>(const IDBKeyData& other) const
    {
        return !(*this < other) && !(*this == other);
    }

    bool operator<=(const IDBKeyData& other) const
    {
        return !(*this > other);
    }

    bool operator>=(const IDBKeyData& other) const
    {
        return !(*this < other);
    }

    bool operator==(const IDBKeyData& other) const;
    bool operator!=(const IDBKeyData& other) const
    {
        return !(*this == other);
    }

    unsigned hash() const
    {
        Vector<unsigned> hashCodes;
        hashCodes.append(static_cast<unsigned>(m_type));
        hashCodes.append(m_isNull ? 1 : 0);
        hashCodes.append(m_isDeletedValue ? 1 : 0);
        switch (m_type) {
        case IndexedDB::KeyType::Invalid:
        case IndexedDB::KeyType::Max:
        case IndexedDB::KeyType::Min:
            break;
        case IndexedDB::KeyType::Number:
        case IndexedDB::KeyType::Date:
            hashCodes.append(StringHasher::hashMemory<sizeof(double)>(&WTF::get<double>(m_value)));
            break;
        case IndexedDB::KeyType::String:
            hashCodes.append(StringHash::hash(WTF::get<String>(m_value)));
            break;
        case IndexedDB::KeyType::Binary: {
            auto* data = WTF::get<ThreadSafeDataBuffer>(m_value).data();
            if (!data)
                hashCodes.append(0);
            else
                hashCodes.append(StringHasher::hashMemory(data->data(), data->size()));
            break;
        }
        case IndexedDB::KeyType::Array:
            for (auto& key : WTF::get<Vector<IDBKeyData>>(m_value))
                hashCodes.append(key.hash());
            break;
        }

        return StringHasher::hashMemory(hashCodes.data(), hashCodes.size() * sizeof(unsigned));
    }

    static IDBKeyData deletedValue();
    bool isDeletedValue() const { return m_isDeletedValue; }

    String string() const
    {
        ASSERT(m_type == IndexedDB::KeyType::String);
        return WTF::get<String>(m_value);
    }

    double date() const
    {
        ASSERT(m_type == IndexedDB::KeyType::Date);
        return WTF::get<double>(m_value);
    }

    double number() const
    {
        ASSERT(m_type == IndexedDB::KeyType::Number);
        return WTF::get<double>(m_value);
    }

    const ThreadSafeDataBuffer& binary() const
    {
        ASSERT(m_type == IndexedDB::KeyType::Binary);
        return WTF::get<ThreadSafeDataBuffer>(m_value);
    }

    const Vector<IDBKeyData>& array() const
    {
        ASSERT(m_type == IndexedDB::KeyType::Array);
        return WTF::get<Vector<IDBKeyData>>(m_value);
    }

    size_t size() const;

private:
    static void isolatedCopy(const IDBKeyData& source, IDBKeyData& destination);

    IndexedDB::KeyType m_type;
    Variant<Vector<IDBKeyData>, String, double, ThreadSafeDataBuffer> m_value;

    bool m_isNull { false };
    bool m_isDeletedValue { false };
};

struct IDBKeyDataHash {
    static unsigned hash(const IDBKeyData& a) { return a.hash(); }
    static bool equal(const IDBKeyData& a, const IDBKeyData& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct IDBKeyDataHashTraits : public WTF::CustomHashTraits<IDBKeyData> {
    static const bool emptyValueIsZero = false;
    static const bool hasIsEmptyValueFunction = true;

    static void constructDeletedValue(IDBKeyData& key)
    {
        new (&key) IDBKeyData;
        key = IDBKeyData::deletedValue();
    }

    static bool isDeletedValue(const IDBKeyData& key)
    {
        return key.isDeletedValue();
    }

    static IDBKeyData emptyValue()
    {
        return IDBKeyData();
    }

    static bool isEmptyValue(const IDBKeyData& key)
    {
        return key.isNull();
    }
};

template<class Encoder>
void IDBKeyData::encode(Encoder& encoder) const
{
    encoder << m_isNull;
    if (m_isNull)
        return;

    encoder.encodeEnum(m_type);

    switch (m_type) {
    case IndexedDB::KeyType::Invalid:
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Min:
        break;
    case IndexedDB::KeyType::Array:
        encoder << WTF::get<Vector<IDBKeyData>>(m_value);
        break;
    case IndexedDB::KeyType::Binary:
        encoder << WTF::get<ThreadSafeDataBuffer>(m_value);
        break;
    case IndexedDB::KeyType::String:
        encoder << WTF::get<String>(m_value);
        break;
    case IndexedDB::KeyType::Date:
    case IndexedDB::KeyType::Number:
        encoder << WTF::get<double>(m_value);
        break;
    }
}

template<class Decoder>
Optional<IDBKeyData> IDBKeyData::decode(Decoder& decoder)
{
    IDBKeyData keyData;
    if (!decoder.decode(keyData.m_isNull))
        return WTF::nullopt;

    if (keyData.m_isNull)
        return keyData;

    if (!decoder.decodeEnum(keyData.m_type))
        return WTF::nullopt;

    switch (keyData.m_type) {
    case IndexedDB::KeyType::Invalid:
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Min:
        break;
    case IndexedDB::KeyType::Array:
        keyData.m_value = Vector<IDBKeyData>();
        if (!decoder.decode(WTF::get<Vector<IDBKeyData>>(keyData.m_value)))
            return WTF::nullopt;
        break;
    case IndexedDB::KeyType::Binary:
        keyData.m_value = ThreadSafeDataBuffer();
        if (!decoder.decode(WTF::get<ThreadSafeDataBuffer>(keyData.m_value)))
            return WTF::nullopt;
        break;
    case IndexedDB::KeyType::String:
        keyData.m_value = String();
        if (!decoder.decode(WTF::get<String>(keyData.m_value)))
            return WTF::nullopt;
        break;
    case IndexedDB::KeyType::Date:
    case IndexedDB::KeyType::Number:
        keyData.m_value = 0.0;
        if (!decoder.decode(WTF::get<double>(keyData.m_value)))
            return WTF::nullopt;
        break;
    }

    return keyData;
}

using IDBKeyDataSet = StdSet<IDBKeyData>;

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
