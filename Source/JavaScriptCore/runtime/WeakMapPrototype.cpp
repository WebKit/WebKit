/*
 * Copyright (C) 2013-2019 Apple, Inc. All rights reserved.
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
#include "WeakMapPrototype.h"

#include "JSCInlines.h"
#include "JSWeakMap.h"

namespace JSC {

const ClassInfo WeakMapPrototype::s_info = { "WeakMap", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakMapPrototype) };

static EncodedJSValue JSC_HOST_CALL protoFuncWeakMapDelete(ExecState*);
static EncodedJSValue JSC_HOST_CALL protoFuncWeakMapGet(ExecState*);
static EncodedJSValue JSC_HOST_CALL protoFuncWeakMapHas(ExecState*);
static EncodedJSValue JSC_HOST_CALL protoFuncWeakMapSet(ExecState*);

void WeakMapPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deleteKeyword, protoFuncWeakMapDelete, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->get, protoFuncWeakMapGet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSWeakMapGetIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->has, protoFuncWeakMapHas, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSWeakMapHasIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, protoFuncWeakMapSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, JSWeakMapSetIntrinsic);

    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(vm, "WeakMap"), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
}

ALWAYS_INLINE static JSWeakMap* getWeakMap(CallFrame* callFrame, JSValue value)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!value.isObject())) {
        throwTypeError(callFrame, scope, "Called WeakMap function on non-object"_s);
        return nullptr;
    }

    auto* map = jsDynamicCast<JSWeakMap*>(vm, asObject(value));
    if (LIKELY(map))
        return map;

    throwTypeError(callFrame, scope, "Called WeakMap function on a non-WeakMap object"_s);
    return nullptr;
}

EncodedJSValue JSC_HOST_CALL protoFuncWeakMapDelete(CallFrame* callFrame)
{
    auto* map = getWeakMap(callFrame, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    return JSValue::encode(jsBoolean(key.isObject() && map->remove(asObject(key))));
}

EncodedJSValue JSC_HOST_CALL protoFuncWeakMapGet(CallFrame* callFrame)
{
    auto* map = getWeakMap(callFrame, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (!key.isObject())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(map->get(asObject(key)));
}

EncodedJSValue JSC_HOST_CALL protoFuncWeakMapHas(CallFrame* callFrame)
{
    auto* map = getWeakMap(callFrame, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    return JSValue::encode(jsBoolean(key.isObject() && map->has(asObject(key))));
}

EncodedJSValue JSC_HOST_CALL protoFuncWeakMapSet(CallFrame* callFrame)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* map = getWeakMap(callFrame, callFrame->thisValue());
    EXCEPTION_ASSERT(!!scope.exception() == !map);
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (!key.isObject())
        return JSValue::encode(throwTypeError(callFrame, scope, "Attempted to set a non-object key in a WeakMap"_s));
    map->set(vm, asObject(key), callFrame->argument(1));
    return JSValue::encode(callFrame->thisValue());
}

}
