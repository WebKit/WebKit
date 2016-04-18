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

#if ENABLE(INDEXED_DATABASE)

#include "IDBBindingUtilities.h"
#include "IDBDatabaseException.h"
#include "IDBKey.h"
#include "IDBKeyData.h"
#include <runtime/JSCJSValue.h>

using namespace JSC;

namespace WebCore {

Ref<IDBKeyRange> IDBKeyRange::create(RefPtr<IDBKey>&& lower, RefPtr<IDBKey>&& upper, bool isLowerOpen, bool isUpperOpen)
{
    return adoptRef(*new IDBKeyRange(WTFMove(lower), WTFMove(upper), isLowerOpen, isUpperOpen));
}

Ref<IDBKeyRange> IDBKeyRange::create(RefPtr<IDBKey>&& key)
{
    auto upper = key;
    return create(WTFMove(key), WTFMove(upper), false, false);
}

IDBKeyRange::IDBKeyRange(RefPtr<IDBKey>&& lower, RefPtr<IDBKey>&& upper, bool isLowerOpen, bool isUpperOpen)
    : m_lower(WTFMove(lower))
    , m_upper(WTFMove(upper))
    , m_isLowerOpen(isLowerOpen)
    , m_isUpperOpen(isUpperOpen)
{
}

IDBKeyRange::~IDBKeyRange()
{
}

JSValue IDBKeyRange::lowerValue(ScriptExecutionContext& context) const
{
    return idbKeyDataToScriptValue(context, IDBKeyData(m_lower.get()));
}

JSValue IDBKeyRange::upperValue(ScriptExecutionContext& context) const
{
    return idbKeyDataToScriptValue(context, IDBKeyData(m_upper.get()));
}

RefPtr<IDBKeyRange> IDBKeyRange::only(RefPtr<IDBKey>&& key, ExceptionCode& ec)
{
    if (!key || !key->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return create(WTFMove(key));
}

RefPtr<IDBKeyRange> IDBKeyRange::only(ScriptExecutionContext& context, JSValue keyValue, ExceptionCode& ec)
{
    auto key = scriptValueToIDBKey(context, keyValue);
    if (!key || !key->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return create(WTFMove(key));
}

RefPtr<IDBKeyRange> IDBKeyRange::lowerBound(ScriptExecutionContext& context, JSValue boundValue, bool open, ExceptionCode& ec)
{
    auto bound = scriptValueToIDBKey(context, boundValue);
    if (!bound || !bound->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return create(WTFMove(bound), nullptr, open, true);
}

RefPtr<IDBKeyRange> IDBKeyRange::upperBound(ScriptExecutionContext& context, JSValue boundValue, bool open, ExceptionCode& ec)
{
    auto bound = scriptValueToIDBKey(context, boundValue);
    if (!bound || !bound->isValid()) {
        ec = IDBDatabaseException::DataError;
        return nullptr;
    }

    return create(nullptr, WTFMove(bound), true, open);
}

RefPtr<IDBKeyRange> IDBKeyRange::bound(ScriptExecutionContext& context, JSValue lowerValue, JSValue upperValue, bool lowerOpen, bool upperOpen, ExceptionCode& ec)
{
    auto lower = scriptValueToIDBKey(context, lowerValue);
    auto upper = scriptValueToIDBKey(context, upperValue);

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

    return create(WTFMove(lower), WTFMove(upper), lowerOpen, upperOpen);
}

bool IDBKeyRange::isOnlyKey() const
{
    return m_lower && m_upper && !m_isLowerOpen && !m_isUpperOpen && m_lower->isEqual(m_upper.get());
}

RefPtr<IDBKeyRange> IDBKeyRange::lowerBound(ScriptExecutionContext& context, JSValue bound, ExceptionCode& ec)
{
    return lowerBound(context, bound, false, ec);
}

RefPtr<IDBKeyRange> IDBKeyRange::upperBound(ScriptExecutionContext& context, JSValue bound, ExceptionCode& ec)
{
    return upperBound(context, bound, false, ec);
}

RefPtr<IDBKeyRange> IDBKeyRange::bound(ScriptExecutionContext& context, JSValue lower, JSValue upper, ExceptionCode& ec)
{
    return bound(context, lower, upper, false, false, ec);
}

RefPtr<IDBKeyRange> IDBKeyRange::bound(ScriptExecutionContext& context, JSValue lower, JSValue upper, bool lowerOpen, ExceptionCode& ec)
{
    return bound(context, lower, upper, lowerOpen, false, ec);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
