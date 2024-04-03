/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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

#include "UnlinkedCodeBlock.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

// FIXME: Create UnlinkedCodeBlock inside UnlinkedCodeBlockGenerator.
// https://bugs.webkit.org/show_bug.cgi?id=207212
class UnlinkedCodeBlockGenerator {
    WTF_MAKE_TZONE_ALLOCATED(UnlinkedCodeBlockGenerator);
    WTF_MAKE_NONCOPYABLE(UnlinkedCodeBlockGenerator)
public:
    UnlinkedCodeBlockGenerator(VM& vm, UnlinkedCodeBlock* codeBlock)
        : m_vm(vm)
        , m_codeBlock(vm, codeBlock)
    {
    }

    VM& vm() { return m_vm; }

    bool isConstructor() const { return m_codeBlock->isConstructor(); }
    ConstructorKind constructorKind() const { return m_codeBlock->constructorKind(); }
    SuperBinding superBinding() const { return m_codeBlock->superBinding(); }
    JSParserScriptMode scriptMode() const { return m_codeBlock->scriptMode(); }
    NeedsClassFieldInitializer needsClassFieldInitializer() const { return m_codeBlock->needsClassFieldInitializer(); }
    PrivateBrandRequirement privateBrandRequirement() const { return m_codeBlock->privateBrandRequirement(); }
    SourceParseMode parseMode() const { return m_codeBlock->parseMode(); }
    bool isArrowFunction() { return m_codeBlock->isArrowFunction(); }
    DerivedContextType derivedContextType() const { return m_codeBlock->derivedContextType(); }
    EvalContextType evalContextType() const { return m_codeBlock->evalContextType(); }
    bool isArrowFunctionContext() const { return m_codeBlock->isArrowFunctionContext(); }
    bool isClassContext() const { return m_codeBlock->isClassContext(); }
    unsigned numCalleeLocals() const { return m_codeBlock->m_numCalleeLocals; }
    unsigned numVars() const { return m_codeBlock->m_numVars; }
    unsigned numParameters() const { return m_codeBlock->numParameters(); }
    VirtualRegister thisRegister() const { return m_codeBlock->thisRegister(); }
    VirtualRegister scopeRegister() const { return m_codeBlock->scopeRegister(); }
    bool wasCompiledWithDebuggingOpcodes() const { return m_codeBlock->wasCompiledWithDebuggingOpcodes(); }
    bool hasCheckpoints() const { return m_codeBlock->hasCheckpoints(); }
    bool hasTailCalls() const { return m_codeBlock->hasTailCalls(); }

    // Updating UnlinkedCodeBlock.
    void setHasCheckpoints() { m_codeBlock->setHasCheckpoints(); }
    void setHasTailCalls() { m_codeBlock->setHasTailCalls(); }
    void setNumCalleeLocals(unsigned numCalleeLocals) { m_codeBlock->m_numCalleeLocals = numCalleeLocals; }
    void setNumVars(unsigned numVars) { m_codeBlock->m_numVars = numVars; }
    void setThisRegister(VirtualRegister thisRegister) { m_codeBlock->setThisRegister(thisRegister); }
    void setScopeRegister(VirtualRegister thisRegister) { m_codeBlock->setScopeRegister(thisRegister); }
    void setNumParameters(unsigned newValue) { m_codeBlock->setNumParameters(newValue); }

    UnlinkedMetadataTable& metadata() { return m_codeBlock->metadata(); }
    void addExpressionInfo(unsigned instructionOffset, unsigned divot, unsigned startOffset, unsigned endOffset, LineColumn);
    void addTypeProfilerExpressionInfo(unsigned instructionOffset, unsigned startDivot, unsigned endDivot);
    void addOpProfileControlFlowBytecodeOffset(JSInstructionStream::Offset offset)
    {
        m_opProfileControlFlowBytecodeOffsets.append(offset);
    }

    size_t numberOfJumpTargets() const { return m_jumpTargets.size(); }
    void addJumpTarget(unsigned jumpTarget) { m_jumpTargets.append(jumpTarget); }
    unsigned jumpTarget(int index) const { return m_jumpTargets[index]; }
    unsigned lastJumpTarget() const { return m_jumpTargets.last(); }

    size_t numberOfUnlinkedSwitchJumpTables() const { return m_unlinkedSwitchJumpTables.size(); }
    UnlinkedSimpleJumpTable& addUnlinkedSwitchJumpTable() { m_unlinkedSwitchJumpTables.append(UnlinkedSimpleJumpTable()); return m_unlinkedSwitchJumpTables.last(); }
    UnlinkedSimpleJumpTable& unlinkedSwitchJumpTable(int tableIndex) { return m_unlinkedSwitchJumpTables[tableIndex]; }

    size_t numberOfUnlinkedStringSwitchJumpTables() const { return m_unlinkedStringSwitchJumpTables.size(); }
    UnlinkedStringJumpTable& addUnlinkedStringSwitchJumpTable() { m_unlinkedStringSwitchJumpTables.append(UnlinkedStringJumpTable()); return m_unlinkedStringSwitchJumpTables.last(); }
    UnlinkedStringJumpTable& unlinkedStringSwitchJumpTable(int tableIndex) { return m_unlinkedStringSwitchJumpTables[tableIndex]; }

    size_t numberOfExceptionHandlers() const { return m_exceptionHandlers.size(); }
    UnlinkedHandlerInfo& exceptionHandler(int index) { return m_exceptionHandlers[index]; }
    void addExceptionHandler(const UnlinkedHandlerInfo& handler) { m_exceptionHandlers.append(handler); }
    UnlinkedHandlerInfo* handlerForBytecodeIndex(BytecodeIndex, RequiredHandler = RequiredHandler::AnyHandler);
    UnlinkedHandlerInfo* handlerForIndex(unsigned, RequiredHandler = RequiredHandler::AnyHandler);

    BitVector& bitVector(size_t i) { return m_bitVectors[i]; }
    unsigned addBitVector(BitVector&& bitVector)
    {
        m_bitVectors.append(WTFMove(bitVector));
        return m_bitVectors.size() - 1;
    }

    unsigned numberOfConstantIdentifierSets() const { return m_constantIdentifierSets.size(); }
    const Vector<IdentifierSet>& constantIdentifierSets() { return m_constantIdentifierSets; }
    unsigned addSetConstant(IdentifierSet&& set)
    {
        ASSERT(m_vm.heap.isDeferred());
        unsigned result = m_constantIdentifierSets.size();
        m_constantIdentifierSets.append(WTFMove(set));
        return result;
    }

    const WriteBarrier<Unknown>& constantRegister(VirtualRegister reg) const { return m_constantRegisters[reg.toConstantIndex()]; }
    const Vector<WriteBarrier<Unknown>>& constantRegisters() { return m_constantRegisters; }
    ALWAYS_INLINE JSValue getConstant(VirtualRegister reg) const { return m_constantRegisters[reg.toConstantIndex()].get(); }

    SourceCodeRepresentation constantSourceCodeRepresentation(VirtualRegister reg) const
    {
        return constantSourceCodeRepresentation(reg.toConstantIndex());
    }
    SourceCodeRepresentation constantSourceCodeRepresentation(unsigned index) const
    {
        if (index < m_constantsSourceCodeRepresentation.size())
            return m_constantsSourceCodeRepresentation[index];
        return SourceCodeRepresentation::Other;
    }

    unsigned addConstant(JSValue v, SourceCodeRepresentation sourceCodeRepresentation = SourceCodeRepresentation::Other)
    {
        ASSERT(m_vm.heap.isDeferred());
        unsigned result = m_constantRegisters.size();
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantRegisters.last().setWithoutWriteBarrier(v);
        m_constantsSourceCodeRepresentation.append(sourceCodeRepresentation);
        return result;
    }
    unsigned addConstant(LinkTimeConstant linkTimeConstant)
    {
        ASSERT(m_vm.heap.isDeferred());
        unsigned result = m_constantRegisters.size();
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantRegisters.last().setWithoutWriteBarrier(jsNumber(static_cast<int32_t>(linkTimeConstant)));
        m_constantsSourceCodeRepresentation.append(SourceCodeRepresentation::LinkTimeConstant);
        return result;
    }

    unsigned addFunctionDecl(UnlinkedFunctionExecutable* n)
    {
        ASSERT(m_vm.heap.isDeferred());
        unsigned size = m_functionDecls.size();
        m_functionDecls.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_functionDecls.last().setWithoutWriteBarrier(n);
        return size;
    }

    unsigned addFunctionExpr(UnlinkedFunctionExecutable* n)
    {
        unsigned size = m_functionExprs.size();
        m_functionExprs.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_functionExprs.last().setWithoutWriteBarrier(n);
        return size;
    }

    size_t numberOfIdentifiers() const { return m_identifiers.size(); }
    const Identifier& identifier(int index) const { return m_identifiers[index]; }
    void addIdentifier(const Identifier& i) { return m_identifiers.append(i); }

    using OutOfLineJumpTargets = HashMap<JSInstructionStream::Offset, int>;
    void addOutOfLineJumpTarget(JSInstructionStream::Offset, int target);
    int outOfLineJumpOffset(JSInstructionStream::Offset);
    int outOfLineJumpOffset(const JSInstructionStream::Ref& instruction)
    {
        return outOfLineJumpOffset(instruction.offset());
    }
    OutOfLineJumpTargets replaceOutOfLineJumpTargets()
    {
        OutOfLineJumpTargets newJumpTargets;
        std::swap(m_outOfLineJumpTargets, newJumpTargets);
        return newJumpTargets;
    }

    size_t metadataSizeInBytes() { return m_codeBlock->metadataSizeInBytes(); }

    void applyModification(BytecodeRewriter&, JSInstructionStreamWriter&);

    void finalize(std::unique_ptr<JSInstructionStream>);

    void dump(PrintStream&) const;

    unsigned addBinaryArithProfile() { return m_numBinaryArithProfiles++; }
    unsigned addUnaryArithProfile() { return m_numUnaryArithProfiles++; }

private:
    VM& m_vm;
    Strong<UnlinkedCodeBlock> m_codeBlock;
    // In non-RareData.
    Vector<JSInstructionStream::Offset> m_jumpTargets;
    Vector<Identifier> m_identifiers;
    Vector<WriteBarrier<Unknown>> m_constantRegisters;
    Vector<SourceCodeRepresentation> m_constantsSourceCodeRepresentation;
    Vector<WriteBarrier<UnlinkedFunctionExecutable>> m_functionDecls;
    Vector<WriteBarrier<UnlinkedFunctionExecutable>> m_functionExprs;
    ExpressionInfo::Encoder m_expressionInfoEncoder;
    OutOfLineJumpTargets m_outOfLineJumpTargets;
    // In RareData.
    Vector<UnlinkedHandlerInfo> m_exceptionHandlers;
    Vector<UnlinkedSimpleJumpTable> m_unlinkedSwitchJumpTables;
    Vector<UnlinkedStringJumpTable> m_unlinkedStringSwitchJumpTables;
    HashMap<unsigned, UnlinkedCodeBlock::RareData::TypeProfilerExpressionRange> m_typeProfilerInfoMap;
    Vector<JSInstructionStream::Offset> m_opProfileControlFlowBytecodeOffsets;
    Vector<BitVector> m_bitVectors;
    Vector<IdentifierSet> m_constantIdentifierSets;
    unsigned m_numBinaryArithProfiles { 0 };
    unsigned m_numUnaryArithProfiles { 0 };
};

} // namespace JSC
