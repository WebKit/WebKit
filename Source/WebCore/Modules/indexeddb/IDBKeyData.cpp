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
            encoder.encodeObject("idbKeyData", key, [](KeyedEncoder& encoder, const IDBKeyData& key) {
                key.encode(encoder);
            });
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

bool IDBKeyData::decode(KeyedDecoder&, IDBKeyData&)
{
    // FIXME: Implement when IDB Get support is implemented (<rdar://problem/15779644>)
    return false;
}

}

#endif // ENABLE(INDEXED_DATABASE)
