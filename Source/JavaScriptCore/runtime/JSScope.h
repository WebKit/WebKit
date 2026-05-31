/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/GetPutInfo.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSType.h>
#include <JavaScriptCore/PropertyDescriptor.h>
#include <JavaScriptCore/SymbolTable.h>
#include <JavaScriptCore/ThrowScope.h>
#include <JavaScriptCore/VariableEnvironment.h>
#include <JavaScriptCore/VariableWriteFireDetail.h>

namespace JSC {

class ScopeChainIterator;
class WatchpointSet;

using TDZEnvironment = UncheckedKeyHashSet<RefPtr<UniquedStringImpl>, IdentifierRepHash>;

// Uniform offset of m_next across every scope-chain participant.
static constexpr ptrdiff_t scopeChainNextOffset = 16;

ALWAYS_INLINE bool isScopeChainCell(const JSObject* object)
{
    JSType t = object->type();
    if (t == GlobalObjectType)
        return true;
    return static_cast<uint32_t>(t) - FirstScopeType <= LastScopeType - FirstScopeType;
}

ALWAYS_INLINE bool isNonGlobalScopeChainCell(const JSObject* object)
{
    JSType t = object->type();
    return static_cast<uint32_t>(t) - FirstScopeType <= LastScopeType - FirstScopeType;
}

JS_EXPORT_PRIVATE JSObject* objectAtScope(JSObject*);
JSObject* nextScope(JSObject*);
SymbolTable* scopeSymbolTable(JSObject*);
bool isVarScope(JSObject*);
bool isLexicalScope(JSObject*);
bool isModuleScope(JSObject*);
bool isCatchScope(JSObject*);
bool isCatchScopeWithSimpleParameter(JSObject*);
bool isFunctionNameScopeObject(JSObject*);
bool isNestedLexicalScope(JSObject*);

JSObject* resolveScope(JSGlobalObject*, JSObject*, const Identifier&);
JSValue resolveScopeForHoistingFuncDeclInEval(JSGlobalObject*, JSObject*, const Identifier&);
ResolveOp abstractResolveScope(JSGlobalObject*, size_t depthOffset, JSObject*, const Identifier&, GetOrPut, ResolveType, InitializationMode);

bool hasConstantScope(ResolveType);
JSObject* constantScopeForCodeBlock(ResolveType, CodeBlock*);

void collectClosureVariablesUnderTDZ(JSObject*, TDZEnvironment& result, PrivateNameEnvironment&);

class ScopeChainIterator {
public:
    ScopeChainIterator(JSObject* node)
        : m_node(node)
    {
    }

    JSObject* get() const { return objectAtScope(m_node); }
    JSObject* operator->() const { return objectAtScope(m_node); }
    JSObject* scope() const { return m_node; }

    ScopeChainIterator& operator++()
    {
        if (m_node && isNonGlobalScopeChainCell(m_node))
            m_node = nextScope(m_node);
        else
            m_node = nullptr;
        return *this;
    }

    // postfix ++ intentionally omitted

    friend bool operator==(const ScopeChainIterator&, const ScopeChainIterator&) = default;

private:
    JSObject* m_node;
};

inline ScopeChainIterator scopeBegin(JSObject* scope) { return ScopeChainIterator(scope); }
inline ScopeChainIterator scopeEnd() { return ScopeChainIterator(nullptr); }

// Duck-typed: any class exposing symbolTable()/variableAt()/isValidScopeOffset() works.

template<typename SymbolTableObjectType>
inline bool symbolTableGet(
    SymbolTableObjectType* object, PropertyName propertyName, PropertySlot& slot)
{
    SymbolTable& symbolTable = *object->symbolTable();
    ConcurrentJSLocker locker(symbolTable.m_lock);
    SymbolTable::Map::iterator iter = symbolTable.find(locker, propertyName.uid());
    if (iter == symbolTable.end(locker))
        return false;
    SymbolTableEntry::Fast entry = iter->value;
    ASSERT(!entry.isNull());

    ScopeOffset offset = entry.scopeOffset();
    // Defend against the inspector asking for a var after it has been optimized out.
    if (!object->isValidScopeOffset(offset))
        return false;

    slot.setValue(object, entry.getAttributes() | PropertyAttribute::DontDelete, object->variableAt(offset).get());
    return true;
}

template<typename SymbolTableObjectType>
inline bool symbolTableGet(
    SymbolTableObjectType* object, PropertyName propertyName, SymbolTableEntry& entry, PropertyDescriptor& descriptor)
{
    SymbolTable& symbolTable = *object->symbolTable();
    ConcurrentJSLocker locker(symbolTable.m_lock);
    SymbolTable::Map::iterator iter = symbolTable.find(locker, propertyName.uid());
    if (iter == symbolTable.end(locker))
        return false;
    entry = iter->value;
    ASSERT(!entry.isNull());

    ScopeOffset offset = entry.scopeOffset();
    // Defend against the inspector asking for a var after it has been optimized out.
    if (!object->isValidScopeOffset(offset))
        return false;

    descriptor.setDescriptor(object->variableAt(offset).get(), entry.getAttributes() | PropertyAttribute::DontDelete);
    return true;
}

template<typename SymbolTableObjectType>
ALWAYS_INLINE void symbolTablePutTouchWatchpointSet(VM& vm, SymbolTableObjectType* object, PropertyName propertyName, JSValue value, WriteBarrierBase<Unknown>* reg, WatchpointSet* set)
{
    reg->set(vm, object, value);
    if (set)
        VariableWriteFireDetail::touch(vm, set, object, propertyName);
}

template<typename SymbolTableObjectType>
ALWAYS_INLINE void symbolTablePutInvalidateWatchpointSet(VM& vm, SymbolTableObjectType* object, PropertyName propertyName, JSValue value, WriteBarrierBase<Unknown>* reg, WatchpointSet* set)
{
    reg->set(vm, object, value);
    if (set)
        set->invalidate(vm, VariableWriteFireDetail(object, propertyName)); // Don't mess around - if we had found this statically, we would have invalidated it.
}

enum class SymbolTablePutMode {
    Touch,
    Invalidate
};

template<SymbolTablePutMode symbolTablePutMode, typename SymbolTableObjectType>
inline bool symbolTablePut(SymbolTableObjectType* object, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, bool shouldThrowReadOnlyError, bool ignoreReadOnlyErrors, bool& putResult)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    WatchpointSet* set = nullptr;
    WriteBarrierBase<Unknown>* reg;
    {
        SymbolTable& symbolTable = *object->symbolTable();
        // FIXME: This is very suspicious. We shouldn't need a GC-safe lock here.
        // https://bugs.webkit.org/show_bug.cgi?id=134601
        GCSafeConcurrentJSLocker locker(symbolTable.m_lock, vm);
        SymbolTable::Map::iterator iter = symbolTable.find(locker, propertyName.uid());
        if (iter == symbolTable.end(locker))
            return false;
        bool wasFat;
        SymbolTableEntry::Fast fastEntry = iter->value.getFast(wasFat);
        ASSERT(!fastEntry.isNull());
        if (fastEntry.isReadOnly() && !ignoreReadOnlyErrors) {
            if (shouldThrowReadOnlyError)
                throwTypeError(globalObject, scope, ReadonlyPropertyWriteError);
            putResult = false;
            return true;
        }

        ScopeOffset offset = fastEntry.scopeOffset();

        // Defend against the inspector asking for a var after it has been optimized out.
        if (!object->isValidScopeOffset(offset))
            return false;

        set = iter->value.watchpointSet();
        reg = &object->variableAt(offset);
    }
    // I'd prefer we not hold lock while executing barriers, since I prefer to reserve
    // the right for barriers to be able to trigger GC. And I don't want to hold VM
    // locks while GC'ing.
    if (symbolTablePutMode == SymbolTablePutMode::Invalidate)
        symbolTablePutInvalidateWatchpointSet(vm, object, propertyName, value, reg, set);
    else
        symbolTablePutTouchWatchpointSet(vm, object, propertyName, value, reg, set);
    putResult = true;
    return true;
}

template<typename SymbolTableObjectType>
inline bool symbolTablePutTouchWatchpointSet(
    SymbolTableObjectType* object, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value,
    bool shouldThrowReadOnlyError, bool ignoreReadOnlyErrors, bool& putResult)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(object));
    return symbolTablePut<SymbolTablePutMode::Touch>(object, globalObject, propertyName, value, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
}

template<typename SymbolTableObjectType>
inline bool symbolTablePutInvalidateWatchpointSet(
    SymbolTableObjectType* object, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value,
    bool shouldThrowReadOnlyError, bool ignoreReadOnlyErrors, bool& putResult)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(object));
    return symbolTablePut<SymbolTablePutMode::Invalidate>(object, globalObject, propertyName, value, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
}

} // namespace JSC
