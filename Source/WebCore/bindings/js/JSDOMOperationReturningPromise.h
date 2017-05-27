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

#include "JSDOMOperation.h"
#include "JSDOMPromiseDeferred.h"

namespace WebCore {

template<typename JSClass>
class IDLOperationReturningPromise {
public:
    using ClassParameter = JSClass*;
    using Operation = JSC::EncodedJSValue(JSC::ExecState*, ClassParameter, Ref<DeferredPromise>&&, JSC::ThrowScope&);
    using StaticOperation = JSC::EncodedJSValue(JSC::ExecState*, Ref<DeferredPromise>&&, JSC::ThrowScope&);

    template<Operation operation, PromiseExecutionScope executionScope, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::RejectPromise>
    static JSC::EncodedJSValue call(JSC::ExecState& state, const char* operationName)
    {
        return JSC::JSValue::encode(callPromiseFunction<executionScope>(state, [&operationName] (JSC::ExecState& state, Ref<DeferredPromise>&& promise) {
            auto throwScope = DECLARE_THROW_SCOPE(state.vm());
            
            auto* thisObject = IDLOperation<JSClass>::cast(state);
            if (shouldThrow != CastedThisErrorBehavior::Assert && UNLIKELY(!thisObject))
                return rejectPromiseWithThisTypeError(promise.get(), JSClass::info()->className, operationName);
            
            ASSERT(thisObject);
            ASSERT_GC_OBJECT_INHERITS(thisObject, JSClass::info());
            
            // FIXME: We should refactor the binding generated code to use references for state and thisObject.
            return operation(&state, thisObject, WTFMove(promise), throwScope);
        }));
    }

    // This function is a special case for custom operations want to handle the creation of the promise themselves.
    // It is triggered via the extended attribute [ReturnsOwnPromise].
    template<typename IDLOperation<JSClass>::Operation operation, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::RejectPromise>
    static JSC::EncodedJSValue callReturningOwnPromise(JSC::ExecState& state, const char* operationName)
    {
        auto throwScope = DECLARE_THROW_SCOPE(state.vm());

        auto* thisObject = IDLOperation<JSClass>::cast(state);
        if (shouldThrow != CastedThisErrorBehavior::Assert && UNLIKELY(!thisObject))
            return rejectPromiseWithThisTypeError(state, JSClass::info()->className, operationName);

        ASSERT(thisObject);
        ASSERT_GC_OBJECT_INHERITS(thisObject, JSClass::info());

        // FIXME: We should refactor the binding generated code to use references for state and thisObject.
        return operation(&state, thisObject, throwScope);
    }

    template<StaticOperation operation, PromiseExecutionScope executionScope, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::RejectPromise>
    static JSC::EncodedJSValue callStatic(JSC::ExecState& state, const char*)
    {
        return JSC::JSValue::encode(callPromiseFunction<executionScope>(state, [] (JSC::ExecState& state, Ref<DeferredPromise>&& promise) {
            auto throwScope = DECLARE_THROW_SCOPE(state.vm());
            
            // FIXME: We should refactor the binding generated code to use references for state.
            return operation(&state, WTFMove(promise), throwScope);
        }));
    }

    // This function is a special case for custom operations want to handle the creation of the promise themselves.
    // It is triggered via the extended attribute [ReturnsOwnPromise].
    template<typename IDLOperation<JSClass>::StaticOperation operation, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::RejectPromise>
    static JSC::EncodedJSValue callStaticReturningOwnPromise(JSC::ExecState& state, const char*)
    {
        auto throwScope = DECLARE_THROW_SCOPE(state.vm());

        // FIXME: We should refactor the binding generated code to use references for state.
        return operation(&state, throwScope);
    }
};

} // namespace WebCore
