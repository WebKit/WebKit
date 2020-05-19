/*
 * Copyright (C) 2019 Apple, Inc. All rights reserved.
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
#include "WeakObjectRefPrototype.h"

#include "JSCInlines.h"
#include "JSWeakObjectRef.h"

namespace JSC {

const ClassInfo WeakObjectRefPrototype::s_info = { "WeakRef", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakObjectRefPrototype) };

static EncodedJSValue JSC_HOST_CALL protoFuncWeakRefDeref(JSGlobalObject*, CallFrame*);

void WeakObjectRefPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    // FIXME: It wouldn't be hard to make this an intrinsic.
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deref, protoFuncWeakRefDeref, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);

    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

ALWAYS_INLINE static JSWeakObjectRef* getWeakRef(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!value.isObject())) {
        throwTypeError(globalObject, scope, "Called WeakRef function on non-object"_s);
        return nullptr;
    }

    auto* ref = jsDynamicCast<JSWeakObjectRef*>(vm, asObject(value));
    if (LIKELY(ref))
        return ref;

    throwTypeError(globalObject, scope, "Called WeakRef function on a non-WeakRef object"_s);
    return nullptr;
}

EncodedJSValue JSC_HOST_CALL protoFuncWeakRefDeref(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto* ref = getWeakRef(globalObject, callFrame->thisValue());
    if (!ref)
        return JSValue::encode(jsUndefined());

    auto* value = ref->deref(vm);
    return value ? JSValue::encode(value) : JSValue::encode(jsNull());
}

}
