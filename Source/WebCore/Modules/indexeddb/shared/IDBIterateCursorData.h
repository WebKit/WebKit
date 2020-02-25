/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

namespace WebCore {

struct IDBIterateCursorData {
    IDBKeyData keyData;
    IDBKeyData primaryKeyData;
    unsigned count;
    IndexedDB::CursorIterateOption option { IndexedDB::CursorIterateOption::Reply };

    WEBCORE_EXPORT IDBIterateCursorData isolatedCopy() const;

#if !LOG_DISABLED
    String loggingString() const;
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBIterateCursorData&);
};

template<class Encoder>
void IDBIterateCursorData::encode(Encoder& encoder) const
{
    encoder << keyData << primaryKeyData << static_cast<uint64_t>(count);
    encoder.encodeEnum(option);
}

template<class Decoder>
bool IDBIterateCursorData::decode(Decoder& decoder, IDBIterateCursorData& iteratorCursorData)
{
    Optional<IDBKeyData> keyData;
    decoder >> keyData;
    if (!keyData)
        return false;
    iteratorCursorData.keyData = WTFMove(*keyData);

    Optional<IDBKeyData> primaryKeyData;
    decoder >> primaryKeyData;
    if (!primaryKeyData)
        return false;
    iteratorCursorData.primaryKeyData = WTFMove(*primaryKeyData);

    uint64_t count;
    if (!decoder.decode(count))
        return false;

    if (count > std::numeric_limits<unsigned>::max())
        return false;
    iteratorCursorData.count = static_cast<unsigned>(count);

    if (!decoder.decodeEnum(iteratorCursorData.option))
        return false;

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
