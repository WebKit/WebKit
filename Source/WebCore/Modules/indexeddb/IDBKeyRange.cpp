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
#include "IDBKey.h"
#include "IDBKeyData.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBKeyRange);

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

IDBKeyRange::~IDBKeyRange() = default;

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::only(RefPtr<IDBKey>&& key)
{
    if (!key || !key->isValid())
        return Exception { DataError };

    return create(WTFMove(key));
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::only(ExecState& state, JSValue keyValue)
{
    return only(scriptValueToIDBKey(state, keyValue));
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::lowerBound(ExecState& state, JSValue boundValue, bool open)
{
    auto bound = scriptValueToIDBKey(state, boundValue);
    if (!bound->isValid())
        return Exception { DataError };

    return create(WTFMove(bound), nullptr, open, true);
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::upperBound(ExecState& state, JSValue boundValue, bool open)
{
    auto bound = scriptValueToIDBKey(state, boundValue);
    if (!bound->isValid())
        return Exception { DataError };

    return create(nullptr, WTFMove(bound), true, open);
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::bound(ExecState& state, JSValue lowerValue, JSValue upperValue, bool lowerOpen, bool upperOpen)
{
    auto lower = scriptValueToIDBKey(state, lowerValue);
    if (!lower->isValid())
        return Exception { DataError };
    auto upper = scriptValueToIDBKey(state, upperValue);
    if (!upper->isValid())
        return Exception { DataError };
    if (upper->isLessThan(lower.get()))
        return Exception { DataError };
    if (upper->isEqual(lower.get()) && (lowerOpen || upperOpen))
        return Exception { DataError };

    return create(WTFMove(lower), WTFMove(upper), lowerOpen, upperOpen);
}

bool IDBKeyRange::isOnlyKey() const
{
    return m_lower && m_upper && !m_isLowerOpen && !m_isUpperOpen && m_lower->isEqual(*m_upper);
}

ExceptionOr<bool> IDBKeyRange::includes(JSC::ExecState& state, JSC::JSValue keyValue)
{
    auto key = scriptValueToIDBKey(state, keyValue);
    if (!key->isValid())
        return Exception { DataError, "Failed to execute 'includes' on 'IDBKeyRange': The passed-in value is not a valid IndexedDB key." };

    if (m_lower) {
        int compare = m_lower->compare(key.get());

        if (compare > 0)
            return false;
        if (m_isLowerOpen && !compare)
            return false;
    }

    if (m_upper) {
        int compare = m_upper->compare(key.get());

        if (compare < 0)
            return false;
        if (m_isUpperOpen && !compare)
            return false;
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
