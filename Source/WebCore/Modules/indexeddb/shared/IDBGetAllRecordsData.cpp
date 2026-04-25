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

#include "config.h"
#include "IDBGetAllRecordsData.h"

#include "IDBKeyRangeData.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

IDBGetAllRecordsData IDBGetAllRecordsData::isolatedCopy() const
{
    return { keyRangeData.isolatedCopy(), getAllType, count, cursorDirection, objectStoreIdentifier, indexIdentifier };
}

#if !LOG_DISABLED

String IDBGetAllRecordsData::loggingString() const
{
    auto directionString = [&] {
        switch (cursorDirection) {
        case IndexedDB::CursorDirection::Next:
            return "next"_s;
        case IndexedDB::CursorDirection::Nextunique:
            return "nextunique"_s;
        case IndexedDB::CursorDirection::Prev:
            return "prev"_s;
        case IndexedDB::CursorDirection::Prevunique:
            return "prevunique"_s;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    auto getAllTypeString = [&] {
        switch (getAllType) {
        case IndexedDB::GetAllType::Keys:
            return "Keys"_s;
        case IndexedDB::GetAllType::Values:
            return "Values"_s;
        case IndexedDB::GetAllType::Records:
            return "Records"_s;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    if (indexIdentifier)
        return makeString("<GetAllRecords: Idx "_s, *indexIdentifier, ", OS "_s, objectStoreIdentifier, ", "_s, "getAllType "_s, getAllTypeString, ", cursorDirection "_s, directionString, ", range "_s, keyRangeData.loggingString(), '>');
    return makeString("<GetAllRecords: OS "_s, objectStoreIdentifier, ", "_s, "getAllType "_s, getAllTypeString, ", cursorDirection "_s, directionString, ", range "_s, keyRangeData.loggingString(), '>');
}

#endif

} // namespace WebCore
