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

#include "config.h"
#include "IDBKeyData.h"

#if ENABLE(INDEXED_DATABASE)

#include "KeyedCoding.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

IDBKeyData::IDBKeyData(const IDBKey* key)
    : m_type(KeyType::Invalid)
{
    if (!key) {
        m_isNull = true;
        return;
    }

    m_type = key->type();

    switch (m_type) {
    case KeyType::Invalid:
        break;
    case KeyType::Array:
        for (auto& key2 : key->array())
            m_arrayValue.append(IDBKeyData(key2.get()));
        break;
    case KeyType::String:
        m_stringValue = key->string();
        break;
    case KeyType::Date:
        m_numberValue = key->date();
        break;
    case KeyType::Number:
        m_numberValue = key->number();
        break;
    case KeyType::Max:
    case KeyType::Min:
        break;
    }
}

PassRefPtr<IDBKey> IDBKeyData::maybeCreateIDBKey() const
{
    if (m_isNull)
        return nullptr;

    switch (m_type) {
    case KeyType::Invalid:
        return IDBKey::createInvalid();
    case KeyType::Array:
        {
            Vector<RefPtr<IDBKey>> array;
            for (auto& keyData : m_arrayValue) {
                array.append(keyData.maybeCreateIDBKey());
                ASSERT(array.last());
            }
            return IDBKey::createArray(array);
        }
    case KeyType::String:
        return IDBKey::createString(m_stringValue);
    case KeyType::Date:
        return IDBKey::createDate(m_numberValue);
    case KeyType::Number:
        return IDBKey::createNumber(m_numberValue);
    case KeyType::Max:
    case KeyType::Min:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

IDBKeyData IDBKeyData::isolatedCopy() const
{
    IDBKeyData result;
    result.m_type = m_type;
    result.m_isNull = m_isNull;

    switch (m_type) {
    case KeyType::Invalid:
        return result;
    case KeyType::Array:
        for (auto& key : m_arrayValue)
            result.m_arrayValue.append(key.isolatedCopy());
        return result;
    case KeyType::String:
        result.m_stringValue = m_stringValue.isolatedCopy();
        return result;
    case KeyType::Date:
    case KeyType::Number:
        result.m_numberValue = m_numberValue;
        return result;
    case KeyType::Max:
    case KeyType::Min:
        return result;
    }

    ASSERT_NOT_REACHED();
    return result;
}

void IDBKeyData::encode(KeyedEncoder& encoder) const
{
    encoder.encodeBool("null", m_isNull);
    if (m_isNull)
        return;

    encoder.encodeEnum("m_type", m_type);

    switch (m_type) {
    case KeyType::Invalid:
        return;
    case KeyType::Array:
        encoder.encodeObjects("array", m_arrayValue.begin(), m_arrayValue.end(), [](KeyedEncoder& encoder, const IDBKeyData& key) {
            key.encode(encoder);
        });
        return;
    case KeyType::String:
        encoder.encodeString("string", m_stringValue);
        return;
    case KeyType::Date:
    case KeyType::Number:
        encoder.encodeDouble("number", m_numberValue);
        return;
    case KeyType::Max:
    case KeyType::Min:
        return;
    }

    ASSERT_NOT_REACHED();
}

bool IDBKeyData::decode(KeyedDecoder& decoder, IDBKeyData& result)
{
    if (!decoder.decodeBool("null", result.m_isNull))
        return false;

    if (result.m_isNull)
        return true;

    auto enumFunction = [](int64_t value) {
        return value == KeyType::Max
            || value == KeyType::Invalid
            || value == KeyType::Array
            || value == KeyType::String
            || value == KeyType::Date
            || value == KeyType::Number
            || value == KeyType::Min;
    };
    if (!decoder.decodeEnum("m_type", result.m_type, enumFunction))
        return false;

    if (result.m_type == KeyType::Invalid)
        return true;

    if (result.m_type == KeyType::Max)
        return true;

    if (result.m_type == KeyType::Min)
        return true;

    if (result.m_type == KeyType::String)
        return decoder.decodeString("string", result.m_stringValue);

    if (result.m_type == KeyType::Number || result.m_type == KeyType::Date)
        return decoder.decodeDouble("number", result.m_numberValue);

    ASSERT(result.m_type == KeyType::Array);

    auto arrayFunction = [](KeyedDecoder& decoder, IDBKeyData& result) {
        return decode(decoder, result);
    };
    
    result.m_arrayValue.clear();
    return decoder.decodeObjects("array", result.m_arrayValue, arrayFunction);
}

int IDBKeyData::compare(const IDBKeyData& other) const
{
    if (m_type == KeyType::Invalid) {
        if (other.m_type != KeyType::Invalid)
            return -1;
        if (other.m_type == KeyType::Invalid)
            return 0;
    } else if (other.m_type == KeyType::Invalid)
        return 1;

    // The IDBKey::m_type enum is in reverse sort order.
    if (m_type != other.m_type)
        return m_type < other.m_type ? 1 : -1;

    // The types are the same, so handle actual value comparison.
    switch (m_type) {
    case KeyType::Invalid:
        // Invalid type should have been fully handled above
        ASSERT_NOT_REACHED();
        return 0;
    case KeyType::Array:
        for (size_t i = 0; i < m_arrayValue.size() && i < other.m_arrayValue.size(); ++i) {
            if (int result = m_arrayValue[i].compare(other.m_arrayValue[i]))
                return result;
        }
        if (m_arrayValue.size() < other.m_arrayValue.size())
            return -1;
        if (m_arrayValue.size() > other.m_arrayValue.size())
            return 1;
        return 0;
    case KeyType::String:
        return codePointCompare(m_stringValue, other.m_stringValue);
    case KeyType::Date:
    case KeyType::Number:
        if (m_numberValue == other.m_numberValue)
            return 0;
        return m_numberValue > other.m_numberValue ? 1 : -1;
    case KeyType::Max:
    case KeyType::Min:
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

#ifndef NDEBUG
String IDBKeyData::loggingString() const
{
    if (m_isNull)
        return "<null>";

    switch (m_type) {
    case KeyType::Invalid:
        return "<invalid>";
    case KeyType::Array:
        {
            StringBuilder result;
            result.appendLiteral("<array> - { ");
            for (size_t i = 0; i < m_arrayValue.size(); ++i) {
                result.append(m_arrayValue[i].loggingString());
                if (i < m_arrayValue.size() - 1)
                    result.appendLiteral(", ");
            }
            result.appendLiteral(" }");
            return result.toString();
        }
    case KeyType::String:
        return "<string> - " + m_stringValue;
    case KeyType::Date:
        return String::format("Date m_type - %f", m_numberValue);
    case KeyType::Number:
        return String::format("<number> - %f", m_numberValue);
    case KeyType::Max:
        return "<maximum>";
    case KeyType::Min:
        return "<minimum>";
    default:
        return String();
    }
    ASSERT_NOT_REACHED();
}
#endif

void IDBKeyData::setArrayValue(const Vector<IDBKeyData>& value)
{
    *this = IDBKeyData();
    m_arrayValue = value;
    m_type = KeyType::Array;
    m_isNull = false;
}

void IDBKeyData::setStringValue(const String& value)
{
    *this = IDBKeyData();
    m_stringValue = value;
    m_type = KeyType::String;
    m_isNull = false;
}

void IDBKeyData::setDateValue(double value)
{
    *this = IDBKeyData();
    m_numberValue = value;
    m_type = KeyType::Date;
    m_isNull = false;
}

void IDBKeyData::setNumberValue(double value)
{
    *this = IDBKeyData();
    m_numberValue = value;
    m_type = KeyType::Number;
    m_isNull = false;
}

IDBKeyData IDBKeyData::deletedValue()
{
    IDBKeyData result;
    result.m_isNull = false;
    result.m_isDeletedValue = true;
    return result;
}

bool IDBKeyData::operator<(const IDBKeyData& rhs) const
{
    return compare(rhs) < 0;
}

bool IDBKeyData::operator==(const IDBKeyData& other) const
{
    if (m_type != other.m_type || m_isNull != other.m_isNull || m_isDeletedValue != other.m_isDeletedValue)
        return false;
    switch (m_type) {
    case KeyType::Invalid:
    case KeyType::Max:
    case KeyType::Min:
        return true;
    case KeyType::Number:
    case KeyType::Date:
        return m_numberValue == other.m_numberValue;
    case KeyType::String:
        return m_stringValue == other.m_stringValue;
    case KeyType::Array:
        if (m_arrayValue.size() != other.m_arrayValue.size())
            return false;
        for (size_t i = 0; i < m_arrayValue.size(); ++i) {
            if (m_arrayValue[0] != other.m_arrayValue[0])
                return false;
        }
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
