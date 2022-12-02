/*
 * Copyright (C) 2015-2017 Apple, Inc. All rights reserved.
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
#include "WeakSetConstructor.h"

#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSWeakSet.h"
#include "WeakMapImplInlines.h"
#include "WeakSetPrototype.h"

namespace JSC {

const ClassInfo WeakSetConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakSetConstructor) };

void WeakSetConstructor::finishCreation(VM& vm, WeakSetPrototype* prototype)
{
    Base::finishCreation(vm, 0, "WeakSet"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

static JSC_DECLARE_HOST_FUNCTION(callWeakSet);
static JSC_DECLARE_HOST_FUNCTION(constructWeakSet);

WeakSetConstructor::WeakSetConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callWeakSet, constructWeakSet)
{
}

JSC_DEFINE_HOST_FUNCTION(callWeakSet, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WeakSet"));
}

JSC_DEFINE_HOST_FUNCTION(constructWeakSet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* weakSetStructure = JSC_GET_DERIVED_STRUCTURE(vm, weakSetStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    JSWeakSet* weakSet = JSWeakSet::create(vm, weakSetStructure);
    JSValue iterable = callFrame->argument(0);
    if (iterable.isUndefinedOrNull())
        return JSValue::encode(weakSet);

    JSValue adderFunction = weakSet->JSObject::get(globalObject, vm.propertyNames->add);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto adderFunctionCallData = JSC::getCallData(adderFunction);
    if (adderFunctionCallData.type == CallData::Type::None)
        return throwVMTypeError(globalObject, scope, "'add' property of a WeakSet should be callable."_s);

    bool canPerformFastAdd = adderFunctionCallData.type == CallData::Type::Native && adderFunctionCallData.native.function == protoFuncWeakSetAdd;

    scope.release();
    forEachInIterable(globalObject, iterable, [&](VM&, JSGlobalObject* globalObject, JSValue nextValue) {
        if (canPerformFastAdd) {
            if (UNLIKELY(!canBeHeldWeakly(nextValue))) {
                throwTypeError(asObject(adderFunction)->globalObject(), scope, WeakSetInvalidValueError);
                return;
            }
            weakSet->add(vm, nextValue.asCell());
            return;
        }

        MarkedArgumentBuffer arguments;
        arguments.append(nextValue);
        ASSERT(!arguments.hasOverflowed());
        call(globalObject, adderFunction, adderFunctionCallData, weakSet, arguments);
    });

    return JSValue::encode(weakSet);
}

}
