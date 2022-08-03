/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSCustomSetterFunction.h"

#include "IdentifierInlines.h"
#include "JSCJSValueInlines.h"

namespace JSC {

const ClassInfo JSCustomSetterFunction::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSCustomSetterFunction) };
static JSC_DECLARE_HOST_FUNCTION(customSetterFunctionCall);

JSC_DEFINE_HOST_FUNCTION(customSetterFunctionCall, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto customSetterFunction = jsCast<JSCustomSetterFunction*>(callFrame->jsCallee());
    auto setter = customSetterFunction->setter();
    setter(globalObject, JSValue::encode(callFrame->thisValue()), JSValue::encode(callFrame->argument(0)), customSetterFunction->propertyName());
    return JSValue::encode(jsUndefined());
}

JSCustomSetterFunction::JSCustomSetterFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, const PropertyName& propertyName, CustomFunctionPointer setter)
    : Base(vm, executable, globalObject, structure)
    , m_propertyName(Identifier::fromUid(vm, propertyName.uid()))
    , m_setter(setter)
{
}

JSCustomSetterFunction* JSCustomSetterFunction::create(VM& vm, JSGlobalObject* globalObject, const PropertyName& propertyName, CustomFunctionPointer setter)
{
    ASSERT(setter);
    NativeExecutable* executable = vm.getHostFunction(customSetterFunctionCall, ImplementationVisibility::Public, callHostFunctionAsConstructor, String(propertyName.publicName()));
    Structure* structure = globalObject->customSetterFunctionStructure();
    JSCustomSetterFunction* function = new (NotNull, allocateCell<JSCustomSetterFunction>(vm)) JSCustomSetterFunction(vm, executable, globalObject, structure, propertyName, setter);

    // Can't do this during initialization because getHostFunction might do a GC allocation.
    auto name = makeString("set ", propertyName.publicName());
    function->finishCreation(vm, executable, 1, name);
    return function;
}

void JSCustomSetterFunction::destroy(JSCell* cell)
{
    static_cast<JSCustomSetterFunction*>(cell)->~JSCustomSetterFunction();
}

} // namespace JSC
