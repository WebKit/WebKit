/*
 * Copyright (C) 2008, 2009, 2014, 2015 Apple Inc. All rights reserved.
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

#include "Interpreter.h"
#include "JSFunction.h"
#include "JSCInlines.h"

using namespace std;

namespace JSC {

const ClassInfo JSLexicalEnvironment::s_info = { "JSLexicalEnvironment", &Base::s_info, 0, CREATE_METHOD_TABLE(JSLexicalEnvironment) };

inline bool JSLexicalEnvironment::symbolTableGet(PropertyName propertyName, PropertySlot& slot)
{
    SymbolTableEntry entry = symbolTable()->inlineGet(propertyName.uid());
    if (entry.isNull())
        return false;

    ScopeOffset offset = entry.scopeOffset();

    // Defend against the inspector asking for a var after it has been optimized out.
    if (!isValid(offset))
        return false;

    slot.setValue(this, DontEnum, variableAt(offset).get());
    return true;
}

inline bool JSLexicalEnvironment::symbolTableGet(PropertyName propertyName, PropertyDescriptor& descriptor)
{
    SymbolTableEntry entry = symbolTable()->inlineGet(propertyName.uid());
    if (entry.isNull())
        return false;

    ScopeOffset offset = entry.scopeOffset();

    // Defend against the inspector asking for a var after it has been optimized out.
    if (!isValid(offset))
        return false;

    descriptor.setDescriptor(variableAt(offset).get(), entry.getAttributes());
    return true;
}

inline bool JSLexicalEnvironment::symbolTablePut(ExecState* exec, PropertyName propertyName, JSValue value, bool shouldThrow)
{
    VM& vm = exec->vm();
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    
    WriteBarrierBase<Unknown>* reg;
    WatchpointSet* set;
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
        ScopeOffset offset = iter->value.scopeOffset();
        // Defend against the inspector asking for a var after it has been optimized out.
        if (!isValid(offset))
            return false;
        set = iter->value.watchpointSet();
        reg = &variableAt(offset);
    }
    reg->set(vm, this, value);
    if (set)
        set->invalidate(VariableWriteFireDetail(this, propertyName)); // Don't mess around - if we had found this statically, we would have invalidated it.
    return true;
}

void JSLexicalEnvironment::getOwnNonIndexPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(object);

    {
        ConcurrentJITLocker locker(thisObject->symbolTable()->m_lock);
        SymbolTable::Map::iterator end = thisObject->symbolTable()->end(locker);
        for (SymbolTable::Map::iterator it = thisObject->symbolTable()->begin(locker); it != end; ++it) {
            if (it->value.getAttributes() & DontEnum && !mode.includeDontEnumProperties())
                continue;
            if (!thisObject->isValid(it->value.scopeOffset()))
                continue;
            if (it->key->isSymbol() && !mode.includeSymbolProperties())
                continue;
            propertyNames.add(Identifier::fromUid(exec, it->key.get()));
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
        
        ScopeOffset offset = entry.scopeOffset();
        if (!isValid(offset))
            return false;
        
        entry.setAttributes(attributes);
        reg = &variableAt(offset);
    }
    reg->set(vm, this, value);
    return true;
}

bool JSLexicalEnvironment::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(object);

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

} // namespace JSC
