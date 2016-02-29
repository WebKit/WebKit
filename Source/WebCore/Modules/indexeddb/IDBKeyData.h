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

#ifndef IDBKeyData_h
#define IDBKeyData_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"
#include <wtf/text/StringHash.h>

namespace WebCore {

class KeyedDecoder;
class KeyedEncoder;

class IDBKeyData {
public:
    IDBKeyData()
        : m_type(KeyType::Invalid)
        , m_isNull(true)
    {
    }

    WEBCORE_EXPORT IDBKeyData(const IDBKey*);

    static IDBKeyData minimum()
    {
        IDBKeyData result;
        result.m_type = KeyType::Min;
        result.m_isNull = false;
        return result;
    }

    static IDBKeyData maximum()
    {
        IDBKeyData result;
        result.m_type = KeyType::Max;
        result.m_isNull = false;
        return result;
    }

    WEBCORE_EXPORT RefPtr<IDBKey> maybeCreateIDBKey() const;

    IDBKeyData isolatedCopy() const;

    WEBCORE_EXPORT void encode(KeyedEncoder&) const;
    WEBCORE_EXPORT static bool decode(KeyedDecoder&, IDBKeyData&);

    // compare() has the same semantics as strcmp().
    //   - Returns negative if this IDBKeyData is less than other.
    //   - Returns positive if this IDBKeyData is greater than other.
    //   - Returns zero if this IDBKeyData is equal to other.
    WEBCORE_EXPORT int compare(const IDBKeyData& other) const;

    void setArrayValue(const Vector<IDBKeyData>&);
    void setStringValue(const String&);
    void setDateValue(double);
    WEBCORE_EXPORT void setNumberValue(double);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBKeyData&);
    
#ifndef NDEBUG
    WEBCORE_EXPORT String loggingString() const;
#endif

    bool isNull() const { return m_isNull; }
    bool isValid() const { return m_type != KeyType::Invalid; }
    KeyType type() const { return m_type; }

    bool operator<(const IDBKeyData&) const;
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
        case KeyType::Invalid:
        case KeyType::Max:
        case KeyType::Min:
            break;
        case KeyType::Number:
        case KeyType::Date:
            hashCodes.append(StringHasher::hashMemory<sizeof(double)>(&m_numberValue));
            break;
        case KeyType::String:
            hashCodes.append(StringHash::hash(m_stringValue));
            break;
        case KeyType::Array:
            for (auto& key : m_arrayValue)
                hashCodes.append(key.hash());
            break;
        }

        return StringHasher::hashMemory(hashCodes.data(), hashCodes.size() * sizeof(unsigned));
    }

    static IDBKeyData deletedValue();
    bool isDeletedValue() const { return m_isDeletedValue; }

    String string() const
    {
        ASSERT(m_type == KeyType::String);
        return m_stringValue;
    }

    double date() const
    {
        ASSERT(m_type == KeyType::Date);
        return m_numberValue;
    }

    double number() const
    {
        ASSERT(m_type == KeyType::Number);
        return m_numberValue;
    }

    const Vector<IDBKeyData>& array() const
    {
        ASSERT(m_type == KeyType::Array);
        return m_arrayValue;
    }

private:
    KeyType m_type;
    Vector<IDBKeyData> m_arrayValue;
    String m_stringValue;
    double m_numberValue { 0 };
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
    case KeyType::Invalid:
    case KeyType::Max:
    case KeyType::Min:
        break;
    case KeyType::Array:
        encoder << m_arrayValue;
        break;
    case KeyType::String:
        encoder << m_stringValue;
        break;
    case KeyType::Date:
    case KeyType::Number:
        encoder << m_numberValue;
        break;
    }
}

template<class Decoder>
bool IDBKeyData::decode(Decoder& decoder, IDBKeyData& keyData)
{
    if (!decoder.decode(keyData.m_isNull))
        return false;

    if (keyData.m_isNull)
        return true;

    if (!decoder.decodeEnum(keyData.m_type))
        return false;

    switch (keyData.m_type) {
    case KeyType::Invalid:
    case KeyType::Max:
    case KeyType::Min:
        break;
    case KeyType::Array:
        if (!decoder.decode(keyData.m_arrayValue))
            return false;
        break;
    case KeyType::String:
        if (!decoder.decode(keyData.m_stringValue))
            return false;
        break;
    case KeyType::Date:
    case KeyType::Number:
        if (!decoder.decode(keyData.m_numberValue))
            return false;
        break;
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBKeyData_h
