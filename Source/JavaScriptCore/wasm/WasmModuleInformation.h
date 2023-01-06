/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmBranchHints.h"
#include "WasmFormat.h"

#include <wtf/BitVector.h>
#include <wtf/HashMap.h>

namespace JSC { namespace Wasm {

struct ModuleInformation : public ThreadSafeRefCounted<ModuleInformation> {

    using BranchHints = HashMap<uint32_t, BranchHintMap, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

    ModuleInformation();
    ModuleInformation(const ModuleInformation&) = delete;
    ModuleInformation(ModuleInformation&&) = delete;

    static Ref<ModuleInformation> create()
    {
        return adoptRef(*new ModuleInformation);
    }

    JS_EXPORT_PRIVATE ~ModuleInformation();
    
    size_t functionIndexSpaceSize() const { return importFunctionTypeIndices.size() + internalFunctionTypeIndices.size(); }
    bool isImportedFunctionFromFunctionIndexSpace(size_t functionIndex) const
    {
        ASSERT(functionIndex < functionIndexSpaceSize());
        return functionIndex < importFunctionTypeIndices.size();
    }
    TypeIndex typeIndexFromFunctionIndexSpace(size_t functionIndex) const
    {
        return isImportedFunctionFromFunctionIndexSpace(functionIndex)
            ? importFunctionTypeIndices[functionIndex]
            : internalFunctionTypeIndices[functionIndex - importFunctionTypeIndices.size()];
    }

    size_t exceptionIndexSpaceSize() const { return importExceptionTypeIndices.size() + internalExceptionTypeIndices.size(); }
    bool isImportedExceptionFromExceptionIndexSpace(size_t exceptionIndex) const
    {
        ASSERT(exceptionIndex < exceptionIndexSpaceSize());
        return exceptionIndex < importExceptionTypeIndices.size();
    }
    TypeIndex typeIndexFromExceptionIndexSpace(size_t exceptionIndex) const
    {
        return isImportedExceptionFromExceptionIndexSpace(exceptionIndex)
            ? importExceptionTypeIndices[exceptionIndex]
            : internalExceptionTypeIndices[exceptionIndex - importExceptionTypeIndices.size()];
    }

    uint32_t importFunctionCount() const { return importFunctionTypeIndices.size(); }
    uint32_t internalFunctionCount() const { return internalFunctionTypeIndices.size(); }
    uint32_t importExceptionCount() const { return importExceptionTypeIndices.size(); }
    uint32_t internalExceptionCount() const { return internalExceptionTypeIndices.size(); }

    // Currently, our wasm implementation allows only one memory and table.
    // If we need to remove this limitation, we would have MemoryInformation and TableInformation in the Vectors.
    uint32_t memoryCount() const { return memory ? 1 : 0; }
    uint32_t tableCount() const { return tables.size(); }
    uint32_t elementCount() const { return elements.size(); }
    uint32_t globalCount() const { return globals.size(); }
    uint32_t dataSegmentsCount() const { return numberOfDataSegments.value_or(0); }

    const TableInformation& table(unsigned index) const { return tables[index]; }

    const BitVector& referencedFunctions() const { return m_referencedFunctions; }
    void addReferencedFunction(unsigned index) const { m_referencedFunctions.set(index); }

    bool isDeclaredFunction(uint32_t index) const { return m_declaredFunctions.contains(index); }
    void addDeclaredFunction(uint32_t index) { m_declaredFunctions.set(index); }

    bool isDeclaredException(uint32_t index) const { return m_declaredExceptions.contains(index); }
    void addDeclaredException(uint32_t index) { m_declaredExceptions.set(index); }

    bool isSIMDFunction(uint32_t index) const
    {
        ASSERT(index <= internalFunctionCount());
        ASSERT(functions[index].finishedValidating);

        // See also: B3Procedure::usesSIMD().
        if (!Options::useWebAssemblySIMD())
            return false;
        if (Options::forceAllFunctionsToUseSIMD())
            return true;
        // The LLInt discovers this value.
        ASSERT(Options::useWasmLLInt());

        return functions[index].isSIMDFunction;
    }
    void addSIMDFunction(uint32_t index) { ASSERT(index <= internalFunctionCount()); ASSERT(!functions[index].finishedValidating); functions[index].isSIMDFunction = true; }
    void doneSeeingFunction(uint32_t index) { ASSERT(!functions[index].finishedValidating); functions[index].finishedValidating = true; }

    uint32_t typeCount() const { return typeSignatures.size(); }

    bool hasMemoryImport() const { return memory.isImport(); }

    BranchHint getBranchHint(uint32_t functionOffset, uint32_t branchOffset) const
    {
        auto it = branchHints.find(functionOffset);
        return it == branchHints.end()
            ? BranchHint::Invalid
            : it->value.getBranchHint(branchOffset);
    }

    const BitVector& clobberingTailCalls() const { return m_clobberingTailCalls; }
    bool callCanClobberInstance(uint32_t index) const { return m_clobberingTailCalls.contains(index); }
    void addClobberingTailCall(uint32_t index) { m_clobberingTailCalls.set(index); }

    Vector<Import> imports;
    Vector<TypeIndex> importFunctionTypeIndices;
    Vector<TypeIndex> internalFunctionTypeIndices;
    Vector<TypeIndex> importExceptionTypeIndices;
    Vector<TypeIndex> internalExceptionTypeIndices;
    Vector<Ref<TypeDefinition>> typeSignatures;

    MemoryInformation memory;

    Vector<FunctionData> functions;

    Vector<Export> exports;
    std::optional<uint32_t> startFunctionIndexSpace;
    Vector<Segment::Ptr> data;
    Vector<Element> elements;
    Vector<TableInformation> tables;
    Vector<GlobalInformation> globals;
    unsigned firstInternalGlobal { 0 };
    uint32_t codeSectionSize { 0 };
    Vector<CustomSection> customSections;
    Ref<NameSection> nameSection;
    BranchHints branchHints;
    std::optional<uint32_t> numberOfDataSegments;

    BitVector m_declaredFunctions;
    BitVector m_declaredExceptions;
    mutable BitVector m_referencedFunctions;
    BitVector m_clobberingTailCalls;
};

    
} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
