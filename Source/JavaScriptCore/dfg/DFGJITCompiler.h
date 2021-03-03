/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "CCallHelpers.h"
#include "CodeBlock.h"
#include "DFGDisassembler.h"
#include "DFGGraph.h"
#include "DFGInlineCacheWrapper.h"
#include "DFGJITCode.h"
#include "DFGOSRExitCompilationInfo.h"
#include "GPRInfo.h"
#include "HandlerInfo.h"
#include "JITCode.h"
#include "JITInlineCacheGenerator.h"
#include "LinkBuffer.h"
#include "MacroAssembler.h"
#include "PCToCodeOriginMap.h"

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
    CallLinkRecord(MacroAssembler::Call call, FunctionPtr<OperationPtrTag> function)
        : m_call(call)
        , m_function(function)
    {
    }

    MacroAssembler::Call m_call;
    FunctionPtr<OperationPtrTag> m_function;
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
    
    // Accessors for properties.
    Graph& graph() { return m_graph; }
    
    // Methods to set labels for the disassembler.
    void setStartOfCode()
    {
        m_pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), CodeOrigin(BytecodeIndex(0)));
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
    
    void setEndOfMainPath();
    void setEndOfCode();
    
    CallSiteIndex addCallSite(CodeOrigin codeOrigin)
    {
        return m_jitCode->common.codeOrigins->addCodeOrigin(codeOrigin);
    }

    CallSiteIndex emitStoreCodeOrigin(CodeOrigin codeOrigin)
    {
        CallSiteIndex callSite = addCallSite(codeOrigin);
        emitStoreCallSiteIndex(callSite);
        return callSite;
    }

    void emitStoreCallSiteIndex(CallSiteIndex callSite)
    {
        store32(TrustedImm32(callSite.bits()), tagFor(CallFrameSlot::argumentCountIncludingThis));
    }

    // Add a call out from JIT code, without an exception check.
    Call appendCall(const FunctionPtr<CFunctionPtrTag> function)
    {
        Call functionCall = call(OperationPtrTag);
        m_calls.append(CallLinkRecord(functionCall, function.retagged<OperationPtrTag>()));
        return functionCall;
    }

    Call appendOperationCall(const FunctionPtr<OperationPtrTag> function)
    {
        Call functionCall = call(OperationPtrTag);
        m_calls.append(CallLinkRecord(functionCall, function));
        return functionCall;
    }
    
    void exceptionCheck();

    void exceptionCheckWithCallFrameRollback()
    {
        m_exceptionChecksWithCallFrameRollback.append(emitExceptionCheck(vm()));
    }

    OSRExitCompilationInfo& appendExitInfo(MacroAssembler::JumpList jumpsToFail = MacroAssembler::JumpList())
    {
        OSRExitCompilationInfo info;
        info.m_failureJumps = jumpsToFail;
        m_exitCompilationInfo.append(info);
        return m_exitCompilationInfo.last();
    }

#if USE(JSVALUE32_64)
    void* addressOfDoubleConstant(Node*);
#endif

    void addGetById(const JITGetByIdGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_getByIds.append(InlineCacheWrapper<JITGetByIdGenerator>(gen, slowPath));
    }
    
    void addGetByIdWithThis(const JITGetByIdWithThisGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_getByIdsWithThis.append(InlineCacheWrapper<JITGetByIdWithThisGenerator>(gen, slowPath));
    }

    void addGetByVal(const JITGetByValGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_getByVals.append(InlineCacheWrapper<JITGetByValGenerator>(gen, slowPath));
    }
    
    void addPutById(const JITPutByIdGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_putByIds.append(InlineCacheWrapper<JITPutByIdGenerator>(gen, slowPath));
    }

    void addDelById(const JITDelByIdGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_delByIds.append(InlineCacheWrapper<JITDelByIdGenerator>(gen, slowPath));
    }

    void addDelByVal(const JITDelByValGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_delByVals.append(InlineCacheWrapper<JITDelByValGenerator>(gen, slowPath));
    }

    void addInstanceOf(const JITInstanceOfGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_instanceOfs.append(InlineCacheWrapper<JITInstanceOfGenerator>(gen, slowPath));
    }

    void addInById(const JITInByIdGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_inByIds.append(InlineCacheWrapper<JITInByIdGenerator>(gen, slowPath));
    }

    void addPrivateBrandAccess(const JITPrivateBrandAccessGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_privateBrandAccesses.append(InlineCacheWrapper<JITPrivateBrandAccessGenerator>(gen, slowPath));
    }

    void addJSCall(Call fastCall, Call slowCall, DataLabelPtr targetToCheck, CallLinkInfo* info)
    {
        m_jsCalls.append(JSCallRecord(fastCall, slowCall, targetToCheck, info));
    }
    
    void addJSDirectCall(Call call, Label slowPath, CallLinkInfo* info)
    {
        m_jsDirectCalls.append(JSDirectCallRecord(call, slowPath, info));
    }
    
    void addJSDirectTailCall(PatchableJump patchableJump, Call call, Label slowPath, CallLinkInfo* info)
    {
        m_jsDirectTailCalls.append(JSDirectTailCallRecord(patchableJump, call, slowPath, info));
    }
    
    void addWeakReference(JSCell* target)
    {
        m_graph.m_plan.weakReferences().addLazily(target);
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

    template<typename T>
    Jump branchWeakStructure(RelationalCondition cond, T left, RegisteredStructure weakStructure)
    {
        Structure* structure = weakStructure.get();
#if USE(JSVALUE64)
        Jump result = branch32(cond, left, TrustedImm32(structure->id()));
        return result;
#else
        return branchPtr(cond, left, TrustedImmPtr(structure));
#endif
    }

    void noticeOSREntry(BasicBlock&, JITCompiler::Label blockHead, LinkBuffer&);
    void noticeCatchEntrypoint(BasicBlock&, JITCompiler::Label blockHead, LinkBuffer&, Vector<FlushFormat>&& argumentFormats);
    
    RefPtr<JITCode> jitCode() { return m_jitCode; }
    
    Vector<Label>& blockHeads() { return m_blockHeads; }

    CallSiteIndex recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(const CodeOrigin&, unsigned eventStreamIndex);

    PCToCodeOriginMapBuilder& pcToCodeOriginMapBuilder() { return m_pcToCodeOriginMapBuilder; }

    VM& vm() { return m_graph.m_vm; }

private:
    friend class OSRExitJumpPlaceholder;
    
    // Internal implementation to compile.
    void compileEntry();
    void compileSetupRegistersForEntry();
    void compileEntryExecutionFlag();
    void compileBody();
    void link(LinkBuffer&);
    
    void exitSpeculativeWithOSR(const OSRExit&, SpeculationRecovery*);
    void compileExceptionHandlers();
    void linkOSRExits();
    void disassemble(LinkBuffer&);

    void appendExceptionHandlingOSRExit(ExitKind, unsigned eventStreamIndex, CodeOrigin, HandlerInfo* exceptionHandler, CallSiteIndex, MacroAssembler::JumpList jumpsToFail = MacroAssembler::JumpList());

    void makeCatchOSREntryBuffer();

    // The dataflow graph currently being generated.
    Graph& m_graph;

    std::unique_ptr<Disassembler> m_disassembler;
    
    RefPtr<JITCode> m_jitCode;
    
    // Vector of calls out from JIT code, including exception handler information.
    // Count of the number of CallRecords with exception handlers.
    Vector<CallLinkRecord> m_calls;
    JumpList m_exceptionChecks;
    JumpList m_exceptionChecksWithCallFrameRollback;
    
    Vector<Label> m_blockHeads;


    struct JSCallRecord {
        JSCallRecord(Call fastCall, Call slowCall, DataLabelPtr targetToCheck, CallLinkInfo* info)
            : fastCall(fastCall)
            , slowCall(slowCall)
            , targetToCheck(targetToCheck)
            , info(info)
        {
            ASSERT(fastCall.isFlagSet(Call::Near));
            ASSERT(slowCall.isFlagSet(Call::Near));
        }
        
        Call fastCall;
        Call slowCall;
        DataLabelPtr targetToCheck;
        CallLinkInfo* info;
    };
    
    struct JSDirectCallRecord {
        JSDirectCallRecord(Call call, Label slowPath, CallLinkInfo* info)
            : call(call)
            , slowPath(slowPath)
            , info(info)
        {
            ASSERT(call.isFlagSet(Call::Near));
        }
        
        Call call;
        Label slowPath;
        CallLinkInfo* info;
    };
    
    struct JSDirectTailCallRecord {
        JSDirectTailCallRecord(PatchableJump patchableJump, Call call, Label slowPath, CallLinkInfo* info)
            : patchableJump(patchableJump)
            , call(call)
            , slowPath(slowPath)
            , info(info)
        {
            ASSERT(call.isFlagSet(Call::Near) && call.isFlagSet(Call::Tail));
        }
        
        PatchableJump patchableJump;
        Call call;
        Label slowPath;
        CallLinkInfo* info;
    };

    
    Vector<InlineCacheWrapper<JITGetByIdGenerator>, 4> m_getByIds;
    Vector<InlineCacheWrapper<JITGetByIdWithThisGenerator>, 4> m_getByIdsWithThis;
    Vector<InlineCacheWrapper<JITGetByValGenerator>, 4> m_getByVals;
    Vector<InlineCacheWrapper<JITPutByIdGenerator>, 4> m_putByIds;
    Vector<InlineCacheWrapper<JITDelByIdGenerator>, 4> m_delByIds;
    Vector<InlineCacheWrapper<JITDelByValGenerator>, 4> m_delByVals;
    Vector<InlineCacheWrapper<JITInByIdGenerator>, 4> m_inByIds;
    Vector<InlineCacheWrapper<JITInstanceOfGenerator>, 4> m_instanceOfs;
    Vector<InlineCacheWrapper<JITPrivateBrandAccessGenerator>, 4> m_privateBrandAccesses;
    Vector<JSCallRecord, 4> m_jsCalls;
    Vector<JSDirectCallRecord, 4> m_jsDirectCalls;
    Vector<JSDirectTailCallRecord, 4> m_jsDirectTailCalls;
    SegmentedVector<OSRExitCompilationInfo, 4> m_exitCompilationInfo;
    Vector<Vector<Label>> m_exitSiteLabels;
    
    struct ExceptionHandlingOSRExitInfo {
        OSRExitCompilationInfo& exitInfo;
        HandlerInfo baselineExceptionHandler;
        CallSiteIndex callSiteIndex;
    };
    Vector<ExceptionHandlingOSRExitInfo> m_exceptionHandlerOSRExitCallSites;
    
    std::unique_ptr<SpeculativeJIT> m_speculative;
    PCToCodeOriginMapBuilder m_pcToCodeOriginMapBuilder;
};

} } // namespace JSC::DFG

#endif
