/*
 * Copyright (C) 2008, 2009, 2014 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "JSLexicalEnvironment.h"

#include "Arguments.h"
#include "Interpreter.h"
#include "JSFunction.h"
#include "JSCInlines.h"

using namespace std;

namespace JSC {

const ClassInfo JSLexicalEnvironment::s_info = { "JSLexicalEnvironment", &Base::s_info, 0, CREATE_METHOD_TABLE(JSLexicalEnvironment) };

void JSLexicalEnvironment::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    // No need to mark our registers if they're still in the JSStack.
    if (!thisObject->isTornOff())
        return;

    for (int i = 0; i < thisObject->symbolTable()->captureCount(); ++i)
        visitor.append(&thisObject->storage()[i]);
}

inline bool JSLexicalEnvironment::symbolTableGet(PropertyName propertyName, PropertySlot& slot)
{
    SymbolTableEntry entry = symbolTable()->inlineGet(propertyName.uid());
    if (entry.isNull())
        return false;

    // Defend against the inspector asking for a var after it has been optimized out.
    if (isTornOff() && !isValid(entry))
        return false;

    slot.setValue(this, DontEnum, registerAt(entry.getIndex()).get());
    return true;
}

inline bool JSLexicalEnvironment::symbolTableGet(PropertyName propertyName, PropertyDescriptor& descriptor)
{
    SymbolTableEntry entry = symbolTable()->inlineGet(propertyName.uid());
    if (entry.isNull())
        return false;

    // Defend against the inspector asking for a var after it has been optimized out.
    if (isTornOff() && !isValid(entry))
        return false;

    descriptor.setDescriptor(registerAt(entry.getIndex()).get(), entry.getAttributes());
    return true;
}

inline bool JSLexicalEnvironment::symbolTablePut(ExecState* exec, PropertyName propertyName, JSValue value, bool shouldThrow)
{
    VM& vm = exec->vm();
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    
    WriteBarrierBase<Unknown>* reg;
    {
        GCSafeConcurrentJITLocker locker(symbolTable()->m_lock, exec->vm().heap);
        SymbolTable::Map::iterator iter = symbolTable()->find(locker, propertyName.uid());
        if (iter == symbolTable()->end(locker))
            return false;
        ASSERT(!iter->value.isNull());
        if (iter->value.isReadOnly()) {
            if (shouldThrow)
                throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
            return true;
        }
        // Defend against the inspector asking for a var after it has been optimized out.
        if (isTornOff() && !isValid(iter->value))
            return false;
        if (VariableWatchpointSet* set = iter->value.watchpointSet())
            set->invalidate(VariableWriteFireDetail(this, propertyName)); // Don't mess around - if we had found this statically, we would have invcalidated it.
        reg = &registerAt(iter->value.getIndex());
    }
    reg->set(vm, this, value);
    return true;
}

void JSLexicalEnvironment::getOwnNonIndexPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(object);

    CallFrame* callFrame = CallFrame::create(reinterpret_cast<Register*>(thisObject->m_registers));
    if (shouldIncludeDontEnumProperties(mode) && !thisObject->isTornOff() && (callFrame->codeBlock()->usesArguments() || callFrame->codeBlock()->usesEval()))
        propertyNames.add(exec->propertyNames().arguments);

    {
        ConcurrentJITLocker locker(thisObject->symbolTable()->m_lock);
        SymbolTable::Map::iterator end = thisObject->symbolTable()->end(locker);
        for (SymbolTable::Map::iterator it = thisObject->symbolTable()->begin(locker); it != end; ++it) {
            if (it->value.getAttributes() & DontEnum && !shouldIncludeDontEnumProperties(mode))
                continue;
            if (!thisObject->isValid(it->value))
                continue;
            propertyNames.add(Identifier(exec, it->key.get()));
        }
    }
    // Skip the JSEnvironmentRecord implementation of getOwnNonIndexPropertyNames
    JSObject::getOwnNonIndexPropertyNames(thisObject, exec, propertyNames, mode);
}

inline bool JSLexicalEnvironment::symbolTablePutWithAttributes(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    
    WriteBarrierBase<Unknown>* reg;
    {
        ConcurrentJITLocker locker(symbolTable()->m_lock);
        SymbolTable::Map::iterator iter = symbolTable()->find(locker, propertyName.uid());
        if (iter == symbolTable()->end(locker))
            return false;
        SymbolTableEntry& entry = iter->value;
        ASSERT(!entry.isNull());
        if (!isValid(entry))
            return false;
        
        entry.setAttributes(attributes);
        reg = &registerAt(entry.getIndex());
    }
    reg->set(vm, this, value);
    return true;
}

bool JSLexicalEnvironment::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(object);

    if (propertyName == exec->propertyNames().arguments) {
        // Defend against the inspector asking for the arguments object after it has been optimized out.
        CallFrame* callFrame = CallFrame::create(reinterpret_cast<Register*>(thisObject->m_registers));
        if (!thisObject->isTornOff() && (callFrame->codeBlock()->usesArguments() || callFrame->codeBlock()->usesEval())) {
            slot.setCustom(thisObject, DontEnum, argumentsGetter);
            return true;
        }
    }

    if (thisObject->symbolTableGet(propertyName, slot))
        return true;

    unsigned attributes;
    if (JSValue value = thisObject->getDirect(exec->vm(), propertyName, attributes)) {
        slot.setValue(thisObject, attributes, value);
        return true;
    }

    // We don't call through to JSObject because there's no way to give a 
    // lexical environment object getter properties or a prototype.
    ASSERT(!thisObject->hasGetterSetterProperties());
    ASSERT(thisObject->prototype().isNull());
    return false;
}

void JSLexicalEnvironment::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (thisObject->symbolTablePut(exec, propertyName, value, slot.isStrictMode()))
        return;

    // We don't call through to JSObject because __proto__ and getter/setter 
    // properties are non-standard extensions that other implementations do not
    // expose in the lexicalEnvironment object.
    ASSERT(!thisObject->hasGetterSetterProperties());
    thisObject->putOwnDataProperty(exec->vm(), propertyName, value, slot);
}

bool JSLexicalEnvironment::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    if (propertyName == exec->propertyNames().arguments)
        return false;

    return Base::deleteProperty(cell, exec, propertyName);
}

JSValue JSLexicalEnvironment::toThis(JSCell*, ExecState* exec, ECMAMode ecmaMode)
{
    if (ecmaMode == StrictMode)
        return jsUndefined();
    return exec->globalThisValue();
}

EncodedJSValue JSLexicalEnvironment::argumentsGetter(ExecState*, JSObject* slotBase, EncodedJSValue, PropertyName)
{
    JSLexicalEnvironment* lexicalEnvironment = jsCast<JSLexicalEnvironment*>(slotBase);
    CallFrame* callFrame = CallFrame::create(reinterpret_cast<Register*>(lexicalEnvironment->m_registers));
    ASSERT(!lexicalEnvironment->isTornOff() && (callFrame->codeBlock()->usesArguments() || callFrame->codeBlock()->usesEval()));
    if (lexicalEnvironment->isTornOff() || !(callFrame->codeBlock()->usesArguments() || callFrame->codeBlock()->usesEval()))
        return JSValue::encode(jsUndefined());

    VirtualRegister argumentsRegister = callFrame->codeBlock()->argumentsRegister();
    if (JSValue arguments = callFrame->uncheckedR(argumentsRegister.offset()).jsValue())
        return JSValue::encode(arguments);
    int realArgumentsRegister = unmodifiedArgumentsRegister(argumentsRegister).offset();

    JSValue arguments = JSValue(Arguments::create(callFrame->vm(), callFrame));
    callFrame->uncheckedR(argumentsRegister.offset()) = arguments;
    callFrame->uncheckedR(realArgumentsRegister) = arguments;
    
    ASSERT(callFrame->uncheckedR(realArgumentsRegister).jsValue().inherits(Arguments::info()));
    return JSValue::encode(callFrame->uncheckedR(realArgumentsRegister).jsValue());
}

} // namespace JSC
