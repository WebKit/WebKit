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
#include "IDBKeyRangeData.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"

namespace WebCore {

IDBKeyRangeData::IDBKeyRangeData(IDBKey* key)
    : isNull(!key)
    , lowerKey(key)
    , upperKey(key)
    , lowerOpen(false)
    , upperOpen(false)
{
}

IDBKeyRangeData::IDBKeyRangeData(const IDBKeyData& keyData)
    : isNull(keyData.isNull())
    , lowerKey(keyData)
    , upperKey(keyData)
    , lowerOpen(false)
    , upperOpen(false)
{
}

IDBKeyRangeData IDBKeyRangeData::isolatedCopy() const
{
    IDBKeyRangeData result;

    result.isNull = isNull;
    result.lowerKey = lowerKey.isolatedCopy();
    result.upperKey = upperKey.isolatedCopy();
    result.lowerOpen = lowerOpen;
    result.upperOpen = upperOpen;

    return result;
}

RefPtr<IDBKeyRange> IDBKeyRangeData::maybeCreateIDBKeyRange() const
{
    if (isNull)
        return nullptr;

    return IDBKeyRange::create(lowerKey.maybeCreateIDBKey(), upperKey.maybeCreateIDBKey(), lowerOpen, upperOpen);
}

bool IDBKeyRangeData::isExactlyOneKey() const
{
    if (isNull || lowerOpen || upperOpen || !upperKey.isValid() || !lowerKey.isValid())
        return false;

    return !lowerKey.compare(upperKey);
}

bool IDBKeyRangeData::containsKey(const IDBKeyData& key) const
{
    if (lowerKey.isValid()) {
        auto compare = lowerKey.compare(key);
        if (compare > 0)
            return false;
        if (lowerOpen && !compare)
            return false;
    }
    if (upperKey.isValid()) {
        auto compare = upperKey.compare(key);
        if (compare < 0)
            return false;
        if (upperOpen && !compare)
            return false;
    }

    return true;
}

bool IDBKeyRangeData::isValid() const
{
    if (isNull)
        return false;

    if (!lowerKey.isValid() && !lowerKey.isNull())
        return false;

    if (!upperKey.isValid() && !upperKey.isNull())
        return false;

    return true;
}

#if !LOG_DISABLED
String IDBKeyRangeData::loggingString() const
{
    auto result = makeString(lowerOpen ? "( " : "[ ", lowerKey.loggingString(), ", ", upperKey.loggingString(), upperOpen ? " )" : " ]");
    if (result.length() > 400) {
        result.truncate(397);
        result.append("..."_s);
    }

    return result;
}
#endif

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
