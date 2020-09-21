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
#include "FinalizationRegistryConstructor.h"

#include "Error.h"
#include "FinalizationRegistryPrototype.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSFinalizationRegistry.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"


namespace JSC {

const ClassInfo FinalizationRegistryConstructor::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FinalizationRegistryConstructor) };

void FinalizationRegistryConstructor::finishCreation(VM& vm, FinalizationRegistryPrototype* prototype)
{
    Base::finishCreation(vm, 1, "FinalizationRegistry"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

static EncodedJSValue JSC_HOST_CALL callFinalizationRegistry(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructFinalizationRegistry(JSGlobalObject*, CallFrame*);

FinalizationRegistryConstructor::FinalizationRegistryConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callFinalizationRegistry, constructFinalizationRegistry)
{
}

static EncodedJSValue JSC_HOST_CALL callFinalizationRegistry(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "FinalizationRegistry"));
}

static EncodedJSValue JSC_HOST_CALL constructFinalizationRegistry(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->argument(0).isCallable(vm))
        return throwVMTypeError(globalObject, scope, "First argument to FinalizationRegistry should be a function"_s);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* finalizationRegistryStructure = callFrame->jsCallee() == newTarget
        ? globalObject->finalizationRegistryStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->finalizationRegistryStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(JSFinalizationRegistry::create(vm, finalizationRegistryStructure, callFrame->uncheckedArgument(0).getObject())));
}

}


