/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#ifndef DFGJITCompiler_h
#define DFGJITCompiler_h

#if ENABLE(DFG_JIT)

#include "CCallHelpers.h"
#include "DFGDisassembler.h"
#include "DFGGraph.h"
#include "DFGInlineCacheWrapper.h"
#include "DFGJITCode.h"
#include "DFGOSRExitCompilationInfo.h"
#include "DFGRegisterBank.h"
#include "TempRegisterSet.h"

namespace JSC {

class AbstractSamplingCounter;
class CodeBlock;
class VM;

namespace DFG {

class JITCodeGenerator;
class NodeToRegisterMap;
class OSRExitJumpPlaceholder;
class SlowPathGenerator;
class SpeculativeJIT;
class SpeculationRecovery;

struct EntryLocation;
struct OSRExit;

// === CallLinkRecord ===
//
// A record of a call out from JIT code that needs linking to a helper function.
// Every CallLinkRecord contains a reference to the call instruction & the function
// that it needs to be linked to.
struct CallLinkRecord {
    CallLinkRecord(MacroAssembler::Call call, FunctionPtr function)
        : m_call(call)
        , m_function(function)
    {
    }

    MacroAssembler::Call m_call;
    FunctionPtr m_function;
};

struct InRecord {
    InRecord(
        MacroAssembler::PatchableJump jump, MacroAssembler::Label done,
        SlowPathGenerator* slowPathGenerator, StructureStubInfo* stubInfo)
        : m_jump(jump)
        , m_done(done)
        , m_slowPathGenerator(slowPathGenerator)
        , m_stubInfo(stubInfo)
    {
    }
    
    MacroAssembler::PatchableJump m_jump;
    MacroAssembler::Label m_done;
    SlowPathGenerator* m_slowPathGenerator;
    StructureStubInfo* m_stubInfo;
};

// === JITCompiler ===
//
// DFG::JITCompiler is responsible for generating JIT code from the dataflow graph.
// It does so by delegating to the speculative & non-speculative JITs, which
// generate to a MacroAssembler (which the JITCompiler owns through an inheritance
// relationship). The JITCompiler holds references to information required during
// compilation, and also records information used in linking (e.g. a list of all
// call to be linked).
class JITCompiler : public CCallHelpers {
public:
    JITCompiler(Graph& dfg);
    ~JITCompiler();
    
    void compile();
    void compileFunction();
    
    void link();
    void linkFunction();

    // Accessors for properties.
    Graph& graph() { return m_graph; }
    
    // Methods to set labels for the disassembler.
    void setStartOfCode()
    {
        if (LIKELY(!m_disassembler))
            return;
        m_disassembler->setStartOfCode(labelIgnoringWatchpoints());
    }
    
    void setForBlockIndex(BlockIndex blockIndex)
    {
        if (LIKELY(!m_disassembler))
            return;
        m_disassembler->setForBlockIndex(blockIndex, labelIgnoringWatchpoints());
    }
    
    void setForNode(Node* node)
    {
        if (LIKELY(!m_disassembler))
            return;
        m_disassembler->setForNode(node, labelIgnoringWatchpoints());
    }
    
    void setEndOfMainPath()
    {
        if (LIKELY(!m_disassembler))
            return;
        m_disassembler->setEndOfMainPath(labelIgnoringWatchpoints());
    }
    
    void setEndOfCode()
    {
        if (LIKELY(!m_disassembler))
            return;
        m_disassembler->setEndOfCode(labelIgnoringWatchpoints());
    }
    
    void emitStoreCodeOrigin(CodeOrigin codeOrigin)
    {
        unsigned index = m_jitCode->common.addCodeOrigin(codeOrigin);
        unsigned locationBits = CallFrame::Location::encodeAsCodeOriginIndex(index);
        store32(TrustedImm32(locationBits), tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));
    }

    // Add a call out from JIT code, without an exception check.
    Call appendCall(const FunctionPtr& function)
    {
        Call functionCall = call();
        m_calls.append(CallLinkRecord(functionCall, function));
        return functionCall;
    }
    
    void exceptionCheck(Jump jumpToHandler)
    {
        m_exceptionChecks.append(jumpToHandler);
    }
    
    void exceptionCheck()
    {
        m_exceptionChecks.append(emitExceptionCheck());
    }

    void exceptionCheckWithCallFrameRollback()
    {
        m_exceptionChecksWithCallFrameRollback.append(emitExceptionCheck());
    }

    // Add a call out from JIT code, with a fast exception check that tests if the return value is zero.
    void fastExceptionCheck()
    {
        m_exceptionChecks.append(branchTestPtr(Zero, GPRInfo::returnValueGPR));
    }
    
    OSRExitCompilationInfo& appendExitInfo(MacroAssembler::JumpList jumpsToFail = MacroAssembler::JumpList())
    {
        OSRExitCompilationInfo info;
        info.m_failureJumps = jumpsToFail;
        m_exitCompilationInfo.append(info);
        return m_exitCompilationInfo.last();
    }

#if USE(JSVALUE32_64)
    void* addressOfDoubleConstant(Node* node)
    {
        ASSERT(m_graph.isNumberConstant(node));
        unsigned constantIndex = node->constantNumber();
        return &(codeBlock()->constantRegister(FirstConstantRegisterIndex + constantIndex));
    }
#endif

    void addGetById(const JITGetByIdGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_getByIds.append(InlineCacheWrapper<JITGetByIdGenerator>(gen, slowPath));
    }
    
    void addPutById(const JITPutByIdGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_putByIds.append(InlineCacheWrapper<JITPutByIdGenerator>(gen, slowPath));
    }

    void addIn(const InRecord& record)
    {
        m_ins.append(record);
    }
    
    unsigned currentJSCallIndex() const
    {
        return m_jsCalls.size();
    }

    void addJSCall(Call fastCall, Call slowCall, DataLabelPtr targetToCheck, CallLinkInfo::CallType callType, GPRReg callee, CodeOrigin codeOrigin)
    {
        m_jsCalls.append(JSCallRecord(fastCall, slowCall, targetToCheck, callType, callee, codeOrigin));
    }
    
    void addWeakReference(JSCell* target)
    {
        m_graph.m_plan.weakReferences.addLazily(target);
    }
    
    void addWeakReferences(const StructureSet& structureSet)
    {
        for (unsigned i = structureSet.size(); i--;)
            addWeakReference(structureSet[i]);
    }
    
    template<typename T>
    Jump branchWeakPtr(RelationalCondition cond, T left, JSCell* weakPtr)
    {
        Jump result = branchPtr(cond, left, TrustedImmPtr(weakPtr));
        addWeakReference(weakPtr);
        return result;
    }
    
    void noticeOSREntry(BasicBlock& basicBlock, JITCompiler::Label blockHead, LinkBuffer& linkBuffer)
    {
        // OSR entry is not allowed into blocks deemed unreachable by control flow analysis.
        if (!basicBlock.cfaHasVisited)
            return;
        
        OSREntryData* entry = m_jitCode->appendOSREntryData(basicBlock.bytecodeBegin, linkBuffer.offsetOf(blockHead));
        
        entry->m_expectedValues = basicBlock.valuesAtHead;
        
        // Fix the expected values: in our protocol, a dead variable will have an expected
        // value of (None, []). But the old JIT may stash some values there. So we really
        // need (Top, TOP).
        for (size_t argument = 0; argument < basicBlock.variablesAtHead.numberOfArguments(); ++argument) {
            Node* node = basicBlock.variablesAtHead.argument(argument);
            if (!node || !node->shouldGenerate())
                entry->m_expectedValues.argument(argument).makeHeapTop();
        }
        for (size_t local = 0; local < basicBlock.variablesAtHead.numberOfLocals(); ++local) {
            Node* node = basicBlock.variablesAtHead.local(local);
            if (!node || !node->shouldGenerate())
                entry->m_expectedValues.local(local).makeHeapTop();
            else {
                VariableAccessData* variable = node->variableAccessData();
                entry->m_machineStackUsed.set(variable->machineLocal().toLocal());
                
                switch (variable->flushFormat()) {
                case FlushedDouble:
                    entry->m_localsForcedDouble.set(local);
                    break;
                case FlushedInt52:
                    entry->m_localsForcedMachineInt.set(local);
                    break;
                default:
                    break;
                }
                
                if (variable->local() != variable->machineLocal()) {
                    entry->m_reshufflings.append(
                        OSREntryReshuffling(
                            variable->local().offset(), variable->machineLocal().offset()));
                }
            }
        }
        
        entry->m_reshufflings.shrinkToFit();
    }
    
    PassRefPtr<JITCode> jitCode() { return m_jitCode; }
    
    Vector<Label>& blockHeads() { return m_blockHeads; }

private:
    friend class OSRExitJumpPlaceholder;
    
    // Internal implementation to compile.
    void compileEntry();
    void compileBody();
    void link(LinkBuffer&);
    
    void exitSpeculativeWithOSR(const OSRExit&, SpeculationRecovery*);
    void compileExceptionHandlers();
    void linkOSRExits();
    void disassemble(LinkBuffer&);
    
    // The dataflow graph currently being generated.
    Graph& m_graph;

    OwnPtr<Disassembler> m_disassembler;
    
    RefPtr<JITCode> m_jitCode;
    
    // Vector of calls out from JIT code, including exception handler information.
    // Count of the number of CallRecords with exception handlers.
    Vector<CallLinkRecord> m_calls;
    JumpList m_exceptionChecks;
    JumpList m_exceptionChecksWithCallFrameRollback;
    
    Vector<Label> m_blockHeads;

    struct JSCallRecord {
        JSCallRecord(Call fastCall, Call slowCall, DataLabelPtr targetToCheck, CallLinkInfo::CallType callType, GPRReg callee, CodeOrigin codeOrigin)
            : m_fastCall(fastCall)
            , m_slowCall(slowCall)
            , m_targetToCheck(targetToCheck)
            , m_callType(callType)
            , m_callee(callee)
            , m_codeOrigin(codeOrigin)
        {
        }
        
        Call m_fastCall;
        Call m_slowCall;
        DataLabelPtr m_targetToCheck;
        CallLinkInfo::CallType m_callType;
        GPRReg m_callee;
        CodeOrigin m_codeOrigin;
    };
    
    Vector<InlineCacheWrapper<JITGetByIdGenerator>, 4> m_getByIds;
    Vector<InlineCacheWrapper<JITPutByIdGenerator>, 4> m_putByIds;
    Vector<InRecord, 4> m_ins;
    Vector<JSCallRecord, 4> m_jsCalls;
    SegmentedVector<OSRExitCompilationInfo, 4> m_exitCompilationInfo;
    Vector<Vector<Label>> m_exitSiteLabels;
    
    Call m_callArityFixup;
    Label m_arityCheck;
    OwnPtr<SpeculativeJIT> m_speculative;
};

} } // namespace JSC::DFG

#endif
#endif

