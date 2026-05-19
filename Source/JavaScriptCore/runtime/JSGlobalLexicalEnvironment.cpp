/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSGlobalLexicalEnvironment.h"

#include "HeapAnalyzer.h"
#include "JSCInlines.h"
#include "PropertyNameArray.h"

namespace JSC {

const ClassInfo JSGlobalLexicalEnvironment::s_info = { "JSGlobalLexicalEnvironment"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSGlobalLexicalEnvironment) };

JSGlobalLexicalEnvironment::JSGlobalLexicalEnvironment(VM& vm, Structure* structure, JSObject* scope)
    : Base(vm, structure)
    , m_next(scope, WriteBarrierEarlyInit)
{
}

JSGlobalLexicalEnvironment::~JSGlobalLexicalEnvironment()
{
#ifndef NDEBUG
    ASSERT(!m_alreadyDestroyed);
    m_alreadyDestroyed = true;
#endif
}

void JSGlobalLexicalEnvironment::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    setSymbolTable(vm, SymbolTable::create(vm));
}

void JSGlobalLexicalEnvironment::destroy(JSCell* cell)
{
    static_cast<JSGlobalLexicalEnvironment*>(cell)->JSGlobalLexicalEnvironment::~JSGlobalLexicalEnvironment();
}

ScopeOffset JSGlobalLexicalEnvironment::findVariableIndex(void* variableAddress)
{
    Locker locker { cellLock() };

    for (unsigned i = m_variables.size(); i--;) {
        if (&m_variables[i] != variableAddress)
            continue;
        return ScopeOffset(i);
    }
    CRASH();
    return ScopeOffset();
}

ScopeOffset JSGlobalLexicalEnvironment::addVariables(unsigned numberOfVariablesToAdd, JSValue initialValue)
{
    Locker locker { cellLock() };

    size_t oldSize = m_variables.size();
    m_variables.grow(oldSize + numberOfVariablesToAdd);

    for (size_t i = numberOfVariablesToAdd; i--;)
        m_variables[oldSize + i].setWithoutWriteBarrier(initialValue);

    return ScopeOffset(oldSize);
}

template<typename Visitor>
void JSGlobalLexicalEnvironment::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSGlobalLexicalEnvironment* thisObject = uncheckedDowncast<JSGlobalLexicalEnvironment>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_symbolTable);
    visitor.append(thisObject->m_next);

    // FIXME: We could avoid locking here if SegmentedVector was lock-free.
    Locker locker { thisObject->cellLock() };
    for (unsigned i = thisObject->m_variables.size(); i--;)
        visitor.appendHidden(thisObject->m_variables[i]);
}

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE, JSGlobalLexicalEnvironment);

void JSGlobalLexicalEnvironment::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    JSGlobalLexicalEnvironment* thisObject = uncheckedDowncast<JSGlobalLexicalEnvironment>(cell);
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

bool JSGlobalLexicalEnvironment::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    JSGlobalLexicalEnvironment* thisObject = uncheckedDowncast<JSGlobalLexicalEnvironment>(cell);
    if (thisObject->symbolTable()->contains(propertyName.uid()))
        return false;
    return Base::deleteProperty(thisObject, globalObject, propertyName, slot);
}

void JSGlobalLexicalEnvironment::getOwnSpecialPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArrayBuilder& propertyNames, DontEnumPropertiesMode mode)
{
    VM& vm = globalObject->vm();
    JSGlobalLexicalEnvironment* thisObject = uncheckedDowncast<JSGlobalLexicalEnvironment>(object);
    SymbolTable* symbolTable = thisObject->symbolTable();
    {
        ConcurrentJSLocker locker(symbolTable->m_lock);
        SymbolTable::Map::iterator end = symbolTable->end(locker);
        for (SymbolTable::Map::iterator it = symbolTable->begin(locker); it != end; ++it) {
            if (mode == DontEnumPropertiesMode::Include || !it->value.isDontEnum()) {
                if (!propertyNames.includeSymbolProperties() && it->key->isSymbol())
                    continue;
                if (propertyNames.privateSymbolMode() == PrivateSymbolMode::Exclude && symbolTable->hasPrivateName(it->key))
                    continue;
                propertyNames.add(Identifier::fromUid(vm, it->key.get()));
            }
        }
    }
}

bool JSGlobalLexicalEnvironment::getOwnPropertySlot(JSObject* object, JSGlobalObject*, PropertyName propertyName, PropertySlot& slot)
{
    JSGlobalLexicalEnvironment* thisObject = uncheckedDowncast<JSGlobalLexicalEnvironment>(object);
    return symbolTableGet(thisObject, propertyName, slot);
}

bool JSGlobalLexicalEnvironment::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSGlobalLexicalEnvironment* thisObject = uncheckedDowncast<JSGlobalLexicalEnvironment>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));
    bool alwaysThrowWhenAssigningToConstProperty = true;
    bool ignoreConstAssignmentError = slot.isInitialization();
    bool putResult = false;
    symbolTablePutTouchWatchpointSet(thisObject, globalObject, propertyName, value, alwaysThrowWhenAssigningToConstProperty, ignoreConstAssignmentError, putResult);
    return putResult;
}

bool JSGlobalLexicalEnvironment::isConstVariable(UniquedStringImpl* impl)
{
    SymbolTableEntry entry = symbolTable()->get(impl);
    ASSERT(!entry.isNull());
    return entry.isReadOnly();
}

} // namespace JSC
