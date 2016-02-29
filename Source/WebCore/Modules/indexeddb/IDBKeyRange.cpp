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

#include "DOMRequestState.h"
#include "IDBBindingUtilities.h"
#include "IDBDatabaseException.h"
#include "IDBKey.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

Ref<IDBKeyRange> IDBKeyRange::create(RefPtr<IDBKey>&& key)
{
    RefPtr<IDBKey> upperBound = key;
    return adoptRef(*new IDBKeyRange(WTFMove(key), WTFMove(upperBound), LowerBoundClosed, UpperBoundClosed));
}

IDBKeyRange::IDBKeyRange(RefPtr<IDBKey>&& lower, RefPtr<IDBKey>&& upper, LowerBoundType lowerType, UpperBoundType upperType)
    : m_lower(WTFMove(lower))
    , m_upper(WTFMove(upper))
    , m_lowerType(lowerType)
    , m_upperType(upperType)
{
}

Deprecated::ScriptValue IDBKeyRange::lowerValue(ScriptExecutionContext* context) const
{
    DOMRequestState requestState(context);
    return idbKeyToScriptValue(&requestState, m_lower);
}

Deprecated::ScriptValue IDBKeyRange::upperValue(ScriptExecutionContext* context) const
{
    DOMRequestState requestState(context);
    return idbKeyToScriptValue(&requestState, m_upper);
}

RefPtr<IDBKeyRange> IDBKeyRange::only(RefPtr<IDBKey>&& key, ExceptionCode& ec)
{
    if (!key || !key->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return create(WTFMove(key));
}

RefPtr<IDBKeyRange> IDBKeyRange::only(ScriptExecutionContext* context, const Deprecated::ScriptValue& keyValue, ExceptionCode& ec)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> key = scriptValueToIDBKey(&requestState, keyValue);
    if (!key || !key->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return create(WTFMove(key));
}

RefPtr<IDBKeyRange> IDBKeyRange::lowerBound(ScriptExecutionContext* context, const Deprecated::ScriptValue& boundValue, bool open, ExceptionCode& ec)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> bound = scriptValueToIDBKey(&requestState, boundValue);
    if (!bound || !bound->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return IDBKeyRange::create(WTFMove(bound), nullptr, open ? LowerBoundOpen : LowerBoundClosed, UpperBoundOpen);
}

RefPtr<IDBKeyRange> IDBKeyRange::upperBound(ScriptExecutionContext* context, const Deprecated::ScriptValue& boundValue, bool open, ExceptionCode& ec)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> bound = scriptValueToIDBKey(&requestState, boundValue);
    if (!bound || !bound->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return IDBKeyRange::create(nullptr, WTFMove(bound), LowerBoundOpen, open ? UpperBoundOpen : UpperBoundClosed);
}

RefPtr<IDBKeyRange> IDBKeyRange::bound(ScriptExecutionContext* context, const Deprecated::ScriptValue& lowerValue, const Deprecated::ScriptValue& upperValue, bool lowerOpen, bool upperOpen, ExceptionCode& ec)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> lower = scriptValueToIDBKey(&requestState, lowerValue);
    RefPtr<IDBKey> upper = scriptValueToIDBKey(&requestState, upperValue);

    if (!lower || !lower->isValid() || !upper || !upper->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }
    if (upper->isLessThan(lower.get())) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }
    if (upper->isEqual(lower.get()) && (lowerOpen || upperOpen)) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return IDBKeyRange::create(WTFMove(lower), WTFMove(upper), lowerOpen ? LowerBoundOpen : LowerBoundClosed, upperOpen ? UpperBoundOpen : UpperBoundClosed);
}

bool IDBKeyRange::isOnlyKey() const
{
    if (m_lowerType != LowerBoundClosed || m_upperType != UpperBoundClosed)
        return false;

    ASSERT(m_lower);
    ASSERT(m_upper);
    return m_lower->isEqual(m_upper.get());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
