/*
 * Copyright (C) 2015-2019 Apple, Inc. All rights reserved.
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
#include "WeakSetPrototype.h"

#include "JSCInlines.h"
#include "JSWeakSet.h"

namespace JSC {

const ClassInfo WeakSetPrototype::s_info = { "WeakSet", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakSetPrototype) };

static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakSetDelete);
static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakSetHas);
static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakSetAdd);

void WeakSetPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deleteKeyword, protoFuncWeakSetDelete, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->has, protoFuncWeakSetHas, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSWeakSetHasIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->add, protoFuncWeakSetAdd, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSWeakSetAddIntrinsic);

    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

ALWAYS_INLINE static JSWeakSet* getWeakSet(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!value.isObject())) {
        throwTypeError(globalObject, scope, "Called WeakSet function on non-object"_s);
        return nullptr;
    }

    auto* set = jsDynamicCast<JSWeakSet*>(vm, asObject(value));
    if (LIKELY(set))
        return set;

    throwTypeError(globalObject, scope, "Called WeakSet function on a non-WeakSet object"_s);
    return nullptr;
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakSetDelete, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* set = getWeakSet(globalObject, callFrame->thisValue());
    if (!set)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    return JSValue::encode(jsBoolean(key.isObject() && set->remove(asObject(key))));
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakSetHas, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* set = getWeakSet(globalObject, callFrame->thisValue());
    if (!set)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    return JSValue::encode(jsBoolean(key.isObject() && set->has(asObject(key))));
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakSetAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* set = getWeakSet(globalObject, callFrame->thisValue());
    EXCEPTION_ASSERT(!!scope.exception() == !set);
    if (!set)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (!key.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Attempted to add a non-object key to a WeakSet"_s));
    set->add(vm, asObject(key));
    return JSValue::encode(callFrame->thisValue());
}

}
