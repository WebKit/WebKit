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

#include "CachedCallInlines.h"
#include "InterpreterInlines.h"
#include "JSCInlines.h"
#include "JSWeakMapInlines.h"
#include "VMEntryScopeInlines.h"

namespace JSC {

const ASCIILiteral WeakMapInvalidKeyError { "WeakMap keys must be objects or non-registered symbols"_s };

const ClassInfo WeakMapPrototype::s_info = { "WeakMap"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakMapPrototype) };

static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakMapDelete);
static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakMapGet);
static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakMapHas);
static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakMapGetOrInsert);
static JSC_DECLARE_HOST_FUNCTION(protoFuncWeakMapGetOrInsertComputed);

void WeakMapPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deleteKeyword, protoFuncWeakMapDelete, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->get, protoFuncWeakMapGet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, JSWeakMapGetIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->has, protoFuncWeakMapHas, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, JSWeakMapHasIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, protoFuncWeakMapSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public, JSWeakMapSetIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("getOrInsert"_s, protoFuncWeakMapGetOrInsert, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("getOrInsertComputed"_s, protoFuncWeakMapGetOrInsertComputed, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

ALWAYS_INLINE static JSWeakMap* getWeakMap(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject()) [[unlikely]] {
        throwTypeError(globalObject, scope, "Called WeakMap function on non-object"_s);
        return nullptr;
    }

    if (auto* map = jsDynamicCast<JSWeakMap*>(asObject(value))) [[likely]]
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
    if (!key.isCell()) [[unlikely]]
        return JSValue::encode(jsBoolean(false));
    return JSValue::encode(jsBoolean(map->remove(key.asCell())));
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakMapGet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* map = getWeakMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (!key.isCell()) [[unlikely]]
        return JSValue::encode(jsUndefined());
    return JSValue::encode(map->get(key.asCell()));
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakMapHas, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* map = getWeakMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    JSValue key = callFrame->argument(0);
    if (!key.isCell()) [[unlikely]]
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
    if (!canBeHeldWeakly(key)) [[unlikely]]
        return throwVMTypeError(globalObject, scope, WeakMapInvalidKeyError);
    map->set(vm, key.asCell(), callFrame->argument(1));
    return JSValue::encode(callFrame->thisValue());
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakMapGetOrInsert, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* map = getWeakMap(globalObject, callFrame->thisValue());
    EXCEPTION_ASSERT(!!scope.exception() == !map);
    if (!map)
        return JSValue::encode(jsUndefined());

    JSValue key = callFrame->argument(0);
    if (!canBeHeldWeakly(key)) [[unlikely]]
        return throwVMTypeError(globalObject, scope, WeakMapInvalidKeyError);

    JSCell* keyCell = key.asCell();

    auto hash = jsWeakMapHash(keyCell);

    JSValue value;

    {
        AssertNoGC assertNoGC;

        auto [index, exists] = map->findBucketIndex(keyCell, hash);
        if (exists)
            value = map->getBucket(keyCell, hash, index);
        else {
            value = callFrame->argument(1);
            map->addBucket(vm, keyCell, value, hash, index);
        }
    }

    return JSValue::encode(value);
}

JSC_DEFINE_HOST_FUNCTION(protoFuncWeakMapGetOrInsertComputed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* map = getWeakMap(globalObject, callFrame->thisValue());
    EXCEPTION_ASSERT(!!scope.exception() == !map);
    if (!map)
        return JSValue::encode(jsUndefined());

    JSValue key = callFrame->argument(0);
    if (!canBeHeldWeakly(key)) [[unlikely]]
        return throwVMTypeError(globalObject, scope, WeakMapInvalidKeyError);

    JSValue valueCallback = callFrame->argument(1);
    auto callData = JSC::getCallDataInline(valueCallback);
    if (callData.type == CallData::Type::None) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "WeakMap.prototype.getOrInsertComputed requires the callback argument to be callable."_s);

    JSCell* keyCell = key.asCell();

    auto hash = jsWeakMapHash(keyCell);
    {
        AssertNoGC assertNoGC;
        auto [index, exists] = map->findBucketIndex(keyCell, hash);
        if (exists)
            return JSValue::encode(map->getBucket(keyCell, hash, index));
    }

    JSValue value;
    if (callData.type == CallData::Type::JS) [[likely]] {
        CachedCall cachedCall(globalObject, jsCast<JSFunction*>(valueCallback), 1);
        RETURN_IF_EXCEPTION(scope, { });

        value = cachedCall.callWithArguments(globalObject, jsUndefined(), key);
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        MarkedArgumentBuffer args;
        args.append(key);
        ASSERT(!args.hasOverflowed());

        value = call(globalObject, valueCallback, callData, jsUndefined(), args);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // FIXME: rdar://145147128 can we optimize this more to detect a rehash like Map does?
    {
        // Call to valueCallback can modify our state, so we need to check if we re-hashed
        AssertNoGC assertNoGC;
        map->add(vm, keyCell, value, hash);
    }

    return JSValue::encode(value);
}

}
