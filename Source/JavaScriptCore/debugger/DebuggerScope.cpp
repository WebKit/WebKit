/*
 * Copyright (C) 2008-2009, 2014 Apple Inc. All rights reserved.
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
#include "DebuggerScope.h"

#include "JSActivation.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DebuggerScope);

const ClassInfo DebuggerScope::s_info = { "DebuggerScope", &Base::s_info, 0, CREATE_METHOD_TABLE(DebuggerScope) };

DebuggerScope::DebuggerScope(VM& vm)
    : JSNonFinalObject(vm, vm.debuggerScopeStructure.get())
{
}

void DebuggerScope::finishCreation(VM& vm, JSObject* activation)
{
    Base::finishCreation(vm);
    ASSERT(activation);
    ASSERT(activation->isActivationObject());
    m_activation.set(vm, this, jsCast<JSActivation*>(activation));
}

void DebuggerScope::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    DebuggerScope* thisObject = jsCast<DebuggerScope*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    JSObject::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_activation);
}

String DebuggerScope::className(const JSObject* object)
{
    const DebuggerScope* thisObject = jsCast<const DebuggerScope*>(object);
    return thisObject->m_activation->methodTable()->className(thisObject->m_activation.get());
}

bool DebuggerScope::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    DebuggerScope* thisObject = jsCast<DebuggerScope*>(object);
    return thisObject->m_activation->methodTable()->getOwnPropertySlot(thisObject->m_activation.get(), exec, propertyName, slot);
}

void DebuggerScope::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    DebuggerScope* thisObject = jsCast<DebuggerScope*>(cell);
    thisObject->m_activation->methodTable()->put(thisObject->m_activation.get(), exec, propertyName, value, slot);
}

bool DebuggerScope::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    DebuggerScope* thisObject = jsCast<DebuggerScope*>(cell);
    return thisObject->m_activation->methodTable()->deleteProperty(thisObject->m_activation.get(), exec, propertyName);
}

void DebuggerScope::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    DebuggerScope* thisObject = jsCast<DebuggerScope*>(object);
    thisObject->m_activation->methodTable()->getPropertyNames(thisObject->m_activation.get(), exec, propertyNames, mode);
}

bool DebuggerScope::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    DebuggerScope* thisObject = jsCast<DebuggerScope*>(object);
    return thisObject->m_activation->methodTable()->defineOwnProperty(thisObject->m_activation.get(), exec, propertyName, descriptor, shouldThrow);
}

} // namespace JSC
