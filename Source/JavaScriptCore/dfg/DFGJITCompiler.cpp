/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#include "DFGInlineCacheWrapperInlines.h"
#include "DFGJITCode.h"
#include "DFGJITFinalizer.h"
#include "DFGOSRExit.h"
#include "DFGOperations.h"
#include "DFGRegisterBank.h"
#include "DFGSlowPathGenerator.h"
#include "DFGSpeculativeJIT.h"
#include "DFGThunks.h"
#include "JSCInlines.h"
#include "JSCJSValueInlines.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "StructureStubInfo.h"
#include "ThunkGenerators.h"
#include "VM.h"

namespace JSC { namespace DFG {

JITCompiler::JITCompiler(Graph& dfg)
    : CCallHelpers(dfg.m_codeBlock)
    , m_graph(dfg)
    , m_jitCode(adoptRef(new JITCode()))
    , m_blockHeads(dfg.numBlocks())
    , m_pcToCodeOriginMapBuilder(dfg.m_vm)
{
    if (UNLIKELY(shouldDumpDisassembly() || m_graph.m_vm.m_perBytecodeProfiler))
        m_disassembler = std::make_unique<Disassembler>(dfg);
#if ENABLE(FTL_JIT)
    m_jitCode->tierUpInLoopHierarchy = WTFMove(m_graph.m_plan.tierUpInLoopHierarchy());
    for (unsigned tierUpBytecode : m_graph.m_plan.tierUpAndOSREnterBytecodes())
        m_jitCode->tierUpEntryTriggers.add(tierUpBytecode, JITCode::TriggerReason::DontTrigger);
#endif
}

JITCompiler::~JITCompiler()
{
}

void JITCompiler::linkOSRExits()
{
    ASSERT(m_jitCode->osrExit.size() == m_exitCompilationInfo.size());
    if (UNLIKELY(m_graph.compilation())) {
        for (unsigned i = 0; i < m_jitCode->osrExit.size(); ++i) {
            OSRExitCompilationInfo& info = m_exitCompilationInfo[i];
            Vector<Label> labels;
            if (!info.m_failureJumps.empty()) {
                for (unsigned j = 0; j < info.m_failureJumps.jumps().size(); ++j)
                    labels.append(info.m_failureJumps.jumps()[j].label());
            } else
                labels.append(info.m_replacementSource);
            m_exitSiteLabels.append(labels);
        }
    }
    
    MacroAssemblerCodeRef<JITThunkPtrTag> osrExitThunk = vm()->getCTIStub(osrExitThunkGenerator);
    auto osrExitThunkLabel = CodeLocationLabel<JITThunkPtrTag>(osrExitThunk.code());
    for (unsigned i = 0; i < m_jitCode->osrExit.size(); ++i) {
        OSRExitCompilationInfo& info = m_exitCompilationInfo[i];
        JumpList& failureJumps = info.m_failureJumps;
        if (!failureJumps.empty())
            failureJumps.link(this);
        else
            info.m_replacementDestination = label();

        jitAssertHasValidCallFrame();
        store32(TrustedImm32(i), &vm()->osrExitIndex);
        if (Options::useProbeOSRExit()) {
            Jump target = jump();
            addLinkTask([target, osrExitThunkLabel] (LinkBuffer& linkBuffer) {
                linkBuffer.link(target, osrExitThunkLabel);
            });
        } else
            info.m_patchableJump = patchableJump();
    }
}

void JITCompiler::compileEntry()
{
    // This code currently matches the old JIT. In the function header we need to
    // save return address and call frame via the prologue and perform a fast stack check.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56292
    // We'll need to convert the remaining cti_ style calls (specifically the stack
    // check) which will be dependent on stack layout. (We'd need to account for this in
    // both normal return code and when jumping to an exception handler).
    emitFunctionPrologue();
    emitPutToCallFrameHeader(m_codeBlock, CallFrameSlot::codeBlock);
}

void JITCompiler::compileSetupRegistersForEntry()
{
    emitSaveCalleeSaves();
    emitMaterializeTagCheckRegisters();    
}

void JITCompiler::compileEntryExecutionFlag()
{
#if ENABLE(FTL_JIT)
    if (m_graph.m_plan.canTierUpAndOSREnter())
        store8(TrustedImm32(0), &m_jitCode->neverExecutedEntry);
#endif // ENABLE(FTL_JIT)
}

void JITCompiler::compileBody()
{
    // We generate the speculative code path, followed by OSR exit code to return
    // to the old JIT code if speculations fail.

    bool compiledSpeculative = m_speculative->compile();
    ASSERT_UNUSED(compiledSpeculative, compiledSpeculative);
}

void JITCompiler::compileExceptionHandlers()
{
    if (!m_exceptionChecksWithCallFrameRollback.empty()) {
        m_exceptionChecksWithCallFrameRollback.link(this);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm()->topEntryFrame);

        // lookupExceptionHandlerFromCallerFrame is passed two arguments, the VM and the exec (the CallFrame*).
        move(TrustedImmPtr(vm()), GPRInfo::argumentGPR0);
        move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
        addPtr(TrustedImm32(m_graph.stackPointerOffset() * sizeof(Register)), GPRInfo::callFrameRegister, stackPointerRegister);

#if CPU(X86)
        // FIXME: should use the call abstraction, but this is currently in the SpeculativeJIT layer!
        poke(GPRInfo::argumentGPR0);
        poke(GPRInfo::argumentGPR1, 1);
#endif
        m_calls.append(CallLinkRecord(call(OperationPtrTag), FunctionPtr<OperationPtrTag>(lookupExceptionHandlerFromCallerFrame)));

        jumpToExceptionHandler(*vm());
    }

    if (!m_exceptionChecks.empty()) {
        m_exceptionChecks.link(this);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm()->topEntryFrame);

        // lookupExceptionHandler is passed two arguments, the VM and the exec (the CallFrame*).
        move(TrustedImmPtr(vm()), GPRInfo::argumentGPR0);
        move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);

#if CPU(X86)
        // FIXME: should use the call abstraction, but this is currently in the SpeculativeJIT layer!
        poke(GPRInfo::argumentGPR0);
        poke(GPRInfo::argumentGPR1, 1);
#endif
        m_calls.append(CallLinkRecord(call(OperationPtrTag), FunctionPtr<OperationPtrTag>(lookupExceptionHandler)));

        jumpToExceptionHandler(*vm());
    }
}

void JITCompiler::link(LinkBuffer& linkBuffer)
{
    // Link the code, populate data in CodeBlock data structures.
    m_jitCode->common.frameRegisterCount = m_graph.frameRegisterCount();
    m_jitCode->common.requiredRegisterCountForExit = m_graph.requiredRegisterCountForExit();

    if (!m_graph.m_plan.inlineCallFrames()->isEmpty())
        m_jitCode->common.inlineCallFrames = m_graph.m_plan.inlineCallFrames();

#if USE(JSVALUE32_64)
    m_jitCode->common.doubleConstants = WTFMove(m_graph.m_doubleConstants);
#endif
    
    m_graph.registerFrozenValues();

    BitVector usedJumpTables;
    for (Bag<SwitchData>::iterator iter = m_graph.m_switchData.begin(); !!iter; ++iter) {
        SwitchData& data = **iter;
        if (!data.didUseJumpTable)
            continue;
        
        if (data.kind == SwitchString)
            continue;
        
        RELEASE_ASSERT(data.kind == SwitchImm || data.kind == SwitchChar);
        
        usedJumpTables.set(data.switchTableIndex);
        SimpleJumpTable& table = m_codeBlock->switchJumpTable(data.switchTableIndex);
        table.ctiDefault = linkBuffer.locationOf<JSSwitchPtrTag>(m_blockHeads[data.fallThrough.block->index]);
        table.ctiOffsets.grow(table.branchOffsets.size());
        for (unsigned j = table.ctiOffsets.size(); j--;)
            table.ctiOffsets[j] = table.ctiDefault;
        for (unsigned j = data.cases.size(); j--;) {
            SwitchCase& myCase = data.cases[j];
            table.ctiOffsets[myCase.value.switchLookupValue(data.kind) - table.min] =
                linkBuffer.locationOf<JSSwitchPtrTag>(m_blockHeads[myCase.target.block->index]);
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
    for (Bag<SwitchData>::iterator switchDataIter = m_graph.m_switchData.begin(); !!switchDataIter; ++switchDataIter) {
        SwitchData& data = **switchDataIter;
        if (!data.didUseJumpTable)
            continue;
        
        if (data.kind != SwitchString)
            continue;
        
        StringJumpTable& table = m_codeBlock->stringSwitchJumpTable(data.switchTableIndex);

        table.ctiDefault = linkBuffer.locationOf<JSSwitchPtrTag>(m_blockHeads[data.fallThrough.block->index]);
        StringJumpTable::StringOffsetTable::iterator iter;
        StringJumpTable::StringOffsetTable::iterator end = table.offsetTable.end();
        for (iter = table.offsetTable.begin(); iter != end; ++iter)
            iter->value.ctiOffset = table.ctiDefault;
        for (unsigned j = data.cases.size(); j--;) {
            SwitchCase& myCase = data.cases[j];
            iter = table.offsetTable.find(myCase.value.stringImpl());
            RELEASE_ASSERT(iter != end);
            iter->value.ctiOffset = linkBuffer.locationOf<JSSwitchPtrTag>(m_blockHeads[myCase.target.block->index]);
        }
    }

    // Link all calls out from the JIT code to their respective functions.
    for (unsigned i = 0; i < m_calls.size(); ++i)
        linkBuffer.link(m_calls[i].m_call, m_calls[i].m_function);

    finalizeInlineCaches(m_getByIds, linkBuffer);
    finalizeInlineCaches(m_getByIdsWithThis, linkBuffer);
    finalizeInlineCaches(m_putByIds, linkBuffer);
    finalizeInlineCaches(m_inByIds, linkBuffer);
    finalizeInlineCaches(m_instanceOfs, linkBuffer);

    auto linkCallThunk = FunctionPtr<NoPtrTag>(vm()->getCTIStub(linkCallThunkGenerator).retaggedCode<NoPtrTag>());
    for (auto& record : m_jsCalls) {
        CallLinkInfo& info = *record.info;
        linkBuffer.link(record.slowCall, linkCallThunk);
        info.setCallLocations(
            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOfNearCall<JSInternalPtrTag>(record.slowCall)),
            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOf<JSInternalPtrTag>(record.targetToCheck)),
            linkBuffer.locationOfNearCall<JSInternalPtrTag>(record.fastCall));
    }
    
    for (JSDirectCallRecord& record : m_jsDirectCalls) {
        CallLinkInfo& info = *record.info;
        linkBuffer.link(record.call, linkBuffer.locationOf<NoPtrTag>(record.slowPath));
        info.setCallLocations(
            CodeLocationLabel<JSInternalPtrTag>(),
            linkBuffer.locationOf<JSInternalPtrTag>(record.slowPath),
            linkBuffer.locationOfNearCall<JSInternalPtrTag>(record.call));
    }
    
    for (JSDirectTailCallRecord& record : m_jsDirectTailCalls) {
        CallLinkInfo& info = *record.info;
        info.setCallLocations(
            linkBuffer.locationOf<JSInternalPtrTag>(record.patchableJump),
            linkBuffer.locationOf<JSInternalPtrTag>(record.slowPath),
            linkBuffer.locationOfNearCall<JSInternalPtrTag>(record.call));
    }
    
    MacroAssemblerCodeRef<JITThunkPtrTag> osrExitThunk = vm()->getCTIStub(osrExitGenerationThunkGenerator);
    auto target = CodeLocationLabel<JITThunkPtrTag>(osrExitThunk.code());
    for (unsigned i = 0; i < m_jitCode->osrExit.size(); ++i) {
        OSRExitCompilationInfo& info = m_exitCompilationInfo[i];
        if (!Options::useProbeOSRExit()) {
            linkBuffer.link(info.m_patchableJump.m_jump, target);
            OSRExit& exit = m_jitCode->osrExit[i];
            exit.m_patchableJumpLocation = linkBuffer.locationOf<JSInternalPtrTag>(info.m_patchableJump);
        }
        if (info.m_replacementSource.isSet()) {
            m_jitCode->common.jumpReplacements.append(JumpReplacement(
                linkBuffer.locationOf<JSInternalPtrTag>(info.m_replacementSource),
                linkBuffer.locationOf<OSRExitPtrTag>(info.m_replacementDestination)));
        }
    }
    
    if (UNLIKELY(m_graph.compilation())) {
        ASSERT(m_exitSiteLabels.size() == m_jitCode->osrExit.size());
        for (unsigned i = 0; i < m_exitSiteLabels.size(); ++i) {
            Vector<Label>& labels = m_exitSiteLabels[i];
            Vector<MacroAssemblerCodePtr<JSInternalPtrTag>> addresses;
            for (unsigned j = 0; j < labels.size(); ++j)
                addresses.append(linkBuffer.locationOf<JSInternalPtrTag>(labels[j]));
            m_graph.compilation()->addOSRExitSite(addresses);
        }
    } else
        ASSERT(!m_exitSiteLabels.size());

    m_jitCode->common.compilation = m_graph.compilation();
    
    // Link new DFG exception handlers and remove baseline JIT handlers.
    m_codeBlock->clearExceptionHandlers();
    for (unsigned  i = 0; i < m_exceptionHandlerOSRExitCallSites.size(); i++) {
        OSRExitCompilationInfo& info = m_exceptionHandlerOSRExitCallSites[i].exitInfo;
        if (info.m_replacementDestination.isSet()) {
            // If this is is *not* set, it means that we already jumped to the OSR exit in pure generated control flow.
            // i.e, we explicitly emitted an exceptionCheck that we know will be caught in this machine frame.
            // If this *is set*, it means we will be landing at this code location from genericUnwind from an
            // exception thrown in a child call frame.
            CodeLocationLabel<ExceptionHandlerPtrTag> catchLabel = linkBuffer.locationOf<ExceptionHandlerPtrTag>(info.m_replacementDestination);
            HandlerInfo newExceptionHandler = m_exceptionHandlerOSRExitCallSites[i].baselineExceptionHandler;
            CallSiteIndex callSite = m_exceptionHandlerOSRExitCallSites[i].callSiteIndex;
            newExceptionHandler.start = callSite.bits();
            newExceptionHandler.end = callSite.bits() + 1;
            newExceptionHandler.nativeCode = catchLabel;
            m_codeBlock->appendExceptionHandler(newExceptionHandler);
        }
    }

    if (m_pcToCodeOriginMapBuilder.didBuildMapping())
        m_codeBlock->setPCToCodeOriginMap(std::make_unique<PCToCodeOriginMap>(WTFMove(m_pcToCodeOriginMapBuilder), linkBuffer));
}

static void emitStackOverflowCheck(JITCompiler& jit, MacroAssembler::JumpList& stackOverflow)
{
    int frameTopOffset = virtualRegisterForLocal(jit.graph().requiredRegisterCountForExecutionAndExit() - 1).offset() * sizeof(Register);
    unsigned maxFrameSize = -frameTopOffset;

    jit.addPtr(MacroAssembler::TrustedImm32(frameTopOffset), GPRInfo::callFrameRegister, GPRInfo::regT1);
    if (UNLIKELY(maxFrameSize > Options::reservedZoneSize()))
        stackOverflow.append(jit.branchPtr(MacroAssembler::Above, GPRInfo::regT1, GPRInfo::callFrameRegister));
    stackOverflow.append(jit.branchPtr(MacroAssembler::Above, MacroAssembler::AbsoluteAddress(jit.vm()->addressOfSoftStackLimit()), GPRInfo::regT1));
}

void JITCompiler::compile()
{
    makeCatchOSREntryBuffer();

    setStartOfCode();
    compileEntry();
    m_speculative = std::make_unique<SpeculativeJIT>(*this);

    // Plant a check that sufficient space is available in the JSStack.
    JumpList stackOverflow;
    emitStackOverflowCheck(*this, stackOverflow);

    addPtr(TrustedImm32(-(m_graph.frameRegisterCount() * sizeof(Register))), GPRInfo::callFrameRegister, stackPointerRegister);
    if (Options::zeroStackFrame())
        clearStackFrame(GPRInfo::callFrameRegister, stackPointerRegister, GPRInfo::regT0, m_graph.frameRegisterCount() * sizeof(Register));
    checkStackPointerAlignment();
    compileSetupRegistersForEntry();
    compileEntryExecutionFlag();
    compileBody();
    setEndOfMainPath();

    // === Footer code generation ===
    //
    // Generate the stack overflow handling; if the stack check in the entry head fails,
    // we need to call out to a helper function to throw the StackOverflowError.
    stackOverflow.link(this);

    emitStoreCodeOrigin(CodeOrigin(0));

    if (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);

    m_speculative->callOperationWithCallFrameRollbackOnException(operationThrowStackOverflowError, m_codeBlock);

    // Generate slow path code.
    m_speculative->runSlowPathGenerators(m_pcToCodeOriginMapBuilder);
    m_pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), PCToCodeOriginMapBuilder::defaultCodeOrigin());
    
    compileExceptionHandlers();
    linkOSRExits();
    
    // Create OSR entry trampolines if necessary.
    m_speculative->createOSREntries();
    setEndOfCode();

    auto linkBuffer = std::make_unique<LinkBuffer>(*this, m_codeBlock, JITCompilationCanFail);
    if (linkBuffer->didFailToAllocate()) {
        m_graph.m_plan.setFinalizer(std::make_unique<FailedFinalizer>(m_graph.m_plan));
        return;
    }
    
    link(*linkBuffer);
    m_speculative->linkOSREntries(*linkBuffer);

    m_jitCode->shrinkToFit();
    codeBlock()->shrinkToFit(CodeBlock::LateShrink);

    disassemble(*linkBuffer);

    m_graph.m_plan.setFinalizer(std::make_unique<JITFinalizer>(
        m_graph.m_plan, m_jitCode.releaseNonNull(), WTFMove(linkBuffer)));
}

void JITCompiler::compileFunction()
{
    makeCatchOSREntryBuffer();

    setStartOfCode();
    Label entryLabel(this);
    compileEntry();

    // === Function header code generation ===
    // This is the main entry point, without performing an arity check.
    // If we needed to perform an arity check we will already have moved the return address,
    // so enter after this.
    Label fromArityCheck(this);
    // Plant a check that sufficient space is available in the JSStack.
    JumpList stackOverflow;
    emitStackOverflowCheck(*this, stackOverflow);

    // Move the stack pointer down to accommodate locals
    addPtr(TrustedImm32(-(m_graph.frameRegisterCount() * sizeof(Register))), GPRInfo::callFrameRegister, stackPointerRegister);
    if (Options::zeroStackFrame())
        clearStackFrame(GPRInfo::callFrameRegister, stackPointerRegister, GPRInfo::regT0, m_graph.frameRegisterCount() * sizeof(Register));
    checkStackPointerAlignment();

    compileSetupRegistersForEntry();
    compileEntryExecutionFlag();

    // === Function body code generation ===
    m_speculative = std::make_unique<SpeculativeJIT>(*this);
    compileBody();
    setEndOfMainPath();

    // === Function footer code generation ===
    //
    // Generate code to perform the stack overflow handling (if the stack check in
    // the function header fails), and generate the entry point with arity check.
    //
    // Generate the stack overflow handling; if the stack check in the function head fails,
    // we need to call out to a helper function to throw the StackOverflowError.
    stackOverflow.link(this);

    emitStoreCodeOrigin(CodeOrigin(0));

    if (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);

    m_speculative->callOperationWithCallFrameRollbackOnException(operationThrowStackOverflowError, m_codeBlock);
    
    // The fast entry point into a function does not check the correct number of arguments
    // have been passed to the call (we only use the fast entry point where we can statically
    // determine the correct number of arguments have been passed, or have already checked).
    // In cases where an arity check is necessary, we enter here.
    // FIXME: change this from a cti call to a DFG style operation (normal C calling conventions).
    Call callArityFixup;
    Label arityCheck;
    bool requiresArityFixup = m_codeBlock->numParameters() != 1;
    if (requiresArityFixup) {
        arityCheck = label();
        compileEntry();

        load32(AssemblyHelpers::payloadFor((VirtualRegister)CallFrameSlot::argumentCount), GPRInfo::regT1);
        branch32(AboveOrEqual, GPRInfo::regT1, TrustedImm32(m_codeBlock->numParameters())).linkTo(fromArityCheck, this);
        emitStoreCodeOrigin(CodeOrigin(0));
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);
        m_speculative->callOperationWithCallFrameRollbackOnException(m_codeBlock->isConstructor() ? operationConstructArityCheck : operationCallArityCheck, GPRInfo::regT0);
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(maxFrameExtentForSlowPathCall), stackPointerRegister);
        branchTest32(Zero, GPRInfo::returnValueGPR).linkTo(fromArityCheck, this);
        emitStoreCodeOrigin(CodeOrigin(0));
        move(GPRInfo::returnValueGPR, GPRInfo::argumentGPR0);
        callArityFixup = nearCall();
        jump(fromArityCheck);
    } else
        arityCheck = entryLabel;

    // Generate slow path code.
    m_speculative->runSlowPathGenerators(m_pcToCodeOriginMapBuilder);
    m_pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), PCToCodeOriginMapBuilder::defaultCodeOrigin());
    
    compileExceptionHandlers();
    linkOSRExits();
    
    // Create OSR entry trampolines if necessary.
    m_speculative->createOSREntries();
    setEndOfCode();

    // === Link ===
    auto linkBuffer = std::make_unique<LinkBuffer>(*this, m_codeBlock, JITCompilationCanFail);
    if (linkBuffer->didFailToAllocate()) {
        m_graph.m_plan.setFinalizer(std::make_unique<FailedFinalizer>(m_graph.m_plan));
        return;
    }
    link(*linkBuffer);
    m_speculative->linkOSREntries(*linkBuffer);
    
    m_jitCode->shrinkToFit();
    codeBlock()->shrinkToFit(CodeBlock::LateShrink);

    if (requiresArityFixup)
        linkBuffer->link(callArityFixup, FunctionPtr<JITThunkPtrTag>(vm()->getCTIStub(arityFixupGenerator).code()));

    disassemble(*linkBuffer);

    MacroAssemblerCodePtr<JSEntryPtrTag> withArityCheck = linkBuffer->locationOf<JSEntryPtrTag>(arityCheck);

    m_graph.m_plan.setFinalizer(std::make_unique<JITFinalizer>(
        m_graph.m_plan, m_jitCode.releaseNonNull(), WTFMove(linkBuffer), withArityCheck));
}

void JITCompiler::disassemble(LinkBuffer& linkBuffer)
{
    if (shouldDumpDisassembly()) {
        m_disassembler->dump(linkBuffer);
        linkBuffer.didAlreadyDisassemble();
    }

    if (UNLIKELY(m_graph.m_plan.compilation()))
        m_disassembler->reportToProfiler(m_graph.m_plan.compilation(), linkBuffer);
}

#if USE(JSVALUE32_64)
void* JITCompiler::addressOfDoubleConstant(Node* node)
{
    double value = node->asNumber();
    int64_t valueBits = bitwise_cast<int64_t>(value);
    auto it = m_graph.m_doubleConstantsMap.find(valueBits);
    if (it != m_graph.m_doubleConstantsMap.end())
        return it->second;

    if (!m_graph.m_doubleConstants)
        m_graph.m_doubleConstants = std::make_unique<Bag<double>>();

    double* addressInConstantPool = m_graph.m_doubleConstants->add();
    *addressInConstantPool = value;
    m_graph.m_doubleConstantsMap[valueBits] = addressInConstantPool;
    return addressInConstantPool;
}
#endif

void JITCompiler::noticeCatchEntrypoint(BasicBlock& basicBlock, JITCompiler::Label blockHead, LinkBuffer& linkBuffer, Vector<FlushFormat>&& argumentFormats)
{
    RELEASE_ASSERT(basicBlock.isCatchEntrypoint);
    RELEASE_ASSERT(basicBlock.intersectionOfCFAHasVisited); // An entrypoint is reachable by definition.
    m_jitCode->common.appendCatchEntrypoint(basicBlock.bytecodeBegin, linkBuffer.locationOf<ExceptionHandlerPtrTag>(blockHead), WTFMove(argumentFormats));
}

void JITCompiler::noticeOSREntry(BasicBlock& basicBlock, JITCompiler::Label blockHead, LinkBuffer& linkBuffer)
{
    RELEASE_ASSERT(!basicBlock.isCatchEntrypoint);

    // OSR entry is not allowed into blocks deemed unreachable by control flow analysis.
    if (!basicBlock.intersectionOfCFAHasVisited)
        return;

    OSREntryData* entry = m_jitCode->appendOSREntryData(basicBlock.bytecodeBegin, linkBuffer.locationOf<OSREntryPtrTag>(blockHead));

    entry->m_expectedValues = basicBlock.intersectionOfPastValuesAtHead;
        
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
                entry->m_localsForcedAnyInt.set(local);
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

void JITCompiler::appendExceptionHandlingOSRExit(ExitKind kind, unsigned eventStreamIndex, CodeOrigin opCatchOrigin, HandlerInfo* exceptionHandler, CallSiteIndex callSite, MacroAssembler::JumpList jumpsToFail)
{
    OSRExit exit(kind, JSValueRegs(), MethodOfGettingAValueProfile(), m_speculative.get(), eventStreamIndex);
    exit.m_codeOrigin = opCatchOrigin;
    exit.m_exceptionHandlerCallSiteIndex = callSite;
    OSRExitCompilationInfo& exitInfo = appendExitInfo(jumpsToFail);
    jitCode()->appendOSRExit(exit);
    m_exceptionHandlerOSRExitCallSites.append(ExceptionHandlingOSRExitInfo { exitInfo, *exceptionHandler, callSite });
}

void JITCompiler::exceptionCheck()
{
    // It's important that we use origin.forExit here. Consider if we hoist string
    // addition outside a loop, and that we exit at the point of that concatenation
    // from an out of memory exception.
    // If the original loop had a try/catch around string concatenation, if we "catch"
    // that exception inside the loop, then the loops induction variable will be undefined 
    // in the OSR exit value recovery. It's more defensible for the string concatenation, 
    // then, to not be caught by the for loops' try/catch.
    // Here is the program I'm speaking about:
    //
    // >>>> lets presume "c = a + b" gets hoisted here.
    // for (var i = 0; i < length; i++) {
    //     try {
    //         c = a + b
    //     } catch(e) { 
    //         If we threw an out of memory error, and we cought the exception
    //         right here, then "i" would almost certainly be undefined, which
    //         would make no sense.
    //         ... 
    //     }
    // }
    CodeOrigin opCatchOrigin;
    HandlerInfo* exceptionHandler;
    bool willCatchException = m_graph.willCatchExceptionInMachineFrame(m_speculative->m_currentNode->origin.forExit, opCatchOrigin, exceptionHandler); 
    if (willCatchException) {
        unsigned streamIndex = m_speculative->m_outOfLineStreamIndex ? *m_speculative->m_outOfLineStreamIndex : m_speculative->m_stream->size();
        MacroAssembler::Jump hadException = emitNonPatchableExceptionCheck(*vm());
        // We assume here that this is called after callOpeartion()/appendCall() is called.
        appendExceptionHandlingOSRExit(ExceptionCheck, streamIndex, opCatchOrigin, exceptionHandler, m_jitCode->common.lastCallSite(), hadException);
    } else
        m_exceptionChecks.append(emitExceptionCheck(*vm()));
}

CallSiteIndex JITCompiler::recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(const CodeOrigin& callSiteCodeOrigin, unsigned eventStreamIndex)
{
    CodeOrigin opCatchOrigin;
    HandlerInfo* exceptionHandler;
    bool willCatchException = m_graph.willCatchExceptionInMachineFrame(callSiteCodeOrigin, opCatchOrigin, exceptionHandler);
    CallSiteIndex callSite = addCallSite(callSiteCodeOrigin);
    if (willCatchException)
        appendExceptionHandlingOSRExit(GenericUnwind, eventStreamIndex, opCatchOrigin, exceptionHandler, callSite);
    return callSite;
}

void JITCompiler::setEndOfMainPath()
{
    m_pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), m_speculative->m_origin.semantic);
    if (LIKELY(!m_disassembler))
        return;
    m_disassembler->setEndOfMainPath(labelIgnoringWatchpoints());
}

void JITCompiler::setEndOfCode()
{
    m_pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), PCToCodeOriginMapBuilder::defaultCodeOrigin());
    if (LIKELY(!m_disassembler))
        return;
    m_disassembler->setEndOfCode(labelIgnoringWatchpoints());
}

void JITCompiler::makeCatchOSREntryBuffer()
{
    if (m_graph.m_maxLocalsForCatchOSREntry) {
        uint32_t numberOfLiveLocals = std::max(*m_graph.m_maxLocalsForCatchOSREntry, 1u); // Make sure we always allocate a non-null catchOSREntryBuffer.
        m_jitCode->common.catchOSREntryBuffer = vm()->scratchBufferForSize(sizeof(JSValue) * numberOfLiveLocals);
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
