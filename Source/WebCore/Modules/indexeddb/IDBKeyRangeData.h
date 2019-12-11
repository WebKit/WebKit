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

#include "IDBKeyData.h"
#include "IDBKeyRange.h"

namespace WebCore {

class IDBKey;

struct IDBKeyRangeData {
    IDBKeyRangeData()
        : isNull(true)
        , lowerOpen(false)
        , upperOpen(false)
    {
    }

    static IDBKeyRangeData allKeys()
    {
        IDBKeyRangeData result;
        result.isNull = false;
        result.lowerKey = IDBKeyData::minimum();
        result.upperKey = IDBKeyData::maximum();
        return result;
    }

    IDBKeyRangeData(IDBKey*);
    IDBKeyRangeData(const IDBKeyData&);

    IDBKeyRangeData(IDBKeyRange* keyRange)
        : isNull(!keyRange)
        , lowerOpen(false)
        , upperOpen(false)
    {
        if (isNull)
            return;

        lowerKey = keyRange->lower();
        upperKey = keyRange->upper();
        lowerOpen = keyRange->lowerOpen();
        upperOpen = keyRange->upperOpen();
    }

    WEBCORE_EXPORT IDBKeyRangeData isolatedCopy() const;

    WEBCORE_EXPORT RefPtr<IDBKeyRange> maybeCreateIDBKeyRange() const;

    WEBCORE_EXPORT bool isExactlyOneKey() const;
    bool containsKey(const IDBKeyData&) const;
    bool isValid() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBKeyRangeData&);

    bool isNull;

    IDBKeyData lowerKey;
    IDBKeyData upperKey;

    bool lowerOpen;
    bool upperOpen;

#if !LOG_DISABLED
    String loggingString() const;
#endif
};

template<class Encoder>
void IDBKeyRangeData::encode(Encoder& encoder) const
{
    encoder << isNull;
    if (isNull)
        return;

    encoder << upperKey << lowerKey << upperOpen << lowerOpen;
}

template<class Decoder>
bool IDBKeyRangeData::decode(Decoder& decoder, IDBKeyRangeData& keyRange)
{
    if (!decoder.decode(keyRange.isNull))
        return false;

    if (keyRange.isNull)
        return true;

    Optional<IDBKeyData> upperKey;
    decoder >> upperKey;
    if (!upperKey)
        return false;
    keyRange.upperKey = WTFMove(*upperKey);
    
    Optional<IDBKeyData> lowerKey;
    decoder >> lowerKey;
    if (!lowerKey)
        return false;
    keyRange.lowerKey = WTFMove(*lowerKey);

    if (!decoder.decode(keyRange.upperOpen))
        return false;

    if (!decoder.decode(keyRange.lowerOpen))
        return false;

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
