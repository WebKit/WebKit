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
#include "WeakMapPrototype.h"

#include "JSCInlines.h"
#include "JSWeakMapInlines.h"

namespace JSC {

const ASCIILiteral WeakMapInvalidKeyError { "WeakMap keys must be objects or non-registered symbols"_s };

const ClassInfo WeakMapPrototype::s_info = { "WeakMap"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakMapPrototype) };

static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakMapDelete);
static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakMapGet);
static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakMapHas);

void WeakMapPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deleteKeyword, protoFuncWeakMapDelete, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->get, protoFuncWeakMapGet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, JSWeakMapGetIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->has, protoFuncWeakMapHas, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, JSWeakMapHasIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, protoFuncWeakMapSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public, JSWeakMapSetIntrinsic);

    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

ALWAYS_INLINE static JSWeakMap* getWeakMap(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!value.isObject())) {
        throwTypeError(globalObject, scope, "Called WeakMap function on non-object"_s);
        return nullptr;
    }

    auto* map = jsDynamicCast<JSWeakMap*>(asObject(value));
    if (LIKELY(map))
        return map;

    throwTypeError(globalObject, scope, "Called WeakMap function on a non-WeakMap object"_s);
    return nullptr;
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakMapDelete, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* map = getWeakMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (UNLIKELY(!key.isCell()))
        return JSValue::encode(jsBoolean(false));
    return JSValue::encode(jsBoolean(map->remove(key.asCell())));
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakMapGet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* map = getWeakMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (UNLIKELY(!key.isCell()))
        return JSValue::encode(jsUndefined());
    return JSValue::encode(map->get(key.asCell()));
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakMapHas, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* map = getWeakMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (UNLIKELY(!key.isCell()))
        return JSValue::encode(jsBoolean(false));
    return JSValue::encode(jsBoolean(map->has(key.asCell())));
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakMapSet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* map = getWeakMap(globalObject, callFrame->thisValue());
    EXCEPTION_ASSERT(!!scope.exception() == !map);
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (UNLIKELY(!canBeHeldWeakly(key)))
        return throwVMTypeError(globalObject, scope, WeakMapInvalidKeyError);
    map->set(vm, key.asCell(), callFrame->argument(1));
    return JSValue::encode(callFrame->thisValue());
}

}
