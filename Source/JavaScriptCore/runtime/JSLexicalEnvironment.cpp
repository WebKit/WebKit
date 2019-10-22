/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "HeapAnalyzer.h"
#include "Interpreter.h"
#include "JSFunction.h"
#include "JSCInlines.h"

namespace JSC {

const ClassInfo JSLexicalEnvironment::s_info = { "JSLexicalEnvironment", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSLexicalEnvironment) };

void JSLexicalEnvironment::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSLexicalEnvironment*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.appendValuesHidden(thisObject->variables(), thisObject->symbolTable()->scopeSize());
}

void JSLexicalEnvironment::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = jsCast<JSLexicalEnvironment*>(cell);
    Base::analyzeHeap(cell, analyzer);

    ConcurrentJSLocker locker(thisObject->symbolTable()->m_lock);
    SymbolTable::Map::iterator end = thisObject->symbolTable()->end(locker);
    for (SymbolTable::Map::iterator it = thisObject->symbolTable()->begin(locker); it != end; ++it) {
        SymbolTableEntry::Fast entry = it->value;
        ASSERT(!entry.isNull());
        ScopeOffset offset = entry.scopeOffset();
        if (!thisObject->isValidScopeOffset(offset))
            continue;

        JSValue toValue = thisObject->variableAt(offset).get();
        if (toValue && toValue.isCell())
            analyzer.analyzeVariableNameEdge(thisObject, toValue.asCell(), it->key.get());
    }
}

void JSLexicalEnvironment::getOwnNonIndexPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(object);

    {
        ConcurrentJSLocker locker(thisObject->symbolTable()->m_lock);
        SymbolTable::Map::iterator end = thisObject->symbolTable()->end(locker);
        VM& vm = globalObject->vm();
        for (SymbolTable::Map::iterator it = thisObject->symbolTable()->begin(locker); it != end; ++it) {
            if (it->value.getAttributes() & PropertyAttribute::DontEnum && !mode.includeDontEnumProperties())
                continue;
            if (!thisObject->isValidScopeOffset(it->value.scopeOffset()))
                continue;
            if (it->key->isSymbol() && !propertyNames.includeSymbolProperties())
                continue;
            propertyNames.add(Identifier::fromUid(vm, it->key.get()));
        }
    }
    // Skip the JSSymbolTableObject's implementation of getOwnNonIndexPropertyNames
    JSObject::getOwnNonIndexPropertyNames(thisObject, globalObject, propertyNames, mode);
}

bool JSLexicalEnvironment::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(object);

    if (symbolTableGet(thisObject, propertyName, slot))
        return true;

    VM& vm = globalObject->vm();
    unsigned attributes;
    if (JSValue value = thisObject->getDirect(vm, propertyName, attributes)) {
        slot.setValue(thisObject, attributes, value);
        return true;
    }

    // We don't call through to JSObject because there's no way to give a 
    // lexical environment object getter properties or a prototype.
    ASSERT(!thisObject->hasGetterSetterProperties(vm));
    ASSERT(thisObject->getPrototypeDirect(vm).isNull());
    return false;
}

bool JSLexicalEnvironment::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSLexicalEnvironment* thisObject = jsCast<JSLexicalEnvironment*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    bool shouldThrowReadOnlyError = slot.isStrictMode() || thisObject->isLexicalScope();
    bool ignoreReadOnlyErrors = false;
    bool putResult = false;
    if (symbolTablePutInvalidateWatchpointSet(thisObject, globalObject, propertyName, value, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult))
        return putResult;

    // We don't call through to JSObject because __proto__ and getter/setter 
    // properties are non-standard extensions that other implementations do not
    // expose in the lexicalEnvironment object.
    ASSERT(!thisObject->hasGetterSetterProperties(globalObject->vm()));
    return thisObject->putOwnDataProperty(globalObject->vm(), propertyName, value, slot);
}

bool JSLexicalEnvironment::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName)
{
    VM& vm = globalObject->vm();
    if (propertyName == vm.propertyNames->arguments)
        return false;

    return Base::deleteProperty(cell, globalObject, propertyName);
}

} // namespace JSC
