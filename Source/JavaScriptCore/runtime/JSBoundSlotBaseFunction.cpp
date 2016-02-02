/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "JSBoundSlotBaseFunction.h"

#include "CustomGetterSetter.h"
#include "GetterSetter.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"

namespace JSC {

const ClassInfo JSBoundSlotBaseFunction::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(JSBoundSlotBaseFunction) };

EncodedJSValue JSC_HOST_CALL boundSlotBaseFunctionCall(ExecState* exec)
{
    JSBoundSlotBaseFunction* boundSlotBaseFunction = jsCast<JSBoundSlotBaseFunction*>(exec->callee());
    JSObject* baseObject = boundSlotBaseFunction->boundSlotBase();
    CustomGetterSetter* customGetterSetter = boundSlotBaseFunction->customGetterSetter();

    if (boundSlotBaseFunction->isSetter()) {
        callCustomSetter(exec, customGetterSetter, baseObject, exec->thisValue(), exec->argument(0));
        return JSValue::encode(jsUndefined());
    }

    CustomGetterSetter::CustomGetter getter = customGetterSetter->getter();
    if (!getter)
        return JSValue::encode(jsUndefined());

    const String& name = boundSlotBaseFunction->name(exec);
    return getter(exec, baseObject, JSValue::encode(exec->thisValue()), PropertyName(Identifier::fromString(exec, name)));
}

JSBoundSlotBaseFunction::JSBoundSlotBaseFunction(VM& vm, JSGlobalObject* globalObject, Structure* structure, const Type type)
    : Base(vm, globalObject, structure)
    , m_type(type)
{
}

JSBoundSlotBaseFunction* JSBoundSlotBaseFunction::create(VM& vm, JSGlobalObject* globalObject, JSObject* boundSlotBase, CustomGetterSetter* getterSetter, const Type type, const String& name)
{
    NativeExecutable* executable = vm.getHostFunction(boundSlotBaseFunctionCall, callHostFunctionAsConstructor, name);

    JSBoundSlotBaseFunction* function = new (NotNull, allocateCell<JSBoundSlotBaseFunction>(vm.heap)) JSBoundSlotBaseFunction(vm, globalObject, globalObject->boundSlotBaseFunctionStructure(), type);

    // Can't do this during initialization because getHostFunction might do a GC allocation.
    function->finishCreation(vm, executable, boundSlotBase, getterSetter, name);
    return function;
}

void JSBoundSlotBaseFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSBoundSlotBaseFunction* thisObject = jsCast<JSBoundSlotBaseFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_boundSlotBase);
    visitor.append(&thisObject->m_getterSetter);
}

void JSBoundSlotBaseFunction::finishCreation(VM& vm, NativeExecutable* executable, JSObject* boundSlotBase, CustomGetterSetter* getterSetter, const String& name)
{
    Base::finishCreation(vm, executable, isSetter(), name);
    ASSERT(inherits(info()));
    ASSERT(boundSlotBase);
    ASSERT(getterSetter);
    m_boundSlotBase.set(vm, this, boundSlotBase);
    m_getterSetter.set(vm, this, getterSetter);
}

} // namespace JSC
