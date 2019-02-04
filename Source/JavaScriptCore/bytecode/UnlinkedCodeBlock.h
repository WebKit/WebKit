/*
 * Copyright (C) 2012-2016 Apple Inc. All Rights Reserved.
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

#include "BytecodeConventions.h"
#include "CodeType.h"
#include "DFGExitProfile.h"
#include "ExpressionRangeInfo.h"
#include "HandlerInfo.h"
#include "Identifier.h"
#include "InstructionStream.h"
#include "JSCast.h"
#include "LockDuringMarking.h"
#include "Opcode.h"
#include "ParserModes.h"
#include "RegExp.h"
#include "SpecialPointer.h"
#include "UnlinkedFunctionExecutable.h"
#include "UnlinkedMetadataTable.h"
#include "VirtualRegister.h"
#include <algorithm>
#include <wtf/BitVector.h>
#include <wtf/HashSet.h>
#include <wtf/TriState.h>
#include <wtf/Vector.h>
#include <wtf/text/UniquedStringImpl.h>

namespace JSC {

class BytecodeGenerator;
class BytecodeLivenessAnalysis;
class BytecodeRewriter;
class CodeBlock;
class Debugger;
class FunctionExecutable;
class ParserError;
class ScriptExecutable;
class SourceCode;
class SourceProvider;
class UnlinkedCodeBlock;
class UnlinkedFunctionCodeBlock;
class UnlinkedFunctionExecutable;
struct ExecutableInfo;

template<typename CodeBlockType>
class CachedCodeBlock;

typedef unsigned UnlinkedValueProfile;
typedef unsigned UnlinkedArrayProfile;
typedef unsigned UnlinkedArrayAllocationProfile;
typedef unsigned UnlinkedObjectAllocationProfile;
typedef unsigned UnlinkedLLIntCallLinkInfo;
using ConstantIdentifierSetEntry = std::pair<IdentifierSet, unsigned>;

struct UnlinkedStringJumpTable {
    struct OffsetLocation {
        int32_t branchOffset;
    };

    typedef HashMap<RefPtr<StringImpl>, OffsetLocation> StringOffsetTable;
    StringOffsetTable offsetTable;

    inline int32_t offsetForValue(StringImpl* value, int32_t defaultOffset)
    {
        StringOffsetTable::const_iterator end = offsetTable.end();
        StringOffsetTable::const_iterator loc = offsetTable.find(value);
        if (loc == end)
            return defaultOffset;
        return loc->value.branchOffset;
    }

};

struct UnlinkedSimpleJumpTable {
    Vector<int32_t> branchOffsets;
    int32_t min;

    int32_t offsetForValue(int32_t value, int32_t defaultOffset);
    void add(int32_t key, int32_t offset)
    {
        if (!branchOffsets[key])
            branchOffsets[key] = offset;
    }
};

class UnlinkedCodeBlock : public JSCell {
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags;

    static const bool needsDestruction = true;

    enum { CallFunction, ApplyFunction };

    bool isConstructor() const { return m_isConstructor; }
    bool isStrictMode() const { return m_isStrictMode; }
    bool usesEval() const { return m_usesEval; }
    SourceParseMode parseMode() const { return m_parseMode; }
    bool isArrowFunction() const { return isArrowFunctionParseMode(parseMode()); }
    DerivedContextType derivedContextType() const { return static_cast<DerivedContextType>(m_derivedContextType); }
    EvalContextType evalContextType() const { return static_cast<EvalContextType>(m_evalContextType); }
    bool isArrowFunctionContext() const { return m_isArrowFunctionContext; }
    bool isClassContext() const { return m_isClassContext; }
    bool hasTailCalls() const { return m_hasTailCalls; }
    void setHasTailCalls() { m_hasTailCalls = true; }
    bool allowDirectEvalCache() const { return !(m_features & NoEvalCacheFeature); }

    void addExpressionInfo(unsigned instructionOffset, int divot,
        int startOffset, int endOffset, unsigned line, unsigned column);

    void addTypeProfilerExpressionInfo(unsigned instructionOffset, unsigned startDivot, unsigned endDivot);

    bool hasExpressionInfo() { return m_expressionInfo.size(); }
    const Vector<ExpressionRangeInfo>& expressionInfo() { return m_expressionInfo; }

    // Special registers
    void setThisRegister(VirtualRegister thisRegister) { m_thisRegister = thisRegister; }
    void setScopeRegister(VirtualRegister scopeRegister) { m_scopeRegister = scopeRegister; }

    bool usesGlobalObject() const { return m_globalObjectRegister.isValid(); }
    void setGlobalObjectRegister(VirtualRegister globalObjectRegister) { m_globalObjectRegister = globalObjectRegister; }
    VirtualRegister globalObjectRegister() const { return m_globalObjectRegister; }

    // Parameter information
    void setNumParameters(int newValue) { m_numParameters = newValue; }
    void addParameter() { m_numParameters++; }
    unsigned numParameters() const { return m_numParameters; }

    // Constant Pools

    size_t numberOfIdentifiers() const { return m_identifiers.size(); }
    void addIdentifier(const Identifier& i) { return m_identifiers.append(i); }
    const Identifier& identifier(int index) const { return m_identifiers[index]; }
    const Vector<Identifier>& identifiers() const { return m_identifiers; }

    const Vector<BitVector>& bitVectors() const { return m_bitVectors; }
    BitVector& bitVector(size_t i) { return m_bitVectors[i]; }
    unsigned addBitVector(BitVector&& bitVector)
    {
        m_bitVectors.append(WTFMove(bitVector));
        return m_bitVectors.size() - 1;
    }

    void addSetConstant(IdentifierSet& set)
    {
        VM& vm = *this->vm();
        auto locker = lockDuringMarking(vm.heap, cellLock());
        unsigned result = m_constantRegisters.size();
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantsSourceCodeRepresentation.append(SourceCodeRepresentation::Other);
        m_constantIdentifierSets.append(ConstantIdentifierSetEntry(set, result));
    }

    unsigned addConstant(JSValue v, SourceCodeRepresentation sourceCodeRepresentation = SourceCodeRepresentation::Other)
    {
        VM& vm = *this->vm();
        auto locker = lockDuringMarking(vm.heap, cellLock());
        unsigned result = m_constantRegisters.size();
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantRegisters.last().set(vm, this, v);
        m_constantsSourceCodeRepresentation.append(sourceCodeRepresentation);
        return result;
    }
    unsigned addConstant(LinkTimeConstant type)
    {
        VM& vm = *this->vm();
        auto locker = lockDuringMarking(vm.heap, cellLock());
        unsigned result = m_constantRegisters.size();
        ASSERT(result);
        unsigned index = static_cast<unsigned>(type);
        ASSERT(index < LinkTimeConstantCount);
        m_linkTimeConstants[index] = result;
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantsSourceCodeRepresentation.append(SourceCodeRepresentation::Other);
        return result;
    }

    unsigned registerIndexForLinkTimeConstant(LinkTimeConstant type)
    {
        unsigned index = static_cast<unsigned>(type);
        ASSERT(index < LinkTimeConstantCount);
        return m_linkTimeConstants[index];
    }
    const Vector<WriteBarrier<Unknown>>& constantRegisters() { return m_constantRegisters; }
    const Vector<ConstantIdentifierSetEntry>& constantIdentifierSets() { return m_constantIdentifierSets; }
    const WriteBarrier<Unknown>& constantRegister(int index) const { return m_constantRegisters[index - FirstConstantRegisterIndex]; }
    ALWAYS_INLINE bool isConstantRegisterIndex(int index) const { return index >= FirstConstantRegisterIndex; }
    ALWAYS_INLINE JSValue getConstant(int index) const { return m_constantRegisters[index - FirstConstantRegisterIndex].get(); }
    const Vector<SourceCodeRepresentation>& constantsSourceCodeRepresentation() { return m_constantsSourceCodeRepresentation; }

    // Jumps
    size_t numberOfJumpTargets() const { return m_jumpTargets.size(); }
    void addJumpTarget(unsigned jumpTarget) { m_jumpTargets.append(jumpTarget); }
    unsigned jumpTarget(int index) const { return m_jumpTargets[index]; }
    unsigned lastJumpTarget() const { return m_jumpTargets.last(); }

    UnlinkedHandlerInfo* handlerForBytecodeOffset(unsigned bytecodeOffset, RequiredHandler = RequiredHandler::AnyHandler);
    UnlinkedHandlerInfo* handlerForIndex(unsigned, RequiredHandler = RequiredHandler::AnyHandler);

    bool isBuiltinFunction() const { return m_isBuiltinFunction; }

    ConstructorKind constructorKind() const { return static_cast<ConstructorKind>(m_constructorKind); }
    SuperBinding superBinding() const { return static_cast<SuperBinding>(m_superBinding); }
    JSParserScriptMode scriptMode() const { return static_cast<JSParserScriptMode>(m_scriptMode); }

    void shrinkToFit();

    void setInstructions(std::unique_ptr<InstructionStream>);
    const InstructionStream& instructions() const;

    int numCalleeLocals() const { return m_numCalleeLocals; }
    int numVars() const { return m_numVars; }

    // Jump Tables

    size_t numberOfSwitchJumpTables() const { return m_rareData ? m_rareData->m_switchJumpTables.size() : 0; }
    UnlinkedSimpleJumpTable& addSwitchJumpTable() { createRareDataIfNecessary(); m_rareData->m_switchJumpTables.append(UnlinkedSimpleJumpTable()); return m_rareData->m_switchJumpTables.last(); }
    UnlinkedSimpleJumpTable& switchJumpTable(int tableIndex) { ASSERT(m_rareData); return m_rareData->m_switchJumpTables[tableIndex]; }

    size_t numberOfStringSwitchJumpTables() const { return m_rareData ? m_rareData->m_stringSwitchJumpTables.size() : 0; }
    UnlinkedStringJumpTable& addStringSwitchJumpTable() { createRareDataIfNecessary(); m_rareData->m_stringSwitchJumpTables.append(UnlinkedStringJumpTable()); return m_rareData->m_stringSwitchJumpTables.last(); }
    UnlinkedStringJumpTable& stringSwitchJumpTable(int tableIndex) { ASSERT(m_rareData); return m_rareData->m_stringSwitchJumpTables[tableIndex]; }

    unsigned addFunctionDecl(UnlinkedFunctionExecutable* n)
    {
        VM& vm = *this->vm();
        auto locker = lockDuringMarking(vm.heap, cellLock());
        unsigned size = m_functionDecls.size();
        m_functionDecls.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_functionDecls.last().set(vm, this, n);
        return size;
    }
    UnlinkedFunctionExecutable* functionDecl(int index) { return m_functionDecls[index].get(); }
    size_t numberOfFunctionDecls() { return m_functionDecls.size(); }
    unsigned addFunctionExpr(UnlinkedFunctionExecutable* n)
    {
        VM& vm = *this->vm();
        auto locker = lockDuringMarking(vm.heap, cellLock());
        unsigned size = m_functionExprs.size();
        m_functionExprs.append(WriteBarrier<UnlinkedFunctionExecutable>());
        m_functionExprs.last().set(vm, this, n);
        return size;
    }
    UnlinkedFunctionExecutable* functionExpr(int index) { return m_functionExprs[index].get(); }
    size_t numberOfFunctionExprs() { return m_functionExprs.size(); }

    // Exception handling support
    size_t numberOfExceptionHandlers() const { return m_rareData ? m_rareData->m_exceptionHandlers.size() : 0; }
    void addExceptionHandler(const UnlinkedHandlerInfo& handler) { createRareDataIfNecessary(); return m_rareData->m_exceptionHandlers.append(handler); }
    UnlinkedHandlerInfo& exceptionHandler(int index) { ASSERT(m_rareData); return m_rareData->m_exceptionHandlers[index]; }

    CodeType codeType() const { return m_codeType; }

    VirtualRegister thisRegister() const { return m_thisRegister; }
    VirtualRegister scopeRegister() const { return m_scopeRegister; }

    void addPropertyAccessInstruction(InstructionStream::Offset propertyAccessInstruction)
    {
        m_propertyAccessInstructions.append(propertyAccessInstruction);
    }

    size_t numberOfPropertyAccessInstructions() const { return m_propertyAccessInstructions.size(); }
    const Vector<InstructionStream::Offset>& propertyAccessInstructions() const { return m_propertyAccessInstructions; }

    bool hasRareData() const { return m_rareData.get(); }

    int lineNumberForBytecodeOffset(unsigned bytecodeOffset);

    void expressionRangeForBytecodeOffset(unsigned bytecodeOffset, int& divot,
        int& startOffset, int& endOffset, unsigned& line, unsigned& column) const;

    bool typeProfilerExpressionInfoForBytecodeOffset(unsigned bytecodeOffset, unsigned& startDivot, unsigned& endDivot);

    void recordParse(CodeFeatures features, bool hasCapturedVariables, unsigned lineCount, unsigned endColumn)
    {
        m_features = features;
        m_hasCapturedVariables = hasCapturedVariables;
        m_lineCount = lineCount;
        // For the UnlinkedCodeBlock, startColumn is always 0.
        m_endColumn = endColumn;
    }

    const String& sourceURLDirective() const { return m_sourceURLDirective; }
    const String& sourceMappingURLDirective() const { return m_sourceMappingURLDirective; }
    void setSourceURLDirective(const String& sourceURL) { m_sourceURLDirective = sourceURL; }
    void setSourceMappingURLDirective(const String& sourceMappingURL) { m_sourceMappingURLDirective = sourceMappingURL; }

    CodeFeatures codeFeatures() const { return m_features; }
    bool hasCapturedVariables() const { return m_hasCapturedVariables; }
    unsigned lineCount() const { return m_lineCount; }
    ALWAYS_INLINE unsigned startColumn() const { return 0; }
    unsigned endColumn() const { return m_endColumn; }

    void addOpProfileControlFlowBytecodeOffset(InstructionStream::Offset offset)
    {
        createRareDataIfNecessary();
        m_rareData->m_opProfileControlFlowBytecodeOffsets.append(offset);
    }
    const Vector<InstructionStream::Offset>& opProfileControlFlowBytecodeOffsets() const
    {
        ASSERT(m_rareData);
        return m_rareData->m_opProfileControlFlowBytecodeOffsets;
    }
    bool hasOpProfileControlFlowBytecodeOffsets() const
    {
        return m_rareData && !m_rareData->m_opProfileControlFlowBytecodeOffsets.isEmpty();
    }

    void dumpExpressionRangeInfo(); // For debugging purpose only.

    bool wasCompiledWithDebuggingOpcodes() const { return m_wasCompiledWithDebuggingOpcodes; }

    TriState didOptimize() const { return m_didOptimize; }
    void setDidOptimize(TriState didOptimize) { m_didOptimize = didOptimize; }

    void dump(PrintStream&) const;

    BytecodeLivenessAnalysis& livenessAnalysis(CodeBlock* codeBlock)
    {
        if (m_liveness)
            return *m_liveness;
        return livenessAnalysisSlow(codeBlock);
    }

#if ENABLE(DFG_JIT)
    bool hasExitSite(const ConcurrentJSLocker& locker, const DFG::FrequentExitSite& site) const
    {
        return m_exitProfile.hasExitSite(locker, site);
    }

    bool hasExitSite(const DFG::FrequentExitSite& site)
    {
        ConcurrentJSLocker locker(m_lock);
        return hasExitSite(locker, site);
    }

    DFG::ExitProfile& exitProfile() { return m_exitProfile; }
#endif

    UnlinkedMetadataTable& metadata() { return m_metadata.get(); }

    size_t metadataSizeInBytes()
    {
        return m_metadata->sizeInBytes();
    }


protected:
    UnlinkedCodeBlock(VM*, Structure*, CodeType, const ExecutableInfo&, DebuggerMode);

    template<typename CodeBlockType>
    UnlinkedCodeBlock(Decoder&, Structure*, const CachedCodeBlock<CodeBlockType>&);

    ~UnlinkedCodeBlock();

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
    }

private:
    friend class BytecodeRewriter;
    friend class BytecodeGenerator;

    template<typename CodeBlockType>
    friend class CachedCodeBlock;

    void applyModification(BytecodeRewriter&, InstructionStreamWriter&);

    void createRareDataIfNecessary()
    {
        if (!m_rareData) {
            auto locker = lockDuringMarking(*heap(), cellLock());
            m_rareData = std::make_unique<RareData>();
        }
    }

    void getLineAndColumn(const ExpressionRangeInfo&, unsigned& line, unsigned& column) const;
    BytecodeLivenessAnalysis& livenessAnalysisSlow(CodeBlock*);

    std::unique_ptr<InstructionStream> m_instructions;
    std::unique_ptr<BytecodeLivenessAnalysis> m_liveness;

    VirtualRegister m_thisRegister;
    VirtualRegister m_scopeRegister;
    VirtualRegister m_globalObjectRegister;

    String m_sourceURLDirective;
    String m_sourceMappingURLDirective;
    Ref<UnlinkedMetadataTable> m_metadata;

#if ENABLE(DFG_JIT)
    DFG::ExitProfile m_exitProfile;
#endif

    unsigned m_usesEval : 1;
    unsigned m_isStrictMode : 1;
    unsigned m_isConstructor : 1;
    unsigned m_hasCapturedVariables : 1;
    unsigned m_isBuiltinFunction : 1;
    unsigned m_superBinding : 1;
    unsigned m_scriptMode: 1;
    unsigned m_isArrowFunctionContext : 1;
    unsigned m_isClassContext : 1;
    unsigned m_wasCompiledWithDebuggingOpcodes : 1;
    unsigned m_constructorKind : 2;
    unsigned m_derivedContextType : 2;
    unsigned m_evalContextType : 2;
    unsigned m_hasTailCalls : 1;

    unsigned m_lineCount { 0 };
    unsigned m_endColumn { UINT_MAX };

    int m_numVars { 0 };
    int m_numCalleeLocals { 0 };
    int m_numParameters { 0 };

public:
    ConcurrentJSLock m_lock;
private:
    CodeFeatures m_features { 0 };
    TriState m_didOptimize;
    SourceParseMode m_parseMode;
    CodeType m_codeType;

    Vector<InstructionStream::Offset> m_jumpTargets;

    Vector<InstructionStream::Offset> m_propertyAccessInstructions;

    // Constant Pools
    Vector<Identifier> m_identifiers;
    Vector<BitVector> m_bitVectors;
    Vector<WriteBarrier<Unknown>> m_constantRegisters;
    Vector<ConstantIdentifierSetEntry> m_constantIdentifierSets;
    Vector<SourceCodeRepresentation> m_constantsSourceCodeRepresentation;
    typedef Vector<WriteBarrier<UnlinkedFunctionExecutable>> FunctionExpressionVector;
    FunctionExpressionVector m_functionDecls;
    FunctionExpressionVector m_functionExprs;
    std::array<unsigned, LinkTimeConstantCount> m_linkTimeConstants;

public:
    struct RareData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Vector<UnlinkedHandlerInfo> m_exceptionHandlers;

        // Jump Tables
        Vector<UnlinkedSimpleJumpTable> m_switchJumpTables;
        Vector<UnlinkedStringJumpTable> m_stringSwitchJumpTables;

        Vector<ExpressionRangeInfo::FatPosition> m_expressionInfoFatPositions;

        struct TypeProfilerExpressionRange {
            unsigned m_startDivot;
            unsigned m_endDivot;
        };
        HashMap<unsigned, TypeProfilerExpressionRange> m_typeProfilerInfoMap;
        Vector<InstructionStream::Offset> m_opProfileControlFlowBytecodeOffsets;
    };

    void addOutOfLineJumpTarget(InstructionStream::Offset, int target);
    int outOfLineJumpOffset(InstructionStream::Offset);
    int outOfLineJumpOffset(const InstructionStream::Ref& instruction)
    {
        return outOfLineJumpOffset(instruction.offset());
    }

private:
    using OutOfLineJumpTargets = HashMap<InstructionStream::Offset, int>;

    OutOfLineJumpTargets replaceOutOfLineJumpTargets()
    {
        OutOfLineJumpTargets newJumpTargets;
        std::swap(m_outOfLineJumpTargets, newJumpTargets);
        return newJumpTargets;
    }

    OutOfLineJumpTargets m_outOfLineJumpTargets;
    std::unique_ptr<RareData> m_rareData;
    Vector<ExpressionRangeInfo> m_expressionInfo;

protected:
    static void visitChildren(JSCell*, SlotVisitor&);
    static size_t estimatedSize(JSCell*, VM&);

public:
    DECLARE_INFO;
};

}
