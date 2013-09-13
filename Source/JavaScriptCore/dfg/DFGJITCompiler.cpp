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

#include "config.h"
#include "DFGJITCompiler.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGFailedFinalizer.h"
#include "DFGJITCode.h"
#include "DFGJITFinalizer.h"
#include "DFGOSRExitCompiler.h"
#include "DFGOperations.h"
#include "DFGRegisterBank.h"
#include "DFGSlowPathGenerator.h"
#include "DFGSpeculativeJIT.h"
#include "DFGThunks.h"
#include "JSCJSValueInlines.h"
#include "VM.h"
#include "LinkBuffer.h"

namespace JSC { namespace DFG {

JITCompiler::JITCompiler(Graph& dfg)
    : CCallHelpers(&dfg.m_vm, dfg.m_codeBlock)
    , m_graph(dfg)
    , m_jitCode(adoptRef(new JITCode()))
    , m_blockHeads(dfg.numBlocks())
    , m_currentCodeOriginIndex(0)
{
    if (shouldShowDisassembly() || m_graph.m_vm.m_perBytecodeProfiler)
        m_disassembler = adoptPtr(new Disassembler(dfg));
}

JITCompiler::~JITCompiler()
{
}

void JITCompiler::linkOSRExits()
{
    ASSERT(m_jitCode->osrExit.size() == m_exitCompilationInfo.size());
    if (m_graph.compilation()) {
        for (unsigned i = 0; i < m_jitCode->osrExit.size(); ++i) {
            OSRExit& exit = m_jitCode->osrExit[i];
            Vector<Label> labels;
            if (exit.m_watchpointIndex == std::numeric_limits<unsigned>::max()) {
                OSRExitCompilationInfo& info = m_exitCompilationInfo[i];
                for (unsigned j = 0; j < info.m_failureJumps.jumps().size(); ++j)
                    labels.append(info.m_failureJumps.jumps()[j].label());
            } else
                labels.append(m_jitCode->watchpoints[exit.m_watchpointIndex].sourceLabel());
            m_exitSiteLabels.append(labels);
        }
    }
    
    for (unsigned i = 0; i < m_jitCode->osrExit.size(); ++i) {
        OSRExit& exit = m_jitCode->osrExit[i];
        JumpList& failureJumps = m_exitCompilationInfo[i].m_failureJumps;
        ASSERT(failureJumps.empty() == (exit.m_watchpointIndex != std::numeric_limits<unsigned>::max()));
        if (exit.m_watchpointIndex == std::numeric_limits<unsigned>::max())
            failureJumps.link(this);
        else
            m_jitCode->watchpoints[exit.m_watchpointIndex].setDestination(label());
        jitAssertHasValidCallFrame();
        store32(TrustedImm32(i), &vm()->osrExitIndex);
        exit.setPatchableCodeOffset(patchableJump());
    }
}

void JITCompiler::compileEntry()
{
    // This code currently matches the old JIT. In the function header we need to
    // pop the return address (since we do not allow any recursion on the machine
    // stack), and perform a fast stack check.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56292
    // We'll need to convert the remaining cti_ style calls (specifically the stack
    // check) which will be dependent on stack layout. (We'd need to account for this in
    // both normal return code and when jumping to an exception handler).
    preserveReturnAddressAfterCall(GPRInfo::regT2);
    emitPutToCallFrameHeader(GPRInfo::regT2, JSStack::ReturnPC);
    emitPutImmediateToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);
}

void JITCompiler::compileBody()
{
    // We generate the speculative code path, followed by OSR exit code to return
    // to the old JIT code if speculations fail.

#if DFG_ENABLE(JIT_BREAK_ON_EVERY_FUNCTION)
    // Handy debug tool!
    breakpoint();
#endif
    
    bool compiledSpeculative = m_speculative->compile();
    ASSERT_UNUSED(compiledSpeculative, compiledSpeculative);
}

void JITCompiler::compileExceptionHandlers()
{
    // Iterate over the m_calls vector, checking for jumps to link.
    bool didLinkExceptionCheck = false;
    for (unsigned i = 0; i < m_exceptionChecks.size(); ++i) {
        Jump& exceptionCheck = m_exceptionChecks[i].m_exceptionCheck;
        if (exceptionCheck.isSet()) {
            exceptionCheck.link(this);
            didLinkExceptionCheck = true;
        }
    }

    // If any exception checks were linked, generate code to lookup a handler.
    if (didLinkExceptionCheck) {
        // lookupExceptionHandler is passed two arguments, exec (the CallFrame*), and
        // the index into the CodeBlock's callReturnIndexVector corresponding to the
        // call that threw the exception (this was set in nonPreservedNonReturnGPR, when
        // the exception check was planted).
        move(GPRInfo::nonPreservedNonReturnGPR, GPRInfo::argumentGPR1);
        move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
#if CPU(X86)
        // FIXME: should use the call abstraction, but this is currently in the SpeculativeJIT layer!
        poke(GPRInfo::argumentGPR0);
        poke(GPRInfo::argumentGPR1, 1);
#endif
        m_calls.append(CallLinkRecord(call(), lookupExceptionHandler));
        // lookupExceptionHandler leaves the handler CallFrame* in the returnValueGPR,
        // and the address of the handler in returnValueGPR2.
        jump(GPRInfo::returnValueGPR2);
    }
}

void JITCompiler::link(LinkBuffer& linkBuffer)
{
    // Link the code, populate data in CodeBlock data structures.
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("JIT code for %p start at [%p, %p). Size = %zu.\n", m_codeBlock, linkBuffer.debugAddress(), static_cast<char*>(linkBuffer.debugAddress()) + linkBuffer.debugSize(), linkBuffer.debugSize());
#endif

    BitVector usedJumpTables;
    for (unsigned i = m_graph.m_switchData.size(); i--;) {
        SwitchData& data = m_graph.m_switchData[i];
        if (!data.didUseJumpTable)
            continue;
        
        if (data.kind == SwitchString)
            continue;
        
        RELEASE_ASSERT(data.kind == SwitchImm || data.kind == SwitchChar);
        
        usedJumpTables.set(data.switchTableIndex);
        SimpleJumpTable& table = m_codeBlock->switchJumpTable(data.switchTableIndex);
        table.ctiDefault = linkBuffer.locationOf(m_blockHeads[data.fallThrough->index]);
        table.ctiOffsets.grow(table.branchOffsets.size());
        for (unsigned j = table.ctiOffsets.size(); j--;)
            table.ctiOffsets[j] = table.ctiDefault;
        for (unsigned j = data.cases.size(); j--;) {
            SwitchCase& myCase = data.cases[j];
            table.ctiOffsets[myCase.value.switchLookupValue() - table.min] =
                linkBuffer.locationOf(m_blockHeads[myCase.target->index]);
        }
    }
    
    for (unsigned i = m_codeBlock->numberOfSwitchJumpTables(); i--;) {
        if (usedJumpTables.get(i))
            continue;
        
        m_codeBlock->switchJumpTable(i).clear();
    }

    // NOTE: we cannot clear string switch tables because (1) we're running concurrently
    // and we cannot deref StringImpl's and (2) it would be weird to deref those
    // StringImpl's since we refer to them.
    for (unsigned i = m_graph.m_switchData.size(); i--;) {
        SwitchData& data = m_graph.m_switchData[i];
        if (!data.didUseJumpTable)
            continue;
        
        if (data.kind != SwitchString)
            continue;
        
        StringJumpTable& table = m_codeBlock->stringSwitchJumpTable(data.switchTableIndex);
        table.ctiDefault = linkBuffer.locationOf(m_blockHeads[data.fallThrough->index]);
        StringJumpTable::StringOffsetTable::iterator iter;
        StringJumpTable::StringOffsetTable::iterator end = table.offsetTable.end();
        for (iter = table.offsetTable.begin(); iter != end; ++iter)
            iter->value.ctiOffset = table.ctiDefault;
        for (unsigned j = data.cases.size(); j--;) {
            SwitchCase& myCase = data.cases[j];
            iter = table.offsetTable.find(myCase.value.stringImpl());
            RELEASE_ASSERT(iter != end);
            iter->value.ctiOffset = linkBuffer.locationOf(m_blockHeads[myCase.target->index]);
        }
    }

    // Link all calls out from the JIT code to their respective functions.
    for (unsigned i = 0; i < m_calls.size(); ++i)
        linkBuffer.link(m_calls[i].m_call, m_calls[i].m_function);

    m_codeBlock->callReturnIndexVector().reserveCapacity(m_exceptionChecks.size());
    for (unsigned i = 0; i < m_exceptionChecks.size(); ++i) {
        unsigned returnAddressOffset = linkBuffer.returnAddressOffset(m_exceptionChecks[i].m_call);
        CodeOrigin codeOrigin = m_exceptionChecks[i].m_codeOrigin;
        while (codeOrigin.inlineCallFrame)
            codeOrigin = codeOrigin.inlineCallFrame->caller;
        unsigned exceptionInfo = codeOrigin.bytecodeIndex;
        m_codeBlock->callReturnIndexVector().append(CallReturnOffsetToBytecodeOffset(returnAddressOffset, exceptionInfo));
    }

    Vector<CodeOrigin, 0, UnsafeVectorOverflow>& codeOrigins = m_codeBlock->codeOrigins();
    codeOrigins.resize(m_exceptionChecks.size());
    
    for (unsigned i = 0; i < m_exceptionChecks.size(); ++i) {
        CallExceptionRecord& record = m_exceptionChecks[i];
        codeOrigins[i] = record.m_codeOrigin;
    }
    
    m_codeBlock->setNumberOfStructureStubInfos(m_propertyAccesses.size() + m_ins.size());
    for (unsigned i = 0; i < m_propertyAccesses.size(); ++i) {
        StructureStubInfo& info = m_codeBlock->structureStubInfo(i);
        CodeLocationCall callReturnLocation = linkBuffer.locationOf(m_propertyAccesses[i].m_slowPathGenerator->call());
        info.codeOrigin = m_propertyAccesses[i].m_codeOrigin;
        info.callReturnLocation = callReturnLocation;
        info.patch.dfg.deltaCheckImmToCall = differenceBetweenCodePtr(linkBuffer.locationOf(m_propertyAccesses[i].m_structureImm), callReturnLocation);
        info.patch.dfg.deltaCallToStructCheck = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_structureCheck));
#if USE(JSVALUE64)
        info.patch.dfg.deltaCallToLoadOrStore = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_loadOrStore));
#else
        info.patch.dfg.deltaCallToTagLoadOrStore = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_tagLoadOrStore));
        info.patch.dfg.deltaCallToPayloadLoadOrStore = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_payloadLoadOrStore));
#endif
        info.patch.dfg.deltaCallToSlowCase = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_slowPathGenerator->label()));
        info.patch.dfg.deltaCallToDone = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_done));
        info.patch.dfg.deltaCallToStorageLoad = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_propertyStorageLoad));
        info.patch.dfg.baseGPR = m_propertyAccesses[i].m_baseGPR;
#if USE(JSVALUE64)
        info.patch.dfg.valueGPR = m_propertyAccesses[i].m_valueGPR;
#else
        info.patch.dfg.valueTagGPR = m_propertyAccesses[i].m_valueTagGPR;
        info.patch.dfg.valueGPR = m_propertyAccesses[i].m_valueGPR;
#endif
        m_propertyAccesses[i].m_usedRegisters.copyInfo(info.patch.dfg.usedRegisters);
        info.patch.dfg.registersFlushed = m_propertyAccesses[i].m_registerMode == PropertyAccessRecord::RegistersFlushed;
    }
    for (unsigned i = 0; i < m_ins.size(); ++i) {
        StructureStubInfo& info = m_codeBlock->structureStubInfo(m_propertyAccesses.size() + i);
        CodeLocationLabel jump = linkBuffer.locationOf(m_ins[i].m_jump);
        CodeLocationCall callReturnLocation = linkBuffer.locationOf(m_ins[i].m_slowPathGenerator->call());
        info.codeOrigin = m_ins[i].m_codeOrigin;
        info.hotPathBegin = jump;
        info.callReturnLocation = callReturnLocation;
        info.patch.dfg.deltaCallToSlowCase = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_ins[i].m_slowPathGenerator->label()));
        info.patch.dfg.baseGPR = m_ins[i].m_baseGPR;
        info.patch.dfg.valueGPR = m_ins[i].m_resultGPR;
        m_ins[i].m_usedRegisters.copyInfo(info.patch.dfg.usedRegisters);
        info.patch.dfg.registersFlushed = false;
    }
    m_codeBlock->sortStructureStubInfos();
    
    m_codeBlock->setNumberOfCallLinkInfos(m_jsCalls.size());
    for (unsigned i = 0; i < m_jsCalls.size(); ++i) {
        CallLinkInfo& info = m_codeBlock->callLinkInfo(i);
        info.callType = m_jsCalls[i].m_callType;
        info.isDFG = true;
        info.codeOrigin = m_jsCalls[i].m_codeOrigin;
        linkBuffer.link(m_jsCalls[i].m_slowCall, FunctionPtr((m_vm->getCTIStub(info.callType == CallLinkInfo::Construct ? linkConstructThunkGenerator : linkCallThunkGenerator)).code().executableAddress()));
        info.callReturnLocation = linkBuffer.locationOfNearCall(m_jsCalls[i].m_slowCall);
        info.hotPathBegin = linkBuffer.locationOf(m_jsCalls[i].m_targetToCheck);
        info.hotPathOther = linkBuffer.locationOfNearCall(m_jsCalls[i].m_fastCall);
        info.calleeGPR = static_cast<unsigned>(m_jsCalls[i].m_callee);
    }
    
    MacroAssemblerCodeRef osrExitThunk = vm()->getCTIStub(osrExitGenerationThunkGenerator);
    CodeLocationLabel target = CodeLocationLabel(osrExitThunk.code());
    for (unsigned i = 0; i < m_jitCode->osrExit.size(); ++i) {
        OSRExit& exit = m_jitCode->osrExit[i];
        linkBuffer.link(exit.getPatchableCodeOffsetAsJump(), target);
        exit.correctJump(linkBuffer);
        if (exit.m_watchpointIndex != std::numeric_limits<unsigned>::max())
            m_jitCode->watchpoints[exit.m_watchpointIndex].correctLabels(linkBuffer);
    }
    
    if (m_graph.compilation()) {
        ASSERT(m_exitSiteLabels.size() == m_jitCode->osrExit.size());
        for (unsigned i = 0; i < m_exitSiteLabels.size(); ++i) {
            Vector<Label>& labels = m_exitSiteLabels[i];
            Vector<const void*> addresses;
            for (unsigned j = 0; j < labels.size(); ++j)
                addresses.append(linkBuffer.locationOf(labels[j]).executableAddress());
            m_graph.compilation()->addOSRExitSite(addresses);
        }
    } else
        ASSERT(!m_exitSiteLabels.size());
    
    m_jitCode->common.compilation = m_graph.compilation();
    
}

void JITCompiler::compile()
{
    SamplingRegion samplingRegion("DFG Backend");

    setStartOfCode();
    compileEntry();
    m_speculative = adoptPtr(new SpeculativeJIT(*this));
    compileBody();
    setEndOfMainPath();

    // Generate slow path code.
    m_speculative->runSlowPathGenerators();
    
    compileExceptionHandlers();
    linkOSRExits();
    
    // Create OSR entry trampolines if necessary.
    m_speculative->createOSREntries();
    setEndOfCode();
}

void JITCompiler::link()
{
    OwnPtr<LinkBuffer> linkBuffer = adoptPtr(new LinkBuffer(*m_vm, this, m_codeBlock, JITCompilationCanFail));
    if (linkBuffer->didFailToAllocate()) {
        m_graph.m_plan.finalizer = adoptPtr(new FailedFinalizer(m_graph.m_plan));
        return;
    }
    
    link(*linkBuffer);
    m_speculative->linkOSREntries(*linkBuffer);
    
    m_jitCode->shrinkToFit();
    codeBlock()->shrinkToFit(CodeBlock::LateShrink);

    disassemble(*linkBuffer);
    
    m_graph.m_plan.finalizer = adoptPtr(new JITFinalizer(
        m_graph.m_plan, m_jitCode.release(), linkBuffer.release()));
}

void JITCompiler::compileFunction()
{
    SamplingRegion samplingRegion("DFG Backend");
    
    setStartOfCode();
    compileEntry();

    // === Function header code generation ===
    // This is the main entry point, without performing an arity check.
    // If we needed to perform an arity check we will already have moved the return address,
    // so enter after this.
    Label fromArityCheck(this);
    // Plant a check that sufficient space is available in the JSStack.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56291
    addPtr(TrustedImm32(-m_codeBlock->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister, GPRInfo::regT1);
    Jump stackCheck = branchPtr(Above, AbsoluteAddress(m_vm->interpreter->stack().addressOfEnd()), GPRInfo::regT1);
    // Return here after stack check.
    Label fromStackCheck = label();


    // === Function body code generation ===
    m_speculative = adoptPtr(new SpeculativeJIT(*this));
    compileBody();
    setEndOfMainPath();

    // === Function footer code generation ===
    //
    // Generate code to perform the slow stack check (if the fast one in
    // the function header fails), and generate the entry point with arity check.
    //
    // Generate the stack check; if the fast check in the function head fails,
    // we need to call out to a helper function to check whether more space is available.
    // FIXME: change this from a cti call to a DFG style operation (normal C calling conventions).
    stackCheck.link(this);
    move(stackPointerRegister, GPRInfo::argumentGPR0);
    poke(GPRInfo::callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));

    CallBeginToken token;
    beginCall(CodeOrigin(0), token);
    m_callStackCheck = call();
    notifyCall(m_callStackCheck, CodeOrigin(0), token);
    jump(fromStackCheck);
    
    // The fast entry point into a function does not check the correct number of arguments
    // have been passed to the call (we only use the fast entry point where we can statically
    // determine the correct number of arguments have been passed, or have already checked).
    // In cases where an arity check is necessary, we enter here.
    // FIXME: change this from a cti call to a DFG style operation (normal C calling conventions).
    m_arityCheck = label();
    compileEntry();

    load32(AssemblyHelpers::payloadFor((VirtualRegister)JSStack::ArgumentCount), GPRInfo::regT1);
    branch32(AboveOrEqual, GPRInfo::regT1, TrustedImm32(m_codeBlock->numParameters())).linkTo(fromArityCheck, this);
    move(stackPointerRegister, GPRInfo::argumentGPR0);
    poke(GPRInfo::callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));
    beginCall(CodeOrigin(0), token);
    m_callArityCheck = call();
    notifyCall(m_callArityCheck, CodeOrigin(0), token);
    branchTest32(Zero, GPRInfo::regT0).linkTo(fromArityCheck, this);
    beginCall(CodeOrigin(0), token);
    m_callArityFixup = call();
    notifyCall(m_callArityFixup, CodeOrigin(0), token);
    jump(fromArityCheck);
    
    // Generate slow path code.
    m_speculative->runSlowPathGenerators();
    
    compileExceptionHandlers();
    linkOSRExits();
    
    // Create OSR entry trampolines if necessary.
    m_speculative->createOSREntries();
    setEndOfCode();
}

void JITCompiler::linkFunction()
{
    // === Link ===
    OwnPtr<LinkBuffer> linkBuffer = adoptPtr(new LinkBuffer(*m_vm, this, m_codeBlock, JITCompilationCanFail));
    if (linkBuffer->didFailToAllocate()) {
        m_graph.m_plan.finalizer = adoptPtr(new FailedFinalizer(m_graph.m_plan));
        return;
    }
    link(*linkBuffer);
    m_speculative->linkOSREntries(*linkBuffer);
    
    m_jitCode->shrinkToFit();
    codeBlock()->shrinkToFit(CodeBlock::LateShrink);
    
    // FIXME: switch the stack check & arity check over to DFGOpertaion style calls, not JIT stubs.
    linkBuffer->link(m_callStackCheck, cti_stack_check);
    linkBuffer->link(m_callArityCheck, m_codeBlock->m_isConstructor ? cti_op_construct_arityCheck : cti_op_call_arityCheck);
    linkBuffer->link(m_callArityFixup, FunctionPtr((m_vm->getCTIStub(arityFixup)).code().executableAddress()));
    
    disassemble(*linkBuffer);
    
    m_graph.m_plan.finalizer = adoptPtr(new JITFinalizer(
        m_graph.m_plan, m_jitCode.release(), linkBuffer.release(), m_arityCheck));
}

void JITCompiler::disassemble(LinkBuffer& linkBuffer)
{
    if (shouldShowDisassembly())
        m_disassembler->dump(linkBuffer);
    
    if (m_graph.m_plan.compilation)
        m_disassembler->reportToProfiler(m_graph.m_plan.compilation.get(), linkBuffer);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
