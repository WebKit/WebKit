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
    CallLinkRecord(MacroAssembler::Call call, CodePtr<OperationPtrTag> function)
        : m_call(call)
        , m_function(function)
    {
    }

    MacroAssembler::Call m_call;
    CodePtr<OperationPtrTag> m_function;
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
    friend class SpeculativeJIT;

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
    Call appendCall(const CodePtr<CFunctionPtrTag> function)
    {
        Call functionCall = call(OperationPtrTag);
        m_calls.append(CallLinkRecord(functionCall, function.retagged<OperationPtrTag>()));
        return functionCall;
    }

    Call appendOperationCall(const CodePtr<OperationPtrTag> function)
    {
        Call functionCall = call(OperationPtrTag);
        m_calls.append(CallLinkRecord(functionCall, function));
        return functionCall;
    }

    void appendCall(CCallHelpers::Address address)
    {
        call(address, OperationPtrTag);
    }
    
    void exceptionCheck();

    void exceptionJumpWithCallFrameRollback()
    {
        m_exceptionChecksWithCallFrameRollback.append(jump());
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

    void addGetByValWithThis(const JITGetByValWithThisGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_getByValsWithThis.append(InlineCacheWrapper<JITGetByValWithThisGenerator>(gen, slowPath));
    }

    void addPutById(const JITPutByIdGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_putByIds.append(InlineCacheWrapper<JITPutByIdGenerator>(gen, slowPath));
    }

    void addPutByVal(const JITPutByValGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_putByVals.append(InlineCacheWrapper<JITPutByValGenerator>(gen, slowPath));
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

    void addInByVal(const JITInByValGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_inByVals.append(InlineCacheWrapper<JITInByValGenerator>(gen, slowPath));
    }

    void addPrivateBrandAccess(const JITPrivateBrandAccessGenerator& gen, SlowPathGenerator* slowPath)
    {
        m_privateBrandAccesses.append(InlineCacheWrapper<JITPrivateBrandAccessGenerator>(gen, slowPath));
    }

    void addJSCall(Label slowPathStart, Label doneLocation, CompileTimeCallLinkInfo info)
    {
        m_jsCalls.append(JSCallRecord(slowPathStart, doneLocation, info));
    }
    
    void addJSDirectCall(Label slowPath, OptimizingCallLinkInfo* info)
    {
        m_jsDirectCalls.append(JSDirectCallRecord(slowPath, info));
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
    Jump branchWeakStructure(RelationalCondition cond, T left, RegisteredStructure weakStructure)
    {
        Structure* structure = weakStructure.get();
#if USE(JSVALUE64)
        Jump result = branch32(cond, left, TrustedImm32(structure->id().bits()));
        return result;
#else
        return branchPtr(cond, left, TrustedImmPtr(structure));
#endif
    }

    void noticeOSREntry(BasicBlock&, JITCompiler::Label blockHead, LinkBuffer&);
    void noticeCatchEntrypoint(BasicBlock&, JITCompiler::Label blockHead, LinkBuffer&, Vector<FlushFormat>&& argumentFormats);

    unsigned appendOSRExit(OSRExit&& exit)
    {
        unsigned result = m_osrExit.size();
        m_osrExit.append(WTFMove(exit));
        return result;
    }

    unsigned appendSpeculationRecovery(const SpeculationRecovery& recovery)
    {
        unsigned result = m_speculationRecovery.size();
        m_speculationRecovery.append(recovery);
        return result;
    }

    RefPtr<JITCode> jitCode() { return m_jitCode; }
    
    Vector<Label>& blockHeads() { return m_blockHeads; }

    CallSiteIndex recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(const CodeOrigin&, unsigned eventStreamIndex);

    PCToCodeOriginMapBuilder& pcToCodeOriginMapBuilder() { return m_pcToCodeOriginMapBuilder; }

    VM& vm() { return m_graph.m_vm; }

    void emitRestoreCalleeSaves()
    {
        emitRestoreCalleeSavesFor(&RegisterAtOffsetList::dfgCalleeSaveRegisters());
    }

    void emitSaveCalleeSaves()
    {
        emitSaveCalleeSavesFor(&RegisterAtOffsetList::dfgCalleeSaveRegisters());
    }

    class LinkableConstant final : public CCallHelpers::ConstantMaterializer {
    public:
        enum NonCellTag { NonCell };
        LinkableConstant() = default;
        LinkableConstant(JITCompiler&, JSCell*);
        LinkableConstant(LinkerIR::Constant index)
            : m_index(index)
        { }

        void materialize(CCallHelpers&, GPRReg);
        void store(CCallHelpers&, CCallHelpers::Address);

        template<typename T>
        static LinkableConstant nonCellPointer(JITCompiler& jit, T* pointer)
        {
            static_assert(!std::is_base_of_v<JSCell, T>);
            return LinkableConstant(jit, pointer, NonCell);
        }

        static LinkableConstant structure(JITCompiler& jit, RegisteredStructure structure)
        {
            return LinkableConstant(jit, structure.get());
        }

        bool isUnlinked() const { return m_index != UINT_MAX; }

        void* pointer() const { return m_pointer; }
        LinkerIR::Constant index() const { return m_index; }

#if USE(JSVALUE64)
        Address unlinkedAddress()
        {
            ASSERT(isUnlinked());
            return Address(GPRInfo::constantsRegister, JITData::offsetOfData() + sizeof(void*) * m_index);
        }
#endif

    private:
        LinkableConstant(JITCompiler&, void*, NonCellTag);

        LinkerIR::Constant m_index { UINT_MAX };
        void* m_pointer { nullptr };
    };

    void loadConstant(LinkerIR::Constant, GPRReg);
    void loadLinkableConstant(LinkableConstant, GPRReg);
    void storeLinkableConstant(LinkableConstant, Address);

    Jump branchLinkableConstant(RelationalCondition cond, GPRReg left, LinkableConstant constant)
    {
#if USE(JSVALUE64)
        if (constant.isUnlinked())
            return CCallHelpers::branchPtr(cond, left, constant.unlinkedAddress());
#endif
        return CCallHelpers::branchPtr(cond, left, CCallHelpers::TrustedImmPtr(constant.pointer()));
    }

    Jump branchLinkableConstant(RelationalCondition cond, Address left, LinkableConstant constant)
    {
#if USE(JSVALUE64)
        if (constant.isUnlinked())
            return CCallHelpers::branchPtr(cond, left, constant.unlinkedAddress());
#endif
        return CCallHelpers::branchPtr(cond, left, CCallHelpers::TrustedImmPtr(constant.pointer()));
    }

    std::tuple<CompileTimeStructureStubInfo, LinkableConstant> addStructureStubInfo();
    std::tuple<CompileTimeCallLinkInfo, LinkableConstant> addCallLinkInfo(CodeOrigin);
    LinkerIR::Constant addToConstantPool(LinkerIR::Type, void*);

private:
    friend class OSRExitJumpPlaceholder;
    
    // Internal implementation to compile.
    void compileEntry();
    void compileSetupRegistersForEntry();
    void compileEntryExecutionFlag();
    void compileBody();
    void link(LinkBuffer&);
    
    void exitSpeculativeWithOSR(const OSRExit&, SpeculationRecovery*);
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
        JSCallRecord(Label slowPathStart, Label doneLocation, CompileTimeCallLinkInfo info)
            : slowPathStart(slowPathStart)
            , doneLocation(doneLocation)
            , info(info)
        {
        }
        
        Label slowPathStart;
        Label doneLocation;
        CompileTimeCallLinkInfo info;
    };
    
    struct JSDirectCallRecord {
        JSDirectCallRecord(Label slowPath, OptimizingCallLinkInfo* info)
            : slowPath(slowPath)
            , info(info)
        {
        }
        
        Label slowPath;
        OptimizingCallLinkInfo* info;
    };
    
    Vector<InlineCacheWrapper<JITGetByIdGenerator>, 4> m_getByIds;
    Vector<InlineCacheWrapper<JITGetByIdWithThisGenerator>, 4> m_getByIdsWithThis;
    Vector<InlineCacheWrapper<JITGetByValGenerator>, 4> m_getByVals;
    Vector<InlineCacheWrapper<JITGetByValWithThisGenerator>, 4> m_getByValsWithThis;
    Vector<InlineCacheWrapper<JITPutByIdGenerator>, 4> m_putByIds;
    Vector<InlineCacheWrapper<JITPutByValGenerator>, 4> m_putByVals;
    Vector<InlineCacheWrapper<JITDelByIdGenerator>, 4> m_delByIds;
    Vector<InlineCacheWrapper<JITDelByValGenerator>, 4> m_delByVals;
    Vector<InlineCacheWrapper<JITInByIdGenerator>, 4> m_inByIds;
    Vector<InlineCacheWrapper<JITInByValGenerator>, 4> m_inByVals;
    Vector<InlineCacheWrapper<JITInstanceOfGenerator>, 4> m_instanceOfs;
    Vector<InlineCacheWrapper<JITPrivateBrandAccessGenerator>, 4> m_privateBrandAccesses;
    Vector<JSCallRecord, 4> m_jsCalls;
    Vector<JSDirectCallRecord, 4> m_jsDirectCalls;
    SegmentedVector<OSRExitCompilationInfo, 4> m_exitCompilationInfo;
    Vector<Vector<Label>> m_exitSiteLabels;
    Vector<DFG::OSREntryData> m_osrEntry;
    Vector<DFG::OSRExit> m_osrExit;
    Vector<DFG::SpeculationRecovery> m_speculationRecovery;
    Vector<LinkerIR::Value> m_constantPool;
    HashMap<LinkerIR::Value, LinkerIR::Constant, LinkerIR::ValueHash, LinkerIR::ValueTraits> m_constantPoolMap;
    SegmentedVector<DFG::UnlinkedStructureStubInfo> m_unlinkedStubInfos;
    SegmentedVector<DFG::UnlinkedCallLinkInfo> m_unlinkedCallLinkInfos;
    
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
