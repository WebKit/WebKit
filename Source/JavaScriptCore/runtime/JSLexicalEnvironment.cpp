/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "JSCInlines.h"

namespace JSC {

const ClassInfo JSLexicalEnvironment::s_info = { "JSLexicalEnvironment"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSLexicalEnvironment) };

template<typename Visitor>
void JSLexicalEnvironment::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<JSLexicalEnvironment>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_symbolTable);
    visitor.append(thisObject->m_next);
    visitor.appendValuesHidden(thisObject->variables(), thisObject->symbolTable()->scopeSize());
    if (auto* slot = thisObject->evalInjectionStorageSlot())
        visitor.append(*slot);
}

DEFINE_VISIT_CHILDREN(JSLexicalEnvironment);

JSObject* JSLexicalEnvironment::ensureEvalInjectionStorage(JSGlobalObject* globalObject)
{
    ASSERT(symbolTable()->usesSloppyEval());
    auto* slot = evalInjectionStorageSlot();
    ASSERT(slot);
    if (auto* existing = slot->get())
        return existing;
    VM& vm = globalObject->vm();
    JSObject* storage = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    slot->set(vm, this, storage);
    return storage;
}

void JSLexicalEnvironment::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = uncheckedDowncast<JSLexicalEnvironment>(cell);
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

void JSLexicalEnvironment::getOwnSpecialPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArrayBuilder& propertyNames, DontEnumPropertiesMode mode)
{
    JSLexicalEnvironment* thisObject = uncheckedDowncast<JSLexicalEnvironment>(object);
    SymbolTable* symbolTable = thisObject->symbolTable();

    {
        ConcurrentJSLocker locker(symbolTable->m_lock);
        SymbolTable::Map::iterator end = symbolTable->end(locker);
        VM& vm = globalObject->vm();
        for (SymbolTable::Map::iterator it = symbolTable->begin(locker); it != end; ++it) {
            if (mode == DontEnumPropertiesMode::Exclude && it->value.isDontEnum())
                continue;
            if (!thisObject->isValidScopeOffset(it->value.scopeOffset()))
                continue;
            if (!propertyNames.includeSymbolProperties() && it->key->isSymbol())
                continue;
            if (propertyNames.privateSymbolMode() == PrivateSymbolMode::Exclude && symbolTable->hasPrivateName(it->key))
                continue;
            propertyNames.add(Identifier::fromUid(vm, it->key.get()));
        }
    }

    if (JSObject* injectionStorage = thisObject->evalInjectionStorage())
        injectionStorage->methodTable()->getOwnSpecialPropertyNames(injectionStorage, globalObject, propertyNames, mode);
}

bool JSLexicalEnvironment::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    JSLexicalEnvironment* thisObject = uncheckedDowncast<JSLexicalEnvironment>(object);

    if (symbolTableGet(thisObject, propertyName, slot))
        return true;

    if (JSObject* injectionStorage = thisObject->evalInjectionStorage()) {
        if (injectionStorage->getOwnPropertySlot(injectionStorage, globalObject, propertyName, slot)) {
            slot.setThisValue(JSValue(thisObject));
            return true;
        }
    }

    // We don't call through to JSObject because there's no way to give a
    // lexical environment object getter properties or a prototype.
    ASSERT(!thisObject->structure()->hasAnyKindOfGetterSetterProperties());
    ASSERT(thisObject->getPrototypeDirect().isNull());
    return false;
}

bool JSLexicalEnvironment::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSLexicalEnvironment* thisObject = uncheckedDowncast<JSLexicalEnvironment>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    bool shouldThrowReadOnlyError = slot.isStrictMode() || isLexicalScope(thisObject);
    bool ignoreReadOnlyErrors = false;
    bool putResult = false;
    if (symbolTablePutInvalidateWatchpointSet(thisObject, globalObject, propertyName, value, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult))
        return putResult;

    // putDirect (not dispatched put) — otherwise PutPropertySlot routes the write
    // back to the activation as receiver instead of the side storage.
    if (thisObject->symbolTable()->usesSloppyEval()) {
        JSObject* injectionStorage = thisObject->ensureEvalInjectionStorage(globalObject);
        VM& vm = globalObject->vm();
        injectionStorage->putDirect(vm, propertyName, value, slot);
        return true;
    }

    // We don't call through to JSObject because __proto__ and getter/setter
    // properties are non-standard extensions that other implementations do not
    // expose in the lexicalEnvironment object.
    ASSERT(!thisObject->structure()->hasAnyKindOfGetterSetterProperties());
    return false;
}

bool JSLexicalEnvironment::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    if (propertyName == vm.propertyNames->arguments)
        return false;

    auto* thisObject = uncheckedDowncast<JSLexicalEnvironment>(cell);
    if (JSObject* injectionStorage = thisObject->evalInjectionStorage()) {
        if (injectionStorage->methodTable()->deleteProperty(injectionStorage, globalObject, propertyName, slot))
            return true;
    }

    return Base::deleteProperty(cell, globalObject, propertyName, slot);
}

} // namespace JSC
