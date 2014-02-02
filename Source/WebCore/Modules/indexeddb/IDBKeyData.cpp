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

namespace WebCore {

IDBKeyData::IDBKeyData(const IDBKey* key)
    : type(IDBKey::InvalidType)
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
    case IDBKey::InvalidType:
        break;
    case IDBKey::ArrayType:
        for (auto key2 : key->array())
            arrayValue.append(IDBKeyData(key2.get()));
        break;
    case IDBKey::StringType:
        stringValue = key->string();
        break;
    case IDBKey::DateType:
        numberValue = key->date();
        break;
    case IDBKey::NumberType:
        numberValue = key->number();
        break;
    case IDBKey::MinType:
        ASSERT_NOT_REACHED();
        break;
    }
}

PassRefPtr<IDBKey> IDBKeyData::maybeCreateIDBKey() const
{
    if (isNull)
        return nullptr;

    switch (type) {
    case IDBKey::InvalidType:
        return IDBKey::createInvalid();
    case IDBKey::ArrayType:
        {
            IDBKey::KeyArray array;
            for (auto keyData : arrayValue) {
                array.append(keyData.maybeCreateIDBKey());
                ASSERT(array.last());
            }
            return IDBKey::createArray(array);
        }
    case IDBKey::StringType:
        return IDBKey::createString(stringValue);
    case IDBKey::DateType:
        return IDBKey::createDate(numberValue);
    case IDBKey::NumberType:
        return IDBKey::createNumber(numberValue);
    case IDBKey::MinType:
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
    case IDBKey::InvalidType:
        return result;
    case IDBKey::ArrayType:
        for (auto key : arrayValue)
            result.arrayValue.append(key.isolatedCopy());
        return result;
    case IDBKey::StringType:
        result.stringValue = stringValue.isolatedCopy();
        return result;
    case IDBKey::DateType:
    case IDBKey::NumberType:
        result.numberValue = numberValue;
        return result;
    case IDBKey::MinType:
        ASSERT_NOT_REACHED();
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
    case IDBKey::InvalidType:
        return;
    case IDBKey::ArrayType:
        encoder.encodeObjects("array", arrayValue.begin(), arrayValue.end(), [](KeyedEncoder& encoder, const IDBKeyData& key) {
            key.encode(encoder);
        });
        return;
    case IDBKey::StringType:
        encoder.encodeString("string", stringValue);
        return;
    case IDBKey::DateType:
    case IDBKey::NumberType:
        encoder.encodeDouble("number", numberValue);
        return;
    case IDBKey::MinType:
        ASSERT_NOT_REACHED();
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
        return value == IDBKey::InvalidType
            || value == IDBKey::ArrayType
            || value == IDBKey::StringType
            || value == IDBKey::DateType
            || value == IDBKey::NumberType
            || value == IDBKey::MinType;
    };
    if (!decoder.decodeVerifiedEnum("type", result.type, enumFunction))
        return false;

    if (result.type == IDBKey::InvalidType)
        return true;

    if (result.type == IDBKey::MinType) {
        ASSERT_NOT_REACHED();
        return true;
    }

    if (result.type == IDBKey::StringType)
        return decoder.decodeString("string", result.stringValue);

    if (result.type == IDBKey::NumberType || result.type == IDBKey::DateType)
        return decoder.decodeDouble("number", result.numberValue);

    ASSERT(result.type == IDBKey::ArrayType);

    auto arrayFunction = [](KeyedDecoder& decoder, IDBKeyData& result) {
        return decode(decoder, result);
    };
    return decoder.decodeObjects("array", result.arrayValue, arrayFunction);
}

int IDBKeyData::compare(const IDBKeyData& other) const
{
    if (type == IDBKey::InvalidType) {
        if (other.type != IDBKey::InvalidType)
            return -1;
        if (other.type == IDBKey::InvalidType)
            return 0;
    } else if (other.type == IDBKey::InvalidType)
        return 1;

    // The IDBKey::Type enum is in reverse sort order.
    if (type != other.type)
        return type < other.type ? 1 : -1;

    // The types are the same, so handle actual value comparison.
    switch (type) {
    case IDBKey::InvalidType:
        // InvalidType should have been fully handled above
        ASSERT_NOT_REACHED();
        return 0;
    case IDBKey::ArrayType:
        for (size_t i = 0; i < arrayValue.size() && i < other.arrayValue.size(); ++i) {
            if (int result = arrayValue[i].compare(other.arrayValue[i]))
                return result;
        }
        if (arrayValue.size() < other.arrayValue.size())
            return -1;
        if (arrayValue.size() > other.arrayValue.size())
            return 1;
        return 0;
    case IDBKey::StringType:
        return codePointCompare(stringValue, other.stringValue);
    case IDBKey::DateType:
    case IDBKey::NumberType:
        if (numberValue == other.numberValue)
            return 0;
        return numberValue > other.numberValue ? 1 : -1;
    case IDBKey::MinType:
        ASSERT_NOT_REACHED();
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
    case IDBKey::InvalidType:
        return "<invalid>";
    case IDBKey::ArrayType:
        {
            String result = "<array> - { ";
            for (size_t i = 0; i < arrayValue.size(); ++i) {
                result.append(arrayValue[i].loggingString());
                if (i < arrayValue.size() - 1)
                    result.append(", ");
            }
            result.append(" }");
            return result;
        }
    case IDBKey::StringType:
        return String("<string> - ") + stringValue;
    case IDBKey::DateType:
        return String::format("Date type - %f", numberValue);
    case IDBKey::NumberType:
        return String::format("<number> - %f", numberValue);
    case IDBKey::MinType:
        return "<minimum>";
    default:
        return String();
    }
    ASSERT_NOT_REACHED();
}
#endif

}

#endif // ENABLE(INDEXED_DATABASE)
