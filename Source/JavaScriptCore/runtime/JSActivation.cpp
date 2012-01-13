/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "config.h"
#include "JSActivation.h"

#include "Arguments.h"
#include "Interpreter.h"
#include "JSFunction.h"

using namespace std;

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSActivation);

const ClassInfo JSActivation::s_info = { "JSActivation", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSActivation) };

JSActivation::JSActivation(CallFrame* callFrame, FunctionExecutable* functionExecutable)
    : Base(callFrame->globalData(), callFrame->globalData().activationStructure.get(), functionExecutable->symbolTable(), callFrame->registers())
    , m_numCapturedArgs(max(callFrame->argumentCount(), functionExecutable->parameterCount()))
    , m_numCapturedVars(functionExecutable->capturedVariableCount())
    , m_requiresDynamicChecks(functionExecutable->usesEval() && !functionExecutable->isStrictMode())
    , m_argumentsRegister(functionExecutable->generatedBytecode().argumentsRegister())
{
}

void JSActivation::finishCreation(CallFrame* callFrame)
{
    Base::finishCreation(callFrame->globalData());
    ASSERT(inherits(&s_info));

    // We have to manually ref and deref the symbol table as JSVariableObject
    // doesn't know about SharedSymbolTable
    static_cast<SharedSymbolTable*>(m_symbolTable)->ref();
    callFrame->globalData().heap.addFinalizer(this, &finalize);
}

void JSActivation::finalize(JSCell* cell)
{
    static_cast<SharedSymbolTable*>(jsCast<JSActivation*>(cell)->m_symbolTable)->deref();
}

void JSActivation::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSActivation* thisObject = jsCast<JSActivation*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(thisObject, visitor);

    // No need to mark our registers if they're still in the RegisterFile.
    WriteBarrier<Unknown>* registerArray = thisObject->m_registerArray.get();
    if (!registerArray)
        return;

    visitor.appendValues(registerArray, thisObject->m_numCapturedArgs);

    // Skip 'this' and call frame.
    visitor.appendValues(registerArray + CallFrame::offsetFor(thisObject->m_numCapturedArgs + 1), thisObject->m_numCapturedVars);
}

inline bool JSActivation::symbolTableGet(const Identifier& propertyName, PropertySlot& slot)
{
    SymbolTableEntry entry = symbolTable().inlineGet(propertyName.impl());
    if (entry.isNull())
        return false;
    if (entry.getIndex() >= m_numCapturedVars)
        return false;

    slot.setValue(registerAt(entry.getIndex()).get());
    return true;
}

inline bool JSActivation::symbolTablePut(ExecState* exec, const Identifier& propertyName, JSValue value, bool shouldThrow)
{
    JSGlobalData& globalData = exec->globalData();
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    
    SymbolTableEntry entry = symbolTable().inlineGet(propertyName.impl());
    if (entry.isNull())
        return false;
    if (entry.isReadOnly()) {
        if (shouldThrow)
            throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
        return true;
    }
    if (entry.getIndex() >= m_numCapturedVars)
        return false;

    registerAt(entry.getIndex()).set(globalData, this, value);
    return true;
}

void JSActivation::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSActivation* thisObject = jsCast<JSActivation*>(object);
    SymbolTable::const_iterator end = thisObject->symbolTable().end();
    for (SymbolTable::const_iterator it = thisObject->symbolTable().begin(); it != end; ++it) {
        if (it->second.getAttributes() & DontEnum && mode != IncludeDontEnumProperties)
            continue;
        if (it->second.getIndex() >= thisObject->m_numCapturedVars)
            continue;
        propertyNames.add(Identifier(exec, it->first.get()));
    }
    // Skip the JSVariableObject implementation of getOwnPropertyNames
    JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

inline bool JSActivation::symbolTablePutWithAttributes(JSGlobalData& globalData, const Identifier& propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    
    SymbolTable::iterator iter = symbolTable().find(propertyName.impl());
    if (iter == symbolTable().end())
        return false;
    SymbolTableEntry& entry = iter->second;
    ASSERT(!entry.isNull());
    if (entry.getIndex() >= m_numCapturedVars)
        return false;

    entry.setAttributes(attributes);
    registerAt(entry.getIndex()).set(globalData, this, value);
    return true;
}

bool JSActivation::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSActivation* thisObject = jsCast<JSActivation*>(cell);
    if (propertyName == exec->propertyNames().arguments) {
        slot.setCustom(thisObject, thisObject->getArgumentsGetter());
        return true;
    }

    if (thisObject->symbolTableGet(propertyName, slot))
        return true;

    if (WriteBarrierBase<Unknown>* location = thisObject->getDirectLocation(exec->globalData(), propertyName)) {
        slot.setValue(location->get());
        return true;
    }

    // We don't call through to JSObject because there's no way to give an 
    // activation object getter properties or a prototype.
    ASSERT(!thisObject->hasGetterSetterProperties());
    ASSERT(thisObject->prototype().isNull());
    return false;
}

void JSActivation::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    JSActivation* thisObject = jsCast<JSActivation*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (thisObject->symbolTablePut(exec, propertyName, value, slot.isStrictMode()))
        return;

    // We don't call through to JSObject because __proto__ and getter/setter 
    // properties are non-standard extensions that other implementations do not
    // expose in the activation object.
    ASSERT(!thisObject->hasGetterSetterProperties());
    thisObject->putOwnDataProperty(exec->globalData(), propertyName, value, slot);
}

// FIXME: Make this function honor ReadOnly (const) and DontEnum
void JSActivation::putDirectVirtual(JSObject* object, ExecState* exec, const Identifier& propertyName, JSValue value, unsigned attributes)
{
    JSActivation* thisObject = jsCast<JSActivation*>(object);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (thisObject->symbolTablePutWithAttributes(exec->globalData(), propertyName, value, attributes))
        return;

    // We don't call through to JSObject because __proto__ and getter/setter 
    // properties are non-standard extensions that other implementations do not
    // expose in the activation object.
    ASSERT(!thisObject->hasGetterSetterProperties());
    JSObject::putDirectVirtual(thisObject, exec, propertyName, value, attributes);
}

bool JSActivation::deleteProperty(JSCell* cell, ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().arguments)
        return false;

    return Base::deleteProperty(cell, exec, propertyName);
}

JSObject* JSActivation::toThisObject(JSCell*, ExecState* exec)
{
    return exec->globalThisValue();
}

JSValue JSActivation::argumentsGetter(ExecState*, JSValue slotBase, const Identifier&)
{
    JSActivation* activation = asActivation(slotBase);
    CallFrame* callFrame = CallFrame::create(reinterpret_cast<Register*>(activation->m_registers));
    int argumentsRegister = activation->m_argumentsRegister;
    if (JSValue arguments = callFrame->uncheckedR(argumentsRegister).jsValue())
        return arguments;
    int realArgumentsRegister = unmodifiedArgumentsRegister(argumentsRegister);

    JSValue arguments = JSValue(Arguments::create(callFrame->globalData(), callFrame));
    callFrame->uncheckedR(argumentsRegister) = arguments;
    callFrame->uncheckedR(realArgumentsRegister) = arguments;
    
    ASSERT(callFrame->uncheckedR(realArgumentsRegister).jsValue().inherits(&Arguments::s_info));
    return callFrame->uncheckedR(realArgumentsRegister).jsValue();
}

// These two functions serve the purpose of isolating the common case from a
// PIC branch.

PropertySlot::GetValueFunc JSActivation::getArgumentsGetter()
{
    return argumentsGetter;
}

} // namespace JSC
