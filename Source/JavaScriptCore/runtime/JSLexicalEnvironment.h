/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "JSSymbolTableObject.h"
#include "SymbolTable.h"

namespace JSC {

class LLIntOffsetsExtractor;

class JSLexicalEnvironment : public JSSymbolTableObject {
    friend class JIT;
    friend class LLIntOffsetsExtractor;
public:
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        static_assert(!CellType::needsDestruction, "");
        return &vm.variableSizedCellSpace;
    }

    using Base = JSSymbolTableObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesAnyFormOfGetPropertyNames;

    WriteBarrierBase<Unknown>* variables()
    {
        return bitwise_cast<WriteBarrierBase<Unknown>*>(bitwise_cast<char*>(this) + offsetOfVariables());
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
        return (offsetOfVariables() + scopeOffset * sizeof(WriteBarrier<Unknown>)).unsafeGet();
    }

    static size_t allocationSizeForScopeSize(Checked<size_t> scopeSize)
    {
        return (offsetOfVariables() + scopeSize * sizeof(WriteBarrier<Unknown>)).unsafeGet();
    }

    static size_t allocationSize(SymbolTable* symbolTable)
    {
        return allocationSizeForScopeSize(symbolTable->scopeSize());
    }

    static JSLexicalEnvironment* create(
        VM& vm, Structure* structure, JSScope* currentScope, SymbolTable* symbolTable, JSValue initialValue)
    {
        JSLexicalEnvironment* result = 
            new (
                NotNull,
                allocateCell<JSLexicalEnvironment>(vm.heap, allocationSize(symbolTable)))
            JSLexicalEnvironment(vm, structure, currentScope, symbolTable);
        result->finishCreation(vm, initialValue);
        return result;
    }

    static JSLexicalEnvironment* create(VM& vm, JSGlobalObject* globalObject, JSScope* currentScope, SymbolTable* symbolTable, JSValue initialValue)
    {
        Structure* structure = globalObject->activationStructure();
        return create(vm, structure, currentScope, symbolTable, initialValue);
    }
        
    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static void getOwnNonIndexPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);

    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject) { return Structure::create(vm, globalObject, jsNull(), TypeInfo(LexicalEnvironmentType, StructureFlags), info()); }

protected:
    JSLexicalEnvironment(VM&, Structure*, JSScope*, SymbolTable*);

    void finishCreationUninitialized(VM& vm)
    {
        Base::finishCreation(vm);
    }

    void finishCreation(VM& vm, JSValue value)
    {
        finishCreationUninitialized(vm);
        ASSERT(value == jsUndefined() || value == jsTDZValue());
        for (unsigned i = symbolTable()->scopeSize(); i--;) {
            // Filling this with undefined/TDZEmptyValue is useful because that's what variables start out as.
            variableAt(ScopeOffset(i)).setStartingValue(value);
        }
    }

    static void visitChildren(JSCell*, SlotVisitor&);
    static void analyzeHeap(JSCell*, HeapAnalyzer&);
};

inline JSLexicalEnvironment::JSLexicalEnvironment(VM& vm, Structure* structure, JSScope* currentScope, SymbolTable* symbolTable)
    : Base(vm, structure, currentScope, symbolTable)
{
}

} // namespace JSC
