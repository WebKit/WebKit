/*
 * Copyright (C) 2013-2021 Apple, Inc. All rights reserved.
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

#include "config.h"
#include "WeakMapConstructor.h"

#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSWeakMapInlines.h"
#include "WeakMapPrototype.h"

namespace JSC {

const ClassInfo WeakMapConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakMapConstructor) };

void WeakMapConstructor::finishCreation(VM& vm, WeakMapPrototype* prototype)
{
    Base::finishCreation(vm, 0, "WeakMap"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

static JSC_DECLARE_HOST_FUNCTION(callWeakMap);
static JSC_DECLARE_HOST_FUNCTION(constructWeakMap);

WeakMapConstructor::WeakMapConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callWeakMap, constructWeakMap)
{
}

JSC_DEFINE_HOST_FUNCTION(callWeakMap, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WeakMap"));
}

JSC_DEFINE_HOST_FUNCTION(constructWeakMap, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* weakMapStructure = JSC_GET_DERIVED_STRUCTURE(vm, weakMapStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    JSWeakMap* weakMap = JSWeakMap::create(vm, weakMapStructure);
    JSValue iterable = callFrame->argument(0);
    if (iterable.isUndefinedOrNull())
        return JSValue::encode(weakMap);

    JSValue adderFunction = weakMap->JSObject::get(globalObject, vm.propertyNames->set);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto adderFunctionCallData = JSC::getCallData(adderFunction);
    if (adderFunctionCallData.type == CallData::Type::None)
        return throwVMTypeError(globalObject, scope, "'set' property of a WeakMap should be callable."_s);

    bool canPerformFastSet = adderFunctionCallData.type == CallData::Type::Native && adderFunctionCallData.native.function == protoFuncWeakMapSet;

    scope.release();
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue nextItem) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (!nextItem.isObject()) {
            throwTypeError(globalObject, scope);
            return;
        }

        JSObject* nextObject = asObject(nextItem);

        JSValue key = nextObject->getIndex(globalObject, static_cast<unsigned>(0));
        RETURN_IF_EXCEPTION(scope, void());

        JSValue value = nextObject->getIndex(globalObject, static_cast<unsigned>(1));
        RETURN_IF_EXCEPTION(scope, void());

        if (canPerformFastSet) {
            if (UNLIKELY(!canBeHeldWeakly(key))) {
                throwTypeError(asObject(adderFunction)->globalObject(), scope, WeakMapInvalidKeyError);
                return;
            }
            weakMap->set(vm, key.asCell(), value);
            return;
        }

        MarkedArgumentBuffer arguments;
        arguments.append(key);
        arguments.append(value);
        ASSERT(!arguments.hasOverflowed());
        scope.release();
        call(globalObject, adderFunction, adderFunctionCallData, weakMap, arguments);
    });

    return JSValue::encode(weakMap);
}

}
