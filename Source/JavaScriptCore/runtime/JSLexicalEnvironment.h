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
 
#pragma once

#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSScope.h>
#include <JavaScriptCore/SymbolTable.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class LLIntOffsetsExtractor;

class JSLexicalEnvironment : public JSObject {
    friend class JIT;
    friend class LLIntOffsetsExtractor;
public:
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        static_assert(CellType::needsDestruction == DoesNotNeedDestruction);
        return &vm.heap.cellSpace;
    }

    using Base = JSObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetOwnSpecialPropertyNames | OverridesPut;

    SymbolTable* symbolTable() const LIFETIME_BOUND { return m_symbolTable.get(); }
    static constexpr ptrdiff_t offsetOfSymbolTable() { return OBJECT_OFFSETOF(JSLexicalEnvironment, m_symbolTable); }

    JSObject* next() const { return m_next.get(); }

    WriteBarrierBase<Unknown>* variables()
    {
        return std::bit_cast<WriteBarrierBase<Unknown>*>(std::bit_cast<char*>(this) + offsetOfVariables());
    }

    bool isValidScopeOffset(ScopeOffset offset)
    {
        return !!offset && offset.offset() < symbolTable()->scopeSize();
    }

    WriteBarrierBase<Unknown>& variableAt(ScopeOffset offset)
    {
        ASSERT(isValidScopeOffset(offset));
        return variables()[offset.offset()];
    }

    static size_t offsetOfVariables()
    {
        return WTF::roundUpToMultipleOf<sizeof(WriteBarrier<Unknown>)>(sizeof(JSLexicalEnvironment));
    }

    static size_t offsetOfVariable(ScopeOffset offset)
    {
        Checked<size_t> scopeOffset = offset.offset();
        return offsetOfVariables() + scopeOffset * sizeof(WriteBarrier<Unknown>);
    }

    static size_t allocationSizeForScopeSize(Checked<size_t> scopeSize)
    {
        return offsetOfVariables() + scopeSize * sizeof(WriteBarrier<Unknown>);
    }

    static size_t allocationSize(SymbolTable* symbolTable)
    {
        size_t base = allocationSizeForScopeSize(symbolTable->scopeSize());
        // Tail slot for eval-injection storage; only allocated for sloppy eval.
        if (symbolTable->usesSloppyEval())
            base += sizeof(WriteBarrier<JSObject>);
        return base;
    }

    static size_t evalInjectionStorageOffset(SymbolTable* symbolTable)
    {
        ASSERT(symbolTable->usesSloppyEval());
        return offsetOfVariables() + Checked<size_t>(symbolTable->scopeSize()) * sizeof(WriteBarrier<Unknown>);
    }

    WriteBarrier<JSObject>* evalInjectionStorageSlot()
    {
        if (!symbolTable()->usesSloppyEval())
            return nullptr;
        return std::bit_cast<WriteBarrier<JSObject>*>(std::bit_cast<char*>(this) + evalInjectionStorageOffset(symbolTable()));
    }

    JSObject* evalInjectionStorage()
    {
        auto* slot = evalInjectionStorageSlot();
        return slot ? slot->get() : nullptr;
    }

    JS_EXPORT_PRIVATE JSObject* ensureEvalInjectionStorage(JSGlobalObject*);

    static JSLexicalEnvironment* create(
        VM& vm, Structure* structure, JSObject* currentScope, SymbolTable* symbolTable, JSValue initialValue)
    {
        JSLexicalEnvironment* result =
            new (
                NotNull,
                allocateCell<JSLexicalEnvironment>(vm, allocationSize(symbolTable)))
            JSLexicalEnvironment(vm, structure, currentScope, symbolTable, initialValue);
        result->finishCreation(vm);
        return result;
    }

    static JSLexicalEnvironment* create(VM& vm, JSGlobalObject* globalObject, JSObject* currentScope, SymbolTable* symbolTable, JSValue initialValue)
    {
        Structure* structure = globalObject->activationStructure();
        return create(vm, structure, currentScope, symbolTable, initialValue);
    }

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static void getOwnSpecialPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArrayBuilder&, DontEnumPropertiesMode);

    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);

    DECLARE_INFO;

    DECLARE_VISIT_CHILDREN;

    inline static Structure* createStructure(VM&, JSGlobalObject*);

protected:
    JSLexicalEnvironment(VM&, Structure*, JSObject*, SymbolTable*, JSValue initialValue);

    DECLARE_DEFAULT_FINISH_CREATION;

    static void analyzeHeap(JSCell*, HeapAnalyzer&);

    void setSymbolTable(VM& vm, SymbolTable* symbolTable)
    {
        ASSERT(!m_symbolTable);
        symbolTable->notifyCreation(vm, this, "Allocated a scope");
        m_symbolTable.set(vm, this, symbolTable);
    }

private:
    WriteBarrier<SymbolTable> m_symbolTable;
    WriteBarrier<JSObject> m_next;

public:
    static constexpr ptrdiff_t offsetOfNext() { return OBJECT_OFFSETOF(JSLexicalEnvironment, m_next); }
};

inline JSLexicalEnvironment::JSLexicalEnvironment(VM& vm, Structure* structure, JSObject* currentScope, SymbolTable* symbolTable, JSValue initialValue)
    : Base(vm, structure)
    , m_symbolTable(symbolTable, WriteBarrierEarlyInit)
    , m_next(currentScope, WriteBarrierEarlyInit)
{
    ASSERT(symbolTable);
    symbolTable->notifyCreation(vm, this, "Allocated a scope");
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    for (unsigned i = this->symbolTable()->scopeSize(); i--;) {
        // Filling this with undefined/TDZEmptyValue is useful because that's what variables start out as.
        variableAt(ScopeOffset(i)).setStartingValue(initialValue);
    }
    if (auto* slot = evalInjectionStorageSlot())
        slot->clear();
}

static_assert(JSLexicalEnvironment::offsetOfNext() == scopeChainNextOffset, "JSLexicalEnvironment::m_next must live at scopeChainNextOffset");

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
