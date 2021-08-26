/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
#include "CodeBlockWithJITType.h"
#include "DFGFailedFinalizer.h"
#include "DFGInlineCacheWrapperInlines.h"
#include "DFGJITCode.h"
#include "DFGJITFinalizer.h"
#include "DFGOSRExit.h"
#include "DFGSpeculativeJIT.h"
#include "DFGThunks.h"
#include "JSCJSValueInlines.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
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
        m_disassembler = makeUnique<Disassembler>(dfg);
#if ENABLE(FTL_JIT)
    m_jitCode->tierUpInLoopHierarchy = WTFMove(m_graph.m_plan.tierUpInLoopHierarchy());
    for (BytecodeIndex tierUpBytecode : m_graph.m_plan.tierUpAndOSREnterBytecodes())
        m_jitCode->tierUpEntryTriggers.add(tierUpBytecode, JITCode::TriggerReason::DontTrigger);
#endif
}

JITCompiler::~JITCompiler()
{
}

void JITCompiler::linkOSRExits()
{
    ASSERT(m_osrExit.size() == m_exitCompilationInfo.size());
    if (UNLIKELY(m_graph.compilation())) {
        for (unsigned i = 0; i < m_osrExit.size(); ++i) {
            OSRExitCompilationInfo& info = m_exitCompilationInfo[i];
            Vector<Label> labels;

            auto appendLabel = [&] (Label label) {
                RELEASE_ASSERT(label.isSet());
                labels.append(label);
            };
            
            if (!info.m_failureJumps.empty()) {
                for (unsigned j = 0; j < info.m_failureJumps.jumps().size(); ++j)
                    appendLabel(info.m_failureJumps.jumps()[j].label());
            } else if (info.m_replacementSource.isSet())
                appendLabel(info.m_replacementSource);
            m_exitSiteLabels.append(labels);
        }
    }
    
    for (unsigned i = 0; i < m_osrExit.size(); ++i) {
        OSRExitCompilationInfo& info = m_exitCompilationInfo[i];
        JumpList& failureJumps = info.m_failureJumps;
        if (!failureJumps.empty())
            failureJumps.link(this);
        else
            info.m_replacementDestination = label();

        jitAssertHasValidCallFrame();
        store32(TrustedImm32(i), &vm().osrExitIndex);
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
#if !ENABLE(EXTRA_CTI_THUNKS)
    if (!m_exceptionChecksWithCallFrameRollback.empty()) {
        m_exceptionChecksWithCallFrameRollback.link(this);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

        // operationLookupExceptionHandlerFromCallerFrame is passed one argument, the VM*.
        move(TrustedImmPtr(&vm()), GPRInfo::argumentGPR0);
        prepareCallOperation(vm());
        addPtr(TrustedImm32(m_graph.stackPointerOffset() * sizeof(Register)), GPRInfo::callFrameRegister, stackPointerRegister);

        appendCall(operationLookupExceptionHandlerFromCallerFrame);

        jumpToExceptionHandler(vm());
    }

    if (!m_exceptionChecks.empty()) {
        m_exceptionChecks.link(this);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

        // operationLookupExceptionHandler is passed one argument, the VM*.
        move(TrustedImmPtr(&vm()), GPRInfo::argumentGPR0);
        prepareCallOperation(vm());

        appendCall(operationLookupExceptionHandler);

        jumpToExceptionHandler(vm());
    }
#endif // ENABLE(EXTRA_CTI_THUNKS)
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

    if (!m_graph.m_stringSwitchJumpTables.isEmpty() || !m_graph.m_switchJumpTables.isEmpty()) {
        ConcurrentJSLocker locker(m_codeBlock->m_lock);
        if (!m_graph.m_stringSwitchJumpTables.isEmpty())
            m_codeBlock->ensureJITData(locker).m_stringSwitchJumpTables = WTFMove(m_graph.m_stringSwitchJumpTables);
        if (!m_graph.m_switchJumpTables.isEmpty())
            m_codeBlock->ensureJITData(locker).m_switchJumpTables = WTFMove(m_graph.m_switchJumpTables);
    }

    for (Bag<SwitchData>::iterator iter = m_graph.m_switchData.begin(); !!iter; ++iter) {
        SwitchData& data = **iter;
        switch (data.kind) {
        case SwitchChar:
        case SwitchImm: {
            if (!data.didUseJumpTable) {
                ASSERT(m_codeBlock->switchJumpTable(data.switchTableIndex).isEmpty());
                continue;
            }

            const UnlinkedSimpleJumpTable& unlinkedTable = m_graph.unlinkedSwitchJumpTable(data.switchTableIndex);
            SimpleJumpTable& linkedTable = m_codeBlock->switchJumpTable(data.switchTableIndex);
            linkedTable.m_ctiDefault = linkBuffer.locationOf<JSSwitchPtrTag>(m_blockHeads[data.fallThrough.block->index]);
            RELEASE_ASSERT(linkedTable.m_ctiOffsets.size() == unlinkedTable.m_branchOffsets.size());
            for (unsigned j = linkedTable.m_ctiOffsets.size(); j--;)
                linkedTable.m_ctiOffsets[j] = linkedTable.m_ctiDefault;
            for (unsigned j = data.cases.size(); j--;) {
                SwitchCase& myCase = data.cases[j];
                linkedTable.m_ctiOffsets[myCase.value.switchLookupValue(data.kind) - unlinkedTable.m_min] =
                    linkBuffer.locationOf<JSSwitchPtrTag>(m_blockHeads[myCase.target.block->index]);
            }
            break;
        }

        case SwitchString: {
            if (!data.didUseJumpTable) {
                ASSERT(m_codeBlock->stringSwitchJumpTable(data.switchTableIndex).isEmpty());
                continue;
            }

            const UnlinkedStringJumpTable& unlinkedTable = m_graph.unlinkedStringSwitchJumpTable(data.switchTableIndex);
            StringJumpTable& linkedTable = m_codeBlock->stringSwitchJumpTable(data.switchTableIndex);
            auto ctiDefault = linkBuffer.locationOf<JSSwitchPtrTag>(m_blockHeads[data.fallThrough.block->index]);
            RELEASE_ASSERT(linkedTable.m_ctiOffsets.size() == unlinkedTable.m_offsetTable.size() + 1);
            for (auto& entry : linkedTable.m_ctiOffsets)
                entry = ctiDefault;
            for (unsigned j = data.cases.size(); j--;) {
                SwitchCase& myCase = data.cases[j];
                auto iter = unlinkedTable.m_offsetTable.find(myCase.value.stringImpl());
                RELEASE_ASSERT(iter != unlinkedTable.m_offsetTable.end());
                linkedTable.m_ctiOffsets[iter->value.m_indexInTable] = linkBuffer.locationOf<JSSwitchPtrTag>(m_blockHeads[myCase.target.block->index]);
            }
            break;
        }

        case SwitchCell:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    // Link all calls out from the JIT code to their respective functions.
    for (unsigned i = 0; i < m_calls.size(); ++i)
        linkBuffer.link(m_calls[i].m_call, m_calls[i].m_function);

    finalizeInlineCaches(m_getByIds, linkBuffer);
    finalizeInlineCaches(m_getByIdsWithThis, linkBuffer);
    finalizeInlineCaches(m_getByVals, linkBuffer);
    finalizeInlineCaches(m_putByIds, linkBuffer);
    finalizeInlineCaches(m_putByVals, linkBuffer);
    finalizeInlineCaches(m_delByIds, linkBuffer);
    finalizeInlineCaches(m_delByVals, linkBuffer);
    finalizeInlineCaches(m_inByIds, linkBuffer);
    finalizeInlineCaches(m_inByVals, linkBuffer);
    finalizeInlineCaches(m_instanceOfs, linkBuffer);
    finalizeInlineCaches(m_privateBrandAccesses, linkBuffer);

    for (auto& record : m_jsCalls) {
        CallLinkInfo& info = *record.info;
        info.setCodeLocations(
            linkBuffer.locationOf<JSInternalPtrTag>(record.slowPathStart),
            linkBuffer.locationOf<JSInternalPtrTag>(record.doneLocation));
    }
    
    for (auto& record : m_jsDirectCalls) {
        CallLinkInfo& info = *record.info;
        info.setCodeLocations(
            linkBuffer.locationOf<JSInternalPtrTag>(record.slowPath),
            CodeLocationLabel<JSInternalPtrTag>());
    }

#if ENABLE(EXTRA_CTI_THUNKS)
    if (!m_exceptionChecks.empty())
        linkBuffer.link(m_exceptionChecks, CodeLocationLabel(vm().getCTIStub(handleExceptionGenerator).retaggedCode<NoPtrTag>()));
    if (!m_exceptionChecksWithCallFrameRollback.empty())
        linkBuffer.link(m_exceptionChecksWithCallFrameRollback, CodeLocationLabel(vm().getCTIStub(handleExceptionWithCallFrameRollbackGenerator).retaggedCode<NoPtrTag>()));
#endif // ENABLE(EXTRA_CTI_THUNKS)

    MacroAssemblerCodeRef<JITThunkPtrTag> osrExitThunk = vm().getCTIStub(osrExitGenerationThunkGenerator);
    auto target = CodeLocationLabel<JITThunkPtrTag>(osrExitThunk.code());
    for (unsigned i = 0; i < m_osrExit.size(); ++i) {
        OSRExitCompilationInfo& info = m_exitCompilationInfo[i];
        if (!Options::useProbeOSRExit()) {
            linkBuffer.link(info.m_patchableJump.m_jump, target);
            OSRExit& exit = m_osrExit[i];
            exit.m_patchableJumpLocation = linkBuffer.locationOf<JSInternalPtrTag>(info.m_patchableJump);
        }
        if (info.m_replacementSource.isSet()) {
            m_jitCode->common.m_jumpReplacements.append(JumpReplacement(
                linkBuffer.locationOf<JSInternalPtrTag>(info.m_replacementSource),
                linkBuffer.locationOf<OSRExitPtrTag>(info.m_replacementDestination)));
        }
    }
    
    if (UNLIKELY(m_graph.compilation())) {
        ASSERT(m_exitSiteLabels.size() == m_osrExit.size());
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
    m_jitCode->m_osrExit = WTFMove(m_osrExit);
    m_jitCode->m_speculationRecovery = WTFMove(m_speculationRecovery);
    
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
        m_codeBlock->setPCToCodeOriginMap(makeUnique<PCToCodeOriginMap>(WTFMove(m_pcToCodeOriginMapBuilder), linkBuffer));
}

static void emitStackOverflowCheck(JITCompiler& jit, MacroAssembler::JumpList& stackOverflow)
{
    int frameTopOffset = virtualRegisterForLocal(jit.graph().requiredRegisterCountForExecutionAndExit() - 1).offset() * sizeof(Register);
    unsigned maxFrameSize = -frameTopOffset;

    jit.addPtr(MacroAssembler::TrustedImm32(frameTopOffset), GPRInfo::callFrameRegister, GPRInfo::regT1);
    if (UNLIKELY(maxFrameSize > Options::reservedZoneSize()))
        stackOverflow.append(jit.branchPtr(MacroAssembler::Above, GPRInfo::regT1, GPRInfo::callFrameRegister));
    stackOverflow.append(jit.branchPtr(MacroAssembler::Above, MacroAssembler::AbsoluteAddress(jit.vm().addressOfSoftStackLimit()), GPRInfo::regT1));
}

void JITCompiler::compile()
{
    makeCatchOSREntryBuffer();

    setStartOfCode();
    compileEntry();
    m_speculative = makeUnique<SpeculativeJIT>(*this);

    // Plant a check that sufficient space is available in the JSStack.
    JumpList stackOverflow;
    emitStackOverflowCheck(*this, stackOverflow);

    addPtr(TrustedImm32(-(m_graph.frameRegisterCount() * sizeof(Register))), GPRInfo::callFrameRegister, stackPointerRegister);
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

    emitStoreCodeOrigin(CodeOrigin(BytecodeIndex(0)));

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

    auto linkBuffer = makeUnique<LinkBuffer>(*this, m_codeBlock, LinkBuffer::Profile::DFG, JITCompilationCanFail);
    if (linkBuffer->didFailToAllocate()) {
        m_graph.m_plan.setFinalizer(makeUnique<FailedFinalizer>(m_graph.m_plan));
        return;
    }
    
    link(*linkBuffer);
    m_speculative->linkOSREntries(*linkBuffer);

    disassemble(*linkBuffer);

    auto codeRef = FINALIZE_DFG_CODE(*linkBuffer, JSEntryPtrTag, "DFG JIT code for %s", toCString(CodeBlockWithJITType(m_codeBlock, JITType::DFGJIT)).data());
    m_jitCode->initializeCodeRefForDFG(codeRef, codeRef.code());

    auto finalizer = makeUnique<JITFinalizer>(m_graph.m_plan, m_jitCode.releaseNonNull(), WTFMove(linkBuffer));
    m_graph.m_plan.setFinalizer(WTFMove(finalizer));
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
    checkStackPointerAlignment();

    compileSetupRegistersForEntry();
    compileEntryExecutionFlag();

    // === Function body code generation ===
    m_speculative = makeUnique<SpeculativeJIT>(*this);
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

    emitStoreCodeOrigin(CodeOrigin(BytecodeIndex(0)));

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

        load32(AssemblyHelpers::payloadFor((VirtualRegister)CallFrameSlot::argumentCountIncludingThis), GPRInfo::regT1);
        branch32(AboveOrEqual, GPRInfo::regT1, TrustedImm32(m_codeBlock->numParameters())).linkTo(fromArityCheck, this);
        emitStoreCodeOrigin(CodeOrigin(BytecodeIndex(0)));
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);
        m_speculative->callOperationWithCallFrameRollbackOnException(m_codeBlock->isConstructor() ? operationConstructArityCheck : operationCallArityCheck, GPRInfo::regT0, m_codeBlock->globalObject());
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(maxFrameExtentForSlowPathCall), stackPointerRegister);
        branchTest32(Zero, GPRInfo::returnValueGPR).linkTo(fromArityCheck, this);
        emitStoreCodeOrigin(CodeOrigin(BytecodeIndex(0)));
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
    auto linkBuffer = makeUnique<LinkBuffer>(*this, m_codeBlock, LinkBuffer::Profile::DFG, JITCompilationCanFail);
    if (linkBuffer->didFailToAllocate()) {
        m_graph.m_plan.setFinalizer(makeUnique<FailedFinalizer>(m_graph.m_plan));
        return;
    }
    link(*linkBuffer);
    m_speculative->linkOSREntries(*linkBuffer);
    
    if (requiresArityFixup)
        linkBuffer->link(callArityFixup, FunctionPtr<JITThunkPtrTag>(vm().getCTIStub(arityFixupGenerator).code()));

    disassemble(*linkBuffer);

    MacroAssemblerCodePtr<JSEntryPtrTag> withArityCheck = linkBuffer->locationOf<JSEntryPtrTag>(arityCheck);

    m_jitCode->initializeCodeRefForDFG(
        FINALIZE_DFG_CODE(*linkBuffer, JSEntryPtrTag, "DFG JIT code for %s", toCString(CodeBlockWithJITType(m_codeBlock, JITType::DFGJIT)).data()),
        withArityCheck);

    auto finalizer = makeUnique<JITFinalizer>(m_graph.m_plan, m_jitCode.releaseNonNull(), WTFMove(linkBuffer), withArityCheck);
    m_graph.m_plan.setFinalizer(WTFMove(finalizer));
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
        m_graph.m_doubleConstants = makeUnique<Bag<double>>();

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
    m_graph.appendCatchEntrypoint(basicBlock.bytecodeBegin, linkBuffer.locationOf<ExceptionHandlerPtrTag>(blockHead), WTFMove(argumentFormats));
}

void JITCompiler::noticeOSREntry(BasicBlock& basicBlock, JITCompiler::Label blockHead, LinkBuffer& linkBuffer)
{
    RELEASE_ASSERT(!basicBlock.isCatchEntrypoint);

    // OSR entry is not allowed into blocks deemed unreachable by control flow analysis.
    if (!basicBlock.intersectionOfCFAHasVisited)
        return;

    OSREntryData entry;
    entry.m_bytecodeIndex = basicBlock.bytecodeBegin;
    entry.m_machineCode = linkBuffer.locationOf<OSREntryPtrTag>(blockHead);

    FixedOperands<AbstractValue> expectedValues(basicBlock.intersectionOfPastValuesAtHead);
    Vector<OSREntryReshuffling> reshufflings;

    // Fix the expected values: in our protocol, a dead variable will have an expected
    // value of (None, []). But the old JIT may stash some values there. So we really
    // need (Top, TOP).
    for (size_t argument = 0; argument < basicBlock.variablesAtHead.numberOfArguments(); ++argument) {
        Node* node = basicBlock.variablesAtHead.argument(argument);
        if (!node || !node->shouldGenerate())
            expectedValues.argument(argument).makeBytecodeTop();
    }
    for (size_t local = 0; local < basicBlock.variablesAtHead.numberOfLocals(); ++local) {
        Node* node = basicBlock.variablesAtHead.local(local);
        if (!node || !node->shouldGenerate())
            expectedValues.local(local).makeBytecodeTop();
        else {
            VariableAccessData* variable = node->variableAccessData();
            entry.m_machineStackUsed.set(variable->machineLocal().toLocal());
                
            switch (variable->flushFormat()) {
            case FlushedDouble:
                entry.m_localsForcedDouble.set(local);
                break;
            case FlushedInt52:
                entry.m_localsForcedAnyInt.set(local);
                break;
            default:
                break;
            }

            ASSERT(!variable->operand().isTmp());
            if (variable->operand().virtualRegister() != variable->machineLocal()) {
                reshufflings.append(
                    OSREntryReshuffling(
                        variable->operand().virtualRegister().offset(), variable->machineLocal().offset()));
            }
        }
    }
        
    entry.m_expectedValues = WTFMove(expectedValues);
    entry.m_reshufflings = WTFMove(reshufflings);
    m_osrEntry.append(WTFMove(entry));
}

void JITCompiler::appendExceptionHandlingOSRExit(ExitKind kind, unsigned eventStreamIndex, CodeOrigin opCatchOrigin, HandlerInfo* exceptionHandler, CallSiteIndex callSite, MacroAssembler::JumpList jumpsToFail)
{
    OSRExit exit(kind, JSValueRegs(), MethodOfGettingAValueProfile(), m_speculative.get(), eventStreamIndex);
    exit.m_codeOrigin = opCatchOrigin;
    exit.m_exceptionHandlerCallSiteIndex = callSite;
    OSRExitCompilationInfo& exitInfo = appendExitInfo(jumpsToFail);
    m_osrExit.append(WTFMove(exit));
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
        MacroAssembler::Jump hadException = emitNonPatchableExceptionCheck(vm());
        // We assume here that this is called after callOpeartion()/appendCall() is called.
        appendExceptionHandlingOSRExit(ExceptionCheck, streamIndex, opCatchOrigin, exceptionHandler, m_jitCode->common.codeOrigins->lastCallSite(), hadException);
    } else
        m_exceptionChecks.append(emitExceptionCheck(vm()));
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
        m_jitCode->common.catchOSREntryBuffer = vm().scratchBufferForSize(sizeof(JSValue) * numberOfLiveLocals);
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
