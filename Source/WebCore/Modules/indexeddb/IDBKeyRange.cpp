/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBKeyRange.h"

#include "IDBDatabaseException.h"
#include "IDBKey.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBKeyRange::IDBKeyRange(PassRefPtr<IDBKey> lower, PassRefPtr<IDBKey> upper, LowerBoundType lowerType, UpperBoundType upperType)
    : m_lower(lower)
    , m_upper(upper)
    , m_lowerType(lowerType)
    , m_upperType(upperType)
{
}

PassRefPtr<IDBKeyRange> IDBKeyRange::only(PassRefPtr<IDBKey> prpKey, ExceptionCode& ec)
{
    RefPtr<IDBKey> key = prpKey;
    if (key && (key->type() == IDBKey::InvalidType)) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    return IDBKeyRange::create(key, key, LowerBoundClosed, UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::lowerBound(PassRefPtr<IDBKey> bound, bool open, ExceptionCode& ec)
{
    if (bound && (bound->type() == IDBKey::InvalidType)) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    return IDBKeyRange::create(bound, 0, open ? LowerBoundOpen : LowerBoundClosed, UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::upperBound(PassRefPtr<IDBKey> bound, bool open, ExceptionCode& ec)
{
    if (bound && (bound->type() == IDBKey::InvalidType)) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    return IDBKeyRange::create(0, bound, LowerBoundClosed, open ? UpperBoundOpen : UpperBoundClosed);
}

PassRefPtr<IDBKeyRange> IDBKeyRange::bound(PassRefPtr<IDBKey> lower, PassRefPtr<IDBKey> upper, bool lowerOpen, bool upperOpen, ExceptionCode& ec)
{
    if ((lower && (lower->type() == IDBKey::InvalidType)) || (upper && (upper->type() == IDBKey::InvalidType))) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }
    if (lower && upper && upper->isLessThan(lower.get())) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }
    if (lower && upper && upper->isEqual(lower.get()) && (lowerOpen || upperOpen)) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    return IDBKeyRange::create(lower, upper, lowerOpen ? LowerBoundOpen : LowerBoundClosed, upperOpen ? UpperBoundOpen : UpperBoundClosed);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
