/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

// Lightweight alternative to JSCJSValueInlines.h for code that only needs
// JSValue property access (get/getPropertySlot/getOwnPropertySlot).
// Avoids the heavy transitive includes that JSCJSValueInlines.h pulls in
// (JSStringInlines.h, JSCellInlines.h, JSBigInt.h, etc.).

#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/GetVM.h>
#include <JavaScriptCore/JSCJSValueCell.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSString.h>

namespace JSC {

ALWAYS_INLINE JSValue JSValue::get(JSGlobalObject* globalObject, PropertyName propertyName) const
{
    PropertySlot slot(asValue(), PropertySlot::InternalMethodType::Get);
    return get(globalObject, propertyName, slot);
}

ALWAYS_INLINE JSValue JSValue::get(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot) const
{
    auto scope = DECLARE_THROW_SCOPE(getVM(globalObject));
    bool hasSlot = getPropertySlot(globalObject, propertyName, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasSlot);
    if (!hasSlot)
        return jsUndefined();
    RELEASE_AND_RETURN(scope, slot.getValue(globalObject, propertyName));
}

template<typename CallbackWhenNoException>
ALWAYS_INLINE typename std::invoke_result<CallbackWhenNoException, bool, PropertySlot&>::type JSValue::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, CallbackWhenNoException callback) const
{
    PropertySlot slot(asValue(), PropertySlot::InternalMethodType::Get);
    return getPropertySlot(globalObject, propertyName, slot, callback);
}

template<typename CallbackWhenNoException>
ALWAYS_INLINE typename std::invoke_result<CallbackWhenNoException, bool, PropertySlot&>::type JSValue::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot, CallbackWhenNoException callback) const
{
    auto scope = DECLARE_THROW_SCOPE(getVM(globalObject));
    bool found = getPropertySlot(globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, callback(found, slot));
}

ALWAYS_INLINE bool JSValue::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot) const
{
    auto scope = DECLARE_THROW_SCOPE(getVM(globalObject));
    // If this is a primitive, we'll need to synthesize the prototype -
    // and if it's a string there are special properties to check first.
    JSObject* object;
    if (!isObject()) [[unlikely]] {
        if (isString()) {
            bool hasProperty = asString(*this)->getStringPropertySlot(globalObject, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, false);
            if (hasProperty)
                return true;
        }
        object = synthesizePrototype(globalObject);
        EXCEPTION_ASSERT(!!scope.exception() == !object);
        if (!object) [[unlikely]]
            return false;
    } else
        object = asObject(asCell());

    RELEASE_AND_RETURN(scope, object->getPropertySlot(globalObject, propertyName, slot));
}

ALWAYS_INLINE bool JSValue::getOwnPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot) const
{
    // If this is a primitive, we'll need to synthesize the prototype -
    // and if it's a string there are special properties to check first.
    auto scope = DECLARE_THROW_SCOPE(getVM(globalObject));
    if (!isObject()) [[unlikely]] {
        if (isString())
            RELEASE_AND_RETURN(scope, asString(*this)->getStringPropertySlot(globalObject, propertyName, slot));

        if (isUndefinedOrNull())
            throwException(globalObject, scope, createNotAnObjectError(globalObject, *this));
        return false;
    }
    RELEASE_AND_RETURN(scope, asObject(asCell())->getOwnPropertySlotInline(globalObject, propertyName, slot));
}

ALWAYS_INLINE JSValue JSValue::get(JSGlobalObject* globalObject, unsigned propertyName) const
{
    PropertySlot slot(asValue(), PropertySlot::InternalMethodType::Get);
    return get(globalObject, propertyName, slot);
}

ALWAYS_INLINE JSValue JSValue::get(JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot) const
{
    auto scope = DECLARE_THROW_SCOPE(getVM(globalObject));
    // If this is a primitive, we'll need to synthesize the prototype -
    // and if it's a string there are special properties to check first.
    JSObject* object;
    if (!isObject()) [[unlikely]] {
        if (isString()) {
            bool hasProperty = asString(*this)->getStringPropertySlot(globalObject, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, { });
            if (hasProperty)
                RELEASE_AND_RETURN(scope, slot.getValue(globalObject, propertyName));
        }
        object = synthesizePrototype(globalObject);
        EXCEPTION_ASSERT(!!scope.exception() == !object);
        if (!object) [[unlikely]]
            return JSValue();
    } else
        object = asObject(asCell());

    bool hasSlot = object->getPropertySlot(globalObject, propertyName, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasSlot);
    if (!hasSlot)
        return jsUndefined();
    RELEASE_AND_RETURN(scope, slot.getValue(globalObject, propertyName));
}

ALWAYS_INLINE JSValue JSValue::get(JSGlobalObject* globalObject, uint64_t propertyName) const
{
    if (propertyName <= std::numeric_limits<unsigned>::max()) [[likely]]
        return get(globalObject, static_cast<unsigned>(propertyName));
    return get(globalObject, Identifier::from(getVM(globalObject), static_cast<double>(propertyName)));
}

template<typename T, typename PropertyNameType>
ALWAYS_INLINE T JSValue::getAs(JSGlobalObject* globalObject, PropertyNameType propertyName) const
{
    JSValue value = get(globalObject, propertyName);
#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    VM& vm = getVM(globalObject);
    if (vm.exceptionForInspection())
        return nullptr;
#endif
    return uncheckedDowncast<std::remove_pointer_t<T>>(value);
}

inline JSString* JSValue::toString(JSGlobalObject* globalObject) const
{
    if (isString())
        return asString(asCell());
    bool returnEmptyStringOnError = true;
    return toStringSlowCase(globalObject, returnEmptyStringOnError);
}

inline JSString* JSValue::toStringOrNull(JSGlobalObject* globalObject) const
{
    if (isString())
        return asString(asCell());
    bool returnEmptyStringOnError = false;
    return toStringSlowCase(globalObject, returnEmptyStringOnError);
}

inline String JSValue::toWTFString(JSGlobalObject* globalObject) const
{
    if (isString())
        return asString(asCell())->value(globalObject);
    return toWTFStringSlowCase(globalObject);
}

} // namespace JSC
