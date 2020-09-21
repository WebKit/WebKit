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
#include "WeakObjectRefConstructor.h"

#include "JSCInlines.h"
#include "JSWeakObjectRef.h"
#include "WeakObjectRefPrototype.h"

namespace JSC {

const ClassInfo WeakObjectRefConstructor::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakObjectRefConstructor) };

void WeakObjectRefConstructor::finishCreation(VM& vm, WeakObjectRefPrototype* prototype)
{
    Base::finishCreation(vm, 1, "WeakRef"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

static EncodedJSValue JSC_HOST_CALL callWeakRef(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructWeakRef(JSGlobalObject*, CallFrame*);

WeakObjectRefConstructor::WeakObjectRefConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callWeakRef, constructWeakRef)
{
}

static EncodedJSValue JSC_HOST_CALL callWeakRef(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WeakRef"));
}

static EncodedJSValue JSC_HOST_CALL constructWeakRef(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->argument(0).isObject())
        return throwVMTypeError(globalObject, scope, "First argument to WeakRef should be an object"_s);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* weakObjectRefStructure = newTarget == callFrame->jsCallee()
        ? globalObject->weakObjectRefStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->weakObjectRefStructure());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(JSWeakObjectRef::create(vm, weakObjectRefStructure, callFrame->uncheckedArgument(0).getObject())));
}

}

