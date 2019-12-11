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

#include "IDBKeyRangeData.h"
#include <wtf/Optional.h>

namespace WebCore {

namespace IndexedDB {
enum class DataSource;
enum class GetAllType;
}

struct IDBGetAllRecordsData {
    IDBKeyRangeData keyRangeData;
    IndexedDB::GetAllType getAllType;
    Optional<uint32_t> count;
    uint64_t objectStoreIdentifier;
    uint64_t indexIdentifier;

    WEBCORE_EXPORT IDBGetAllRecordsData isolatedCopy() const;

#if !LOG_DISABLED
    String loggingString() const;
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBGetAllRecordsData&);
};

template<class Encoder>
void IDBGetAllRecordsData::encode(Encoder& encoder) const
{
    encoder << keyRangeData;
    encoder.encodeEnum(getAllType);
    encoder << count << objectStoreIdentifier << indexIdentifier;
}

template<class Decoder>
bool IDBGetAllRecordsData::decode(Decoder& decoder, IDBGetAllRecordsData& getAllRecordsData)
{
    if (!decoder.decode(getAllRecordsData.keyRangeData))
        return false;

    if (!decoder.decodeEnum(getAllRecordsData.getAllType))
        return false;

    if (!decoder.decode(getAllRecordsData.count))
        return false;

    if (!decoder.decode(getAllRecordsData.objectStoreIdentifier))
        return false;

    if (!decoder.decode(getAllRecordsData.indexIdentifier))
        return false;

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
