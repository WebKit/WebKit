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

#include "ExceptionOr.h"
#include "IDBBindingUtilities.h"
#include "IDBKey.h"
#include "IDBKeyData.h"
#include "JSIDBKeyRange.h"
#include "ScriptExecutionContext.h"
#include "ScriptWrappableInlines.h"
#include <JavaScriptCore/DateInstance.h>
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBKeyRange);

Ref<IDBKeyRange> IDBKeyRange::create(RefPtr<IDBKey>&& lower, RefPtr<IDBKey>&& upper, bool isLowerOpen, bool isUpperOpen)
{
    return adoptRef(*new IDBKeyRange(WTF::move(lower), WTF::move(upper), isLowerOpen, isUpperOpen));
}

Ref<IDBKeyRange> IDBKeyRange::create(RefPtr<IDBKey>&& key)
{
    auto upper = key;
    return create(WTF::move(key), WTF::move(upper), false, false);
}

IDBKeyRange::IDBKeyRange(RefPtr<IDBKey>&& lower, RefPtr<IDBKey>&& upper, bool isLowerOpen, bool isUpperOpen)
    : m_lower(WTF::move(lower))
    , m_upper(WTF::move(upper))
    , m_isLowerOpen(isLowerOpen)
    , m_isUpperOpen(isUpperOpen)
{
}

IDBKeyRange::~IDBKeyRange() = default;

// https://w3c.github.io/IndexedDB/#convert-a-value-to-a-key-range.
ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::fromValue(JSC::JSGlobalObject& execState, JSC::JSValue value)
{
    if (RefPtr keyRange = JSIDBKeyRange::toWrapped(execState.vm(), value))
        return { *keyRange };

    if (value.isUndefinedOrNull())
        return IDBKeyRange::unbounded();

    return IDBKeyRange::only(execState, value);
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::only(RefPtr<IDBKey>&& key)
{
    if (!key || !key->isValid())
        return Exception { ExceptionCode::DataError };

    return create(WTF::move(key));
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::only(JSGlobalObject& state, JSValue keyValue)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto key = scriptValueToIDBKey(state, keyValue);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || !key->isValid());
    return only(WTF::move(key));
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::lowerBound(JSGlobalObject& state, JSValue boundValue, bool open)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto bound = scriptValueToIDBKey(state, boundValue);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || !bound->isValid());
    if (!bound->isValid())
        return Exception { ExceptionCode::DataError };

    return create(WTF::move(bound), nullptr, open, true);
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::upperBound(JSGlobalObject& state, JSValue boundValue, bool open)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto bound = scriptValueToIDBKey(state, boundValue);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || !bound->isValid());
    if (!bound->isValid())
        return Exception { ExceptionCode::DataError };

    return create(nullptr, WTF::move(bound), true, open);
}

ExceptionOr<Ref<IDBKeyRange>> IDBKeyRange::bound(JSGlobalObject& state, JSValue lowerValue, JSValue upperValue, bool lowerOpen, bool upperOpen)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto lower = scriptValueToIDBKey(state, lowerValue);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || !lower->isValid());
    if (!lower->isValid())
        return Exception { ExceptionCode::DataError };
    auto upper = scriptValueToIDBKey(state, upperValue);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || !upper->isValid());
    if (!upper->isValid())
        return Exception { ExceptionCode::DataError };
    if (upper->isLessThan(lower.get()))
        return Exception { ExceptionCode::DataError };
    if (upper->isEqual(lower.get()) && (lowerOpen || upperOpen))
        return Exception { ExceptionCode::DataError };

    return create(WTF::move(lower), WTF::move(upper), lowerOpen, upperOpen);
}

// https://w3c.github.io/IndexedDB/#unbounded-key-range.
Ref<IDBKeyRange> IDBKeyRange::unbounded()
{
    return IDBKeyRange::create(nullptr, nullptr, false, false);
}

ExceptionOr<bool> IDBKeyRange::includes(JSC::JSGlobalObject& state, JSC::JSValue keyValue)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto key = scriptValueToIDBKey(state, keyValue);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || !key->isValid());
    if (!key->isValid())
        return Exception { ExceptionCode::DataError, "Failed to execute 'includes' on 'IDBKeyRange': The passed-in value is not a valid IndexedDB key."_s };

    if (m_lower) {
        auto compare = m_lower->compare(key.get());

        if (is_gt(compare))
            return false;
        if (m_isLowerOpen && is_eq(compare))
            return false;
    }

    if (m_upper) {
        auto compare = m_upper->compare(key.get());

        if (is_lt(compare))
            return false;
        if (m_isUpperOpen && is_eq(compare))
            return false;
    }

    return true;
}

// https://w3c.github.io/IndexedDB/#is-a-potentially-valid-key-range
bool IDBKeyRange::isPotentiallyValidKeyRange(JSC::JSGlobalObject& execState, JSC::JSValue value)
{
    if (JSIDBKeyRange::toWrapped(execState.vm(), value))
        return true;
    if (value.isNumber())
        return true;
    if (value.isString())
        return true;
    if (!value.isObject())
        return false;
    if (value.inherits<JSC::DateInstance>())
        return true;
    if (is<JSC::JSArray>(value))
        return true;
    if (is<JSC::JSArrayBuffer>(value))
        return true;
    if (is<JSC::JSArrayBufferView>(value))
        return true;
    return false;
}

} // namespace WebCore
