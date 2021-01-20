/*
 * Copyright (C) 2020 Apple, Inc. All rights reserved.
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
#include "FinalizationRegistryPrototype.h"

#include "Error.h"
#include "JSCInlines.h"
#include "JSFinalizationRegistry.h"

namespace JSC {

const ClassInfo FinalizationRegistryPrototype::s_info = { "FinalizationRegistry", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FinalizationRegistryPrototype) };

static JSC_DECLARE_HOST_FUNCTION(protoFuncFinalizationRegistryRegister);
static JSC_DECLARE_HOST_FUNCTION(protoFuncFinalizationRegistryUnregister);

void FinalizationRegistryPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    // We can't make this a property name because it's a resevered word in C++...
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(Identifier::fromString(vm, "register"), protoFuncFinalizationRegistryRegister, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(Identifier::fromString(vm, "unregister"), protoFuncFinalizationRegistryUnregister, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

ALWAYS_INLINE static JSFinalizationRegistry* getFinalizationRegistry(VM& vm, JSGlobalObject* globalObject, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!value.isObject())) {
        throwTypeError(globalObject, scope, "Called FinalizationRegistry function on non-object"_s);
        return nullptr;
    }

    auto* group = jsDynamicCast<JSFinalizationRegistry*>(vm, asObject(value));
    if (LIKELY(group))
        return group;

    throwTypeError(globalObject, scope, "Called FinalizationRegistry function on a non-FinalizationRegistry object"_s);
    return nullptr;
}

JSC_DEFINE_HOST_FUNCTION(protoFuncFinalizationRegistryRegister, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* group = getFinalizationRegistry(vm, globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return throwVMTypeError(globalObject, scope, "register requires an object as the target"_s);

    JSValue holdings = callFrame->argument(1);
    if (sameValue(globalObject, target, holdings))
        return throwVMTypeError(globalObject, scope, "register expects the target object and the holdings parameter are not the same. Otherwise, the target can never be collected"_s);

    JSValue unregisterToken = callFrame->argument(2);
    if (!unregisterToken.isUndefined() && !unregisterToken.isObject())
        return throwVMTypeError(globalObject, scope, "register requires an object as the unregistration token"_s);

    group->registerTarget(vm, target.getObject(), holdings, unregisterToken);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(protoFuncFinalizationRegistryUnregister, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* group = getFinalizationRegistry(vm, globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    if (auto* token = jsDynamicCast<JSObject*>(vm, callFrame->argument(0))) {
        bool result = group->unregister(vm, token);
        return JSValue::encode(jsBoolean(result));
    }

    return throwVMTypeError(globalObject, scope, "unregister requires an object is the unregistration token"_s);
}

}

