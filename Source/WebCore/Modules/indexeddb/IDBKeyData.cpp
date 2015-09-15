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
    : type(KeyType::Invalid)
    , numberValue(0)
    , isNull(false)
{
    if (!key) {
        isNull = true;
        return;
    }

    type = key->type();
    numberValue = 0;

    switch (type) {
    case KeyType::Invalid:
        break;
    case KeyType::Array:
        for (auto& key2 : key->array())
            arrayValue.append(IDBKeyData(key2.get()));
        break;
    case KeyType::String:
        stringValue = key->string();
        break;
    case KeyType::Date:
        numberValue = key->date();
        break;
    case KeyType::Number:
        numberValue = key->number();
        break;
    case KeyType::Max:
    case KeyType::Min:
        break;
    }
}

PassRefPtr<IDBKey> IDBKeyData::maybeCreateIDBKey() const
{
    if (isNull)
        return nullptr;

    switch (type) {
    case KeyType::Invalid:
        return IDBKey::createInvalid();
    case KeyType::Array:
        {
            Vector<RefPtr<IDBKey>> array;
            for (auto& keyData : arrayValue) {
                array.append(keyData.maybeCreateIDBKey());
                ASSERT(array.last());
            }
            return IDBKey::createArray(array);
        }
    case KeyType::String:
        return IDBKey::createString(stringValue);
    case KeyType::Date:
        return IDBKey::createDate(numberValue);
    case KeyType::Number:
        return IDBKey::createNumber(numberValue);
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
    result.type = type;
    result.isNull = isNull;

    switch (type) {
    case KeyType::Invalid:
        return result;
    case KeyType::Array:
        for (auto& key : arrayValue)
            result.arrayValue.append(key.isolatedCopy());
        return result;
    case KeyType::String:
        result.stringValue = stringValue.isolatedCopy();
        return result;
    case KeyType::Date:
    case KeyType::Number:
        result.numberValue = numberValue;
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
    encoder.encodeBool("null", isNull);
    if (isNull)
        return;

    encoder.encodeEnum("type", type);

    switch (type) {
    case KeyType::Invalid:
        return;
    case KeyType::Array:
        encoder.encodeObjects("array", arrayValue.begin(), arrayValue.end(), [](KeyedEncoder& encoder, const IDBKeyData& key) {
            key.encode(encoder);
        });
        return;
    case KeyType::String:
        encoder.encodeString("string", stringValue);
        return;
    case KeyType::Date:
    case KeyType::Number:
        encoder.encodeDouble("number", numberValue);
        return;
    case KeyType::Max:
    case KeyType::Min:
        return;
    }

    ASSERT_NOT_REACHED();
}

bool IDBKeyData::decode(KeyedDecoder& decoder, IDBKeyData& result)
{
    if (!decoder.decodeBool("null", result.isNull))
        return false;

    if (result.isNull)
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
    if (!decoder.decodeEnum("type", result.type, enumFunction))
        return false;

    if (result.type == KeyType::Invalid)
        return true;

    if (result.type == KeyType::Max)
        return true;

    if (result.type == KeyType::Min)
        return true;

    if (result.type == KeyType::String)
        return decoder.decodeString("string", result.stringValue);

    if (result.type == KeyType::Number || result.type == KeyType::Date)
        return decoder.decodeDouble("number", result.numberValue);

    ASSERT(result.type == KeyType::Array);

    auto arrayFunction = [](KeyedDecoder& decoder, IDBKeyData& result) {
        return decode(decoder, result);
    };
    
    result.arrayValue.clear();
    return decoder.decodeObjects("array", result.arrayValue, arrayFunction);
}

int IDBKeyData::compare(const IDBKeyData& other) const
{
    if (type == KeyType::Invalid) {
        if (other.type != KeyType::Invalid)
            return -1;
        if (other.type == KeyType::Invalid)
            return 0;
    } else if (other.type == KeyType::Invalid)
        return 1;

    // The IDBKey::Type enum is in reverse sort order.
    if (type != other.type)
        return type < other.type ? 1 : -1;

    // The types are the same, so handle actual value comparison.
    switch (type) {
    case KeyType::Invalid:
        // InvalidType should have been fully handled above
        ASSERT_NOT_REACHED();
        return 0;
    case KeyType::Array:
        for (size_t i = 0; i < arrayValue.size() && i < other.arrayValue.size(); ++i) {
            if (int result = arrayValue[i].compare(other.arrayValue[i]))
                return result;
        }
        if (arrayValue.size() < other.arrayValue.size())
            return -1;
        if (arrayValue.size() > other.arrayValue.size())
            return 1;
        return 0;
    case KeyType::String:
        return codePointCompare(stringValue, other.stringValue);
    case KeyType::Date:
    case KeyType::Number:
        if (numberValue == other.numberValue)
            return 0;
        return numberValue > other.numberValue ? 1 : -1;
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
    if (isNull)
        return "<null>";

    switch (type) {
    case KeyType::Invalid:
        return "<invalid>";
    case KeyType::Array:
        {
            StringBuilder result;
            result.appendLiteral("<array> - { ");
            for (size_t i = 0; i < arrayValue.size(); ++i) {
                result.append(arrayValue[i].loggingString());
                if (i < arrayValue.size() - 1)
                    result.appendLiteral(", ");
            }
            result.appendLiteral(" }");
            return result.toString();
        }
    case KeyType::String:
        return "<string> - " + stringValue;
    case KeyType::Date:
        return String::format("Date type - %f", numberValue);
    case KeyType::Number:
        return String::format("<number> - %f", numberValue);
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
    arrayValue = value;
    type = KeyType::Array;
    isNull = false;
}

void IDBKeyData::setStringValue(const String& value)
{
    *this = IDBKeyData();
    stringValue = value;
    type = KeyType::String;
    isNull = false;
}

void IDBKeyData::setDateValue(double value)
{
    *this = IDBKeyData();
    numberValue = value;
    type = KeyType::Date;
    isNull = false;
}

void IDBKeyData::setNumberValue(double value)
{
    *this = IDBKeyData();
    numberValue = value;
    type = KeyType::Number;
    isNull = false;
}

}

#endif // ENABLE(INDEXED_DATABASE)
