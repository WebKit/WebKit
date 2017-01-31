/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2006, 2008-2009, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *  Copyright (C) 2012 Ericsson AB. All rights reserved.
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "JSDOMExceptionHandling.h"

namespace WebCore {

enum class CastedThisErrorBehavior { Throw, ReturnEarly, RejectPromise, Assert };

template<typename JSClass>
struct BindingCaller {
    using AttributeSetterFunction = bool(JSC::ExecState&, JSClass&, JSC::JSValue, JSC::ThrowScope&);
    using AttributeGetterFunction = JSC::JSValue(JSC::ExecState&, JSClass&, JSC::ThrowScope&);
    using OperationCallerFunction = JSC::EncodedJSValue(JSC::ExecState*, JSClass*, JSC::ThrowScope&);
    using PromiseOperationCallerFunction = JSC::EncodedJSValue(JSC::ExecState*, JSClass*, Ref<DeferredPromise>&&, JSC::ThrowScope&);

    static JSClass* castForAttribute(JSC::ExecState&, JSC::EncodedJSValue);
    static JSClass* castForOperation(JSC::ExecState&);

    template<PromiseOperationCallerFunction operationCaller, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::RejectPromise>
    static JSC::EncodedJSValue callPromiseOperation(JSC::ExecState* state, Ref<DeferredPromise>&& promise, const char* operationName)
    {
        ASSERT(state);
        auto throwScope = DECLARE_THROW_SCOPE(state->vm());
        auto* thisObject = castForOperation(*state);
        if (shouldThrow != CastedThisErrorBehavior::Assert && UNLIKELY(!thisObject))
            return rejectPromiseWithThisTypeError(promise.get(), JSClass::info()->className, operationName);
        ASSERT(thisObject);
        ASSERT_GC_OBJECT_INHERITS(thisObject, JSClass::info());
        // FIXME: We should refactor the binding generated code to use references for state and thisObject.
        return operationCaller(state, thisObject, WTFMove(promise), throwScope);
    }

    template<OperationCallerFunction operationCaller, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::Throw>
    static JSC::EncodedJSValue callOperation(JSC::ExecState* state, const char* operationName)
    {
        ASSERT(state);
        auto throwScope = DECLARE_THROW_SCOPE(state->vm());
        auto* thisObject = castForOperation(*state);
        if (shouldThrow != CastedThisErrorBehavior::Assert && UNLIKELY(!thisObject)) {
            if (shouldThrow == CastedThisErrorBehavior::Throw)
                return throwThisTypeError(*state, throwScope, JSClass::info()->className, operationName);
            // For custom promise-returning operations
            return rejectPromiseWithThisTypeError(*state, JSClass::info()->className, operationName);
        }
        ASSERT(thisObject);
        ASSERT_GC_OBJECT_INHERITS(thisObject, JSClass::info());
        // FIXME: We should refactor the binding generated code to use references for state and thisObject.
        return operationCaller(state, thisObject, throwScope);
    }

    template<AttributeSetterFunction setter, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::Throw>
    static bool setAttribute(JSC::ExecState* state, JSC::EncodedJSValue thisValue, JSC::EncodedJSValue encodedValue, const char* attributeName)
    {
        ASSERT(state);
        auto throwScope = DECLARE_THROW_SCOPE(state->vm());
        auto* thisObject = castForAttribute(*state, thisValue);
        if (UNLIKELY(!thisObject))
            return (shouldThrow == CastedThisErrorBehavior::Throw) ? throwSetterTypeError(*state, throwScope, JSClass::info()->className, attributeName) : false;
        return setter(*state, *thisObject, JSC::JSValue::decode(encodedValue), throwScope);
    }

    template<AttributeGetterFunction getter, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::Throw>
    static JSC::EncodedJSValue attribute(JSC::ExecState* state, JSC::EncodedJSValue thisValue, const char* attributeName)
    {
        ASSERT(state);
        auto throwScope = DECLARE_THROW_SCOPE(state->vm());
        auto* thisObject = castForAttribute(*state, thisValue);
        if (UNLIKELY(!thisObject)) {
            if (shouldThrow == CastedThisErrorBehavior::Throw)
                return throwGetterTypeError(*state, throwScope, JSClass::info()->className, attributeName);
            if (shouldThrow == CastedThisErrorBehavior::RejectPromise)
                return rejectPromiseWithGetterTypeError(*state, JSClass::info()->className, attributeName);
            return JSC::JSValue::encode(JSC::jsUndefined());
        }
        return JSC::JSValue::encode(getter(*state, *thisObject, throwScope));
    }
};

} // namespace WebCore
