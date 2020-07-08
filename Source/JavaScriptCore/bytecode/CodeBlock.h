/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
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

#include "ArrayProfile.h"
#include "BytecodeConventions.h"
#include "CallLinkInfo.h"
#include "CodeBlockHash.h"
#include "CodeOrigin.h"
#include "CodeType.h"
#include "CompilationResult.h"
#include "ConcurrentJSLock.h"
#include "DFGCodeOriginPool.h"
#include "DFGCommon.h"
#include "DirectEvalCodeCache.h"
#include "EvalExecutable.h"
#include "ExecutionCounter.h"
#include "ExpressionRangeInfo.h"
#include "FunctionExecutable.h"
#include "HandlerInfo.h"
#include "ICStatusMap.h"
#include "Instruction.h"
#include "InstructionStream.h"
#include "JITCode.h"
#include "JITCodeMap.h"
#include "JITMathICForwards.h"
#include "JSCast.h"
#include "JSGlobalObject.h"
#include "JumpTable.h"
#include "LLIntCallLinkInfo.h"
#include "LazyOperandValueProfile.h"
#include "MetadataTable.h"
#include "ModuleProgramExecutable.h"
#include "ObjectAllocationProfile.h"
#include "Options.h"
#include "Printer.h"
#include "ProfilerJettisonReason.h"
#include "ProgramExecutable.h"
#include "PutPropertySlot.h"
#include "ValueProfile.h"
#include "VirtualRegister.h"
#include "Watchpoint.h"
#include <wtf/Bag.h>
#include <wtf/FastMalloc.h>
#include <wtf/RefCountedArray.h>
#include <wtf/RefPtr.h>
#include <wtf/SegmentedVector.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

#if ENABLE(DFG_JIT)
namespace DFG {
struct OSRExitState;
} // namespace DFG
#endif

class UnaryArithProfile;
class BinaryArithProfile;
class BytecodeLivenessAnalysis;
class CodeBlockSet;
class ExecutableToCodeBlockEdge;
class JSModuleEnvironment;
class LLIntOffsetsExtractor;
class LLIntPrototypeLoadAdaptiveStructureWatchpoint;
class MetadataTable;
class PCToCodeOriginMap;
class RegisterAtOffsetList;
class StructureStubInfo;
struct ByValInfo;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CodeBlockRareData);

enum class AccessType : int8_t;

struct OpCatch;

enum ReoptimizationMode { DontCountReoptimization, CountReoptimization };

class CodeBlock : public JSCell {
    typedef JSCell Base;
    friend class BytecodeLivenessAnalysis;
    friend class JIT;
    friend class LLIntOffsetsExtractor;

public:

    enum CopyParsedBlockTag { CopyParsedBlock };

    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;
    static constexpr bool needsDestruction = true;

    template<typename, SubspaceAccess>
    static void subspaceFor(VM&)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
    // GC strongly assumes CodeBlock is not a PreciseAllocation for now.
    static constexpr uint8_t numberOfLowerTierCells = 0;

    DECLARE_INFO;

protected:
    CodeBlock(VM&, Structure*, CopyParsedBlockTag, CodeBlock& other);
    CodeBlock(VM&, Structure*, ScriptExecutable* ownerExecutable, UnlinkedCodeBlock*, JSScope*);

    void finishCreation(VM&, CopyParsedBlockTag, CodeBlock& other);
    bool finishCreation(VM&, ScriptExecutable* ownerExecutable, UnlinkedCodeBlock*, JSScope*);
    
    void finishCreationCommon(VM&);

    WriteBarrier<JSGlobalObject> m_globalObject;

public:
    JS_EXPORT_PRIVATE ~CodeBlock();

    UnlinkedCodeBlock* unlinkedCodeBlock() const { return m_unlinkedCode.get(); }

    CString inferredName() const;
    CodeBlockHash hash() const;
    bool hasHash() const;
    bool isSafeToComputeHash() const;
    CString hashAsStringIfPossible() const;
    CString sourceCodeForTools() const; // Not quite the actual source we parsed; this will do things like prefix the source for a function with a reified signature.
    CString sourceCodeOnOneLine() const; // As sourceCodeForTools(), but replaces all whitespace runs with a single space.
    void dumpAssumingJITType(PrintStream&, JITType) const;
    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

    MetadataTable* metadataTable() const { return m_metadata.get(); }

    int numParameters() const { return m_numParameters; }
    void setNumParameters(int newValue);

    int numberOfArgumentsToSkip() const { return m_numberOfArgumentsToSkip; }

    int numCalleeLocals() const { return m_numCalleeLocals; }

    int numVars() const { return m_numVars; }
    int numTmps() const { return m_unlinkedCode->hasCheckpoints() * maxNumCheckpointTmps; }

    int* addressOfNumParameters() { return &m_numParameters; }
    static ptrdiff_t offsetOfNumParameters() { return OBJECT_OFFSETOF(CodeBlock, m_numParameters); }

    CodeBlock* alternative() const { return static_cast<CodeBlock*>(m_alternative.get()); }
    void setAlternative(VM&, CodeBlock*);

    template <typename Functor> void forEachRelatedCodeBlock(Functor&& functor)
    {
        Functor f(std::forward<Functor>(functor));
        Vector<CodeBlock*, 4> codeBlocks;
        codeBlocks.append(this);

        while (!codeBlocks.isEmpty()) {
            CodeBlock* currentCodeBlock = codeBlocks.takeLast();
            f(currentCodeBlock);

            if (CodeBlock* alternative = currentCodeBlock->alternative())
                codeBlocks.append(alternative);
            if (CodeBlock* osrEntryBlock = currentCodeBlock->specialOSREntryBlockOrNull())
                codeBlocks.append(osrEntryBlock);
        }
    }
    
    CodeSpecializationKind specializationKind() const
    {
        return specializationFromIsConstruct(isConstructor());
    }

    CodeBlock* alternativeForJettison();    
    JS_EXPORT_PRIVATE CodeBlock* baselineAlternative();
    
    // FIXME: Get rid of this.
    // https://bugs.webkit.org/show_bug.cgi?id=123677
    CodeBlock* baselineVersion();

    static size_t estimatedSize(JSCell*, VM&);
    static void visitChildren(JSCell*, SlotVisitor&);
    static void destroy(JSCell*);
    void visitChildren(SlotVisitor&);
    void finalizeUnconditionally(VM&);

    void notifyLexicalBindingUpdate();

    void dumpSource();
    void dumpSource(PrintStream&);

    void dumpBytecode();
    void dumpBytecode(PrintStream&);
    void dumpBytecode(PrintStream& out, const InstructionStream::Ref& it, const ICStatusMap& = ICStatusMap());
    void dumpBytecode(PrintStream& out, unsigned bytecodeOffset, const ICStatusMap& = ICStatusMap());

    void dumpExceptionHandlers(PrintStream&);
    void printStructures(PrintStream&, const Instruction*);
    void printStructure(PrintStream&, const char* name, const Instruction*, int operand);

    void dumpMathICStats();

    bool isConstructor() const { return m_unlinkedCode->isConstructor(); }
    CodeType codeType() const { return m_unlinkedCode->codeType(); }

    JSParserScriptMode scriptMode() const { return m_unlinkedCode->scriptMode(); }

    bool hasInstalledVMTrapBreakpoints() const;
    bool installVMTrapBreakpoints();

    inline bool isKnownCell(VirtualRegister reg)
    {
        // FIXME: Consider adding back the optimization where we return true if `reg` is `this` and we're in sloppy mode.
        // https://bugs.webkit.org/show_bug.cgi?id=210145
        if (reg.isConstant())
            return getConstant(reg).isCell();

        return false;
    }

    ALWAYS_INLINE bool isTemporaryRegister(VirtualRegister reg)
    {
        return reg.offset() >= m_numVars;
    }

    HandlerInfo* handlerForBytecodeIndex(BytecodeIndex, RequiredHandler = RequiredHandler::AnyHandler);
    HandlerInfo* handlerForIndex(unsigned, RequiredHandler = RequiredHandler::AnyHandler);
    void removeExceptionHandlerForCallSite(DisposableCallSiteIndex);
    unsigned lineNumberForBytecodeIndex(BytecodeIndex);
    unsigned columnNumberForBytecodeIndex(BytecodeIndex);
    void expressionRangeForBytecodeIndex(BytecodeIndex, int& divot,
        int& startOffset, int& endOffset, unsigned& line, unsigned& column) const;

    Optional<BytecodeIndex> bytecodeIndexFromCallSiteIndex(CallSiteIndex);

    // Because we might throw out baseline JIT code and all its baseline JIT data (m_jitData),
    // you need to be careful about the lifetime of when you use the return value of this function.
    // The return value may have raw pointers into this data structure that gets thrown away.
    // Specifically, you need to ensure that no GC can be finalized (typically that means no
    // allocations) between calling this and the last use of it.
    void getICStatusMap(const ConcurrentJSLocker&, ICStatusMap& result);
    void getICStatusMap(ICStatusMap& result);
    
#if ENABLE(JIT)
    struct JITData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        Bag<StructureStubInfo> m_stubInfos;
        Bag<JITAddIC> m_addICs;
        Bag<JITMulIC> m_mulICs;
        Bag<JITNegIC> m_negICs;
        Bag<JITSubIC> m_subICs;
        Bag<ByValInfo> m_byValInfos;
        Bag<CallLinkInfo> m_callLinkInfos;
        SentinelLinkedList<CallLinkInfo, PackedRawSentinelNode<CallLinkInfo>> m_incomingCalls;
        SentinelLinkedList<PolymorphicCallNode, PackedRawSentinelNode<PolymorphicCallNode>> m_incomingPolymorphicCalls;
        RefCountedArray<RareCaseProfile> m_rareCaseProfiles;
        std::unique_ptr<PCToCodeOriginMap> m_pcToCodeOriginMap;
        std::unique_ptr<RegisterAtOffsetList> m_calleeSaveRegisters;
        JITCodeMap m_jitCodeMap;
    };

    JITData& ensureJITData(const ConcurrentJSLocker& locker)
    {
        if (LIKELY(m_jitData))
            return *m_jitData;
        return ensureJITDataSlow(locker);
    }
    JITData& ensureJITDataSlow(const ConcurrentJSLocker&);

    JITAddIC* addJITAddIC(BinaryArithProfile*);
    JITMulIC* addJITMulIC(BinaryArithProfile*);
    JITNegIC* addJITNegIC(UnaryArithProfile*);
    JITSubIC* addJITSubIC(BinaryArithProfile*);

    template <typename Generator, typename = typename std::enable_if<std::is_same<Generator, JITAddGenerator>::value>::type>
    JITAddIC* addMathIC(BinaryArithProfile* profile) { return addJITAddIC(profile); }

    template <typename Generator, typename = typename std::enable_if<std::is_same<Generator, JITMulGenerator>::value>::type>
    JITMulIC* addMathIC(BinaryArithProfile* profile) { return addJITMulIC(profile); }

    template <typename Generator, typename = typename std::enable_if<std::is_same<Generator, JITNegGenerator>::value>::type>
    JITNegIC* addMathIC(UnaryArithProfile* profile) { return addJITNegIC(profile); }

    template <typename Generator, typename = typename std::enable_if<std::is_same<Generator, JITSubGenerator>::value>::type>
    JITSubIC* addMathIC(BinaryArithProfile* profile) { return addJITSubIC(profile); }

    StructureStubInfo* addStubInfo(AccessType);

    // O(n) operation. Use getICStatusMap() unless you really only intend to get one stub info.
    StructureStubInfo* findStubInfo(CodeOrigin);
    // O(n) operation. Use getICStatusMap() unless you really only intend to get one by-val-info.
    ByValInfo* findByValInfo(CodeOrigin);

    ByValInfo* addByValInfo();

    CallLinkInfo* addCallLinkInfo();

    // This is a slow function call used primarily for compiling OSR exits in the case
    // that there had been inlining. Chances are if you want to use this, you're really
    // looking for a CallLinkInfoMap to amortize the cost of calling this.
    CallLinkInfo* getCallLinkInfoForBytecodeIndex(BytecodeIndex);
    
    void setJITCodeMap(JITCodeMap&& jitCodeMap)
    {
        ConcurrentJSLocker locker(m_lock);
        ensureJITData(locker).m_jitCodeMap = WTFMove(jitCodeMap);
    }
    const JITCodeMap& jitCodeMap()
    {
        ConcurrentJSLocker locker(m_lock);
        return ensureJITData(locker).m_jitCodeMap;
    }

    void setPCToCodeOriginMap(std::unique_ptr<PCToCodeOriginMap>&&);
    Optional<CodeOrigin> findPC(void* pc);

    void setCalleeSaveRegisters(RegisterSet);
    void setCalleeSaveRegisters(std::unique_ptr<RegisterAtOffsetList>);

    void setRareCaseProfiles(RefCountedArray<RareCaseProfile>&&);
    RareCaseProfile* rareCaseProfileForBytecodeIndex(const ConcurrentJSLocker&, BytecodeIndex);
    unsigned rareCaseProfileCountForBytecodeIndex(const ConcurrentJSLocker&, BytecodeIndex);

    bool likelyToTakeSlowCase(BytecodeIndex bytecodeIndex)
    {
        if (!hasBaselineJITProfiling())
            return false;
        ConcurrentJSLocker locker(m_lock);
        unsigned value = rareCaseProfileCountForBytecodeIndex(locker, bytecodeIndex);
        return value >= Options::likelyToTakeSlowCaseMinimumCount();
    }

    bool couldTakeSlowCase(BytecodeIndex bytecodeIndex)
    {
        if (!hasBaselineJITProfiling())
            return false;
        ConcurrentJSLocker locker(m_lock);
        unsigned value = rareCaseProfileCountForBytecodeIndex(locker, bytecodeIndex);
        return value >= Options::couldTakeSlowCaseMinimumCount();
    }

    // We call this when we want to reattempt compiling something with the baseline JIT. Ideally
    // the baseline JIT would not add data to CodeBlock, but instead it would put its data into
    // a newly created JITCode, which could be thrown away if we bail on JIT compilation. Then we
    // would be able to get rid of this silly function.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=159061
    void resetJITData();
#endif // ENABLE(JIT)

    void unlinkIncomingCalls();

#if ENABLE(JIT)
    void linkIncomingCall(CallFrame* callerFrame, CallLinkInfo*);
    void linkIncomingPolymorphicCall(CallFrame* callerFrame, PolymorphicCallNode*);
#endif // ENABLE(JIT)

    void linkIncomingCall(CallFrame* callerFrame, LLIntCallLinkInfo*);

    const Instruction* outOfLineJumpTarget(const Instruction* pc);
    int outOfLineJumpOffset(InstructionStream::Offset offset)
    {
        return m_unlinkedCode->outOfLineJumpOffset(offset);
    }
    int outOfLineJumpOffset(const Instruction* pc);
    int outOfLineJumpOffset(const InstructionStream::Ref& instruction)
    {
        return outOfLineJumpOffset(instruction.ptr());
    }

    inline unsigned bytecodeOffset(const Instruction* returnAddress)
    {
        const auto* instructionsBegin = instructions().at(0).ptr();
        const auto* instructionsEnd = reinterpret_cast<const Instruction*>(reinterpret_cast<uintptr_t>(instructionsBegin) + instructions().size());
        RELEASE_ASSERT(returnAddress >= instructionsBegin && returnAddress < instructionsEnd);
        return returnAddress - instructionsBegin;
    }

    inline BytecodeIndex bytecodeIndex(const Instruction* returnAddress)
    {
        return BytecodeIndex(bytecodeOffset(returnAddress));
    }

    const InstructionStream& instructions() const { return m_unlinkedCode->instructions(); }
    const Instruction* instructionAt(BytecodeIndex index) const { return instructions().at(index).ptr(); }

    size_t predictedMachineCodeSize();

    unsigned instructionsSize() const { return instructions().size(); }
    unsigned bytecodeCost() const { return m_bytecodeCost; }

    // Exactly equivalent to codeBlock->ownerExecutable()->newReplacementCodeBlockFor(codeBlock->specializationKind())
    CodeBlock* newReplacement();
    
    void setJITCode(Ref<JITCode>&& code)
    {
        if (!code->isShared())
            heap()->reportExtraMemoryAllocated(code->size());

        ConcurrentJSLocker locker(m_lock);
        WTF::storeStoreFence(); // This is probably not needed because the lock will also do something similar, but it's good to be paranoid.
        m_jitCode = WTFMove(code);
    }

    RefPtr<JITCode> jitCode() { return m_jitCode; }
    static ptrdiff_t jitCodeOffset() { return OBJECT_OFFSETOF(CodeBlock, m_jitCode); }
    JITType jitType() const
    {
        JITCode* jitCode = m_jitCode.get();
        WTF::loadLoadFence();
        JITType result = JITCode::jitTypeFor(jitCode);
        WTF::loadLoadFence(); // This probably isn't needed. Oh well, paranoia is good.
        return result;
    }

    bool hasBaselineJITProfiling() const
    {
        return jitType() == JITType::BaselineJIT;
    }
    
#if ENABLE(JIT)
    CodeBlock* replacement();

    DFG::CapabilityLevel computeCapabilityLevel();
    DFG::CapabilityLevel capabilityLevel();
    DFG::CapabilityLevel capabilityLevelState() { return static_cast<DFG::CapabilityLevel>(m_capabilityLevelState); }

    CodeBlock* optimizedReplacement(JITType typeToReplace);
    CodeBlock* optimizedReplacement(); // the typeToReplace is my JITType
    bool hasOptimizedReplacement(JITType typeToReplace);
    bool hasOptimizedReplacement(); // the typeToReplace is my JITType
#endif

    void jettison(Profiler::JettisonReason, ReoptimizationMode = DontCountReoptimization, const FireDetail* = nullptr);
    
    ScriptExecutable* ownerExecutable() const { return m_ownerExecutable.get(); }
    
    ExecutableToCodeBlockEdge* ownerEdge() const { return m_ownerEdge.get(); }

    VM& vm() const { return *m_vm; }

    VirtualRegister thisRegister() const { return m_unlinkedCode->thisRegister(); }

    bool usesEval() const { return m_unlinkedCode->usesEval(); }

    void setScopeRegister(VirtualRegister scopeRegister)
    {
        ASSERT(scopeRegister.isLocal() || !scopeRegister.isValid());
        m_scopeRegister = scopeRegister;
    }

    VirtualRegister scopeRegister() const
    {
        return m_scopeRegister;
    }
    
    PutPropertySlot::Context putByIdContext() const
    {
        if (codeType() == EvalCode)
            return PutPropertySlot::PutByIdEval;
        return PutPropertySlot::PutById;
    }

    const SourceCode& source() const { return m_ownerExecutable->source(); }
    unsigned sourceOffset() const { return m_ownerExecutable->source().startOffset(); }
    unsigned firstLineColumnOffset() const { return m_ownerExecutable->startColumn(); }

    size_t numberOfJumpTargets() const { return m_unlinkedCode->numberOfJumpTargets(); }
    unsigned jumpTarget(int index) const { return m_unlinkedCode->jumpTarget(index); }

    String nameForRegister(VirtualRegister);

    unsigned numberOfArgumentValueProfiles()
    {
        ASSERT(m_numParameters >= 0);
        ASSERT(m_argumentValueProfiles.size() == static_cast<unsigned>(m_numParameters) || !Options::useJIT());
        return m_argumentValueProfiles.size();
    }

    ValueProfile& valueProfileForArgument(unsigned argumentIndex)
    {
        ASSERT(Options::useJIT()); // This is only called from the various JIT compilers or places that first check numberOfArgumentValueProfiles before calling this.
        ValueProfile& result = m_argumentValueProfiles[argumentIndex];
        return result;
    }

    ValueProfile& valueProfileForBytecodeIndex(BytecodeIndex);
    SpeculatedType valueProfilePredictionForBytecodeIndex(const ConcurrentJSLocker&, BytecodeIndex);

    template<typename Functor> void forEachValueProfile(const Functor&);
    template<typename Functor> void forEachArrayProfile(const Functor&);
    template<typename Functor> void forEachArrayAllocationProfile(const Functor&);
    template<typename Functor> void forEachObjectAllocationProfile(const Functor&);
    template<typename Functor> void forEachLLIntCallLinkInfo(const Functor&);

    BinaryArithProfile* binaryArithProfileForBytecodeIndex(BytecodeIndex);
    UnaryArithProfile* unaryArithProfileForBytecodeIndex(BytecodeIndex);
    BinaryArithProfile* binaryArithProfileForPC(const Instruction*);
    UnaryArithProfile* unaryArithProfileForPC(const Instruction*);

    bool couldTakeSpecialArithFastCase(BytecodeIndex bytecodeOffset);

    ArrayProfile* getArrayProfile(const ConcurrentJSLocker&, BytecodeIndex);
    ArrayProfile* getArrayProfile(BytecodeIndex);

    // Exception handling support

    size_t numberOfExceptionHandlers() const { return m_rareData ? m_rareData->m_exceptionHandlers.size() : 0; }
    HandlerInfo& exceptionHandler(int index) { RELEASE_ASSERT(m_rareData); return m_rareData->m_exceptionHandlers[index]; }

    bool hasExpressionInfo() { return m_unlinkedCode->hasExpressionInfo(); }

#if ENABLE(DFG_JIT)
    DFG::CodeOriginPool& codeOrigins();
    
    // Having code origins implies that there has been some inlining.
    bool hasCodeOrigins()
    {
        return JITCode::isOptimizingJIT(jitType());
    }
        
    bool canGetCodeOrigin(CallSiteIndex index)
    {
        if (!hasCodeOrigins())
            return false;
        return index.bits() < codeOrigins().size();
    }

    CodeOrigin codeOrigin(CallSiteIndex index)
    {
        return codeOrigins().get(index.bits());
    }

    CompressedLazyOperandValueProfileHolder& lazyOperandValueProfiles(const ConcurrentJSLocker&)
    {
        return m_lazyOperandValueProfiles;
    }
#endif // ENABLE(DFG_JIT)

    // Constant Pool
#if ENABLE(DFG_JIT)
    size_t numberOfIdentifiers() const { return m_unlinkedCode->numberOfIdentifiers() + numberOfDFGIdentifiers(); }
    size_t numberOfDFGIdentifiers() const;
    const Identifier& identifier(int index) const;
#else
    size_t numberOfIdentifiers() const { return m_unlinkedCode->numberOfIdentifiers(); }
    const Identifier& identifier(int index) const { return m_unlinkedCode->identifier(index); }
#endif

    Vector<WriteBarrier<Unknown>>& constants() { return m_constantRegisters; }
    Vector<SourceCodeRepresentation>& constantsSourceCodeRepresentation() { return m_constantsSourceCodeRepresentation; }
    unsigned addConstant(const ConcurrentJSLocker&, JSValue v)
    {
        unsigned result = m_constantRegisters.size();
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantRegisters.last().set(*m_vm, this, v);
        m_constantsSourceCodeRepresentation.append(SourceCodeRepresentation::Other);
        return result;
    }

    unsigned addConstantLazily(const ConcurrentJSLocker&)
    {
        unsigned result = m_constantRegisters.size();
        m_constantRegisters.append(WriteBarrier<Unknown>());
        m_constantsSourceCodeRepresentation.append(SourceCodeRepresentation::Other);
        return result;
    }

    const Vector<WriteBarrier<Unknown>>& constantRegisters() { return m_constantRegisters; }
    WriteBarrier<Unknown>& constantRegister(VirtualRegister reg) { return m_constantRegisters[reg.toConstantIndex()]; }
    ALWAYS_INLINE JSValue getConstant(VirtualRegister reg) const { return m_constantRegisters[reg.toConstantIndex()].get(); }
    ALWAYS_INLINE SourceCodeRepresentation constantSourceCodeRepresentation(VirtualRegister reg) const { return m_constantsSourceCodeRepresentation[reg.toConstantIndex()]; }

    FunctionExecutable* functionDecl(int index) { return m_functionDecls[index].get(); }
    int numberOfFunctionDecls() { return m_functionDecls.size(); }
    FunctionExecutable* functionExpr(int index) { return m_functionExprs[index].get(); }
    
    const BitVector& bitVector(size_t i) { return m_unlinkedCode->bitVector(i); }

    Heap* heap() const { return &m_vm->heap; }
    JSGlobalObject* globalObject() { return m_globalObject.get(); }

    JSGlobalObject* globalObjectFor(CodeOrigin);

    BytecodeLivenessAnalysis& livenessAnalysis()
    {
        return m_unlinkedCode->livenessAnalysis(this);
    }
    
    void validate();

    // Jump Tables

    size_t numberOfSwitchJumpTables() const { return m_rareData ? m_rareData->m_switchJumpTables.size() : 0; }
    SimpleJumpTable& switchJumpTable(int tableIndex) { RELEASE_ASSERT(m_rareData); return m_rareData->m_switchJumpTables[tableIndex]; }
    void clearSwitchJumpTables()
    {
        if (!m_rareData)
            return;
        m_rareData->m_switchJumpTables.clear();
    }
#if ENABLE(DFG_JIT)
    void addSwitchJumpTableFromProfiledCodeBlock(SimpleJumpTable& profiled)
    {
        createRareDataIfNecessary();
        m_rareData->m_switchJumpTables.append(profiled.cloneNonJITPart());
    }
#endif

    size_t numberOfStringSwitchJumpTables() const { return m_rareData ? m_rareData->m_stringSwitchJumpTables.size() : 0; }
    StringJumpTable& stringSwitchJumpTable(int tableIndex) { RELEASE_ASSERT(m_rareData); return m_rareData->m_stringSwitchJumpTables[tableIndex]; }

    DirectEvalCodeCache& directEvalCodeCache() { createRareDataIfNecessary(); return m_rareData->m_directEvalCodeCache; }

    enum class ShrinkMode {
        // Shrink prior to generating machine code that may point directly into vectors.
        EarlyShrink,

        // Shrink after generating machine code, and after possibly creating new vectors
        // and appending to others. At this time it is not safe to shrink certain vectors
        // because we would have generated machine code that references them directly.
        LateShrink,
    };
    void shrinkToFit(const ConcurrentJSLocker&, ShrinkMode);

    // Functions for controlling when JITting kicks in, in a mixed mode
    // execution world.

    bool checkIfJITThresholdReached()
    {
        return m_llintExecuteCounter.checkIfThresholdCrossedAndSet(this);
    }

    void dontJITAnytimeSoon()
    {
        m_llintExecuteCounter.deferIndefinitely();
    }

    int32_t thresholdForJIT(int32_t threshold);
    void jitAfterWarmUp();
    void jitSoon();

    const BaselineExecutionCounter& llintExecuteCounter() const
    {
        return m_llintExecuteCounter;
    }

    typedef HashMap<std::tuple<StructureID, unsigned>, Vector<LLIntPrototypeLoadAdaptiveStructureWatchpoint>> StructureWatchpointMap;
    StructureWatchpointMap& llintGetByIdWatchpointMap() { return m_llintGetByIdWatchpointMap; }

    // Functions for controlling when tiered compilation kicks in. This
    // controls both when the optimizing compiler is invoked and when OSR
    // entry happens. Two triggers exist: the loop trigger and the return
    // trigger. In either case, when an addition to m_jitExecuteCounter
    // causes it to become non-negative, the optimizing compiler is
    // invoked. This includes a fast check to see if this CodeBlock has
    // already been optimized (i.e. replacement() returns a CodeBlock
    // that was optimized with a higher tier JIT than this one). In the
    // case of the loop trigger, if the optimized compilation succeeds
    // (or has already succeeded in the past) then OSR is attempted to
    // redirect program flow into the optimized code.

    // These functions are called from within the optimization triggers,
    // and are used as a single point at which we define the heuristics
    // for how much warm-up is mandated before the next optimization
    // trigger files. All CodeBlocks start out with optimizeAfterWarmUp(),
    // as this is called from the CodeBlock constructor.

    // When we observe a lot of speculation failures, we trigger a
    // reoptimization. But each time, we increase the optimization trigger
    // to avoid thrashing.
    JS_EXPORT_PRIVATE unsigned reoptimizationRetryCounter() const;
    void countReoptimization();

#if !ENABLE(C_LOOP)
    const RegisterAtOffsetList* calleeSaveRegisters() const;

    static unsigned numberOfLLIntBaselineCalleeSaveRegisters() { return RegisterSet::llintBaselineCalleeSaveRegisters().numberOfSetRegisters(); }
    static size_t llintBaselineCalleeSaveSpaceAsVirtualRegisters();
    size_t calleeSaveSpaceAsVirtualRegisters();
#else
    static unsigned numberOfLLIntBaselineCalleeSaveRegisters() { return 0; }
    static size_t llintBaselineCalleeSaveSpaceAsVirtualRegisters() { return 1; };
    size_t calleeSaveSpaceAsVirtualRegisters() { return 0; }
#endif

#if ENABLE(JIT)
    unsigned numberOfDFGCompiles();

    int32_t codeTypeThresholdMultiplier() const;

    int32_t adjustedCounterValue(int32_t desiredThreshold);

    int32_t* addressOfJITExecuteCounter()
    {
        return &m_jitExecuteCounter.m_counter;
    }

    static ptrdiff_t offsetOfJITExecuteCounter() { return OBJECT_OFFSETOF(CodeBlock, m_jitExecuteCounter) + OBJECT_OFFSETOF(BaselineExecutionCounter, m_counter); }
    static ptrdiff_t offsetOfJITExecutionActiveThreshold() { return OBJECT_OFFSETOF(CodeBlock, m_jitExecuteCounter) + OBJECT_OFFSETOF(BaselineExecutionCounter, m_activeThreshold); }
    static ptrdiff_t offsetOfJITExecutionTotalCount() { return OBJECT_OFFSETOF(CodeBlock, m_jitExecuteCounter) + OBJECT_OFFSETOF(BaselineExecutionCounter, m_totalCount); }

    const BaselineExecutionCounter& jitExecuteCounter() const { return m_jitExecuteCounter; }

    unsigned optimizationDelayCounter() const { return m_optimizationDelayCounter; }

    // Check if the optimization threshold has been reached, and if not,
    // adjust the heuristics accordingly. Returns true if the threshold has
    // been reached.
    bool checkIfOptimizationThresholdReached();

    // Call this to force the next optimization trigger to fire. This is
    // rarely wise, since optimization triggers are typically more
    // expensive than executing baseline code.
    void optimizeNextInvocation();

    // Call this to prevent optimization from happening again. Note that
    // optimization will still happen after roughly 2^29 invocations,
    // so this is really meant to delay that as much as possible. This
    // is called if optimization failed, and we expect it to fail in
    // the future as well.
    void dontOptimizeAnytimeSoon();

    // Call this to reinitialize the counter to its starting state,
    // forcing a warm-up to happen before the next optimization trigger
    // fires. This is called in the CodeBlock constructor. It also
    // makes sense to call this if an OSR exit occurred. Note that
    // OSR exit code is code generated, so the value of the execute
    // counter that this corresponds to is also available directly.
    void optimizeAfterWarmUp();

    // Call this to force an optimization trigger to fire only after
    // a lot of warm-up.
    void optimizeAfterLongWarmUp();

    // Call this to cause an optimization trigger to fire soon, but
    // not necessarily the next one. This makes sense if optimization
    // succeeds. Successful optimization means that all calls are
    // relinked to the optimized code, so this only affects call
    // frames that are still executing this CodeBlock. The value here
    // is tuned to strike a balance between the cost of OSR entry
    // (which is too high to warrant making every loop back edge to
    // trigger OSR immediately) and the cost of executing baseline
    // code (which is high enough that we don't necessarily want to
    // have a full warm-up). The intuition for calling this instead of
    // optimizeNextInvocation() is for the case of recursive functions
    // with loops. Consider that there may be N call frames of some
    // recursive function, for a reasonably large value of N. The top
    // one triggers optimization, and then returns, and then all of
    // the others return. We don't want optimization to be triggered on
    // each return, as that would be superfluous. It only makes sense
    // to trigger optimization if one of those functions becomes hot
    // in the baseline code.
    void optimizeSoon();

    void forceOptimizationSlowPathConcurrently();

    void setOptimizationThresholdBasedOnCompilationResult(CompilationResult);
    
    BytecodeIndex bytecodeIndexForExit(BytecodeIndex) const;
    uint32_t osrExitCounter() const { return m_osrExitCounter; }

    void countOSRExit() { m_osrExitCounter++; }

    enum class OptimizeAction { None, ReoptimizeNow };
#if ENABLE(DFG_JIT)
    OptimizeAction updateOSRExitCounterAndCheckIfNeedToReoptimize(DFG::OSRExitState&);
#endif

    static ptrdiff_t offsetOfOSRExitCounter() { return OBJECT_OFFSETOF(CodeBlock, m_osrExitCounter); }

    uint32_t adjustedExitCountThreshold(uint32_t desiredThreshold);
    uint32_t exitCountThresholdForReoptimization();
    uint32_t exitCountThresholdForReoptimizationFromLoop();
    bool shouldReoptimizeNow();
    bool shouldReoptimizeFromLoopNow();

#else // No JIT
    void optimizeAfterWarmUp() { }
    unsigned numberOfDFGCompiles() { return 0; }
#endif

    bool shouldOptimizeNow();
    void updateAllValueProfilePredictions();
    void updateAllArrayPredictions();
    void updateAllPredictions();

    unsigned frameRegisterCount();
    int stackPointerOffset();

    bool hasOpDebugForLineAndColumn(unsigned line, Optional<unsigned> column);

    bool hasDebuggerRequests() const { return m_debuggerRequests; }
    void* debuggerRequestsAddress() { return &m_debuggerRequests; }

    void addBreakpoint(unsigned numBreakpoints);
    void removeBreakpoint(unsigned numBreakpoints)
    {
        ASSERT(m_numBreakpoints >= numBreakpoints);
        m_numBreakpoints -= numBreakpoints;
    }

    enum SteppingMode {
        SteppingModeDisabled,
        SteppingModeEnabled
    };
    void setSteppingMode(SteppingMode);

    void clearDebuggerRequests()
    {
        m_steppingMode = SteppingModeDisabled;
        m_numBreakpoints = 0;
    }

    bool wasCompiledWithDebuggingOpcodes() const { return m_unlinkedCode->wasCompiledWithDebuggingOpcodes(); }
    
    // This is intentionally public; it's the responsibility of anyone doing any
    // of the following to hold the lock:
    //
    // - Modifying any inline cache in this code block.
    //
    // - Quering any inline cache in this code block, from a thread other than
    //   the main thread.
    //
    // Additionally, it's only legal to modify the inline cache on the main
    // thread. This means that the main thread can query the inline cache without
    // locking. This is crucial since executing the inline cache is effectively
    // "querying" it.
    //
    // Another exception to the rules is that the GC can do whatever it wants
    // without holding any locks, because the GC is guaranteed to wait until any
    // concurrent compilation threads finish what they're doing.
    mutable ConcurrentJSLock m_lock;

    bool m_shouldAlwaysBeInlined; // Not a bitfield because the JIT wants to store to it.

#if ENABLE(JIT)
    unsigned m_capabilityLevelState : 2; // DFG::CapabilityLevel
#endif

    bool m_allTransitionsHaveBeenMarked : 1; // Initialized and used on every GC.

    bool m_didFailJITCompilation : 1;
    bool m_didFailFTLCompilation : 1;
    bool m_hasBeenCompiledWithFTL : 1;

    bool m_hasLinkedOSRExit : 1;
    bool m_isEligibleForLLIntDowngrade : 1;

    // Internal methods for use by validation code. It would be private if it wasn't
    // for the fact that we use it from anonymous namespaces.
    void beginValidationDidFail();
    NO_RETURN_DUE_TO_CRASH void endValidationDidFail();

    struct RareData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CodeBlockRareData);
    public:
        Vector<HandlerInfo> m_exceptionHandlers;

        // Jump Tables
        Vector<SimpleJumpTable> m_switchJumpTables;
        Vector<StringJumpTable> m_stringSwitchJumpTables;

        Vector<std::unique_ptr<ValueProfileAndVirtualRegisterBuffer>> m_catchProfiles;

        DirectEvalCodeCache m_directEvalCodeCache;
    };

    void clearExceptionHandlers()
    {
        if (m_rareData)
            m_rareData->m_exceptionHandlers.clear();
    }

    void appendExceptionHandler(const HandlerInfo& handler)
    {
        createRareDataIfNecessary(); // We may be handling the exception of an inlined call frame.
        m_rareData->m_exceptionHandlers.append(handler);
    }

    DisposableCallSiteIndex newExceptionHandlingCallSiteIndex(CallSiteIndex originalCallSite);

    void ensureCatchLivenessIsComputedForBytecodeIndex(BytecodeIndex);

    bool hasTailCalls() const { return m_unlinkedCode->hasTailCalls(); }

    template<typename Metadata>
    Metadata& metadata(OpcodeID opcodeID, unsigned metadataID)
    {
        ASSERT(m_metadata);
        return bitwise_cast<Metadata*>(m_metadata->get(opcodeID))[metadataID];
    }

    size_t metadataSizeInBytes()
    {
        return m_unlinkedCode->metadataSizeInBytes();
    }

    MetadataTable* metadataTable() { return m_metadata.get(); }
    const void* instructionsRawPointer() { return m_instructionsRawPointer; }

    bool loopHintsAreEligibleForFuzzingEarlyReturn()
    {
        // Some builtins are required to always complete the loops they run.
        return !m_unlinkedCode->isBuiltinFunction();
    }

protected:
    void finalizeLLIntInlineCaches();
#if ENABLE(JIT)
    void finalizeBaselineJITInlineCaches();
#endif
#if ENABLE(DFG_JIT)
    void tallyFrequentExitSites();
#else
    void tallyFrequentExitSites() { }
#endif

private:
    friend class CodeBlockSet;
    friend class ExecutableToCodeBlockEdge;

    BytecodeLivenessAnalysis& livenessAnalysisSlow();
    
    CodeBlock* specialOSREntryBlockOrNull();
    
    void noticeIncomingCall(CallFrame* callerFrame);
    
    double optimizationThresholdScalingFactor();

    void updateAllValueProfilePredictionsAndCountLiveness(unsigned& numberOfLiveNonArgumentValueProfiles, unsigned& numberOfSamplesInProfiles);

    void setConstantIdentifierSetRegisters(VM&, const RefCountedArray<ConstantIdentifierSetEntry>& constants);

    void setConstantRegisters(const RefCountedArray<WriteBarrier<Unknown>>& constants, const RefCountedArray<SourceCodeRepresentation>& constantsSourceCodeRepresentation, ScriptExecutable* topLevelExecutable);

    void replaceConstant(VirtualRegister reg, JSValue value)
    {
        ASSERT(reg.isConstant() && static_cast<size_t>(reg.toConstantIndex()) < m_constantRegisters.size());
        m_constantRegisters[reg.toConstantIndex()].set(*m_vm, this, value);
    }

    bool shouldVisitStrongly(const ConcurrentJSLocker&);
    bool shouldJettisonDueToWeakReference(VM&);
    bool shouldJettisonDueToOldAge(const ConcurrentJSLocker&);
    
    void propagateTransitions(const ConcurrentJSLocker&, SlotVisitor&);
    void determineLiveness(const ConcurrentJSLocker&, SlotVisitor&);
        
    void stronglyVisitStrongReferences(const ConcurrentJSLocker&, SlotVisitor&);
    void stronglyVisitWeakReferences(const ConcurrentJSLocker&, SlotVisitor&);
    void visitOSRExitTargets(const ConcurrentJSLocker&, SlotVisitor&);

    unsigned numberOfNonArgumentValueProfiles() { return m_numberOfNonArgumentValueProfiles; }
    unsigned totalNumberOfValueProfiles() { return numberOfArgumentValueProfiles() + numberOfNonArgumentValueProfiles(); }
    ValueProfile* tryGetValueProfileForBytecodeIndex(BytecodeIndex);

    Seconds timeSinceCreation()
    {
        return MonotonicTime::now() - m_creationTime;
    }

    void createRareDataIfNecessary()
    {
        if (!m_rareData) {
            auto rareData = makeUnique<RareData>();
            WTF::storeStoreFence(); // m_catchProfiles can be touched from compiler threads.
            m_rareData = WTFMove(rareData);
        }
    }

    void insertBasicBlockBoundariesForControlFlowProfiler();
    void ensureCatchLivenessIsComputedForBytecodeIndexSlow(const OpCatch&, BytecodeIndex);

    int m_numCalleeLocals;
    int m_numVars;
    int m_numParameters;
    int m_numberOfArgumentsToSkip { 0 };
    unsigned m_numberOfNonArgumentValueProfiles { 0 };
    union {
        unsigned m_debuggerRequests;
        struct {
            unsigned m_hasDebuggerStatement : 1;
            unsigned m_steppingMode : 1;
            unsigned m_numBreakpoints : 30;
        };
    };
    unsigned m_bytecodeCost { 0 };
    VirtualRegister m_scopeRegister;
    mutable CodeBlockHash m_hash;

    WriteBarrier<UnlinkedCodeBlock> m_unlinkedCode;
    WriteBarrier<ScriptExecutable> m_ownerExecutable;
    WriteBarrier<ExecutableToCodeBlockEdge> m_ownerEdge;
    // m_vm must be a pointer (instead of a reference) because the JSCLLIntOffsetsExtractor
    // cannot handle it being a reference.
    VM* m_vm;

    const void* m_instructionsRawPointer { nullptr };
    SentinelLinkedList<LLIntCallLinkInfo, PackedRawSentinelNode<LLIntCallLinkInfo>> m_incomingLLIntCalls;
    StructureWatchpointMap m_llintGetByIdWatchpointMap;
    RefPtr<JITCode> m_jitCode;
#if ENABLE(JIT)
    std::unique_ptr<JITData> m_jitData;
#endif
#if ENABLE(DFG_JIT)
    // This is relevant to non-DFG code blocks that serve as the profiled code block
    // for DFG code blocks.
    CompressedLazyOperandValueProfileHolder m_lazyOperandValueProfiles;
#endif
    RefCountedArray<ValueProfile> m_argumentValueProfiles;

    // Constant Pool
    COMPILE_ASSERT(sizeof(Register) == sizeof(WriteBarrier<Unknown>), Register_must_be_same_size_as_WriteBarrier_Unknown);
    // TODO: This could just be a pointer to m_unlinkedCodeBlock's data, but the DFG mutates
    // it, so we're stuck with it for now.
    Vector<WriteBarrier<Unknown>> m_constantRegisters;
    Vector<SourceCodeRepresentation> m_constantsSourceCodeRepresentation;
    RefCountedArray<WriteBarrier<FunctionExecutable>> m_functionDecls;
    RefCountedArray<WriteBarrier<FunctionExecutable>> m_functionExprs;

    WriteBarrier<CodeBlock> m_alternative;
    
    BaselineExecutionCounter m_llintExecuteCounter;

    BaselineExecutionCounter m_jitExecuteCounter;
    uint32_t m_osrExitCounter;

    uint16_t m_optimizationDelayCounter;
    uint16_t m_reoptimizationRetryCounter;

    RefPtr<MetadataTable> m_metadata;

    MonotonicTime m_creationTime;
    double m_previousCounter { 0 };

    std::unique_ptr<RareData> m_rareData;
};

template <typename ExecutableType>
Exception* ScriptExecutable::prepareForExecution(VM& vm, JSFunction* function, JSScope* scope, CodeSpecializationKind kind, CodeBlock*& resultCodeBlock)
{
    if (hasJITCodeFor(kind)) {
        if constexpr (std::is_same<ExecutableType, EvalExecutable>::value) {
            resultCodeBlock = jsCast<CodeBlock*>(jsCast<ExecutableType*>(this)->codeBlock());
            return nullptr;
        }
        if constexpr (std::is_same<ExecutableType, ProgramExecutable>::value) {
            resultCodeBlock = jsCast<CodeBlock*>(jsCast<ExecutableType*>(this)->codeBlock());
            return nullptr;
        }
        if constexpr (std::is_same<ExecutableType, ModuleProgramExecutable>::value) {
            resultCodeBlock = jsCast<CodeBlock*>(jsCast<ExecutableType*>(this)->codeBlock());
            return nullptr;
        }
        if constexpr (std::is_same<ExecutableType, FunctionExecutable>::value) {
            resultCodeBlock = jsCast<CodeBlock*>(jsCast<ExecutableType*>(this)->codeBlockFor(kind));
            return nullptr;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
    return prepareForExecutionImpl(vm, function, scope, kind, resultCodeBlock);
}

#define CODEBLOCK_LOG_EVENT(codeBlock, summary, details) \
    do { \
        if (codeBlock) \
            (codeBlock->vm().logEvent(codeBlock, summary, [&] () { return toCString details; })); \
    } while (0)


void setPrinter(Printer::PrintRecord&, CodeBlock*);

} // namespace JSC

namespace WTF {
    
JS_EXPORT_PRIVATE void printInternal(PrintStream&, JSC::CodeBlock*);

} // namespace WTF
