/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSScope.h>
#include <JavaScriptCore/SymbolTable.h>
#include <wtf/SegmentedVector.h>

namespace JSC {

class JSGlobalLexicalEnvironment final : public JSObject {
public:
    using Base = JSObject;

    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetOwnSpecialPropertyNames | OverridesPut;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.globalLexicalEnvironmentSpace();
    }

    static constexpr DestructionMode needsDestruction = NeedsDestruction;

    SymbolTable* symbolTable() const LIFETIME_BOUND { return m_symbolTable.get(); }
    static constexpr ptrdiff_t offsetOfSymbolTable() { return OBJECT_OFFSETOF(JSGlobalLexicalEnvironment, m_symbolTable); }

    bool isValidScopeOffset(ScopeOffset offset)
    {
        return !!offset && offset.offset() < m_variables.size();
    }

    WriteBarrier<Unknown>& variableAt(ScopeOffset offset) { return m_variables[offset.offset()]; }

    JS_EXPORT_PRIVATE ScopeOffset findVariableIndex(void*);

    WriteBarrier<Unknown>* assertVariableIsInThisObject(WriteBarrier<Unknown>* variablePointer)
    {
        if (ASSERT_ENABLED)
            findVariableIndex(variablePointer);
        return variablePointer;
    }

    JS_EXPORT_PRIVATE ScopeOffset addVariables(unsigned numberOfVariablesToAdd, JSValue);

    static JSGlobalLexicalEnvironment* create(VM& vm, Structure* structure, JSObject* parentScope)
    {
        JSGlobalLexicalEnvironment* result =
            new (NotNull, allocateCell<JSGlobalLexicalEnvironment>(vm)) JSGlobalLexicalEnvironment(vm, structure, parentScope);
        result->finishCreation(vm);
        result->symbolTable()->setScopeType(SymbolTable::ScopeType::GlobalLexicalScope);
        return result;
    }

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    static void getOwnSpecialPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArrayBuilder&, DontEnumPropertiesMode);

    static void destroy(JSCell*);

    bool isEmpty() const { return !symbolTable()->size(); }
    bool isConstVariable(UniquedStringImpl*);

    DECLARE_INFO;
    DECLARE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE);
    JS_EXPORT_PRIVATE static void analyzeHeap(JSCell*, HeapAnalyzer&);

    inline static Structure* createStructure(VM&, JSGlobalObject*);

    JSObject* next() const { return m_next.get(); }

private:
    JSGlobalLexicalEnvironment(VM&, Structure*, JSObject*);
    ~JSGlobalLexicalEnvironment();

    void finishCreation(VM&);

    void setSymbolTable(VM& vm, SymbolTable* symbolTable)
    {
        ASSERT(!m_symbolTable);
        symbolTable->notifyCreation(vm, this, "Allocated a scope");
        m_symbolTable.set(vm, this, symbolTable);
    }

    WriteBarrier<SymbolTable> m_symbolTable;
    WriteBarrier<JSObject> m_next;
    SegmentedVector<WriteBarrier<Unknown>, 16> m_variables;
#ifndef NDEBUG
    bool m_alreadyDestroyed { false };
#endif

public:
    static constexpr ptrdiff_t offsetOfNext() { return OBJECT_OFFSETOF(JSGlobalLexicalEnvironment, m_next); }
};

static_assert(JSGlobalLexicalEnvironment::offsetOfNext() == scopeChainNextOffset, "JSGlobalLexicalEnvironment::m_next must live at scopeChainNextOffset");

} // namespace JSC
