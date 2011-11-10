/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef CodeBlock_h
#define CodeBlock_h

#include "CodeOrigin.h"
#include "CompactJITCodeMap.h"
#include "DFGOSREntry.h"
#include "DFGOSRExit.h"
#include "EvalCodeCache.h"
#include "Heuristics.h"
#include "Instruction.h"
#include "JITCode.h"
#include "JITWriteBarrier.h"
#include "JSGlobalObject.h"
#include "JumpTable.h"
#include "Nodes.h"
#include "PredictionTracker.h"
#include "RegExpObject.h"
#include "UString.h"
#include "ValueProfile.h"
#include <wtf/FastAllocBase.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/SegmentedVector.h>
#include <wtf/SentinelLinkedList.h>
#include <wtf/Vector.h>

#if ENABLE(JIT)
#include "StructureStubInfo.h"
#endif

// Register numbers used in bytecode operations have different meaning according to their ranges:
//      0x80000000-0xFFFFFFFF  Negative indices from the CallFrame pointer are entries in the call frame, see RegisterFile.h.
//      0x00000000-0x3FFFFFFF  Forwards indices from the CallFrame pointer are local vars and temporaries with the function's callframe.
//      0x40000000-0x7FFFFFFF  Positive indices from 0x40000000 specify entries in the constant pool on the CodeBlock.
static const int FirstConstantRegisterIndex = 0x40000000;

namespace JSC {

    enum HasSeenShouldRepatch {
        hasSeenShouldRepatch
    };

    class ExecState;

    enum CodeType { GlobalCode, EvalCode, FunctionCode };

    inline int unmodifiedArgumentsRegister(int argumentsRegister) { return argumentsRegister - 1; }

    static ALWAYS_INLINE int missingThisObjectMarker() { return std::numeric_limits<int>::max(); }

    struct HandlerInfo {
        uint32_t start;
        uint32_t end;
        uint32_t target;
        uint32_t scopeDepth;
#if ENABLE(JIT)
        CodeLocationLabel nativeCode;
#endif
    };

    struct ExpressionRangeInfo {
        enum {
            MaxOffset = (1 << 7) - 1, 
            MaxDivot = (1 << 25) - 1
        };
        uint32_t instructionOffset : 25;
        uint32_t divotPoint : 25;
        uint32_t startOffset : 7;
        uint32_t endOffset : 7;
    };

    struct LineInfo {
        uint32_t instructionOffset;
        int32_t lineNumber;
    };

#if ENABLE(JIT)
    struct CallLinkInfo : public BasicRawSentinelNode<CallLinkInfo> {
        CallLinkInfo()
            : hasSeenShouldRepatch(false)
            , isCall(false)
            , isDFG(false)
        {
        }
        
        ~CallLinkInfo()
        {
            if (isOnList())
                remove();
        }

        CodeLocationLabel callReturnLocation; // it's a near call in the old JIT, or a normal call in DFG
        CodeLocationDataLabelPtr hotPathBegin;
        CodeLocationNearCall hotPathOther;
        JITWriteBarrier<JSFunction> callee;
        WriteBarrier<JSFunction> lastSeenCallee;
        bool hasSeenShouldRepatch : 1;
        bool isCall : 1;
        bool isDFG : 1;
        unsigned bytecodeIndex;

        bool isLinked() { return callee; }
        void unlink(JSGlobalData&, RepatchBuffer&);

        bool seenOnce()
        {
            return hasSeenShouldRepatch;
        }

        void setSeen()
        {
            hasSeenShouldRepatch = true;
        }
    };

    struct MethodCallLinkInfo {
        MethodCallLinkInfo()
            : seen(false)
        {
        }

        bool seenOnce()
        {
            return seen;
        }

        void setSeen()
        {
            seen = true;
        }

        unsigned bytecodeIndex;
        CodeLocationCall callReturnLocation;
        JITWriteBarrier<Structure> cachedStructure;
        JITWriteBarrier<Structure> cachedPrototypeStructure;
        // We'd like this to actually be JSFunction, but InternalFunction and JSFunction
        // don't have a common parent class and we allow specialisation on both
        JITWriteBarrier<JSObject> cachedFunction;
        JITWriteBarrier<JSObject> cachedPrototype;
        bool seen;
    };

    struct GlobalResolveInfo {
        GlobalResolveInfo(unsigned bytecodeOffset)
            : offset(0)
            , bytecodeOffset(bytecodeOffset)
        {
        }

        WriteBarrier<Structure> structure;
        unsigned offset;
        unsigned bytecodeOffset;
    };

    // This structure is used to map from a call return location
    // (given as an offset in bytes into the JIT code) back to
    // the bytecode index of the corresponding bytecode operation.
    // This is then used to look up the corresponding handler.
    // FIXME: This should be made inlining aware! Currently it isn't
    // because we never inline code that has exception handlers.
    struct CallReturnOffsetToBytecodeOffset {
        CallReturnOffsetToBytecodeOffset(unsigned callReturnOffset, unsigned bytecodeOffset)
            : callReturnOffset(callReturnOffset)
            , bytecodeOffset(bytecodeOffset)
        {
        }

        unsigned callReturnOffset;
        unsigned bytecodeOffset;
    };

    // valueAtPosition helpers for the binarySearch algorithm.

    inline void* getStructureStubInfoReturnLocation(StructureStubInfo* structureStubInfo)
    {
        return structureStubInfo->callReturnLocation.executableAddress();
    }

    inline unsigned getStructureStubInfoBytecodeIndex(StructureStubInfo* structureStubInfo)
    {
        return structureStubInfo->bytecodeIndex;
    }

    inline void* getCallLinkInfoReturnLocation(CallLinkInfo* callLinkInfo)
    {
        return callLinkInfo->callReturnLocation.executableAddress();
    }

    inline unsigned getCallLinkInfoBytecodeIndex(CallLinkInfo* callLinkInfo)
    {
        return callLinkInfo->bytecodeIndex;
    }

    inline void* getMethodCallLinkInfoReturnLocation(MethodCallLinkInfo* methodCallLinkInfo)
    {
        return methodCallLinkInfo->callReturnLocation.executableAddress();
    }

    inline unsigned getMethodCallLinkInfoBytecodeIndex(MethodCallLinkInfo* methodCallLinkInfo)
    {
        return methodCallLinkInfo->bytecodeIndex;
    }

    inline unsigned getCallReturnOffset(CallReturnOffsetToBytecodeOffset* pc)
    {
        return pc->callReturnOffset;
    }
#endif

    class CodeBlock {
        WTF_MAKE_FAST_ALLOCATED;
        friend class JIT;
    protected:
        CodeBlock(ScriptExecutable* ownerExecutable, CodeType, JSGlobalObject*, PassRefPtr<SourceProvider>, unsigned sourceOffset, SymbolTable*, bool isConstructor, PassOwnPtr<CodeBlock> alternative);

        WriteBarrier<JSGlobalObject> m_globalObject;
        Heap* m_heap;

    public:
        virtual ~CodeBlock();

        CodeBlock* alternative() { return m_alternative.get(); }
        PassOwnPtr<CodeBlock> releaseAlternative() { return m_alternative.release(); }
        void setAlternative(PassOwnPtr<CodeBlock> alternative) { m_alternative = alternative; }

#if ENABLE(JIT)
        CodeBlock* baselineVersion()
        {
            CodeBlock* result = replacement();
            if (!result)
                return 0; // This can happen if we're in the process of creating the baseline version.
            while (result->alternative())
                result = result->alternative();
            ASSERT(result);
            ASSERT(result->getJITType() == JITCode::BaselineJIT);
            return result;
        }
#endif

        void visitAggregate(SlotVisitor&);

        static void dumpStatistics();

#if !defined(NDEBUG) || ENABLE_OPCODE_SAMPLING
        void dump(ExecState*) const;
        void printStructures(const Instruction*) const;
        void printStructure(const char* name, const Instruction*, int operand) const;
#endif

        bool isStrictMode() const { return m_isStrictMode; }

        inline bool isKnownNotImmediate(int index)
        {
            if (index == m_thisRegister && !m_isStrictMode)
                return true;

            if (isConstantRegisterIndex(index))
                return getConstant(index).isCell();

            return false;
        }

        ALWAYS_INLINE bool isTemporaryRegisterIndex(int index)
        {
            return index >= m_numVars;
        }

        HandlerInfo* handlerForBytecodeOffset(unsigned bytecodeOffset);
        int lineNumberForBytecodeOffset(unsigned bytecodeOffset);
        void expressionRangeForBytecodeOffset(unsigned bytecodeOffset, int& divot, int& startOffset, int& endOffset);

#if ENABLE(JIT)

        StructureStubInfo& getStubInfo(ReturnAddressPtr returnAddress)
        {
            return *(binarySearch<StructureStubInfo, void*, getStructureStubInfoReturnLocation>(m_structureStubInfos.begin(), m_structureStubInfos.size(), returnAddress.value()));
        }

        StructureStubInfo& getStubInfo(unsigned bytecodeIndex)
        {
            return *(binarySearch<StructureStubInfo, unsigned, getStructureStubInfoBytecodeIndex>(m_structureStubInfos.begin(), m_structureStubInfos.size(), bytecodeIndex));
        }

        CallLinkInfo& getCallLinkInfo(ReturnAddressPtr returnAddress)
        {
            return *(binarySearch<CallLinkInfo, void*, getCallLinkInfoReturnLocation>(m_callLinkInfos.begin(), m_callLinkInfos.size(), returnAddress.value()));
        }
        
        CallLinkInfo& getCallLinkInfo(unsigned bytecodeIndex)
        {
            return *(binarySearch<CallLinkInfo, unsigned, getCallLinkInfoBytecodeIndex>(m_callLinkInfos.begin(), m_callLinkInfos.size(), bytecodeIndex));
        }

        MethodCallLinkInfo& getMethodCallLinkInfo(ReturnAddressPtr returnAddress)
        {
            return *(binarySearch<MethodCallLinkInfo, void*, getMethodCallLinkInfoReturnLocation>(m_methodCallLinkInfos.begin(), m_methodCallLinkInfos.size(), returnAddress.value()));
        }

        MethodCallLinkInfo& getMethodCallLinkInfo(unsigned bytecodeIndex)
        {
            return *(binarySearch<MethodCallLinkInfo, unsigned, getMethodCallLinkInfoBytecodeIndex>(m_methodCallLinkInfos.begin(), m_methodCallLinkInfos.size(), bytecodeIndex));
        }

        unsigned bytecodeOffset(ReturnAddressPtr returnAddress)
        {
            if (!m_rareData)
                return 1;
            Vector<CallReturnOffsetToBytecodeOffset>& callIndices = m_rareData->m_callReturnIndexVector;
            if (!callIndices.size())
                return 1;
            return binarySearch<CallReturnOffsetToBytecodeOffset, unsigned, getCallReturnOffset>(callIndices.begin(), callIndices.size(), getJITCode().offsetOf(returnAddress.value()))->bytecodeOffset;
        }

        void unlinkCalls();
        
        bool hasIncomingCalls() { return m_incomingCalls.begin() != m_incomingCalls.end(); }
        
        void linkIncomingCall(CallLinkInfo* incoming)
        {
            m_incomingCalls.push(incoming);
        }
        
        void unlinkIncomingCalls();
#endif

#if ENABLE(DFG_JIT)
        void setJITCodeMap(PassOwnPtr<CompactJITCodeMap> jitCodeMap)
        {
            m_jitCodeMap = jitCodeMap;
        }
        CompactJITCodeMap* jitCodeMap()
        {
            return m_jitCodeMap.get();
        }
        
        void createDFGDataIfNecessary()
        {
            if (!!m_dfgData)
                return;
            
            m_dfgData = adoptPtr(new DFGData);
        }
        
        DFG::OSREntryData* appendDFGOSREntryData(unsigned bytecodeIndex, unsigned machineCodeOffset)
        {
            createDFGDataIfNecessary();
            DFG::OSREntryData entry;
            entry.m_bytecodeIndex = bytecodeIndex;
            entry.m_machineCodeOffset = machineCodeOffset;
            m_dfgData->osrEntry.append(entry);
            return &m_dfgData->osrEntry.last();
        }
        unsigned numberOfDFGOSREntries() const
        {
            if (!m_dfgData)
                return 0;
            return m_dfgData->osrEntry.size();
        }
        DFG::OSREntryData* dfgOSREntryData(unsigned i) { return &m_dfgData->osrEntry[i]; }
        DFG::OSREntryData* dfgOSREntryDataForBytecodeIndex(unsigned bytecodeIndex)
        {
            return binarySearch<DFG::OSREntryData, unsigned, DFG::getOSREntryDataBytecodeIndex>(m_dfgData->osrEntry.begin(), m_dfgData->osrEntry.size(), bytecodeIndex);
        }
        
        void appendOSRExit(const DFG::OSRExit& osrExit)
        {
            createDFGDataIfNecessary();
            m_dfgData->osrExit.append(osrExit);
        }
        
        void appendSpeculationRecovery(const DFG::SpeculationRecovery& recovery)
        {
            createDFGDataIfNecessary();
            m_dfgData->speculationRecovery.append(recovery);
        }
        
        unsigned numberOfOSRExits()
        {
            if (!m_dfgData)
                return 0;
            return m_dfgData->osrExit.size();
        }
        
        unsigned numberOfSpeculationRecoveries()
        {
            if (!m_dfgData)
                return 0;
            return m_dfgData->speculationRecovery.size();
        }
        
        DFG::OSRExit& osrExit(unsigned index)
        {
            return m_dfgData->osrExit[index];
        }
        
        DFG::SpeculationRecovery& speculationRecovery(unsigned index)
        {
            return m_dfgData->speculationRecovery[index];
        }
#endif

#if ENABLE(INTERPRETER)
        unsigned bytecodeOffset(Instruction* returnAddress)
        {
            return static_cast<Instruction*>(returnAddress) - instructions().begin();
        }
#endif

        void setIsNumericCompareFunction(bool isNumericCompareFunction) { m_isNumericCompareFunction = isNumericCompareFunction; }
        bool isNumericCompareFunction() { return m_isNumericCompareFunction; }

        bool hasInstructions() const { return !!m_instructions; }
        unsigned numberOfInstructions() const { return !m_instructions ? 0 : m_instructions->m_instructions.size(); }
        Vector<Instruction>& instructions() { return m_instructions->m_instructions; }
        const Vector<Instruction>& instructions() const { return m_instructions->m_instructions; }
        void discardBytecode() { m_instructions.clear(); }

#ifndef NDEBUG
        bool usesOpcode(OpcodeID);
#endif

        unsigned instructionCount() { return m_instructionCount; }
        void setInstructionCount(unsigned instructionCount) { m_instructionCount = instructionCount; }

#if ENABLE(JIT)
        void setJITCode(const JITCode& code, MacroAssemblerCodePtr codeWithArityCheck)
        {
            m_jitCode = code;
            m_jitCodeWithArityCheck = codeWithArityCheck;
        }
        JITCode& getJITCode() { return m_jitCode; }
        MacroAssemblerCodePtr getJITCodeWithArityCheck() { return m_jitCodeWithArityCheck; }
        JITCode::JITType getJITType() { return m_jitCode.jitType(); }
        ExecutableMemoryHandle* executableMemory() { return getJITCode().getExecutableMemory(); }
        virtual JSObject* compileOptimized(ExecState*, ScopeChainNode*) = 0;
        virtual void jettison(JSGlobalData&) = 0;
        virtual CodeBlock* replacement() = 0;
        virtual bool canCompileWithDFG() = 0;
        bool hasOptimizedReplacement()
        {
            ASSERT(getJITType() == JITCode::BaselineJIT);
            bool result = replacement()->getJITType() > getJITType();
#if !ASSERT_DISABLED
            if (result)
                ASSERT(replacement()->getJITType() == JITCode::DFGJIT);
            else {
                ASSERT(replacement()->getJITType() == JITCode::BaselineJIT);
                ASSERT(replacement() == this);
            }
#endif
            return result;
        }
#else
        JITCode::JITType getJITType() { return JITCode::BaselineJIT; }
#endif

        ScriptExecutable* ownerExecutable() const { return m_ownerExecutable.get(); }

        void setGlobalData(JSGlobalData* globalData) { m_globalData = globalData; }
        JSGlobalData* globalData() { return m_globalData; }

        void setThisRegister(int thisRegister) { m_thisRegister = thisRegister; }
        int thisRegister() const { return m_thisRegister; }

        void setNeedsFullScopeChain(bool needsFullScopeChain) { m_needsFullScopeChain = needsFullScopeChain; }
        bool needsFullScopeChain() const { return m_needsFullScopeChain; }
        void setUsesEval(bool usesEval) { m_usesEval = usesEval; }
        bool usesEval() const { return m_usesEval; }
        
        void setArgumentsRegister(int argumentsRegister)
        {
            ASSERT(argumentsRegister != -1);
            m_argumentsRegister = argumentsRegister;
            ASSERT(usesArguments());
        }
        int argumentsRegister()
        {
            ASSERT(usesArguments());
            return m_argumentsRegister;
        }
        void setActivationRegister(int activationRegister)
        {
            m_activationRegister = activationRegister;
        }
        int activationRegister()
        {
            ASSERT(needsFullScopeChain());
            return m_activationRegister;
        }
        bool usesArguments() const { return m_argumentsRegister != -1; }

        CodeType codeType() const { return m_codeType; }

        SourceProvider* source() const { return m_source.get(); }
        unsigned sourceOffset() const { return m_sourceOffset; }

        size_t numberOfJumpTargets() const { return m_jumpTargets.size(); }
        void addJumpTarget(unsigned jumpTarget) { m_jumpTargets.append(jumpTarget); }
        unsigned jumpTarget(int index) const { return m_jumpTargets[index]; }
        unsigned lastJumpTarget() const { return m_jumpTargets.last(); }

        void createActivation(CallFrame*);

        void clearEvalCache();

#if ENABLE(INTERPRETER)
        void addPropertyAccessInstruction(unsigned propertyAccessInstruction)
        {
            if (!m_globalData->canUseJIT())
                m_propertyAccessInstructions.append(propertyAccessInstruction);
        }
        void addGlobalResolveInstruction(unsigned globalResolveInstruction)
        {
            if (!m_globalData->canUseJIT())
                m_globalResolveInstructions.append(globalResolveInstruction);
        }
        bool hasGlobalResolveInstructionAtBytecodeOffset(unsigned bytecodeOffset);
#endif
#if ENABLE(JIT)
        void setNumberOfStructureStubInfos(size_t size) { m_structureStubInfos.grow(size); }
        size_t numberOfStructureStubInfos() const { return m_structureStubInfos.size(); }
        StructureStubInfo& structureStubInfo(int index) { return m_structureStubInfos[index]; }

        void addGlobalResolveInfo(unsigned globalResolveInstruction)
        {
            if (m_globalData->canUseJIT())
                m_globalResolveInfos.append(GlobalResolveInfo(globalResolveInstruction));
        }
        GlobalResolveInfo& globalResolveInfo(int index) { return m_globalResolveInfos[index]; }
        bool hasGlobalResolveInfoAtBytecodeOffset(unsigned bytecodeOffset);

        void setNumberOfCallLinkInfos(size_t size) { m_callLinkInfos.grow(size); }
        size_t numberOfCallLinkInfos() const { return m_callLinkInfos.size(); }
        CallLinkInfo& callLinkInfo(int index) { return m_callLinkInfos[index]; }

        void addMethodCallLinkInfos(unsigned n) { ASSERT(m_globalData->canUseJIT()); m_methodCallLinkInfos.grow(n); }
        MethodCallLinkInfo& methodCallLinkInfo(int index) { return m_methodCallLinkInfos[index]; }
#endif
        
#if ENABLE(VALUE_PROFILER)
        ValueProfile* addValueProfile(int bytecodeOffset)
        {
            m_valueProfiles.append(ValueProfile(bytecodeOffset));
            return &m_valueProfiles.last();
        }
        unsigned numberOfValueProfiles() { return m_valueProfiles.size(); }
        ValueProfile* valueProfile(int index) { return &m_valueProfiles[index]; }
        ValueProfile* valueProfileForBytecodeOffset(int bytecodeOffset)
        {
            return WTF::genericBinarySearch<ValueProfile, int, getValueProfileBytecodeOffset>(m_valueProfiles, m_valueProfiles.size(), bytecodeOffset);
        }
        ValueProfile* valueProfileForArgument(int argumentIndex)
        {
            int index = argumentIndex;
            if (static_cast<unsigned>(index) >= m_valueProfiles.size())
                return 0;
            ValueProfile* result = valueProfile(argumentIndex);
            if (result->m_bytecodeOffset != -1)
                return 0;
            return result;
        }
        
        RareCaseProfile* addRareCaseProfile(int bytecodeOffset)
        {
            m_rareCaseProfiles.append(RareCaseProfile(bytecodeOffset));
            return &m_rareCaseProfiles.last();
        }
        unsigned numberOfRareCaseProfiles() { return m_rareCaseProfiles.size(); }
        RareCaseProfile* rareCaseProfile(int index) { return &m_rareCaseProfiles[index]; }
        RareCaseProfile* rareCaseProfileForBytecodeOffset(int bytecodeOffset)
        {
            return WTF::genericBinarySearch<RareCaseProfile, int, getRareCaseProfileBytecodeOffset>(m_rareCaseProfiles, m_rareCaseProfiles.size(), bytecodeOffset);
        }
        
        bool likelyToTakeSlowCase(int bytecodeOffset)
        {
            return rareCaseProfileForBytecodeOffset(bytecodeOffset)->m_counter >= Heuristics::likelyToTakeSlowCaseThreshold;
        }
        
        bool couldTakeSlowCase(int bytecodeOffset)
        {
            return rareCaseProfileForBytecodeOffset(bytecodeOffset)->m_counter >= Heuristics::couldTakeSlowCaseThreshold;
        }
        
        RareCaseProfile* addSpecialFastCaseProfile(int bytecodeOffset)
        {
            m_specialFastCaseProfiles.append(RareCaseProfile(bytecodeOffset));
            return &m_specialFastCaseProfiles.last();
        }
        unsigned numberOfSpecialFastCaseProfiles() { return m_specialFastCaseProfiles.size(); }
        RareCaseProfile* specialFastCaseProfile(int index) { return &m_specialFastCaseProfiles[index]; }
        RareCaseProfile* specialFastCaseProfileForBytecodeOffset(int bytecodeOffset)
        {
            return WTF::genericBinarySearch<RareCaseProfile, int, getRareCaseProfileBytecodeOffset>(m_specialFastCaseProfiles, m_specialFastCaseProfiles.size(), bytecodeOffset);
        }
        
        bool likelyToTakeSpecialFastCase(int bytecodeOffset)
        {
            unsigned specialFastCaseCount = specialFastCaseProfileForBytecodeOffset(bytecodeOffset)->m_counter;
            return specialFastCaseCount >= Heuristics::likelyToTakeSlowCaseThreshold;
        }
        
        bool likelyToTakeDeepestSlowCase(int bytecodeOffset)
        {
            unsigned slowCaseCount = rareCaseProfileForBytecodeOffset(bytecodeOffset)->m_counter;
            unsigned specialFastCaseCount = specialFastCaseProfileForBytecodeOffset(bytecodeOffset)->m_counter;
            return (slowCaseCount - specialFastCaseCount) >= Heuristics::likelyToTakeSlowCaseThreshold;
        }
        
        bool likelyToTakeAnySlowCase(int bytecodeOffset)
        {
            unsigned slowCaseCount = rareCaseProfileForBytecodeOffset(bytecodeOffset)->m_counter;
            unsigned specialFastCaseCount = specialFastCaseProfileForBytecodeOffset(bytecodeOffset)->m_counter;
            return (slowCaseCount + specialFastCaseCount) >= Heuristics::likelyToTakeSlowCaseThreshold;
        }
        
        void resetRareCaseProfiles();
#endif

        unsigned globalResolveInfoCount() const
        {
#if ENABLE(JIT)    
            if (m_globalData->canUseJIT())
                return m_globalResolveInfos.size();
#endif
            return 0;
        }

        // Exception handling support

        size_t numberOfExceptionHandlers() const { return m_rareData ? m_rareData->m_exceptionHandlers.size() : 0; }
        void addExceptionHandler(const HandlerInfo& hanler) { createRareDataIfNecessary(); return m_rareData->m_exceptionHandlers.append(hanler); }
        HandlerInfo& exceptionHandler(int index) { ASSERT(m_rareData); return m_rareData->m_exceptionHandlers[index]; }

        void addExpressionInfo(const ExpressionRangeInfo& expressionInfo)
        {
            createRareDataIfNecessary();
            m_rareData->m_expressionInfo.append(expressionInfo);
        }

        void addLineInfo(unsigned bytecodeOffset, int lineNo)
        {
            createRareDataIfNecessary();
            Vector<LineInfo>& lineInfo = m_rareData->m_lineInfo;
            if (!lineInfo.size() || lineInfo.last().lineNumber != lineNo) {
                LineInfo info = { bytecodeOffset, lineNo };
                lineInfo.append(info);
            }
        }

        bool hasExpressionInfo() { return m_rareData && m_rareData->m_expressionInfo.size(); }
        bool hasLineInfo() { return m_rareData && m_rareData->m_lineInfo.size(); }
        //  We only generate exception handling info if the user is debugging
        // (and may want line number info), or if the function contains exception handler.
        bool needsCallReturnIndices()
        {
            return m_rareData &&
                (m_rareData->m_expressionInfo.size() || m_rareData->m_lineInfo.size() || m_rareData->m_exceptionHandlers.size());
        }

#if ENABLE(JIT)
        Vector<CallReturnOffsetToBytecodeOffset>& callReturnIndexVector()
        {
            createRareDataIfNecessary();
            return m_rareData->m_callReturnIndexVector;
        }
#endif

#if ENABLE(DFG_JIT)
        SegmentedVector<InlineCallFrame, 4>& inlineCallFrames()
        {
            createRareDataIfNecessary();
            return m_rareData->m_inlineCallFrames;
        }
        
        Vector<CodeOriginAtCallReturnOffset>& codeOrigins()
        {
            createRareDataIfNecessary();
            return m_rareData->m_codeOrigins;
        }
        
        // Having code origins implies that there has been some inlining.
        bool hasCodeOrigins()
        {
            return m_rareData && !!m_rareData->m_codeOrigins.size();
        }
        
        CodeOrigin codeOriginForReturn(ReturnAddressPtr returnAddress)
        {
            ASSERT(hasCodeOrigins());
            return binarySearch<CodeOriginAtCallReturnOffset, unsigned, getCallReturnOffsetForCodeOrigin>(codeOrigins().begin(), codeOrigins().size(), getJITCode().offsetOf(returnAddress.value()))->codeOrigin;
        }
#endif

        // Constant Pool

        size_t numberOfIdentifiers() const { return m_identifiers.size(); }
        void addIdentifier(const Identifier& i) { return m_identifiers.append(i); }
        Identifier& identifier(int index) { return m_identifiers[index]; }

        size_t numberOfConstantRegisters() const { return m_constantRegisters.size(); }
        void addConstant(JSValue v)
        {
            m_constantRegisters.append(WriteBarrier<Unknown>());
            m_constantRegisters.last().set(m_globalObject->globalData(), m_ownerExecutable.get(), v);
        }
        WriteBarrier<Unknown>& constantRegister(int index) { return m_constantRegisters[index - FirstConstantRegisterIndex]; }
        ALWAYS_INLINE bool isConstantRegisterIndex(int index) const { return index >= FirstConstantRegisterIndex; }
        ALWAYS_INLINE JSValue getConstant(int index) const { return m_constantRegisters[index - FirstConstantRegisterIndex].get(); }

        unsigned addFunctionDecl(FunctionExecutable* n)
        {
            unsigned size = m_functionDecls.size();
            m_functionDecls.append(WriteBarrier<FunctionExecutable>());
            m_functionDecls.last().set(m_globalObject->globalData(), m_ownerExecutable.get(), n);
            return size;
        }
        FunctionExecutable* functionDecl(int index) { return m_functionDecls[index].get(); }
        int numberOfFunctionDecls() { return m_functionDecls.size(); }
        unsigned addFunctionExpr(FunctionExecutable* n)
        {
            unsigned size = m_functionExprs.size();
            m_functionExprs.append(WriteBarrier<FunctionExecutable>());
            m_functionExprs.last().set(m_globalObject->globalData(), m_ownerExecutable.get(), n);
            return size;
        }
        FunctionExecutable* functionExpr(int index) { return m_functionExprs[index].get(); }

        unsigned addRegExp(RegExp* r)
        {
            createRareDataIfNecessary();
            unsigned size = m_rareData->m_regexps.size();
            m_rareData->m_regexps.append(WriteBarrier<RegExp>(*m_globalData, ownerExecutable(), r));
            return size;
        }
        unsigned numberOfRegExps() const
        {
            if (!m_rareData)
                return 0;
            return m_rareData->m_regexps.size();
        }
        RegExp* regexp(int index) const { ASSERT(m_rareData); return m_rareData->m_regexps[index].get(); }

        unsigned addConstantBuffer(unsigned length)
        {
            createRareDataIfNecessary();
            unsigned size = m_rareData->m_constantBuffers.size();
            m_rareData->m_constantBuffers.append(Vector<JSValue>(length));
            return size;
        }

        JSValue* constantBuffer(unsigned index)
        {
            ASSERT(m_rareData);
            return m_rareData->m_constantBuffers[index].data();
        }

        JSGlobalObject* globalObject() { return m_globalObject.get(); }

        // Jump Tables

        size_t numberOfImmediateSwitchJumpTables() const { return m_rareData ? m_rareData->m_immediateSwitchJumpTables.size() : 0; }
        SimpleJumpTable& addImmediateSwitchJumpTable() { createRareDataIfNecessary(); m_rareData->m_immediateSwitchJumpTables.append(SimpleJumpTable()); return m_rareData->m_immediateSwitchJumpTables.last(); }
        SimpleJumpTable& immediateSwitchJumpTable(int tableIndex) { ASSERT(m_rareData); return m_rareData->m_immediateSwitchJumpTables[tableIndex]; }

        size_t numberOfCharacterSwitchJumpTables() const { return m_rareData ? m_rareData->m_characterSwitchJumpTables.size() : 0; }
        SimpleJumpTable& addCharacterSwitchJumpTable() { createRareDataIfNecessary(); m_rareData->m_characterSwitchJumpTables.append(SimpleJumpTable()); return m_rareData->m_characterSwitchJumpTables.last(); }
        SimpleJumpTable& characterSwitchJumpTable(int tableIndex) { ASSERT(m_rareData); return m_rareData->m_characterSwitchJumpTables[tableIndex]; }

        size_t numberOfStringSwitchJumpTables() const { return m_rareData ? m_rareData->m_stringSwitchJumpTables.size() : 0; }
        StringJumpTable& addStringSwitchJumpTable() { createRareDataIfNecessary(); m_rareData->m_stringSwitchJumpTables.append(StringJumpTable()); return m_rareData->m_stringSwitchJumpTables.last(); }
        StringJumpTable& stringSwitchJumpTable(int tableIndex) { ASSERT(m_rareData); return m_rareData->m_stringSwitchJumpTables[tableIndex]; }


        SymbolTable* symbolTable() { return m_symbolTable; }
        SharedSymbolTable* sharedSymbolTable() { ASSERT(m_codeType == FunctionCode); return static_cast<SharedSymbolTable*>(m_symbolTable); }

        EvalCodeCache& evalCodeCache() { createRareDataIfNecessary(); return m_rareData->m_evalCodeCache; }

        void shrinkToFit();
        
        void copyDataFrom(CodeBlock* alternative);
        void copyDataFromAlternative();
        
        // Functions for controlling when tiered compilation kicks in. This
        // controls both when the optimizing compiler is invoked and when OSR
        // entry happens. Two triggers exist: the loop trigger and the return
        // trigger. In either case, when an addition to m_executeCounter
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
        unsigned reoptimizationRetryCounter() const
        {
            ASSERT(m_reoptimizationRetryCounter <= Heuristics::reoptimizationRetryCounterMax);
            return m_reoptimizationRetryCounter;
        }
        
        void countReoptimization()
        {
            m_reoptimizationRetryCounter++;
            if (m_reoptimizationRetryCounter > Heuristics::reoptimizationRetryCounterMax)
                m_reoptimizationRetryCounter = Heuristics::reoptimizationRetryCounterMax;
        }
        
        int32_t counterValueForOptimizeAfterWarmUp()
        {
            return Heuristics::executionCounterValueForOptimizeAfterWarmUp << reoptimizationRetryCounter();
        }
        
        int32_t counterValueForOptimizeAfterLongWarmUp()
        {
            return Heuristics::executionCounterValueForOptimizeAfterLongWarmUp << reoptimizationRetryCounter();
        }
        
        int32_t* addressOfExecuteCounter()
        {
            return &m_executeCounter;
        }
        
        static ptrdiff_t offsetOfExecuteCounter() { return OBJECT_OFFSETOF(CodeBlock, m_executeCounter); }

        int32_t executeCounter() const { return m_executeCounter; }
        
        unsigned optimizationDelayCounter() const { return m_optimizationDelayCounter; }
        
        // Call this to force the next optimization trigger to fire. This is
        // rarely wise, since optimization triggers are typically more
        // expensive than executing baseline code.
        void optimizeNextInvocation()
        {
            m_executeCounter = Heuristics::executionCounterValueForOptimizeNextInvocation;
        }
        
        // Call this to prevent optimization from happening again. Note that
        // optimization will still happen after roughly 2^29 invocations,
        // so this is really meant to delay that as much as possible. This
        // is called if optimization failed, and we expect it to fail in
        // the future as well.
        void dontOptimizeAnytimeSoon()
        {
            m_executeCounter = Heuristics::executionCounterValueForDontOptimizeAnytimeSoon;
        }
        
        // Call this to reinitialize the counter to its starting state,
        // forcing a warm-up to happen before the next optimization trigger
        // fires. This is called in the CodeBlock constructor. It also
        // makes sense to call this if an OSR exit occurred. Note that
        // OSR exit code is code generated, so the value of the execute
        // counter that this corresponds to is also available directly.
        void optimizeAfterWarmUp()
        {
            m_executeCounter = counterValueForOptimizeAfterWarmUp();
        }
        
        // Call this to force an optimization trigger to fire only after
        // a lot of warm-up.
        void optimizeAfterLongWarmUp()
        {
            m_executeCounter = counterValueForOptimizeAfterLongWarmUp();
        }
        
        // Call this to cause an optimization trigger to fire soon, but
        // not necessarily the next one. This makes sense if optimization
        // succeeds. Successfuly optimization means that all calls are
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
        void optimizeSoon()
        {
            m_executeCounter = Heuristics::executionCounterValueForOptimizeSoon << reoptimizationRetryCounter();
        }
        
        // The speculative JIT tracks its success rate, so that we can
        // decide when to reoptimize. It's interesting to note that these
        // counters may overflow without any protection. The success
        // counter will overflow before the fail one does, becuase the
        // fail one is used as a trigger to reoptimize. So the worst case
        // is that the success counter overflows and we reoptimize without
        // needing to. But this is harmless. If a method really did
        // execute 2^32 times then compiling it again probably won't hurt
        // anyone.
        
        void countSpeculationSuccess()
        {
            m_speculativeSuccessCounter++;
        }
        
        void countSpeculationFailure()
        {
            m_speculativeFailCounter++;
        }
        
        uint32_t speculativeSuccessCounter() const { return m_speculativeSuccessCounter; }
        uint32_t speculativeFailCounter() const { return m_speculativeFailCounter; }
        
        uint32_t* addressOfSpeculativeSuccessCounter() { return &m_speculativeSuccessCounter; }
        uint32_t* addressOfSpeculativeFailCounter() { return &m_speculativeFailCounter; }
        
        static ptrdiff_t offsetOfSpeculativeSuccessCounter() { return OBJECT_OFFSETOF(CodeBlock, m_speculativeSuccessCounter); }
        static ptrdiff_t offsetOfSpeculativeFailCounter() { return OBJECT_OFFSETOF(CodeBlock, m_speculativeFailCounter); }
        
        // The number of failures that triggers the use of the ratio.
        unsigned largeFailCountThreshold() { return Heuristics::largeFailCountThresholdBase << alternative()->reoptimizationRetryCounter(); }
        unsigned largeFailCountThresholdForLoop() { return Heuristics::largeFailCountThresholdBaseForLoop << alternative()->reoptimizationRetryCounter(); }
        
        bool shouldReoptimizeNow()
        {
            return Heuristics::desiredSpeculativeSuccessFailRatio * speculativeFailCounter() >= speculativeSuccessCounter() && speculativeFailCounter() >= largeFailCountThreshold();
        }
        
        bool shouldReoptimizeFromLoopNow()
        {
            return Heuristics::desiredSpeculativeSuccessFailRatio * speculativeFailCounter() >= speculativeSuccessCounter() && speculativeFailCounter() >= largeFailCountThresholdForLoop();
        }
        
#if ENABLE(VALUE_PROFILER)
        bool shouldOptimizeNow();
#else
        bool shouldOptimizeNow() { return false; }
#endif
        
#if ENABLE(JIT)
        void reoptimize(JSGlobalData& globalData)
        {
            ASSERT(replacement() != this);
            replacement()->jettison(globalData);
            countReoptimization();
            optimizeAfterWarmUp();
        }
#endif

#if ENABLE(VERBOSE_VALUE_PROFILE)
        void dumpValueProfiles();
#endif
        
        // FIXME: Make these remaining members private.

        int m_numCalleeRegisters;
        int m_numVars;
        int m_numCapturedVars;
        int m_numParameters;
        bool m_isConstructor;

    private:
#if !defined(NDEBUG) || ENABLE(OPCODE_SAMPLING)
        void dump(ExecState*, const Vector<Instruction>::const_iterator& begin, Vector<Instruction>::const_iterator&) const;

        CString registerName(ExecState*, int r) const;
        void printUnaryOp(ExecState*, int location, Vector<Instruction>::const_iterator&, const char* op) const;
        void printBinaryOp(ExecState*, int location, Vector<Instruction>::const_iterator&, const char* op) const;
        void printConditionalJump(ExecState*, const Vector<Instruction>::const_iterator&, Vector<Instruction>::const_iterator&, int location, const char* op) const;
        void printGetByIdOp(ExecState*, int location, Vector<Instruction>::const_iterator&, const char* op) const;
        void printPutByIdOp(ExecState*, int location, Vector<Instruction>::const_iterator&, const char* op) const;
#endif
        void visitStructures(SlotVisitor&, Instruction* vPC) const;

        void createRareDataIfNecessary()
        {
            if (!m_rareData)
                m_rareData = adoptPtr(new RareData);
        }

        WriteBarrier<ScriptExecutable> m_ownerExecutable;
        JSGlobalData* m_globalData;

        struct Instructions : public RefCounted<Instructions> {
            Vector<Instruction> m_instructions;
        };
        RefPtr<Instructions> m_instructions;
        unsigned m_instructionCount;

        int m_thisRegister;
        int m_argumentsRegister;
        int m_activationRegister;

        bool m_needsFullScopeChain;
        bool m_usesEval;
        bool m_isNumericCompareFunction;
        bool m_isStrictMode;

        CodeType m_codeType;

        RefPtr<SourceProvider> m_source;
        unsigned m_sourceOffset;

#if ENABLE(INTERPRETER)
        Vector<unsigned> m_propertyAccessInstructions;
        Vector<unsigned> m_globalResolveInstructions;
#endif
#if ENABLE(JIT)
        Vector<StructureStubInfo> m_structureStubInfos;
        Vector<GlobalResolveInfo> m_globalResolveInfos;
        Vector<CallLinkInfo> m_callLinkInfos;
        Vector<MethodCallLinkInfo> m_methodCallLinkInfos;
        JITCode m_jitCode;
        MacroAssemblerCodePtr m_jitCodeWithArityCheck;
        SentinelLinkedList<CallLinkInfo, BasicRawSentinelNode<CallLinkInfo> > m_incomingCalls;
#endif
#if ENABLE(DFG_JIT)
        OwnPtr<CompactJITCodeMap> m_jitCodeMap;
        
        struct DFGData {
            Vector<DFG::OSREntryData, 4> osrEntry;
            SegmentedVector<DFG::OSRExit, 16> osrExit;
            Vector<DFG::SpeculationRecovery, 4> speculationRecovery;
        };
        
        OwnPtr<DFGData> m_dfgData;
#endif
#if ENABLE(VALUE_PROFILER)
        SegmentedVector<ValueProfile, 8> m_valueProfiles;
        SegmentedVector<RareCaseProfile, 8> m_rareCaseProfiles;
        SegmentedVector<RareCaseProfile, 8> m_specialFastCaseProfiles;
#endif

        Vector<unsigned> m_jumpTargets;
        Vector<unsigned> m_loopTargets;

        // Constant Pool
        Vector<Identifier> m_identifiers;
        COMPILE_ASSERT(sizeof(Register) == sizeof(WriteBarrier<Unknown>), Register_must_be_same_size_as_WriteBarrier_Unknown);
        Vector<WriteBarrier<Unknown> > m_constantRegisters;
        Vector<WriteBarrier<FunctionExecutable> > m_functionDecls;
        Vector<WriteBarrier<FunctionExecutable> > m_functionExprs;

        SymbolTable* m_symbolTable;

        OwnPtr<CodeBlock> m_alternative;
        
        int32_t m_executeCounter;
        uint32_t m_speculativeSuccessCounter;
        uint32_t m_speculativeFailCounter;
        uint8_t m_optimizationDelayCounter;
        uint8_t m_reoptimizationRetryCounter;

        struct RareData {
           WTF_MAKE_FAST_ALLOCATED;
        public:
            Vector<HandlerInfo> m_exceptionHandlers;

            // Rare Constants
            Vector<WriteBarrier<RegExp> > m_regexps;

            // Buffers used for large array literals
            Vector<Vector<JSValue> > m_constantBuffers;
            
            // Jump Tables
            Vector<SimpleJumpTable> m_immediateSwitchJumpTables;
            Vector<SimpleJumpTable> m_characterSwitchJumpTables;
            Vector<StringJumpTable> m_stringSwitchJumpTables;

            EvalCodeCache m_evalCodeCache;

            // Expression info - present if debugging.
            Vector<ExpressionRangeInfo> m_expressionInfo;
            // Line info - present if profiling or debugging.
            Vector<LineInfo> m_lineInfo;
#if ENABLE(JIT)
            Vector<CallReturnOffsetToBytecodeOffset> m_callReturnIndexVector;
#endif
#if ENABLE(DFG_JIT)
            SegmentedVector<InlineCallFrame, 4> m_inlineCallFrames;
            Vector<CodeOriginAtCallReturnOffset> m_codeOrigins;
#endif
        };
#if COMPILER(MSVC)
        friend void WTF::deleteOwnedPtr<RareData>(RareData*);
#endif
        OwnPtr<RareData> m_rareData;
    };

    // Program code is not marked by any function, so we make the global object
    // responsible for marking it.

    class GlobalCodeBlock : public CodeBlock {
    protected:
        GlobalCodeBlock(ScriptExecutable* ownerExecutable, CodeType codeType, JSGlobalObject* globalObject, PassRefPtr<SourceProvider> sourceProvider, unsigned sourceOffset, PassOwnPtr<CodeBlock> alternative)
            : CodeBlock(ownerExecutable, codeType, globalObject, sourceProvider, sourceOffset, &m_unsharedSymbolTable, false, alternative)
        {
        }

    private:
        SymbolTable m_unsharedSymbolTable;
    };

    class ProgramCodeBlock : public GlobalCodeBlock {
    public:
        ProgramCodeBlock(ProgramExecutable* ownerExecutable, CodeType codeType, JSGlobalObject* globalObject, PassRefPtr<SourceProvider> sourceProvider, PassOwnPtr<CodeBlock> alternative)
            : GlobalCodeBlock(ownerExecutable, codeType, globalObject, sourceProvider, 0, alternative)
        {
        }
        
#if ENABLE(JIT)
    protected:
        virtual JSObject* compileOptimized(ExecState*, ScopeChainNode*);
        virtual void jettison(JSGlobalData&);
        virtual CodeBlock* replacement();
        virtual bool canCompileWithDFG();
#endif
    };

    class EvalCodeBlock : public GlobalCodeBlock {
    public:
        EvalCodeBlock(EvalExecutable* ownerExecutable, JSGlobalObject* globalObject, PassRefPtr<SourceProvider> sourceProvider, int baseScopeDepth, PassOwnPtr<CodeBlock> alternative)
            : GlobalCodeBlock(ownerExecutable, EvalCode, globalObject, sourceProvider, 0, alternative)
            , m_baseScopeDepth(baseScopeDepth)
        {
        }

        int baseScopeDepth() const { return m_baseScopeDepth; }

        const Identifier& variable(unsigned index) { return m_variables[index]; }
        unsigned numVariables() { return m_variables.size(); }
        void adoptVariables(Vector<Identifier>& variables)
        {
            ASSERT(m_variables.isEmpty());
            m_variables.swap(variables);
        }
        
#if ENABLE(JIT)
    protected:
        virtual JSObject* compileOptimized(ExecState*, ScopeChainNode*);
        virtual void jettison(JSGlobalData&);
        virtual CodeBlock* replacement();
        virtual bool canCompileWithDFG();
#endif

    private:
        int m_baseScopeDepth;
        Vector<Identifier> m_variables;
    };

    class FunctionCodeBlock : public CodeBlock {
    public:
        // Rather than using the usual RefCounted::create idiom for SharedSymbolTable we just use new
        // as we need to initialise the CodeBlock before we could initialise any RefPtr to hold the shared
        // symbol table, so we just pass as a raw pointer with a ref count of 1.  We then manually deref
        // in the destructor.
        FunctionCodeBlock(FunctionExecutable* ownerExecutable, CodeType codeType, JSGlobalObject* globalObject, PassRefPtr<SourceProvider> sourceProvider, unsigned sourceOffset, bool isConstructor, PassOwnPtr<CodeBlock> alternative = nullptr)
            : CodeBlock(ownerExecutable, codeType, globalObject, sourceProvider, sourceOffset, SharedSymbolTable::create().leakRef(), isConstructor, alternative)
        {
        }
        ~FunctionCodeBlock()
        {
            sharedSymbolTable()->deref();
        }
        
#if ENABLE(JIT)
    protected:
        virtual JSObject* compileOptimized(ExecState*, ScopeChainNode*);
        virtual void jettison(JSGlobalData&);
        virtual CodeBlock* replacement();
        virtual bool canCompileWithDFG();
#endif
    };

    inline Register& ExecState::r(int index)
    {
        CodeBlock* codeBlock = this->codeBlock();
        if (codeBlock->isConstantRegisterIndex(index))
            return *reinterpret_cast<Register*>(&codeBlock->constantRegister(index));
        return this[index];
    }

    inline Register& ExecState::uncheckedR(int index)
    {
        ASSERT(index < FirstConstantRegisterIndex);
        return this[index];
    }

#if ENABLE(DFG_JIT)
    inline bool ExecState::isInlineCallFrame()
    {
        if (LIKELY(!codeBlock() || codeBlock()->getJITType() != JITCode::DFGJIT))
            return false;
        return isInlineCallFrameSlow();
    }
#endif
    
} // namespace JSC

#endif // CodeBlock_h
