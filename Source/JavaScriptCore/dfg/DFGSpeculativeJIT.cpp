/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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
#include "DFGSpeculativeJIT.h"

#if ENABLE(DFG_JIT)

#include "BinarySwitch.h"
#include "CPUInlines.h"
#include "CodeBlockWithJITType.h"
#include "DFGAbstractInterpreterInlines.h"
#include "DFGArrayifySlowPathGenerator.h"
#include "DFGCallArrayAllocatorSlowPathGenerator.h"
#include "DFGCallCreateDirectArgumentsSlowPathGenerator.h"
#include "DFGCapabilities.h"
#include "DFGClobberize.h"
#include "DFGFailedFinalizer.h"
#include "DFGJITFinalizer.h"
#include "DFGMayExit.h"
#include "DFGOSRExitFuzz.h"
#include "DFGSaneStringGetByValSlowPathGenerator.h"
#include "DFGSlowPathGenerator.h"
#include "DFGSnippetParams.h"
#include "DirectArguments.h"
#include "DisallowMacroScratchRegisterUsage.h"
#include "JITBitAndGenerator.h"
#include "JITBitOrGenerator.h"
#include "JITBitXorGenerator.h"
#include "JITDivGenerator.h"
#include "JITLeftShiftGenerator.h"
#include "JITRightShiftGenerator.h"
#include "JITSizeStatistics.h"
#include "JSArrayIterator.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSGeneratorFunction.h"
#include "JSImmutableButterfly.h"
#include "JSIteratorHelper.h"
#include "JSLexicalEnvironment.h"
#include "JSMapIterator.h"
#include "JSPropertyNameEnumerator.h"
#include "JSSetIterator.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntEntrypoint.h"
#include "LLIntThunks.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "ProbeContext.h"
#include "RegExpObject.h"
#include "ScopedArguments.h"
#include "ThunkGenerators.h"
#include "TypeProfilerLog.h"
#include "WeakMapImpl.h"
#include "WebAssemblyModuleRecord.h"
#include <wtf/BitVector.h>
#include <wtf/Box.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace DFG {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SpeculativeJIT);

WTF_MAKE_TZONE_ALLOCATED_IMPL(FPRTemporary);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GPRTemporary);
WTF_MAKE_TZONE_ALLOCATED_IMPL(JSValueRegsFlushedCallResult);
WTF_MAKE_TZONE_ALLOCATED_IMPL(JSValueRegsTemporary);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateInt32Operand);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateStrictInt32Operand);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateInt52Operand);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateStrictInt52Operand);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateWhicheverInt52Operand);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateDoubleOperand);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateCellOperand);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateBooleanOperand);
#if USE(BIGINT32)
WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeculateBigInt32Operand);
#endif
WTF_MAKE_TZONE_ALLOCATED_IMPL(JSValueOperand);
WTF_MAKE_TZONE_ALLOCATED_IMPL(StorageOperand);

SpeculativeJIT::SpeculativeJIT(Graph& dfg)
    : Base(dfg)
    , m_graph(graph())
    , m_currentNode(nullptr)
    , m_lastGeneratedNode(LastNodeType)
    , m_indexInBlock(0)
    , m_generationInfo(m_graph.frameRegisterCount())
    , m_compileOkay(true)
    , m_state(m_graph)
    , m_interpreter(m_graph, m_state)
    , m_minifiedGraph(&jitCode()->minifiedDFG)
{
}

SpeculativeJIT::~SpeculativeJIT() = default;

static void emitStackOverflowCheck(JITCompiler& jit, MacroAssembler::JumpList& stackOverflow)
{
    int frameTopOffset = virtualRegisterForLocal(jit.graph().requiredRegisterCountForExecutionAndExit() - 1).offset() * sizeof(Register);
    unsigned maxFrameSize = -frameTopOffset;

    jit.addPtr(MacroAssembler::TrustedImm32(frameTopOffset), GPRInfo::callFrameRegister, GPRInfo::regT1);
    if (UNLIKELY(maxFrameSize > Options::reservedZoneSize()))
        stackOverflow.append(jit.branchPtr(MacroAssembler::Above, GPRInfo::regT1, GPRInfo::callFrameRegister));
    stackOverflow.append(jit.branchPtr(MacroAssembler::Above, MacroAssembler::AbsoluteAddress(jit.vm().addressOfSoftStackLimit()), GPRInfo::regT1));
}

void SpeculativeJIT::compile()
{
    makeCatchOSREntryBuffer();

    setStartOfCode();
    compileEntry();

    // Plant a check that sufficient space is available in the JSStack.
    JumpList stackOverflow;
    emitStackOverflowCheck(*this, stackOverflow);

    addPtr(TrustedImm32(-(m_graph.frameRegisterCount() * sizeof(Register))), GPRInfo::callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();
    compileSetupRegistersForEntry();
    compileEntryExecutionFlag();
    compileBody();
    setEndOfMainPath(m_origin.semantic);

    // === Footer code generation ===
    //
    // Generate the stack overflow handling; if the stack check in the entry head fails,
    // we need to call out to a helper function to throw the StackOverflowError.
    stackOverflow.link(this);

    emitStoreCodeOrigin(CodeOrigin(BytecodeIndex(0)));

    if (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);

    emitGetFromCallFrameHeaderPtr(CallFrameSlot::codeBlock, GPRInfo::argumentGPR0);
    callThrowOperationWithCallFrameRollback(operationThrowStackOverflowError, GPRInfo::argumentGPR0);

    // Generate slow path code.
    runSlowPathGenerators(m_pcToCodeOriginMapBuilder);
    m_pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

    linkOSRExits();

    // Create OSR entry trampolines if necessary.
    createOSREntries();
    setEndOfCode();

    LinkBuffer linkBuffer(*this, m_codeBlock, LinkBuffer::Profile::DFG, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) {
        m_graph.m_plan.setFinalizer(makeUnique<FailedFinalizer>(m_graph.m_plan));
        return;
    }

    link(linkBuffer);
    linkOSREntries(linkBuffer);

    disassemble(linkBuffer);

    auto codeRef = FINALIZE_DFG_CODE(linkBuffer, JSEntryPtrTag, "DFG JIT code for %s", toCString(CodeBlockWithJITType(m_codeBlock, JITType::DFGJIT)).data());
    m_jitCode->initializeCodeRefForDFG(codeRef, codeRef.code());
    m_jitCode->variableEventStream = finalizeEventStream();

    auto finalizer = makeUnique<JITFinalizer>(m_graph.m_plan, m_jitCode.releaseNonNull());
    m_graph.m_plan.setFinalizer(WTFMove(finalizer));
}

void SpeculativeJIT::compileFunction()
{
    makeCatchOSREntryBuffer();

    setStartOfCode();
    Label entryLabel(this);
    compileEntry();

    // === Function header code generation ===
    // This is the main entry point, without performing an arity check.
    // If we needed to perform an arity check we will already have moved the return address,
    // so enter after this.

    // Plant a check that sufficient space is available in the JSStack.
    JumpList stackOverflow;
    emitStackOverflowCheck(*this, stackOverflow);

    // Move the stack pointer down to accommodate locals
    addPtr(TrustedImm32(-(m_graph.frameRegisterCount() * sizeof(Register))), GPRInfo::callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    compileSetupRegistersForEntry();
    compileEntryExecutionFlag();

    // === Function body code generation ===
    compileBody();
    setEndOfMainPath(m_origin.semantic);

    // === Function footer code generation ===
    //
    // The fast entry point into a function does not check the correct number of arguments
    // have been passed to the call (we only use the fast entry point where we can statically
    // determine the correct number of arguments have been passed, or have already checked).
    // In cases where an arity check is necessary, we enter here.
    // FIXME: change this from a cti call to a DFG style operation (normal C calling conventions).
    Label arityCheck;
    JumpList stackOverflowWithEntry;
    bool requiresArityFixup = m_codeBlock->numParameters() != 1;
    if (requiresArityFixup) {
        arityCheck = label();
        unsigned numberOfParameters = m_codeBlock->numParameters();
        load32(calleeFramePayloadSlot(CallFrameSlot::argumentCountIncludingThis).withOffset(sizeof(CallerFrameAndPC) - prologueStackPointerDelta()), GPRInfo::argumentGPR2);
        branch32(AboveOrEqual, GPRInfo::argumentGPR2, TrustedImm32(numberOfParameters)).linkTo(entryLabel, this);

        getArityPadding(vm(), numberOfParameters, GPRInfo::argumentGPR2, GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR3, stackOverflowWithEntry);

#if CPU(X86_64)
        pop(GPRInfo::argumentGPR1);
#else
        tagPtr(NoPtrTag, linkRegister);
        move(linkRegister, GPRInfo::argumentGPR1);
#endif
        nearCallThunk(CodeLocationLabel { LLInt::arityFixup() });
#if CPU(X86_64)
        push(GPRInfo::argumentGPR1);
#else
        move(GPRInfo::argumentGPR1, linkRegister);
        untagPtr(NoPtrTag, linkRegister);
        validateUntaggedPtr(linkRegister, GPRInfo::argumentGPR0);
#endif
        jump(entryLabel);
    } else
        arityCheck = entryLabel;

    // Generate code to perform the stack overflow handling (if the stack check in
    // the function header fails), and generate the entry point with arity check.
    //
    // Generate the stack overflow handling; if the stack check in the function head fails,
    // we need to call out to a helper function to throw the StackOverflowError.
    stackOverflowWithEntry.link(this);
    compileEntry();
    stackOverflow.link(this);

    emitStoreCodeOrigin(CodeOrigin(BytecodeIndex(0)));

    if (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);

    emitGetFromCallFrameHeaderPtr(CallFrameSlot::codeBlock, GPRInfo::argumentGPR0);
    callThrowOperationWithCallFrameRollback(operationThrowStackOverflowError, GPRInfo::argumentGPR0);

    // Generate slow path code.
    runSlowPathGenerators(m_pcToCodeOriginMapBuilder);
    m_pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

    linkOSRExits();

    // Create OSR entry trampolines if necessary.
    createOSREntries();
    setEndOfCode();

    // === Link ===
    LinkBuffer linkBuffer(*this, m_codeBlock, LinkBuffer::Profile::DFG, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) {
        m_graph.m_plan.setFinalizer(makeUnique<FailedFinalizer>(m_graph.m_plan));
        return;
    }
    link(linkBuffer);
    linkOSREntries(linkBuffer);

    disassemble(linkBuffer);

    CodePtr<JSEntryPtrTag> withArityCheck = linkBuffer.locationOf<JSEntryPtrTag>(arityCheck);

    m_jitCode->initializeCodeRefForDFG(
        FINALIZE_DFG_CODE(linkBuffer, JSEntryPtrTag, "DFG JIT code for %s", toCString(CodeBlockWithJITType(m_codeBlock, JITType::DFGJIT)).data()),
        withArityCheck);
    m_jitCode->variableEventStream = finalizeEventStream();

    auto finalizer = makeUnique<JITFinalizer>(m_graph.m_plan, m_jitCode.releaseNonNull(), withArityCheck);
    m_graph.m_plan.setFinalizer(WTFMove(finalizer));
}

void SpeculativeJIT::exceptionCheck(GPRReg exceptionReg)
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
    bool willCatchException = m_graph.willCatchExceptionInMachineFrame(m_currentNode->origin.forExit, opCatchOrigin, exceptionHandler);
    if (willCatchException) {
        RELEASE_ASSERT(!m_underSilentSpill);
        unsigned streamIndex = m_outOfLineStreamIndex ? *m_outOfLineStreamIndex : m_stream.size();
        Jump hadException = emitNonPatchableExceptionCheck(vm(), exceptionReg);
        // We assume here that this is called after callOperation()/appendCall() is called.
        appendExceptionHandlingOSRExit(this, ExceptionCheck, streamIndex, opCatchOrigin, exceptionHandler, m_jitCode->common.codeOrigins->lastCallSite(), hadException);
    } else
        emitNonPatchableExceptionCheck(vm(), exceptionReg).linkThunk(CodeLocationLabel(vm().getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), this);
}

CallSiteIndex SpeculativeJIT::recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(const CodeOrigin& callSiteCodeOrigin, unsigned eventStreamIndex)
{
    CodeOrigin opCatchOrigin;
    HandlerInfo* exceptionHandler;
    bool willCatchException = m_graph.willCatchExceptionInMachineFrame(callSiteCodeOrigin, opCatchOrigin, exceptionHandler);
    CallSiteIndex callSite = addCallSite(callSiteCodeOrigin);
    if (willCatchException)
        appendExceptionHandlingOSRExit(this, GenericUnwind, eventStreamIndex, opCatchOrigin, exceptionHandler, callSite);
    return callSite;
}

void SpeculativeJIT::emitAllocateRawObject(GPRReg resultGPR, RegisteredStructure structure, GPRReg storageGPR, unsigned numElements, unsigned vectorLength)
{
    ASSERT(!isCopyOnWrite(structure->indexingMode()));
    IndexingType indexingType = structure->indexingType();
    bool hasIndexingHeader = hasIndexedProperties(indexingType);

    unsigned inlineCapacity = structure->inlineCapacity();
    unsigned outOfLineCapacity = structure->outOfLineCapacity();
    
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);
    GPRReg scratchGPR = scratch.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

    ASSERT(vectorLength >= numElements);
    vectorLength = Butterfly::optimalContiguousVectorLength(structure.get(), vectorLength);
    
    JumpList slowCases;

    size_t size = 0;
    if (hasIndexingHeader)
        size += vectorLength * sizeof(JSValue) + sizeof(IndexingHeader);
    size += outOfLineCapacity * sizeof(JSValue);

    move(TrustedImmPtr(nullptr), storageGPR);

    VM& vm = this->vm();
    if (size) {
        if (Allocator allocator = vm.auxiliarySpace().allocatorFor(size, AllocatorForMode::AllocatorIfExists)) {
            emitAllocate(storageGPR, JITAllocator::constant(allocator), scratchGPR, scratch2GPR, slowCases);
            
            addPtr(
                TrustedImm32(outOfLineCapacity * sizeof(JSValue) + sizeof(IndexingHeader)),
                storageGPR);
            
            if (hasIndexingHeader)
                store32(TrustedImm32(vectorLength), Address(storageGPR, Butterfly::offsetOfVectorLength()));
        } else
            slowCases.append(jump());
    }

    Allocator allocator;
    if (structure->typeInfo().type() == JSType::ArrayType)
        allocator = allocatorForConcurrently<JSArray>(vm, JSArray::allocationSize(inlineCapacity), AllocatorForMode::AllocatorIfExists);
    else
        allocator = allocatorForConcurrently<JSFinalObject>(vm, JSFinalObject::allocationSize(inlineCapacity), AllocatorForMode::AllocatorIfExists);
    if (allocator) {
        emitAllocateJSObject(resultGPR, JITAllocator::constant(allocator), scratchGPR, TrustedImmPtr(structure), storageGPR, scratch2GPR, slowCases, SlowAllocationResult::UndefinedBehavior);
        emitInitializeInlineStorage(resultGPR, structure->inlineCapacity(), scratchGPR);
    } else
        slowCases.append(jump());

    // I want a slow path that also loads out the storage pointer, and that's
    // what this custom CallArrayAllocatorSlowPathGenerator gives me. It's a lot
    // of work for a very small piece of functionality. :-/
    addSlowPathGenerator(makeUnique<CallArrayAllocatorSlowPathGenerator>(
        slowCases, this, operationNewRawObject, resultGPR, storageGPR,
        structure, vectorLength));

    if (numElements < vectorLength) {
        if (hasDouble(structure->indexingType()))
            emitFillStorageWithDoubleEmpty(storageGPR, sizeof(double) * numElements, vectorLength - numElements, scratchGPR);
        else
            emitFillStorageWithJSEmpty(storageGPR, sizeof(EncodedJSValue) * numElements, vectorLength - numElements, scratchGPR);
    }
    
    if (hasIndexingHeader)
        store32(TrustedImm32(numElements), Address(storageGPR, Butterfly::offsetOfPublicLength()));
    
    emitInitializeOutOfLineStorage(storageGPR, structure->outOfLineCapacity(), scratchGPR);
    
    mutatorFence(vm);
}

void SpeculativeJIT::emitGetLength(InlineCallFrame* inlineCallFrame, GPRReg lengthGPR, bool includeThis)
{
    if (inlineCallFrame && !inlineCallFrame->isVarargs())
        move(TrustedImm32(inlineCallFrame->argumentCountIncludingThis - !includeThis), lengthGPR);
    else {
        VirtualRegister argumentCountRegister = argumentCount(inlineCallFrame);
        load32(payloadFor(argumentCountRegister), lengthGPR);
        if (!includeThis)
            sub32(TrustedImm32(1), lengthGPR);
    }
}

void SpeculativeJIT::emitGetLength(CodeOrigin origin, GPRReg lengthGPR, bool includeThis)
{
    emitGetLength(origin.inlineCallFrame(), lengthGPR, includeThis);
}

void SpeculativeJIT::emitGetCallee(CodeOrigin origin, GPRReg calleeGPR)
{
    auto* inlineCallFrame = origin.inlineCallFrame();
    if (inlineCallFrame) {
        if (inlineCallFrame->isClosureCall) {
            loadPtr(
                addressFor(inlineCallFrame->calleeRecovery.virtualRegister()),
                calleeGPR);
        } else
            loadLinkableConstant(LinkableConstant(*this, inlineCallFrame->calleeRecovery.constant().asCell()), calleeGPR);
    } else
        loadPtr(addressFor(CallFrameSlot::callee), calleeGPR);
}

void SpeculativeJIT::emitGetArgumentStart(CodeOrigin origin, GPRReg startGPR)
{
    addPtr(
        TrustedImm32(
            argumentsStart(origin).offset() * static_cast<int>(sizeof(Register))),
        GPRInfo::callFrameRegister, startGPR);
}

MacroAssembler::Jump SpeculativeJIT::emitOSRExitFuzzCheck()
{
    if (!Options::useOSRExitFuzz()
        || !canUseOSRExitFuzzing(m_graph.baselineCodeBlockFor(m_origin.semantic))
        || !doOSRExitFuzzing())
        return Jump();
    
    Jump result;
    
    pushToSave(GPRInfo::regT0);
    load32(&g_numberOfOSRExitFuzzChecks, GPRInfo::regT0);
    add32(TrustedImm32(1), GPRInfo::regT0);
    store32(GPRInfo::regT0, &g_numberOfOSRExitFuzzChecks);
    unsigned atOrAfter = Options::fireOSRExitFuzzAtOrAfter();
    unsigned at = Options::fireOSRExitFuzzAt();
    if (at || atOrAfter) {
        unsigned threshold;
        RelationalCondition condition;
        if (atOrAfter) {
            threshold = atOrAfter;
            condition = Below;
        } else {
            threshold = at;
            condition = NotEqual;
        }
        Jump ok = branch32(
            condition, GPRInfo::regT0, TrustedImm32(threshold));
        popToRestore(GPRInfo::regT0);
        result = jump();
        ok.link(this);
    }
    popToRestore(GPRInfo::regT0);
    
    return result;
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Node* node, Jump jumpToFail)
{
    if (!m_compileOkay)
        return;
    Jump fuzzJump = emitOSRExitFuzzCheck();
    if (fuzzJump.isSet()) {
        JumpList jumpsToFail;
        jumpsToFail.append(fuzzJump);
        jumpsToFail.append(jumpToFail);
        appendExitInfo(jumpsToFail);
    } else
        appendExitInfo(jumpToFail);
    appendOSRExit(OSRExit(kind, jsValueSource, m_graph.methodOfGettingAValueProfileFor(m_currentNode, node), this, m_stream.size()));
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Node* node, const JumpList& jumpsToFail)
{
    if (!m_compileOkay)
        return;
    Jump fuzzJump = emitOSRExitFuzzCheck();
    if (fuzzJump.isSet()) {
        JumpList myJumpsToFail;
        myJumpsToFail.append(jumpsToFail);
        myJumpsToFail.append(fuzzJump);
        appendExitInfo(myJumpsToFail);
    } else
        appendExitInfo(jumpsToFail);
    appendOSRExit(OSRExit(kind, jsValueSource, m_graph.methodOfGettingAValueProfileFor(m_currentNode, node), this, m_stream.size()));
}

OSRExitJumpPlaceholder SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Node* node)
{
    if (!m_compileOkay)
        return OSRExitJumpPlaceholder();
    unsigned index = m_osrExit.size();
    appendExitInfo();
    appendOSRExit(OSRExit(kind, jsValueSource, m_graph.methodOfGettingAValueProfileFor(m_currentNode, node), this, m_stream.size()));
    return OSRExitJumpPlaceholder(index);
}

OSRExitJumpPlaceholder SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Edge nodeUse)
{
    return speculationCheck(kind, jsValueSource, nodeUse.node());
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Edge nodeUse, Jump jumpToFail)
{
    speculationCheck(kind, jsValueSource, nodeUse.node(), jumpToFail);
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Edge nodeUse, const JumpList& jumpsToFail)
{
    speculationCheck(kind, jsValueSource, nodeUse.node(), jumpsToFail);
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Node* node, Jump jumpToFail, const SpeculationRecovery& recovery)
{
    if (!m_compileOkay)
        return;
    unsigned recoveryIndex = appendSpeculationRecovery(recovery);
    appendExitInfo(jumpToFail);
    appendOSRExit(OSRExit(kind, jsValueSource, m_graph.methodOfGettingAValueProfileFor(m_currentNode, node), this, m_stream.size(), recoveryIndex));
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Edge nodeUse, Jump jumpToFail, const SpeculationRecovery& recovery)
{
    speculationCheck(kind, jsValueSource, nodeUse.node(), jumpToFail, recovery);
}

void SpeculativeJIT::compileInvalidationPoint(Node* node)
{
    if (!m_compileOkay)
        return;

#if USE(JSVALUE64)
    if (m_graph.m_plan.isUnlinked()) {
        auto exitJump = branchTest8(NonZero, Address(GPRInfo::jitDataRegister, JITData::offsetOfIsInvalidated()));
        speculationCheck(UncountableInvalidation, JSValueRegs(), nullptr, exitJump);
        noResult(node);
        return;
    }
#endif

    OSRExitCompilationInfo& info = appendExitInfo(JumpList());
    appendOSRExit(OSRExit(
        UncountableInvalidation, JSValueSource(), MethodOfGettingAValueProfile(),
        this, m_stream.size()));
    info.m_replacementSource = watchpointLabel();
    RELEASE_ASSERT(info.m_replacementSource.isSet());
    noResult(node);
}

void SpeculativeJIT::unreachable(Node* node)
{
    m_compileOkay = false;
    abortWithReason(DFGUnreachableNode, node->op());
}

void SpeculativeJIT::terminateSpeculativeExecution(ExitKind kind, JSValueRegs jsValueRegs, Node* node)
{
    if (!m_compileOkay)
        return;
    speculationCheck(kind, jsValueRegs, node, jump());
    m_compileOkay = false;
    if (verboseCompilationEnabled())
        dataLog("Bailing compilation.\n");
}

void SpeculativeJIT::terminateSpeculativeExecution(ExitKind kind, JSValueRegs jsValueRegs, Edge nodeUse)
{
    terminateSpeculativeExecution(kind, jsValueRegs, nodeUse.node());
}

void SpeculativeJIT::typeCheck(JSValueSource source, Edge edge, SpeculatedType typesPassedThrough, Jump jumpToFail, ExitKind exitKind)
{
    ASSERT(needsTypeCheck(edge, typesPassedThrough));
    m_interpreter.filter(edge, typesPassedThrough);
    speculationCheck(exitKind, source, edge.node(), jumpToFail);
}

void SpeculativeJIT::typeCheck(JSValueSource source, Edge edge, SpeculatedType typesPassedThrough, JumpList jumpListToFail, ExitKind exitKind)
{
    ASSERT(needsTypeCheck(edge, typesPassedThrough));
    m_interpreter.filter(edge, typesPassedThrough);
    speculationCheck(exitKind, source, edge.node(), jumpListToFail);
}

RegisterSetBuilder SpeculativeJIT::usedRegisters()
{
    RegisterSetBuilder result;
    
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        GPRReg gpr = GPRInfo::toRegister(i);
        if (m_gprs.isInUse(gpr))
            result.add(gpr, IgnoreVectors);
    }
    for (unsigned i = FPRInfo::numberOfRegisters; i--;) {
        FPRReg fpr = FPRInfo::toRegister(i);
        if (m_fprs.isInUse(fpr))
            result.add(fpr, IgnoreVectors);
    }
    
    // FIXME: This is overly conservative. We could subtract out those callee-saves that we
    // actually saved.
    // https://bugs.webkit.org/show_bug.cgi?id=185686
    result.merge(RegisterSetBuilder::stubUnavailableRegisters());
    
    return result;
}

void SpeculativeJIT::addSlowPathGenerator(std::unique_ptr<SlowPathGenerator> slowPathGenerator)
{
    m_slowPathGenerators.append(WTFMove(slowPathGenerator));
}

void SpeculativeJIT::addSlowPathGeneratorLambda(Function<void()>&& lambda)
{
    m_slowPathLambdas.append(SlowPathLambda { WTFMove(lambda), m_currentNode, static_cast<unsigned>(m_stream.size()) });
}

void SpeculativeJIT::runSlowPathGenerators(PCToCodeOriginMapBuilder& pcToCodeOriginMapBuilder)
{
    auto markSlowPathIfNeeded = [&] (Node* node) {
        std::optional<JITSizeStatistics::Marker> sizeMarker;
        if (UNLIKELY(Options::dumpDFGJITSizeStatistics())) {
            String id = makeString("DFG_slow_"_s, m_graph.opName(node->op()));
            sizeMarker = vm().jitSizeStatistics->markStart(id, *this);
        }
        return sizeMarker;
    };

    for (auto& slowPathGenerator : m_slowPathGenerators) {
        pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), slowPathGenerator->origin().semantic);
        auto sizeMarker = markSlowPathIfNeeded(slowPathGenerator->currentNode());

        slowPathGenerator->generate(this);

        if (UNLIKELY(sizeMarker))
            vm().jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this, m_graph.m_plan);
    }
    for (auto& slowPathLambda : m_slowPathLambdas) {
        Node* currentNode = slowPathLambda.currentNode;
        m_currentNode = currentNode;
        m_outOfLineStreamIndex = slowPathLambda.streamIndex;
        pcToCodeOriginMapBuilder.appendItem(labelIgnoringWatchpoints(), currentNode->origin.semantic);
        auto sizeMarker = markSlowPathIfNeeded(currentNode);

        slowPathLambda.generator();
        ASSERT(!m_underSilentSpill);
        m_outOfLineStreamIndex = std::nullopt;
        if (UNLIKELY(sizeMarker))
            vm().jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this, m_graph.m_plan);
    }
}

void SpeculativeJIT::clearGenerationInfo()
{
    for (unsigned i = 0; i < m_generationInfo.size(); ++i)
        m_generationInfo[i] = GenerationInfo();
    m_gprs = RegisterBank<GPRInfo>();
    m_fprs = RegisterBank<FPRInfo>();
}

SilentRegisterSavePlan SpeculativeJIT::silentSavePlanForGPR(VirtualRegister spillMe, GPRReg source)
{
    GenerationInfo& info = generationInfoFromVirtualRegister(spillMe);
    Node* node = info.node();
    DataFormat registerFormat = info.registerFormat();
    ASSERT(registerFormat != DataFormatNone);
    ASSERT(registerFormat != DataFormatDouble);
        
    SilentSpillAction spillAction;
    SilentFillAction fillAction;
        
    if (!info.needsSpill())
        spillAction = DoNothingForSpill;
    else {
#if USE(JSVALUE64)
        ASSERT(info.gpr() == source);
        if (registerFormat == DataFormatInt32)
            spillAction = Store32Payload;
        else if (registerFormat == DataFormatCell || registerFormat == DataFormatStorage)
            spillAction = StorePtr;
        else if (registerFormat == DataFormatInt52 || registerFormat == DataFormatStrictInt52)
            spillAction = Store64;
        else {
            ASSERT(registerFormat & DataFormatJS);
            spillAction = Store64;
        }
#elif USE(JSVALUE32_64)
        if (registerFormat & DataFormatJS) {
            ASSERT(info.tagGPR() == source || info.payloadGPR() == source);
            spillAction = source == info.tagGPR() ? Store32Tag : Store32Payload;
        } else {
            ASSERT(info.gpr() == source);
            spillAction = Store32Payload;
        }
#endif
    }
        
    if (registerFormat == DataFormatInt32) {
        ASSERT(info.gpr() == source);
        ASSERT(isJSInt32(info.registerFormat()));
        if (node->hasConstant()) {
            ASSERT(node->isInt32Constant());
            fillAction = SetInt32Constant;
        } else
            fillAction = Load32Payload;
    } else if (registerFormat == DataFormatBoolean) {
#if USE(JSVALUE64)
        RELEASE_ASSERT_NOT_REACHED();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
        fillAction = DoNothingForFill;
#endif
#elif USE(JSVALUE32_64)
        ASSERT(info.gpr() == source);
        if (node->hasConstant()) {
            ASSERT(node->isBooleanConstant());
            fillAction = SetBooleanConstant;
        } else
            fillAction = Load32Payload;
#endif
    } else if (registerFormat == DataFormatCell) {
        ASSERT(info.gpr() == source);
        if (node->hasConstant()) {
            DFG_ASSERT(m_graph, m_currentNode, node->isCellConstant());
            node->asCell(); // To get the assertion.
            fillAction = SetCellConstant;
        } else {
#if USE(JSVALUE64)
            fillAction = LoadPtr;
#else
            fillAction = Load32Payload;
#endif
        }
    } else if (registerFormat == DataFormatStorage) {
        ASSERT(info.gpr() == source);
        fillAction = LoadPtr;
    } else if (registerFormat == DataFormatInt52) {
        if (node->hasConstant())
            fillAction = SetInt52Constant;
        else if (info.spillFormat() == DataFormatInt52)
            fillAction = Load64;
        else if (info.spillFormat() == DataFormatStrictInt52)
            fillAction = Load64ShiftInt52Left;
        else if (info.spillFormat() == DataFormatNone)
            fillAction = Load64;
        else {
            RELEASE_ASSERT_NOT_REACHED();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
            fillAction = Load64; // Make GCC happy.
#endif
        }
    } else if (registerFormat == DataFormatStrictInt52) {
        if (node->hasConstant())
            fillAction = SetStrictInt52Constant;
        else if (info.spillFormat() == DataFormatInt52)
            fillAction = Load64ShiftInt52Right;
        else if (info.spillFormat() == DataFormatStrictInt52)
            fillAction = Load64;
        else if (info.spillFormat() == DataFormatNone)
            fillAction = Load64;
        else {
            RELEASE_ASSERT_NOT_REACHED();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
            fillAction = Load64; // Make GCC happy.
#endif
        }
    } else {
        ASSERT(registerFormat & DataFormatJS);
#if USE(JSVALUE64)
        ASSERT(info.gpr() == source);
        if (node->hasConstant()) {
            if (node->isCellConstant())
                fillAction = SetTrustedJSConstant;
            else
                fillAction = SetJSConstant;
        } else if (info.spillFormat() == DataFormatInt32) {
            ASSERT(registerFormat == DataFormatJSInt32);
            fillAction = Load32PayloadBoxInt;
        } else
            fillAction = Load64;
#else
        ASSERT(info.tagGPR() == source || info.payloadGPR() == source);
        if (node->hasConstant())
            fillAction = info.tagGPR() == source ? SetJSConstantTag : SetJSConstantPayload;
        else if (info.payloadGPR() == source)
            fillAction = Load32Payload;
        else { // Fill the Tag
            switch (info.spillFormat()) {
            case DataFormatInt32:
                ASSERT(registerFormat == DataFormatJSInt32);
                fillAction = SetInt32Tag;
                break;
            case DataFormatCell:
                ASSERT(registerFormat == DataFormatJSCell);
                fillAction = SetCellTag;
                break;
            case DataFormatBoolean:
                ASSERT(registerFormat == DataFormatJSBoolean);
                fillAction = SetBooleanTag;
                break;
            default:
                fillAction = Load32Tag;
                break;
            }
        }
#endif
    }
        
    return SilentRegisterSavePlan(spillAction, fillAction, node, source);
}
    
SilentRegisterSavePlan SpeculativeJIT::silentSavePlanForFPR(VirtualRegister spillMe, FPRReg source)
{
    GenerationInfo& info = generationInfoFromVirtualRegister(spillMe);
    Node* node = info.node();
    ASSERT(info.registerFormat() == DataFormatDouble);

    SilentSpillAction spillAction;
    SilentFillAction fillAction;
        
    if (!info.needsSpill())
        spillAction = DoNothingForSpill;
    else {
        ASSERT(!node->hasConstant());
        ASSERT(info.spillFormat() == DataFormatNone);
        ASSERT(info.fpr() == source);
        spillAction = StoreDouble;
    }
        
#if USE(JSVALUE64)
    if (node->hasConstant()) {
        node->asNumber(); // To get the assertion.
        fillAction = SetDoubleConstant;
    } else {
        ASSERT(info.spillFormat() == DataFormatNone || info.spillFormat() == DataFormatDouble);
        fillAction = LoadDouble;
    }
#elif USE(JSVALUE32_64)
    ASSERT(info.registerFormat() == DataFormatDouble);
    if (node->hasConstant()) {
        node->asNumber(); // To get the assertion.
        fillAction = SetDoubleConstant;
    } else
        fillAction = LoadDouble;
#endif

    return SilentRegisterSavePlan(spillAction, fillAction, node, source);
}
    
void SpeculativeJIT::silentSpillImpl(const SilentRegisterSavePlan& plan)
{
    ASSERT(m_underSilentSpill);
    switch (plan.spillAction()) {
    case DoNothingForSpill:
        break;
    case Store32Tag:
        store32(plan.gpr(), tagFor(plan.node()->virtualRegister()));
        break;
    case Store32Payload:
        store32(plan.gpr(), payloadFor(plan.node()->virtualRegister()));
        break;
    case StorePtr:
        storePtr(plan.gpr(), addressFor(plan.node()->virtualRegister()));
        break;
#if USE(JSVALUE64)
    case Store64:
        store64(plan.gpr(), addressFor(plan.node()->virtualRegister()));
        break;
#endif
    case StoreDouble:
        storeDouble(plan.fpr(), addressFor(plan.node()->virtualRegister()));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}
    
void SpeculativeJIT::silentFillImpl(const SilentRegisterSavePlan& plan)
{
    ASSERT(m_underSilentSpill);
    switch (plan.fillAction()) {
    case DoNothingForFill:
        break;
    case SetInt32Constant:
        move(Imm32(plan.node()->asInt32()), plan.gpr());
        break;
#if USE(JSVALUE64)
    case SetInt52Constant:
        move(Imm64(plan.node()->asAnyInt() << JSValue::int52ShiftAmount), plan.gpr());
        break;
    case SetStrictInt52Constant:
        move(Imm64(plan.node()->asAnyInt()), plan.gpr());
        break;
#endif // USE(JSVALUE64)
    case SetBooleanConstant:
        move(TrustedImm32(plan.node()->asBoolean()), plan.gpr());
        break;
    case SetCellConstant:
        ASSERT(plan.node()->constant()->value().isCell());
        loadLinkableConstant(LinkableConstant(*this, plan.node()->constant()->value().asCell()), plan.gpr());
        break;
#if USE(JSVALUE64)
    case SetTrustedJSConstant:
        move(valueOfJSConstantAsImm64(plan.node()).asTrustedImm64(), plan.gpr());
        break;
    case SetJSConstant:
        move(valueOfJSConstantAsImm64(plan.node()), plan.gpr());
        break;
    case SetDoubleConstant:
        move64ToDouble(Imm64(reinterpretDoubleToInt64(plan.node()->asNumber())), plan.fpr());
        break;
    case Load32PayloadBoxInt:
        load32(payloadFor(plan.node()->virtualRegister()), plan.gpr());
        or64(GPRInfo::numberTagRegister, plan.gpr());
        break;
    case Load32PayloadConvertToInt52:
        load32(payloadFor(plan.node()->virtualRegister()), plan.gpr());
        signExtend32ToPtr(plan.gpr(), plan.gpr());
        lshift64(TrustedImm32(JSValue::int52ShiftAmount), plan.gpr());
        break;
    case Load32PayloadSignExtend:
        load32(payloadFor(plan.node()->virtualRegister()), plan.gpr());
        signExtend32ToPtr(plan.gpr(), plan.gpr());
        break;
#else
    case SetJSConstantTag:
        move(Imm32(plan.node()->asJSValue().tag()), plan.gpr());
        break;
    case SetJSConstantPayload:
        move(Imm32(plan.node()->asJSValue().payload()), plan.gpr());
        break;
    case SetInt32Tag:
        move(TrustedImm32(JSValue::Int32Tag), plan.gpr());
        break;
    case SetCellTag:
        move(TrustedImm32(JSValue::CellTag), plan.gpr());
        break;
    case SetBooleanTag:
        move(TrustedImm32(JSValue::BooleanTag), plan.gpr());
        break;
    case SetDoubleConstant:
        loadDouble(TrustedImmPtr(addressOfDoubleConstant(plan.node())), plan.fpr());
        break;
#endif
    case Load32Tag:
        load32(tagFor(plan.node()->virtualRegister()), plan.gpr());
        break;
    case Load32Payload:
        load32(payloadFor(plan.node()->virtualRegister()), plan.gpr());
        break;
    case LoadPtr:
        loadPtr(addressFor(plan.node()->virtualRegister()), plan.gpr());
        break;
#if USE(JSVALUE64)
    case Load64:
        load64(addressFor(plan.node()->virtualRegister()), plan.gpr());
        break;
    case Load64ShiftInt52Right:
        load64(addressFor(plan.node()->virtualRegister()), plan.gpr());
        rshift64(TrustedImm32(JSValue::int52ShiftAmount), plan.gpr());
        break;
    case Load64ShiftInt52Left:
        load64(addressFor(plan.node()->virtualRegister()), plan.gpr());
        lshift64(TrustedImm32(JSValue::int52ShiftAmount), plan.gpr());
        break;
#endif
    case LoadDouble:
        loadDouble(addressFor(plan.node()->virtualRegister()), plan.fpr());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

JITCompiler::JumpList SpeculativeJIT::jumpSlowForUnwantedArrayMode(GPRReg tempGPR, ArrayMode arrayMode)
{
    JumpList result;

    IndexingType indexingModeMask = IsArray | IndexingShapeMask;
    if (arrayMode.action() == Array::Write)
        indexingModeMask |= CopyOnWrite;

    switch (arrayMode.type()) {
    case Array::Int32:
    case Array::Double:
    case Array::Contiguous:
    case Array::Undecided:
    case Array::ArrayStorage: {
        IndexingType shape = arrayMode.shapeMask();
        switch (arrayMode.arrayClass()) {
        case Array::OriginalArray:
        case Array::OriginalNonCopyOnWriteArray:
        case Array::OriginalCopyOnWriteArray:
            RELEASE_ASSERT_NOT_REACHED();
            return result;

        case Array::Array:
            and32(TrustedImm32(indexingModeMask), tempGPR);
            result.append(branch32(
                NotEqual, tempGPR, TrustedImm32(IsArray | shape)));
            return result;

        case Array::NonArray:
        case Array::OriginalNonArray:
            and32(TrustedImm32(indexingModeMask), tempGPR);
            result.append(branch32(
                NotEqual, tempGPR, TrustedImm32(shape)));
            return result;

        case Array::PossiblyArray:
            and32(TrustedImm32(indexingModeMask & ~IsArray), tempGPR);
            result.append(branch32(NotEqual, tempGPR, TrustedImm32(shape)));
            return result;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return result;
    }

    case Array::SlowPutArrayStorage: {
        ASSERT(!arrayMode.isJSArrayWithOriginalStructure());

        switch (arrayMode.arrayClass()) {
        case Array::OriginalArray:
        case Array::OriginalNonCopyOnWriteArray:
        case Array::OriginalCopyOnWriteArray:
            RELEASE_ASSERT_NOT_REACHED();
            return result;

        case Array::Array:
            result.append(
                branchTest32(
                    Zero, tempGPR, TrustedImm32(IsArray)));
            break;

        case Array::NonArray:
        case Array::OriginalNonArray:
            result.append(
                branchTest32(
                    NonZero, tempGPR, TrustedImm32(IsArray)));
            break;

        case Array::PossiblyArray:
            break;
        }

        and32(TrustedImm32(IndexingShapeMask), tempGPR);
        sub32(TrustedImm32(ArrayStorageShape), tempGPR);
        result.append(
            branch32(
                Above, tempGPR,
                TrustedImm32(SlowPutArrayStorageShape - ArrayStorageShape)));
        return result;
    }
    default:
        CRASH();
        break;
    }
    
    return result;
}

void SpeculativeJIT::checkArray(Node* node)
{
    ArrayMode arrayMode = node->arrayMode();
    ASSERT(arrayMode.isSpecific());
    ASSERT(!arrayMode.doesConversion());
    
    SpeculateCellOperand base(this, node->child1());
    GPRReg baseReg = base.gpr();
    
    if (arrayMode.alreadyChecked(m_graph, node, m_state.forNode(node->child1()))) {
        // We can purge Empty check completely in this case of CheckArrayOrEmpty since CellUse only accepts SpecCell | SpecEmpty.
#if USE(JSVALUE64)
        ASSERT(typeFilterFor(node->child1().useKind()) & SpecEmpty);
#endif
        noResult(m_currentNode);
        return;
    }

    std::optional<GPRTemporary> temp;
    std::optional<GPRReg> tempGPR;
    switch (arrayMode.type()) {
    case Array::Int32:
    case Array::Double:
    case Array::Contiguous:
    case Array::Undecided:
    case Array::ArrayStorage:
    case Array::SlowPutArrayStorage: {
        temp.emplace(this);
        tempGPR = temp->gpr();
        break;
    }
    default:
        break;
    }

    Jump isEmpty;

#if USE(JSVALUE64)
    if (node->op() == CheckArrayOrEmpty) {
        if (m_interpreter.forNode(node->child1()).m_type & SpecEmpty)
            isEmpty = branchIfEmpty(baseReg);
    }
#endif

    switch (arrayMode.type()) {
    case Array::String:
        RELEASE_ASSERT_NOT_REACHED(); // Should have been a Phantom(String:)
        return;
    case Array::Int32:
    case Array::Double:
    case Array::Contiguous:
    case Array::Undecided:
    case Array::ArrayStorage:
    case Array::SlowPutArrayStorage: {
        load8(Address(baseReg, JSCell::indexingTypeAndMiscOffset()), tempGPR.value());
        speculationCheck(
            BadIndexingType, JSValueSource::unboxedCell(baseReg), nullptr,
            jumpSlowForUnwantedArrayMode(tempGPR.value(), arrayMode));
        break;
    }
    case Array::DirectArguments:
        speculateCellTypeWithoutTypeFiltering(node->child1(), baseReg, DirectArgumentsType);
        break;
    case Array::ScopedArguments:
        speculateCellTypeWithoutTypeFiltering(node->child1(), baseReg, ScopedArgumentsType);
        break;
    default: {
        DFG_ASSERT(m_graph, node, arrayMode.isSomeTypedArrayView());

        if (arrayMode.type() == Array::AnyTypedArray)
            speculationCheck(BadType, JSValueSource::unboxedCell(baseReg), nullptr, branchIfNotType(baseReg, JSTypeRange { JSType(FirstTypedArrayType), JSType(LastTypedArrayTypeExcludingDataView) }));
        else
            speculateCellTypeWithoutTypeFiltering(node->child1(), baseReg, typeForTypedArrayType(arrayMode.typedArrayType()));
        break;
    }
    }

    if (isEmpty.isSet())
        isEmpty.link(this);
    noResult(m_currentNode);
}

void SpeculativeJIT::arrayify(Node* node, GPRReg baseReg, GPRReg propertyReg)
{
    ASSERT(node->arrayMode().doesConversion());
    
    GPRTemporary temp(this);
    GPRTemporary structure;
    GPRReg tempGPR = temp.gpr();
    GPRReg structureGPR = InvalidGPRReg;
    
    if (node->op() != ArrayifyToStructure) {
        GPRTemporary realStructure(this);
        structure.adopt(realStructure);
        structureGPR = structure.gpr();
    }
        
    // We can skip all that comes next if we already have array storage.
    JumpList slowPath;
    
    if (node->op() == ArrayifyToStructure) {
        ASSERT(!isCopyOnWrite(node->structure()->indexingMode()));
        ASSERT((node->structure()->indexingType() & IndexingShapeMask) == node->arrayMode().shapeMask());
        slowPath.append(branchWeakStructure(
            NotEqual,
            Address(baseReg, JSCell::structureIDOffset()),
            node->structure()));
    } else {
        load8(
            Address(baseReg, JSCell::indexingTypeAndMiscOffset()), tempGPR);
        
        slowPath.append(jumpSlowForUnwantedArrayMode(tempGPR, node->arrayMode()));
    }
    
    addSlowPathGenerator(makeUnique<ArrayifySlowPathGenerator>(
        slowPath, this, node, baseReg, propertyReg, tempGPR, structureGPR));
    
    noResult(m_currentNode);
}

void SpeculativeJIT::arrayify(Node* node)
{
    ASSERT(node->arrayMode().isSpecific());
    
    SpeculateCellOperand base(this, node->child1());
    
    if (!node->child2()) {
        arrayify(node, base.gpr(), InvalidGPRReg);
        return;
    }
    
    SpeculateInt32Operand property(this, node->child2());
    
    arrayify(node, base.gpr(), property.gpr());
}

GPRReg SpeculativeJIT::fillStorage(Edge edge)
{
    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
    
    switch (info.registerFormat()) {
    case DataFormatNone: {
        if (info.spillFormat() == DataFormatStorage) {
            GPRReg gpr = allocate();
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            loadPtr(addressFor(virtualRegister), gpr);
            info.fillStorage(m_stream, gpr);
            return gpr;
        }
        
        // Must be a cell; fill it as a cell and then return the pointer.
        return fillSpeculateCell(edge);
    }
        
    case DataFormatStorage: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }
        
    default:
        return fillSpeculateCell(edge);
    }
}

void SpeculativeJIT::useChildren(Node* node)
{
    if (node->flags() & NodeHasVarArgs) {
        for (unsigned childIdx = node->firstChild(); childIdx < node->firstChild() + node->numChildren(); childIdx++) {
            if (!!m_graph.m_varArgChildren[childIdx])
                use(m_graph.m_varArgChildren[childIdx]);
        }
    } else {
        Edge child1 = node->child1();
        if (!child1) {
            ASSERT(!node->child2() && !node->child3());
            return;
        }
        use(child1);
        
        Edge child2 = node->child2();
        if (!child2) {
            ASSERT(!node->child3());
            return;
        }
        use(child2);
        
        Edge child3 = node->child3();
        if (!child3)
            return;
        use(child3);
    }
}

void SpeculativeJIT::compilePushWithScope(Node* node)
{
    SpeculateCellOperand currentScope(this, node->child1());
    GPRReg currentScopeGPR = currentScope.gpr();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    auto objectEdge = node->child2();
    if (objectEdge.useKind() == ObjectUse) {
        SpeculateCellOperand object(this, objectEdge);
        GPRReg objectGPR = object.gpr();
        speculateObject(objectEdge, objectGPR);

        flushRegisters();
        callOperationWithoutExceptionCheck(operationPushWithScopeObject, resultGPR, LinkableConstant::globalObject(*this, node), currentScopeGPR, objectGPR);
        // No exception check here as we did not have to call toObject().
    } else {
        ASSERT(objectEdge.useKind() == UntypedUse);
        JSValueOperand object(this, objectEdge);
        JSValueRegs objectRegs = object.jsValueRegs();

        flushRegisters();
        callOperation(operationPushWithScope, resultGPR, LinkableConstant::globalObject(*this, node), currentScopeGPR, objectRegs);
    }
    
    cellResult(resultGPR, node);
}

bool SpeculativeJIT::genericJSValueStrictEq(Node* node, bool invert)
{
    unsigned branchIndexInBlock = detectPeepHoleBranch();
    if (branchIndexInBlock != UINT_MAX) {
        Node* branchNode = m_block->at(branchIndexInBlock);

        ASSERT(node->adjustedRefCount() == 1);
        
        nonSpeculativePeepholeStrictEq(node, branchNode, invert);
    
        m_indexInBlock = branchIndexInBlock;
        m_currentNode = branchNode;
        
        return true;
    }
    
    genericJSValueNonPeepholeStrictEq(node, invert);
    
    return false;
}

static const char* dataFormatString(DataFormat format)
{
    // These values correspond to the DataFormat enum.
    const char* strings[] = {
        "[  ]",
        "[ i]",
        "[ d]",
        "[ c]",
        "Err!",
        "Err!",
        "Err!",
        "Err!",
        "[J ]",
        "[Ji]",
        "[Jd]",
        "[Jc]",
        "Err!",
        "Err!",
        "Err!",
        "Err!",
    };
    return strings[format];
}

void SpeculativeJIT::dump(const char* label)
{
    if (label)
        dataLogF("<%s>\n", label);

    dataLogF("  gprs:\n");
    m_gprs.dump();
    dataLogF("  fprs:\n");
    m_fprs.dump();
    dataLogF("  VirtualRegisters:\n");
    for (unsigned i = 0; i < m_generationInfo.size(); ++i) {
        GenerationInfo& info = m_generationInfo[i];
        if (info.alive())
            dataLogF("    % 3d:%s%s", i, dataFormatString(info.registerFormat()), dataFormatString(info.spillFormat()));
        else
            dataLogF("    % 3d:[__][__]", i);
        if (info.registerFormat() == DataFormatDouble)
            dataLogF(":fpr%d\n", info.fpr());
        else if (info.registerFormat() != DataFormatNone
#if USE(JSVALUE32_64)
            && !(info.registerFormat() & DataFormatJS)
#endif
            ) {
            ASSERT(info.gpr() != InvalidGPRReg);
            dataLogF(":%s\n", GPRInfo::debugName(info.gpr()).characters());
        } else
            dataLogF("\n");
    }
    if (label)
        dataLogF("</%s>\n", label);
}

GPRTemporary::GPRTemporary()
    : m_jit(nullptr)
    , m_gpr(InvalidGPRReg)
{
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, GPRReg specific)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    m_gpr = m_jit->allocate(specific);
}

#if USE(JSVALUE32_64)
GPRTemporary::GPRTemporary(
    SpeculativeJIT* jit, ReuseTag, JSValueOperand& op1, WhichValueWord which)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (!op1.isDouble() && m_jit->canReuse(op1.node()))
        m_gpr = m_jit->reuse(op1.gpr(which));
    else
        m_gpr = m_jit->allocate();
}
#else // USE(JSVALUE32_64)
GPRTemporary::GPRTemporary(SpeculativeJIT* jit, ReuseTag, JSValueOperand& op1, WhichValueWord)
    : GPRTemporary(jit, Reuse, op1)
{
}
#endif

JSValueRegsTemporary::JSValueRegsTemporary() = default;

JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit)
#if USE(JSVALUE64)
    : m_gpr(jit)
#else
    : m_payloadGPR(jit)
    , m_tagGPR(jit)
#endif
{
}

JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit, GPRReg specificPayload)
#if USE(JSVALUE64)
    : m_gpr(jit, specificPayload)
#else
    : m_payloadGPR(jit, specificPayload)
    , m_tagGPR(jit)
#endif
{
}

#if USE(JSVALUE64)
JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit, ReuseTag, JSValueOperand& operand)
{
    m_gpr = GPRTemporary(jit, Reuse, operand);
}
#else
JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit, ReuseTag, JSValueOperand& operand)
{
    if (jit->canReuse(operand.node())) {
        m_payloadGPR = GPRTemporary(jit, Reuse, operand, PayloadWord);
        m_tagGPR = GPRTemporary(jit, Reuse, operand, TagWord);
    } else {
        m_payloadGPR = GPRTemporary(jit);
        m_tagGPR = GPRTemporary(jit);
    }
}
#endif

JSValueRegsTemporary::~JSValueRegsTemporary() = default;

JSValueRegs JSValueRegsTemporary::regs()
{
#if USE(JSVALUE64)
    return JSValueRegs(m_gpr.gpr());
#else
    return JSValueRegs(m_tagGPR.gpr(), m_payloadGPR.gpr());
#endif
}

void GPRTemporary::adopt(GPRTemporary& other)
{
    ASSERT(!m_jit);
    ASSERT(m_gpr == InvalidGPRReg);
    ASSERT(other.m_jit);
    ASSERT(other.m_gpr != InvalidGPRReg);
    m_jit = other.m_jit;
    m_gpr = other.m_gpr;
    other.m_jit = nullptr;
    other.m_gpr = InvalidGPRReg;
}

FPRTemporary::FPRTemporary(FPRTemporary&& other)
{
    ASSERT(other.m_jit);
    ASSERT(other.m_fpr != InvalidFPRReg);
    m_jit = other.m_jit;
    m_fpr = other.m_fpr;

    other.m_jit = nullptr;
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit, SpeculateDoubleOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.node()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit, SpeculateDoubleOperand& op1, SpeculateDoubleOperand& op2)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.node()))
        m_fpr = m_jit->reuse(op1.fpr());
    else if (m_jit->canReuse(op2.node()))
        m_fpr = m_jit->reuse(op2.fpr());
    else if (m_jit->canReuse(op1.node(), op2.node()) && op1.fpr() == op2.fpr())
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

#if USE(JSVALUE32_64)
FPRTemporary::FPRTemporary(SpeculativeJIT* jit, JSValueOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (op1.isDouble() && m_jit->canReuse(op1.node()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}
#endif

void SpeculativeJIT::compilePeepHoleDoubleBranch(Node* node, Node* branchNode, DoubleCondition condition)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    if (taken == nextBlock()) {
        condition = invert(condition);
        std::swap(taken, notTaken);
    }

    SpeculateDoubleOperand op1(this, node->child1());
    SpeculateDoubleOperand op2(this, node->child2());
    
    branchDouble(condition, op1.fpr(), op2.fpr(), taken);
    jump(notTaken);
}

void SpeculativeJIT::compilePeepHoleObjectEquality(Node* node, Node* branchNode)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    RelationalCondition condition = Equal;
    
    if (taken == nextBlock()) {
        condition = NotEqual;
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    SpeculateCellOperand op1(this, node->child1());
    SpeculateCellOperand op2(this, node->child2());
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    
    if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
        if (m_state.forNode(node->child1()).m_type & ~SpecObject) {
            speculationCheck(
                BadType, JSValueSource::unboxedCell(op1GPR), node->child1(), branchIfNotObject(op1GPR));
        }
        if (m_state.forNode(node->child2()).m_type & ~SpecObject) {
            speculationCheck(
                BadType, JSValueSource::unboxedCell(op2GPR), node->child2(), branchIfNotObject(op2GPR));
        }
    } else {
        if (m_state.forNode(node->child1()).m_type & ~SpecObject) {
            speculationCheck(
                BadType, JSValueSource::unboxedCell(op1GPR), node->child1(),
                branchIfNotObject(op1GPR));
        }
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node->child1(),
            branchTest8(
                NonZero,
                Address(op1GPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));

        if (m_state.forNode(node->child2()).m_type & ~SpecObject) {
            speculationCheck(
                BadType, JSValueSource::unboxedCell(op2GPR), node->child2(),
                branchIfNotObject(op2GPR));
        }
        speculationCheck(BadType, JSValueSource::unboxedCell(op2GPR), node->child2(),
            branchTest8(
                NonZero,
                Address(op2GPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));
    }

    branchPtr(condition, op1GPR, op2GPR, taken);
    jump(notTaken);
}

void SpeculativeJIT::compilePeepHoleBooleanBranch(Node* node, Node* branchNode, RelationalCondition condition)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        condition = invert(condition);
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    if (node->child1()->isInt32Constant()) {
        int32_t imm = node->child1()->asInt32();
        SpeculateBooleanOperand op2(this, node->child2());
        branch32(condition, Imm32(imm), op2.gpr(), taken);
    } else if (node->child2()->isInt32Constant()) {
        SpeculateBooleanOperand op1(this, node->child1());
        int32_t imm = node->child2()->asInt32();
        branch32(condition, op1.gpr(), Imm32(imm), taken);
    } else {
        SpeculateBooleanOperand op1(this, node->child1());
        SpeculateBooleanOperand op2(this, node->child2());
        branch32(condition, op1.gpr(), op2.gpr(), taken);
    }

    jump(notTaken);
}

void SpeculativeJIT::compileStringSlice(Node* node)
{
    SpeculateCellOperand string(this, node->child1());

    GPRReg stringGPR = string.gpr();

    speculateString(node->child1(), stringGPR);

    SpeculateInt32Operand start(this, node->child2());
    GPRReg startGPR = start.gpr();

    std::optional<SpeculateInt32Operand> end;
    std::optional<GPRReg> endGPR;
    if (node->child3()) {
        end.emplace(this, node->child3());
        endGPR.emplace(end->gpr());
    }

    GPRTemporary temp(this);
    GPRTemporary temp2(this);
    GPRTemporary startIndex(this);

    GPRReg tempGPR = temp.gpr();
    GPRReg temp2GPR = temp2.gpr();
    GPRReg startIndexGPR = startIndex.gpr();

    loadPtr(Address(stringGPR, JSString::offsetOfValue()), tempGPR);
    auto isRope = branchIfRopeStringImpl(tempGPR);
    {
        load32(Address(tempGPR, StringImpl::lengthMemoryOffset()), temp2GPR);

        emitPopulateSliceIndex(node->child2(), startGPR, temp2GPR, startIndexGPR);

        if (node->child3())
            emitPopulateSliceIndex(node->child3(), endGPR.value(), temp2GPR, tempGPR);
        else
            move(temp2GPR, tempGPR);
    }

    JumpList doneCases;
    JumpList slowCases;

    VM& vm = this->vm();
    auto nonEmptyCase = branch32(Below, startIndexGPR, tempGPR);
    loadLinkableConstant(LinkableConstant(*this, jsEmptyString(vm)), tempGPR);
    doneCases.append(jump());

    nonEmptyCase.link(this);
    sub32(startIndexGPR, tempGPR); // the size of the sliced string.
    slowCases.append(branch32(NotEqual, tempGPR, TrustedImm32(1)));

    // Refill StringImpl* here.
    loadPtr(Address(stringGPR, JSString::offsetOfValue()), temp2GPR);
    loadPtr(Address(temp2GPR, StringImpl::dataOffset()), tempGPR);

    // Load the character into scratchReg
    zeroExtend32ToWord(startIndexGPR, startIndexGPR);
    auto is16Bit = branchTest32(Zero, Address(temp2GPR, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    load8(BaseIndex(tempGPR, startIndexGPR, TimesOne, 0), tempGPR);
    auto cont8Bit = jump();

    is16Bit.link(this);
    load16(BaseIndex(tempGPR, startIndexGPR, TimesTwo, 0), tempGPR);

    auto bigCharacter = branch32(Above, tempGPR, TrustedImm32(maxSingleCharacterString));

    // 8 bit string values don't need the isASCII check.
    cont8Bit.link(this);

    lshift32(TrustedImm32(sizeof(void*) == 4 ? 2 : 3), tempGPR);
    addPtr(TrustedImmPtr(vm.smallStrings.singleCharacterStrings()), tempGPR);
    loadPtr(Address(tempGPR), tempGPR);

    addSlowPathGenerator(slowPathCall(bigCharacter, this, operationSingleCharacterString, tempGPR, TrustedImmPtr(&vm), tempGPR));

    addSlowPathGenerator(slowPathCall(slowCases, this, operationStringSubstr, tempGPR, LinkableConstant::globalObject(*this, node), stringGPR, startIndexGPR, tempGPR));

    if (endGPR)
        addSlowPathGenerator(slowPathCall(isRope, this, operationStringSliceWithEnd, tempGPR, LinkableConstant::globalObject(*this, node), stringGPR, startGPR, *endGPR));
    else
        addSlowPathGenerator(slowPathCall(isRope, this, operationStringSlice, tempGPR, LinkableConstant::globalObject(*this, node), stringGPR, startGPR));

    doneCases.link(this);
    cellResult(tempGPR, node);
}

void SpeculativeJIT::compileStringSubstring(Node* node)
{
    SpeculateCellOperand string(this, node->child1());

    SpeculateInt32Operand start(this, node->child2());
    if (node->child3()) {
        SpeculateInt32Operand end(this, node->child3());

        GPRReg stringGPR = string.gpr();
        GPRReg startGPR = start.gpr();
        GPRReg endGPR = end.gpr();

        speculateString(node->child1(), stringGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationStringSubstringWithEnd, resultGPR, LinkableConstant::globalObject(*this, node), stringGPR, startGPR, endGPR);
        cellResult(resultGPR, node);
        return;
    }

    GPRReg stringGPR = string.gpr();
    GPRReg startGPR = start.gpr();

    speculateString(node->child1(), stringGPR);

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationStringSubstring, resultGPR, LinkableConstant::globalObject(*this, node), stringGPR, startGPR);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileToLowerCase(Node* node)
{
    ASSERT(node->op() == ToLowerCase);
    SpeculateCellOperand string(this, node->child1());
    GPRTemporary temp(this);
    GPRTemporary index(this);
    GPRTemporary charReg(this);
    GPRTemporary length(this);

    GPRReg stringGPR = string.gpr();
    GPRReg tempGPR = temp.gpr();
    GPRReg indexGPR = index.gpr();
    GPRReg charGPR = charReg.gpr();
    GPRReg lengthGPR = length.gpr();

    speculateString(node->child1(), stringGPR);

    JumpList slowPath;

    move(TrustedImmPtr(nullptr), indexGPR);

    loadPtr(Address(stringGPR, JSString::offsetOfValue()), tempGPR);
    slowPath.append(branchIfRopeStringImpl(tempGPR));
    slowPath.append(branchTest32(
        Zero, Address(tempGPR, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit())));
    load32(Address(tempGPR, StringImpl::lengthMemoryOffset()), lengthGPR);
    loadPtr(Address(tempGPR, StringImpl::dataOffset()), tempGPR);

    auto loopStart = label();
    auto loopDone = branch32(AboveOrEqual, indexGPR, lengthGPR);
    load8(BaseIndex(tempGPR, indexGPR, TimesOne), charGPR);
    slowPath.append(branchTest32(NonZero, charGPR, TrustedImm32(~0x7F)));
    sub32(TrustedImm32('A'), charGPR);
    slowPath.append(branch32(BelowOrEqual, charGPR, TrustedImm32('Z' - 'A')));

    add32(TrustedImm32(1), indexGPR);
    jump().linkTo(loopStart, this);
    
    slowPath.link(this);
    callOperationWithSilentSpill(operationToLowerCase, lengthGPR, LinkableConstant::globalObject(*this, node), stringGPR, indexGPR);
    auto done = jump();

    loopDone.link(this);
    move(stringGPR, lengthGPR);

    done.link(this);
    cellResult(lengthGPR, node);
}

void SpeculativeJIT::compilePeepHoleInt32Branch(Node* node, Node* branchNode, RelationalCondition condition)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        condition = invert(condition);
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    if (node->child1()->isInt32Constant()) {
        int32_t imm = node->child1()->asInt32();
        SpeculateInt32Operand op2(this, node->child2());
        branch32(condition, Imm32(imm), op2.gpr(), taken);
    } else if (node->child2()->isInt32Constant()) {
        SpeculateInt32Operand op1(this, node->child1());
        int32_t imm = node->child2()->asInt32();
        branch32(condition, op1.gpr(), Imm32(imm), taken);
    } else {
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        branch32(condition, op1.gpr(), op2.gpr(), taken);
    }

    jump(notTaken);
}

// Returns true if the compare is fused with a subsequent branch.
bool SpeculativeJIT::compilePeepHoleBranch(Node* node, RelationalCondition condition, DoubleCondition doubleCondition, S_JITOperation_GJJ operation)
{
    // Fused compare & branch.
    unsigned branchIndexInBlock = detectPeepHoleBranch();
    if (branchIndexInBlock != UINT_MAX) {
        Node* branchNode = m_block->at(branchIndexInBlock);

        // detectPeepHoleBranch currently only permits the branch to be the very next node,
        // so can be no intervening nodes to also reference the compare. 
        ASSERT(node->adjustedRefCount() == 1);

        if (node->isBinaryUseKind(Int32Use))
            compilePeepHoleInt32Branch(node, branchNode, condition);
#if USE(BIGINT32)
        else if (node->isBinaryUseKind(BigInt32Use))
            compilePeepHoleBigInt32Branch(node, branchNode, condition);
#endif
#if USE(JSVALUE64)
        else if (node->isBinaryUseKind(Int52RepUse))
            compilePeepHoleInt52Branch(node, branchNode, condition);
#endif // USE(JSVALUE64)
        else if (node->isBinaryUseKind(StringUse) || node->isBinaryUseKind(StringIdentUse)) {
            // Use non-peephole comparison, for now.
            return false;
        } else if (node->isBinaryUseKind(DoubleRepUse))
            compilePeepHoleDoubleBranch(node, branchNode, doubleCondition);
        else if (node->op() == CompareEq) {
            if (node->isBinaryUseKind(BooleanUse))
                compilePeepHoleBooleanBranch(node, branchNode, condition);
            else if (node->isBinaryUseKind(SymbolUse))
                compilePeepHoleSymbolEquality(node, branchNode);
            else if (node->isBinaryUseKind(ObjectUse))
                compilePeepHoleObjectEquality(node, branchNode);
            else if (node->isBinaryUseKind(ObjectUse, ObjectOrOtherUse))
                compilePeepHoleObjectToObjectOrOtherEquality(node->child1(), node->child2(), branchNode);
            else if (node->isBinaryUseKind(ObjectOrOtherUse, ObjectUse))
                compilePeepHoleObjectToObjectOrOtherEquality(node->child2(), node->child1(), branchNode);
            else if (!needsTypeCheck(node->child1(), SpecOther))
                nonSpeculativePeepholeBranchNullOrUndefined(node->child2(), branchNode);
            else if (!needsTypeCheck(node->child2(), SpecOther))
                nonSpeculativePeepholeBranchNullOrUndefined(node->child1(), branchNode);
            else {
                genericJSValuePeepholeBranch(node, branchNode, condition, operation);
                return true;
            }
        } else {
            genericJSValuePeepholeBranch(node, branchNode, condition, operation);
            return true;
        }

        use(node->child1());
        use(node->child2());
        m_indexInBlock = branchIndexInBlock;
        m_currentNode = branchNode;
        return true;
    }
    return false;
}

void SpeculativeJIT::noticeOSRBirth(Node* node)
{
    if (!node->hasVirtualRegister())
        return;
    
    VirtualRegister virtualRegister = node->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
    
    info.noticeOSRBirth(m_stream, node, virtualRegister);
}

void SpeculativeJIT::compileLoopHint(Node* node)
{
    if (UNLIKELY(Options::returnEarlyFromInfiniteLoopsForFuzzing())) {
        bool emitEarlyReturn = true;
        node->origin.semantic.walkUpInlineStack([&](CodeOrigin origin) {
            CodeBlock* baselineCodeBlock = m_graph.baselineCodeBlockFor(origin);
            if (!baselineCodeBlock->loopHintsAreEligibleForFuzzingEarlyReturn())
                emitEarlyReturn = false;
        });
        if (emitEarlyReturn) {
            CodeBlock* baselineCodeBlock = m_graph.baselineCodeBlockFor(node->origin.semantic);
            BytecodeIndex bytecodeIndex = node->origin.semantic.bytecodeIndex();
            const auto* instruction = baselineCodeBlock->instructions().at(bytecodeIndex.offset()).ptr();

            uintptr_t* ptr = vm().getLoopHintExecutionCounter(instruction);
            pushToSave(GPRInfo::regT0);
            loadPtr(ptr, GPRInfo::regT0);
            auto skipEarlyReturn = branchPtr(Below, GPRInfo::regT0, TrustedImmPtr(Options::earlyReturnFromInfiniteLoopsLimit()));

            if constexpr (validateDFGDoesGC) {
                if (Options::validateDoesGC()) {
                    // We need to mock what a Return does: claims to GC.
                    DoesGCCheck check;
                    check.u.encoded = DoesGCCheck::encode(true, DoesGCCheck::Special::Uninitialized);
#if USE(JSVALUE64)
                    store64(TrustedImm64(check.u.encoded), vm().addressOfDoesGC());
#else
                    store32(TrustedImm32(check.u.other), &vm().addressOfDoesGC()->u.other);
                    store32(TrustedImm32(check.u.nodeIndex), &vm().addressOfDoesGC()->u.nodeIndex);
#endif
                }
            }

            popToRestore(GPRInfo::regT0);

            constexpr JSValueRegs resultRegs = JSRInfo::returnValueJSR;

            loadLinkableConstant(LinkableConstant::globalObject(*this, node), resultRegs.payloadGPR());
            loadPtr(Address(resultRegs.payloadGPR(), JSGlobalObject::offsetOfGlobalThis()), resultRegs.payloadGPR());
            boxCell(resultRegs.payloadGPR(), resultRegs);
            emitRestoreCalleeSaves();
            emitFunctionEpilogue();
            ret();

            skipEarlyReturn.link(this);
            addPtr(TrustedImm32(1), GPRInfo::regT0);
            storePtr(GPRInfo::regT0, ptr);
            popToRestore(GPRInfo::regT0);
        }
    }

    noResult(node);
}

void SpeculativeJIT::compileMovHint(Node* node)
{
    ASSERT(node->containsMovHint());
    
    Node* child = node->child1().node();
    noticeOSRBirth(child);
    
    m_stream.appendAndLog(VariableEvent::movHint(MinifiedID(child), node->unlinkedOperand()));
}

void SpeculativeJIT::compileCheckDetached(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRReg baseReg = base.gpr();

    speculationCheck(
        BadIndexingType, JSValueSource::unboxedCell(baseReg), node->child1(), 
        branchTestPtr(Zero, Address(baseReg, JSArrayBufferView::offsetOfVector())));

    noResult(node);
}

void SpeculativeJIT::bail(AbortReason reason)
{
    if (verboseCompilationEnabled())
        dataLog("Bailing compilation.\n");
    m_compileOkay = true;
    abortWithReason(reason, m_lastGeneratedNode);
    clearGenerationInfo();
}

void SpeculativeJIT::compileCurrentBlock()
{
    ASSERT(m_compileOkay);
    
    if (!m_block)
        return;
    
    ASSERT(m_block->isReachable);
    
    blockHeads()[m_block->index] = label();

    if (!m_block->intersectionOfCFAHasVisited) {
        // Don't generate code for basic blocks that are unreachable according to CFA.
        // But to be sure that nobody has generated a jump to this block, drop in a
        // breakpoint here.
        abortWithReason(DFGUnreachableBasicBlock);
        return;
    }

    if (m_block->isCatchEntrypoint) {
        addPtr(TrustedImm32(-(m_graph.frameRegisterCount() * sizeof(Register))), GPRInfo::callFrameRegister,  stackPointerRegister);
        compileSetupRegistersForEntry();
    }

    m_stream.appendAndLog(VariableEvent::reset());
    
    jitAssertHasValidCallFrame();
    jitAssertTagsInPlace();
    jitAssertArgumentCountSane();

    m_state.reset();
    m_state.beginBasicBlock(m_block);
    
    for (size_t i = m_block->variablesAtHead.size(); i--;) {
        Operand operand = m_block->variablesAtHead.operandForIndex(i);
        Node* node = m_block->variablesAtHead[i];
        if (!node)
            continue; // No need to record dead SetLocal's.
        
        VariableAccessData* variable = node->variableAccessData();
        DataFormat format;
        if (!node->refCount())
            continue; // No need to record dead SetLocal's.
        format = dataFormatFor(variable->flushFormat());
        DFG_ASSERT(m_graph, node, !operand.isArgument() || operand.virtualRegister().toArgument() >= 0);
        m_stream.appendAndLog(VariableEvent::setLocal(operand, variable->machineLocal(), format));
    }

    m_origin = NodeOrigin();

    if (Options::validateDFGClobberize()) {
        bool clobberedWorld = m_block->predecessors.isEmpty() || m_block->isOSRTarget || m_block->isCatchEntrypoint;
        auto validateClobberize = [&] () {
            clobberedWorld = true;
        };

        for (auto* predecessor : m_block->predecessors) {
            Node* terminal = predecessor->terminal();
            // We sometimes fuse compare followed by branch.
            if (terminal->isBranch())
                terminal = terminal->child1().node();
            clobberize(m_graph, terminal, [] (auto...) { }, [] (auto...) { }, [] (auto...) { }, validateClobberize);
        }

        if (!clobberedWorld) {
            auto ok = branchTest8(Zero, AbsoluteAddress(&vm().didEnterVM));
            breakpoint();
            ok.link(this);
        } else
            store8(TrustedImm32(0), &vm().didEnterVM);
    }

    for (m_indexInBlock = 0; m_indexInBlock < m_block->size(); ++m_indexInBlock) {
        m_currentNode = m_block->at(m_indexInBlock);
        
        // We may have hit a contradiction that the CFA was aware of but that the JIT
        // didn't cause directly.
        if (!m_state.isValid()) {
            bail(DFGBailedAtTopOfBlock);
            return;
        }

        m_interpreter.startExecuting();
        m_interpreter.executeKnownEdgeTypes(m_currentNode);
        setForNode(m_currentNode);
        m_origin = m_currentNode->origin;
        m_lastGeneratedNode = m_currentNode->op();
        
        ASSERT(m_currentNode->shouldGenerate());
        
        if (verboseCompilationEnabled())
            dataLogLn("SpeculativeJIT generating Node @", (int)m_currentNode->index(), " (", m_currentNode->origin.semantic.bytecodeIndex().offset(), ") at JIT offset 0x", debugOffset());

        if (Options::validateDFGExceptionHandling() && (mayExit(m_graph, m_currentNode) != DoesNotExit || m_currentNode->isTerminal()))
            jitReleaseAssertNoException(vm());

        pcToCodeOriginMapBuilder().appendItem(labelIgnoringWatchpoints(), m_origin.semantic);

        if (m_indexInBlock && Options::validateDFGClobberize()) {
            bool clobberedWorld = false;
            auto validateClobberize = [&] () {
                clobberedWorld = true;
            };

            clobberize(m_graph, m_block->at(m_indexInBlock - 1), [] (auto...) { }, [] (auto...) { }, [] (auto...) { }, validateClobberize);
            if (!clobberedWorld) {
                auto ok = branchTest8(Zero, AbsoluteAddress(&vm().didEnterVM));
                breakpoint();
                ok.link(this);
            } else
                store8(TrustedImm32(0), &vm().didEnterVM);
        }

        std::optional<JITSizeStatistics::Marker> sizeMarker;
        if (UNLIKELY(Options::dumpDFGJITSizeStatistics())) {
            String id = makeString("DFG_fast_"_s, m_graph.opName(m_currentNode->op()));
            sizeMarker = vm().jitSizeStatistics->markStart(id, *this);
        }

        compile(m_currentNode);

        if (UNLIKELY(sizeMarker))
            vm().jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this, m_graph.m_plan);

        if (belongsInMinifiedGraph(m_currentNode->op()))
            m_minifiedGraph->append(MinifiedNode::fromNode(m_currentNode));
        
#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
        clearRegisterAllocationOffsets();
#endif
        
        if (!m_compileOkay) {
            bail(DFGBailedAtEndOfNode);
            return;
        }
        
        // Make sure that the abstract state is rematerialized for the next node.
        m_interpreter.executeEffects(m_indexInBlock);
    }
    
    // Perform the most basic verification that children have been used correctly.
    if (ASSERT_ENABLED) {
        for (auto& info : m_generationInfo)
            RELEASE_ASSERT(!info.alive());
    }
}

// If we are making type predictions about our arguments then
// we need to check that they are correct on function entry.
void SpeculativeJIT::checkArgumentTypes()
{
    ASSERT(!m_currentNode);
    m_origin = NodeOrigin(CodeOrigin(BytecodeIndex(0)), CodeOrigin(BytecodeIndex(0)), true);

    auto& arguments = m_graph.m_rootToArguments.find(m_graph.block(0))->value;
    for (unsigned i = 0; i < codeBlock()->numParameters(); ++i) {
        Node* node = arguments[i];
        if (!node) {
            // The argument is dead. We don't do any checks for such arguments.
            continue;
        }
        
        ASSERT(node->op() == SetArgumentDefinitely);
        ASSERT(node->shouldGenerate());

        VariableAccessData* variableAccessData = node->variableAccessData();
        FlushFormat format = variableAccessData->flushFormat();
        
        if (format == FlushedJSValue)
            continue;
        
        VirtualRegister virtualRegister = variableAccessData->operand().virtualRegister();
        ASSERT(virtualRegister.isArgument());

        JSValueSource valueSource = JSValueSource(addressFor(virtualRegister));
        
#if USE(JSVALUE64)
        switch (format) {
        case FlushedInt32: {
            speculationCheck(BadType, valueSource, node, branch64(Below, addressFor(virtualRegister), GPRInfo::numberTagRegister));
            break;
        }
        case FlushedBoolean: {
            GPRTemporary temp(this);
            load64(addressFor(virtualRegister), temp.gpr());
            xor64(TrustedImm32(JSValue::ValueFalse), temp.gpr());
            speculationCheck(BadType, valueSource, node, branchTest64(NonZero, temp.gpr(), TrustedImm32(static_cast<int32_t>(~1))));
            break;
        }
        case FlushedCell: {
            speculationCheck(BadType, valueSource, node, branchTest64(NonZero, addressFor(virtualRegister), GPRInfo::notCellMaskRegister));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
#else
        switch (format) {
        case FlushedInt32: {
            speculationCheck(BadType, valueSource, node, branch32(NotEqual, tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag)));
            break;
        }
        case FlushedBoolean: {
            speculationCheck(BadType, valueSource, node, branch32(NotEqual, tagFor(virtualRegister), TrustedImm32(JSValue::BooleanTag)));
            break;
        }
        case FlushedCell: {
            speculationCheck(BadType, valueSource, node, branchIfNotCell(tagFor(virtualRegister)));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
#endif
    }

    m_origin = NodeOrigin();
}

void SpeculativeJIT::compileBody()
{
    checkArgumentTypes();
    
    ASSERT(!m_currentNode);
    for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
        setForBlockIndex(blockIndex);
        m_block = m_graph.block(blockIndex);
        compileCurrentBlock();
    }
    linkBranches();
}

void SpeculativeJIT::createOSREntries()
{
    for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
        BasicBlock* block = m_graph.block(blockIndex);
        if (!block)
            continue;
        if (block->isOSRTarget || block->isCatchEntrypoint) {
            // Currently we don't have OSR entry trampolines. We could add them
            // here if need be.
            m_osrEntryHeads.append(blockHeads()[blockIndex]);
        }
    }
}

void SpeculativeJIT::linkOSREntries(LinkBuffer& linkBuffer)
{
    unsigned osrEntryIndex = 0;
    for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
        BasicBlock* block = m_graph.block(blockIndex);
        if (!block)
            continue;
        if (!block->isOSRTarget && !block->isCatchEntrypoint)
            continue;
        if (block->isCatchEntrypoint) {
            auto& argumentsVector = m_graph.m_rootToArguments.find(block)->value;
            auto argumentFormats = argumentsVector.map([](auto* setArgument) {
                if (setArgument) {
                    FlushFormat flushFormat = setArgument->variableAccessData()->flushFormat();
                    ASSERT(flushFormat == FlushedInt32 || flushFormat == FlushedCell || flushFormat == FlushedBoolean || flushFormat == FlushedJSValue);
                    return flushFormat;
                }
                return DeadFlush;
            });
            noticeCatchEntrypoint(*block, m_osrEntryHeads[osrEntryIndex++], linkBuffer, WTFMove(argumentFormats));
        } else {
            ASSERT(block->isOSRTarget);
            noticeOSREntry(*block, m_osrEntryHeads[osrEntryIndex++], linkBuffer);
        }
    }

    jitCode()->finalizeOSREntrypoints(WTFMove(m_osrEntry));
    jitCode()->common.finalizeCatchEntrypoints(WTFMove(m_graph.m_catchEntrypoints));

    ASSERT(osrEntryIndex == m_osrEntryHeads.size());
    
    if (verboseCompilationEnabled()) {
        DumpContext dumpContext;
        dataLog("OSR Entries:\n");
        for (OSREntryData& entryData : jitCode()->m_osrEntry)
            dataLog("    ", inContext(entryData, &dumpContext), "\n");
        if (!dumpContext.isEmpty())
            dumpContext.dump(WTF::dataFile());
    }
}
    
void SpeculativeJIT::compileCheckTraps(Node* node)
{
    ASSERT(Options::usePollingTraps() || m_graph.m_plan.isUnlinked());
    GPRTemporary unused(this);
    GPRReg unusedGPR = unused.gpr();

    Jump needTrapHandling = branchTest32(NonZero,
        AbsoluteAddress(vm().traps().trapBitsAddress()),
        TrustedImm32(VMTraps::AsyncEvents));

    addSlowPathGenerator(slowPathCall(needTrapHandling, this, operationHandleTraps, unusedGPR, LinkableConstant::globalObject(*this, node)));
    noResult(node);
}

void SpeculativeJIT::compileContiguousPutByVal(Node* node)
{
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    JSValueOperand value(this, m_graph.varArgChild(node, 2), ManualOperandSpeculation);

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    if (!m_compileOkay)
        return;

    StorageOperand storage(this, m_graph.varArgChild(node, 3));
    GPRReg storageReg = storage.gpr();

    if (node->op() == PutByValAlias) {
        // Store the value to the array.
        GPRReg propertyReg = property.gpr();
        storeValue(valueRegs, BaseIndex(storageReg, propertyReg, TimesEight));
        noResult(node);
        return;
    }

    GPRTemporary temporary;
    GPRReg temporaryReg = temporaryRegisterForPutByVal(temporary, node);

    Jump slowCase;

    ArrayMode arrayMode = node->arrayMode();
    if (arrayMode.isInBounds()) {
        speculationCheck(
            OutOfBounds, JSValueRegs(), nullptr,
            branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));
    } else {
        Jump inBounds = branch32(Below, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength()));

        slowCase = branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfVectorLength()));

        if (!arrayMode.isOutOfBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, slowCase);

        add32(TrustedImm32(1), propertyReg, temporaryReg);
        store32(temporaryReg, Address(storageReg, Butterfly::offsetOfPublicLength()));

        inBounds.link(this);
    }

    storeValue(valueRegs, BaseIndex(storageReg, propertyReg, TimesEight));

    base.use();
    property.use();
    value.use();
    storage.use();

    if (arrayMode.isOutOfBounds()) {
        addSlowPathGenerator(slowPathCall(
            slowCase, this,
            node->ecmaMode().isStrict() ?
                (node->op() == PutByValDirect ? operationPutByValDirectBeyondArrayBoundsStrict : operationPutByValBeyondArrayBoundsStrict) :
                (node->op() == PutByValDirect ? operationPutByValDirectBeyondArrayBoundsSloppy : operationPutByValBeyondArrayBoundsSloppy),
            NoResult, LinkableConstant::globalObject(*this, node), baseReg, propertyReg, valueRegs));
    }

    noResult(node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileDoublePutByVal(Node* node)
{
    ArrayMode arrayMode = node->arrayMode();

    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    SpeculateDoubleOperand value(this, m_graph.varArgChild(node, 2));

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    FPRReg valueReg = value.fpr();

    DFG_TYPE_CHECK(
        JSValueRegs(), m_graph.varArgChild(node, 2), SpecFullRealNumber,
        branchIfNaN(valueReg));

    if (!m_compileOkay)
        return;

    StorageOperand storage(this, m_graph.varArgChild(node, 3));
    GPRReg storageReg = storage.gpr();

    if (node->op() == PutByValAlias) {
        // Store the value to the array.
        GPRReg propertyReg = property.gpr();
        FPRReg valueReg = value.fpr();
        storeDouble(valueReg, BaseIndex(storageReg, propertyReg, TimesEight));

        noResult(m_currentNode);
        return;
    }

    GPRTemporary temporary;
    GPRReg temporaryReg = temporaryRegisterForPutByVal(temporary, node);

    Jump slowCase;

    if (arrayMode.isInBounds()) {
        speculationCheck(
            OutOfBounds, JSValueRegs(), nullptr,
            branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength())));
    } else {
        Jump inBounds = branch32(Below, propertyReg, Address(storageReg, Butterfly::offsetOfPublicLength()));

        slowCase = branch32(AboveOrEqual, propertyReg, Address(storageReg, Butterfly::offsetOfVectorLength()));

        if (!arrayMode.isOutOfBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, slowCase);

        add32(TrustedImm32(1), propertyReg, temporaryReg);
        store32(temporaryReg, Address(storageReg, Butterfly::offsetOfPublicLength()));

        inBounds.link(this);
    }

    storeDouble(valueReg, BaseIndex(storageReg, propertyReg, TimesEight));

    base.use();
    property.use();
    value.use();
    storage.use();

    if (arrayMode.isOutOfBounds()) {
        addSlowPathGenerator(
            slowPathCall(
                slowCase, this,
                node->ecmaMode().isStrict() ?
                    (node->op() == PutByValDirect ? operationPutDoubleByValDirectBeyondArrayBoundsStrict : operationPutDoubleByValBeyondArrayBoundsStrict) :
                    (node->op() == PutByValDirect ? operationPutDoubleByValDirectBeyondArrayBoundsSloppy : operationPutDoubleByValBeyondArrayBoundsSloppy),
                NoResult, LinkableConstant::globalObject(*this, node), baseReg, propertyReg, valueReg));
    }

    noResult(m_currentNode, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileGetCharCodeAt(Node* node)
{
    SpeculateCellOperand string(this, node->child1());
    SpeculateStrictInt32Operand index(this, node->child2());

    GPRReg stringReg = string.gpr();
    GPRReg indexReg = index.gpr();
    
    ASSERT(speculationChecked(m_state.forNode(node->child1()).m_type, SpecString));

    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();

    loadPtr(Address(stringReg, JSString::offsetOfValue()), scratchReg);
    
    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(Uncountable, JSValueRegs(), nullptr, branch32(AboveOrEqual, indexReg, Address(scratchReg, StringImpl::lengthMemoryOffset())));

    // Load the character into scratchReg
    Jump is16Bit = branchTest32(Zero, Address(scratchReg, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    loadPtr(Address(scratchReg, StringImpl::dataOffset()), scratchReg);
    load8(BaseIndex(scratchReg, indexReg, TimesOne, 0), scratchReg);
    Jump cont8Bit = jump();

    is16Bit.link(this);

    loadPtr(Address(scratchReg, StringImpl::dataOffset()), scratchReg);
    load16(BaseIndex(scratchReg, indexReg, TimesTwo, 0), scratchReg);

    cont8Bit.link(this);

    strictInt32Result(scratchReg, m_currentNode);
}

void SpeculativeJIT::compileGetByValOnString(Node* node, const ScopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>& prefix)
{
    SpeculateCellOperand base(this, m_graph.child(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.child(node, 1));
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();

    JumpList doneCases;

    JSValueRegs resultRegs;
    DataFormat format;
    constexpr bool needsFlush = false;
    std::tie(resultRegs, format) = prefix(node->arrayMode().isOutOfBounds() ? DataFormatJS : DataFormatCell, needsFlush);
    GPRReg scratchReg = resultRegs.payloadGPR();

    // unsigned comparison so we can filter out negative indices and indices that are too large
    loadPtr(Address(baseReg, JSString::offsetOfValue()), scratchReg);
    Jump outOfBounds = branch32(
        AboveOrEqual, propertyReg,
        Address(scratchReg, StringImpl::lengthMemoryOffset()));
    if (node->op() != StringCharAt) {
        if (node->arrayMode().isInBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, outOfBounds);
    }

    // Load the character into scratchReg
    Jump is16Bit = branchTest32(Zero, Address(scratchReg, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    loadPtr(Address(scratchReg, StringImpl::dataOffset()), scratchReg);
    load8(BaseIndex(scratchReg, propertyReg, TimesOne, 0), scratchReg);
    Jump cont8Bit = jump();

    if (node->op() == StringCharAt) {
        outOfBounds.link(this);
#if USE(JSVALUE32_64)
        if (format == DataFormatJS)
            move(TrustedImm32(JSValue::CellTag), resultRegs.tagGPR());
#endif
        loadLinkableConstant(LinkableConstant(*this, jsEmptyString(vm())), resultRegs.payloadGPR());
        doneCases.append(jump());
    }

    is16Bit.link(this);

    loadPtr(Address(scratchReg, StringImpl::dataOffset()), scratchReg);
    load16(BaseIndex(scratchReg, propertyReg, TimesTwo, 0), scratchReg);

    Jump bigCharacter =
        branch32(Above, scratchReg, TrustedImm32(maxSingleCharacterString));

    // 8 bit string values don't need the isASCII check.
    cont8Bit.link(this);

    VM& vm = this->vm();
    lshift32(TrustedImm32(sizeof(void*) == 4 ? 2 : 3), scratchReg);
    addPtr(TrustedImmPtr(vm.smallStrings.singleCharacterStrings()), scratchReg);
    loadPtr(Address(scratchReg), scratchReg);

    addSlowPathGenerator(
        slowPathCall(
            bigCharacter, this, operationSingleCharacterString, scratchReg, TrustedImmPtr(&vm), scratchReg));

    if (node->op() != StringCharAt && node->arrayMode().isOutOfBounds()) {
        ASSERT(format == DataFormatJS);
#if USE(JSVALUE32_64)
        move(TrustedImm32(JSValue::CellTag), resultRegs.tagGPR());
#endif

        if (m_graph.isWatchingStringPrototypeChainIsSaneWatchpoint(node)) {
            // FIXME: This could be captured using a Speculation mode that means "out-of-bounds
            // loads return a trivial value". Something like OutOfBoundsSaneChain. This should
            // speculate that we don't take negative out-of-bounds, or better yet, it should rely
            // on a stringPrototypeChainIsSaneConcurrently() guaranteeing that the prototypes have no negative
            // indexed properties either.
            // https://bugs.webkit.org/show_bug.cgi?id=144668
            addSlowPathGenerator(makeUnique<SaneStringGetByValSlowPathGenerator>(
                outOfBounds, this, resultRegs, LinkableConstant::globalObject(*this, node), baseReg, propertyReg));
        } else {
            addSlowPathGenerator(
                slowPathCall(
                    outOfBounds, this, operationGetByValStringInt,
                    resultRegs, LinkableConstant::globalObject(*this, node), baseReg, propertyReg));
        }

        jsValueResult(resultRegs, m_currentNode);
        return;
    }

    doneCases.link(this);
    if (format == DataFormatJS)
        jsValueResult(resultRegs, m_currentNode);
    else {
        ASSERT(format == DataFormatCell);
        cellResult(resultRegs.payloadGPR(), m_currentNode);
    }
}

void SpeculativeJIT::compileFromCharCode(Node* node)
{
    Edge& child = node->child1();
    if (child.useKind() == UntypedUse) {
        JSValueOperand opr(this, child);
        JSValueRegs oprRegs = opr.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationStringFromCharCodeUntyped, resultRegs, LinkableConstant::globalObject(*this, node), oprRegs);
        
        jsValueResult(resultRegs, node);
        return;
    }

    SpeculateStrictInt32Operand property(this, child);
    GPRReg propertyReg = property.gpr();
    GPRTemporary smallStrings(this);
    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();
    GPRReg smallStringsReg = smallStrings.gpr();

    JumpList slowCases;
    slowCases.append(branch32(Above, propertyReg, TrustedImm32(maxSingleCharacterString)));
    move(TrustedImmPtr(vm().smallStrings.singleCharacterStrings()), smallStringsReg);
    loadPtr(BaseIndex(smallStringsReg, propertyReg, ScalePtr, 0), scratchReg);

    slowCases.append(branchTest32(Zero, scratchReg));
    addSlowPathGenerator(slowPathCall(slowCases, this, operationStringFromCharCode, scratchReg, LinkableConstant::globalObject(*this, node), propertyReg));
    cellResult(scratchReg, m_currentNode);
}

GeneratedOperandType SpeculativeJIT::checkGeneratedTypeForToInt32(Node* node)
{
    VirtualRegister virtualRegister = node->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatStorage:
        RELEASE_ASSERT_NOT_REACHED();

    case DataFormatBoolean:
    case DataFormatCell:
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), nullptr);
        return GeneratedOperandTypeUnknown;

    case DataFormatNone:
    case DataFormatJSCell:
    case DataFormatJS:
    case DataFormatJSBoolean:
    case DataFormatJSDouble:
    case DataFormatJSBigInt32:
        return GeneratedOperandJSValue;

    case DataFormatJSInt32:
    case DataFormatInt32:
        return GeneratedOperandInteger;

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return GeneratedOperandTypeUnknown;
    }
}

void SpeculativeJIT::compileValueToInt32(Node* node)
{
    switch (node->child1().useKind()) {
#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand op1(this, node->child1());
        GPRTemporary result(this, Reuse, op1);
        GPRReg op1GPR = op1.gpr();
        GPRReg resultGPR = result.gpr();
        zeroExtend32ToWord(op1GPR, resultGPR);
        strictInt32Result(resultGPR, node, DataFormatInt32);
        return;
    }
#endif // USE(JSVALUE64)
        
    case DoubleRepUse: {
        GPRTemporary result(this);
        SpeculateDoubleOperand op1(this, node->child1());
        FPRReg fpr = op1.fpr();
        GPRReg gpr = result.gpr();
#if CPU(ARM64)
        if (MacroAssemblerARM64::supportsDoubleToInt32ConversionUsingJavaScriptSemantics())
            convertDoubleToInt32UsingJavaScriptSemantics(fpr, gpr);
        else
#endif
        {
            Jump notTruncatedToInteger = branchTruncateDoubleToInt32(fpr, gpr, BranchIfTruncateFailed);
            addSlowPathGenerator(slowPathCall(notTruncatedToInteger, this,
                hasSensibleDoubleToInt() ? operationToInt32SensibleSlow : operationToInt32, NeedToSpill, ExceptionCheckRequirement::CheckNotNeeded, gpr, fpr));
        }
        strictInt32Result(gpr, node);
        return;
    }
    
    case NumberUse:
    case NotCellNorBigIntUse: {
        switch (checkGeneratedTypeForToInt32(node->child1().node())) {
        case GeneratedOperandInteger: {
            SpeculateInt32Operand op1(this, node->child1(), ManualOperandSpeculation);
            GPRTemporary result(this, Reuse, op1);
            move(op1.gpr(), result.gpr());
            strictInt32Result(result.gpr(), node, op1.format());
            return;
        }
        case GeneratedOperandJSValue: {
            GPRTemporary result(this);
#if USE(JSVALUE64)
            JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);

            GPRReg gpr = op1.gpr();
            GPRReg resultGpr = result.gpr();
            FPRTemporary tempFpr(this);
            FPRReg fpr = tempFpr.fpr();

            Jump isInteger = branchIfInt32(gpr);
            JumpList converted;

            if (node->child1().useKind() == NumberUse) {
                DFG_TYPE_CHECK(
                    JSValueRegs(gpr), node->child1(), SpecBytecodeNumber,
                    branchIfNotNumber(gpr));
            } else {
                Jump isNumber = branchIfNumber(gpr);
                
                DFG_TYPE_CHECK(
                    JSValueRegs(gpr), node->child1(), ~SpecCellCheck, branchIfCell(JSValueRegs(gpr)));
#if USE(BIGINT32)
                DFG_TYPE_CHECK(
                    JSValueRegs(gpr), node->child1(), ~SpecCellCheck & ~SpecBigInt, branchIfBigInt32(JSValueRegs(gpr), resultGpr));
#endif
                
                // It's not a cell: so true turns into 1 and all else turns into 0.
                compare64(Equal, gpr, TrustedImm32(JSValue::ValueTrue), resultGpr);
                converted.append(jump());
                
                isNumber.link(this);
            }

            // First, if we get here we have a double encoded as a JSValue
            unboxDouble(gpr, resultGpr, fpr);
#if CPU(ARM64)
            if (MacroAssemblerARM64::supportsDoubleToInt32ConversionUsingJavaScriptSemantics())
                convertDoubleToInt32UsingJavaScriptSemantics(fpr, resultGpr);
            else
#endif
            {
                silentSpillAllRegisters(resultGpr);
                callOperationWithoutExceptionCheck(operationToInt32, resultGpr, fpr);
                silentFillAllRegisters();
            }

            converted.append(jump());

            isInteger.link(this);
            zeroExtend32ToWord(gpr, resultGpr);

            converted.link(this);
#else
            Node* childNode = node->child1().node();
            VirtualRegister virtualRegister = childNode->virtualRegister();
            GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

            JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);

            GPRReg payloadGPR = op1.payloadGPR();
            GPRReg resultGpr = result.gpr();
        
            JumpList converted;

            if (info.registerFormat() == DataFormatJSInt32)
                move(payloadGPR, resultGpr);
            else {
                GPRReg tagGPR = op1.tagGPR();
                FPRTemporary tempFpr(this);
                FPRReg fpr = tempFpr.fpr();

                Jump isInteger = branchIfInt32(tagGPR);

                if (node->child1().useKind() == NumberUse) {
                    DFG_TYPE_CHECK(
                        op1.jsValueRegs(), node->child1(), SpecBytecodeNumber,
                        branch32(
                            AboveOrEqual, tagGPR,
                            TrustedImm32(JSValue::LowestTag)));
                } else {
                    Jump isNumber = branch32(Below, tagGPR, TrustedImm32(JSValue::LowestTag));
                    
                    DFG_TYPE_CHECK(
                        op1.jsValueRegs(), node->child1(), ~SpecCell,
                        branchIfCell(op1.jsValueRegs()));
                    
                    // It's not a cell: so true turns into 1 and all else turns into 0.
                    Jump isBoolean = branchIfBoolean(tagGPR, InvalidGPRReg);
                    move(TrustedImm32(0), resultGpr);
                    converted.append(jump());
                    
                    isBoolean.link(this);
                    move(payloadGPR, resultGpr);
                    converted.append(jump());
                    
                    isNumber.link(this);
                }

                unboxDouble(tagGPR, payloadGPR, fpr);

                silentSpillAllRegisters(resultGpr);
                callOperationWithoutExceptionCheck(operationToInt32, resultGpr, fpr);
                silentFillAllRegisters();

                converted.append(jump());

                isInteger.link(this);
                move(payloadGPR, resultGpr);

                converted.link(this);
            }
#endif
            strictInt32Result(resultGpr, node);
            return;
        }
        case GeneratedOperandTypeUnknown:
            RELEASE_ASSERT(!m_compileOkay);
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
    
    default:
        ASSERT(!m_compileOkay);
        return;
    }
}

void SpeculativeJIT::compileUInt32ToNumber(Node* node)
{
    if (doesOverflow(node->arithMode())) {
        if (enableInt52()) {
            SpeculateInt32Operand op1(this, node->child1());
            GPRTemporary result(this, Reuse, op1);
            zeroExtend32ToWord(op1.gpr(), result.gpr());
            strictInt52Result(result.gpr(), node);
            return;
        }
        SpeculateInt32Operand op1(this, node->child1());
        FPRTemporary result(this);
            
        GPRReg inputGPR = op1.gpr();
        FPRReg outputFPR = result.fpr();
            
        convertInt32ToDouble(inputGPR, outputFPR);
            
        Jump positive = branch32(GreaterThanOrEqual, inputGPR, TrustedImm32(0));
        addDouble(AbsoluteAddress(&twoToThe32), outputFPR);
        positive.link(this);
            
        doubleResult(outputFPR, node);
        return;
    }
    
    RELEASE_ASSERT(node->arithMode() == Arith::CheckOverflow);

    SpeculateInt32Operand op1(this, node->child1());
    GPRTemporary result(this);

    move(op1.gpr(), result.gpr());

    speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, Base::branch32(LessThan, result.gpr(), TrustedImm32(0)));

    strictInt32Result(result.gpr(), node, op1.format());
}

void SpeculativeJIT::compileDoubleAsInt32(Node* node)
{
    SpeculateDoubleOperand op1(this, node->child1());
    FPRTemporary scratch(this);
    GPRTemporary result(this);
    
    FPRReg valueFPR = op1.fpr();
    FPRReg scratchFPR = scratch.fpr();
    GPRReg resultGPR = result.gpr();

    JumpList failureCases;
    RELEASE_ASSERT(shouldCheckOverflow(node->arithMode()));
    branchConvertDoubleToInt32(
        valueFPR, resultGPR, failureCases, scratchFPR,
        shouldCheckNegativeZero(node->arithMode()));
    speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, failureCases);

    strictInt32Result(resultGPR, node);
}

void SpeculativeJIT::compileDoubleRep(Node* node)
{
    switch (node->child1().useKind()) {
    case RealNumberUse: {
        JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
        FPRTemporary result(this);
        
        JSValueRegs op1Regs = op1.jsValueRegs();
        FPRReg resultFPR = result.fpr();
        
#if USE(JSVALUE64)
        GPRTemporary temp(this);
        GPRReg tempGPR = temp.gpr();
        unboxDoubleWithoutAssertions(op1Regs.gpr(), tempGPR, resultFPR);
#else
        unboxDouble(op1Regs.tagGPR(), op1Regs.payloadGPR(), resultFPR);
#endif
        
        Jump done = branchIfNotNaN(resultFPR);
        
        DFG_TYPE_CHECK(
            op1Regs, node->child1(), SpecBytecodeRealNumber, branchIfNotInt32(op1Regs));
        convertInt32ToDouble(op1Regs.payloadGPR(), resultFPR);
        
        done.link(this);
        
        doubleResult(resultFPR, node);
        return;
    }
    
    case NotCellNorBigIntUse:
    case NumberUse: {
        SpeculatedType possibleTypes = m_state.forNode(node->child1()).m_type;
        if (isInt32Speculation(possibleTypes)) {
            SpeculateInt32Operand op1(this, node->child1(), ManualOperandSpeculation);
            FPRTemporary result(this);
            convertInt32ToDouble(op1.gpr(), result.fpr());
            doubleResult(result.fpr(), node);
            return;
        }

        JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
        FPRTemporary result(this);

#if USE(JSVALUE64)
        GPRTemporary temp(this);

        GPRReg op1GPR = op1.gpr();
        GPRReg tempGPR = temp.gpr();
        FPRReg resultFPR = result.fpr();
        JumpList done;

        Jump isInteger = branchIfInt32(op1GPR);

        if (node->child1().useKind() == NotCellNorBigIntUse) {
            Jump isNumber = branchIfNumber(op1GPR);
            Jump isUndefined = branchIfUndefined(op1GPR);

            moveZeroToDouble(resultFPR);

            Jump isNull = branchIfNull(op1GPR);
            done.append(isNull);

            DFG_TYPE_CHECK(JSValueRegs(op1GPR), node->child1(), ~SpecCellCheck & ~SpecBigInt,
                branchTest64(Zero, op1GPR, TrustedImm32(JSValue::BoolTag)));

            Jump isFalse = branch64(Equal, op1GPR, TrustedImm64(JSValue::ValueFalse));
            move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(1.0)), resultFPR);
            done.append(jump());
            done.append(isFalse);

            isUndefined.link(this);
            move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(PNaN)), resultFPR);
            done.append(jump());

            isNumber.link(this);
        } else if (needsTypeCheck(node->child1(), SpecBytecodeNumber)) {
            typeCheck(
                JSValueRegs(op1GPR), node->child1(), SpecBytecodeNumber,
                branchIfNotNumber(op1GPR));
        }

        unboxDouble(op1GPR, tempGPR, resultFPR);
        done.append(jump());
    
        isInteger.link(this);
        convertInt32ToDouble(op1GPR, resultFPR);
        done.link(this);
#else // USE(JSVALUE64) -> this is the 32_64 case
        GPRReg op1TagGPR = op1.tagGPR();
        GPRReg op1PayloadGPR = op1.payloadGPR();
        FPRReg resultFPR = result.fpr();
        JumpList done;
    
        Jump isInteger = branchIfInt32(op1TagGPR);

        if (node->child1().useKind() == NotCellNorBigIntUse) {
            Jump isNumber = branch32(Below, op1TagGPR, TrustedImm32(JSValue::LowestTag + 1));
            Jump isUndefined = branchIfUndefined(op1TagGPR);

            moveZeroToDouble(resultFPR);

            Jump isNull = branchIfNull(op1TagGPR);
            done.append(isNull);

            DFG_TYPE_CHECK(JSValueRegs(op1TagGPR, op1PayloadGPR), node->child1(), ~SpecCell, branchIfNotBoolean(op1TagGPR, InvalidGPRReg));

            Jump isFalse = branchTest32(Zero, op1PayloadGPR, TrustedImm32(1));
            move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(1.0)), resultFPR);
            done.append(jump());
            done.append(isFalse);

            isUndefined.link(this);
            move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(PNaN)), resultFPR);
            done.append(jump());

            isNumber.link(this);
        } else if (needsTypeCheck(node->child1(), SpecBytecodeNumber)) {
            // This check fails with Int32Tag, but it is OK since Int32 case is already excluded.
            typeCheck(
                JSValueRegs(op1TagGPR, op1PayloadGPR), node->child1(), SpecBytecodeNumber,
                branch32(AboveOrEqual, op1TagGPR, TrustedImm32(JSValue::LowestTag)));
        }

        unboxDouble(op1TagGPR, op1PayloadGPR, resultFPR);
        done.append(jump());
    
        isInteger.link(this);
        convertInt32ToDouble(op1PayloadGPR, resultFPR);
        done.link(this);
#endif // USE(JSVALUE64)
    
        doubleResult(resultFPR, node);
        return;
    }
        
#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand value(this, node->child1());
        FPRTemporary result(this);
        
        GPRReg valueGPR = value.gpr();
        FPRReg resultFPR = result.fpr();

        convertInt64ToDouble(valueGPR, resultFPR);
        
        doubleResult(resultFPR, node);
        return;
    }
#endif // USE(JSVALUE64)
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileValueRep(Node* node)
{
    switch (node->child1().useKind()) {
    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        JSValueRegsTemporary result(this);
        
        FPRReg valueFPR = value.fpr();
        JSValueRegs resultRegs = result.regs();
        
        // It's very tempting to in-place filter the value to indicate that it's not impure NaN
        // anymore. Unfortunately, this would be unsound. If it's a GetLocal or if the value was
        // subject to a prior SetLocal, filtering the value would imply that the corresponding
        // local was purified.
        if (m_state.forNode(node->child1()).couldBeType(SpecDoubleImpureNaN))
            purifyNaN(valueFPR);

        boxDouble(valueFPR, resultRegs);
        
        jsValueResult(resultRegs, node);
        return;
    }
        
#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand value(this, node->child1());
        GPRTemporary result(this);
        
        GPRReg valueGPR = value.gpr();
        GPRReg resultGPR = result.gpr();
        
        boxInt52(valueGPR, resultGPR, DataFormatStrictInt52);
        
        jsValueResult(resultGPR, node);
        return;
    }
#endif // USE(JSVALUE64)
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

static double clampDoubleToByte(double d)
{
    if (!(d > 0))
        d = 0;
    else if (d > 255)
        d = 255;
    return std::nearbyint(d);
}

static void compileClampIntegerToByte(JITCompiler& jit, GPRReg resultGPR, GPRReg scratch1GPR)
{
#if CPU(ARM64)
    jit.clearBitsWithMaskRightShift32(resultGPR, resultGPR, CCallHelpers::TrustedImm32(31), resultGPR);
    jit.move(CCallHelpers::TrustedImm32(0xff), scratch1GPR);
    jit.moveConditionally32(CCallHelpers::Below, resultGPR, scratch1GPR, resultGPR, scratch1GPR, resultGPR);
#else
    UNUSED_PARAM(scratch1GPR);
    MacroAssembler::Jump inBounds = jit.branch32(MacroAssembler::BelowOrEqual, resultGPR, JITCompiler::TrustedImm32(0xff));
    MacroAssembler::Jump tooBig = jit.branch32(MacroAssembler::GreaterThan, resultGPR, JITCompiler::TrustedImm32(0xff));
    jit.xorPtr(resultGPR, resultGPR);
    MacroAssembler::Jump clamped = jit.jump();
    tooBig.link(&jit);
    jit.move(JITCompiler::TrustedImm32(255), resultGPR);
    clamped.link(&jit);
    inBounds.link(&jit);
#endif
}

static void compileClampDoubleToByte(JITCompiler& jit, GPRReg result, FPRReg source, FPRReg scratch)
{
    // Unordered compare so we pick up NaN
    jit.moveZeroToDouble(scratch);
    MacroAssembler::Jump tooSmall = jit.branchDouble(MacroAssembler::DoubleLessThanOrEqualOrUnordered, source, scratch);
    jit.move64ToDouble(CCallHelpers::TrustedImm64(bitwise_cast<uint64_t>(255.0)), scratch);
    MacroAssembler::Jump tooBig = jit.branchDouble(MacroAssembler::DoubleGreaterThanAndOrdered, source, scratch);
    
    jit.roundTowardNearestIntDouble(source, scratch);
    jit.truncateDoubleToInt32(scratch, result);   
    MacroAssembler::Jump truncatedInt = jit.jump();
    
    tooSmall.link(&jit);
    jit.xorPtr(result, result);
    MacroAssembler::Jump zeroed = jit.jump();
    
    tooBig.link(&jit);
    jit.move(JITCompiler::TrustedImm32(255), result);
    
    truncatedInt.link(&jit);
    zeroed.link(&jit);

}

JITCompiler::Jump SpeculativeJIT::jumpForTypedArrayOutOfBounds(Node* node, GPRReg baseGPR, GPRReg indexGPR, GPRReg scratchGPR, GPRReg scratch2GPR)
{
    if (node->op() == PutByValAlias)
        return Jump();
    Edge& edge = m_graph.child(node, 0);
    JSArrayBufferView* view = m_graph.tryGetFoldableView(m_state.forNode(edge).m_value, node->arrayMode());
    if (view && !view->isResizableOrGrowableShared()) {
        size_t length = view->length();
        Node* indexNode = m_graph.child(node, 1).node();
        if (indexNode->isAnyIntConstant() && static_cast<uint64_t>(indexNode->asAnyInt()) < length)
            return Jump();
#if USE(LARGE_TYPED_ARRAYS)
        signExtend32ToPtr(indexGPR, scratchGPR);
        return branch64(
            AboveOrEqual, scratchGPR, Imm64(length));
#else
        UNUSED_PARAM(scratchGPR);
        return branch32(
            AboveOrEqual, indexGPR, Imm32(length));
#endif
    }

#if USE(JSVALUE64)
    if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
        loadTypedArrayLength(baseGPR, scratch2GPR, scratchGPR, scratch2GPR, node->arrayMode().type() == Array::AnyTypedArray ? std::nullopt : std::optional { node->arrayMode().typedArrayType() });
#if USE(LARGE_TYPED_ARRAYS)
        signExtend32ToPtr(indexGPR, scratchGPR);
        return branch64(AboveOrEqual, scratchGPR, scratch2GPR);
#else
        return branch32(AboveOrEqual, indexGPR, scratch2GPR);
#endif
    }
#else
    UNUSED_PARAM(scratch2GPR);
#endif

    if (!m_graph.isNeverResizableOrGrowableSharedTypedArrayIncludingDataView(m_state.forNode(edge)))
        speculationCheck(UnexpectedResizableArrayBufferView, JSValueSource::unboxedCell(baseGPR), node, branchTest8(NonZero, Address(baseGPR, JSArrayBufferView::offsetOfMode()), TrustedImm32(isResizableOrGrowableSharedMode)));

#if USE(LARGE_TYPED_ARRAYS)
    signExtend32ToPtr(indexGPR, scratchGPR);
    return branch64(
        AboveOrEqual, scratchGPR,
        Address(baseGPR, JSArrayBufferView::offsetOfLength()));
#else
    return branch32(
        AboveOrEqual, indexGPR,
        Address(baseGPR, JSArrayBufferView::offsetOfLength()));
#endif
}

void SpeculativeJIT::loadFromIntTypedArray(GPRReg storageReg, GPRReg propertyReg, GPRReg resultReg, TypedArrayType type)
{
    switch (elementSize(type)) {
    case 1:
        if (JSC::isSigned(type))
            load8SignedExtendTo32(BaseIndex(storageReg, propertyReg, TimesOne), resultReg);
        else
            load8(BaseIndex(storageReg, propertyReg, TimesOne), resultReg);
        break;
    case 2:
        if (JSC::isSigned(type))
            load16SignedExtendTo32(BaseIndex(storageReg, propertyReg, TimesTwo), resultReg);
        else
            load16(BaseIndex(storageReg, propertyReg, TimesTwo), resultReg);
        break;
    case 4:
        load32(BaseIndex(storageReg, propertyReg, TimesFour), resultReg);
        break;
    default:
        CRASH();
    }
}

void SpeculativeJIT::setIntTypedArrayLoadResult(Node* node, JSValueRegs resultRegs, TypedArrayType type, bool canSpeculate, bool shouldBox, FPRReg resultFPR, Jump outOfBounds)
{
    bool isUInt32 = elementSize(type) == 4 && !JSC::isSigned(type);
    if (isUInt32)
        ASSERT(resultFPR != InvalidFPRReg);
    GPRReg resultReg = resultRegs.payloadGPR();

    if (shouldBox) {
        if (isUInt32) {
            convertInt32ToDouble(resultReg, resultFPR);
            Jump positive = branch32(GreaterThanOrEqual, resultReg, TrustedImm32(0));
            addDouble(AbsoluteAddress(&twoToThe32), resultFPR);
            positive.link(this);
            boxDouble(resultFPR, resultRegs);
        } else
            boxInt32(resultRegs.payloadGPR(), resultRegs);
        if (outOfBounds.isSet())
            outOfBounds.link(this);
        jsValueResult(resultRegs, node);
        return;
    }

    if (!isUInt32) {
        ASSERT(elementSize(type) < 4 || JSC::isSigned(type));
        strictInt32Result(resultReg, node);
        return;
    }

    if (node->shouldSpeculateInt32() && canSpeculate) {
        speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branch32(LessThan, resultReg, TrustedImm32(0)));
        strictInt32Result(resultReg, node);
        return;
    }
    
#if USE(JSVALUE64)
    if (node->shouldSpeculateInt52()) {
        ASSERT(enableInt52());
        zeroExtend32ToWord(resultReg, resultReg);
        strictInt52Result(resultReg, node);
        return;
    }
#endif
    
    convertInt32ToDouble(resultReg, resultFPR);
    Jump positive = branch32(GreaterThanOrEqual, resultReg, TrustedImm32(0));
    addDouble(AbsoluteAddress(&twoToThe32), resultFPR);
    positive.link(this);
    doubleResult(resultFPR, node);
}

void SpeculativeJIT::compileGetByValOnIntTypedArray(Node* node, TypedArrayType type, const ScopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>& prefix)
{
    ASSERT(isInt(type));
    
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    StorageOperand storage(this, m_graph.varArgChild(node, 2));
    GPRTemporary scratch(this);

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg storageReg = storage.gpr();
    GPRReg scratchGPR = scratch.gpr();

    std::optional<FPRTemporary> fprTemp;
    FPRReg resultFPR = InvalidFPRReg;
    if (elementSize(type) == 4 && !JSC::isSigned(type)) {
        fprTemp.emplace(this);
        resultFPR = fprTemp->fpr();
    }

    std::optional<GPRTemporary> scratch2;
    GPRReg scratch2GPR = InvalidGPRReg;
#if USE(JSVALUE64)
    if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
        scratch2.emplace(this);
        scratch2GPR = scratch2->gpr();
    }
#endif

    JSValueRegs resultRegs;
    DataFormat format = DataFormatInt32;
    if (node->arrayMode().isOutOfBounds())
        format = DataFormatJS;
    constexpr bool needsFlush = false;
    std::tie(resultRegs, format) = prefix(format, needsFlush);
    bool shouldBox = format == DataFormatJS;

    if (node->arrayMode().isOutOfBounds()) {
        ASSERT(shouldBox);
        moveTrustedValue(jsUndefined(), resultRegs);
    }

    Jump jump = jumpForTypedArrayOutOfBounds(node, baseReg, propertyReg, scratchGPR, scratch2GPR);
    if (jump.isSet()) {
        if (!node->arrayMode().isOutOfBounds()) {
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, jump);
            jump = { };
        }
    }

    loadFromIntTypedArray(storageReg, propertyReg, resultRegs.payloadGPR(), type);
    constexpr bool canSpeculate = true;
    setIntTypedArrayLoadResult(node, resultRegs, type, canSpeculate, shouldBox, resultFPR, jump);
}

bool SpeculativeJIT::getIntTypedArrayStoreOperand(
    GPRTemporary& value,
    GPRReg property,
#if USE(JSVALUE32_64)
    GPRTemporary& propertyTag,
    GPRTemporary& valueTag,
#endif
    Edge valueUse, JumpList& slowPathCases, bool isClamped)
{
    bool isAppropriateConstant = false;
    if (valueUse->isConstant()) {
        JSValue jsValue = valueUse->asJSValue();
        SpeculatedType expectedType = typeFilterFor(valueUse.useKind());
        SpeculatedType actualType = speculationFromValue(jsValue);
        isAppropriateConstant = (expectedType | actualType) == expectedType;
    }
    
    if (isAppropriateConstant) {
        JSValue jsValue = valueUse->asJSValue();
        if (!jsValue.isNumber()) {
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), nullptr);
            return false;
        }
        double d = jsValue.asNumber();
        if (isClamped)
            d = clampDoubleToByte(d);
        GPRTemporary scratch(this);
        GPRReg scratchReg = scratch.gpr();
        move(Imm32(toInt32(d)), scratchReg);
        value.adopt(scratch);
    } else {
        switch (valueUse.useKind()) {
        case Int32Use: {
            SpeculateInt32Operand valueOp(this, valueUse);
            GPRTemporary scratch1(this);
            GPRReg scratch1GPR = scratch1.gpr();
            if (isClamped) {
                GPRTemporary scratch2(this);
                GPRReg valueGPR = valueOp.gpr();
                GPRReg scratch2GPR = scratch2.gpr();
                move(valueGPR, scratch1GPR);
                compileClampIntegerToByte(*this, scratch1GPR, scratch2GPR);
            } else
                move(valueOp.gpr(), scratch1GPR);
            value.adopt(scratch1);
            break;
        }
            
#if USE(JSVALUE64)
        case Int52RepUse: {
            SpeculateStrictInt52Operand valueOp(this, valueUse);
            GPRTemporary scratch(this);
            GPRReg scratchReg = scratch.gpr();
            move(valueOp.gpr(), scratchReg);
            if (isClamped) {
                Jump inBounds = branch64(
                    BelowOrEqual, scratchReg, TrustedImm64(0xff));
                Jump tooBig = branch64(
                    GreaterThan, scratchReg, TrustedImm64(0xff));
                move(TrustedImm32(0), scratchReg);
                Jump clamped = jump();
                tooBig.link(this);
                move(TrustedImm32(255), scratchReg);
                clamped.link(this);
                inBounds.link(this);
            }
            value.adopt(scratch);
            break;
        }
#endif // USE(JSVALUE64)
            
        case DoubleRepUse: {
            RELEASE_ASSERT(!isAtomicsIntrinsic(m_currentNode->op()));
            if (isClamped) {
                SpeculateDoubleOperand valueOp(this, valueUse);
                GPRTemporary result(this);
                FPRTemporary floatScratch(this);
                FPRReg fpr = valueOp.fpr();
                GPRReg gpr = result.gpr();
                compileClampDoubleToByte(*this, gpr, fpr, floatScratch.fpr());
                value.adopt(result);
            } else {
#if USE(JSVALUE32_64)
                GPRTemporary realPropertyTag(this);
                propertyTag.adopt(realPropertyTag);
                GPRReg propertyTagGPR = propertyTag.gpr();

                GPRTemporary realValueTag(this);
                valueTag.adopt(realValueTag);
                GPRReg valueTagGPR = valueTag.gpr();
#endif
                SpeculateDoubleOperand valueOp(this, valueUse);
                GPRTemporary result(this);
                FPRReg fpr = valueOp.fpr();
                GPRReg gpr = result.gpr();
                Jump notNaN = branchIfNotNaN(fpr);
                xorPtr(gpr, gpr);
                JumpList fixed(jump());
                notNaN.link(this);

                fixed.append(branchTruncateDoubleToInt32(
                    fpr, gpr, BranchIfTruncateSuccessful));

#if USE(JSVALUE64)
                or64(GPRInfo::numberTagRegister, property);
                boxDouble(fpr, gpr);
#else
                UNUSED_PARAM(property);
                move(TrustedImm32(JSValue::Int32Tag), propertyTagGPR);
                boxDouble(fpr, valueTagGPR, gpr);
#endif
                slowPathCases.append(jump());

                fixed.link(this);
                value.adopt(result);
            }
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    return true;
}

bool SpeculativeJIT::getIntTypedArrayStoreOperandForAtomics(
    GPRTemporary& value,
    GPRReg property,
#if USE(JSVALUE32_64)
    GPRTemporary& propertyTag,
    GPRTemporary& valueTag,
#endif
    Edge valueUse)
{
    JumpList slowPathCases;
    constexpr bool isClamped = false;
    bool result = getIntTypedArrayStoreOperand(
        value,
        property,
#if USE(JSVALUE32_64)
        propertyTag,
        valueTag,
#endif
        valueUse,
        slowPathCases,
        isClamped);
    ASSERT(slowPathCases.empty());
    return result;
}

void SpeculativeJIT::compilePutByValForIntTypedArray(Node* node, TypedArrayType type)
{
    ASSERT(isInt(type));

    Edge child1 = m_graph.varArgChild(node, 0);
    Edge child2 = m_graph.varArgChild(node, 1);
    Edge child3 = m_graph.varArgChild(node, 2);
    Edge child4 = m_graph.varArgChild(node, 3);

    SpeculateCellOperand base(this, child1);
    SpeculateStrictInt32Operand property(this, child2);
    StorageOperand storage(this, child4);

    GPRTemporary scratch(this);
    std::optional<GPRTemporary> scratch2;
    GPRReg storageReg = storage.gpr();
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();

    GPRTemporary value;
#if USE(JSVALUE32_64)
    GPRTemporary propertyTag;
    GPRTemporary valueTag;
#endif

    JumpList slowPathCases;

    bool isClamped = JSC::isClamped(type);
    if (isClamped) {
        if (child3->op() == GetByVal) {
            switch (child3->arrayMode().type()) {
            case Array::Uint8Array:
            case Array::Uint8ClampedArray: {
                // If the value is coming from Uint8Array / Uint8ClampedArray, the value is always within uint8_t.
                isClamped = false;
                break;
            }
            default:
                break;
            }
        }
    }

    bool result = getIntTypedArrayStoreOperand(
        value, propertyReg,
#if USE(JSVALUE32_64)
        propertyTag, valueTag,
#endif
        child3, slowPathCases, isClamped);
    if (!result) {
        noResult(node);
        return;
    }

    GPRReg scratch2GPR = InvalidGPRReg;
#if USE(JSVALUE64)
    if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
        scratch2.emplace(this);
        scratch2GPR = scratch2->gpr();
    }
#endif

    GPRReg valueGPR = value.gpr();
    GPRReg scratchGPR = scratch.gpr();
#if USE(JSVALUE32_64)
    GPRReg propertyTagGPR = propertyTag.gpr();
    GPRReg valueTagGPR = valueTag.gpr();
#endif

    ASSERT_UNUSED(valueGPR, valueGPR != propertyReg);
    ASSERT(valueGPR != baseReg);
    ASSERT(valueGPR != storageReg);
    Jump outOfBounds = jumpForTypedArrayOutOfBounds(node, baseReg, propertyReg, scratchGPR, scratch2GPR);

    switch (elementSize(type)) {
    case 1:
        store8(value.gpr(), BaseIndex(storageReg, propertyReg, TimesOne));
        break;
    case 2:
        store16(value.gpr(), BaseIndex(storageReg, propertyReg, TimesTwo));
        break;
    case 4:
        store32(value.gpr(), BaseIndex(storageReg, propertyReg, TimesFour));
        break;
    default:
        CRASH();
    }

    if (outOfBounds.isSet()) {
        if (node->arrayMode().isInBounds())
            speculationCheck(OutOfBounds, JSValueSource(), nullptr, outOfBounds);
        else {
            if (node->op() == PutByValDirect)
                speculationCheck(Uncountable, JSValueSource(), nullptr, outOfBounds);
            else
                outOfBounds.link(this);
        }
    }

    if (!slowPathCases.empty()) {
        addSlowPathGenerator(slowPathCall(
            slowPathCases, this,
            node->ecmaMode().isStrict() ?
                (node->op() == PutByValDirect ? operationDirectPutByValStrictGeneric : operationPutByValStrictGeneric) :
                (node->op() == PutByValDirect ? operationDirectPutByValSloppyGeneric : operationPutByValSloppyGeneric),
#if USE(JSVALUE64)
            NoResult, LinkableConstant::globalObject(*this, node), baseReg, propertyReg, valueGPR));
#else // not USE(JSVALUE64)
            NoResult, LinkableConstant::globalObject(*this, node), CellValue(baseReg), JSValueRegs(propertyTagGPR, propertyReg), JSValueRegs(valueTagGPR, valueGPR)));
#endif
    }

    noResult(node);
}

void SpeculativeJIT::compileGetByValOnFloatTypedArray(Node* node, TypedArrayType type, const ScopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>& prefix)
{
    ASSERT(isFloat(type));
    
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    StorageOperand storage(this, m_graph.varArgChild(node, 2));
    GPRTemporary scratch(this);
    FPRTemporary result(this);

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg storageReg = storage.gpr();
    GPRReg scratchGPR = scratch.gpr();
    FPRReg resultReg = result.fpr();

    std::optional<GPRTemporary> scratch2;
    GPRReg scratch2GPR = InvalidGPRReg;
#if USE(JSVALUE64)
    if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
        scratch2.emplace(this);
        scratch2GPR = scratch2->gpr();
    }
#endif

    JSValueRegs resultRegs;
    DataFormat format = DataFormatDouble;
    if (node->arrayMode().isOutOfBounds())
        format = DataFormatJS;
    constexpr bool needsFlush = false;
    std::tie(resultRegs, format) = prefix(format, needsFlush);

    if (node->arrayMode().isOutOfBounds())
        moveTrustedValue(jsUndefined(), resultRegs);

    Jump jump = jumpForTypedArrayOutOfBounds(node, baseReg, propertyReg, scratchGPR, scratch2GPR);
    if (jump.isSet()) {
        if (!node->arrayMode().isOutOfBounds()) {
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, jump);
            jump = { };
        }
    }

    switch (elementSize(type)) {
    case 2:
        loadFloat16(BaseIndex(storageReg, propertyReg, TimesTwo), resultReg);
        convertFloat16ToDouble(resultReg, resultReg);
        break;
    case 4:
        loadFloat(BaseIndex(storageReg, propertyReg, TimesFour), resultReg);
        convertFloatToDouble(resultReg, resultReg);
        break;
    case 8: {
        loadDouble(BaseIndex(storageReg, propertyReg, TimesEight), resultReg);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (format == DataFormatJS) {
        purifyNaN(resultReg);
        boxDouble(resultReg, resultRegs);
        if (jump.isSet())
            jump.link(this);
        jsValueResult(resultRegs, node);
    } else {
        ASSERT(format == DataFormatDouble);
        doubleResult(resultReg, node);
    }
}

void SpeculativeJIT::compilePutByValForFloatTypedArray(Node* node, TypedArrayType type)
{
    ASSERT(isFloat(type));

    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    SpeculateDoubleOperand valueOp(this, m_graph.varArgChild(node, 2));
    StorageOperand storage(this, m_graph.varArgChild(node, 3));

    FPRTemporary scratch(this);
    GPRTemporary gpScratch(this);
    std::optional<GPRTemporary> scratch2;

    GPRReg scratch2GPR = InvalidGPRReg;
#if USE(JSVALUE64)
    if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
        scratch2.emplace(this);
        scratch2GPR = scratch2->gpr();
    }
#endif

    FPRReg valueFPR = valueOp.fpr();
    FPRReg scratchFPR = scratch.fpr();
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg scratchGPR = gpScratch.gpr();
    GPRReg storageReg = storage.gpr();

    Jump outOfBounds = jumpForTypedArrayOutOfBounds(node, baseReg, propertyReg, scratchGPR, scratch2GPR);
    switch (elementSize(type)) {
    case 2: {
        convertDoubleToFloat16(valueFPR, scratchFPR);
        storeFloat16(scratchFPR, BaseIndex(storageReg, propertyReg, TimesTwo));
        break;
    }
    case 4: {
        convertDoubleToFloat(valueFPR, scratchFPR);
        storeFloat(scratchFPR, BaseIndex(storageReg, propertyReg, TimesFour));
        break;
    }
    case 8:
        storeDouble(valueFPR, BaseIndex(storageReg, propertyReg, TimesEight));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    if (outOfBounds.isSet()) {
        if (node->arrayMode().isInBounds())
            speculationCheck(OutOfBounds, JSValueSource(), nullptr, outOfBounds);
        else {
            if (node->op() == PutByValDirect)
                speculationCheck(Uncountable, JSValueSource(), nullptr, outOfBounds);
            else
                outOfBounds.link(this);
        }
    }

    noResult(node);
}

void SpeculativeJIT::compileGetByValForObjectWithString(Node* node, const ScopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>& prefix)
{
    SpeculateCellOperand arg1(this, m_graph.varArgChild(node, 0));
    SpeculateCellOperand arg2(this, m_graph.varArgChild(node, 1));

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();

    constexpr bool needsFlush = true;
    auto [resultRegs, dataFormat] = prefix(DataFormatJS, needsFlush);
    speculateObject(m_graph.varArgChild(node, 0), arg1GPR);
    speculateString(m_graph.varArgChild(node, 1), arg2GPR);
    callOperation(operationGetByValObjectString, resultRegs, LinkableConstant::globalObject(*this, node), arg1GPR, arg2GPR);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetByValForObjectWithSymbol(Node* node, const ScopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>& prefix)
{
    SpeculateCellOperand arg1(this, m_graph.varArgChild(node, 0));
    SpeculateCellOperand arg2(this, m_graph.varArgChild(node, 1));

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();

    constexpr bool needsFlush = true;
    auto [resultRegs, dataFormat] = prefix(DataFormatJS, needsFlush);
    speculateObject(m_graph.varArgChild(node, 0), arg1GPR);
    speculateSymbol(m_graph.varArgChild(node, 1), arg2GPR);
    callOperation(operationGetByValObjectSymbol, resultRegs, LinkableConstant::globalObject(*this, node), arg1GPR, arg2GPR);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetPrivateName(Node* node)
{
    switch (m_graph.child(node, 0).useKind()) {
    case CellUse: {
        SpeculateCellOperand base(this, m_graph.child(node, 0));
        SpeculateCellOperand property(this, m_graph.child(node, 1));

        compileGetPrivateNameByVal(node, JSValueRegs::payloadOnly(base.gpr()), JSValueRegs::payloadOnly(property.gpr()));
        break;
    }
    case UntypedUse: {
        JSValueOperand base(this, m_graph.child(node, 0));
        SpeculateCellOperand property(this, m_graph.child(node, 1));

        compileGetPrivateNameByVal(node, base.jsValueRegs(), JSValueRegs::payloadOnly(property.gpr()));
        break;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
    }
}

void SpeculativeJIT::compilePutByValForCellWithString(Node* node)
{
    SpeculateCellOperand arg1(this, m_graph.varArgChild(node, 0));
    SpeculateCellOperand arg2(this, m_graph.varArgChild(node, 1));
    JSValueOperand arg3(this, m_graph.varArgChild(node, 2));

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    JSValueRegs arg3Regs = arg3.jsValueRegs();

    speculateString(m_graph.varArgChild(node, 1), arg2GPR);

    flushRegisters();
    callOperation(
        node->ecmaMode().isStrict() ?
            (node->op() == PutByValDirect ? operationPutByValDirectCellStringStrict : operationPutByValCellStringStrict) :
            (node->op() == PutByValDirect ? operationPutByValDirectCellStringSloppy : operationPutByValCellStringSloppy),
        LinkableConstant::globalObject(*this, node), arg1GPR, arg2GPR, arg3Regs);

    noResult(node);
}

void SpeculativeJIT::compilePutByValForCellWithSymbol(Node* node)
{
    SpeculateCellOperand arg1(this, m_graph.varArgChild(node, 0));
    SpeculateCellOperand arg2(this, m_graph.varArgChild(node, 1));
    JSValueOperand arg3(this, m_graph.varArgChild(node, 2));

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    JSValueRegs arg3Regs = arg3.jsValueRegs();

    speculateSymbol(m_graph.varArgChild(node, 1), arg2GPR);

    flushRegisters();
    callOperation(
        node->ecmaMode().isStrict()
            ? (node->op() == PutByValDirect ? operationPutByValDirectCellSymbolStrict : operationPutByValCellSymbolStrict)
            : (node->op() == PutByValDirect ? operationPutByValDirectCellSymbolSloppy : operationPutByValCellSymbolSloppy),
        LinkableConstant::globalObject(*this, node), arg1GPR, arg2GPR, arg3Regs);

    noResult(node);
}

void SpeculativeJIT::compileCheckTypeInfoFlags(Node* node)
{
    SpeculateCellOperand base(this, node->child1());

    GPRReg baseGPR = base.gpr();

    // FIXME: This only works for checking if a single bit is set. If we want to check more
    // than one bit at once, we'll need to fix this:
    // https://bugs.webkit.org/show_bug.cgi?id=185705
    speculationCheck(BadTypeInfoFlags, JSValueRegs(), nullptr, branchTest8(Zero, Address(baseGPR, JSCell::typeInfoFlagsOffset()), TrustedImm32(node->typeInfoOperand())));

    noResult(node);
}

void SpeculativeJIT::compileParseInt(Node* node)
{
    if (node->child2()) {
        SpeculateInt32Operand radix(this, node->child2());
        GPRReg radixGPR = radix.gpr();
        switch (node->child1().useKind()) {
        case UntypedUse: {
            JSValueOperand value(this, node->child1());
            JSValueRegs valueRegs = value.jsValueRegs();

            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationParseIntGeneric, resultRegs, LinkableConstant::globalObject(*this, node), valueRegs, radixGPR);
            jsValueResult(resultRegs, node);
            return;
        }

        case StringUse: {
            SpeculateCellOperand value(this, node->child1());
            GPRReg valueGPR = value.gpr();
            speculateString(node->child1(), valueGPR);

            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationParseIntString, resultRegs, LinkableConstant::globalObject(*this, node), valueGPR, radixGPR);
            jsValueResult(resultRegs, node);
            return;
        }

        case Int32Use: {
            SpeculateInt32Operand value(this, node->child1());
            GPRReg valueGPR = value.gpr();

            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationParseIntInt32, resultRegs, LinkableConstant::globalObject(*this, node), valueGPR, radixGPR);
            jsValueResult(resultRegs, node);
            return;
        }

        case DoubleRepUse: {
            SpeculateDoubleOperand value(this, node->child1());
            FPRReg valueFPR = value.fpr();

            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationParseIntDouble, resultRegs, LinkableConstant::globalObject(*this, node), valueFPR, radixGPR);
            jsValueResult(resultRegs, node);
            return;
        }

        default:
            DFG_CRASH(m_graph, node, "Bad use kind");
            return;
        }
    }

    switch (node->child1().useKind()) {
    case UntypedUse: {
        JSValueOperand value(this, node->child1());
        JSValueRegs valueRegs = value.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationParseIntGenericNoRadix, resultRegs, LinkableConstant::globalObject(*this, node), valueRegs);
        jsValueResult(resultRegs, node);
        return;
    }

    case StringUse: {
        SpeculateCellOperand value(this, node->child1());
        GPRReg valueGPR = value.gpr();
        speculateString(node->child1(), valueGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationParseIntStringNoRadix, resultRegs, LinkableConstant::globalObject(*this, node), valueGPR);
        jsValueResult(resultRegs, node);
        return;
    }

    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        FPRReg valueFPR = value.fpr();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationParseIntDoubleNoRadix, resultRegs, LinkableConstant::globalObject(*this, node), valueFPR);
        jsValueResult(resultRegs, node);
        return;
    }

    // Int32Use is converted to Identity.

    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        return;
    }
}

void SpeculativeJIT::compileOverridesHasInstance(Node* node)
{
    Node* hasInstanceValueNode = node->child2().node();
    JSFunction* defaultHasInstanceFunction = jsCast<JSFunction*>(node->cellOperand()->value());

    JumpList notDefault;
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand hasInstanceValue(this, node->child2());
    GPRTemporary result(this);

    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();

    // It would be great if constant folding handled automatically the case where we knew the hasInstance function
    // was a constant. Unfortunately, the folding rule for OverridesHasInstance is in the strength reduction phase
    // since it relies on OSR information. https://bugs.webkit.org/show_bug.cgi?id=154832
    if (!hasInstanceValueNode->isCellConstant() || defaultHasInstanceFunction != hasInstanceValueNode->asCell()) {
        // FIXME: uDFG should avoid generating this node when node->cellOperand() is not the top-level JSGlobalObject.
        JSValueRegs hasInstanceValueRegs = hasInstanceValue.jsValueRegs();
        loadLinkableConstant(LinkableConstant(*this, node->cellOperand()->cell()), resultGPR);
#if USE(JSVALUE64)
        notDefault.append(branchPtr(NotEqual, hasInstanceValueRegs.gpr(), resultGPR));
#else
        notDefault.append(branchIfNotCell(hasInstanceValueRegs));
        notDefault.append(branchPtr(NotEqual, hasInstanceValueRegs.payloadGPR(), resultGPR));
#endif
    }

    // Check that base 'ImplementsDefaultHasInstance'.
    test8(Zero, Address(baseGPR, JSCell::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance), resultGPR);
    Jump done = jump();

    if (!notDefault.empty()) {
        notDefault.link(this);
        move(TrustedImm32(1), resultGPR);
    }

    done.link(this);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileValueBitNot(Node* node)
{
    Edge& child1 = node->child1();

#if USE(BIGINT32)
    if (child1.useKind() == BigInt32Use) {
        SpeculateBigInt32Operand operand(this, child1);
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        // The following trick relies on details of the representation of BigInt32, and will have to be updated if we move bits around.
        static_assert(JSValue::BigInt32Tag == 0x12);
        static_assert(JSValue::BigInt32Mask == static_cast<int64_t>(0xfffe000000000012));
        constexpr uint64_t maskForBigInt32Bits = 0x0000ffffffff0000;
        static_assert(!(JSValue::BigInt32Mask & maskForBigInt32Bits));
        move(TrustedImm64(maskForBigInt32Bits), resultGPR);
        xor64(operand.gpr(), resultGPR);

        jsValueResult(resultGPR, node);

        return;
    }
    // FIXME: add support for mixed BigInt32 / HeapBigInt
#endif

    if (child1.useKind() == HeapBigIntUse) {
        SpeculateCellOperand operand(this, child1);
        GPRReg operandGPR = operand.gpr();

        speculateHeapBigInt(child1, operandGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        callOperation(operationBitNotHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), operandGPR);
        jsValueResult(resultRegs, node);

        return;
    }

    ASSERT(child1.useKind() == UntypedUse || child1.useKind() == AnyBigIntUse);
    JSValueOperand operand(this, child1, ManualOperandSpeculation);
    speculate(node, child1); // Required for the AnyBigIntUse case
    JSValueRegs operandRegs = operand.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationValueBitNot, resultRegs, LinkableConstant::globalObject(*this, node), operandRegs);

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileBitwiseNot(Node* node)
{
    Edge& child1 = node->child1();

    SpeculateInt32Operand operand(this, child1);
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();

    move(operand.gpr(), resultGPR);

    not32(resultGPR);

    strictInt32Result(resultGPR, node);
}

template<typename SnippetGenerator, J_JITOperation_GJJ snippetSlowPathFunction>
void SpeculativeJIT::emitUntypedOrAnyBigIntBitOp(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    DFG_ASSERT(m_graph, node, node->isBinaryUseKind(UntypedUse) || node->isBinaryUseKind(AnyBigIntUse) || node->isBinaryUseKind(HeapBigIntUse) || node->isBinaryUseKind(BigInt32Use));

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node())) {
        JSValueOperand left(this, leftChild, ManualOperandSpeculation);
        JSValueOperand right(this, rightChild, ManualOperandSpeculation);
        speculate(node, leftChild);
        speculate(node, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(snippetSlowPathFunction, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

        jsValueResult(resultRegs, node);
        return;
    }

    std::optional<JSValueOperand> left;
    std::optional<JSValueOperand> right;

    JSValueRegs leftRegs;
    JSValueRegs rightRegs;

#if USE(JSVALUE64)
    GPRTemporary result(this);
    JSValueRegs resultRegs = JSValueRegs(result.gpr());
    GPRTemporary scratch(this);
    GPRReg scratchGPR = scratch.gpr();
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs = JSValueRegs(resultPayload.gpr(), resultTag.gpr());
    GPRReg scratchGPR = resultTag.gpr();
#endif

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    // The snippet generator does not support both operands being constant. If the left
    // operand is already const, we'll ignore the right operand's constness.
    if (leftChild->isInt32Constant())
        leftOperand.setConstInt32(leftChild->asInt32());
    else if (rightChild->isInt32Constant())
        rightOperand.setConstInt32(rightChild->asInt32());

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst()) {
        left.emplace(this, leftChild, ManualOperandSpeculation);
        speculate(node, leftChild); // Required for AnyBigIntUse
        leftRegs = left->jsValueRegs();
    }
    if (!rightOperand.isConst()) {
        right.emplace(this, rightChild, ManualOperandSpeculation);
        speculate(node, rightChild); // Required for AnyBigIntUse
        rightRegs = right->jsValueRegs();
    }

    SnippetGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, scratchGPR);
    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().append(jump());

    gen.slowPathJumpList().link(this);

    if (leftOperand.isConst()) {
        leftRegs = resultRegs;
        moveValue(leftChild->asJSValue(), leftRegs);
    } else if (rightOperand.isConst()) {
        rightRegs = resultRegs;
        moveValue(rightChild->asJSValue(), rightRegs);
    }

    callOperationWithSilentSpill(snippetSlowPathFunction, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

    gen.endJumpList().link(this);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileValueBitwiseOp(Node* node)
{
    NodeType op = node->op();
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

#if USE(BIGINT32)
    if (leftChild.useKind() == BigInt32Use && rightChild.useKind() == BigInt32Use) {
        SpeculateBigInt32Operand left(this, leftChild);
        SpeculateBigInt32Operand right(this, rightChild);
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        move(left.gpr(), resultGPR);

        switch (op) {
        case ValueBitAnd:
            // No need to unbox/box: bitAnd does not interfere with the encoding of BigInt32
            and64(right.gpr(), resultGPR);
            break;
        case ValueBitOr:
            // No need to unbox/box: bitOr does not interfere with the encoding of BigInt32
            or64(right.gpr(), resultGPR);
            break;
        case ValueBitXor:
            // BitXor removes the tag, so we must add it back after doing the operation
            xor64(right.gpr(), resultGPR);
            or64(TrustedImm32(JSValue::BigInt32Tag), resultGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        jsValueResult(resultGPR, node);
        return;
    }
    // FIXME: add support for mixed BigInt32 / HeapBigInt
#endif

    if (node->isBinaryUseKind(HeapBigIntUse)) {
        SpeculateCellOperand left(this, node->child1());
        SpeculateCellOperand right(this, node->child2());
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        switch (op) {
        case ValueBitAnd:
            callOperation(operationBitAndHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);
            break;
        case ValueBitXor:
            callOperation(operationBitXorHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);
            break;
        case ValueBitOr:
            callOperation(operationBitOrHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        jsValueResult(resultRegs, node);
        return;
    }

    switch (op) {
    case ValueBitAnd:
        emitUntypedOrAnyBigIntBitOp<JITBitAndGenerator, operationValueBitAnd>(node);
        return;
    case ValueBitXor:
        emitUntypedOrAnyBigIntBitOp<JITBitXorGenerator, operationValueBitXor>(node);
        return;
    case ValueBitOr:
        emitUntypedOrAnyBigIntBitOp<JITBitOrGenerator, operationValueBitOr>(node);
        return;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileBitwiseOp(Node* node)
{
    NodeType op = node->op();
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (leftChild->isInt32Constant()) {
        SpeculateInt32Operand op2(this, rightChild);
        GPRTemporary result(this, Reuse, op2);

        bitOp(op, leftChild->asInt32(), op2.gpr(), result.gpr());

        strictInt32Result(result.gpr(), node);
        return;
    }

    if (rightChild->isInt32Constant()) {
        SpeculateInt32Operand op1(this, leftChild);
        GPRTemporary result(this, Reuse, op1);

        bitOp(op, rightChild->asInt32(), op1.gpr(), result.gpr());

        strictInt32Result(result.gpr(), node);
        return;
    }

    SpeculateInt32Operand op1(this, leftChild);
    SpeculateInt32Operand op2(this, rightChild);
    GPRTemporary result(this, Reuse, op1, op2);

    GPRReg reg1 = op1.gpr();
    GPRReg reg2 = op2.gpr();
    bitOp(op, reg1, reg2, result.gpr());

    strictInt32Result(result.gpr(), node);
}

void SpeculativeJIT::emitUntypedOrBigIntRightShiftBitOp(Node* node)
{
    J_JITOperation_GJJ snippetSlowPathFunction = node->op() == ValueBitRShift
        ? operationValueBitRShift : operationValueBitURShift;
    JITRightShiftGenerator::ShiftType shiftType = node->op() == ValueBitRShift
        ? JITRightShiftGenerator::SignedShift : JITRightShiftGenerator::UnsignedShift;

    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node()) || node->isBinaryUseKind(BigInt32Use) || node->isBinaryUseKind(AnyBigIntUse)) {
        JSValueOperand left(this, leftChild, ManualOperandSpeculation);
        JSValueOperand right(this, rightChild, ManualOperandSpeculation);
        speculate(node, leftChild);
        speculate(node, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(snippetSlowPathFunction, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

        jsValueResult(resultRegs, node);
        return;
    }

    std::optional<JSValueOperand> left;
    std::optional<JSValueOperand> right;

    JSValueRegs leftRegs;
    JSValueRegs rightRegs;

    FPRTemporary leftNumber(this);
    FPRReg leftFPR = leftNumber.fpr();

#if USE(JSVALUE64)
    GPRTemporary result(this);
    JSValueRegs resultRegs = JSValueRegs(result.gpr());
    GPRTemporary scratch(this);
    GPRReg scratchGPR = scratch.gpr();
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs = JSValueRegs(resultPayload.gpr(), resultTag.gpr());
    GPRReg scratchGPR = resultTag.gpr();
#endif

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    // The snippet generator does not support both operands being constant. If the left
    // operand is already const, we'll ignore the right operand's constness.
    if (leftChild->isInt32Constant())
        leftOperand.setConstInt32(leftChild->asInt32());
    else if (rightChild->isInt32Constant())
        rightOperand.setConstInt32(rightChild->asInt32());

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst()) {
        left.emplace(this, leftChild);
        leftRegs = left->jsValueRegs();
    }
    if (!rightOperand.isConst()) {
        right.emplace(this, rightChild);
        rightRegs = right->jsValueRegs();
    }

    JITRightShiftGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, leftFPR, scratchGPR, shiftType);
    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().append(jump());

    gen.slowPathJumpList().link(this);

    if (leftOperand.isConst()) {
        leftRegs = resultRegs;
        moveValue(leftChild->asJSValue(), leftRegs);
    } else if (rightOperand.isConst()) {
        rightRegs = resultRegs;
        moveValue(rightChild->asJSValue(), rightRegs);
    }

    callOperationWithSilentSpill(snippetSlowPathFunction, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

    gen.endJumpList().link(this);
    jsValueResult(resultRegs, node);
    return;
}

void SpeculativeJIT::compileValueLShiftOp(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    // FIXME: support BigInt32
    if (node->binaryUseKind() == HeapBigIntUse) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        callOperation(operationBitLShiftHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);
        jsValueResult(resultRegs, node);
        return;
    }

    emitUntypedOrAnyBigIntBitOp<JITLeftShiftGenerator, operationValueBitLShift>(node);
}

void SpeculativeJIT::compileValueBitRShift(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    // FIXME: support BigInt32
    if (node->isBinaryUseKind(HeapBigIntUse)) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationBitRShiftHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);

        jsValueResult(resultRegs, node);
        return;
    }

    emitUntypedOrBigIntRightShiftBitOp(node);
}

void SpeculativeJIT::compileShiftOp(Node* node)
{
    NodeType op = node->op();
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (leftChild.useKind() == UntypedUse || rightChild.useKind() == UntypedUse) {
        RELEASE_ASSERT(op == BitURShift);
        emitUntypedOrBigIntRightShiftBitOp(node);
        return;
    }

    if (rightChild->isInt32Constant()) {
        SpeculateInt32Operand op1(this, leftChild);
        GPRTemporary result(this, Reuse, op1);

        shiftOp(op, op1.gpr(), rightChild->asInt32() & 0x1f, result.gpr());

        strictInt32Result(result.gpr(), node);
    } else {
        // Do not allow shift amount to be used as the result, MacroAssembler does not permit this.
        SpeculateInt32Operand op1(this, leftChild);
        SpeculateInt32Operand op2(this, rightChild);
        GPRTemporary result(this, Reuse, op1);

        GPRReg reg1 = op1.gpr();
        GPRReg reg2 = op2.gpr();
        shiftOp(op, reg1, reg2, result.gpr());

        strictInt32Result(result.gpr(), node);
    }
}

void SpeculativeJIT::compileValueAdd(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

#if USE(BIGINT32)
    // FIXME: Introduce another BigInt32 code generation: binary use kinds are BigIntUse32, but result is SpecAnyInt and accepting overflow.
    // Let's distinguish these modes based on result type information by introducing NodeResultBigInt32.
    // https://bugs.webkit.org/show_bug.cgi?id=210957
    // https://bugs.webkit.org/show_bug.cgi?id=211040
    if (node->isBinaryUseKind(BigInt32Use)) {
        SpeculateBigInt32Operand left(this, leftChild);
        SpeculateBigInt32Operand right(this, rightChild);
        GPRTemporary result(this);
        GPRTemporary temp(this);

        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg tempGPR = temp.gpr();

        unboxBigInt32(leftGPR, resultGPR);
        unboxBigInt32(rightGPR, tempGPR);

        Jump check = branchAdd32(Overflow, resultGPR, tempGPR, resultGPR);

        speculationCheck(BigInt32Overflow, JSValueRegs(), nullptr, check);

        boxBigInt32(resultGPR);
        jsValueResult(resultGPR, node);
        return;
    }

    if (node->isBinaryUseKind(AnyBigIntUse)) {
        JSValueOperand left(this, leftChild, ManualOperandSpeculation);
        JSValueOperand right(this, rightChild, ManualOperandSpeculation);
        speculate(node, leftChild);
        speculate(node, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        // FIXME: call a more specialized function
        callOperation(operationValueAddNotNumber, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

        jsValueResult(resultRegs, node);
        return;
    }
    // FIXME: add support for mixed BigInt32/HeapBigInt
#endif // USE(BIGINT32)

    if (node->isBinaryUseKind(HeapBigIntUse)) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationAddHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);

        jsValueResult(resultRegs, node);
        return;
    }

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node())) {
        JSValueOperand left(this, leftChild);
        JSValueOperand right(this, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationValueAddNotNumber, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);
    
        jsValueResult(resultRegs, node);
        return;
    }

    CodeBlock* baselineCodeBlock = m_graph.baselineCodeBlockFor(node->origin.semantic);
    BytecodeIndex bytecodeIndex = node->origin.semantic.bytecodeIndex();
    BinaryArithProfile* arithProfile = baselineCodeBlock->binaryArithProfileForBytecodeIndex(bytecodeIndex);
    JITAddIC* addIC = jitCode()->common.addJITAddIC(arithProfile);
    auto repatchingFunction = operationValueAddOptimize;
    auto nonRepatchingFunction = operationValueAdd;
    
    compileMathIC(node, addIC, repatchingFunction, nonRepatchingFunction);
}

void SpeculativeJIT::compileValueSub(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

#if USE(BIGINT32)
    // FIXME: Introduce another BigInt32 code generation: binary use kinds are BigIntUse32, but result is SpecAnyInt and accepting overflow.
    // Let's distinguish these modes based on result type information by introducing NodeResultBigInt32.
    // https://bugs.webkit.org/show_bug.cgi?id=210957
    // https://bugs.webkit.org/show_bug.cgi?id=211040
    if (node->binaryUseKind() == BigInt32Use) {
        SpeculateBigInt32Operand left(this, node->child1());
        SpeculateBigInt32Operand right(this, node->child2());
        GPRTemporary result(this);
        GPRTemporary temp(this);

        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg tempGPR = temp.gpr();

        unboxBigInt32(leftGPR, resultGPR);
        unboxBigInt32(rightGPR, tempGPR);

        Jump check = branchSub32(Overflow, resultGPR, tempGPR, resultGPR);

        speculationCheck(BigInt32Overflow, JSValueRegs(), nullptr, check);

        boxBigInt32(resultGPR);
        jsValueResult(resultGPR, node);
        return;
    }
    // FIXME: add support for mixed BigInt32/HeapBigInt

    // FIXME: why do compileValueAdd/compileValueMul use isKnownNotNumber but not ValueSub?
    if (node->binaryUseKind() == AnyBigIntUse) {
        JSValueOperand left(this, leftChild, ManualOperandSpeculation);
        JSValueOperand right(this, rightChild, ManualOperandSpeculation);
        speculateAnyBigInt(leftChild);
        speculateAnyBigInt(rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationValueSub, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

        jsValueResult(resultRegs, node);
        return;
    }
#endif // USE(BIGINT32)

    if (node->binaryUseKind() == HeapBigIntUse) {
        SpeculateCellOperand left(this, node->child1());
        SpeculateCellOperand right(this, node->child2());
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        callOperation(operationSubHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);

        jsValueResult(resultRegs, node);
        return;
    }

    CodeBlock* baselineCodeBlock = m_graph.baselineCodeBlockFor(node->origin.semantic);
    BytecodeIndex bytecodeIndex = node->origin.semantic.bytecodeIndex();
    BinaryArithProfile* arithProfile = baselineCodeBlock->binaryArithProfileForBytecodeIndex(bytecodeIndex);
    JITSubIC* subIC = jitCode()->common.addJITSubIC(arithProfile);
    auto repatchingFunction = operationValueSubOptimize;
    auto nonRepatchingFunction = operationValueSub;

    compileMathIC(node, subIC, repatchingFunction, nonRepatchingFunction);
}

template <typename Generator, typename RepatchingFunction, typename NonRepatchingFunction>
void SpeculativeJIT::compileMathIC(Node* node, JITBinaryMathIC<Generator>* mathIC, RepatchingFunction repatchingFunction, NonRepatchingFunction nonRepatchingFunction)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    std::optional<JSValueOperand> left;
    std::optional<JSValueOperand> right;

    JSValueRegs leftRegs;
    JSValueRegs rightRegs;

    FPRTemporary leftNumber(this);
    FPRTemporary rightNumber(this);
    FPRReg leftFPR = leftNumber.fpr();
    FPRReg rightFPR = rightNumber.fpr();

    GPRReg scratchGPR = InvalidGPRReg;

#if USE(JSVALUE64)
    GPRTemporary gprScratch(this);
    scratchGPR = gprScratch.gpr();
    GPRTemporary result(this);
    JSValueRegs resultRegs = JSValueRegs(result.gpr());
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs = JSValueRegs(resultPayload.gpr(), resultTag.gpr());
    scratchGPR = resultRegs.tagGPR();
#endif

    SnippetOperand leftOperand(m_state.forNode(leftChild).resultType());
    SnippetOperand rightOperand(m_state.forNode(rightChild).resultType());

    // The snippet generator does not support both operands being constant. If the left
    // operand is already const, we'll ignore the right operand's constness.
    if (leftChild->isInt32Constant())
        leftOperand.setConstInt32(leftChild->asInt32());
    else if (rightChild->isInt32Constant())
        rightOperand.setConstInt32(rightChild->asInt32());

    ASSERT(!leftOperand.isConst() || !rightOperand.isConst());
    ASSERT(!(Generator::isLeftOperandValidConstant(leftOperand) && Generator::isRightOperandValidConstant(rightOperand)));

    if (!Generator::isLeftOperandValidConstant(leftOperand)) {
        left.emplace(this, leftChild);
        leftRegs = left->jsValueRegs();
    }
    if (!Generator::isRightOperandValidConstant(rightOperand)) {
        right.emplace(this, rightChild);
        rightRegs = right->jsValueRegs();
    }

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = label();
#endif

    Box<MathICGenerationState> addICGenerationState = Box<MathICGenerationState>::create();
    mathIC->m_generator = Generator(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, leftFPR, rightFPR, scratchGPR);

    bool shouldEmitProfiling = false;
    bool generatedInline = mathIC->generateInline(*this, *addICGenerationState, shouldEmitProfiling);
    if (generatedInline) {
        ASSERT(!addICGenerationState->slowPathJumps.empty());

        Vector<SilentRegisterSavePlan> savePlans;
        silentSpillAllRegistersImpl(false, savePlans, resultRegs);

        auto done = label();

        addSlowPathGeneratorLambda([=, this, savePlans = WTFMove(savePlans)] () {
            addICGenerationState->slowPathJumps.link(this);
            addICGenerationState->slowPathStart = label();
#if ENABLE(MATH_IC_STATS)
            auto slowPathStart = label();
#endif

            auto innerLeftRegs = leftRegs;
            auto innerRightRegs = rightRegs;
            if (Generator::isLeftOperandValidConstant(leftOperand)) {
                innerLeftRegs = resultRegs;
                moveValue(leftChild->asJSValue(), innerLeftRegs);
            } else if (Generator::isRightOperandValidConstant(rightOperand)) {
                innerRightRegs = resultRegs;
                moveValue(rightChild->asJSValue(), innerRightRegs);
            }

            if (addICGenerationState->shouldSlowPathRepatch)
                addICGenerationState->slowPathCall = callOperationWithSilentSpill(savePlans, repatchingFunction, resultRegs, LinkableConstant::globalObject(*this, node), innerLeftRegs, innerRightRegs, TrustedImmPtr(mathIC));
            else
                addICGenerationState->slowPathCall = callOperationWithSilentSpill(savePlans, nonRepatchingFunction, resultRegs, LinkableConstant::globalObject(*this, node), innerLeftRegs, innerRightRegs);

            jump().linkTo(done, this);

            addLinkTask([=] (LinkBuffer& linkBuffer) {
                mathIC->finalizeInlineCode(*addICGenerationState, linkBuffer);
            });

#if ENABLE(MATH_IC_STATS)
            auto slowPathEnd = label();
            addLinkTask([=] (LinkBuffer& linkBuffer) {
                size_t size = static_cast<char*>(linkBuffer.locationOf(slowPathEnd).taggedPtr()) - static_cast<char*>(linkBuffer.locationOf(slowPathStart).taggedPtr());
                mathIC->m_generatedCodeSize += size;
            });
#endif

        });
    } else {
        if (Generator::isLeftOperandValidConstant(leftOperand)) {
            left.emplace(this, leftChild);
            leftRegs = left->jsValueRegs();
        } else if (Generator::isRightOperandValidConstant(rightOperand)) {
            right.emplace(this, rightChild);
            rightRegs = right->jsValueRegs();
        }

        flushRegisters();
        callOperation(nonRepatchingFunction, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);
    }

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = static_cast<char*>(linkBuffer.locationOf(inlineEnd).taggedPtr()) - static_cast<char*>(linkBuffer.locationOf(inlineStart).taggedPtr());
        mathIC->m_generatedCodeSize += size;
    });
#endif

    jsValueResult(resultRegs, node);
    return;
}

void SpeculativeJIT::compileInstanceOfCustom(Node* node)
{
    // We could do something smarter here but this case is currently super rare and unless
    // Symbol.hasInstance becomes popular will likely remain that way.

    JSValueOperand value(this, node->child1());
    SpeculateCellOperand constructor(this, node->child2());
    JSValueOperand hasInstanceValue(this, node->child3());
    GPRTemporary result(this);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg constructorGPR = constructor.gpr();
    JSValueRegs hasInstanceRegs = hasInstanceValue.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    Jump slowCase = jump();

    addSlowPathGenerator(slowPathCall(slowCase, this, operationInstanceOfCustom, resultGPR, LinkableConstant::globalObject(*this, node), valueRegs, constructorGPR, hasInstanceRegs));

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileIsCellWithType(Node* node)
{
    switch (node->child1().useKind()) {
    case UntypedUse: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value, PayloadWord);

        JSValueRegs valueRegs = value.jsValueRegs();
        GPRReg resultGPR = result.gpr();

        Jump isNotCell = branchIfNotCell(valueRegs);

        compare8(Equal,
            Address(valueRegs.payloadGPR(), JSCell::typeInfoTypeOffset()),
            TrustedImm32(node->queriedType()),
            resultGPR);
        blessBoolean(resultGPR);
        Jump done = jump();

        isNotCell.link(this);
        moveFalseTo(resultGPR);

        done.link(this);
        blessedBooleanResult(resultGPR, node);
        return;
    }

    case CellUse: {
        SpeculateCellOperand cell(this, node->child1());
        GPRTemporary result(this, Reuse, cell);

        GPRReg cellGPR = cell.gpr();
        GPRReg resultGPR = result.gpr();

        compare8(Equal,
            Address(cellGPR, JSCell::typeInfoTypeOffset()),
            TrustedImm32(node->queriedType()),
            resultGPR);
        blessBoolean(resultGPR);
        blessedBooleanResult(resultGPR, node);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileIsTypedArrayView(Node* node)
{
    JSValueOperand value(this, node->child1());
    GPRTemporary result(this, Reuse, value, PayloadWord);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    Jump isNotCell = branchIfNotCell(valueRegs);

    load8(Address(valueRegs.payloadGPR(), JSCell::typeInfoTypeOffset()), resultGPR);
    sub32(TrustedImm32(FirstTypedArrayType), resultGPR);
    compare32(Below,
        resultGPR,
        TrustedImm32(NumberOfTypedArrayTypesExcludingDataView),
        resultGPR);
    blessBoolean(resultGPR);
    Jump done = jump();

    isNotCell.link(this);
    moveFalseTo(resultGPR);

    done.link(this);
    blessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileHasStructureWithFlags(Node* node)
{
    SpeculateCellOperand object(this, node->child1());
    GPRTemporary result(this, Reuse, object);

    GPRReg objectGPR = object.gpr();
    GPRReg resultGPR = result.gpr();

    emitLoadStructure(vm(), objectGPR, resultGPR);
    test32(NonZero, Address(resultGPR, Structure::bitFieldOffset()), TrustedImm32(node->structureFlags()), resultGPR);

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileToObjectOrCallObjectConstructor(Node* node)
{
    RELEASE_ASSERT(node->child1().useKind() == UntypedUse);

    JSValueOperand value(this, node->child1());
    GPRTemporary result(this, Reuse, value, PayloadWord);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    JumpList slowCases;
    slowCases.append(branchIfNotCell(valueRegs));
    slowCases.append(branchIfNotObject(valueRegs.payloadGPR()));
    move(valueRegs.payloadGPR(), resultGPR);

    if (node->op() == ToObject) {
        UniquedStringImpl* errorMessage = nullptr;
        if (node->identifierNumber() != UINT32_MAX)
            errorMessage = identifierUID(node->identifierNumber());
        addSlowPathGenerator(slowPathCall(slowCases, this, operationToObject, resultGPR, LinkableConstant::globalObject(*this, node), valueRegs, TrustedImmPtr(errorMessage)));
    } else
        addSlowPathGenerator(slowPathCall(slowCases, this, operationCallObjectConstructor, resultGPR, LinkableConstant(*this, node->cellOperand()->cell()), valueRegs));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileArithAdd(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        ASSERT(!shouldCheckNegativeZero(node->arithMode()));

        if (node->child2()->isInt32Constant()) {
            SpeculateInt32Operand op1(this, node->child1());
            GPRTemporary result(this, Reuse, op1);

            GPRReg gpr1 = op1.gpr();
            int32_t imm2 = node->child2()->asInt32();
            GPRReg gprResult = result.gpr();

            if (!shouldCheckOverflow(node->arithMode())) {
                add32(Imm32(imm2), gpr1, gprResult);
                strictInt32Result(gprResult, node);
                return;
            }

            Jump check = branchAdd32(Overflow, gpr1, Imm32(imm2), gprResult);
            if (gpr1 == gprResult) {
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, check,
                    SpeculationRecovery(SpeculativeAddImmediate, gpr1, imm2));
            } else
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, check);

            strictInt32Result(gprResult, node);
            return;
        }
                
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this, Reuse, op1, op2);

        GPRReg gpr1 = op1.gpr();
        GPRReg gpr2 = op2.gpr();
        GPRReg gprResult = result.gpr();

        if (!shouldCheckOverflow(node->arithMode()))
            add32(gpr1, gpr2, gprResult);
        else {
            Jump check = branchAdd32(Overflow, gpr1, gpr2, gprResult);
                
            if (gpr1 == gprResult && gpr2 == gprResult)
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, check, SpeculationRecovery(SpeculativeAddSelf, gprResult, gpr2));
            else if (gpr1 == gprResult)
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, check, SpeculationRecovery(SpeculativeAdd, gprResult, gpr2));
            else if (gpr2 == gprResult)
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, check, SpeculationRecovery(SpeculativeAdd, gprResult, gpr1));
            else
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, check);
        }

        strictInt32Result(gprResult, node);
        return;
    }
        
#if USE(JSVALUE64)
    case Int52RepUse: {
        ASSERT(shouldCheckOverflow(node->arithMode()));
        ASSERT(!shouldCheckNegativeZero(node->arithMode()));

        // Will we need an overflow check? If we can prove that neither input can be
        // Int52 then the overflow check will not be necessary.
        if (!m_state.forNode(node->child1()).couldBeType(SpecNonInt32AsInt52)
            && !m_state.forNode(node->child2()).couldBeType(SpecNonInt32AsInt52)) {
            SpeculateWhicheverInt52Operand op1(this, node->child1());
            SpeculateWhicheverInt52Operand op2(this, node->child2(), op1);
            GPRTemporary result(this, Reuse, op1);
            add64(op1.gpr(), op2.gpr(), result.gpr());
            int52Result(result.gpr(), node, op1.format());
            return;
        }
        
        SpeculateInt52Operand op1(this, node->child1());
        SpeculateInt52Operand op2(this, node->child2());
        GPRTemporary result(this);
        move(op1.gpr(), result.gpr());
        speculationCheck(
            Int52Overflow, JSValueRegs(), nullptr,
            branchAdd64(Overflow, op2.gpr(), result.gpr()));
        int52Result(result.gpr(), node);
        return;
    }
#endif // USE(JSVALUE64)
    
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1, op2);

        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        addDouble(reg1, reg2, result.fpr());

        doubleResult(result.fpr(), node);
        return;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileArithAbs(Node* node)
{
    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateStrictInt32Operand op1(this, node->child1());
        GPRTemporary result(this, Reuse, op1);
        GPRTemporary scratch(this);

        move(op1.gpr(), result.gpr());
        rshift32(result.gpr(), TrustedImm32(31), scratch.gpr());
        add32(scratch.gpr(), result.gpr());
        xor32(scratch.gpr(), result.gpr());
        if (shouldCheckOverflow(node->arithMode()))
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchTest32(Signed, result.gpr()));
        strictInt32Result(result.gpr(), node);
        break;
    }

    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRTemporary result(this);

        absDouble(op1.fpr(), result.fpr());
        doubleResult(result.fpr(), node);
        break;
    }

    default: {
        DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());
        JSValueOperand op1(this, node->child1());
        JSValueRegs op1Regs = op1.jsValueRegs();
        flushRegisters();
        FPRResult result(this);
        callOperation(operationArithAbs, result.fpr(), LinkableConstant::globalObject(*this, node), op1Regs);
        doubleResult(result.fpr(), node);
        break;
    }
    }
}

void SpeculativeJIT::compileArithClz32(Node* node)
{
    if (node->child1().useKind() == Int32Use || node->child1().useKind() == KnownInt32Use) {
        SpeculateInt32Operand value(this, node->child1());
        GPRTemporary result(this, Reuse, value);
        GPRReg valueReg = value.gpr();
        GPRReg resultReg = result.gpr();
        countLeadingZeros32(valueReg, resultReg);
        strictInt32Result(resultReg, node);
        return;
    }
    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    GPRTemporary result(this);
    GPRReg resultReg = result.gpr();
    flushRegisters();
    callOperation(operationArithClz32, resultReg, LinkableConstant::globalObject(*this, node), op1Regs);
    strictInt32Result(resultReg, node);
}

void SpeculativeJIT::compileArithDoubleUnaryOp(Node* node, Arith::UnaryFunction doubleFunction, Arith::UnaryOperation operation)
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRReg op1FPR = op1.fpr();

        flushRegisters();

        FPRResult result(this);
        callOperationWithoutExceptionCheck(doubleFunction, result.fpr(), op1FPR);

        doubleResult(result.fpr(), node);
        return;
    }

    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    flushRegisters();
    FPRResult result(this);
    callOperation(operation, result.fpr(), LinkableConstant::globalObject(*this, node), op1Regs);
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileArithSub(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        ASSERT(!shouldCheckNegativeZero(node->arithMode()));
        
        if (node->child2()->isInt32Constant()) {
            SpeculateInt32Operand op1(this, node->child1());
            int32_t imm2 = node->child2()->asInt32();
            GPRTemporary result(this);

            if (!shouldCheckOverflow(node->arithMode())) {
                move(op1.gpr(), result.gpr());
                sub32(Imm32(imm2), result.gpr());
            } else {
                GPRTemporary scratch(this);
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchSub32(Overflow, op1.gpr(), Imm32(imm2), result.gpr(), scratch.gpr()));
            }

            strictInt32Result(result.gpr(), node);
            return;
        }
            
        if (node->child1()->isInt32Constant()) {
            int32_t imm1 = node->child1()->asInt32();
            SpeculateInt32Operand op2(this, node->child2());
            GPRTemporary result(this);
                
            move(Imm32(imm1), result.gpr());
            if (!shouldCheckOverflow(node->arithMode()))
                sub32(op2.gpr(), result.gpr());
            else
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchSub32(Overflow, op2.gpr(), result.gpr()));
                
            strictInt32Result(result.gpr(), node);
            return;
        }
            
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this);

        if (!shouldCheckOverflow(node->arithMode())) {
            move(op1.gpr(), result.gpr());
            sub32(op2.gpr(), result.gpr());
        } else
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchSub32(Overflow, op1.gpr(), op2.gpr(), result.gpr()));

        strictInt32Result(result.gpr(), node);
        return;
    }
        
#if USE(JSVALUE64)
    case Int52RepUse: {
        ASSERT(shouldCheckOverflow(node->arithMode()));
        ASSERT(!shouldCheckNegativeZero(node->arithMode()));

        // Will we need an overflow check? If we can prove that neither input can be
        // Int52 then the overflow check will not be necessary.
        if (!m_state.forNode(node->child1()).couldBeType(SpecNonInt32AsInt52)
            && !m_state.forNode(node->child2()).couldBeType(SpecNonInt32AsInt52)) {
            SpeculateWhicheverInt52Operand op1(this, node->child1());
            SpeculateWhicheverInt52Operand op2(this, node->child2(), op1);
            GPRTemporary result(this, Reuse, op1);
            move(op1.gpr(), result.gpr());
            sub64(op2.gpr(), result.gpr());
            int52Result(result.gpr(), node, op1.format());
            return;
        }
        
        SpeculateInt52Operand op1(this, node->child1());
        SpeculateInt52Operand op2(this, node->child2());
        GPRTemporary result(this);
        move(op1.gpr(), result.gpr());
        speculationCheck(
            Int52Overflow, JSValueRegs(), nullptr,
            branchSub64(Overflow, op2.gpr(), result.gpr()));
        int52Result(result.gpr(), node);
        return;
    }
#endif // USE(JSVALUE64)

    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1);

        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        subDouble(reg1, reg2, result.fpr());

        doubleResult(result.fpr(), node);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileIncOrDec(Node* node)
{
    // In all other cases the node should have been transformed into an add or a sub by FixupPhase
    ASSERT(node->child1().useKind() == UntypedUse);

    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    auto operation = node->op() == Inc ? operationInc : operationDec;
    callOperation(operation, resultRegs, LinkableConstant::globalObject(*this, node), op1Regs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileValueNegate(Node* node)
{
    // FIXME: add a fast path, at least for BigInt32, but probably also for HeapBigInt here.
    CodeBlock* baselineCodeBlock = m_graph.baselineCodeBlockFor(node->origin.semantic);
    BytecodeIndex bytecodeIndex = node->origin.semantic.bytecodeIndex();
    UnaryArithProfile* arithProfile = baselineCodeBlock->unaryArithProfileForBytecodeIndex(bytecodeIndex);
    JITNegIC* negIC = jitCode()->common.addJITNegIC(arithProfile);
    auto repatchingFunction = operationArithNegateOptimize;
    auto nonRepatchingFunction = operationArithNegate;
    compileMathIC(node, negIC, repatchingFunction, nonRepatchingFunction);
}

void SpeculativeJIT::compileArithNegate(Node* node)
{
    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateInt32Operand op1(this, node->child1());
        GPRTemporary result(this);

        move(op1.gpr(), result.gpr());

        // Note: there is no notion of being not used as a number, but someone
        // caring about negative zero.
        
        if (!shouldCheckOverflow(node->arithMode()))
            neg32(result.gpr());
        else if (!shouldCheckNegativeZero(node->arithMode()))
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchNeg32(Overflow, result.gpr()));
        else {
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchTest32(Zero, result.gpr(), TrustedImm32(0x7fffffff)));
            neg32(result.gpr());
        }

        strictInt32Result(result.gpr(), node);
        return;
    }

#if USE(JSVALUE64)
    case Int52RepUse: {
        ASSERT(shouldCheckOverflow(node->arithMode()));
        
        if (!m_state.forNode(node->child1()).couldBeType(SpecNonInt32AsInt52)) {
            SpeculateWhicheverInt52Operand op1(this, node->child1());
            GPRTemporary result(this);
            GPRReg op1GPR = op1.gpr();
            GPRReg resultGPR = result.gpr();
            move(op1GPR, resultGPR);
            neg64(resultGPR);
            if (shouldCheckNegativeZero(node->arithMode())) {
                speculationCheck(
                    NegativeZero, JSValueRegs(), nullptr,
                    branchTest64(Zero, resultGPR));
            }
            int52Result(resultGPR, node, op1.format());
            return;
        }
        
        SpeculateInt52Operand op1(this, node->child1());
        GPRTemporary result(this);
        GPRReg op1GPR = op1.gpr();
        GPRReg resultGPR = result.gpr();
        move(op1GPR, resultGPR);
        speculationCheck(
            Int52Overflow, JSValueRegs(), nullptr,
            branchNeg64(Overflow, resultGPR));
        if (shouldCheckNegativeZero(node->arithMode())) {
            speculationCheck(
                NegativeZero, JSValueRegs(), nullptr,
                branchTest64(Zero, resultGPR));
        }
        int52Result(resultGPR, node);
        return;
    }
#endif // USE(JSVALUE64)
        
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRTemporary result(this);
        
        negateDouble(op1.fpr(), result.fpr());
        
        doubleResult(result.fpr(), node);
        return;
    }
        
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }
}

template <typename Generator, typename RepatchingFunction, typename NonRepatchingFunction>
void SpeculativeJIT::compileMathIC(Node* node, JITUnaryMathIC<Generator>* mathIC, RepatchingFunction repatchingFunction, NonRepatchingFunction nonRepatchingFunction)
{
    GPRTemporary gprScratch(this);
    GPRReg scratchGPR = gprScratch.gpr();
    JSValueOperand childOperand(this, node->child1());
    JSValueRegs childRegs = childOperand.jsValueRegs();
#if USE(JSVALUE64)
    GPRTemporary result(this, Reuse, childOperand);
    JSValueRegs resultRegs(result.gpr());
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs(resultPayload.gpr(), resultTag.gpr());
#endif

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = label();
#endif

    Box<MathICGenerationState> icGenerationState = Box<MathICGenerationState>::create();
    mathIC->m_generator = Generator(resultRegs, childRegs, scratchGPR);

    bool shouldEmitProfiling = false;
    bool generatedInline = mathIC->generateInline(*this, *icGenerationState, shouldEmitProfiling);
    if (generatedInline) {
        ASSERT(!icGenerationState->slowPathJumps.empty());

        Vector<SilentRegisterSavePlan> savePlans;
        silentSpillAllRegistersImpl(false, savePlans, resultRegs);

        auto done = label();

        addSlowPathGeneratorLambda([=, this, savePlans = WTFMove(savePlans)] () {
            icGenerationState->slowPathJumps.link(this);
            icGenerationState->slowPathStart = label();
#if ENABLE(MATH_IC_STATS)
            auto slowPathStart = label();
#endif

            if (icGenerationState->shouldSlowPathRepatch)
                icGenerationState->slowPathCall = callOperationWithSilentSpill(savePlans, repatchingFunction, resultRegs, LinkableConstant::globalObject(*this, node), childRegs, TrustedImmPtr(mathIC));
            else
                icGenerationState->slowPathCall = callOperationWithSilentSpill(savePlans, nonRepatchingFunction, resultRegs, LinkableConstant::globalObject(*this, node), childRegs);

            jump().linkTo(done, this);

            addLinkTask([=] (LinkBuffer& linkBuffer) {
                mathIC->finalizeInlineCode(*icGenerationState, linkBuffer);
            });

#if ENABLE(MATH_IC_STATS)
            auto slowPathEnd = label();
            addLinkTask([=] (LinkBuffer& linkBuffer) {
                size_t size = static_cast<char*>(linkBuffer.locationOf(slowPathEnd).taggedPtr()) - static_cast<char*>(linkBuffer.locationOf(slowPathStart).taggedPtr());
                mathIC->m_generatedCodeSize += size;
            });
#endif

        });
    } else {
        flushRegisters();
        callOperation(nonRepatchingFunction, resultRegs, LinkableConstant::globalObject(*this, node), childRegs);
    }

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = static_cast<char*>(linkBuffer.locationOf(inlineEnd).taggedPtr()) - static_cast<char*>(linkBuffer.locationOf(inlineStart).taggedPtr());
        mathIC->m_generatedCodeSize += size;
    });
#endif

    jsValueResult(resultRegs, node);
    return;
}

void SpeculativeJIT::compileValueMul(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

#if USE(BIGINT32)
    // FIXME: Introduce another BigInt32 code generation: binary use kinds are BigIntUse32, but result is SpecAnyInt and accepting overflow.
    // Let's distinguish these modes based on result type information by introducing NodeResultBigInt32.
    // https://bugs.webkit.org/show_bug.cgi?id=210957
    // https://bugs.webkit.org/show_bug.cgi?id=211040
    if (node->binaryUseKind() == BigInt32Use) {
        // FIXME: the code between compileValueAdd, compileValueSub and compileValueMul for BigInt32 is nearly identical, so try to get rid of the duplication.
        SpeculateBigInt32Operand left(this, node->child1());
        SpeculateBigInt32Operand right(this, node->child2());
        GPRTemporary result(this);
        GPRTemporary temp(this);

        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg tempGPR = temp.gpr();

        unboxBigInt32(leftGPR, resultGPR);
        unboxBigInt32(rightGPR, tempGPR);

        Jump check = branchMul32(Overflow, resultGPR, tempGPR, resultGPR);

        speculationCheck(BigInt32Overflow, JSValueRegs(), nullptr, check);

        boxBigInt32(resultGPR);
        jsValueResult(resultGPR, node);
        return;
    }
    // FIXME: add support for mixed BigInt32/HeapBigInt
#endif

    if (leftChild.useKind() == HeapBigIntUse && rightChild.useKind() == HeapBigIntUse) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        callOperation(operationMulHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);

        jsValueResult(resultRegs, node);
        return;
    }

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node()) || node->isBinaryUseKind(AnyBigIntUse)) {
        JSValueOperand left(this, leftChild, ManualOperandSpeculation);
        JSValueOperand right(this, rightChild, ManualOperandSpeculation);
        speculate(node, leftChild);
        speculate(node, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationValueMul, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

        jsValueResult(resultRegs, node);
        return;
    }

    CodeBlock* baselineCodeBlock = m_graph.baselineCodeBlockFor(node->origin.semantic);
    BytecodeIndex bytecodeIndex = node->origin.semantic.bytecodeIndex();
    BinaryArithProfile* arithProfile = baselineCodeBlock->binaryArithProfileForBytecodeIndex(bytecodeIndex);
    JITMulIC* mulIC = jitCode()->common.addJITMulIC(arithProfile);
    auto repatchingFunction = operationValueMulOptimize;
    auto nonRepatchingFunction = operationValueMul;

    compileMathIC(node, mulIC, repatchingFunction, nonRepatchingFunction);
}

void SpeculativeJIT::compileArithMul(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        if (node->child2()->isInt32Constant()) {
            SpeculateInt32Operand op1(this, node->child1());
            GPRTemporary result(this);

            int32_t imm = node->child2()->asInt32();
            GPRReg op1GPR = op1.gpr();
            GPRReg resultGPR = result.gpr();

            if (!shouldCheckOverflow(node->arithMode()))
                mul32(Imm32(imm), op1GPR, resultGPR);
            else {
                speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr,
                    branchMul32(Overflow, op1GPR, Imm32(imm), resultGPR));
            }

            // The only way to create negative zero with a constant is:
            // -negative-op1 * 0.
            // -zero-op1 * negative constant.
            if (shouldCheckNegativeZero(node->arithMode())) {
                if (!imm)
                    speculationCheck(NegativeZero, JSValueRegs(), nullptr, branchTest32(Signed, op1GPR));
                else if (imm < 0) {
                    if (shouldCheckOverflow(node->arithMode()))
                        speculationCheck(NegativeZero, JSValueRegs(), nullptr, branchTest32(Zero, resultGPR));
                    else
                        speculationCheck(NegativeZero, JSValueRegs(), nullptr, branchTest32(Zero, op1GPR));
                }
            }

            strictInt32Result(resultGPR, node);
            return;
        }
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this);

        GPRReg reg1 = op1.gpr();
        GPRReg reg2 = op2.gpr();

        // We can perform truncated multiplications if we get to this point, because if the
        // fixup phase could not prove that it would be safe, it would have turned us into
        // a double multiplication.
        if (!shouldCheckOverflow(node->arithMode()))
            mul32(reg1, reg2, result.gpr());
        else {
            speculationCheck(
                ExitKind::Overflow, JSValueRegs(), nullptr,
                branchMul32(Overflow, reg1, reg2, result.gpr()));
        }
            
        // Check for negative zero, if the users of this node care about such things.
        if (shouldCheckNegativeZero(node->arithMode())) {
            Jump resultNonZero = branchTest32(NonZero, result.gpr());
            speculationCheck(NegativeZero, JSValueRegs(), nullptr, branchTest32(Signed, reg1));
            speculationCheck(NegativeZero, JSValueRegs(), nullptr, branchTest32(Signed, reg2));
            resultNonZero.link(this);
        }

        strictInt32Result(result.gpr(), node);
        return;
    }

#if USE(JSVALUE64)
    case Int52RepUse: {
        ASSERT(shouldCheckOverflow(node->arithMode()));
        
        // This is super clever. We want to do an int52 multiplication and check the
        // int52 overflow bit. There is no direct hardware support for this, but we do
        // have the ability to do an int64 multiplication and check the int64 overflow
        // bit. We leverage that. Consider that a, b are int52 numbers inside int64
        // registers, with the high 12 bits being sign-extended. We can do:
        //
        //     (a * (b << 12))
        //
        // This will give us a left-shifted int52 (value is in high 52 bits, low 16
        // bits are zero) plus the int52 overflow bit. I.e. whether this 64-bit
        // multiplication overflows is identical to whether the 'a * b' 52-bit
        // multiplication overflows.
        //
        // In our nomenclature, this is:
        //
        //     strictInt52(a) * int52(b) => int52
        //
        // That is "strictInt52" means unshifted and "int52" means left-shifted by 16
        // bits.
        //
        // We don't care which of op1 or op2 serves as the left-shifted operand, so
        // we just do whatever is more convenient for op1 and have op2 do the
        // opposite. This ensures that we do at most one shift.

        SpeculateWhicheverInt52Operand op1(this, node->child1());
        SpeculateWhicheverInt52Operand op2(this, node->child2(), OppositeShift, op1);
        GPRTemporary result(this);
        
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
        GPRReg resultGPR = result.gpr();

        speculationCheck(
            Int52Overflow, JSValueRegs(), nullptr,
            branchMul64(Overflow, op1GPR, op2GPR, resultGPR));

        if (shouldCheckNegativeZero(node->arithMode())) {
            Jump resultNonZero = branchTest64(
                NonZero, resultGPR);
            speculationCheck(
                NegativeZero, JSValueRegs(), nullptr,
                branch64(LessThan, op1GPR, TrustedImm32(0)));
            speculationCheck(
                NegativeZero, JSValueRegs(), nullptr,
                branch64(LessThan, op2GPR, TrustedImm32(0)));
            resultNonZero.link(this);
        }
        
        int52Result(resultGPR, node);
        return;
    }
#endif // USE(JSVALUE64)
        
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1, op2);
        
        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        
        mulDouble(reg1, reg2, result.fpr());
        
        doubleResult(result.fpr(), node);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileValueDiv(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    // FIXME: add a fast path for BigInt32. Currently we go through the slow path, because of how ugly the code for Div gets.
    // https://bugs.webkit.org/show_bug.cgi?id=211041

    if (node->isBinaryUseKind(HeapBigIntUse)) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        callOperation(operationDivHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);

        jsValueResult(resultRegs, node);
        return;
    }

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node()) || node->isBinaryUseKind(AnyBigIntUse) || node->isBinaryUseKind(BigInt32Use)) {
        JSValueOperand left(this, leftChild, ManualOperandSpeculation);
        JSValueOperand right(this, rightChild, ManualOperandSpeculation);
        speculate(node, leftChild);
        speculate(node, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationValueDiv, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

        jsValueResult(resultRegs, node);
        return;
    }

    ASSERT(node->isBinaryUseKind(UntypedUse));

    std::optional<JSValueOperand> left;
    std::optional<JSValueOperand> right;

    JSValueRegs leftRegs;
    JSValueRegs rightRegs;

    FPRTemporary leftNumber(this);
    FPRTemporary rightNumber(this);
    FPRReg leftFPR = leftNumber.fpr();
    FPRReg rightFPR = rightNumber.fpr();
    FPRTemporary fprScratch(this);
    FPRReg scratchFPR = fprScratch.fpr();

#if USE(JSVALUE64)
    GPRTemporary result(this);
    JSValueRegs resultRegs = JSValueRegs(result.gpr());
    GPRTemporary scratch(this);
    GPRReg scratchGPR = scratch.gpr();
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs = JSValueRegs(resultPayload.gpr(), resultTag.gpr());
    GPRReg scratchGPR = resultTag.gpr();
#endif

    SnippetOperand leftOperand(m_state.forNode(leftChild).resultType());
    SnippetOperand rightOperand(m_state.forNode(rightChild).resultType());

    if (leftChild->isInt32Constant())
        leftOperand.setConstInt32(leftChild->asInt32());
#if USE(JSVALUE64)
    else if (leftChild->isDoubleConstant())
        leftOperand.setConstDouble(leftChild->asNumber());
#endif

    if (leftOperand.isConst()) {
        // The snippet generator only supports 1 argument as a constant.
        // Ignore the rightChild's const-ness.
    } else if (rightChild->isInt32Constant())
        rightOperand.setConstInt32(rightChild->asInt32());
#if USE(JSVALUE64)
    else if (rightChild->isDoubleConstant())
        rightOperand.setConstDouble(rightChild->asNumber());
#endif

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst()) {
        left.emplace(this, leftChild);
        leftRegs = left->jsValueRegs();
    }
    if (!rightOperand.isConst()) {
        right.emplace(this, rightChild);
        rightRegs = right->jsValueRegs();
    }

    JITDivGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs,
        leftFPR, rightFPR, scratchGPR, scratchFPR);
    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().append(jump());

    gen.slowPathJumpList().link(this);

    if (leftOperand.isConst()) {
        leftRegs = resultRegs;
        moveValue(leftChild->asJSValue(), leftRegs);
    }
    if (rightOperand.isConst()) {
        rightRegs = resultRegs;
        moveValue(rightChild->asJSValue(), rightRegs);
    }

    callOperationWithSilentSpill(operationValueDiv, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

    gen.endJumpList().link(this);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileArithDiv(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
#if CPU(X86_64)
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary eax(this, X86Registers::eax);
        GPRTemporary edx(this, X86Registers::edx);
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
    
        GPRReg op2TempGPR;
        GPRReg temp;
        if (op2GPR == X86Registers::eax || op2GPR == X86Registers::edx) {
            op2TempGPR = allocate();
            temp = op2TempGPR;
        } else {
            op2TempGPR = InvalidGPRReg;
            if (op1GPR == X86Registers::eax)
                temp = X86Registers::edx;
            else
                temp = X86Registers::eax;
        }
    
        ASSERT(temp != op1GPR);
        ASSERT(temp != op2GPR);
    
        add32(TrustedImm32(1), op2GPR, temp);
    
        Jump safeDenominator = branch32(Above, temp, TrustedImm32(1));
    
        JumpList done;
        if (shouldCheckOverflow(node->arithMode())) {
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchTest32(Zero, op2GPR));
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branch32(Equal, op1GPR, TrustedImm32(-2147483647-1)));
        } else {
            // This is the case where we convert the result to an int after we're done, and we
            // already know that the denominator is either -1 or 0. So, if the denominator is
            // zero, then the result should be zero. If the denominator is not zero (i.e. it's
            // -1) and the numerator is -2^31 then the result should be -2^31. Otherwise we
            // are happy to fall through to a normal division, since we're just dividing
            // something by negative 1.
        
            Jump notZero = branchTest32(NonZero, op2GPR);
            move(TrustedImm32(0), eax.gpr());
            done.append(jump());
        
            notZero.link(this);
            Jump notNeg2ToThe31 =
                branch32(NotEqual, op1GPR, TrustedImm32(-2147483647-1));
            zeroExtend32ToWord(op1GPR, eax.gpr());
            done.append(jump());
        
            notNeg2ToThe31.link(this);
        }
    
        safeDenominator.link(this);
    
        // If the user cares about negative zero, then speculate that we're not about
        // to produce negative zero.
        if (shouldCheckNegativeZero(node->arithMode())) {
            Jump numeratorNonZero = branchTest32(NonZero, op1GPR);
            speculationCheck(NegativeZero, JSValueRegs(), nullptr, branch32(LessThan, op2GPR, TrustedImm32(0)));
            numeratorNonZero.link(this);
        }
    
        if (op2TempGPR != InvalidGPRReg) {
            move(op2GPR, op2TempGPR);
            op2GPR = op2TempGPR;
        }
            
        move(op1GPR, eax.gpr());
        x86ConvertToDoubleWord32();
        x86Div32(op2GPR);
            
        if (op2TempGPR != InvalidGPRReg)
            unlock(op2TempGPR);

        // Check that there was no remainder. If there had been, then we'd be obligated to
        // produce a double result instead.
        if (shouldCheckOverflow(node->arithMode()))
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchTest32(NonZero, edx.gpr()));
        
        done.link(this);
        strictInt32Result(eax.gpr(), node);
#elif HAVE(ARM_IDIV_INSTRUCTIONS) || CPU(ARM64)
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
        GPRTemporary quotient(this);
        GPRTemporary multiplyAnswer(this);

        // If the user cares about negative zero, then speculate that we're not about
        // to produce negative zero.
        if (shouldCheckNegativeZero(node->arithMode())) {
            Jump numeratorNonZero = branchTest32(NonZero, op1GPR);
            speculationCheck(NegativeZero, JSValueRegs(), 0, branch32(LessThan, op2GPR, TrustedImm32(0)));
            numeratorNonZero.link(this);
        }

        if (shouldCheckOverflow(node->arithMode()))
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchTest32(Zero, op2GPR));

        // Note that it is fine that sdiv with 0-divisor. The resulted value is zero (no trap).
        assembler().sdiv<32>(quotient.gpr(), op1GPR, op2GPR);

        // Check that there was no remainder. If there had been, then we'd be obligated to
        // produce a double result instead.
        if (shouldCheckOverflow(node->arithMode())) {
            speculationCheck(ExitKind::Overflow, JSValueRegs(), 0, branchMul32(Overflow, quotient.gpr(), op2GPR, multiplyAnswer.gpr()));
            speculationCheck(ExitKind::Overflow, JSValueRegs(), 0, branch32(NotEqual, multiplyAnswer.gpr(), op1GPR));
        }

        strictInt32Result(quotient.gpr(), node);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
        break;
    }
        
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1);
        
        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        divDouble(reg1, reg2, result.fpr());
        
        doubleResult(result.fpr(), node);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileArithFRound(Node* node)
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRTemporary result(this, op1);
        convertDoubleToFloat(op1.fpr(), result.fpr());
        convertFloatToDouble(result.fpr(), result.fpr());
        doubleResult(result.fpr(), node);
        return;
    }

    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    flushRegisters();
    FPRResult result(this);
    callOperation(operationArithFRound, result.fpr(), LinkableConstant::globalObject(*this, node), op1Regs);
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileArithF16Round(Node* node)
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRTemporary result(this, op1);
        convertDoubleToFloat16(op1.fpr(), result.fpr());
        convertFloat16ToDouble(result.fpr(), result.fpr());
        doubleResult(result.fpr(), node);
        return;
    }

    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    flushRegisters();
    FPRResult result(this);
    callOperation(operationArithF16Round, result.fpr(), LinkableConstant::globalObject(*this, node), op1Regs);
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileValueMod(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    // FIXME: add a fast path for BigInt32. Currently we go through the slow path, because of how ugly the code for Mod gets.

    if (node->binaryUseKind() == HeapBigIntUse) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        callOperation(operationModHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);

        jsValueResult(resultRegs, node);
        return;
    }

    DFG_ASSERT(m_graph, node, node->binaryUseKind() == UntypedUse || node->binaryUseKind() == AnyBigIntUse || node->binaryUseKind() == BigInt32Use, node->binaryUseKind());
    JSValueOperand op1(this, leftChild, ManualOperandSpeculation);
    JSValueOperand op2(this, rightChild, ManualOperandSpeculation);
    speculate(node, leftChild);
    speculate(node, rightChild);
    JSValueRegs op1Regs = op1.jsValueRegs();
    JSValueRegs op2Regs = op2.jsValueRegs();
    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationValueMod, resultRegs, LinkableConstant::globalObject(*this, node), op1Regs, op2Regs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileArithMod(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        // In the fast path, the dividend value could be the final result
        // (in case of |dividend| < |divisor|), so we speculate it as strict int32.
        SpeculateStrictInt32Operand op1(this, node->child1());
        
        if (node->child2()->isInt32Constant()) {
            int32_t divisor = node->child2()->asInt32();
            if (divisor > 1 && hasOneBitSet(divisor)) {
                unsigned logarithm = WTF::fastLog2(static_cast<uint32_t>(divisor));
                GPRReg dividendGPR = op1.gpr();
                GPRTemporary result(this);
                GPRReg resultGPR = result.gpr();

                // This is what LLVM generates. It's pretty crazy. Here's my
                // attempt at understanding it.
                
                // First, compute either divisor - 1, or 0, depending on whether
                // the dividend is negative:
                //
                // If dividend < 0:  resultGPR = divisor - 1
                // If dividend >= 0: resultGPR = 0
                move(dividendGPR, resultGPR);
                rshift32(TrustedImm32(31), resultGPR);
                urshift32(TrustedImm32(32 - logarithm), resultGPR);
                
                // Add in the dividend, so that:
                //
                // If dividend < 0:  resultGPR = dividend + divisor - 1
                // If dividend >= 0: resultGPR = dividend
                add32(dividendGPR, resultGPR);
                
                // Mask so as to only get the *high* bits. This rounds down
                // (towards negative infinity) resultGPR to the nearest multiple
                // of divisor, so that:
                //
                // If dividend < 0:  resultGPR = floor((dividend + divisor - 1) / divisor)
                // If dividend >= 0: resultGPR = floor(dividend / divisor)
                //
                // Note that this can be simplified to:
                //
                // If dividend < 0:  resultGPR = ceil(dividend / divisor)
                // If dividend >= 0: resultGPR = floor(dividend / divisor)
                //
                // Note that if the dividend is negative, resultGPR will also be negative.
                // Regardless of the sign of dividend, resultGPR will be rounded towards
                // zero, because of how things are conditionalized.
                and32(TrustedImm32(-divisor), resultGPR);
                
                // Subtract resultGPR from dividendGPR, which yields the remainder:
                //
                // resultGPR = dividendGPR - resultGPR
                neg32(resultGPR);
                add32(dividendGPR, resultGPR);
                
                if (shouldCheckNegativeZero(node->arithMode())) {
                    // Check that we're not about to create negative zero.
                    Jump numeratorPositive = branch32(GreaterThanOrEqual, dividendGPR, TrustedImm32(0));
                    speculationCheck(NegativeZero, JSValueRegs(), nullptr, branchTest32(Zero, resultGPR));
                    numeratorPositive.link(this);
                }

                strictInt32Result(resultGPR, node);
                return;
            }
        }
        
#if CPU(X86_64)
        if (node->child2()->isInt32Constant()) {
            int32_t divisor = node->child2()->asInt32();
            if (divisor && divisor != -1) {
                GPRReg op1Gpr = op1.gpr();

                GPRTemporary eax(this, X86Registers::eax);
                GPRTemporary edx(this, X86Registers::edx);
                GPRTemporary scratch(this);
                GPRReg scratchGPR = scratch.gpr();

                GPRReg op1SaveGPR;
                if (op1Gpr == X86Registers::eax || op1Gpr == X86Registers::edx) {
                    op1SaveGPR = allocate();
                    ASSERT(op1Gpr != op1SaveGPR);
                    move(op1Gpr, op1SaveGPR);
                } else
                    op1SaveGPR = op1Gpr;
                ASSERT(op1SaveGPR != X86Registers::eax);
                ASSERT(op1SaveGPR != X86Registers::edx);

                move(op1Gpr, eax.gpr());
                move(TrustedImm32(divisor), scratchGPR);
                x86ConvertToDoubleWord32();
                x86Div32(scratchGPR);
                if (shouldCheckNegativeZero(node->arithMode())) {
                    Jump numeratorPositive = branch32(GreaterThanOrEqual, op1SaveGPR, TrustedImm32(0));
                    speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchTest32(Zero, edx.gpr()));
                    numeratorPositive.link(this);
                }
            
                if (op1SaveGPR != op1Gpr)
                    unlock(op1SaveGPR);

                strictInt32Result(edx.gpr(), node);
                return;
            }
        }
#endif

        SpeculateInt32Operand op2(this, node->child2());
#if CPU(X86_64)
        GPRTemporary eax(this, X86Registers::eax);
        GPRTemporary edx(this, X86Registers::edx);
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
    
        GPRReg op2TempGPR;
        GPRReg temp;
        GPRReg op1SaveGPR;
    
        if (op2GPR == X86Registers::eax || op2GPR == X86Registers::edx) {
            op2TempGPR = allocate();
            temp = op2TempGPR;
        } else {
            op2TempGPR = InvalidGPRReg;
            if (op1GPR == X86Registers::eax)
                temp = X86Registers::edx;
            else
                temp = X86Registers::eax;
        }
    
        if (op1GPR == X86Registers::eax || op1GPR == X86Registers::edx) {
            op1SaveGPR = allocate();
            ASSERT(op1GPR != op1SaveGPR);
            move(op1GPR, op1SaveGPR);
        } else
            op1SaveGPR = op1GPR;
    
        ASSERT(temp != op1GPR);
        ASSERT(temp != op2GPR);
        ASSERT(op1SaveGPR != X86Registers::eax);
        ASSERT(op1SaveGPR != X86Registers::edx);
    
        add32(TrustedImm32(1), op2GPR, temp);
    
        Jump safeDenominator = branch32(Above, temp, TrustedImm32(1));
    
        JumpList done;
        
        // FIXME: -2^31 / -1 will actually yield negative zero, so we could have a
        // separate case for that. But it probably doesn't matter so much.
        if (shouldCheckOverflow(node->arithMode())) {
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchTest32(Zero, op2GPR));
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branch32(Equal, op1GPR, TrustedImm32(-2147483647-1)));
        } else {
            // This is the case where we convert the result to an int after we're done, and we
            // already know that the denominator is either -1 or 0. So, if the denominator is
            // zero, then the result should be zero. If the denominator is not zero (i.e. it's
            // -1) and the numerator is -2^31 then the result should be 0. Otherwise we are
            // happy to fall through to a normal division, since we're just dividing something
            // by negative 1.
        
            Jump notZero = branchTest32(NonZero, op2GPR);
            move(TrustedImm32(0), edx.gpr());
            done.append(jump());
        
            notZero.link(this);
            Jump notNeg2ToThe31 =
                branch32(NotEqual, op1GPR, TrustedImm32(-2147483647-1));
            move(TrustedImm32(0), edx.gpr());
            done.append(jump());
        
            notNeg2ToThe31.link(this);
        }
        
        safeDenominator.link(this);
            
        if (op2TempGPR != InvalidGPRReg) {
            move(op2GPR, op2TempGPR);
            op2GPR = op2TempGPR;
        }
            
        move(op1GPR, eax.gpr());
        x86ConvertToDoubleWord32();
        x86Div32(op2GPR);
            
        if (op2TempGPR != InvalidGPRReg)
            unlock(op2TempGPR);

        // Check that we're not about to create negative zero.
        if (shouldCheckNegativeZero(node->arithMode())) {
            Jump numeratorPositive = branch32(GreaterThanOrEqual, op1SaveGPR, TrustedImm32(0));
            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchTest32(Zero, edx.gpr()));
            numeratorPositive.link(this);
        }
    
        if (op1SaveGPR != op1GPR)
            unlock(op1SaveGPR);
            
        done.link(this);
        strictInt32Result(edx.gpr(), node);

#elif HAVE(ARM_IDIV_INSTRUCTIONS) || CPU(ARM64)
        GPRTemporary temp(this);
        GPRTemporary quotientThenRemainder(this);
        GPRTemporary multiplyAnswer(this);
        GPRReg dividendGPR = op1.gpr();
        GPRReg divisorGPR = op2.gpr();
        GPRReg quotientThenRemainderGPR = quotientThenRemainder.gpr();
        GPRReg multiplyAnswerGPR = multiplyAnswer.gpr();

        JumpList done;
    
        if (shouldCheckOverflow(node->arithMode()))
            speculationCheck(ExitKind::Overflow, JSValueRegs(), 0, branchTest32(Zero, divisorGPR));
        else {
            Jump denominatorNotZero = branchTest32(NonZero, divisorGPR);
            // We know that the low 32-bit of divisorGPR is 0, but we don't know if the high bits are.
            // So, use TrustedImm32(0) on ARM instead because done expects the result to be in DataFormatInt32.
            // Using an immediate 0 doesn't cost anything extra on ARM.
            move(TrustedImm32(0), quotientThenRemainderGPR);
            done.append(jump());
            denominatorNotZero.link(this);
        }

        assembler().sdiv<32>(quotientThenRemainderGPR, dividendGPR, divisorGPR);
        // FIXME: It seems like there are cases where we don't need this? What if we have
        // arithMode() == Arith::Unchecked?
        // https://bugs.webkit.org/show_bug.cgi?id=126444
        speculationCheck(ExitKind::Overflow, JSValueRegs(), 0, branchMul32(Overflow, quotientThenRemainderGPR, divisorGPR, multiplyAnswerGPR));
#if HAVE(ARM_IDIV_INSTRUCTIONS)
        assembler().sub(quotientThenRemainderGPR, dividendGPR, multiplyAnswerGPR);
#else
        assembler().sub<32>(quotientThenRemainderGPR, dividendGPR, multiplyAnswerGPR);
#endif

        // If the user cares about negative zero, then speculate that we're not about
        // to produce negative zero.
        if (shouldCheckNegativeZero(node->arithMode())) {
            // Check that we're not about to create negative zero.
            Jump numeratorPositive = branch32(GreaterThanOrEqual, dividendGPR, TrustedImm32(0));
            speculationCheck(ExitKind::Overflow, JSValueRegs(), 0, branchTest32(Zero, quotientThenRemainderGPR));
            numeratorPositive.link(this);
        }

        done.link(this);

        strictInt32Result(quotientThenRemainderGPR, node);
#else // not architecture that can do integer division
        RELEASE_ASSERT_NOT_REACHED();
#endif
        return;
    }
        
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        
        FPRReg op1FPR = op1.fpr();
        FPRReg op2FPR = op2.fpr();
        
        flushRegisters();
        
        FPRResult result(this);

        callOperationWithoutExceptionCheck(Math::fmodDouble, result.fpr(), op1FPR, op2FPR);
        
        doubleResult(result.fpr(), node);
        return;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileArithRounding(Node* node)
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand value(this, node->child1());
        FPRReg valueFPR = value.fpr();

        auto setResult = [&] (FPRReg resultFPR) {
            if (producesInteger(node->arithRoundingMode())) {
                GPRTemporary roundedResultAsInt32(this);
                FPRTemporary scratch(this);
                FPRReg scratchFPR = scratch.fpr();
                GPRReg resultGPR = roundedResultAsInt32.gpr();
                JumpList failureCases;
                branchConvertDoubleToInt32(resultFPR, resultGPR, failureCases, scratchFPR, shouldCheckNegativeZero(node->arithRoundingMode()));
                speculationCheck(ExitKind::Overflow, JSValueRegs(), node, failureCases);

                strictInt32Result(resultGPR, node);
            } else
                doubleResult(resultFPR, node);
        };

        if (supportsFloatingPointRounding()) {
            switch (node->op()) {
            case ArithRound: {
                FPRTemporary result(this);
                FPRReg resultFPR = result.fpr();
                if (producesInteger(node->arithRoundingMode()) && !shouldCheckNegativeZero(node->arithRoundingMode())) {
                    move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(0.5)), resultFPR);
                    addDouble(valueFPR, resultFPR);
                    floorDouble(resultFPR, resultFPR);
                } else {
                    ceilDouble(valueFPR, resultFPR);

                    FPRTemporary scratch(this);
                    FPRReg scratchFPR = scratch.fpr();
                    move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(-0.5)), scratchFPR);
                    addDouble(resultFPR, scratchFPR);

                    Jump shouldUseCeiled = branchDouble(DoubleLessThanOrEqualAndOrdered, scratchFPR, valueFPR);
                    move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(-1.0)), scratchFPR);
                    addDouble(scratchFPR, resultFPR);
                    shouldUseCeiled.link(this);
                }
                setResult(resultFPR);
                return;
            }

            case ArithFloor: {
                FPRTemporary rounded(this);
                FPRReg resultFPR = rounded.fpr();
                floorDouble(valueFPR, resultFPR);
                setResult(resultFPR);
                return;
            }

            case ArithCeil: {
                FPRTemporary rounded(this);
                FPRReg resultFPR = rounded.fpr();
                ceilDouble(valueFPR, resultFPR);
                setResult(resultFPR);
                return;
            }

            case ArithTrunc: {
                FPRTemporary rounded(this);
                FPRReg resultFPR = rounded.fpr();
                roundTowardZeroDouble(valueFPR, resultFPR);
                setResult(resultFPR);
                return;
            }

            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            flushRegisters();
            FPRResult roundedResultAsDouble(this);
            FPRReg resultFPR = roundedResultAsDouble.fpr();
            if (node->op() == ArithRound)
                callOperationWithoutExceptionCheck(Math::roundDouble, resultFPR, valueFPR);
            else if (node->op() == ArithFloor)
                callOperationWithoutExceptionCheck(Math::floorDouble, resultFPR, valueFPR);
            else if (node->op() == ArithCeil)
                callOperationWithoutExceptionCheck(Math::ceilDouble, resultFPR, valueFPR);
            else {
                ASSERT(node->op() == ArithTrunc);
                callOperationWithoutExceptionCheck(Math::truncDouble, resultFPR, valueFPR);
            }
            setResult(resultFPR);
        }
        return;
    }

    DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());

    JSValueOperand argument(this, node->child1());
    JSValueRegs argumentRegs = argument.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    J_JITOperation_GJ operation = nullptr;
    if (node->op() == ArithRound)
        operation = operationArithRound;
    else if (node->op() == ArithFloor)
        operation = operationArithFloor;
    else if (node->op() == ArithCeil)
        operation = operationArithCeil;
    else {
        ASSERT(node->op() == ArithTrunc);
        operation = operationArithTrunc;
    }
    callOperation(operation, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileArithUnary(Node* node)
{
    compileArithDoubleUnaryOp(node, arithUnaryFunction(node->arithUnaryType()), arithUnaryOperation(node->arithUnaryType()));
}

void SpeculativeJIT::compileArithSqrt(Node* node)
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRReg op1FPR = op1.fpr();

        if (!supportsFloatingPointSqrt() || !Options::useArchitectureSpecificOptimizations()) {
            flushRegisters();
            FPRResult result(this);
            callOperationWithoutExceptionCheck(Math::sqrtDouble, result.fpr(), op1FPR);
            doubleResult(result.fpr(), node);
        } else {
            FPRTemporary result(this, op1);
            sqrtDouble(op1.fpr(), result.fpr());
            doubleResult(result.fpr(), node);
        }
        return;
    }

    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    flushRegisters();
    FPRResult result(this);
    callOperation(operationArithSqrt, result.fpr(), LinkableConstant::globalObject(*this, node), op1Regs);
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileArithMinMax(Node* node)
{
    switch (m_graph.child(node, 0).useKind()) {
    case Int32Use: {
        if (node->numChildren() == 2) {
            // Optimize this pattern since it is the most common one.
            SpeculateStrictInt32Operand op1(this, m_graph.child(node, 0));
            SpeculateStrictInt32Operand op2(this, m_graph.child(node, 1));
            GPRTemporary result(this, Reuse, op1);

            GPRReg op1GPR = op1.gpr();
            GPRReg op2GPR = op2.gpr();
            GPRReg resultGPR = result.gpr();

            move(op1GPR, resultGPR);

#if CPU(ARM64) || CPU(X86_64)
            moveConditionally32(node->op() == ArithMin ? GreaterThan : LessThan, resultGPR, op2GPR, op2GPR, resultGPR);
#else
            auto resultLess = branch32(node->op() == ArithMin ? LessThan : GreaterThan, resultGPR, op2GPR);
            move(op2GPR, resultGPR);
            resultLess.link(this);
#endif

            strictInt32Result(resultGPR, node);
            break;
        }

        SpeculateStrictInt32Operand op1(this, m_graph.child(node, 0));
        GPRTemporary result(this); // Do not use Reuse because Int32Use speculation can fail in the following loop.

        GPRReg op1GPR = op1.gpr();
        GPRReg resultGPR = result.gpr();

        move(op1GPR, resultGPR);

        for (unsigned index = 1; index < node->numChildren(); ++index) {
            SpeculateStrictInt32Operand op2(this, m_graph.child(node, index));
            GPRReg op2GPR = op2.gpr();
#if CPU(ARM64) || CPU(X86_64)
            moveConditionally32(node->op() == ArithMin ? GreaterThan : LessThan, resultGPR, op2GPR, op2GPR, resultGPR);
#else
            auto resultLess = branch32(node->op() == ArithMin ? LessThan : GreaterThan, resultGPR, op2GPR);
            move(op2GPR, resultGPR);
            resultLess.link(this);
#endif
        }

        strictInt32Result(resultGPR, node);
        break;
    }

    case DoubleRepUse: {
        if (node->numChildren() == 2) {
            SpeculateDoubleOperand op1(this, m_graph.child(node, 0));
            SpeculateDoubleOperand op2(this, m_graph.child(node, 1));
            FPRTemporary result(this, op1);

            FPRReg op1FPR = op1.fpr();
            FPRReg op2FPR = op2.fpr();
            FPRReg resultFPR = result.fpr();
#if CPU(ARM64)
            if (node->op() == ArithMin)
                doubleMin(op1FPR, op2FPR, resultFPR);
            else
                doubleMax(op1FPR, op2FPR, resultFPR);
            doubleResult(resultFPR, node);
#else
            JumpList done;

            Jump op1Less = branchDouble(node->op() == ArithMin ? DoubleLessThanAndOrdered : DoubleGreaterThanAndOrdered, op1FPR, op2FPR);
            Jump opNotEqualOrUnordered = branchDouble(DoubleNotEqualOrUnordered, op1FPR, op2FPR);

            // The spec for Math.min and Math.max states that +0 is considered to be larger than -0.
            if (node->op() == ArithMin)
                orDouble(op1FPR, op2FPR, resultFPR);
            else
                andDouble(op1FPR, op2FPR, resultFPR);

            done.append(jump());

            opNotEqualOrUnordered.link(this);
            // op2 is either the lesser one or one of then is NaN
            Jump op2Less = branchDouble(node->op() == ArithMin ? DoubleGreaterThanAndOrdered : DoubleLessThanAndOrdered, op1FPR, op2FPR);

            // Unordered case. We don't know which of op1, op2 is NaN. Manufacture NaN by adding
            // op1 + op2 and putting it into result.
            addDouble(op1FPR, op2FPR, resultFPR);
            done.append(jump());

            op2Less.link(this);
            moveDouble(op2FPR, resultFPR);

            if (op1FPR != resultFPR) {
                done.append(jump());

                op1Less.link(this);
                moveDouble(op1FPR, resultFPR);
            } else
                op1Less.link(this);

            done.link(this);

            doubleResult(resultFPR, node);
#endif
            break;
        }

#if CPU(ARM64)
        SpeculateDoubleOperand op1(this, m_graph.child(node, 0));
        FPRTemporary result(this);

        FPRReg op1FPR = op1.fpr();
        FPRReg resultFPR = result.fpr();

        moveDouble(op1FPR, resultFPR);

        for (unsigned index = 1; index < node->numChildren(); ++index) {
            SpeculateDoubleOperand op2(this, m_graph.child(node, index));
            FPRReg op2FPR = op2.fpr();
            if (node->op() == ArithMin)
                doubleMin(op2FPR, resultFPR, resultFPR);
            else
                doubleMax(op2FPR, resultFPR, resultFPR);
        }

        doubleResult(resultFPR, node);
#else
        GPRTemporary buffer(this);

        GPRReg bufferGPR = buffer.gpr();

        size_t scratchSize = sizeof(double) * node->numChildren();
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        move(TrustedImmPtr(bitwise_cast<const double*>(scratchBuffer->dataBuffer())), bufferGPR);

        for (unsigned index = 0; index < node->numChildren(); ++index) {
            SpeculateDoubleOperand op(this, m_graph.child(node, index));
            FPRReg opFPR = op.fpr();
            storeDouble(opFPR, Address(bufferGPR, sizeof(double) * index));
        }

        flushRegisters();
        FPRResult result(this);
        FPRReg resultFPR = result.fpr();
        callOperationWithoutExceptionCheck(node->op() == ArithMin ? operationArithMinMultipleDouble : operationArithMaxMultipleDouble, resultFPR, bufferGPR, TrustedImm32(node->numChildren()));
        doubleResult(resultFPR, node);
#endif
        break;
    }

    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

// For small positive integers , it is worth doing a tiny inline loop to exponentiate the base.
// Every register is clobbered by this helper.
static MacroAssembler::Jump compileArithPowIntegerFastPath(JITCompiler& assembler, FPRReg xOperand, GPRReg yOperand, FPRReg result)
{
    MacroAssembler::JumpList skipFastPath;
    skipFastPath.append(assembler.branch32(MacroAssembler::Above, yOperand, MacroAssembler::TrustedImm32(maxExponentForIntegerMathPow)));

    assembler.move64ToDouble(CCallHelpers::TrustedImm64(bitwise_cast<uint64_t>(1.0)), result);

    MacroAssembler::Label startLoop(assembler.label());
    MacroAssembler::Jump exponentIsEven = assembler.branchTest32(MacroAssembler::Zero, yOperand, MacroAssembler::TrustedImm32(1));
    assembler.mulDouble(xOperand, result);
    exponentIsEven.link(&assembler);
    assembler.mulDouble(xOperand, xOperand);
    assembler.rshift32(MacroAssembler::TrustedImm32(1), yOperand);
    assembler.branchTest32(MacroAssembler::NonZero, yOperand).linkTo(startLoop, &assembler);

    MacroAssembler::Jump skipSlowPath = assembler.jump();
    skipFastPath.link(&assembler);

    return skipSlowPath;
}

void SpeculativeJIT::compileValuePow(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    // FIXME: do we want a fast path for BigInt32 for Pow? I expect it would overflow pretty often.
    if (node->binaryUseKind() == HeapBigIntUse) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateHeapBigInt(leftChild, leftGPR);
        speculateHeapBigInt(rightChild, rightGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        callOperation(operationPowHeapBigInt, resultRegs, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);

        jsValueResult(resultRegs, node);
        return;
    }

    DFG_ASSERT(m_graph, node, node->binaryUseKind() == UntypedUse || node->binaryUseKind() == AnyBigIntUse || node->binaryUseKind() == BigInt32Use, node->binaryUseKind());

    JSValueOperand left(this, leftChild, ManualOperandSpeculation);
    JSValueOperand right(this, rightChild, ManualOperandSpeculation);
    speculate(node, leftChild);
    speculate(node, rightChild);
    JSValueRegs leftRegs = left.jsValueRegs();
    JSValueRegs rightRegs = right.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationValuePow, resultRegs, LinkableConstant::globalObject(*this, node), leftRegs, rightRegs);

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileArithPow(Node* node)
{
    if (node->child2().useKind() == Int32Use) {
        SpeculateDoubleOperand xOperand(this, node->child1());
        SpeculateInt32Operand yOperand(this, node->child2());
        FPRReg xOperandfpr = xOperand.fpr();
        GPRReg yOperandGpr = yOperand.gpr();
        FPRTemporary yOperandfpr(this);

        flushRegisters();

        FPRResult result(this);
        FPRReg resultFpr = result.fpr();

        FPRTemporary xOperandCopy(this);
        FPRReg xOperandCopyFpr = xOperandCopy.fpr();
        moveDouble(xOperandfpr, xOperandCopyFpr);

        GPRTemporary counter(this);
        GPRReg counterGpr = counter.gpr();
        move(yOperandGpr, counterGpr);

        Jump skipFallback = compileArithPowIntegerFastPath(*this, xOperandCopyFpr, counterGpr, resultFpr);
        convertInt32ToDouble(yOperandGpr, yOperandfpr.fpr());
        callOperationWithoutExceptionCheck(operationMathPow, resultFpr, xOperandfpr, yOperandfpr.fpr());

        skipFallback.link(this);
        doubleResult(resultFpr, node);
        return;
    }

    if (node->child2()->isDoubleConstant()) {
        double exponent = node->child2()->asNumber();
        if (exponent == 0.5) {
            SpeculateDoubleOperand xOperand(this, node->child1());
            FPRTemporary result(this);
            FPRReg xOperandFpr = xOperand.fpr();
            FPRReg resultFpr = result.fpr();

            moveZeroToDouble(resultFpr);
            Jump xIsZeroOrNegativeZero = branchDouble(DoubleEqualAndOrdered, xOperandFpr, resultFpr);

            move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(-std::numeric_limits<double>::infinity())), resultFpr);
            Jump xIsMinusInfinity = branchDouble(DoubleEqualAndOrdered, xOperandFpr, resultFpr);
            sqrtDouble(xOperandFpr, resultFpr);
            Jump doneWithSqrt = jump();

            xIsMinusInfinity.link(this);
            if (isX86())
                move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(std::numeric_limits<double>::infinity())), resultFpr);
            else
                absDouble(resultFpr, resultFpr);

            xIsZeroOrNegativeZero.link(this);
            doneWithSqrt.link(this);
            doubleResult(resultFpr, node);
            return;
        }
        if (exponent == -0.5) {
            SpeculateDoubleOperand xOperand(this, node->child1());
            FPRTemporary scratch(this);
            FPRTemporary result(this);
            FPRReg xOperandFpr = xOperand.fpr();
            FPRReg scratchFPR = scratch.fpr();
            FPRReg resultFpr = result.fpr();

            moveZeroToDouble(resultFpr);
            Jump xIsZeroOrNegativeZero = branchDouble(DoubleEqualAndOrdered, xOperandFpr, resultFpr);

            move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(-std::numeric_limits<double>::infinity())), resultFpr);
            Jump xIsMinusInfinity = branchDouble(DoubleEqualAndOrdered, xOperandFpr, resultFpr);

            move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(1.0)), resultFpr);
            sqrtDouble(xOperandFpr, scratchFPR);
            divDouble(resultFpr, scratchFPR, resultFpr);
            Jump doneWithSqrt = jump();

            xIsZeroOrNegativeZero.link(this);
            move64ToDouble(TrustedImm64(bitwise_cast<uint64_t>(std::numeric_limits<double>::infinity())), resultFpr);
            Jump doneWithBaseZero = jump();

            xIsMinusInfinity.link(this);
            moveZeroToDouble(resultFpr);

            doneWithBaseZero.link(this);
            doneWithSqrt.link(this);
            doubleResult(resultFpr, node);
            return;
        }
    }

    SpeculateDoubleOperand xOperand(this, node->child1());
    SpeculateDoubleOperand yOperand(this, node->child2());
    FPRReg xOperandfpr = xOperand.fpr();
    FPRReg yOperandfpr = yOperand.fpr();

    flushRegisters();

    FPRResult result(this);
    FPRReg resultFpr = result.fpr();

    FPRTemporary xOperandCopy(this);
    FPRReg xOperandCopyFpr = xOperandCopy.fpr();

    FPRTemporary scratch(this);
    FPRReg scratchFpr = scratch.fpr();

    GPRTemporary yOperandInteger(this);
    GPRReg yOperandIntegerGpr = yOperandInteger.gpr();
    JumpList failedExponentConversionToInteger;
    branchConvertDoubleToInt32(yOperandfpr, yOperandIntegerGpr, failedExponentConversionToInteger, scratchFpr, false);

    moveDouble(xOperandfpr, xOperandCopyFpr);
    Jump skipFallback = compileArithPowIntegerFastPath(*this, xOperandCopyFpr, yOperandInteger.gpr(), resultFpr);
    failedExponentConversionToInteger.link(this);

    callOperationWithoutExceptionCheck(operationMathPow, resultFpr, xOperandfpr, yOperandfpr);
    skipFallback.link(this);
    doubleResult(resultFpr, node);
}

// Returns true if the compare is fused with a subsequent branch.
bool SpeculativeJIT::compare(Node* node, RelationalCondition condition, DoubleCondition doubleCondition, S_JITOperation_GJJ operation)
{
    if (compilePeepHoleBranch(node, condition, doubleCondition, operation))
        return true;

    if (node->isBinaryUseKind(Int32Use)) {
        compileInt32Compare(node, condition);
        return false;
    }
    
#if USE(BIGINT32)
    if (node->isBinaryUseKind(BigInt32Use)) {
        compileBigInt32Compare(node, condition);
        return false;
    }
#endif

#if USE(JSVALUE64)
    if (node->isBinaryUseKind(Int52RepUse)) {
        compileInt52Compare(node, condition);
        return false;
    }
#endif // USE(JSVALUE64)
    
    if (node->isBinaryUseKind(DoubleRepUse)) {
        compileDoubleCompare(node, doubleCondition);
        return false;
    }

    if (node->isBinaryUseKind(StringUse)) {
        if (node->op() == CompareEq)
            compileStringEquality(node);
        else
            compileStringCompare(node, condition);
        return false;
    }

    if (node->isBinaryUseKind(StringIdentUse)) {
        if (node->op() == CompareEq)
            compileStringIdentEquality(node);
        else
            compileStringIdentCompare(node, condition);
        return false;
    }

    // FIXME: add HeapBigInt case here.
    // Not having it means that the compare will not be fused with the branch for this case.

    if (node->op() == CompareEq) {
        if (node->isBinaryUseKind(BooleanUse)) {
            compileBooleanCompare(node, condition);
            return false;
        }

        if (node->isBinaryUseKind(SymbolUse)) {
            compileSymbolEquality(node);
            return false;
        }
        
        if (node->isBinaryUseKind(ObjectUse)) {
            compileObjectEquality(node);
            return false;
        }
        
        if (node->isBinaryUseKind(ObjectUse, ObjectOrOtherUse)) {
            compileObjectToObjectOrOtherEquality(node->child1(), node->child2());
            return false;
        }
        
        if (node->isBinaryUseKind(ObjectOrOtherUse, ObjectUse)) {
            compileObjectToObjectOrOtherEquality(node->child2(), node->child1());
            return false;
        }

        if (!needsTypeCheck(node->child1(), SpecOther)) {
            nonSpeculativeNonPeepholeCompareNullOrUndefined(node->child2());
            return false;
        }

        if (!needsTypeCheck(node->child2(), SpecOther)) {
            nonSpeculativeNonPeepholeCompareNullOrUndefined(node->child1());
            return false;
        }
    }

    genericJSValueNonPeepholeCompare(node, condition, operation);
    return false;
}

void SpeculativeJIT::compileCompareUnsigned(Node* node, RelationalCondition condition)
{
    compileInt32Compare(node, condition);
}

bool SpeculativeJIT::compileStrictEq(Node* node)
{
    if (node->isBinaryUseKind(BooleanUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleBooleanBranch(node, branchNode, Equal);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileBooleanCompare(node, Equal);
        return false;
    }

    if (node->isBinaryUseKind(Int32Use)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleInt32Branch(node, branchNode, Equal);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileInt32Compare(node, Equal);
        return false;
    }
    
#if USE(BIGINT32)
    if (node->isBinaryUseKind(BigInt32Use)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleBigInt32Branch(node, branchNode, Equal);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileBigInt32Compare(node, Equal);
        return false;
    }
#endif

#if USE(JSVALUE64)
    if (node->isBinaryUseKind(Int52RepUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleInt52Branch(node, branchNode, Equal);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileInt52Compare(node, Equal);
        return false;
    }
#endif // USE(JSVALUE64)

    if (node->isBinaryUseKind(DoubleRepUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleDoubleBranch(node, branchNode, DoubleEqualAndOrdered);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileDoubleCompare(node, DoubleEqualAndOrdered);
        return false;
    }

    if (node->isBinaryUseKind(SymbolUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleSymbolEquality(node, branchNode);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileSymbolEquality(node);
        return false;
    }

#if !USE(BIGINT32)
    if (node->isBinaryUseKind(NotDoubleUse, NeitherDoubleNorHeapBigIntNorStringUse)) {
        Edge notDoubleChild = node->child1();
        Edge neitherDoubleNorHeapBigIntNorStringChild = node->child2();
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleNotDoubleNeitherDoubleNorHeapBigIntNorStringStrictEquality(node, branchNode, notDoubleChild, neitherDoubleNorHeapBigIntNorStringChild);
            use(notDoubleChild);
            use(neitherDoubleNorHeapBigIntNorStringChild);
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileNotDoubleNeitherDoubleNorHeapBigIntNorStringStrictEquality(node, notDoubleChild, neitherDoubleNorHeapBigIntNorStringChild);
        return false;
    }
    if (node->isBinaryUseKind(NeitherDoubleNorHeapBigIntNorStringUse, NotDoubleUse)) {
        Edge neitherDoubleNorHeapBigIntNorStringChild = node->child1();
        Edge notDoubleChild = node->child2();
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleNotDoubleNeitherDoubleNorHeapBigIntNorStringStrictEquality(node, branchNode, notDoubleChild, neitherDoubleNorHeapBigIntNorStringChild);
            use(notDoubleChild);
            use(neitherDoubleNorHeapBigIntNorStringChild);
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileNotDoubleNeitherDoubleNorHeapBigIntNorStringStrictEquality(node, notDoubleChild, neitherDoubleNorHeapBigIntNorStringChild);
        return false;
    }
#if USE(JSVALUE64)
    if (node->isBinaryUseKind(NeitherDoubleNorHeapBigIntUse, NotDoubleUse)) {
        Edge neitherDoubleNorHeapBigIntChild = node->child1();
        Edge notDoubleChild = node->child2();
        compileNeitherDoubleNorHeapBigIntToNotDoubleStrictEquality(node, neitherDoubleNorHeapBigIntChild, notDoubleChild);
        return false;
    }
    if (node->isBinaryUseKind(NotDoubleUse, NeitherDoubleNorHeapBigIntUse)) {
        Edge notDoubleChild = node->child1();
        Edge neitherDoubleNorHeapBigIntChild = node->child2();
        compileNeitherDoubleNorHeapBigIntToNotDoubleStrictEquality(node, neitherDoubleNorHeapBigIntChild, notDoubleChild);
        return false;
    }
#endif // USE(JSVALUE64)
#endif // !USE(BIGINT32)

    if (node->isBinaryUseKind(HeapBigIntUse)) {
        compileHeapBigIntEquality(node);
        return false;
    }
    
    if (node->isBinaryUseKind(SymbolUse, UntypedUse)) {
        compileSymbolUntypedEquality(node, node->child1(), node->child2());
        return false;
    }
    
    if (node->isBinaryUseKind(UntypedUse, SymbolUse)) {
        compileSymbolUntypedEquality(node, node->child2(), node->child1());
        return false;
    }
    
    if (node->isBinaryUseKind(StringUse)) {
        compileStringEquality(node);
        return false;
    }
    
    if (node->isBinaryUseKind(StringIdentUse)) {
        compileStringIdentEquality(node);
        return false;
    }

    if (node->isBinaryUseKind(ObjectUse, UntypedUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleObjectStrictEquality(node->child1(), node->child2(), branchNode);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileObjectStrictEquality(node->child1(), node->child2());
        return false;
    }
    
    if (node->isBinaryUseKind(UntypedUse, ObjectUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleObjectStrictEquality(node->child2(), node->child1(), branchNode);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileObjectStrictEquality(node->child2(), node->child1());
        return false;
    }

    if (node->isBinaryUseKind(ObjectUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleObjectEquality(node, branchNode);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileObjectEquality(node);
        return false;
    }

    if (node->isSymmetricBinaryUseKind(BooleanUse, UntypedUse)
        || node->isBinaryUseKind(OtherUse)
        || node->isSymmetricBinaryUseKind(OtherUse, UntypedUse)
        || node->isBinaryUseKind(MiscUse)
        || node->isSymmetricBinaryUseKind(MiscUse, UntypedUse)) {
        compileBitwiseStrictEq(node);
        return false;
    }

    if (node->isBinaryUseKind(StringIdentUse, NotStringVarUse)) {
        compileStringIdentToNotStringVarEquality(node, node->child1(), node->child2());
        return false;
    }
    
    if (node->isBinaryUseKind(NotStringVarUse, StringIdentUse)) {
        compileStringIdentToNotStringVarEquality(node, node->child2(), node->child1());
        return false;
    }
    
    if (node->isBinaryUseKind(StringUse, UntypedUse)) {
        compileStringToUntypedEquality(node, node->child1(), node->child2());
        return false;
    }
    
    if (node->isBinaryUseKind(UntypedUse, StringUse)) {
        compileStringToUntypedEquality(node, node->child2(), node->child1());
        return false;
    }

    ASSERT(node->isBinaryUseKind(UntypedUse) || node->isBinaryUseKind(AnyBigIntUse));
    return genericJSValueStrictEq(node);
}

void SpeculativeJIT::compileBooleanCompare(Node* node, RelationalCondition condition)
{
    SpeculateBooleanOperand op1(this, node->child1());
    SpeculateBooleanOperand op2(this, node->child2());
    GPRTemporary result(this);
    
    compare32(condition, op1.gpr(), op2.gpr(), result.gpr());
    
    unblessedBooleanResult(result.gpr(), node);
}

void SpeculativeJIT::compileInt32Compare(Node* node, RelationalCondition condition)
{
    if (node->child1()->isInt32Constant()) {
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this, Reuse, op2);
        int32_t imm = node->child1()->asInt32();
        compare32(condition, Imm32(imm), op2.gpr(), result.gpr());

        unblessedBooleanResult(result.gpr(), node);
    } else if (node->child2()->isInt32Constant()) {
        SpeculateInt32Operand op1(this, node->child1());
        GPRTemporary result(this, Reuse, op1);
        int32_t imm = node->child2()->asInt32();
        compare32(condition, op1.gpr(), Imm32(imm), result.gpr());

        unblessedBooleanResult(result.gpr(), node);
    } else {
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this, Reuse, op1, op2);
        compare32(condition, op1.gpr(), op2.gpr(), result.gpr());

        unblessedBooleanResult(result.gpr(), node);
    }
}

void SpeculativeJIT::compileDoubleCompare(Node* node, DoubleCondition condition)
{
    SpeculateDoubleOperand op1(this, node->child1());
    SpeculateDoubleOperand op2(this, node->child2());
    GPRTemporary result(this);

    FPRReg op1FPR = op1.fpr();
    FPRReg op2FPR = op2.fpr();
    GPRReg resultGPR = result.gpr();

    compareDouble(condition, op1FPR, op2FPR, resultGPR);

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileObjectEquality(Node* node)
{
    SpeculateCellOperand op1(this, node->child1());
    SpeculateCellOperand op2(this, node->child2());
    GPRTemporary result(this, Reuse, op1);

    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultGPR = result.gpr();

    if (masqueradesAsUndefinedWatchpointSetIsStillValid()) {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), node->child1(), SpecObject, branchIfNotObject(op1GPR));
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op2GPR), node->child2(), SpecObject, branchIfNotObject(op2GPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), node->child1(), SpecObject, branchIfNotObject(op1GPR));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node->child1(),
            branchTest8(
                NonZero,
                Address(op1GPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));

        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op2GPR), node->child2(), SpecObject, branchIfNotObject(op2GPR));
        speculationCheck(BadType, JSValueSource::unboxedCell(op2GPR), node->child2(),
            branchTest8(
                NonZero,
                Address(op2GPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined)));
    }

    comparePtr(Equal, op1GPR, op2GPR, resultGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileSymbolEquality(Node* node)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRTemporary result(this, Reuse, left, right);

    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg resultGPR = result.gpr();

    speculateSymbol(node->child1(), leftGPR);
    speculateSymbol(node->child2(), rightGPR);

    comparePtr(Equal, leftGPR, rightGPR, resultGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compilePeepHoleSymbolEquality(Node* node, Node* branchNode)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());

    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();

    speculateSymbol(node->child1(), leftGPR);
    speculateSymbol(node->child2(), rightGPR);

    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    if (taken == nextBlock()) {
        branchPtr(NotEqual, leftGPR, rightGPR, notTaken);
        jump(taken);
    } else {
        branchPtr(Equal, leftGPR, rightGPR, taken);
        jump(notTaken);
    }
}

void SpeculativeJIT::emitBitwiseJSValueEquality(JSValueRegs& left, JSValueRegs& right, GPRReg& result)
{
#if USE(JSVALUE64)
    compare64(Equal, left.gpr(), right.gpr(), result);
#else
    move(TrustedImm32(0), result);
    Jump notEqual = branch32(NotEqual, left.tagGPR(), right.tagGPR());
    compare32(Equal, left.payloadGPR(), right.payloadGPR(), result);
    notEqual.link(this);
#endif
}

void SpeculativeJIT::emitBranchOnBitwiseJSValueEquality(JSValueRegs& left, JSValueRegs& right, BasicBlock* taken, BasicBlock* notTaken)
{
#if USE(JSVALUE64)
    if (taken == nextBlock()) {
        branch64(NotEqual, left.gpr(), right.gpr(), notTaken);
        jump(taken);
    } else {
        branch64(Equal, left.gpr(), right.gpr(), taken);
        jump(notTaken);
    }
#else
    branch32(NotEqual, left.tagGPR(), right.tagGPR(), notTaken);
    if (taken == nextBlock()) {
        branch32(NotEqual, left.payloadGPR(), right.payloadGPR(), notTaken);
        jump(taken);
    } else {
        branch32(Equal, left.payloadGPR(), right.payloadGPR(), taken);
        jump(notTaken);
    }
#endif
}

void SpeculativeJIT::compileNotDoubleNeitherDoubleNorHeapBigIntNorStringStrictEquality(Node* node, Edge notDoubleChild, Edge neitherDoubleNorHeapBigIntNorStringChild)
{
    JSValueOperand left(this, notDoubleChild, ManualOperandSpeculation);
    JSValueOperand right(this, neitherDoubleNorHeapBigIntNorStringChild, ManualOperandSpeculation);

    GPRTemporary temp(this);
#if USE(JSVALUE64)
    GPRTemporary result(this, Reuse, left, right);
#else
    GPRTemporary result(this);
#endif
    JSValueRegs leftRegs = left.jsValueRegs();
    JSValueRegs rightRegs = right.jsValueRegs();
    GPRReg tempGPR = temp.gpr();
    GPRReg resultGPR = result.gpr();

    speculateNotDouble(notDoubleChild, leftRegs, tempGPR);
    speculateNeitherDoubleNorHeapBigIntNorString(neitherDoubleNorHeapBigIntNorStringChild, rightRegs, tempGPR);

    emitBitwiseJSValueEquality(leftRegs, rightRegs, resultGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compilePeepHoleNotDoubleNeitherDoubleNorHeapBigIntNorStringStrictEquality(Node*, Node* branchNode, Edge notDoubleChild, Edge neitherDoubleNorHeapBigIntNorStringChild)
{
    JSValueOperand left(this, notDoubleChild, ManualOperandSpeculation);
    JSValueOperand right(this, neitherDoubleNorHeapBigIntNorStringChild, ManualOperandSpeculation);

    GPRTemporary temp(this);
    JSValueRegs leftRegs = left.jsValueRegs();
    JSValueRegs rightRegs = right.jsValueRegs();
    GPRReg tempGPR = temp.gpr();

    speculateNotDouble(notDoubleChild, leftRegs, tempGPR);
    speculateNeitherDoubleNorHeapBigIntNorString(neitherDoubleNorHeapBigIntNorStringChild, rightRegs, tempGPR);

    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    emitBranchOnBitwiseJSValueEquality(leftRegs, rightRegs, taken, notTaken);
}

void SpeculativeJIT::compileStringEquality(
    Node* node, GPRReg leftGPR, GPRReg rightGPR, GPRReg lengthGPR, GPRReg leftTempGPR,
    GPRReg rightTempGPR, GPRReg leftTemp2GPR, GPRReg rightTemp2GPR,
    const JumpList& fastTrue, const JumpList& fastFalse)
{
    JumpList trueCase;
    JumpList falseCase;
    JumpList slowCase;
    
    trueCase.append(fastTrue);
    falseCase.append(fastFalse);

    loadPtr(Address(leftGPR, JSString::offsetOfValue()), leftTempGPR);
    loadPtr(Address(rightGPR, JSString::offsetOfValue()), rightTempGPR);

    slowCase.append(branchIfRopeStringImpl(leftTempGPR));
    slowCase.append(branchIfRopeStringImpl(rightTempGPR));

    load32(Address(leftTempGPR, StringImpl::lengthMemoryOffset()), lengthGPR);
    
    falseCase.append(branch32(
        NotEqual,
        Address(rightTempGPR, StringImpl::lengthMemoryOffset()),
        lengthGPR));
    
    trueCase.append(branchTest32(Zero, lengthGPR));
    
    slowCase.append(branchTest32(
        Zero,
        Address(leftTempGPR, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit())));
    slowCase.append(branchTest32(
        Zero,
        Address(rightTempGPR, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit())));
    
    loadPtr(Address(leftTempGPR, StringImpl::dataOffset()), leftTempGPR);
    loadPtr(Address(rightTempGPR, StringImpl::dataOffset()), rightTempGPR);
    
    Label loop = label();
    
    sub32(TrustedImm32(1), lengthGPR);

    // This isn't going to generate the best code on x86. But that's OK, it's still better
    // than not inlining.
    load8(BaseIndex(leftTempGPR, lengthGPR, TimesOne), leftTemp2GPR);
    load8(BaseIndex(rightTempGPR, lengthGPR, TimesOne), rightTemp2GPR);
    falseCase.append(branch32(NotEqual, leftTemp2GPR, rightTemp2GPR));
    
    branchTest32(NonZero, lengthGPR).linkTo(loop, this);
    
    trueCase.link(this);
    moveTrueTo(leftTempGPR);
    
    Jump done = jump();

    falseCase.link(this);
    moveFalseTo(leftTempGPR);
    
    done.link(this);
    addSlowPathGenerator(
        slowPathCall(
            slowCase, this, operationCompareStringEq, leftTempGPR, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR));
    
    blessedBooleanResult(leftTempGPR, node);
}

void SpeculativeJIT::compileStringEquality(Node* node)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRTemporary length(this);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    GPRTemporary leftTemp2(this, Reuse, left);
    GPRTemporary rightTemp2(this, Reuse, right);
    
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg lengthGPR = length.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();
    GPRReg leftTemp2GPR = leftTemp2.gpr();
    GPRReg rightTemp2GPR = rightTemp2.gpr();
    
    speculateString(node->child1(), leftGPR);
    
    // It's safe to branch around the type check below, since proving that the values are
    // equal does indeed prove that the right value is a string.
    Jump fastTrue = branchPtr(Equal, leftGPR, rightGPR);
    
    speculateString(node->child2(), rightGPR);
    
    compileStringEquality(
        node, leftGPR, rightGPR, lengthGPR, leftTempGPR, rightTempGPR, leftTemp2GPR,
        rightTemp2GPR, fastTrue, Jump());
}

void SpeculativeJIT::compileStringToUntypedEquality(Node* node, Edge stringEdge, Edge untypedEdge)
{
    SpeculateCellOperand left(this, stringEdge);
    JSValueOperand right(this, untypedEdge, ManualOperandSpeculation);
    GPRTemporary length(this);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    GPRTemporary leftTemp2(this, Reuse, left);
    GPRTemporary rightTemp2(this);
    
    GPRReg leftGPR = left.gpr();
    JSValueRegs rightRegs = right.jsValueRegs();
    GPRReg lengthGPR = length.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();
    GPRReg leftTemp2GPR = leftTemp2.gpr();
    GPRReg rightTemp2GPR = rightTemp2.gpr();
    
    speculateString(stringEdge, leftGPR);
    
    JumpList fastTrue;
    JumpList fastFalse;
    
    fastFalse.append(branchIfNotCell(rightRegs));
    
    // It's safe to branch around the type check below, since proving that the values are
    // equal does indeed prove that the right value is a string.
    fastTrue.append(branchPtr(
        Equal, leftGPR, rightRegs.payloadGPR()));
    
    fastFalse.append(branchIfNotString(rightRegs.payloadGPR()));
    
    compileStringEquality(
        node, leftGPR, rightRegs.payloadGPR(), lengthGPR, leftTempGPR, rightTempGPR, leftTemp2GPR,
        rightTemp2GPR, fastTrue, fastFalse);
}

void SpeculativeJIT::compileStringIdentEquality(Node* node)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();

    speculateString(node->child1(), leftGPR);
    speculateString(node->child2(), rightGPR);
    
    speculateStringIdentAndLoadStorage(node->child1(), leftGPR, leftTempGPR);
    speculateStringIdentAndLoadStorage(node->child2(), rightGPR, rightTempGPR);
    
    comparePtr(Equal, leftTempGPR, rightTempGPR, leftTempGPR);
    
    unblessedBooleanResult(leftTempGPR, node);
}

void SpeculativeJIT::compileStringIdentToNotStringVarEquality(
    Node* node, Edge stringEdge, Edge notStringVarEdge)
{
    SpeculateCellOperand left(this, stringEdge);
    JSValueOperand right(this, notStringVarEdge, ManualOperandSpeculation);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();
    GPRReg leftGPR = left.gpr();
    JSValueRegs rightRegs = right.jsValueRegs();
    
    speculateString(stringEdge, leftGPR);
    speculateStringIdentAndLoadStorage(stringEdge, leftGPR, leftTempGPR);

    moveFalseTo(rightTempGPR);
    JumpList notString;
    notString.append(branchIfNotCell(rightRegs));
    notString.append(branchIfNotString(rightRegs.payloadGPR()));
    
    speculateStringIdentAndLoadStorage(notStringVarEdge, rightRegs.payloadGPR(), rightTempGPR);
    
    comparePtr(Equal, leftTempGPR, rightTempGPR, rightTempGPR);
    notString.link(this);
    
    unblessedBooleanResult(rightTempGPR, node);
}

void SpeculativeJIT::compileStringCompare(Node* node, RelationalCondition condition)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();

    speculateString(node->child1(), leftGPR);
    speculateString(node->child2(), rightGPR);

    C_JITOperation_B_GJssJss compareFunction = nullptr;
    if (condition == LessThan)
        compareFunction = operationCompareStringLess;
    else if (condition == LessThanOrEqual)
        compareFunction = operationCompareStringLessEq;
    else if (condition == GreaterThan)
        compareFunction = operationCompareStringGreater;
    else if (condition == GreaterThanOrEqual)
        compareFunction = operationCompareStringGreaterEq;
    else
        RELEASE_ASSERT_NOT_REACHED();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    flushRegisters();
    callOperation(compareFunction, resultGPR, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileStringIdentCompare(Node* node, RelationalCondition condition)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRFlushedCallResult result(this);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);

    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();

    speculateString(node->child1(), leftGPR);
    speculateString(node->child2(), rightGPR);

    C_JITOperation_TT compareFunction = nullptr;
    if (condition == LessThan)
        compareFunction = operationCompareStringImplLess;
    else if (condition == LessThanOrEqual)
        compareFunction = operationCompareStringImplLessEq;
    else if (condition == GreaterThan)
        compareFunction = operationCompareStringImplGreater;
    else if (condition == GreaterThanOrEqual)
        compareFunction = operationCompareStringImplGreaterEq;
    else
        RELEASE_ASSERT_NOT_REACHED();

    speculateStringIdentAndLoadStorage(node->child1(), leftGPR, leftTempGPR);
    speculateStringIdentAndLoadStorage(node->child2(), rightGPR, rightTempGPR);

    flushRegisters();
    callOperationWithoutExceptionCheck(compareFunction, resultGPR, leftTempGPR, rightTempGPR);

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileSameValue(Node* node)
{
    if (node->isBinaryUseKind(DoubleRepUse)) {
        SpeculateDoubleOperand arg1(this, node->child1());
        SpeculateDoubleOperand arg2(this, node->child2());
        GPRTemporary result(this);
        GPRTemporary temp(this);
        GPRTemporary temp2(this);

        FPRReg arg1FPR = arg1.fpr();
        FPRReg arg2FPR = arg2.fpr();
        GPRReg resultGPR = result.gpr();
        GPRReg tempGPR = temp.gpr();
        GPRReg temp2GPR = temp2.gpr();

#if USE(JSVALUE64)
        moveDoubleTo64(arg1FPR, tempGPR);
        moveDoubleTo64(arg2FPR, temp2GPR);
        auto trueCase = branch64(Equal, tempGPR, temp2GPR);
#else
        GPRTemporary temp3(this);
        GPRReg temp3GPR = temp3.gpr();

        moveDoubleToInts(arg1FPR, tempGPR, temp2GPR);
        moveDoubleToInts(arg2FPR, temp3GPR, resultGPR);
        auto notEqual = branch32(NotEqual, tempGPR, temp3GPR);
        auto trueCase = branch32(Equal, temp2GPR, resultGPR);
        notEqual.link(this);
#endif

        compareDouble(DoubleNotEqualOrUnordered, arg1FPR, arg1FPR, tempGPR);
        compareDouble(DoubleNotEqualOrUnordered, arg2FPR, arg2FPR, temp2GPR);
        and32(tempGPR, temp2GPR, resultGPR);
        auto done = jump();

        trueCase.link(this);
        move(TrustedImm32(1), resultGPR);
        done.link(this);

        unblessedBooleanResult(resultGPR, node);
        return;
    }

    ASSERT(node->isBinaryUseKind(UntypedUse));

    JSValueOperand arg1(this, node->child1());
    JSValueOperand arg2(this, node->child2());
    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();

    arg1.use();
    arg2.use();

    flushRegisters();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationSameValue, resultGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs);

    unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileToBooleanString(Node* node, bool invert)
{
    SpeculateCellOperand str(this, node->child1());
    GPRReg strGPR = str.gpr();

    // Make sure that this is a string.
    speculateString(node->child1(), strGPR);

    GPRTemporary eq(this);
    GPRReg eqGPR = eq.gpr();

    loadLinkableConstant(LinkableConstant(*this, jsEmptyString(vm())), eqGPR);
    comparePtr(invert ? Equal : NotEqual, strGPR, eqGPR, eqGPR);
    unblessedBooleanResult(eqGPR, node);
}

void SpeculativeJIT::compileToBooleanStringOrOther(Node* node, bool invert)
{
    JSValueOperand value(this, node->child1(), ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg tempGPR = temp.gpr();

    Jump notCell = branchIfNotCell(valueRegs);
    GPRReg cellGPR = valueRegs.payloadGPR();
    DFG_TYPE_CHECK(
        valueRegs, node->child1(), (~SpecCellCheck) | SpecString, branchIfNotString(cellGPR));

    loadLinkableConstant(LinkableConstant(*this, jsEmptyString(vm())), tempGPR);
    comparePtr(invert ? Equal : NotEqual, cellGPR, tempGPR, tempGPR);
    auto done = jump();

    notCell.link(this);
    DFG_TYPE_CHECK(
        valueRegs, node->child1(), SpecCellCheck | SpecOther, branchIfNotOther(valueRegs, tempGPR));
    move(invert ? TrustedImm32(1) : TrustedImm32(0), tempGPR);

    done.link(this);
    unblessedBooleanResult(tempGPR, node);
}

void SpeculativeJIT::emitStringBranch(Edge nodeUse, BasicBlock* taken, BasicBlock* notTaken)
{
    SpeculateCellOperand str(this, nodeUse);

    GPRReg strGPR = str.gpr();

    speculateString(nodeUse, strGPR);

    branchLinkableConstant(Equal, strGPR, LinkableConstant(*this, jsEmptyString(vm())), notTaken);
    jump(taken);

    noResult(m_currentNode);
}

void SpeculativeJIT::emitStringOrOtherBranch(Edge nodeUse, BasicBlock* taken, BasicBlock* notTaken)
{
    JSValueOperand value(this, nodeUse, ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg tempGPR = temp.gpr();
    
    Jump notCell = branchIfNotCell(valueRegs);
    GPRReg cellGPR = valueRegs.payloadGPR();
    DFG_TYPE_CHECK(valueRegs, nodeUse, (~SpecCellCheck) | SpecString, branchIfNotString(cellGPR));

    branchLinkableConstant(Equal, cellGPR, LinkableConstant(*this, jsEmptyString(vm())), notTaken);
    jump(taken, ForceJump);

    notCell.link(this);
    DFG_TYPE_CHECK(
        valueRegs, nodeUse, SpecCellCheck | SpecOther, branchIfNotOther(valueRegs, tempGPR));
    jump(notTaken);
    noResult(m_currentNode);
}

void SpeculativeJIT::compileConstantStoragePointer(Node* node)
{
    GPRTemporary storage(this);
    GPRReg storageGPR = storage.gpr();
    loadLinkableConstant(LinkableConstant::nonCellPointer(*this, node->storagePointer()), storageGPR);
    storageResult(storageGPR, node);
}

void SpeculativeJIT::cageTypedArrayStorage(GPRReg baseReg, GPRReg storageReg)
{
    UNUSED_PARAM(baseReg);
    UNUSED_PARAM(storageReg);
#if GIGACAGE_ENABLED
    if (!Gigacage::shouldBeEnabled())
        return;
    
    if (!Gigacage::disablingPrimitiveGigacageIsForbidden()) {
        VM& vm = this->vm();
        if (!vm.primitiveGigacageEnabled().isStillValid())
            return;
        m_graph.watchpoints().addLazily(vm.primitiveGigacageEnabled());
    }
    
    cage(Gigacage::Primitive, storageReg);
#endif
}

void SpeculativeJIT::compileGetIndexedPropertyStorage(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRReg baseReg = base.gpr();
    
    GPRTemporary storage(this);
    GPRReg storageReg = storage.gpr();
    
    ASSERT(node->arrayMode().type() != Array::String);
    auto typedArrayType = node->arrayMode().typedArrayType();
    ASSERT_UNUSED(typedArrayType, isTypedView(typedArrayType));

    loadPtr(Address(baseReg, JSArrayBufferView::offsetOfVector()), storageReg);
    cageTypedArrayStorage(baseReg, storageReg);
    storageResult(storageReg, node);
}

void SpeculativeJIT::compileResolveRope(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary result(this);

    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();

    loadPtr(Address(baseGPR, JSString::offsetOfValue()), resultGPR);

    JumpList slowCases;
    slowCases.append(branchIfRopeStringImpl(resultGPR));
    move(baseGPR, resultGPR);

    addSlowPathGenerator(
        slowPathCall(
            slowCases,
            this, operationResolveRopeString, resultGPR, LinkableConstant::globalObject(*this, node), baseGPR));
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetTypedArrayByteOffset(Node* node)
{
#if USE(JSVALUE64)
    if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary scratch1(this);
        GPRTemporary scratch2(this);
        GPRTemporary result(this);

        GPRReg baseGPR = base.gpr();
        GPRReg scratch1GPR = scratch1.gpr();
        GPRReg scratch2GPR = scratch2.gpr();
        GPRReg resultGPR = result.gpr();

        auto outOfBounds = branchIfResizableOrGrowableSharedTypedArrayIsOutOfBounds(baseGPR, scratch1GPR, scratch2GPR, node->arrayMode().type() == Array::AnyTypedArray ? std::nullopt : std::optional { node->arrayMode().typedArrayType() });
#if USE(LARGE_TYPED_ARRAYS)
        load64(Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), resultGPR);
        // AI promises that the result of GetTypedArrayByteOffset will be Int32, so we must uphold that promise here.
        speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branch64(Above, resultGPR, TrustedImm32(std::numeric_limits<int32_t>::max())));
#else
        load32(Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), resultGPR);
#endif
        auto done = jump();

        outOfBounds.link(this);
        move(TrustedImm32(0), resultGPR);
        done.link(this);

        strictInt32Result(resultGPR, node);
        return;
    }
#endif

    SpeculateCellOperand base(this, node->child1());
    GPRTemporary result(this);

    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();


    if (!m_graph.isNeverResizableOrGrowableSharedTypedArrayIncludingDataView(m_state.forNode(node->child1())))
        speculationCheck(UnexpectedResizableArrayBufferView, JSValueSource::unboxedCell(baseGPR), node, branchTest8(NonZero, Address(baseGPR, JSArrayBufferView::offsetOfMode()), TrustedImm32(isResizableOrGrowableSharedMode)));

#if USE(LARGE_TYPED_ARRAYS)
    load64(Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), resultGPR);
    // AI promises that the result of GetTypedArrayByteOffset will be Int32, so we must uphold that promise here.
    speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branch64(Above, resultGPR, TrustedImm32(std::numeric_limits<int32_t>::max())));
#else
    load32(Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), resultGPR);
#endif

    strictInt32Result(resultGPR, node);
}

void SpeculativeJIT::compileGetByValOnDirectArguments(Node* node, const ScopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>& prefix)
{
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();

    JSValueRegs resultRegs;
    constexpr bool needsFlush = false;
    std::tie(resultRegs, std::ignore) = prefix(DataFormatJS, needsFlush);
    GPRReg scratchReg = resultRegs.payloadGPR();
    
    if (!m_compileOkay)
        return;
    
    speculationCheck(
        ExoticObjectMode, JSValueSource(), nullptr,
        branchTestPtr(
            NonZero,
            Address(baseReg, DirectArguments::offsetOfMappedArguments())));

    load32(Address(baseReg, DirectArguments::offsetOfLength()), scratchReg);
    auto isOutOfBounds = branch32(AboveOrEqual, propertyReg, scratchReg);
    if (node->arrayMode().isInBounds())
        speculationCheck(OutOfBounds, JSValueSource(), nullptr, isOutOfBounds);
    
    loadValue(
        BaseIndex(
            baseReg, propertyReg, TimesEight, DirectArguments::storageOffset()),
        resultRegs);
    
    if (!node->arrayMode().isInBounds()) {
        addSlowPathGenerator(
            slowPathCall(
                isOutOfBounds, this, operationGetByValObjectInt,
                extractResult(resultRegs), LinkableConstant::globalObject(*this, node), baseReg, propertyReg));
    }
    
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetByValOnScopedArguments(Node* node, const ScopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat preferredFormat, bool needsFlush)>& prefix)
{
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);
    
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg scratchReg = scratch.gpr();
    GPRReg scratch2Reg = scratch2.gpr();
    
    if (!m_compileOkay)
        return;

    JSValueRegs resultRegs;
    constexpr bool needsFlush = false;
    std::tie(resultRegs, std::ignore) = prefix(DataFormatJS, needsFlush);
    
    loadPtr(
        Address(baseReg, ScopedArguments::offsetOfStorage()), resultRegs.payloadGPR());

    speculationCheck(
        ExoticObjectMode, JSValueSource(), nullptr,
        branch32(
            AboveOrEqual, propertyReg,
            Address(baseReg, ScopedArguments::offsetOfTotalLength())));
    
    loadPtr(Address(baseReg, ScopedArguments::offsetOfTable()), scratchReg);
    load32(
        Address(scratchReg, ScopedArgumentsTable::offsetOfLength()), scratch2Reg);
    
    Jump overflowArgument = branch32(
        AboveOrEqual, propertyReg, scratch2Reg);
    
    loadPtr(Address(baseReg, ScopedArguments::offsetOfScope()), scratch2Reg);

    loadPtr(
        Address(scratchReg, ScopedArgumentsTable::offsetOfArguments()),
        scratchReg);
    load32(
        BaseIndex(scratchReg, propertyReg, TimesFour),
        scratchReg);
    
    speculationCheck(
        ExoticObjectMode, JSValueSource(), nullptr,
        branch32(
            Equal, scratchReg, TrustedImm32(ScopeOffset::invalidOffset)));
    
    loadValue(
        BaseIndex(
            scratch2Reg, propertyReg, TimesEight,
            JSLexicalEnvironment::offsetOfVariables()),
        resultRegs);
    
    Jump done = jump();
    overflowArgument.link(this);
    
    sub32(propertyReg, scratch2Reg);
    neg32(scratch2Reg);
    
    loadValue(
        BaseIndex(
            resultRegs.payloadGPR(), scratch2Reg, TimesEight),
        resultRegs);
    speculationCheck(ExoticObjectMode, JSValueSource(), nullptr, branchIfEmpty(resultRegs));
    
    done.link(this);
    
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetScope(Node* node)
{
    SpeculateCellOperand function(this, node->child1());
    GPRTemporary result(this, Reuse, function);
    loadPtr(Address(function.gpr(), JSFunction::offsetOfScopeChain()), result.gpr());
    cellResult(result.gpr(), node);
}
    
void SpeculativeJIT::compileSkipScope(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRTemporary result(this, Reuse, scope);
    loadPtr(Address(scope.gpr(), JSScope::offsetOfNext()), result.gpr());
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileGetGlobalObject(Node* node)
{
    SpeculateCellOperand object(this, node->child1());
    GPRTemporary result(this);
    emitLoadStructure(vm(), object.gpr(), result.gpr());
    loadPtr(Address(result.gpr(), Structure::globalObjectOffset()), result.gpr());
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileGetGlobalThis(Node* node)
{
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    loadLinkableConstant(LinkableConstant::globalObject(*this, node), resultGPR);
    loadPtr(Address(resultGPR, JSGlobalObject::offsetOfGlobalThis()), resultGPR);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileUnwrapGlobalProxy(Node* node)
{
    SpeculateCellOperand object(this, node->child1());
    GPRTemporary result(this);

    GPRReg objectGPR = object.gpr();
    GPRReg resultGPR = result.gpr();

    speculateGlobalProxy(node->child1(), objectGPR);
    loadPtr(Address(objectGPR, JSGlobalProxy::targetOffset()), resultGPR);
    cellResult(resultGPR, node);
}

bool SpeculativeJIT::canBeRope(Edge& edge)
{
    if (m_state.forNode(edge).isType(SpecStringIdent))
        return false;
    // If this value is LazyValue, it will be converted to JSString, and the result must be non-rope string.
    String string = edge->tryGetString(m_graph);
    if (!string.isNull())
        return false;
    return true;
}

void SpeculativeJIT::compileGetArrayLength(Node* node)
{
    switch (node->arrayMode().type()) {
    case Array::Undecided:
    case Array::Int32:
    case Array::Double:
    case Array::Contiguous: {
        StorageOperand storage(this, node->child2());
        GPRTemporary result(this, Reuse, storage);
        GPRReg storageReg = storage.gpr();
        GPRReg resultReg = result.gpr();
        load32(Address(storageReg, Butterfly::offsetOfPublicLength()), resultReg);
            
        strictInt32Result(resultReg, node);
        break;
    }
    case Array::ArrayStorage:
    case Array::SlowPutArrayStorage: {
        StorageOperand storage(this, node->child2());
        GPRTemporary result(this, Reuse, storage);
        GPRReg storageReg = storage.gpr();
        GPRReg resultReg = result.gpr();
        load32(Address(storageReg, Butterfly::offsetOfPublicLength()), resultReg);
            
        speculationCheck(Uncountable, JSValueRegs(), nullptr, branch32(LessThan, resultReg, TrustedImm32(0)));
            
        strictInt32Result(resultReg, node);
        break;
    }
    case Array::String: {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary result(this, Reuse, base);
        GPRTemporary temp(this);
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg tempGPR = temp.gpr();

        bool needsRopeCase = canBeRope(node->child1());

        loadPtr(Address(baseGPR, JSString::offsetOfValue()), tempGPR);
        Jump isRope;
        if (needsRopeCase)
            isRope = branchIfRopeStringImpl(tempGPR);
        load32(Address(tempGPR, StringImpl::lengthMemoryOffset()), resultGPR);
        if (needsRopeCase) {
            auto done = jump();

            isRope.link(this);
            load32(Address(baseGPR, JSRopeString::offsetOfLength()), resultGPR);

            done.link(this);
        }
        strictInt32Result(resultGPR, node);
        break;
    }
    case Array::DirectArguments: {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary result(this, Reuse, base);
        
        GPRReg baseReg = base.gpr();
        GPRReg resultReg = result.gpr();
        
        if (!m_compileOkay)
            return;
        
        speculationCheck(
            ExoticObjectMode, JSValueSource(), nullptr,
            branchTestPtr(
                NonZero,
                Address(baseReg, DirectArguments::offsetOfMappedArguments())));
        
        load32(
            Address(baseReg, DirectArguments::offsetOfLength()), resultReg);
        
        strictInt32Result(resultReg, node);
        break;
    }
    case Array::ScopedArguments: {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary result(this, Reuse, base);
        
        GPRReg baseReg = base.gpr();
        GPRReg resultReg = result.gpr();
        
        if (!m_compileOkay)
            return;
        
        speculationCheck(
            ExoticObjectMode, JSValueSource(), nullptr,
            branchTest8(
                NonZero,
                Address(baseReg, ScopedArguments::offsetOfOverrodeThings())));
        
        load32(
            Address(baseReg, ScopedArguments::offsetOfTotalLength()), resultReg);
        
        strictInt32Result(resultReg, node);
        break;
    }
    default: {
        ASSERT(node->arrayMode().isSomeTypedArrayView());
#if USE(JSVALUE64)
        if (node->arrayMode().mayBeResizableOrGrowableSharedTypedArray()) {
            SpeculateCellOperand base(this, node->child1());
            GPRTemporary scratch1(this);
            GPRTemporary result(this);

            GPRReg baseGPR = base.gpr();
            GPRReg scratch1GPR = scratch1.gpr();
            GPRReg resultGPR = result.gpr();

            loadTypedArrayLength(baseGPR, resultGPR, scratch1GPR, resultGPR, node->arrayMode().type() == Array::AnyTypedArray ? std::nullopt : std::optional { node->arrayMode().typedArrayType() });

            speculationCheck(ExitKind::Overflow, JSValueSource(), nullptr, branch64(Above, resultGPR, TrustedImm64(std::numeric_limits<int32_t>::max())));
            strictInt32Result(resultGPR, node);
            return;
        }
#endif

        SpeculateCellOperand base(this, node->child1());
        GPRTemporary result(this);
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();

        if (!m_graph.isNeverResizableOrGrowableSharedTypedArrayIncludingDataView(m_state.forNode(node->child1())))
            speculationCheck(UnexpectedResizableArrayBufferView, JSValueSource::unboxedCell(baseGPR), node, branchTest8(NonZero, Address(baseGPR, JSArrayBufferView::offsetOfMode()), TrustedImm32(isResizableOrGrowableSharedMode)));
#if USE(LARGE_TYPED_ARRAYS)
        load64(Address(baseGPR, JSArrayBufferView::offsetOfLength()), resultGPR);
        speculationCheck(ExitKind::Overflow, JSValueSource(), nullptr, branch64(Above, resultGPR, TrustedImm64(std::numeric_limits<int32_t>::max())));
#else
        load32(Address(baseGPR, JSArrayBufferView::offsetOfLength()), resultGPR);
#endif
        strictInt32Result(resultGPR, node);
        break;
    } }
}


void SpeculativeJIT::compileCheckIdent(Node* node)
{
    SpeculateCellOperand stringOrSymbol(this, node->child1());
    GPRTemporary impl(this);
    GPRReg stringOrSymbolGPR = stringOrSymbol.gpr();
    GPRReg implGPR = impl.gpr();

    if (node->child1().useKind() == StringIdentUse) {
        speculateString(node->child1(), stringOrSymbolGPR);
        speculateStringIdentAndLoadStorage(node->child1(), stringOrSymbolGPR, implGPR);
    } else {
        ASSERT(node->child1().useKind() == SymbolUse);
        speculateSymbol(node->child1(), stringOrSymbolGPR);
        loadPtr(Address(stringOrSymbolGPR, Symbol::offsetOfSymbolImpl()), implGPR);
    }

    UniquedStringImpl* uid = node->uidOperand();
    speculationCheck(
        BadIdent, JSValueSource(), nullptr,
        branchPtr(NotEqual, implGPR, TrustedImmPtr(uid)));
    noResult(node);
}

template <typename ClassType>
void SpeculativeJIT::compileNewFunctionCommon(GPRReg resultGPR, RegisteredStructure structure, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scopeGPR, JumpList& slowPath, size_t size, FunctionExecutable* executable)
{
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObjectWithKnownSize<ClassType>(resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR, slowPath, size, SlowAllocationResult::UndefinedBehavior);
    
    storePtr(scopeGPR, Address(resultGPR, JSFunction::offsetOfScopeChain()));
    storeLinkableConstant(LinkableConstant(*this, executable), Address(resultGPR, JSFunction::offsetOfExecutableOrRareData()));
    mutatorFence(vm());
}

void SpeculativeJIT::compileNewFunction(Node* node)
{
    NodeType nodeType = node->op();
    ASSERT(nodeType == NewFunction || nodeType == NewGeneratorFunction || nodeType == NewAsyncFunction || nodeType == NewAsyncGeneratorFunction);
    
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();

    FunctionExecutable* executable = node->castOperand<FunctionExecutable*>();

    if (executable->singleton().isStillValid()) {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        
        flushRegisters();

        auto function = operationNewFunction;
        if (nodeType == NewGeneratorFunction)
            function = operationNewGeneratorFunction;
        else if (nodeType == NewAsyncFunction)
            function = operationNewAsyncFunction;
        else if (nodeType == NewAsyncGeneratorFunction)
            function = operationNewAsyncGeneratorFunction;
        else
            function = selectNewFunctionOperation(executable);
        callOperation(function, resultGPR, LinkableConstant:: globalObject(*this, node), scopeGPR, LinkableConstant(*this, executable));
        cellResult(resultGPR, node);
        return;
    }

    RegisteredStructure structure = m_graph.registerStructure(
        [&] () {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
            switch (nodeType) {
            case NewGeneratorFunction:
                return globalObject->generatorFunctionStructure();
            case NewAsyncFunction:
                return globalObject->asyncFunctionStructure();
            case NewAsyncGeneratorFunction:
                return globalObject->asyncGeneratorFunctionStructure();
            case NewFunction:
                return JSFunction::selectStructureForNewFuncExp(globalObject, node->castOperand<FunctionExecutable*>());
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }());
    
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    
    JumpList slowPath;
    
    if (nodeType == NewFunction) {
        compileNewFunctionCommon<JSFunction>(resultGPR, structure, scratch1GPR, scratch2GPR, scopeGPR, slowPath, JSFunction::allocationSize(0), executable);
            
        addSlowPathGenerator(slowPathCall(slowPath, this, selectNewFunctionWithInvalidatedReallocationWatchpointOperation(executable), resultGPR, LinkableConstant:: globalObject(*this, node), scopeGPR, LinkableConstant(*this, executable)));
    }

    if (nodeType == NewGeneratorFunction) {
        compileNewFunctionCommon<JSGeneratorFunction>(resultGPR, structure, scratch1GPR, scratch2GPR, scopeGPR, slowPath, JSGeneratorFunction::allocationSize(0), executable);

        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint, resultGPR, LinkableConstant:: globalObject(*this, node), scopeGPR, LinkableConstant(*this, executable)));
    }

    if (nodeType == NewAsyncFunction) {
        compileNewFunctionCommon<JSAsyncFunction>(resultGPR, structure, scratch1GPR, scratch2GPR, scopeGPR, slowPath, JSAsyncFunction::allocationSize(0), executable);

        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewAsyncFunctionWithInvalidatedReallocationWatchpoint, resultGPR, LinkableConstant:: globalObject(*this, node), scopeGPR, LinkableConstant(*this, executable)));
    }

    if (nodeType == NewAsyncGeneratorFunction) {
        compileNewFunctionCommon<JSAsyncGeneratorFunction>(resultGPR, structure, scratch1GPR, scratch2GPR, scopeGPR, slowPath, JSAsyncGeneratorFunction::allocationSize(0), executable);
        
        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewAsyncGeneratorFunctionWithInvalidatedReallocationWatchpoint, resultGPR, LinkableConstant:: globalObject(*this, node), scopeGPR, LinkableConstant(*this, executable)));
    }
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileSetFunctionName(Node* node)
{
    SpeculateCellOperand func(this, node->child1());
    GPRReg funcGPR = func.gpr();
    JSValueOperand nameValue(this, node->child2());
    JSValueRegs nameValueRegs = nameValue.jsValueRegs();

    flushRegisters();
    callOperation(operationSetFunctionName, LinkableConstant::globalObject(*this, node), funcGPR, nameValueRegs);

    noResult(node);
}

void SpeculativeJIT::compileVarargsLength(Node* node)
{
    LoadVarargsData* data = node->loadVarargsData();

    JSValueRegs argumentsRegs;
    lock(GPRInfo::returnValueGPR);
    JSValueOperand arguments(this, node->argumentsChild());
    argumentsRegs = arguments.jsValueRegs();
    flushRegisters();
    unlock(GPRInfo::returnValueGPR);

    callOperation(operationSizeOfVarargs, GPRInfo::returnValueGPR, LinkableConstant::globalObject(*this, node), argumentsRegs, data->offset);

    lock(GPRInfo::returnValueGPR);
    GPRTemporary argCountIncludingThis(this);
    GPRReg argCountIncludingThisGPR = argCountIncludingThis.gpr();
    unlock(GPRInfo::returnValueGPR);

    add32(TrustedImm32(1), GPRInfo::returnValueGPR, argCountIncludingThisGPR);

    strictInt32Result(argCountIncludingThisGPR, node);  
}

void SpeculativeJIT::compileLoadVarargs(Node* node)
{
    LoadVarargsData* data = node->loadVarargsData();

    SpeculateStrictInt32Operand argumentCount(this, node->child1());
    JSValueOperand arguments(this, node->argumentsChild(), ManualOperandSpeculation);

    GPRReg argumentCountIncludingThis = argumentCount.gpr();
    JSValueRegs argumentsRegs = arguments.jsValueRegs();

    speculate(node, node->argumentsChild());

    switch (node->argumentsChild().useKind()) {
    case UntypedUse: {
        speculationCheck(VarargsOverflow, JSValueSource(), Edge(), branchTest32(Zero, argumentCountIncludingThis));
        speculationCheck(VarargsOverflow, JSValueSource(), Edge(), branch32(Above, argumentCountIncludingThis, TrustedImm32(data->limit)));

        flushRegisters();
        store32(argumentCountIncludingThis, payloadFor(data->machineCount));
        callOperation(operationLoadVarargs, LinkableConstant::globalObject(*this, node), data->machineStart.offset(), argumentsRegs, data->offset, argumentCountIncludingThis, data->mandatoryMinimum);
        noResult(node);
        break;
    }
    case OtherUse: {
        // argumentCountIncludingThis is 1
        if (!data->limit) {
            terminateSpeculativeExecution(VarargsOverflow, JSValueRegs(), nullptr);
            break;
        }

        if (data->mandatoryMinimum) {
            flushRegisters();
            store32(argumentCountIncludingThis, payloadFor(data->machineCount));
            callOperation(operationLoadVarargs, LinkableConstant::globalObject(*this, node), data->machineStart.offset(), argumentsRegs, data->offset, argumentCountIncludingThis, data->mandatoryMinimum);
            noResult(node);
        } else {
            store32(argumentCountIncludingThis, payloadFor(data->machineCount));
            noResult(node);
        }
        break;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }


}

void SpeculativeJIT::compileForwardVarargs(Node* node)
{
    LoadVarargsData* data = node->loadVarargsData();
    InlineCallFrame* inlineCallFrame;
    if (node->argumentsChild())
        inlineCallFrame = node->argumentsChild()->origin.semantic.inlineCallFrame();
    else
        inlineCallFrame = node->origin.semantic.inlineCallFrame();

    SpeculateStrictInt32Operand argumentCount(this, node->child1());
    GPRTemporary length(this);
    JSValueRegsTemporary temp(this);
    GPRReg argumentCountIncludingThis = argumentCount.gpr();
    GPRReg lengthGPR = argumentCount.gpr();
    JSValueRegs tempRegs = temp.regs();
    
    move(argumentCountIncludingThis, lengthGPR);
    if (data->offset)
        sub32(TrustedImm32(data->offset), lengthGPR);
        
    speculationCheck(
        VarargsOverflow, JSValueSource(), Edge(), branch32(
            Above,
            lengthGPR, TrustedImm32(data->limit)));
        
    store32(lengthGPR, payloadFor(data->machineCount));
        
    VirtualRegister sourceStart = argumentsStart(inlineCallFrame) + data->offset;
    VirtualRegister targetStart = data->machineStart;

    sub32(TrustedImm32(1), lengthGPR);
        
    // First have a loop that fills in the undefined slots in case of an arity check failure.
    move(TrustedImm32(data->mandatoryMinimum), tempRegs.payloadGPR());
    Jump done = branch32(BelowOrEqual, tempRegs.payloadGPR(), lengthGPR);
        
    Label loop = label();
    sub32(TrustedImm32(1), tempRegs.payloadGPR());
    storeTrustedValue(
        jsUndefined(),
        BaseIndex(
            GPRInfo::callFrameRegister, tempRegs.payloadGPR(), TimesEight,
            targetStart.offset() * sizeof(EncodedJSValue)));
    branch32(Above, tempRegs.payloadGPR(), lengthGPR).linkTo(loop, this);
    done.link(this);
        
    // And then fill in the actual argument values.
    done = branchTest32(Zero, lengthGPR);
        
    loop = label();
    sub32(TrustedImm32(1), lengthGPR);
    loadValue(
        BaseIndex(
            GPRInfo::callFrameRegister, lengthGPR, TimesEight,
            sourceStart.offset() * sizeof(EncodedJSValue)),
        tempRegs);
    storeValue(
        tempRegs,
        BaseIndex(
            GPRInfo::callFrameRegister, lengthGPR, TimesEight,
            targetStart.offset() * sizeof(EncodedJSValue)));
    branchTest32(NonZero, lengthGPR).linkTo(loop, this);
        
    done.link(this);
        
    noResult(node);
}

void SpeculativeJIT::compileCreateActivation(Node* node)
{
    SymbolTable* table = node->castOperand<SymbolTable*>();
    RegisteredStructure structure = m_graph.registerStructure(m_graph.globalObjectFor(
        node->origin.semantic)->activationStructure());
        
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    JSValue initializationValue = node->initializationValueForActivation();
    ASSERT(initializationValue == jsUndefined() || initializationValue == jsTDZValue());
    
    if (table->singleton().isStillValid()) {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

#if USE(JSVALUE32_64)
        JSValueRegsTemporary initialization(this);
        JSValueRegs initializationRegs = initialization.regs();
        moveTrustedValue(initializationValue, initializationRegs);
#endif

        flushRegisters();

#if USE(JSVALUE64)
        callOperation(operationCreateActivationDirect,
            resultGPR, TrustedImmPtr(&vm()), structure, scopeGPR, LinkableConstant(*this, table), TrustedImm64(JSValue::encode(initializationValue)));
#else
        callOperation(operationCreateActivationDirect,
            resultGPR, TrustedImmPtr(&vm()), structure, scopeGPR, LinkableConstant(*this, table), initializationRegs);
#endif
        cellResult(resultGPR, node);
        return;
    }
    
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

#if USE(JSVALUE32_64)
    JSValueRegsTemporary initialization(this);
    JSValueRegs initializationRegs = initialization.regs();
    moveTrustedValue(initializationValue, initializationRegs);
#endif

    JumpList slowPath;
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObjectWithKnownSize<JSLexicalEnvironment>(
        resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR,
        slowPath, JSLexicalEnvironment::allocationSize(table), SlowAllocationResult::UndefinedBehavior);
        
    // Don't need a memory barriers since we just fast-created the activation, so the
    // activation must be young.
    storePtr(scopeGPR, Address(resultGPR, JSScope::offsetOfNext()));
    storeLinkableConstant(LinkableConstant(*this, node->cellOperand()->cell()), Address(resultGPR, JSLexicalEnvironment::offsetOfSymbolTable()));

    // Must initialize all members to undefined or the TDZ empty value.
    for (unsigned i = 0; i < table->scopeSize(); ++i) {
        storeTrustedValue(
            initializationValue,
            Address(
                resultGPR, JSLexicalEnvironment::offsetOfVariable(ScopeOffset(i))));
    }
    
    mutatorFence(vm());

#if USE(JSVALUE64)
    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationCreateActivationDirect, resultGPR, TrustedImmPtr(&vm()), structure, scopeGPR, LinkableConstant(*this, table), TrustedImm64(JSValue::encode(initializationValue))));
#else
    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationCreateActivationDirect, resultGPR, TrustedImmPtr(&vm()), structure, scopeGPR, LinkableConstant(*this, table), initializationRegs));
#endif

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileCreateDirectArguments(Node* node)
{
    // FIXME: A more effective way of dealing with the argument count and callee is to have
    // them be explicit arguments to this node.
    // https://bugs.webkit.org/show_bug.cgi?id=142207
    
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary length;
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg lengthGPR = InvalidGPRReg;
    JSValueRegs valueRegs = JSValueRegs::withTwoAvailableRegs(scratch1GPR, scratch2GPR);
        
    unsigned minCapacity = m_graph.baselineCodeBlockFor(node->origin.semantic)->numParameters() - 1;
        
    unsigned knownLength;
    bool lengthIsKnown; // if false, lengthGPR will have the length.
    auto* inlineCallFrame = node->origin.semantic.inlineCallFrame();
    if (inlineCallFrame
        && !inlineCallFrame->isVarargs()) {
        knownLength = static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1);
        lengthIsKnown = true;
    } else {
        knownLength = UINT_MAX;
        lengthIsKnown = false;
            
        GPRTemporary realLength(this);
        length.adopt(realLength);
        lengthGPR = length.gpr();

        VirtualRegister argumentCountRegister = argumentCount(node->origin.semantic);
        load32(payloadFor(argumentCountRegister), lengthGPR);
        sub32(TrustedImm32(1), lengthGPR);
    }
        
    RegisteredStructure structure =
        m_graph.registerStructure(m_graph.globalObjectFor(node->origin.semantic)->directArgumentsStructure());
        
    // Use a different strategy for allocating the object depending on whether we know its
    // size statically.
    JumpList slowPath;
    if (lengthIsKnown) {
        auto butterfly = TrustedImmPtr(nullptr);
        emitAllocateJSObjectWithKnownSize<DirectArguments>(
            resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR,
            slowPath, DirectArguments::allocationSize(std::max(knownLength, minCapacity)), SlowAllocationResult::UndefinedBehavior);
            
        store32(
            TrustedImm32(knownLength),
            Address(resultGPR, DirectArguments::offsetOfLength()));
    } else {
        Jump tooFewArguments;
        if (minCapacity) {
            tooFewArguments =
                branch32(Below, lengthGPR, TrustedImm32(minCapacity));
        }
        lshift32(lengthGPR, TrustedImm32(3), scratch1GPR);
        add32(TrustedImm32(DirectArguments::storageOffset()), scratch1GPR);
        if (minCapacity) {
            Jump done = jump();
            tooFewArguments.link(this);
            move(TrustedImm32(DirectArguments::allocationSize(minCapacity)), scratch1GPR);
            done.link(this);
        }
        
        emitAllocateVariableSizedJSObject<DirectArguments>(
            resultGPR, TrustedImmPtr(structure), scratch1GPR, scratch1GPR, scratch2GPR,
            slowPath, SlowAllocationResult::UndefinedBehavior);
            
        store32(
            lengthGPR, Address(resultGPR, DirectArguments::offsetOfLength()));
    }
        
    store32(
        TrustedImm32(minCapacity),
        Address(resultGPR, DirectArguments::offsetOfMinCapacity()));
        
    storePtr(
        TrustedImmPtr(nullptr), Address(resultGPR, DirectArguments::offsetOfMappedArguments()));

    storePtr(
        TrustedImmPtr(nullptr), Address(resultGPR, DirectArguments::offsetOfModifiedArgumentsDescriptor()));
    
    if (lengthIsKnown) {
        addSlowPathGenerator(
            slowPathCall(
                slowPath, this, operationCreateDirectArguments, resultGPR, TrustedImmPtr(&vm()), structure,
                knownLength, minCapacity));
    } else {
        auto generator = makeUnique<CallCreateDirectArgumentsSlowPathGenerator>(
            slowPath, this, resultGPR, structure, lengthGPR, minCapacity);
        addSlowPathGenerator(WTFMove(generator));
    }

    if (inlineCallFrame) {
        if (inlineCallFrame->isClosureCall) {
            loadPtr(
                addressFor(
                    inlineCallFrame->calleeRecovery.virtualRegister()),
                scratch1GPR);
        } else
            loadLinkableConstant(LinkableConstant(*this, inlineCallFrame->calleeRecovery.constant().asCell()), scratch1GPR);
    } else
        loadPtr(addressFor(CallFrameSlot::callee), scratch1GPR);

    // Don't need a memory barriers since we just fast-created the activation, so the
    // activation must be young.
    storePtr(
        scratch1GPR, Address(resultGPR, DirectArguments::offsetOfCallee()));
        
    VirtualRegister start = argumentsStart(node->origin.semantic);
    if (lengthIsKnown) {
        for (unsigned i = 0; i < std::max(knownLength, minCapacity); ++i) {
            loadValue(addressFor(start + i), valueRegs);
            storeValue(
                valueRegs, Address(resultGPR, DirectArguments::offsetOfSlot(i)));
        }
    } else {
        Jump done;
        if (minCapacity) {
            Jump startLoop = branch32(
                AboveOrEqual, lengthGPR, TrustedImm32(minCapacity));
            move(TrustedImm32(minCapacity), lengthGPR);
            startLoop.link(this);
        } else
            done = branchTest32(Zero, lengthGPR);
        Label loop = label();
        sub32(TrustedImm32(1), lengthGPR);
        loadValue(
            BaseIndex(
                GPRInfo::callFrameRegister, lengthGPR, TimesEight,
                start.offset() * static_cast<int>(sizeof(Register))),
            valueRegs);
        storeValue(
            valueRegs,
            BaseIndex(
                resultGPR, lengthGPR, TimesEight,
                DirectArguments::storageOffset()));
        branchTest32(NonZero, lengthGPR).linkTo(loop, this);
        if (done.isSet())
            done.link(this);
    }
        
    mutatorFence(vm());
        
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetFromArguments(Node* node)
{
    SpeculateCellOperand arguments(this, node->child1());
    JSValueRegsTemporary result(this);
    
    GPRReg argumentsGPR = arguments.gpr();
    JSValueRegs resultRegs = result.regs();
    
    loadValue(Address(argumentsGPR, DirectArguments::offsetOfSlot(node->capturedArgumentsOffset().offset())), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutToArguments(Node* node)
{
    SpeculateCellOperand arguments(this, node->child1());
    JSValueOperand value(this, node->child2());
    
    GPRReg argumentsGPR = arguments.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    
    storeValue(valueRegs, Address(argumentsGPR, DirectArguments::offsetOfSlot(node->capturedArgumentsOffset().offset())));
    noResult(node);
}

void SpeculativeJIT::compileGetArgument(Node* node)
{
    GPRTemporary argumentCount(this);
    JSValueRegsTemporary result(this);
    GPRReg argumentCountGPR = argumentCount.gpr();
    JSValueRegs resultRegs = result.regs();
    load32(payloadFor(Base::argumentCount(node->origin.semantic)), argumentCountGPR);
    auto argumentOutOfBounds = branch32(LessThanOrEqual, argumentCountGPR, TrustedImm32(node->argumentIndex()));
    loadValue(addressFor(argumentsStart(node->origin.semantic) + node->argumentIndex() - 1), resultRegs);
    auto done = jump();

    argumentOutOfBounds.link(this);
    moveValue(jsUndefined(), resultRegs);

    done.link(this);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileCreateScopedArguments(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    flushRegisters();

    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    
    // We set up the arguments ourselves, because we have the whole register file and we can
    // set them up directly into the argument registers. This also means that we don't have to
    // invent a four-argument-register shuffle.
    
    // Arguments: 0:JSGlobalObject*, 1:structure, 2:start, 3:length, 4:callee, 5:scope
    
    // Do the scopeGPR first, since it might alias an argument register.
    setupArgument(5, [&] (GPRReg destGPR) { move(scopeGPR, destGPR); });
    
    // These other things could be done in any order.
    setupArgument(4, [&] (GPRReg destGPR) { emitGetCallee(node->origin.semantic, destGPR); });
    setupArgument(3, [&] (GPRReg destGPR) { emitGetLength(node->origin.semantic, destGPR); });
    setupArgument(2, [&] (GPRReg destGPR) { emitGetArgumentStart(node->origin.semantic, destGPR); });
    setupArgument(
        1, [&] (GPRReg destGPR) {
            loadLinkableConstant(LinkableConstant(*this, globalObject->scopedArgumentsStructure()), destGPR);
        });
    setupArgument(
        0, [&] (GPRReg destGPR) {
            loadLinkableConstant(LinkableConstant::globalObject(*this, node), destGPR);
        });
    
    appendCall(operationCreateScopedArguments);
    operationExceptionCheck<decltype(operationCreateScopedArguments)>();
    setupResults(resultGPR);
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileCreateRest(Node* node)
{
    ASSERT(node->op() == CreateRest);

    if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
        SpeculateStrictInt32Operand arrayLength(this, node->child1());
        GPRTemporary arrayResult(this);

        GPRReg arrayLengthGPR = arrayLength.gpr();
        GPRReg arrayResultGPR = arrayResult.gpr();

        // We can tell compileAllocateNewArrayWithSize() that it does not need to check
        // for large arrays and use ArrayStorage structure because arrayLength here will
        // always be bounded by stack size. Realistically, we won't be able to push enough
        // arguments to have arrayLength exceed MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH.
        bool shouldAllowForArrayStorageStructureForLargeArrays = false;
        compileAllocateNewArrayWithSize(node, arrayResultGPR, arrayLengthGPR, ArrayWithContiguous, shouldAllowForArrayStorageStructureForLargeArrays);

        GPRTemporary argumentsStart(this);
        GPRReg argumentsStartGPR = argumentsStart.gpr();

        emitGetArgumentStart(node->origin.semantic, argumentsStartGPR);

        GPRTemporary butterfly(this);
        GPRTemporary currentLength(this);
        JSValueRegsTemporary value(this);

        JSValueRegs valueRegs = value.regs();
        GPRReg currentLengthGPR = currentLength.gpr();
        GPRReg butterflyGPR = butterfly.gpr();

        loadPtr(Address(arrayResultGPR, JSObject::butterflyOffset()), butterflyGPR);

        Jump skipLoop = branch32(Equal, arrayLengthGPR, TrustedImm32(0));
        zeroExtend32ToWord(arrayLengthGPR, currentLengthGPR);
        addPtr(Imm32(sizeof(Register) * node->numberOfArgumentsToSkip()), argumentsStartGPR);

        auto loop = label();
        sub32(TrustedImm32(1), currentLengthGPR);
        loadValue(BaseIndex(argumentsStartGPR, currentLengthGPR, TimesEight), valueRegs);
        storeValue(valueRegs, BaseIndex(butterflyGPR, currentLengthGPR, TimesEight));
        branch32(NotEqual, currentLengthGPR, TrustedImm32(0)).linkTo(loop, this);

        skipLoop.link(this);
        cellResult(arrayResultGPR, node);
        return;
    }

    SpeculateStrictInt32Operand arrayLength(this, node->child1());
    GPRTemporary argumentsStart(this);
    GPRTemporary numberOfArgumentsToSkip(this);

    GPRReg arrayLengthGPR = arrayLength.gpr();
    GPRReg argumentsStartGPR = argumentsStart.gpr();

    emitGetArgumentStart(node->origin.semantic, argumentsStartGPR);

    flushRegisters();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationCreateRest, resultGPR, LinkableConstant::globalObject(*this, node), argumentsStartGPR, Imm32(node->numberOfArgumentsToSkip()), arrayLengthGPR);

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileSpread(Node* node)
{
    ASSERT(node->op() == Spread);

    SpeculateCellOperand operand(this, node->child1());
    GPRReg argument = operand.gpr();

    if (node->child1().useKind() == ArrayUse)
        speculateArray(node->child1(), argument);

    if (m_graph.canDoFastSpread(node, m_state.forNode(node->child1()))) {
#if USE(JSVALUE64)
        GPRTemporary result(this);
        GPRTemporary scratch1(this);
        GPRTemporary scratch2(this);
        GPRTemporary length(this);
        FPRTemporary doubleRegister(this);

        GPRReg resultGPR = result.gpr();
        GPRReg scratch1GPR = scratch1.gpr();
        GPRReg scratch2GPR = scratch2.gpr();
        GPRReg lengthGPR = length.gpr();
        FPRReg doubleFPR = doubleRegister.fpr();

        JumpList slowPath;
        JumpList done;

        load8(Address(argument, JSCell::indexingTypeAndMiscOffset()), scratch1GPR);
        and32(TrustedImm32(IndexingModeMask), scratch1GPR);
        auto notShareCase = branch32(NotEqual, scratch1GPR, TrustedImm32(CopyOnWriteArrayWithContiguous));
        loadPtr(Address(argument, JSObject::butterflyOffset()), resultGPR);
        addPtr(TrustedImm32(-static_cast<ptrdiff_t>(JSImmutableButterfly::offsetOfData())), resultGPR);
        done.append(jump());

        notShareCase.link(this);
        and32(TrustedImm32(IndexingShapeMask), scratch1GPR);
        sub32(TrustedImm32(Int32Shape), scratch1GPR);

        slowPath.append(branch32(Above, scratch1GPR, TrustedImm32(ContiguousShape - Int32Shape)));

        loadPtr(Address(argument, JSObject::butterflyOffset()), lengthGPR);
        load32(Address(lengthGPR, Butterfly::offsetOfPublicLength()), lengthGPR);
        slowPath.append(branch32(Above, lengthGPR, TrustedImm32(MAX_STORAGE_VECTOR_LENGTH)));
        static_assert(sizeof(JSValue) == 8 && 1 << 3 == 8, "This is strongly assumed in the code below.");
        lshift32(lengthGPR, TrustedImm32(3), scratch1GPR);
        add32(TrustedImm32(JSImmutableButterfly::offsetOfData()), scratch1GPR);

        emitAllocateVariableSizedCell<JSImmutableButterfly>(vm(), resultGPR, TrustedImmPtr(m_graph.registerStructure(vm().immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get())), scratch1GPR, scratch1GPR, scratch2GPR, slowPath, SlowAllocationResult::UndefinedBehavior);
        static_assert(JSImmutableButterfly::offsetOfPublicLength() + static_cast<ptrdiff_t>(sizeof(uint32_t)) == JSImmutableButterfly::offsetOfVectorLength());
        storePair32(lengthGPR, lengthGPR, resultGPR, TrustedImm32(JSImmutableButterfly::offsetOfPublicLength()));

        loadPtr(Address(argument, JSObject::butterflyOffset()), scratch1GPR);

        load8(Address(argument, JSCell::indexingTypeAndMiscOffset()), scratch2GPR);
        and32(TrustedImm32(IndexingShapeMask), scratch2GPR);
        auto isDoubleArray = branch32(Equal, scratch2GPR, TrustedImm32(DoubleShape));

        {
            done.append(branchTest32(Zero, lengthGPR));
            auto loopStart = label();
            sub32(TrustedImm32(1), lengthGPR);
            load64(BaseIndex(scratch1GPR, lengthGPR, TimesEight), scratch2GPR);
            auto notEmpty = branchIfNotEmpty(scratch2GPR);
            move(TrustedImm64(JSValue::encode(jsUndefined())), scratch2GPR);
            notEmpty.link(this);
            store64(scratch2GPR, BaseIndex(resultGPR, lengthGPR, TimesEight, JSImmutableButterfly::offsetOfData()));
            branchTest32(NonZero, lengthGPR).linkTo(loopStart, this);
            done.append(jump());
        }

        isDoubleArray.link(this);
        {
            done.append(branchTest32(Zero, lengthGPR));
            auto loopStart = label();
            sub32(TrustedImm32(1), lengthGPR);
            loadDouble(BaseIndex(scratch1GPR, lengthGPR, TimesEight), doubleFPR);
            auto notEmpty = branchIfNotNaN(doubleFPR);
            move(TrustedImm64(JSValue::encode(jsUndefined())), scratch2GPR);
            auto doStore = jump();
            notEmpty.link(this);
            boxDouble(doubleFPR, scratch2GPR);
            doStore.link(this);
            store64(scratch2GPR, BaseIndex(resultGPR, lengthGPR, TimesEight, JSImmutableButterfly::offsetOfData()));
            branchTest32(NonZero, lengthGPR).linkTo(loopStart, this);
            done.append(jump());
        }
        
        addSlowPathGenerator(slowPathCall(slowPath, this, operationSpreadFastArray, resultGPR, LinkableConstant::globalObject(*this, node), argument));

        done.link(this);
        mutatorFence(vm());
        cellResult(resultGPR, node);
#else
        flushRegisters();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationSpreadFastArray, resultGPR, LinkableConstant::globalObject(*this, node), argument);
        cellResult(resultGPR, node);
#endif // USE(JSVALUE64)
    } else {
        flushRegisters();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationSpreadGeneric, resultGPR, LinkableConstant::globalObject(*this, node), argument);
        cellResult(resultGPR, node);
    }
}

void SpeculativeJIT::compileNewArray(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    RegisteredStructure structure = m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()));
    if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(node->indexingType())) {
        unsigned numElements = node->numChildren();
        unsigned vectorLengthHint = node->vectorLengthHint();
        ASSERT(vectorLengthHint >= numElements);

        // Because we first speculate on all of the children here, we can never exit after creating
        // uninitialized contiguous JSArray, which ensures that we will never produce a half-baked JSArray.
        for (unsigned operandIndex = 0; operandIndex < node->numChildren(); ++operandIndex)
            speculate(node, m_graph.varArgChild(node, operandIndex));

        GPRTemporary result(this);
        GPRTemporary storage(this);

        GPRReg resultGPR = result.gpr();
        GPRReg storageGPR = storage.gpr();

        emitAllocateRawObject(resultGPR, structure, storageGPR, numElements, vectorLengthHint);

        // At this point, one way or another, resultGPR and storageGPR have pointers to
        // the JSArray and the Butterfly, respectively.

        ASSERT(!hasUndecided(structure->indexingType()) || !node->numChildren());

        for (unsigned operandIndex = 0; operandIndex < node->numChildren(); ++operandIndex) {
            Edge use = m_graph.varArgChild(node, operandIndex);
            switch (node->indexingType()) {
            case ALL_BLANK_INDEXING_TYPES:
            case ALL_UNDECIDED_INDEXING_TYPES:
                CRASH();
                break;
            case ALL_DOUBLE_INDEXING_TYPES: {
                SpeculateDoubleOperand operand(this, use);
                FPRReg opFPR = operand.fpr();
                storeDouble(opFPR, Address(storageGPR, sizeof(double) * operandIndex));
                break;
            }
            case ALL_INT32_INDEXING_TYPES:
            case ALL_CONTIGUOUS_INDEXING_TYPES: {
                JSValueOperand operand(this, use, ManualOperandSpeculation);
                JSValueRegs operandRegs = operand.jsValueRegs();
                storeValue(operandRegs, Address(storageGPR, sizeof(JSValue) * operandIndex));
                break;
            }
            default:
                CRASH();
                break;
            }
        }

        // Yuck, we should *really* have a way of also returning the storageGPR. But
        // that's the least of what's wrong with this code. We really shouldn't be
        // allocating the array after having computed - and probably spilled to the
        // stack - all of the things that will go into the array. The solution to that
        // bigger problem will also likely fix the redundancy in reloading the storage
        // pointer that we currently have.

        cellResult(resultGPR, node);
        return;
    }

    if (!node->numChildren()) {
        flushRegisters();
        GPRFlushedCallResult result(this);
        callOperation(operationNewEmptyArray, result.gpr(), TrustedImmPtr(&vm()), structure);
        cellResult(result.gpr(), node);
        return;
    }

    size_t scratchSize = sizeof(EncodedJSValue) * node->numChildren();
    ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
    EncodedJSValue* buffer = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : nullptr;
    switch (node->indexingType()) {
    // Need to perform the speculations that this node promises to perform. If we're
    // emitting code here and the indexing type is not array storage then there is
    // probably something hilarious going on and we're already failing at all the
    // things, but at least we're going to be sound.
    case ALL_DOUBLE_INDEXING_TYPES: {
        for (unsigned operandIdx = 0; operandIdx < node->numChildren(); ++operandIdx) {
            Edge use = m_graph.m_varArgChildren[node->firstChild() + operandIdx];
            SpeculateDoubleOperand operand(this, use);
            FPRReg opFPR = operand.fpr();
            DFG_TYPE_CHECK(
                JSValueRegs(), use, SpecDoubleReal,
                branchIfNaN(opFPR));
        }
        for (unsigned operandIdx = 0; operandIdx < node->numChildren(); ++operandIdx) {
            Edge use = m_graph.m_varArgChildren[node->firstChild() + operandIdx];
            SpeculateDoubleOperand operand(this, use);
            FPRReg opFPR = operand.fpr();
#if USE(JSVALUE64)
            JSValueRegsTemporary scratch(this);
            JSValueRegs scratchRegs = scratch.regs();
            boxDouble(opFPR, scratchRegs);
            storeValue(scratchRegs, buffer + operandIdx);
#else
            storeDouble(opFPR, TrustedImmPtr(buffer + operandIdx));
#endif
            operand.use();
        }
        break;
    }
    case ALL_INT32_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        if (hasInt32(node->indexingType())) {
            for (unsigned operandIdx = 0; operandIdx < node->numChildren(); ++operandIdx) {
                Edge use = m_graph.m_varArgChildren[node->firstChild() + operandIdx];
                JSValueOperand operand(this, use, ManualOperandSpeculation);
                JSValueRegs operandRegs = operand.jsValueRegs();
                DFG_TYPE_CHECK(
                    operandRegs, use, SpecInt32Only,
                    branchIfNotInt32(operandRegs));
            }
        }
        for (unsigned operandIdx = 0; operandIdx < node->numChildren(); ++operandIdx) {
            Edge use = m_graph.m_varArgChildren[node->firstChild() + operandIdx];
            JSValueOperand operand(this, use, ManualOperandSpeculation);
            JSValueRegs operandRegs = operand.jsValueRegs();
            storeValue(operandRegs, buffer + operandIdx);
            operand.use();
        }
        break;
    }
    default:
        CRASH();
        break;
    }

    flushRegisters();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    callOperation(
        operationNewArray, resultGPR, LinkableConstant::globalObject(*this, node), m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(node->indexingType())),
        TrustedImmPtr(buffer), size_t(node->numChildren()));

    cellResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileNewArrayWithSpread(Node* node)
{
    ASSERT(node->op() == NewArrayWithSpread);

#if USE(JSVALUE64)
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

    if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        BitVector* bitVector = node->bitVector();

        if (node->numChildren() == 1 && bitVector->get(0)) {
            Edge use = m_graph.varArgChild(node, 0);
            SpeculateCellOperand immutableButterfly(this, use);
            GPRTemporary result(this);
            GPRTemporary butterfly(this);
            GPRTemporary scratch1(this);
            GPRTemporary scratch2(this);

            GPRReg immutableButterflyGPR = immutableButterfly.gpr();
            GPRReg resultGPR = result.gpr();
            GPRReg butterflyGPR = butterfly.gpr();
            GPRReg scratch1GPR = scratch1.gpr();
            GPRReg scratch2GPR = scratch2.gpr();

            RegisteredStructure structure = m_graph.registerStructure(globalObject->originalArrayStructureForIndexingType(CopyOnWriteArrayWithContiguous));

            JumpList slowCases;

            move(immutableButterflyGPR, butterflyGPR);
            addPtr(TrustedImm32(JSImmutableButterfly::offsetOfData()), butterflyGPR);

            emitAllocateJSObject<JSArray>(resultGPR, TrustedImmPtr(structure), butterflyGPR, scratch1GPR, scratch2GPR, slowCases, SlowAllocationResult::UndefinedBehavior);

            addSlowPathGenerator(slowPathCall(slowCases, this, operationNewArrayBuffer, resultGPR, TrustedImmPtr(&vm()), structure, immutableButterflyGPR));

            cellResult(resultGPR, node);
            return;
        }

        {
            unsigned startLength = 0;
            for (unsigned i = 0; i < node->numChildren(); ++i) {
                if (!bitVector->get(i))
                    ++startLength;
            }

            GPRTemporary length(this);
            GPRReg lengthGPR = length.gpr();
            move(TrustedImm32(startLength), lengthGPR);

            for (unsigned i = 0; i < node->numChildren(); ++i) {
                if (bitVector->get(i)) {
                    Edge use = m_graph.varArgChild(node, i);
                    SpeculateCellOperand immutableButterfly(this, use);
                    GPRReg immutableButterflyGPR = immutableButterfly.gpr();
                    speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branchAdd32(Overflow, Address(immutableButterflyGPR, JSImmutableButterfly::offsetOfPublicLength()), lengthGPR));
                }
            }

            speculationCheck(ExitKind::Overflow, JSValueRegs(), nullptr, branch32(AboveOrEqual, lengthGPR, TrustedImm32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)));

            // We can tell compileAllocateNewArrayWithSize() that it does not need to
            // check for large arrays and use ArrayStorage structure because we already
            // ensured above that the spread array length will definitely fit in a
            // non-ArrayStorage shaped array.
            bool shouldAllowForArrayStorageStructureForLargeArrays = false;
            compileAllocateNewArrayWithSize(node, resultGPR, lengthGPR, ArrayWithContiguous, shouldAllowForArrayStorageStructureForLargeArrays);
        }

        GPRTemporary index(this);
        GPRReg indexGPR = index.gpr();

        GPRTemporary storage(this);
        GPRReg storageGPR = storage.gpr();

        move(TrustedImm32(0), indexGPR);
        loadPtr(Address(resultGPR, JSObject::butterflyOffset()), storageGPR);

        for (unsigned i = 0; i < node->numChildren(); ++i) {
            Edge use = m_graph.varArgChild(node, i);
            if (bitVector->get(i)) {
                SpeculateCellOperand immutableButterfly(this, use);
                GPRReg immutableButterflyGPR = immutableButterfly.gpr();

                GPRTemporary immutableButterflyIndex(this);
                GPRReg immutableButterflyIndexGPR = immutableButterflyIndex.gpr();

                GPRTemporary item(this);
                GPRReg itemGPR = item.gpr();

                GPRTemporary immutableButterflyLength(this);
                GPRReg immutableButterflyLengthGPR = immutableButterflyLength.gpr();

                load32(Address(immutableButterflyGPR, JSImmutableButterfly::offsetOfPublicLength()), immutableButterflyLengthGPR);
                move(TrustedImm32(0), immutableButterflyIndexGPR);
                auto done = branchPtr(AboveOrEqual, immutableButterflyIndexGPR, immutableButterflyLengthGPR);
                auto loopStart = label();
                load64(
                    BaseIndex(immutableButterflyGPR, immutableButterflyIndexGPR, TimesEight, JSImmutableButterfly::offsetOfData()),
                    itemGPR);

                store64(itemGPR, BaseIndex(storageGPR, indexGPR, TimesEight));
                addPtr(TrustedImm32(1), immutableButterflyIndexGPR);
                addPtr(TrustedImm32(1), indexGPR);
                branchPtr(Below, immutableButterflyIndexGPR, immutableButterflyLengthGPR).linkTo(loopStart, this);

                done.link(this);
            } else {
                JSValueOperand item(this, use);
                GPRReg itemGPR = item.gpr();
                store64(itemGPR, BaseIndex(storageGPR, indexGPR, TimesEight));
                addPtr(TrustedImm32(1), indexGPR);
            }
        }

        cellResult(resultGPR, node);
        return;
    }
#endif // USE(JSVALUE64)

    ASSERT(node->numChildren());
    size_t scratchSize = sizeof(EncodedJSValue) * node->numChildren();
    ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
    EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());

    BitVector* bitVector = node->bitVector();
    for (unsigned i = 0; i < node->numChildren(); ++i) {
        Edge use = m_graph.m_varArgChildren[node->firstChild() + i];
        if (bitVector->get(i)) {
            SpeculateCellOperand immutableButterfly(this, use);
            GPRReg immutableButterflyGPR = immutableButterfly.gpr();
            storeCell(immutableButterflyGPR, &buffer[i]);
        } else {
            JSValueOperand input(this, use);
            JSValueRegs inputRegs = input.jsValueRegs();
            storeValue(inputRegs, &buffer[i]);
        }
    }

    flushRegisters();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    callOperation(operationNewArrayWithSpreadSlow, resultGPR, LinkableConstant::globalObject(*this, node), TrustedImmPtr(buffer), node->numChildren());

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetRestLength(Node* node)
{
    ASSERT(node->op() == GetRestLength);

    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();

    emitGetLength(node->origin.semantic, resultGPR);
    Jump hasNonZeroLength = branch32(Above, resultGPR, Imm32(node->numberOfArgumentsToSkip()));
    move(TrustedImm32(0), resultGPR);
    Jump done = jump();
    hasNonZeroLength.link(this);
    if (node->numberOfArgumentsToSkip())
        sub32(TrustedImm32(node->numberOfArgumentsToSkip()), resultGPR);
    done.link(this);
    strictInt32Result(resultGPR, node);
}

void SpeculativeJIT::emitPopulateSliceIndex(Edge& target, std::optional<GPRReg> indexGPR, GPRReg lengthGPR, GPRReg resultGPR)
{
    if (target->isInt32Constant()) {
        int32_t value = target->asInt32();
        if (value == 0) {
            move(TrustedImm32(0), resultGPR);
            return;
        }

        JumpList done;
        if (value > 0) {
            move(TrustedImm32(value), resultGPR);
            done.append(branch32(BelowOrEqual, resultGPR, lengthGPR));
            move(lengthGPR, resultGPR);
        } else {
            ASSERT(value != 0);
            move(lengthGPR, resultGPR);
            done.append(branchAdd32(PositiveOrZero, TrustedImm32(value), resultGPR));
            move(TrustedImm32(0), resultGPR);
        }
        done.link(this);
        return;
    }

    std::optional<SpeculateInt32Operand> index;
    if (!indexGPR) {
        index.emplace(this, target);
        indexGPR = index->gpr();
    }
    JumpList done;

    auto isPositive = branch32(GreaterThanOrEqual, indexGPR.value(), TrustedImm32(0));
    move(lengthGPR, resultGPR);
    done.append(branchAdd32(PositiveOrZero, indexGPR.value(), resultGPR));
    move(TrustedImm32(0), resultGPR);
    done.append(jump());

    isPositive.link(this);
    move(indexGPR.value(), resultGPR);
    done.append(branch32(BelowOrEqual, resultGPR, lengthGPR));
    move(lengthGPR, resultGPR);

    done.link(this);
}

void SpeculativeJIT::compileArraySlice(Node* node)
{
    ASSERT(node->op() == ArraySlice);

    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

    GPRTemporary temp(this);
    StorageOperand storage(this, m_graph.varArgChild(node, node->numChildren() - 1));
    GPRTemporary result(this);
    
    GPRReg storageGPR = storage.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg tempGPR = temp.gpr();

    if (node->numChildren() == 2)
        load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), tempGPR);
    else {
        ASSERT(node->numChildren() == 3 || node->numChildren() == 4);
        GPRTemporary tempLength(this);
        GPRReg lengthGPR = tempLength.gpr();
        load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), lengthGPR);

        if (node->numChildren() == 4)
            emitPopulateSliceIndex(m_graph.varArgChild(node, 2), std::nullopt, lengthGPR, tempGPR);
        else
            move(lengthGPR, tempGPR);

        if (m_graph.varArgChild(node, 1)->isInt32Constant() && m_graph.varArgChild(node, 1)->asInt32() == 0) {
            // Do nothing for array.slice(0, end) or array.slice(0) cases.
            // `tempGPR` already points to the size of a newly created array.
        } else {
            GPRTemporary tempStartIndex(this);
            GPRReg startGPR = tempStartIndex.gpr();
            emitPopulateSliceIndex(m_graph.varArgChild(node, 1), std::nullopt, lengthGPR, startGPR);

            auto tooBig = branch32(Above, startGPR, tempGPR);
            sub32(startGPR, tempGPR); // the size of the array we'll make.
            auto done = jump();

            tooBig.link(this);
            move(TrustedImm32(0), tempGPR);
            done.link(this);
        }
    }

    GPRTemporary temp3(this);
    GPRReg tempValue = temp3.gpr();

    {
        // We need to keep the source array alive at least until after we're done
        // with anything that can GC (e.g. allocating the result array below).
        SpeculateCellOperand cell(this, m_graph.varArgChild(node, 0));

        load8(Address(cell.gpr(), JSCell::indexingTypeAndMiscOffset()), tempValue);
        // We can ignore the writability of the cell since we won't write to the source.
        and32(TrustedImm32(AllWritableArrayTypesAndHistory), tempValue);

        JSValueRegsTemporary emptyValue(this);
        JSValueRegs emptyValueRegs = emptyValue.regs();

        GPRTemporary storage(this);
        GPRReg storageResultGPR = storage.gpr();

        GPRReg sizeGPR = tempGPR; 

        JumpList done;

        auto emitMoveEmptyValue = [&] (JSValue v) {
            moveValue(v, emptyValueRegs);
        };

        auto isContiguous = branch32(Equal, tempValue, TrustedImm32(ArrayWithContiguous));
        auto isInt32 = branch32(Equal, tempValue, TrustedImm32(ArrayWithInt32));
        // When we emit an ArraySlice, we dominate the use of the array by a CheckStructure
        // to ensure the incoming array is one to be one of the original array structures
        // with one of the following indexing shapes: Int32, Contiguous, Double. Therefore,
        // we're a double array here.
        move(TrustedImmPtr(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithDouble))), tempValue);
        emitMoveEmptyValue(jsNaN());
        done.append(jump());

        isContiguous.link(this);
        move(TrustedImmPtr(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous))), tempValue);
        emitMoveEmptyValue(JSValue());
        done.append(jump());

        isInt32.link(this);
        move(TrustedImmPtr(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithInt32))), tempValue);
        emitMoveEmptyValue(JSValue());

        done.link(this);

        JumpList slowCases;
        move(TrustedImmPtr(nullptr), storageResultGPR);
        // Enable the fast case on 64-bit platforms, where a sufficient amount of GP registers should be available.
        // Other platforms could support the same approach with custom code, but that is not currently worth the extra code maintenance.
        if (is64Bit()) {
            GPRTemporary scratch(this);
            GPRTemporary scratch2(this);
            GPRReg scratchGPR = scratch.gpr();
            GPRReg scratch2GPR = scratch2.gpr();

            emitAllocateButterfly(storageResultGPR, sizeGPR, scratchGPR, scratch2GPR, resultGPR, slowCases);
            emitInitializeButterfly(storageResultGPR, sizeGPR, emptyValueRegs, scratchGPR);
            emitAllocateJSObject<JSArray>(resultGPR, tempValue, storageResultGPR, scratchGPR, scratch2GPR, slowCases, SlowAllocationResult::UndefinedBehavior);
            mutatorFence(vm());
        } else {
            slowCases.append(jump());
        }

        addSlowPathGenerator(makeUnique<CallArrayAllocatorWithVariableStructureVariableSizeSlowPathGenerator>(
            slowCases, this, operationNewArrayWithSize, resultGPR, LinkableConstant::globalObject(*this, node), tempValue, sizeGPR, storageResultGPR));
    }

    GPRTemporary temp4(this);
    GPRReg loadIndex = temp4.gpr();

    if (node->numChildren() == 2) {
        load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), tempGPR);
        move(TrustedImm32(0), loadIndex);
    } else {
        load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), tempValue);
        if (node->numChildren() == 4)
            emitPopulateSliceIndex(m_graph.varArgChild(node, 2), std::nullopt, tempValue, tempGPR);
        else
            move(tempValue, tempGPR);
        emitPopulateSliceIndex(m_graph.varArgChild(node, 1), std::nullopt, tempValue, loadIndex);
    }

    GPRTemporary temp5(this);
    GPRReg storeIndex = temp5.gpr();
    move(TrustedImmPtr(nullptr), storeIndex);

    GPRTemporary temp2(this);
    GPRReg resultButterfly = temp2.gpr();

    loadPtr(Address(resultGPR, JSObject::butterflyOffset()), resultButterfly);
    zeroExtend32ToWord(tempGPR, tempGPR);
    zeroExtend32ToWord(loadIndex, loadIndex);
    auto done = branchPtr(AboveOrEqual, loadIndex, tempGPR);

    auto loop = label();
#if USE(JSVALUE64)
    load64(
        BaseIndex(storageGPR, loadIndex, TimesEight), tempValue);
    store64(
        tempValue, BaseIndex(resultButterfly, storeIndex, TimesEight));
#else
    load32(
        BaseIndex(storageGPR, loadIndex, TimesEight, PayloadOffset), tempValue);
    store32(
        tempValue, BaseIndex(resultButterfly, storeIndex, TimesEight, PayloadOffset));
    load32(
        BaseIndex(storageGPR, loadIndex, TimesEight, TagOffset), tempValue);
    store32(
        tempValue, BaseIndex(resultButterfly, storeIndex, TimesEight, TagOffset));
#endif // USE(JSVALUE64)
    addPtr(TrustedImm32(1), loadIndex);
    addPtr(TrustedImm32(1), storeIndex);
    branchPtr(Below, loadIndex, tempGPR).linkTo(loop, this);

    done.link(this);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileArraySplice(Node* node)
{
    unsigned refCount = node->refCount();
    bool mustGenerate = node->mustGenerate();
    if (mustGenerate)
        --refCount;

    SpeculateCellOperand base(this, m_graph.child(node, 0));
    SpeculateInt32Operand start(this, m_graph.child(node, 1));
    SpeculateInt32Operand deleteCount(this, m_graph.child(node, 2));
    GPRTemporary buffer(this);

    GPRReg baseGPR = base.gpr();
    GPRReg startGPR = start.gpr();
    GPRReg deleteCountGPR = deleteCount.gpr();
    GPRReg bufferGPR = buffer.gpr();

    speculateArray(m_graph.child(node, 0), baseGPR);

    unsigned insertionCount = node->numChildren() - 3;
    if (insertionCount) {
        size_t scratchSize = sizeof(EncodedJSValue) * insertionCount;
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = bitwise_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());

        move(TrustedImmPtr(buffer), bufferGPR);
        for (unsigned index = 0; index < insertionCount; ++index) {
            JSValueOperand arg(this, m_graph.child(node, index + 3));
            JSValueRegs argRegs = arg.regs();
            storeValue(argRegs, Address(bufferGPR, sizeof(EncodedJSValue) * index));
        }

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(refCount ? operationArraySplice : operationArraySpliceIgnoreResult, resultRegs, LinkableConstant::globalObject(*this, node), baseGPR, startGPR, deleteCountGPR, bufferGPR, TrustedImm32(insertionCount));
        jsValueResult(resultRegs, node);
        return;
    }

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(refCount ? operationArraySplice : operationArraySpliceIgnoreResult, resultRegs, LinkableConstant::globalObject(*this, node), baseGPR, startGPR, deleteCountGPR, nullptr, TrustedImm32(insertionCount));
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileArrayIndexOf(Node* node)
{
    ASSERT(node->op() == ArrayIndexOf);

    StorageOperand storage(this, m_graph.varArgChild(node, node->numChildren() == 3 ? 2 : 3));
    GPRTemporary index(this);
    GPRTemporary tempLength(this);

    GPRReg storageGPR = storage.gpr();
    GPRReg indexGPR = index.gpr();
    GPRReg lengthGPR = tempLength.gpr();

    load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), lengthGPR);

    if (node->numChildren() == 4)
        emitPopulateSliceIndex(m_graph.varArgChild(node, 2), std::nullopt, lengthGPR, indexGPR);
    else
        move(TrustedImm32(0), indexGPR);

    Edge& searchElementEdge = m_graph.varArgChild(node, 1);
    switch (searchElementEdge.useKind()) {
    case Int32Use: {
        auto emitLoop = [&] (auto emitCompare) {
#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
            clearRegisterAllocationOffsets();
#endif

            zeroExtend32ToWord(lengthGPR, lengthGPR);
            zeroExtend32ToWord(indexGPR, indexGPR);

            auto loop = label();
            auto notFound = branch32(Equal, indexGPR, lengthGPR);

            auto found = emitCompare();

            add32(TrustedImm32(1), indexGPR);
            jump().linkTo(loop, this);

            notFound.link(this);
            move(TrustedImm32(-1), indexGPR);
            found.link(this);
            strictInt32Result(indexGPR, node);
        };

        ASSERT(node->arrayMode().type() == Array::Int32);
#if USE(JSVALUE64)
        JSValueOperand searchElement(this, searchElementEdge, ManualOperandSpeculation);
        JSValueRegs searchElementRegs = searchElement.jsValueRegs();
        speculateInt32(searchElementEdge, searchElementRegs);
        GPRReg searchElementGPR = searchElementRegs.payloadGPR();
#else
        SpeculateInt32Operand searchElement(this, searchElementEdge);
        GPRReg searchElementGPR = searchElement.gpr();

        GPRTemporary temp(this);
        GPRReg tempGPR = temp.gpr();
#endif
        emitLoop([&] () {
#if USE(JSVALUE64)
            auto found = branch64(Equal, BaseIndex(storageGPR, indexGPR, TimesEight), searchElementGPR);
#else
            auto skip = branch32(NotEqual, BaseIndex(storageGPR, indexGPR, TimesEight, TagOffset), TrustedImm32(JSValue::Int32Tag));
            load32(BaseIndex(storageGPR, indexGPR, TimesEight, PayloadOffset), tempGPR);
            auto found = branch32(Equal, tempGPR, searchElementGPR);
            skip.link(this);
#endif
            return found;
        });
        return;
    }

    case DoubleRepUse: {
        ASSERT(node->arrayMode().type() == Array::Double);
        SpeculateDoubleOperand searchElement(this, searchElementEdge);
        FPRTemporary tempDouble(this);

        FPRReg searchElementFPR = searchElement.fpr();
        FPRReg tempFPR = tempDouble.fpr();

#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
        clearRegisterAllocationOffsets();
#endif

        zeroExtend32ToWord(lengthGPR, lengthGPR);
        zeroExtend32ToWord(indexGPR, indexGPR);

        auto loop = label();
        auto notFound = branch32(Equal, indexGPR, lengthGPR);
        loadDouble(BaseIndex(storageGPR, indexGPR, TimesEight), tempFPR);
        auto found = branchDouble(DoubleEqualAndOrdered, tempFPR, searchElementFPR);
        add32(TrustedImm32(1), indexGPR);
        jump().linkTo(loop, this);

        notFound.link(this);
        move(TrustedImm32(-1), indexGPR);
        found.link(this);
        strictInt32Result(indexGPR, node);
        return;
    }

    case StringUse: {
        ASSERT(node->arrayMode().type() == Array::Contiguous);
        SpeculateCellOperand searchElement(this, searchElementEdge);

        GPRReg searchElementGPR = searchElement.gpr();

        speculateString(searchElementEdge, searchElementGPR);

        flushRegisters();

        callOperation(operationArrayIndexOfString, lengthGPR, LinkableConstant::globalObject(*this, node), storageGPR, searchElementGPR, indexGPR);

        strictInt32Result(lengthGPR, node);
        return;
    }

    case ObjectUse:
    case SymbolUse:
    case OtherUse: {
        JSValueOperand value(this, searchElementEdge, ManualOperandSpeculation);

        JSValueRegs valueRegs = value.jsValueRegs();
        speculate(node, searchElementEdge);

        ASSERT(node->arrayMode().type() == Array::Contiguous);

        flushRegisters();
        callOperationWithoutExceptionCheck(operationArrayIndexOfNonStringIdentityValueContiguous, lengthGPR, storageGPR, valueRegs, indexGPR);
        strictInt32Result(lengthGPR, node);
        return;
    }

    case UntypedUse: {
        JSValueOperand searchElement(this, searchElementEdge);

        JSValueRegs searchElementRegs = searchElement.jsValueRegs();

        flushRegisters();
        switch (node->arrayMode().type()) {
        case Array::Double:
            callOperation(operationArrayIndexOfValueDouble, lengthGPR, storageGPR, searchElementRegs, indexGPR);
            break;
        case Array::Int32:
        case Array::Contiguous:
            callOperation(operationArrayIndexOfValueInt32OrContiguous, lengthGPR, LinkableConstant::globalObject(*this, node), storageGPR, searchElementRegs, indexGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        strictInt32Result(lengthGPR, node);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileArrayPush(Node* node)
{
    ASSERT(node->arrayMode().isJSArray());

    Edge& storageEdge = m_graph.varArgChild(node, 0);
    Edge& arrayEdge = m_graph.varArgChild(node, 1);
    unsigned elementOffset = 2;
    unsigned elementCount = node->numChildren() - elementOffset;

    SpeculateCellOperand base(this, arrayEdge);
    StorageOperand storage(this, storageEdge);
    GPRTemporary storageLength(this);

    GPRReg baseGPR = base.gpr();
    GPRReg storageGPR = storage.gpr();
    GPRReg storageLengthGPR = storageLength.gpr();

#if USE(JSVALUE32_64)
    GPRTemporary tag(this);
    GPRReg tagGPR = tag.gpr();
    JSValueRegs resultRegs { tagGPR, storageLengthGPR };
#else
    JSValueRegs resultRegs { storageLengthGPR };
#endif

    auto getStorageBufferAddress = [&] (GPRReg storageGPR, GPRReg indexGPR, int32_t offset, GPRReg bufferGPR) {
        static_assert(sizeof(JSValue) == 8 && 1 << 3 == 8, "This is strongly assumed in the code below.");
        getEffectiveAddress(BaseIndex(storageGPR, indexGPR, TimesEight, offset), bufferGPR);
    };

    switch (node->arrayMode().type()) {
    case Array::Int32:
    case Array::Contiguous: {
        if (elementCount == 1) {
            Edge& element = m_graph.varArgChild(node, elementOffset);
            if (node->arrayMode().type() == Array::Int32) {
                ASSERT(element.useKind() == Int32Use);
                speculateInt32(element);
            }
            JSValueOperand value(this, element, ManualOperandSpeculation);
            JSValueRegs valueRegs = value.jsValueRegs();

            load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
            Jump slowPath = branch32(AboveOrEqual, storageLengthGPR, Address(storageGPR, Butterfly::offsetOfVectorLength()));
            storeValue(valueRegs, BaseIndex(storageGPR, storageLengthGPR, TimesEight));
            add32(TrustedImm32(1), storageLengthGPR);
            store32(storageLengthGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));
            boxInt32(storageLengthGPR, resultRegs);

            addSlowPathGenerator(
                slowPathCall(slowPath, this, operationArrayPush, resultRegs, LinkableConstant::globalObject(*this, node), valueRegs, baseGPR));

            jsValueResult(resultRegs, node);
            return;
        }

        if (node->arrayMode().type() == Array::Int32) {
            for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
                Edge element = m_graph.varArgChild(node, elementIndex + elementOffset);
                ASSERT(element.useKind() == Int32Use);
                speculateInt32(element);
            }
        }

        GPRTemporary buffer(this);
        GPRReg bufferGPR = buffer.gpr();

        load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
        move(storageLengthGPR, bufferGPR);
        add32(TrustedImm32(elementCount), bufferGPR);
        Jump slowPath = branch32(Above, bufferGPR, Address(storageGPR, Butterfly::offsetOfVectorLength()));

        store32(bufferGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));
        getStorageBufferAddress(storageGPR, storageLengthGPR, 0, bufferGPR);
        add32(TrustedImm32(elementCount), storageLengthGPR);
        boxInt32(storageLengthGPR, resultRegs);
        auto storageDone = jump();

        slowPath.link(this);

        size_t scratchSize = sizeof(EncodedJSValue) * elementCount;
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        move(TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())), bufferGPR);

        storageDone.link(this);
        for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Edge& element = m_graph.varArgChild(node, elementIndex + elementOffset);
            JSValueOperand value(this, element, ManualOperandSpeculation); // We did type checks above.
            JSValueRegs valueRegs = value.jsValueRegs();

            storeValue(valueRegs, Address(bufferGPR, sizeof(EncodedJSValue) * elementIndex));
            value.use();
        }

        Jump fastPath = branchPtr(NotEqual, bufferGPR, TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())));

        addSlowPathGenerator(slowPathCall(jump(), this, operationArrayPushMultiple, resultRegs, LinkableConstant::globalObject(*this, node), baseGPR, bufferGPR, TrustedImm32(elementCount)));

        base.use();
        storage.use();

        fastPath.link(this);
        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        return;
    }

    case Array::Double: {
        if (elementCount == 1) {
            Edge& element = m_graph.varArgChild(node, elementOffset);
            speculate(node, element);
            SpeculateDoubleOperand value(this, element);
            FPRReg valueFPR = value.fpr();

            load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
            Jump slowPath = branch32(AboveOrEqual, storageLengthGPR, Address(storageGPR, Butterfly::offsetOfVectorLength()));
            storeDouble(valueFPR, BaseIndex(storageGPR, storageLengthGPR, TimesEight));
            add32(TrustedImm32(1), storageLengthGPR);
            store32(storageLengthGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));
            boxInt32(storageLengthGPR, resultRegs);

            addSlowPathGenerator(
                slowPathCall(slowPath, this, operationArrayPushDouble, resultRegs, LinkableConstant::globalObject(*this, node), valueFPR, baseGPR));

            jsValueResult(resultRegs, node);
            return;
        }

        for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Edge element = m_graph.varArgChild(node, elementIndex + elementOffset);
            ASSERT(element.useKind() == DoubleRepRealUse);
            speculate(node, element);
        }

        GPRTemporary buffer(this);
        GPRReg bufferGPR = buffer.gpr();

        load32(Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
        move(storageLengthGPR, bufferGPR);
        add32(TrustedImm32(elementCount), bufferGPR);
        Jump slowPath = branch32(Above, bufferGPR, Address(storageGPR, Butterfly::offsetOfVectorLength()));

        store32(bufferGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));
        getStorageBufferAddress(storageGPR, storageLengthGPR, 0, bufferGPR);
        add32(TrustedImm32(elementCount), storageLengthGPR);
        boxInt32(storageLengthGPR, resultRegs);
        auto storageDone = jump();

        slowPath.link(this);

        size_t scratchSize = sizeof(double) * elementCount;
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        move(TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())), bufferGPR);

        storageDone.link(this);
        for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Edge& element = m_graph.varArgChild(node, elementIndex + elementOffset);
            SpeculateDoubleOperand value(this, element);
            FPRReg valueFPR = value.fpr();

            storeDouble(valueFPR, Address(bufferGPR, sizeof(double) * elementIndex));
            value.use();
        }

        Jump fastPath = branchPtr(NotEqual, bufferGPR, TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())));

        addSlowPathGenerator(slowPathCall(jump(), this, operationArrayPushDoubleMultiple, resultRegs, LinkableConstant::globalObject(*this, node), baseGPR, bufferGPR, TrustedImm32(elementCount)));

        base.use();
        storage.use();

        fastPath.link(this);
        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        return;
    }

    case Array::ArrayStorage: {
        // This ensures that the result of ArrayPush is Int32 in AI.
        int32_t largestPositiveInt32Length = 0x7fffffff - elementCount;
        if (elementCount == 1) {
            Edge& element = m_graph.varArgChild(node, elementOffset);
            JSValueOperand value(this, element);
            JSValueRegs valueRegs = value.jsValueRegs();

            load32(Address(storageGPR, ArrayStorage::lengthOffset()), storageLengthGPR);

            // Refuse to handle bizarre lengths.
            speculationCheck(Uncountable, JSValueRegs(), nullptr, branch32(Above, storageLengthGPR, TrustedImm32(largestPositiveInt32Length)));

            Jump slowPath = branch32(AboveOrEqual, storageLengthGPR, Address(storageGPR, ArrayStorage::vectorLengthOffset()));

            storeValue(valueRegs, BaseIndex(storageGPR, storageLengthGPR, TimesEight, ArrayStorage::vectorOffset()));

            add32(TrustedImm32(1), storageLengthGPR);
            store32(storageLengthGPR, Address(storageGPR, ArrayStorage::lengthOffset()));
            add32(TrustedImm32(1), Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
            boxInt32(storageLengthGPR, resultRegs);

            addSlowPathGenerator(
                slowPathCall(slowPath, this, operationArrayPush, resultRegs, LinkableConstant::globalObject(*this, node), valueRegs, baseGPR));

            jsValueResult(resultRegs, node);
            return;
        }

        GPRTemporary buffer(this);
        GPRReg bufferGPR = buffer.gpr();

        load32(Address(storageGPR, ArrayStorage::lengthOffset()), storageLengthGPR);

        // Refuse to handle bizarre lengths.
        speculationCheck(Uncountable, JSValueRegs(), nullptr, branch32(Above, storageLengthGPR, TrustedImm32(largestPositiveInt32Length)));

        move(storageLengthGPR, bufferGPR);
        add32(TrustedImm32(elementCount), bufferGPR);
        Jump slowPath = branch32(Above, bufferGPR, Address(storageGPR, ArrayStorage::vectorLengthOffset()));

        store32(bufferGPR, Address(storageGPR, ArrayStorage::lengthOffset()));
        getStorageBufferAddress(storageGPR, storageLengthGPR, ArrayStorage::vectorOffset(), bufferGPR);
        add32(TrustedImm32(elementCount), Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        add32(TrustedImm32(elementCount), storageLengthGPR);
        boxInt32(storageLengthGPR, resultRegs);
        auto storageDone = jump();

        slowPath.link(this);

        size_t scratchSize = sizeof(EncodedJSValue) * elementCount;
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        move(TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())), bufferGPR);

        storageDone.link(this);
        for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Edge& element = m_graph.varArgChild(node, elementIndex + elementOffset);
            JSValueOperand value(this, element);
            JSValueRegs valueRegs = value.jsValueRegs();

            storeValue(valueRegs, Address(bufferGPR, sizeof(EncodedJSValue) * elementIndex));
            value.use();
        }

        Jump fastPath = branchPtr(NotEqual, bufferGPR, TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())));

        addSlowPathGenerator(
            slowPathCall(jump(), this, operationArrayPushMultiple, resultRegs, LinkableConstant::globalObject(*this, node), baseGPR, bufferGPR, TrustedImm32(elementCount)));

        base.use();
        storage.use();

        fastPath.link(this);
        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        return;
    }

    case Array::SlowPutArrayStorage: {
        GPRTemporary buffer(this);
        GPRReg bufferGPR = buffer.gpr();

        size_t scratchSize = sizeof(EncodedJSValue) * elementCount;
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        move(TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())), bufferGPR);

        for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Edge& element = m_graph.varArgChild(node, elementIndex + elementOffset);
            JSValueOperand value(this, element);
            JSValueRegs valueRegs = value.jsValueRegs();
            storeValue(valueRegs, Address(bufferGPR, sizeof(EncodedJSValue) * elementIndex));
            value.use();
        }
        base.use();
        storage.use();

        flushRegisters();
        callOperation(operationArrayPushMultipleSlow, resultRegs, LinkableConstant::globalObject(*this, node), baseGPR, bufferGPR, TrustedImm32(elementCount));

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        return;
    }

    case Array::ForceExit:
        DFG_CRASH(m_graph, node, "Bad array mode type");
        break;

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileNotifyWrite(Node* node)
{
    WatchpointSet* set = node->watchpointSet();
    
    Jump slowCase = branch8(
        NotEqual,
        AbsoluteAddress(set->addressOfState()),
        TrustedImm32(IsInvalidated));
    
    addSlowPathGenerator(
        slowPathCall(slowCase, this, operationNotifyWrite, NeedToSpill, ExceptionCheckRequirement::CheckNotNeeded, NoResult, TrustedImmPtr(&vm()), TrustedImmPtr(set)));
    
    noResult(node);
}

void SpeculativeJIT::compileIsObject(Node* node)
{
    JSValueOperand value(this, node->child1());
    GPRTemporary result(this, Reuse, value, TagWord);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    Jump isNotCell = branchIfNotCell(valueRegs);

    compare8(AboveOrEqual,
        Address(valueRegs.payloadGPR(), JSCell::typeInfoTypeOffset()),
        TrustedImm32(ObjectType),
        resultGPR);
    Jump done = jump();

    isNotCell.link(this);
    move(TrustedImm32(0), resultGPR);

    done.link(this);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileTypeOfIsObject(Node* node)
{
    JSValueOperand value(this, node->child1());
    JSValueRegs valueRegs = value.jsValueRegs();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    Jump isCell = branchIfCell(valueRegs);
    
    Jump isNull = branchIfEqual(valueRegs, jsNull());
    Jump isNonNullNonCell = jump();
    
    isCell.link(this);
    Jump isFunction = branchIfFunction(valueRegs.payloadGPR());
    Jump notObject = branchIfNotObject(valueRegs.payloadGPR());
    
    Jump slowPath = branchTest8(
        NonZero,
        Address(valueRegs.payloadGPR(), JSCell::typeInfoFlagsOffset()),
        TrustedImm32(MasqueradesAsUndefined | OverridesGetCallData));
    
    isNull.link(this);
    move(TrustedImm32(1), resultGPR);
    Jump done = jump();
    
    isNonNullNonCell.link(this);
    isFunction.link(this);
    notObject.link(this);
    move(TrustedImm32(0), resultGPR);
    
    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationTypeOfIsObject, resultGPR, LinkableConstant::globalObject(*this, node),
            valueRegs.payloadGPR()));
    
    done.link(this);
    
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileIsCallable(Node* node, S_JITOperation_GC slowPathOperation)
{
    JSValueOperand value(this, node->child1());
    JSValueRegs valueRegs = value.jsValueRegs();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    Jump notCell = branchIfNotCell(valueRegs);
    Jump isFunction = branchIfFunction(valueRegs.payloadGPR());
    Jump notObject = branchIfNotObject(valueRegs.payloadGPR());
    
    Jump slowPath = branchTest8(
        NonZero,
        Address(valueRegs.payloadGPR(), JSCell::typeInfoFlagsOffset()),
        TrustedImm32(MasqueradesAsUndefined | OverridesGetCallData));
    
    notCell.link(this);
    notObject.link(this);
    move(TrustedImm32(0), resultGPR);
    Jump done = jump();
    
    isFunction.link(this);
    move(TrustedImm32(1), resultGPR);
    
    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, slowPathOperation, resultGPR, LinkableConstant::globalObject(*this, node),
            valueRegs.payloadGPR()));
    
    done.link(this);
    
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileIsConstructor(Node* node)
{
    JSValueOperand input(this, node->child1());
    JSValueRegs inputRegs = input.jsValueRegs();
    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    callOperationWithoutExceptionCheck(operationIsConstructor, resultGPR, LinkableConstant::globalObject(*this, node), inputRegs);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileTypeOf(Node* node)
{
    JSValueOperand value(this, node->child1());
    JSValueRegs valueRegs = value.jsValueRegs();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    JumpList done;
    Jump slowPath;
    emitTypeOf(
        valueRegs, resultGPR,
        [&] (TypeofType type, bool fallsThrough) {
            loadLinkableConstant(LinkableConstant(*this, vm().smallStrings.typeString(type)), resultGPR);
            if (!fallsThrough)
                done.append(jump());
        },
        [&] (Jump theSlowPath) {
            slowPath = theSlowPath;
        });
    done.link(this);

    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationTypeOfObject, resultGPR, LinkableConstant::globalObject(*this, node),
            valueRegs.payloadGPR()));
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::emitStructureCheck(Node* node, GPRReg cellGPR, GPRReg tempGPR)
{
    ASSERT(node->structureSet().size());
    
    if (node->structureSet().size() == 1) {
        speculationCheck(
            BadCache, JSValueSource::unboxedCell(cellGPR), nullptr,
            branchWeakStructure(
                NotEqual,
                Address(cellGPR, JSCell::structureIDOffset()),
                node->structureSet()[0]));
    } else {
        std::unique_ptr<GPRTemporary> structure;
        GPRReg structureGPR;
        
        if (tempGPR == InvalidGPRReg) {
            structure = makeUnique<GPRTemporary>(this);
            structureGPR = structure->gpr();
        } else
            structureGPR = tempGPR;
        
        load32(Address(cellGPR, JSCell::structureIDOffset()), structureGPR);
        
        JumpList done;
        
        for (size_t i = 0; i < node->structureSet().size() - 1; ++i) {
            done.append(
                branchWeakStructure(Equal, structureGPR, node->structureSet()[i]));
        }
        
        speculationCheck(
            BadCache, JSValueSource::unboxedCell(cellGPR), nullptr,
            branchWeakStructure(
                NotEqual, structureGPR, node->structureSet().last()));
        
        done.link(this);
    }
}

void SpeculativeJIT::compileCheckIsConstant(Node* node)
{
    if (node->child1().useKind() == CellUse) {
        SpeculateCellOperand cell(this, node->child1());
        speculationCheck(BadConstantValue, JSValueSource::unboxedCell(cell.gpr()), node->child1(), branchLinkableConstant(NotEqual, cell.gpr(), LinkableConstant(*this, node->cellOperand()->cell())));
    } else {
        ASSERT(!node->constant()->value().isCell() || !node->constant()->value());
        JSValueOperand operand(this, node->child1());
        JSValueRegs regs = operand.jsValueRegs();

#if USE(JSVALUE64)
        speculationCheck(BadConstantValue, regs, node->child1(), branch64(NotEqual, regs.gpr(), TrustedImm64(JSValue::encode(node->constant()->value()))));
#else
        speculationCheck(BadConstantValue, regs, node->child1(), branch32(NotEqual, regs.tagGPR(), TrustedImm32(node->constant()->value().tag())));
        speculationCheck(BadConstantValue, regs, node->child1(), branch32(NotEqual, regs.payloadGPR(), TrustedImm32(node->constant()->value().payload())));
#endif
    }


    noResult(node);
}

void SpeculativeJIT::compileCheckNotEmpty(Node* node)
{
    JSValueOperand operand(this, node->child1());
    JSValueRegs regs = operand.jsValueRegs();
    speculationCheck(TDZFailure, JSValueSource(), nullptr, branchIfEmpty(regs));
    noResult(node);
}

void SpeculativeJIT::compileCheckStructure(Node* node)
{
    switch (node->child1().useKind()) {
    case CellUse:
    case KnownCellUse: {
        SpeculateCellOperand cell(this, node->child1());
        emitStructureCheck(node, cell.gpr(), InvalidGPRReg);
        noResult(node);
        return;
    }

    case CellOrOtherUse: {
        JSValueOperand value(this, node->child1(), ManualOperandSpeculation);
        GPRTemporary temp(this);

        JSValueRegs valueRegs = value.jsValueRegs();
        GPRReg tempGPR = temp.gpr();

        Jump cell = branchIfCell(valueRegs);
        DFG_TYPE_CHECK(
            valueRegs, node->child1(), SpecCell | SpecOther,
            branchIfNotOther(valueRegs, tempGPR));
        Jump done = jump();
        cell.link(this);
        emitStructureCheck(node, valueRegs.payloadGPR(), tempGPR);
        done.link(this);
        noResult(node);
        return;
    }

    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        return;
    }
}

void SpeculativeJIT::compileAllocatePropertyStorage(Node* node)
{
    ASSERT(!node->transition()->previous->outOfLineCapacity());
    ASSERT(initialOutOfLineCapacity == node->transition()->next->outOfLineCapacity());
    
    size_t size = initialOutOfLineCapacity * sizeof(JSValue);

    Allocator allocator = vm().auxiliarySpace().allocatorFor(size, AllocatorForMode::AllocatorIfExists);

    if (!allocator || node->transition()->previous->couldHaveIndexingHeader()) {
        SpeculateCellOperand base(this, node->child1());
        
        GPRReg baseGPR = base.gpr();
        
        flushRegisters();

        GPRFlushedCallResult result(this);
        callOperation(operationAllocateComplexPropertyStorageWithInitialCapacity, result.gpr(), TrustedImmPtr(&vm()), baseGPR);
        
        storageResult(result.gpr(), node);
        return;
    }
    
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
        
    GPRReg scratchGPR1 = scratch1.gpr();
    GPRReg scratchGPR2 = scratch2.gpr();
    GPRReg scratchGPR3 = scratch3.gpr();
        
    JumpList slowPath;
    emitAllocate(scratchGPR1, JITAllocator::constant(allocator), scratchGPR2, scratchGPR3, slowPath, SlowAllocationResult::UndefinedBehavior);
    addPtr(TrustedImm32(size + sizeof(IndexingHeader)), scratchGPR1);

    addSlowPathGenerator(
        slowPathCall(slowPath, this, operationAllocateSimplePropertyStorageWithInitialCapacity, scratchGPR1, TrustedImmPtr(&vm())));

    for (ptrdiff_t offset = 0; offset < static_cast<ptrdiff_t>(size); offset += sizeof(void*))
        storePtr(TrustedImmPtr(nullptr), Address(scratchGPR1, -(offset + sizeof(JSValue) + sizeof(void*))));

    storageResult(scratchGPR1, node);
}

void SpeculativeJIT::compileReallocatePropertyStorage(Node* node)
{
    size_t oldSize = node->transition()->previous->outOfLineCapacity() * sizeof(JSValue);
    size_t newSize = oldSize * outOfLineGrowthFactor;
    ASSERT(newSize == node->transition()->next->outOfLineCapacity() * sizeof(JSValue));
    
    Allocator allocator = vm().auxiliarySpace().allocatorFor(newSize, AllocatorForMode::AllocatorIfExists);

    if (!allocator || node->transition()->previous->couldHaveIndexingHeader()) {
        SpeculateCellOperand base(this, node->child1());
        
        GPRReg baseGPR = base.gpr();
        
        flushRegisters();

        GPRFlushedCallResult result(this);
        callOperation(operationAllocateComplexPropertyStorage, result.gpr(), TrustedImmPtr(&vm()), baseGPR, newSize / sizeof(JSValue));

        storageResult(result.gpr(), node);
        return;
    }
    
    StorageOperand oldStorage(this, node->child2());
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);

    GPRReg oldStorageGPR = oldStorage.gpr();
    GPRReg scratchGPR1 = scratch1.gpr();
    GPRReg scratchGPR2 = scratch2.gpr();
    GPRReg scratchGPR3 = scratch3.gpr();
    
    JumpList slowPath;
    emitAllocate(scratchGPR1, JITAllocator::constant(allocator), scratchGPR2, scratchGPR3, slowPath, SlowAllocationResult::UndefinedBehavior);
    
    addPtr(TrustedImm32(newSize + sizeof(IndexingHeader)), scratchGPR1);

    addSlowPathGenerator(
        slowPathCall(slowPath, this, operationAllocateSimplePropertyStorage, scratchGPR1, TrustedImmPtr(&vm()), newSize / sizeof(JSValue)));

    for (ptrdiff_t offset = oldSize; offset < static_cast<ptrdiff_t>(newSize); offset += sizeof(void*))
        storePtr(TrustedImmPtr(nullptr), Address(scratchGPR1, -(offset + sizeof(JSValue) + sizeof(void*))));

    // We have scratchGPR1 = new storage, scratchGPR2 = scratch
    for (ptrdiff_t offset = 0; offset < static_cast<ptrdiff_t>(oldSize); offset += sizeof(void*)) {
        loadPtr(Address(oldStorageGPR, -(offset + sizeof(JSValue) + sizeof(void*))), scratchGPR2);
        storePtr(scratchGPR2, Address(scratchGPR1, -(offset + sizeof(JSValue) + sizeof(void*))));
    }

    storageResult(scratchGPR1, node);
}

void SpeculativeJIT::compileNukeStructureAndSetButterfly(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    StorageOperand storage(this, node->child2());

    GPRReg baseGPR = base.gpr();
    GPRReg storageGPR = storage.gpr();

    nukeStructureAndStoreButterfly(vm(), storageGPR, baseGPR);
    
    noResult(node);
}

void SpeculativeJIT::compileGetButterfly(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary result(this, Reuse, base);
    
    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();
    
    loadPtr(Address(baseGPR, JSObject::butterflyOffset()), resultGPR);

    storageResult(resultGPR, node);
}

static void allocateTemporaryRegistersForSnippet(SpeculativeJIT* jit, Vector<GPRTemporary>& gpHolders, Vector<FPRTemporary>& fpHolders, Vector<GPRReg>& gpScratch, Vector<FPRReg>& fpScratch, Snippet& snippet)
{
    for (unsigned i = 0; i < snippet.numGPScratchRegisters; ++i) {
        GPRTemporary temporary(jit);
        gpScratch.append(temporary.gpr());
        gpHolders.append(WTFMove(temporary));
    }

    for (unsigned i = 0; i < snippet.numFPScratchRegisters; ++i) {
        FPRTemporary temporary(jit);
        fpScratch.append(temporary.fpr());
        fpHolders.append(WTFMove(temporary));
    }
}

void SpeculativeJIT::compileCallDOM(Node* node)
{
    const DOMJIT::Signature* signature = node->signature();

    // FIXME: We should have a way to call functions with the vector of registers.
    // https://bugs.webkit.org/show_bug.cgi?id=163099
    using OperandVariant = std::variant<SpeculateCellOperand, SpeculateInt32Operand, SpeculateBooleanOperand>;
    Vector<OperandVariant, JSC_DOMJIT_SIGNATURE_MAX_ARGUMENTS_INCLUDING_THIS> operands;
    Vector<GPRReg, JSC_DOMJIT_SIGNATURE_MAX_ARGUMENTS_INCLUDING_THIS> regs;

    auto appendCell = [&](Edge& edge) {
        SpeculateCellOperand operand(this, edge);
        regs.append(operand.gpr());
        operands.append(OperandVariant(std::in_place_type<SpeculateCellOperand>, WTFMove(operand)));
    };

    auto appendString = [&](Edge& edge) {
        SpeculateCellOperand operand(this, edge);
        GPRReg gpr = operand.gpr();
        regs.append(gpr);
        speculateString(edge, gpr);
        operands.append(OperandVariant(std::in_place_type<SpeculateCellOperand>, WTFMove(operand)));
    };

    auto appendInt32 = [&](Edge& edge) {
        SpeculateInt32Operand operand(this, edge);
        regs.append(operand.gpr());
        operands.append(OperandVariant(std::in_place_type<SpeculateInt32Operand>, WTFMove(operand)));
    };

    auto appendBoolean = [&](Edge& edge) {
        SpeculateBooleanOperand operand(this, edge);
        regs.append(operand.gpr());
        operands.append(OperandVariant(std::in_place_type<SpeculateBooleanOperand>, WTFMove(operand)));
    };

    unsigned index = 0;
    m_graph.doToChildren(node, [&](Edge edge) {
        if (!index)
            appendCell(edge);
        else {
            switch (signature->arguments[index - 1]) {
            case SpecString:
                appendString(edge);
                break;
            case SpecInt32Only:
                appendInt32(edge);
                break;
            case SpecBoolean:
                appendBoolean(edge);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
        ++index;
    });

    JSValueRegsTemporary result(this);
    JSValueRegs resultRegs = result.regs();

    flushRegisters();

    // FIXME: Revisit JSGlobalObject.
    // https://bugs.webkit.org/show_bug.cgi?id=203204
    auto function = signature->functionWithoutTypeCheck();
    unsigned argumentCountIncludingThis = signature->argumentCount + 1;
    switch (argumentCountIncludingThis) {
    case 1:
        callOperation(reinterpret_cast<J_JITOperation_GP>(function.untypedFunc()), extractResult(resultRegs), LinkableConstant::globalObject(*this, node), regs[0]);
        break;
    case 2:
        callOperation(reinterpret_cast<J_JITOperation_GPP>(function.untypedFunc()), extractResult(resultRegs), LinkableConstant::globalObject(*this, node), regs[0], regs[1]);
        break;
    case 3:
        callOperation(reinterpret_cast<J_JITOperation_GPPP>(function.untypedFunc()), extractResult(resultRegs), LinkableConstant::globalObject(*this, node), regs[0], regs[1], regs[2]);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileCallDOMGetter(Node* node)
{
    DOMJIT::CallDOMGetterSnippet* snippet = node->callDOMGetterData()->snippet;
    if (!snippet) {
        CodePtr<CustomAccessorPtrTag> getter = node->callDOMGetterData()->customAccessorGetter;
        SpeculateCellOperand base(this, node->child1());
        JSValueRegsTemporary result(this);

        JSValueRegs resultRegs = result.regs();
        GPRReg baseGPR = base.gpr();

        flushRegisters();

        storePtr(GPRInfo::callFrameRegister, &vm().topCallFrame);
        emitStoreCodeOrigin(m_currentNode->origin.semantic);
        if (Options::useJITCage())
            callOperation(vmEntryCustomGetter, resultRegs, LinkableConstant::globalObject(*this, node), CellValue(baseGPR), TrustedImmPtr(identifierUID(node->callDOMGetterData()->identifierNumber)), TrustedImmPtr(getter.taggedPtr()));
        else {
            CodePtr<OperationPtrTag> bypassedFunction(WTF::tagNativeCodePtrImpl<OperationPtrTag>(WTF::untagNativeCodePtrImpl<CustomAccessorPtrTag>(getter.taggedPtr())));
            callOperation<J_JITOperation_GJI>(bypassedFunction, resultRegs, LinkableConstant::globalObject(*this, node), CellValue(baseGPR), TrustedImmPtr(identifierUID(node->callDOMGetterData()->identifierNumber)));
        }

        jsValueResult(resultRegs, node);
        return;
    }

    Vector<GPRReg> gpScratch;
    Vector<FPRReg> fpScratch;
    Vector<SnippetParams::Value> regs;

    JSValueRegsTemporary result(this);
    regs.append(result.regs());

    Edge& baseEdge = node->child1();
    SpeculateCellOperand base(this, baseEdge);
    regs.append(SnippetParams::Value(base.gpr(), m_state.forNode(baseEdge).value()));

    std::optional<SpeculateCellOperand> globalObject;
    if (snippet->requireGlobalObject) {
        Edge& globalObjectEdge = node->child2();
        globalObject.emplace(this, globalObjectEdge);
        regs.append(SnippetParams::Value(globalObject->gpr(), m_state.forNode(globalObjectEdge).value()));
    }

    Vector<GPRTemporary> gpTempraries;
    Vector<FPRTemporary> fpTempraries;
    allocateTemporaryRegistersForSnippet(this, gpTempraries, fpTempraries, gpScratch, fpScratch, *snippet);
    SnippetParams params(this, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
    snippet->generator()->run(*this, params);
    jsValueResult(result.regs(), node);
}

void SpeculativeJIT::compileCheckJSCast(Node* node)
{
    DFG_ASSERT(m_graph, node, node->op() == CheckJSCast || node->op() == CheckNotJSCast);
    const ClassInfo* classInfo = node->classInfo();
    if (classInfo->inheritsJSTypeRange) {
        SpeculateCellOperand base(this, node->child1());
        GPRReg baseGPR = base.gpr();

        Jump checkFailed;
        if (node->op() == CheckJSCast)
            checkFailed = branchIfNotType(baseGPR, classInfo->inheritsJSTypeRange.value());
        else
            checkFailed = branchIfType(baseGPR, classInfo->inheritsJSTypeRange.value());
        speculationCheck(BadType, JSValueSource::unboxedCell(baseGPR), node->child1(), checkFailed);
        noResult(node);
        return;
    }

    if (!classInfo->checkSubClassSnippet) {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary other(this);
        GPRTemporary specified(this);

        GPRReg baseGPR = base.gpr();
        GPRReg otherGPR = other.gpr();
        GPRReg specifiedGPR = specified.gpr();

        emitLoadStructure(vm(), baseGPR, otherGPR);
        loadCompactPtr(Address(otherGPR, Structure::classInfoOffset()), otherGPR);
        move(TrustedImmPtr(node->classInfo()), specifiedGPR);

        Label loop = label();
        auto found = branchPtr(Equal, otherGPR, specifiedGPR);
        loadPtr(Address(otherGPR, ClassInfo::offsetOfParentClass()), otherGPR);
        branchTestPtr(NonZero, otherGPR).linkTo(loop, this);
        if (node->op() == CheckJSCast) {
            speculationCheck(BadType, JSValueSource::unboxedCell(baseGPR), node->child1(), jump());
            found.link(this);
        } else {
            auto notFound = jump();
            speculationCheck(BadType, JSValueSource::unboxedCell(baseGPR), node->child1(), found);
            notFound.link(this);
        }
        noResult(node);
        return;
    }

    Ref<Snippet> snippet = classInfo->checkSubClassSnippet();

    Vector<GPRReg> gpScratch;
    Vector<FPRReg> fpScratch;
    Vector<SnippetParams::Value> regs;

    SpeculateCellOperand base(this, node->child1());
    GPRReg baseGPR = base.gpr();
    regs.append(SnippetParams::Value(baseGPR, m_state.forNode(node->child1()).value()));

    Vector<GPRTemporary> gpTempraries;
    Vector<FPRTemporary> fpTempraries;
    allocateTemporaryRegistersForSnippet(this, gpTempraries, fpTempraries, gpScratch, fpScratch, snippet.get());

    SnippetParams params(this, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
    JumpList failureCases = snippet->generator()->run(*this, params);
    if (node->op() == CheckJSCast)
        speculationCheck(BadType, JSValueSource::unboxedCell(baseGPR), node->child1(), failureCases);
    else {
        speculationCheck(BadType, JSValueSource::unboxedCell(baseGPR), node->child1(), jump());
        failureCases.link(this);
    }
    noResult(node);
}

void SpeculativeJIT::compileCallCustomAccessorGetter(Node* node)
{
    auto getter = node->customAccessor();
    UniquedStringImpl* uid = node->cacheableIdentifier().uid();

    JSValueOperand base(this, node->child1());

    JSValueRegs baseRegs = base.jsValueRegs();

    flushRegisters();

    storePtr(GPRInfo::callFrameRegister, &vm().topCallFrame);
    emitStoreCodeOrigin(m_currentNode->origin.semantic);

    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();

    if (Options::useJITCage())
        callOperation(vmEntryCustomGetter, resultRegs, LinkableConstant::globalObject(*this, node), baseRegs, TrustedImmPtr(uid), TrustedImmPtr(getter.taggedPtr()));
    else {
        CodePtr<OperationPtrTag> bypassedFunction(WTF::tagNativeCodePtrImpl<OperationPtrTag>(WTF::untagNativeCodePtrImpl<CustomAccessorPtrTag>(getter.taggedPtr())));
        callOperation<GetValueFunc>(bypassedFunction, resultRegs, LinkableConstant::globalObject(*this, node), baseRegs, TrustedImmPtr(uid));
    }

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileCallCustomAccessorSetter(Node* node)
{
    auto setter = node->customAccessor();
    UniquedStringImpl* uid = node->cacheableIdentifier().uid();

    JSValueOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());

    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueRegs valueRegs = value.jsValueRegs();

    flushRegisters();

    storePtr(GPRInfo::callFrameRegister, &vm().topCallFrame);
    emitStoreCodeOrigin(m_currentNode->origin.semantic);

    if (Options::useJITCage())
        callOperation(vmEntryCustomSetter, LinkableConstant::globalObject(*this, node), baseRegs, valueRegs, TrustedImmPtr(uid), TrustedImmPtr(setter.taggedPtr()));
    else {
        // We can't use callOperation here because PutValueFunc returns a bool but we don't pass that result to JS.
        setupArguments<PutValueFunc>(LinkableConstant::globalObject(*this, node), baseRegs, valueRegs, TrustedImmPtr(uid));
        CodePtr<OperationPtrTag> bypassedFunction(WTF::tagNativeCodePtrImpl<OperationPtrTag>(WTF::untagNativeCodePtrImpl<CustomAccessorPtrTag>(setter.taggedPtr())));
        appendOperationCall(bypassedFunction);
        operationExceptionCheck<PutValueFunc>();
    }
    noResult(node);
}

GPRReg SpeculativeJIT::temporaryRegisterForPutByVal(GPRTemporary& temporary, ArrayMode arrayMode)
{
    if (!putByValWillNeedExtraRegister(arrayMode))
        return InvalidGPRReg;
    
    GPRTemporary realTemporary(this);
    temporary.adopt(realTemporary);
    return temporary.gpr();
}

void SpeculativeJIT::compileToStringOrCallStringConstructorOrStringValueOf(Node* node)
{
    ASSERT(node->op() != StringValueOf || node->child1().useKind() == UntypedUse);
    switch (node->child1().useKind()) {
    case NotCellUse: {
        JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
        JSValueRegs op1Regs = op1.jsValueRegs();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        speculateNotCell(node->child1(), op1Regs);

        flushRegisters();

        if (node->op() == ToString)
            callOperation(operationToString, resultGPR, LinkableConstant::globalObject(*this, node), op1Regs);
        else {
            ASSERT(node->op() == CallStringConstructor);
            callOperation(operationCallStringConstructor, resultGPR, LinkableConstant::globalObject(*this, node), op1Regs);
        }
        cellResult(resultGPR, node);
        return;
    }

    case StringOrOtherUse: {
        JSValueOperand arg(this, node->child1(), ManualOperandSpeculation);
        GPRTemporary result(this);

        JSValueRegs argRegs = arg.jsValueRegs();
        GPRReg resultGPR = result.gpr();

        Edge& edge = node->child1();
        JumpList doneCases;

        auto notCell = branchIfNotCell(argRegs);
        GPRReg cell = argRegs.payloadGPR();
        DFG_TYPE_CHECK(argRegs, edge, (~SpecCellCheck) | SpecString, branchIfNotString(cell));
        move(cell, resultGPR);
        doneCases.append(jump());

        notCell.link(this);
        auto isUndefined = branchIfUndefined(argRegs);
        auto isNull = branchIfNull(argRegs);
        DFG_TYPE_CHECK(argRegs, edge, SpecCellCheck | SpecOther, jump());

        isUndefined.link(this);
        loadLinkableConstant(LinkableConstant(*this, vm().smallStrings.undefinedString()), resultGPR);
        doneCases.append(jump());

        isNull.link(this);
        loadLinkableConstant(LinkableConstant(*this, vm().smallStrings.nullString()), resultGPR);

        doneCases.link(this);
        cellResult(resultGPR, node);
        return;
    }

    case KnownPrimitiveUse:
    case UntypedUse: {
        JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
        GPRFlushedCallResult result(this);

        JSValueRegs op1Regs = op1.jsValueRegs();
        GPRReg op1PayloadGPR = op1Regs.payloadGPR();
        GPRReg resultGPR = result.gpr();

        speculate(node, node->child1());

        flushRegisters();

        Jump done;
        if (node->child1()->prediction() & SpecString) {
            Jump slowPath1 = branchIfNotCell(op1.jsValueRegs());
            Jump slowPath2 = branchIfNotString(op1PayloadGPR);
            move(op1PayloadGPR, resultGPR);
            done = jump();
            slowPath1.link(this);
            slowPath2.link(this);
        }
        if (node->op() == ToString)
            callOperation(operationToString, resultGPR, LinkableConstant::globalObject(*this, node), op1Regs);
        else if (node->op() == StringValueOf)
            callOperation(operationStringValueOf, resultGPR, LinkableConstant::globalObject(*this, node), op1Regs);
        else {
            ASSERT(node->op() == CallStringConstructor);
            callOperation(operationCallStringConstructor, resultGPR, LinkableConstant::globalObject(*this, node), op1Regs);
        }
        if (done.isSet())
            done.link(this);
        cellResult(resultGPR, node);
        return;
    }

    case Int32Use:
    case Int52RepUse:
    case DoubleRepUse:
        compileNumberToStringWithValidRadixConstant(node, 10);
        return;

    default:
        break;
    }

    SpeculateCellOperand op1(this, node->child1());
    GPRReg op1GPR = op1.gpr();
    
    switch (node->child1().useKind()) {
    case StringObjectUse: {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
        
        speculateStringObject(node->child1(), op1GPR);

        loadPtr(Address(op1GPR, JSWrapperObject::internalValueCellOffset()), resultGPR);
        cellResult(resultGPR, node);
        break;
    }
        
    case StringOrStringObjectUse: {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        load8(Address(op1GPR, JSCell::typeInfoTypeOffset()), resultGPR);
        Jump isString = branch32(Equal, resultGPR, TrustedImm32(StringType));

        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node->child1().node(), branch32(NotEqual, resultGPR, TrustedImm32(StringObjectType)));
        loadPtr(Address(op1GPR, JSWrapperObject::internalValueCellOffset()), resultGPR);
        Jump done = jump();

        isString.link(this);
        move(op1GPR, resultGPR);
        done.link(this);
        
        m_interpreter.filter(node->child1(), SpecString | SpecStringObject);
        
        cellResult(resultGPR, node);
        break;
    }
        
    case CellUse: {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        
        // We flush registers instead of silent spill/fill because in this mode we
        // believe that most likely the input is not a string, and we need to take
        // slow path.
        flushRegisters();
        Jump done;
        if (node->child1()->prediction() & SpecString) {
            Jump needCall = branchIfNotString(op1GPR);
            move(op1GPR, resultGPR);
            done = jump();
            needCall.link(this);
        }
        if (node->op() == ToString)
            callOperation(operationToStringOnCell, resultGPR, LinkableConstant::globalObject(*this, node), op1GPR);
        else {
            ASSERT(node->op() == CallStringConstructor);
            callOperation(operationCallStringConstructorOnCell, resultGPR, LinkableConstant::globalObject(*this, node), op1GPR);
        }
        if (done.isSet())
            done.link(this);
        cellResult(resultGPR, node);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static void getExecutable(JITCompiler& jit, GPRReg functionGPR, GPRReg resultGPR)
{
    jit.loadPtr(JITCompiler::Address(functionGPR, JSFunction::offsetOfExecutableOrRareData()), resultGPR);
    auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, resultGPR, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
    jit.loadPtr(CCallHelpers::Address(resultGPR, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), resultGPR);
    hasExecutable.link(&jit);
}

void SpeculativeJIT::compileFunctionToString(Node* node)
{
    SpeculateCellOperand function(this, node->child1());
    GPRTemporary executable(this);
    GPRTemporary result(this);
    JumpList slowCases;

    speculateFunction(node->child1(), function.gpr());

    emitLoadStructure(vm(), function.gpr(), result.gpr());
    loadCompactPtr(Address(result.gpr(), Structure::classInfoOffset()), result.gpr());
    static_assert(std::is_final_v<JSBoundFunction>, "We don't handle subclasses when comparing classInfo below");
    slowCases.append(branchPtr(Equal, result.gpr(), TrustedImmPtr(JSBoundFunction::info())));

    static_assert(std::is_final_v<JSRemoteFunction>, "We don't handle subclasses when comparing classInfo below");
    slowCases.append(branchPtr(Equal, result.gpr(), TrustedImmPtr(JSRemoteFunction::info())));

    getExecutable(*this, function.gpr(), executable.gpr());
    Jump isNativeExecutable = branch8(Equal, Address(executable.gpr(), JSCell::typeInfoTypeOffset()), TrustedImm32(NativeExecutableType));

    loadPtr(Address(executable.gpr(), FunctionExecutable::offsetOfRareData()), result.gpr());
    slowCases.append(branchTestPtr(Zero, result.gpr()));
    loadPtr(Address(result.gpr(), FunctionExecutable::RareData::offsetOfAsString()), result.gpr());
    Jump continuation = jump();

    isNativeExecutable.link(this);
    loadPtr(Address(executable.gpr(), NativeExecutable::offsetOfAsString()), result.gpr());

    continuation.link(this);
    slowCases.append(branchTestPtr(Zero, result.gpr()));

    addSlowPathGenerator(slowPathCall(slowCases, this, operationFunctionToString, result.gpr(), LinkableConstant::globalObject(*this, node), function.gpr()));

    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileNumberToStringWithValidRadixConstant(Node* node)
{
    compileNumberToStringWithValidRadixConstant(node, node->validRadixConstant());
}

void SpeculativeJIT::compileNumberToStringWithValidRadixConstant(Node* node, int32_t radix)
{
    auto callToString = [&] (auto operation, GPRReg resultGPR, auto valueReg) {
        flushRegisters();
        callOperation(operation, resultGPR, LinkableConstant::globalObject(*this, node), valueReg, TrustedImm32(radix));
        cellResult(resultGPR, node);
    };

    switch (node->child1().useKind()) {
    case Int32Use: {
        if (radix == 10) {
            SpeculateStrictInt32Operand value(this, node->child1());
            GPRTemporary result(this);

            GPRReg valueGPR = value.gpr();
            GPRReg resultGPR = result.gpr();

            flushRegisters();

            JumpList slowCases;
            JumpList doneCases;

            slowCases.append(branch32(AboveOrEqual, valueGPR, TrustedImm32(NumericStrings::cacheSize)));

            move(valueGPR, resultGPR);
            static_assert(hasOneBitSet(sizeof(NumericStrings::StringWithJSString)), "size should be a power of two.");
            lshiftPtr(TrustedImm32(WTF::fastLog2(static_cast<unsigned>(sizeof(NumericStrings::StringWithJSString)))), resultGPR);
            addPtr(TrustedImmPtr(vm().numericStrings.smallIntCache()), resultGPR);
            loadPtr(Address(resultGPR, NumericStrings::StringWithJSString::offsetOfJSString()), resultGPR);
            doneCases.append(branchTestPtr(NonZero, resultGPR));
            // Fall-through.

            slowCases.link(this);
            callOperation(operationInt32ToStringWithValidRadix, resultGPR, LinkableConstant::globalObject(*this, node), valueGPR, TrustedImm32(10));

            doneCases.link(this);
            cellResult(resultGPR, node);
            break;
        }

        SpeculateStrictInt32Operand value(this, node->child1());
        GPRFlushedCallResult result(this);
        callToString(operationInt32ToStringWithValidRadix, result.gpr(), value.gpr());
        break;
    }

#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand value(this, node->child1());
        GPRFlushedCallResult result(this);
        callToString(operationInt52ToStringWithValidRadix, result.gpr(), value.gpr());
        break;
    }
#endif

    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        GPRFlushedCallResult result(this);
        callToString(operationDoubleToStringWithValidRadix, result.gpr(), value.fpr());
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileNumberToStringWithRadix(Node* node)
{
    bool validRadixIsGuaranteed = false;
    if (node->child2()->isInt32Constant()) {
        int32_t radix = node->child2()->asInt32();
        if (radix >= 2 && radix <= 36)
            validRadixIsGuaranteed = true;
    }

    auto callToString = [&] (auto operation, GPRReg resultGPR, auto valueReg, GPRReg radixGPR) {
        flushRegisters();
        callOperation(operation, resultGPR, LinkableConstant::globalObject(*this, node), valueReg, radixGPR);
        cellResult(resultGPR, node);
    };

    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateStrictInt32Operand value(this, node->child1());
        SpeculateStrictInt32Operand radix(this, node->child2());
        GPRFlushedCallResult result(this);
        callToString(validRadixIsGuaranteed ? operationInt32ToStringWithValidRadix : operationInt32ToString, result.gpr(), value.gpr(), radix.gpr());
        break;
    }

#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand value(this, node->child1());
        SpeculateStrictInt32Operand radix(this, node->child2());
        GPRFlushedCallResult result(this);
        callToString(validRadixIsGuaranteed ? operationInt52ToStringWithValidRadix : operationInt52ToString, result.gpr(), value.gpr(), radix.gpr());
        break;
    }
#endif

    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        SpeculateStrictInt32Operand radix(this, node->child2());
        GPRFlushedCallResult result(this);
        callToString(validRadixIsGuaranteed ? operationDoubleToStringWithValidRadix : operationDoubleToString, result.gpr(), value.fpr(), radix.gpr());
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileNewStringObject(Node* node)
{
    SpeculateCellOperand operand(this, node->child1());
    
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);

    GPRReg operandGPR = operand.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    
    JumpList slowPath;

    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject<StringObject>(
        resultGPR, TrustedImmPtr(node->structure()), butterfly, scratch1GPR, scratch2GPR,
        slowPath, SlowAllocationResult::UndefinedBehavior);
    
    storeCell(operandGPR, Address(resultGPR, JSWrapperObject::internalValueOffset()));

    mutatorFence(vm());
    
    addSlowPathGenerator(slowPathCall(
        slowPath, this, operationNewStringObject, resultGPR, TrustedImmPtr(&vm()), operandGPR, node->structure()));
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewSymbol(Node* node)
{
    if (!node->child1()) {
        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationNewSymbol, resultGPR, TrustedImmPtr(&vm()));
        cellResult(resultGPR, node);
        return;
    }


    if (node->child1().useKind() == StringUse) {
        SpeculateCellOperand operand(this, node->child1());
        GPRReg stringGPR = operand.gpr();
        speculateString(node->child1(), stringGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationNewSymbolWithStringDescription, resultGPR, LinkableConstant::globalObject(*this, node), stringGPR);
        cellResult(resultGPR, node);
        return;
    }

    JSValueOperand operand(this, node->child1());
    JSValueRegs inputRegs = operand.jsValueRegs();
    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationNewSymbolWithDescription, resultGPR, LinkableConstant::globalObject(*this, node), inputRegs);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewMap(Node* node)
{
    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationNewMap, resultGPR, TrustedImmPtr(&vm()), node->structure());
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewSet(Node* node)
{
    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationNewSet, resultGPR, TrustedImmPtr(&vm()), node->structure());
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewTypedArrayWithSize(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    auto typedArrayType = node->typedArrayType();
    bool isResizableOrGrowableShared = false;
    RegisteredStructure structure = m_graph.registerStructure(globalObject->typedArrayStructureConcurrently(typedArrayType, isResizableOrGrowableShared));
    RELEASE_ASSERT(structure.get());

#if USE(LARGE_TYPED_ARRAYS)
    // The operations we call on the slow path expect a intptr_t, so int64_t on 64 bit platforms
    SpeculateInt32Operand size(this, node->child1());
    GPRTemporary scratch(this);
    GPRReg sizeGPR = size.gpr();
    GPRReg scratchGPR = scratch.gpr();
    signExtend32ToPtr(sizeGPR, scratchGPR);
    emitNewTypedArrayWithSizeInRegister(node, typedArrayType, structure, scratchGPR);
#else
    SpeculateInt32Operand size(this, node->child1());
    GPRReg sizeGPR = size.gpr();
    emitNewTypedArrayWithSizeInRegister(node, typedArrayType, structure, sizeGPR);
#endif
}

void SpeculativeJIT::emitNewTypedArrayWithSizeInRegister(Node* node, TypedArrayType typedArrayType, RegisteredStructure structure, GPRReg sizeGPR)
{
    GPRTemporary result(this);
    GPRTemporary storage(this);
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);
    GPRReg resultGPR = result.gpr();
    GPRReg storageGPR = storage.gpr();
    GPRReg scratchGPR = scratch.gpr();
    GPRReg scratchGPR2 = scratch2.gpr();
    
    JumpList slowCases;
    
    move(TrustedImmPtr(nullptr), storageGPR);

#if USE(LARGE_TYPED_ARRAYS)
    slowCases.append(branch64(
        Above, sizeGPR, TrustedImm64(JSArrayBufferView::fastSizeLimit)));
    // We assume through the rest of the fast path that the size is a 32-bit number.
    static_assert(isInBounds<int32_t>(JSArrayBufferView::fastSizeLimit));
#else
    slowCases.append(branch32(
        Above, sizeGPR, TrustedImm32(JSArrayBufferView::fastSizeLimit)));
#endif
    
    lshift32(sizeGPR, TrustedImm32(logElementSize(typedArrayType)), scratchGPR);
    if (elementSize(typedArrayType) < 8) {
        add32(TrustedImm32(7), scratchGPR);
        and32(TrustedImm32(~7), scratchGPR);
    }
    emitAllocateVariableSized(
        storageGPR, vm().primitiveGigacageAuxiliarySpace(), scratchGPR, scratchGPR,
        scratchGPR2, slowCases);
    
    Jump done = branchTest32(Zero, sizeGPR);
    move(sizeGPR, scratchGPR);
    if (elementSize(typedArrayType) != 4) {
        if (elementSize(typedArrayType) > 4)
            lshift32(TrustedImm32(logElementSize(typedArrayType) - 2), scratchGPR);
        else {
            if (elementSize(typedArrayType) > 1)
                lshift32(TrustedImm32(logElementSize(typedArrayType)), scratchGPR);
            add32(TrustedImm32(3), scratchGPR);
            urshift32(TrustedImm32(2), scratchGPR);
        }
    }
    Label loop = label();
    sub32(TrustedImm32(1), scratchGPR);
    store32(
        TrustedImm32(0),
        BaseIndex(storageGPR, scratchGPR, TimesFour));
    branchTest32(NonZero, scratchGPR).linkTo(loop, this);
    done.link(this);

    auto butterfly = TrustedImmPtr(nullptr);
    switch (typedArrayType) {
#define TYPED_ARRAY_TYPE_CASE(name) \
    case Type ## name: \
        emitAllocateJSObject<JS##name##Array>(resultGPR, TrustedImmPtr(structure), butterfly, scratchGPR, scratchGPR2, slowCases, SlowAllocationResult::UndefinedBehavior); \
        break;
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
    case TypeDataView:
        emitAllocateJSObject<JSDataView>(resultGPR, TrustedImmPtr(structure), butterfly, scratchGPR, scratchGPR2, slowCases, SlowAllocationResult::UndefinedBehavior);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    storePtr(
        storageGPR,
        Address(resultGPR, JSArrayBufferView::offsetOfVector()));
#if USE(LARGE_TYPED_ARRAYS)
    store64(sizeGPR, Address(resultGPR, JSArrayBufferView::offsetOfLength()));
    store64(TrustedImm32(0), Address(resultGPR, JSArrayBufferView::offsetOfByteOffset()));
#else
    store32(sizeGPR, Address(resultGPR, JSArrayBufferView::offsetOfLength()));
    store32(TrustedImm32(0), Address(resultGPR, JSArrayBufferView::offsetOfByteOffset()));
#endif
    store8(
        TrustedImm32(FastTypedArray),
        Address(resultGPR, JSArrayBufferView::offsetOfMode()));
    
    mutatorFence(vm());
    
    addSlowPathGenerator(slowPathCall(
        slowCases, this, operationNewTypedArrayWithSizeForType(typedArrayType),
        resultGPR, LinkableConstant::globalObject(*this, node), structure, sizeGPR, storageGPR));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewRegexp(Node* node)
{
    RegExp* regexp = node->castOperand<RegExp*>();

    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    JSValueOperand lastIndex(this, node->child1());

    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    JSValueRegs lastIndexRegs = lastIndex.jsValueRegs();

    JumpList slowPath;

    auto structure = m_graph.registerStructure(m_graph.globalObjectFor(node->origin.semantic)->regExpStructure());
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject<RegExpObject>(resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR, slowPath, SlowAllocationResult::UndefinedBehavior);

    storeLinkableConstant(LinkableConstant(*this, node->cellOperand()->cell()), Address(resultGPR, RegExpObject::offsetOfRegExpAndFlags()));
    storeValue(lastIndexRegs, Address(resultGPR, RegExpObject::offsetOfLastIndex()));
    mutatorFence(vm());

    addSlowPathGenerator(slowPathCall(slowPath, this, operationNewRegexpWithLastIndex, resultGPR, LinkableConstant::globalObject(*this, node), LinkableConstant(*this, regexp), lastIndexRegs));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::speculateCellTypeWithoutTypeFiltering(
    Edge edge, GPRReg cellGPR, JSType jsType)
{
    speculationCheck(
        BadType, JSValueSource::unboxedCell(cellGPR), edge,
        branchIfNotType(cellGPR, jsType));
}

void SpeculativeJIT::speculateCellType(
    Edge edge, GPRReg cellGPR, SpeculatedType specType, JSType jsType)
{
    DFG_TYPE_CHECK(
        JSValueSource::unboxedCell(cellGPR), edge, specType,
        branchIfNotType(cellGPR, jsType));
}

void SpeculativeJIT::speculateInt32(Edge edge)
{
    if (!needsTypeCheck(edge, SpecInt32Only))
        return;
    
    (SpeculateInt32Operand(this, edge)).gpr();
}

void SpeculativeJIT::speculateNumber(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBytecodeNumber))
        return;
    
    JSValueOperand value(this, edge, ManualOperandSpeculation);
#if USE(JSVALUE64)
    GPRReg gpr = value.gpr();
    typeCheck(
        JSValueRegs(gpr), edge, SpecBytecodeNumber,
        branchIfNotNumber(gpr));
#else
    static_assert(JSValue::Int32Tag >= JSValue::LowestTag, "Int32Tag is included in >= JSValue::LowestTag range.");
    GPRReg tagGPR = value.tagGPR();
    DFG_TYPE_CHECK(
        value.jsValueRegs(), edge, ~SpecInt32Only,
        branchIfInt32(tagGPR));
    DFG_TYPE_CHECK(
        value.jsValueRegs(), edge, SpecBytecodeNumber,
        branch32(AboveOrEqual, tagGPR, TrustedImm32(JSValue::LowestTag)));
#endif
}

void SpeculativeJIT::speculateRealNumber(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBytecodeRealNumber))
        return;
    
    JSValueOperand op1(this, edge, ManualOperandSpeculation);
    FPRTemporary result(this);
    
    JSValueRegs op1Regs = op1.jsValueRegs();
    FPRReg resultFPR = result.fpr();
    
#if USE(JSVALUE64)
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    unboxDoubleWithoutAssertions(op1Regs.gpr(), tempGPR, resultFPR);
#else
    unboxDouble(op1Regs.tagGPR(), op1Regs.payloadGPR(), resultFPR);
#endif
    
    Jump done = branchIfNotNaN(resultFPR);

    typeCheck(op1Regs, edge, SpecBytecodeRealNumber, branchIfNotInt32(op1Regs));
    
    done.link(this);
}

void SpeculativeJIT::speculateDoubleRepReal(Edge edge)
{
    if (!needsTypeCheck(edge, SpecDoubleReal))
        return;
    
    SpeculateDoubleOperand operand(this, edge);
    FPRReg fpr = operand.fpr();
    typeCheck(
        JSValueRegs(), edge, SpecDoubleReal,
        branchIfNaN(fpr));
}

void SpeculativeJIT::speculateBoolean(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBoolean))
        return;
    
    (SpeculateBooleanOperand(this, edge)).gpr();
}

void SpeculativeJIT::speculateCell(Edge edge)
{
    if (!needsTypeCheck(edge, SpecCellCheck))
        return;
    
    (SpeculateCellOperand(this, edge)).gpr();
}

void SpeculativeJIT::speculateCellOrOther(Edge edge)
{
    if (!needsTypeCheck(edge, SpecCellCheck | SpecOther))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();

    Jump ok = branchIfCell(operand.jsValueRegs());
    DFG_TYPE_CHECK(
        operand.jsValueRegs(), edge, SpecCellCheck | SpecOther,
        branchIfNotOther(operand.jsValueRegs(), tempGPR));
    ok.link(this);
}

void SpeculativeJIT::speculateObject(Edge edge, GPRReg cell)
{
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(cell), edge, SpecObject, branchIfNotObject(cell));
}

void SpeculativeJIT::speculateObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateFunction(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecFunction, JSFunctionType);
}

void SpeculativeJIT::speculateFunction(Edge edge)
{
    if (!needsTypeCheck(edge, SpecFunction))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateFunction(edge, operand.gpr());
}

void SpeculativeJIT::speculateFinalObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecFinalObject, FinalObjectType);
}

void SpeculativeJIT::speculateFinalObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecFinalObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateFinalObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateRegExpObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecRegExpObject, RegExpObjectType);
}

void SpeculativeJIT::speculateRegExpObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecRegExpObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateRegExpObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateArray(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecArray, ArrayType);
}

void SpeculativeJIT::speculateArray(Edge edge)
{
    if (!needsTypeCheck(edge, SpecArray))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateArray(edge, operand.gpr());
}

void SpeculativeJIT::speculateProxyObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecProxyObject, ProxyObjectType);
}

void SpeculativeJIT::speculateProxyObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecProxyObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateProxyObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateGlobalProxy(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecGlobalProxy, GlobalProxyType);
}

void SpeculativeJIT::speculateGlobalProxy(Edge edge)
{
    if (!needsTypeCheck(edge, SpecGlobalProxy))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateGlobalProxy(edge, operand.gpr());
}

void SpeculativeJIT::speculateDerivedArray(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecDerivedArray, DerivedArrayType);
}

void SpeculativeJIT::speculateDerivedArray(Edge edge)
{
    if (!needsTypeCheck(edge, SpecDerivedArray))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateDerivedArray(edge, operand.gpr());
}

void SpeculativeJIT::speculatePromiseObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecPromiseObject, JSPromiseType);
}

void SpeculativeJIT::speculatePromiseObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecPromiseObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculatePromiseObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateDateObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecDateObject, JSDateType);
}

void SpeculativeJIT::speculateDateObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecDateObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateDateObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateImmutableButterfly(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecCellOther, JSImmutableButterflyType);
}

void SpeculativeJIT::speculateImmutableButterfly(Edge edge)
{
    if (!needsTypeCheck(edge, SpecCellOther))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateMapObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateMapObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecMapObject, JSMapType);
}

void SpeculativeJIT::speculateMapObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecMapObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateMapObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateSetObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecSetObject, JSSetType);
}

void SpeculativeJIT::speculateSetObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecSetObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateSetObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateMapIteratorObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecObjectOther, JSMapIteratorType);
}

void SpeculativeJIT::speculateMapIteratorObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecObjectOther))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateMapIteratorObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateSetIteratorObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecObjectOther, JSSetIteratorType);
}

void SpeculativeJIT::speculateSetIteratorObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecObjectOther))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateSetIteratorObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateWeakMapObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecWeakMapObject, JSWeakMapType);
}

void SpeculativeJIT::speculateWeakMapObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecWeakMapObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateWeakMapObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateWeakSetObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecWeakSetObject, JSWeakSetType);
}

void SpeculativeJIT::speculateWeakSetObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecWeakSetObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateWeakSetObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateDataViewObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecDataViewObject, DataViewType);
}

void SpeculativeJIT::speculateDataViewObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecDataViewObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateDataViewObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateObjectOrOther(Edge edge)
{
    if (!needsTypeCheck(edge, SpecObject | SpecOther))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    Jump notCell = branchIfNotCell(operand.jsValueRegs());
    GPRReg gpr = operand.jsValueRegs().payloadGPR();
    DFG_TYPE_CHECK(
        operand.jsValueRegs(), edge, (~SpecCellCheck) | SpecObject, branchIfNotObject(gpr));
    Jump done = jump();
    notCell.link(this);
    DFG_TYPE_CHECK(
        operand.jsValueRegs(), edge, SpecCellCheck | SpecOther,
        branchIfNotOther(operand.jsValueRegs(), tempGPR));
    done.link(this);
}

void SpeculativeJIT::speculateString(Edge edge, GPRReg cell)
{
    DFG_TYPE_CHECK(
        JSValueSource::unboxedCell(cell), edge, SpecString | ~SpecCellCheck, branchIfNotString(cell));
}

void SpeculativeJIT::speculateStringOrOther(Edge edge, JSValueRegs regs, GPRReg scratch)
{
    Jump notCell = branchIfNotCell(regs);
    GPRReg cell = regs.payloadGPR();
    DFG_TYPE_CHECK(regs, edge, (~SpecCellCheck) | SpecString, branchIfNotString(cell));
    Jump done = jump();
    notCell.link(this);
    DFG_TYPE_CHECK(regs, edge, SpecCellCheck | SpecOther, branchIfNotOther(regs, scratch));
    done.link(this);
}

void SpeculativeJIT::speculateStringOrOther(Edge edge)
{
    if (!needsTypeCheck(edge, SpecString | SpecOther))
        return;

    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs regs = operand.jsValueRegs();
    GPRReg tempGPR = temp.gpr();
    speculateStringOrOther(edge, regs, tempGPR);
}

void SpeculativeJIT::speculateStringIdentAndLoadStorage(Edge edge, GPRReg string, GPRReg storage)
{
    loadPtr(Address(string, JSString::offsetOfValue()), storage);
    
    if (!needsTypeCheck(edge, SpecStringIdent | ~SpecString))
        return;

    speculationCheck(BadStringType, JSValueSource::unboxedCell(string), edge, branchIfRopeStringImpl(storage));
    speculationCheck(BadStringType, JSValueSource::unboxedCell(string), edge, branchTest32(Zero, Address(storage, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIsAtom())));
    
    m_interpreter.filter(edge, SpecStringIdent | ~SpecString);
}

void SpeculativeJIT::speculateStringIdent(Edge edge, GPRReg string)
{
    if (!needsTypeCheck(edge, SpecStringIdent))
        return;

    GPRTemporary temp(this);
    speculateStringIdentAndLoadStorage(edge, string, temp.gpr());
}

void SpeculativeJIT::speculateStringIdent(Edge edge)
{
    if (!needsTypeCheck(edge, SpecStringIdent))
        return;
    
    SpeculateCellOperand operand(this, edge);
    GPRReg gpr = operand.gpr();
    speculateString(edge, gpr);
    speculateStringIdent(edge, gpr);
}

void SpeculativeJIT::speculateString(Edge edge)
{
    if (!needsTypeCheck(edge, SpecString))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateString(edge, operand.gpr());
}

void SpeculativeJIT::speculateStringObject(Edge edge, GPRReg cellGPR)
{
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(cellGPR), edge, ~SpecCellCheck | SpecStringObject, branchIfNotType(cellGPR, StringObjectType));
}

void SpeculativeJIT::speculateStringObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecStringObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    GPRReg gpr = operand.gpr();
    speculateStringObject(edge, gpr);
}

void SpeculativeJIT::speculateStringOrStringObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecString | SpecStringObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    GPRReg gpr = operand.gpr();
    if (!needsTypeCheck(edge, SpecString | SpecStringObject))
        return;

    GPRTemporary typeTemp(this);
    GPRReg typeGPR = typeTemp.gpr();

    load8(Address(gpr, JSCell::typeInfoTypeOffset()), typeGPR);

    Jump isString = branch32(Equal, typeGPR, TrustedImm32(StringType));
    speculationCheck(BadType, JSValueSource::unboxedCell(gpr), edge.node(), branch32(NotEqual, typeGPR, TrustedImm32(StringObjectType)));
    isString.link(this);
    
    m_interpreter.filter(edge, SpecString | SpecStringObject);
}

void SpeculativeJIT::speculateNotStringVar(Edge edge)
{
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    
    Jump notCell = branchIfNotCell(operand.jsValueRegs());
    GPRReg cell = operand.jsValueRegs().payloadGPR();
    
    Jump notString = branchIfNotString(cell);
    
    speculateStringIdentAndLoadStorage(edge, cell, tempGPR);
    
    notString.link(this);
    notCell.link(this);
}

void SpeculativeJIT::speculateNotSymbol(Edge edge)
{
    if (!needsTypeCheck(edge, ~SpecSymbol))
        return;

    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    auto valueRegs = operand.jsValueRegs();
    GPRReg value = valueRegs.payloadGPR();
    Jump notCell;

    bool needsCellCheck = needsTypeCheck(edge, SpecCell);
    if (needsCellCheck)
        notCell = branchIfNotCell(valueRegs);

    speculationCheck(BadType, JSValueSource::unboxedCell(value), edge.node(), branchIfSymbol(value));

    if (needsCellCheck)
        notCell.link(this);

    m_interpreter.filter(edge, ~SpecSymbol);
}

void SpeculativeJIT::speculateSymbol(Edge edge, GPRReg cell)
{
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(cell), edge, ~SpecCellCheck | SpecSymbol, branchIfNotSymbol(cell));
}

void SpeculativeJIT::speculateSymbol(Edge edge)
{
    if (!needsTypeCheck(edge, SpecSymbol))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateSymbol(edge, operand.gpr());
}

void SpeculativeJIT::speculateHeapBigInt(Edge edge, GPRReg cell)
{
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(cell), edge, ~SpecCellCheck | SpecHeapBigInt, branchIfNotHeapBigInt(cell));
}

void SpeculativeJIT::speculateHeapBigInt(Edge edge)
{
    if (!needsTypeCheck(edge, SpecHeapBigInt))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateHeapBigInt(edge, operand.gpr());
}

void SpeculativeJIT::speculateNotCell(Edge edge, JSValueRegs regs)
{
    DFG_TYPE_CHECK(regs, edge, ~SpecCellCheck, branchIfCell(regs));
}

void SpeculativeJIT::speculateNotCell(Edge edge)
{
    if (!needsTypeCheck(edge, ~SpecCellCheck))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation); 
    speculateNotCell(edge, operand.jsValueRegs());
}

void SpeculativeJIT::speculateNotCellNorBigInt(Edge edge)
{
#if USE(BIGINT32)
    if (!needsTypeCheck(edge, ~SpecCellCheck & ~SpecBigInt))
        return;

    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);

    JSValueRegs regs = operand.jsValueRegs();
    GPRReg tempGPR = temp.gpr();

    DFG_TYPE_CHECK(regs, edge, ~SpecCellCheck, branchIfCell(regs));
    DFG_TYPE_CHECK(regs, edge, ~SpecCellCheck & ~SpecBigInt, branchIfBigInt32(regs, tempGPR));
#else
    speculateNotCell(edge);
#endif
}

void SpeculativeJIT::speculateNotDouble(Edge edge, JSValueRegs regs, GPRReg tempGPR)
{
    if (!needsTypeCheck(edge, ~SpecFullDouble))
        return;

    Jump done;

    bool mayBeInt32 = needsTypeCheck(edge, ~SpecInt32Only);
    if (mayBeInt32)
        done = branchIfInt32(regs);

    DFG_TYPE_CHECK(regs, edge, ~SpecFullDouble, branchIfNumber(regs, tempGPR));

    if (mayBeInt32)
        done.link(this);
}

void SpeculativeJIT::speculateNotDouble(Edge edge)
{
    if (!needsTypeCheck(edge, ~SpecFullDouble))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs regs = operand.jsValueRegs();
    GPRReg tempGPR = temp.gpr();
    
    speculateNotDouble(edge, regs, tempGPR);
}

void SpeculativeJIT::speculateNeitherDoubleNorHeapBigInt(Edge edge, JSValueRegs regs, GPRReg tempGPR)
{
    if (!needsTypeCheck(edge, ~(SpecFullDouble | SpecHeapBigInt)))
        return;

    JumpList done;

    bool mayBeInt32 = needsTypeCheck(edge, ~SpecInt32Only);
    if (mayBeInt32)
        done.append(branchIfInt32(regs));

    DFG_TYPE_CHECK(regs, edge, ~SpecFullDouble, branchIfNumber(regs, tempGPR));

    bool mayBeNotCell = needsTypeCheck(edge, SpecCell);
    if (mayBeNotCell)
        done.append(branchIfNotCell(regs));

    DFG_TYPE_CHECK(regs, edge, ~SpecHeapBigInt, branchIfHeapBigInt(regs.payloadGPR()));

    if (mayBeInt32 || mayBeNotCell)
        done.link(this);
}

void SpeculativeJIT::speculateNeitherDoubleNorHeapBigInt(Edge edge)
{
    if (!needsTypeCheck(edge, ~(SpecFullDouble | SpecHeapBigInt)))
        return;

    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs regs = operand.jsValueRegs();
    GPRReg tempGPR = temp.gpr();

    speculateNeitherDoubleNorHeapBigInt(edge, regs, tempGPR);
}

void SpeculativeJIT::speculateNeitherDoubleNorHeapBigIntNorString(Edge edge, JSValueRegs regs, GPRReg tempGPR)
{
    if (!needsTypeCheck(edge, ~(SpecFullDouble | SpecString | SpecHeapBigInt)))
        return;

    JumpList done;

    bool mayBeInt32 = needsTypeCheck(edge, ~SpecInt32Only);
    if (mayBeInt32)
        done.append(branchIfInt32(regs));

    DFG_TYPE_CHECK(regs, edge, ~SpecFullDouble, branchIfNumber(regs, tempGPR));

    bool mayBeNotCell = needsTypeCheck(edge, SpecCell);
    if (mayBeNotCell)
        done.append(branchIfNotCell(regs));

    static_assert(StringType + 1 == HeapBigIntType);
    DFG_TYPE_CHECK(regs, edge, ~(SpecString | SpecHeapBigInt), branchIfType(regs.payloadGPR(), JSTypeRange { StringType, HeapBigIntType }));

    if (mayBeInt32 || mayBeNotCell)
        done.link(this);
}

void SpeculativeJIT::speculateNeitherDoubleNorHeapBigIntNorString(Edge edge)
{
    if (!needsTypeCheck(edge, ~(SpecFullDouble | SpecHeapBigInt | SpecString)))
        return;

    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs regs = operand.jsValueRegs();
    GPRReg tempGPR = temp.gpr();

    speculateNeitherDoubleNorHeapBigIntNorString(edge, regs, tempGPR);
}

void SpeculativeJIT::speculateOther(Edge edge, JSValueRegs regs, GPRReg tempGPR)
{
    DFG_TYPE_CHECK(regs, edge, SpecOther, branchIfNotOther(regs, tempGPR));
}

void SpeculativeJIT::speculateOther(Edge edge, JSValueRegs regs)
{
    if (!needsTypeCheck(edge, SpecOther))
        return;

    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    speculateOther(edge, regs, tempGPR);
}

void SpeculativeJIT::speculateOther(Edge edge)
{
    if (!needsTypeCheck(edge, SpecOther))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    speculateOther(edge, operand.jsValueRegs());
}

void SpeculativeJIT::speculateMisc(Edge edge, JSValueRegs regs)
{
#if USE(JSVALUE64)
    DFG_TYPE_CHECK(
        regs, edge, SpecMisc,
        branch64(Above, regs.gpr(), TrustedImm64(JSValue::MiscTag)));
#else
    static_assert(JSValue::Int32Tag >= JSValue::UndefinedTag, "Int32Tag is included in >= JSValue::UndefinedTag range.");
    DFG_TYPE_CHECK(
        regs, edge, ~SpecInt32Only,
        branchIfInt32(regs.tagGPR()));
    DFG_TYPE_CHECK(
        regs, edge, SpecMisc,
        branch32(Below, regs.tagGPR(), TrustedImm32(JSValue::UndefinedTag)));
#endif
}

void SpeculativeJIT::speculateMisc(Edge edge)
{
    if (!needsTypeCheck(edge, SpecMisc))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    speculateMisc(edge, operand.jsValueRegs());
}

void SpeculativeJIT::speculate(Node*, Edge edge)
{
    switch (edge.useKind()) {
    case UntypedUse:
        break;
    case DoubleRepUse:
    case Int52RepUse:
    case KnownInt32Use:
    case KnownCellUse:
    case KnownStringUse:
    case KnownPrimitiveUse:
    case KnownOtherUse:
    case KnownBooleanUse:
        ASSERT(!m_interpreter.needsTypeCheck(edge));
        break;
    case Int32Use:
        speculateInt32(edge);
        break;
    case NumberUse:
        speculateNumber(edge);
        break;
    case RealNumberUse:
        speculateRealNumber(edge);
        break;
    case DoubleRepRealUse:
        speculateDoubleRepReal(edge);
        break;
#if USE(JSVALUE64)
    case AnyIntUse:
        speculateAnyInt(edge);
        break;
    case DoubleRepAnyIntUse:
        speculateDoubleRepAnyInt(edge);
        break;
#endif
    case BooleanUse:
        speculateBoolean(edge);
        break;
    case CellUse:
        speculateCell(edge);
        break;
    case CellOrOtherUse:
        speculateCellOrOther(edge);
        break;
    case ObjectUse:
        speculateObject(edge);
        break;
    case FunctionUse:
        speculateFunction(edge);
        break;
    case ArrayUse:
        speculateArray(edge);
        break;
    case FinalObjectUse:
        speculateFinalObject(edge);
        break;
    case RegExpObjectUse:
        speculateRegExpObject(edge);
        break;
    case PromiseObjectUse:
        speculatePromiseObject(edge);
        break;
    case ProxyObjectUse:
        speculateProxyObject(edge);
        break;
    case GlobalProxyUse:
        speculateGlobalProxy(edge);
        break;
    case DerivedArrayUse:
        speculateDerivedArray(edge);
        break;
    case DateObjectUse:
        speculateDateObject(edge);
        break;
    case MapObjectUse:
        speculateMapObject(edge);
        break;
    case SetObjectUse:
        speculateSetObject(edge);
        break;
    case MapIteratorObjectUse:
        speculateMapIteratorObject(edge);
        break;
    case SetIteratorObjectUse:
        speculateSetIteratorObject(edge);
        break;
    case WeakMapObjectUse:
        speculateWeakMapObject(edge);
        break;
    case WeakSetObjectUse:
        speculateWeakSetObject(edge);
        break;
    case DataViewObjectUse:
        speculateDataViewObject(edge);
        break;
    case ObjectOrOtherUse:
        speculateObjectOrOther(edge);
        break;
    case StringIdentUse:
        speculateStringIdent(edge);
        break;
    case StringUse:
        speculateString(edge);
        break;
    case StringOrOtherUse:
        speculateStringOrOther(edge);
        break;
    case SymbolUse:
        speculateSymbol(edge);
        break;
#if USE(BIGINT32)
    case BigInt32Use:
        speculateBigInt32(edge);
        break;
    case AnyBigIntUse:
        speculateAnyBigInt(edge);
        break;
#endif
    case HeapBigIntUse:
        speculateHeapBigInt(edge);
        break;
    case StringObjectUse:
        speculateStringObject(edge);
        break;
    case StringOrStringObjectUse:
        speculateStringOrStringObject(edge);
        break;
    case NotStringVarUse:
        speculateNotStringVar(edge);
        break;
    case NotSymbolUse:
        speculateNotSymbol(edge);
        break;
    case NotCellUse:
        speculateNotCell(edge);
        break;
    case NotCellNorBigIntUse:
        speculateNotCellNorBigInt(edge);
        break;
    case NotDoubleUse:
        speculateNotDouble(edge);
        break;
    case NeitherDoubleNorHeapBigIntUse:
        speculateNeitherDoubleNorHeapBigInt(edge);
        break;
    case NeitherDoubleNorHeapBigIntNorStringUse:
        speculateNeitherDoubleNorHeapBigIntNorString(edge);
        break;
    case OtherUse:
        speculateOther(edge);
        break;
    case MiscUse:
        speculateMisc(edge);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::emitSwitchIntJump(
    SwitchData* data, GPRReg value, GPRReg scratch)
{
    const UnlinkedSimpleJumpTable& unlinkedTable = m_graph.unlinkedSwitchJumpTable(data->switchTableIndex);
    SimpleJumpTable& linkedTable = m_graph.switchJumpTable(data->switchTableIndex);
    linkedTable.ensureCTITable(unlinkedTable);
    sub32(Imm32(unlinkedTable.m_min), value);
    addBranch(
        branch32(AboveOrEqual, value, Imm32(linkedTable.m_ctiOffsets.size())),
        data->fallThrough.block);
    move(TrustedImmPtr(linkedTable.m_ctiOffsets.mutableSpan().data()), scratch);

#if USE(JSVALUE64)
    farJump(BaseIndex(scratch, value, ScalePtr), JSSwitchPtrTag);
#else
    loadPtr(BaseIndex(scratch, value, ScalePtr), scratch);
    farJump(scratch, JSSwitchPtrTag);
#endif
    data->didUseJumpTable = true;
}

void SpeculativeJIT::emitSwitchImm(Node* node, SwitchData* data)
{
    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateInt32Operand value(this, node->child1());
        GPRTemporary temp(this);
        emitSwitchIntJump(data, value.gpr(), temp.gpr());
        noResult(node);
        break;
    }
        
    case UntypedUse: {
        JSValueOperand value(this, node->child1());
        GPRTemporary temp(this);
        JSValueRegs valueRegs = value.jsValueRegs();
        GPRReg scratch = temp.gpr();

        value.use();

        auto notInt32 = branchIfNotInt32(valueRegs);
        emitSwitchIntJump(data, valueRegs.payloadGPR(), scratch);
        notInt32.link(this);
        addBranch(branchIfNotNumber(valueRegs, scratch), data->fallThrough.block);

        const UnlinkedSimpleJumpTable& unlinkedTable = m_graph.unlinkedSwitchJumpTable(data->switchTableIndex);
        silentSpillAllRegisters(scratch);
        callOperationWithoutExceptionCheck(operationFindSwitchImmTargetForDouble, scratch, TrustedImmPtr(&vm()), valueRegs, data->switchTableIndex, unlinkedTable.m_min);
        silentFillAllRegisters();

        farJump(scratch, JSSwitchPtrTag);
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    ASSERT(data->didUseJumpTable);
}

void SpeculativeJIT::emitSwitchCharStringJump(Node* node, SwitchData* data, GPRReg value, GPRReg scratch)
{
    loadPtr(Address(value, JSString::offsetOfValue()), scratch);
    auto isRope = branchIfRopeStringImpl(scratch);
    addSlowPathGenerator(slowPathCall(isRope, this, operationResolveRope, scratch, LinkableConstant::globalObject(*this, node), value));
    
    addBranch(
        branch32(
            NotEqual,
            Address(scratch, StringImpl::lengthMemoryOffset()),
            TrustedImm32(1)),
        data->fallThrough.block);
    
    loadPtr(Address(scratch, StringImpl::dataOffset()), value);
    
    Jump is8Bit = branchTest32(
        NonZero,
        Address(scratch, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit()));
    
    load16(Address(value), scratch);
    
    Jump ready = jump();
    
    is8Bit.link(this);
    load8(Address(value), scratch);
    
    ready.link(this);
    emitSwitchIntJump(data, scratch, value);
}

void SpeculativeJIT::emitSwitchChar(Node* node, SwitchData* data)
{
    switch (node->child1().useKind()) {
    case StringUse: {
        SpeculateCellOperand op1(this, node->child1());
        GPRTemporary temp(this);

        GPRReg op1GPR = op1.gpr();
        GPRReg tempGPR = temp.gpr();

        op1.use();

        speculateString(node->child1(), op1GPR);
        emitSwitchCharStringJump(node, data, op1GPR, tempGPR);
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    case UntypedUse: {
        JSValueOperand op1(this, node->child1());
        GPRTemporary temp(this);

        JSValueRegs op1Regs = op1.jsValueRegs();
        GPRReg tempGPR = temp.gpr();

        op1.use();
        
        addBranch(branchIfNotCell(op1Regs), data->fallThrough.block);
        
        addBranch(branchIfNotString(op1Regs.payloadGPR()), data->fallThrough.block);
        
        emitSwitchCharStringJump(node, data, op1Regs.payloadGPR(), tempGPR);
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    ASSERT(data->didUseJumpTable);
}

namespace {

struct CharacterCase {
    bool operator<(const CharacterCase& other) const
    {
        return character < other.character;
    }
    
    LChar character;
    unsigned begin;
    unsigned end;
};

} // anonymous namespace

void SpeculativeJIT::emitBinarySwitchStringRecurse(
    SwitchData* data, const Vector<SpeculativeJIT::StringSwitchCase>& cases,
    unsigned numChecked, unsigned begin, unsigned end, GPRReg buffer, GPRReg length,
    GPRReg temp, unsigned alreadyCheckedLength, bool checkedExactLength)
{
    static constexpr bool verbose = false;
    
    if (verbose) {
        dataLog("We're down to the following cases, alreadyCheckedLength = ", alreadyCheckedLength, ":\n");
        for (unsigned i = begin; i < end; ++i) {
            dataLog("    ", cases[i].string, "\n");
        }
    }
    
    if (begin == end) {
        jump(data->fallThrough.block, ForceJump);
        return;
    }
    
    unsigned minLength = cases[begin].string->length();
    unsigned commonChars = minLength;
    bool allLengthsEqual = true;
    for (unsigned i = begin + 1; i < end; ++i) {
        unsigned myCommonChars = numChecked;
        for (unsigned j = numChecked;
            j < std::min(cases[begin].string->length(), cases[i].string->length());
            ++j) {
            if (cases[begin].string->at(j) != cases[i].string->at(j)) {
                if (verbose)
                    dataLog("string(", cases[i].string, ")[", j, "] != string(", cases[begin].string, ")[", j, "]\n");
                break;
            }
            myCommonChars++;
        }
        commonChars = std::min(commonChars, myCommonChars);
        if (minLength != cases[i].string->length())
            allLengthsEqual = false;
        minLength = std::min(minLength, cases[i].string->length());
    }
    
    if (checkedExactLength) {
        RELEASE_ASSERT(alreadyCheckedLength == minLength);
        RELEASE_ASSERT(allLengthsEqual);
    }
    
    RELEASE_ASSERT(minLength >= commonChars);
    
    if (verbose)
        dataLog("length = ", minLength, ", commonChars = ", commonChars, ", allLengthsEqual = ", allLengthsEqual, "\n");
    
    if (!allLengthsEqual && alreadyCheckedLength < minLength)
        branch32(Below, length, Imm32(minLength), data->fallThrough.block);
    if (allLengthsEqual && (alreadyCheckedLength < minLength || !checkedExactLength))
        branch32(NotEqual, length, Imm32(minLength), data->fallThrough.block);
    
    for (unsigned i = numChecked; i < commonChars; ++i) {
        branch8(
            NotEqual, Address(buffer, i),
            TrustedImm32(cases[begin].string->at(i)), data->fallThrough.block);
    }
    
    if (minLength == commonChars) {
        // This is the case where one of the cases is a prefix of all of the other cases.
        // We've already checked that the input string is a prefix of all of the cases,
        // so we just check length to jump to that case.
        
        if (ASSERT_ENABLED) {
            ASSERT(cases[begin].string->length() == commonChars);
            for (unsigned i = begin + 1; i < end; ++i)
                ASSERT(cases[i].string->length() > commonChars);
        }
        
        if (allLengthsEqual) {
            RELEASE_ASSERT(end == begin + 1);
            jump(cases[begin].target, ForceJump);
            return;
        }
        
        branch32(Equal, length, Imm32(commonChars), cases[begin].target);
        
        // We've checked if the length is >= minLength, and then we checked if the
        // length is == commonChars. We get to this point if it is >= minLength but not
        // == commonChars. Hence we know that it now must be > minLength, i.e., that
        // it's >= minLength + 1.
        emitBinarySwitchStringRecurse(
            data, cases, commonChars, begin + 1, end, buffer, length, temp, minLength + 1, false);
        return;
    }
    
    // At this point we know that the string is longer than commonChars, and we've only
    // verified commonChars. Use a binary switch on the next unchecked character, i.e.
    // string[commonChars].
    
    RELEASE_ASSERT(end >= begin + 2);
    
    load8(Address(buffer, commonChars), temp);
    
    Vector<CharacterCase> characterCases;
    CharacterCase currentCase;
    currentCase.character = cases[begin].string->at(commonChars);
    currentCase.begin = begin;
    currentCase.end = begin + 1;
    for (unsigned i = begin + 1; i < end; ++i) {
        if (cases[i].string->at(commonChars) != currentCase.character) {
            if (verbose)
                dataLog("string(", cases[i].string, ")[", commonChars, "] != string(", cases[begin].string, ")[", commonChars, "]\n");
            currentCase.end = i;
            characterCases.append(currentCase);
            currentCase.character = cases[i].string->at(commonChars);
            currentCase.begin = i;
            currentCase.end = i + 1;
        } else
            currentCase.end = i + 1;
    }
    characterCases.append(currentCase);
    
    Vector<int64_t, 16> characterCaseValues;
    for (unsigned i = 0; i < characterCases.size(); ++i)
        characterCaseValues.append(characterCases[i].character);
    
    BinarySwitch binarySwitch(temp, characterCaseValues.span(), BinarySwitch::Int32);
    while (binarySwitch.advance(*this)) {
        const CharacterCase& myCase = characterCases[binarySwitch.caseIndex()];
        emitBinarySwitchStringRecurse(
            data, cases, commonChars + 1, myCase.begin, myCase.end, buffer, length,
            temp, minLength, allLengthsEqual);
    }
    
    addBranch(binarySwitch.fallThrough(), data->fallThrough.block);
}

void SpeculativeJIT::emitSwitchStringOnString(Node* node, SwitchData* data, GPRReg string)
{
    data->didUseJumpTable = true;

    const UnlinkedStringJumpTable& unlinkedTable = m_graph.unlinkedStringSwitchJumpTable(data->switchTableIndex);
    StringJumpTable& linkedTable = m_graph.stringSwitchJumpTable(data->switchTableIndex);
    linkedTable.ensureCTITable(unlinkedTable);

    bool canDoBinarySwitch = true;
    unsigned totalLength = 0;
    
    for (unsigned i = data->cases.size(); i--;) {
        StringImpl* string = data->cases[i].value.stringImpl();
        if (!string->is8Bit()) {
            canDoBinarySwitch = false;
            break;
        }
        if (string->length() > Options::maximumBinaryStringSwitchCaseLength()) {
            canDoBinarySwitch = false;
            break;
        }
        totalLength += string->length();
    }

    if (!canDoBinarySwitch || totalLength > Options::maximumBinaryStringSwitchTotalLength()) {
        flushRegisters();
        callOperation(operationSwitchString, string, LinkableConstant::globalObject(*this, node), static_cast<size_t>(data->switchTableIndex), TrustedImmPtr(&unlinkedTable), string);
        farJump(string, JSSwitchPtrTag);
        return;
    }
    
    GPRTemporary length(this);
    GPRTemporary temp(this);
    
    GPRReg lengthGPR = length.gpr();
    GPRReg tempGPR = temp.gpr();
    
    JumpList isRopeCases;
    JumpList slowCases;
    loadPtr(Address(string, JSString::offsetOfValue()), tempGPR);
    isRopeCases.append(branchIfRopeStringImpl(tempGPR));
    load32(Address(tempGPR, StringImpl::lengthMemoryOffset()), lengthGPR);
    
    slowCases.append(branchTest32(
        Zero,
        Address(tempGPR, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit())));
    
    loadPtr(Address(tempGPR, StringImpl::dataOffset()), string);
    
    Vector<StringSwitchCase> cases;
    for (unsigned i = 0; i < data->cases.size(); ++i) {
        cases.append(
            StringSwitchCase(data->cases[i].value.stringImpl(), data->cases[i].target.block));
    }
    
    std::sort(cases.begin(), cases.end());
    
    emitBinarySwitchStringRecurse(data, cases, 0, 0, cases.size(), string, lengthGPR, tempGPR, 0, false);

    isRopeCases.link(this);
    load32(Address(string, JSRopeString::offsetOfLength()), tempGPR);
    sub32(TrustedImm32(unlinkedTable.minLength()), tempGPR);
    branch32(Above, tempGPR, TrustedImm32(unlinkedTable.maxLength() - unlinkedTable.minLength()), data->fallThrough.block);
    
    slowCases.link(this);
    callOperationWithSilentSpill(operationSwitchString, string, LinkableConstant::globalObject(*this, node), static_cast<size_t>(data->switchTableIndex), TrustedImmPtr(&unlinkedTable), string);
    farJump(string, JSSwitchPtrTag);
}

void SpeculativeJIT::emitSwitchString(Node* node, SwitchData* data)
{
    switch (node->child1().useKind()) {
    case StringIdentUse: {
        // Note that we do not use JumpTable in this case.
        SpeculateCellOperand op1(this, node->child1());
        GPRTemporary temp(this);
        
        GPRReg op1GPR = op1.gpr();
        GPRReg tempGPR = temp.gpr();
        
        speculateString(node->child1(), op1GPR);
        speculateStringIdentAndLoadStorage(node->child1(), op1GPR, tempGPR);
        
        Vector<int64_t, 16> identifierCaseValues;
        for (unsigned i = 0; i < data->cases.size(); ++i) {
            identifierCaseValues.append(
                static_cast<int64_t>(bitwise_cast<intptr_t>(data->cases[i].value.stringImpl())));
        }
        
        BinarySwitch binarySwitch(tempGPR, identifierCaseValues.span(), BinarySwitch::IntPtr);
        while (binarySwitch.advance(*this))
            jump(data->cases[binarySwitch.caseIndex()].target.block, ForceJump);
        addBranch(binarySwitch.fallThrough(), data->fallThrough.block);
        
        noResult(node);
        break;
    }
        
    case StringUse: {
        SpeculateCellOperand op1(this, node->child1());
        
        GPRReg op1GPR = op1.gpr();
        
        speculateString(node->child1(), op1GPR);

        op1.use();

        emitSwitchStringOnString(node, data, op1GPR);
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    case UntypedUse: {
        JSValueOperand op1(this, node->child1());
        
        JSValueRegs op1Regs = op1.jsValueRegs();
        
        op1.use();
        
        addBranch(branchIfNotCell(op1Regs), data->fallThrough.block);
        
        addBranch(branchIfNotString(op1Regs.payloadGPR()), data->fallThrough.block);
        
        emitSwitchStringOnString(node, data, op1Regs.payloadGPR());
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::emitSwitch(Node* node)
{
    SwitchData* data = node->switchData();
    switch (data->kind) {
    case SwitchImm: {
        emitSwitchImm(node, data);
        return;
    }
    case SwitchChar: {
        emitSwitchChar(node, data);
        return;
    }
    case SwitchString: {
        emitSwitchString(node, data);
        return;
    }
    case SwitchCell: {
        DFG_CRASH(m_graph, node, "Bad switch kind");
        return;
    } }
    RELEASE_ASSERT_NOT_REACHED();
}

void SpeculativeJIT::addBranch(const JumpList& jump, BasicBlock* destination)
{
    for (unsigned i = jump.jumps().size(); i--;)
        addBranch(jump.jumps()[i], destination);
}

void SpeculativeJIT::linkBranches()
{
    for (auto& branch : m_branches)
        branch.jump.linkTo(blockHeads()[branch.destination->index], this);
}

void SpeculativeJIT::compileStoreBarrier(Node* node)
{
    ASSERT(node->op() == StoreBarrier || node->op() == FencedStoreBarrier);
    
    bool isFenced = node->op() == FencedStoreBarrier;
    
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary scratch1(this);
    
    GPRReg baseGPR = base.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    
    JumpList ok;
    
    if (isFenced) {
        ok.append(barrierBranch(vm(), baseGPR, scratch1GPR));
        
        Jump noFence = jumpIfMutatorFenceNotNeeded(vm());
        memoryFence();
        ok.append(barrierBranchWithoutFence(baseGPR));
        noFence.link(this);
    } else
        ok.append(barrierBranchWithoutFence(baseGPR));

    silentSpillAllRegisters(InvalidGPRReg);
    callOperationWithoutExceptionCheck(operationWriteBarrierSlowPath, TrustedImmPtr(&vm()), baseGPR);
    silentFillAllRegisters();

    ok.link(this);

    noResult(node);
}

void SpeculativeJIT::compilePutAccessorById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    SpeculateCellOperand accessor(this, node->child2());

    GPRReg baseGPR = base.gpr();
    GPRReg accessorGPR = accessor.gpr();

    flushRegisters();
    callOperation(node->op() == PutGetterById ? operationPutGetterById : operationPutSetterById, LinkableConstant::globalObject(*this, node), baseGPR, TrustedImmPtr(identifierUID(node->identifierNumber())), node->accessorAttributes(), accessorGPR);

    noResult(node);
}

void SpeculativeJIT::compilePutGetterSetterById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand getter(this, node->child2());
    JSValueOperand setter(this, node->child3());

#if USE(JSVALUE64)
    GPRReg baseGPR = base.gpr();
    GPRReg getterGPR = getter.gpr();
    GPRReg setterGPR = setter.gpr();

    flushRegisters();
    callOperation(operationPutGetterSetter, LinkableConstant::globalObject(*this, node), baseGPR, TrustedImmPtr(identifierUID(node->identifierNumber())), node->accessorAttributes(), getterGPR, setterGPR);
#else
    // These JSValues may be JSUndefined OR JSFunction*.
    // At that time,
    // 1. If the JSValue is JSUndefined, its payload becomes nullptr.
    // 2. If the JSValue is JSFunction*, its payload becomes JSFunction*.
    // So extract payload and pass it to operationPutGetterSetter. This hack is used as the same way in baseline JIT.
    GPRReg baseGPR = base.gpr();
    JSValueRegs getterRegs = getter.jsValueRegs();
    JSValueRegs setterRegs = setter.jsValueRegs();

    flushRegisters();
    callOperation(operationPutGetterSetter, LinkableConstant::globalObject(*this, node), baseGPR, TrustedImmPtr(identifierUID(node->identifierNumber())), node->accessorAttributes(), getterRegs.payloadGPR(), setterRegs.payloadGPR());
#endif

    noResult(node);
}

void SpeculativeJIT::compileResolveScope(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    flushRegisters();
    callOperation(operationResolveScope, resultGPR, LinkableConstant::globalObject(*this, node), scopeGPR, TrustedImmPtr(identifierUID(node->identifierNumber())));
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileResolveScopeForHoistingFuncDeclInEval(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationResolveScopeForHoistingFuncDeclInEval, resultRegs, LinkableConstant::globalObject(*this, node), scopeGPR, TrustedImmPtr(identifierUID(node->identifierNumber())));
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetGlobalVariable(Node* node)
{
    JSValueRegsTemporary result(this);
    JSValueRegs resultRegs = result.regs();
    loadValue(node->variablePointer(), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutGlobalVariable(Node* node)
{
    JSValueOperand value(this, node->child2());
    JSValueRegs valueRegs = value.jsValueRegs();
    storeValue(valueRegs, node->variablePointer());
    noResult(node);
}

void SpeculativeJIT::compileGetDynamicVar(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationGetDynamicVar, resultRegs, LinkableConstant::globalObject(*this, node), scopeGPR, TrustedImmPtr(identifierUID(node->identifierNumber())), node->getPutInfo());
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutDynamicVar(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    JSValueOperand value(this, node->child2());

    GPRReg scopeGPR = scope.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    flushRegisters();
    callOperation(node->ecmaMode().isStrict() ? operationPutDynamicVarStrict : operationPutDynamicVarSloppy, LinkableConstant::globalObject(*this, node), scopeGPR, valueRegs, TrustedImmPtr(identifierUID(node->identifierNumber())), node->getPutInfo());
    noResult(node);
}

void SpeculativeJIT::compileGetClosureVar(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueRegsTemporary result(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs resultRegs = result.regs();

    loadValue(Address(baseGPR, JSLexicalEnvironment::offsetOfVariable(node->scopeOffset())), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutClosureVar(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    storeValue(valueRegs, Address(baseGPR, JSLexicalEnvironment::offsetOfVariable(node->scopeOffset())));
    noResult(node);
}

void SpeculativeJIT::compileGetInternalField(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueRegsTemporary result(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs resultRegs = result.regs();

    loadValue(Address(baseGPR, JSInternalFieldObjectImpl<>::offsetOfInternalField(node->internalFieldIndex())), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutInternalField(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    storeValue(valueRegs, Address(baseGPR, JSInternalFieldObjectImpl<>::offsetOfInternalField(node->internalFieldIndex())));
    noResult(node);
}

void SpeculativeJIT::compilePutAccessorByVal(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand subscript(this, node->child2());
    SpeculateCellOperand accessor(this, node->child3());

    auto operation = node->op() == PutGetterByVal ? operationPutGetterByVal : operationPutSetterByVal;

    GPRReg baseGPR = base.gpr();
    JSValueRegs subscriptRegs = subscript.jsValueRegs();
    GPRReg accessorGPR = accessor.gpr();

    flushRegisters();
    callOperation(operation, LinkableConstant::globalObject(*this, node), baseGPR, subscriptRegs, node->accessorAttributes(), accessorGPR);

    noResult(node);
}

void SpeculativeJIT::compileGetRegExpObjectLastIndex(Node* node)
{
    SpeculateCellOperand regExp(this, node->child1());
    JSValueRegsTemporary result(this);
    GPRReg regExpGPR = regExp.gpr();
    JSValueRegs resultRegs = result.regs();
    speculateRegExpObject(node->child1(), regExpGPR);
    loadValue(Address(regExpGPR, RegExpObject::offsetOfLastIndex()), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileSetRegExpObjectLastIndex(Node* node)
{
    SpeculateCellOperand regExp(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRReg regExpGPR = regExp.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    if (!node->ignoreLastIndexIsWritable()) {
        speculateRegExpObject(node->child1(), regExpGPR);
        speculationCheck(
            ExoticObjectMode, JSValueRegs(), nullptr,
            branchTestPtr(
                NonZero,
                Address(regExpGPR, RegExpObject::offsetOfRegExpAndFlags()),
                TrustedImm32(RegExpObject::lastIndexIsNotWritableFlag)));
    }

    storeValue(valueRegs, Address(regExpGPR, RegExpObject::offsetOfLastIndex()));
    noResult(node);
}

void SpeculativeJIT::compileRegExpExec(Node* node)
{
    bool sample = false;
    if (sample)
        incrementSuperSamplerCount();

    SpeculateCellOperand globalObject(this, node->child1());
    GPRReg globalObjectGPR = globalObject.gpr();

    if (node->child2().useKind() == RegExpObjectUse) {
        if (node->child3().useKind() == StringUse) {
            SpeculateCellOperand base(this, node->child2());
            SpeculateCellOperand argument(this, node->child3());
            GPRReg baseGPR = base.gpr();
            GPRReg argumentGPR = argument.gpr();
            speculateRegExpObject(node->child2(), baseGPR);
            speculateString(node->child3(), argumentGPR);

            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationRegExpExecString, resultRegs, globalObjectGPR, baseGPR, argumentGPR);

            jsValueResult(resultRegs, node);

            if (sample)
                decrementSuperSamplerCount();
            return;
        }

        SpeculateCellOperand base(this, node->child2());
        JSValueOperand argument(this, node->child3());
        GPRReg baseGPR = base.gpr();
        JSValueRegs argumentRegs = argument.jsValueRegs();
        speculateRegExpObject(node->child2(), baseGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationRegExpExec, resultRegs, globalObjectGPR, baseGPR, argumentRegs);

        jsValueResult(resultRegs, node);

        if (sample)
            decrementSuperSamplerCount();
        return;
    }

    JSValueOperand base(this, node->child2());
    JSValueOperand argument(this, node->child3());
    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueRegs argumentRegs = argument.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationRegExpExecGeneric, resultRegs, globalObjectGPR, baseRegs, argumentRegs);

    jsValueResult(resultRegs, node);

    if (sample)
        decrementSuperSamplerCount();
}

void SpeculativeJIT::compileRegExpTest(Node* node)
{
    SpeculateCellOperand globalObject(this, node->child1());
    GPRReg globalObjectGPR = globalObject.gpr();

    if (node->child2().useKind() == RegExpObjectUse) {
        if (node->child3().useKind() == StringUse) {
            SpeculateCellOperand base(this, node->child2());
            SpeculateCellOperand argument(this, node->child3());
            GPRReg baseGPR = base.gpr();
            GPRReg argumentGPR = argument.gpr();
            speculateRegExpObject(node->child2(), baseGPR);
            speculateString(node->child3(), argumentGPR);

            flushRegisters();
            GPRFlushedCallResult result(this);
            callOperation(operationRegExpTestString, result.gpr(), globalObjectGPR, baseGPR, argumentGPR);

            unblessedBooleanResult(result.gpr(), node);
            return;
        }

        SpeculateCellOperand base(this, node->child2());
        JSValueOperand argument(this, node->child3());
        GPRReg baseGPR = base.gpr();
        JSValueRegs argumentRegs = argument.jsValueRegs();
        speculateRegExpObject(node->child2(), baseGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        callOperation(operationRegExpTest, result.gpr(), globalObjectGPR, baseGPR, argumentRegs);

        unblessedBooleanResult(result.gpr(), node);
        return;
    }

    JSValueOperand base(this, node->child2());
    JSValueOperand argument(this, node->child3());
    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueRegs argumentRegs = argument.jsValueRegs();

    flushRegisters();
    GPRFlushedCallResult result(this);
    callOperation(operationRegExpTestGeneric, result.gpr(), globalObjectGPR, baseRegs, argumentRegs);

    unblessedBooleanResult(result.gpr(), node);
}

void SpeculativeJIT::compileStringReplace(Node* node)
{
    ASSERT(node->op() == StringReplace || node->op() == StringReplaceRegExp);
    bool sample = false;
    if (sample)
        incrementSuperSamplerCount();
    auto scopeExit = WTF::makeScopeExit([&] {
        if (sample)
            decrementSuperSamplerCount();
    });

    if (node->child1().useKind() == StringUse
        && node->child2().useKind() == RegExpObjectUse
        && node->child3().useKind() == StringUse) {
        if (JSString* replace = node->child3()->dynamicCastConstant<JSString*>(); replace && !replace->length()) {
            SpeculateCellOperand string(this, node->child1());
            SpeculateCellOperand regExp(this, node->child2());
            GPRReg stringGPR = string.gpr();
            GPRReg regExpGPR = regExp.gpr();
            speculateString(node->child1(), stringGPR);
            speculateRegExpObject(node->child2(), regExpGPR);

            flushRegisters();
            GPRFlushedCallResult result(this);
            callOperation(operationStringProtoFuncReplaceRegExpEmptyStr, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, regExpGPR);
            cellResult(result.gpr(), node);
            return;
        }

        SpeculateCellOperand string(this, node->child1());
        SpeculateCellOperand regExp(this, node->child2());
        SpeculateCellOperand replace(this, node->child3());
        GPRReg stringGPR = string.gpr();
        GPRReg regExpGPR = regExp.gpr();
        GPRReg replaceGPR = replace.gpr();
        speculateString(node->child1(), stringGPR);
        speculateRegExpObject(node->child2(), regExpGPR);
        speculateString(node->child3(), replaceGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        callOperation(operationStringProtoFuncReplaceRegExpString, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, regExpGPR, replaceGPR);
        cellResult(result.gpr(), node);
        return;
    }

    // If we fixed up the edge of child2, we inserted a Check(@child2, String).
    OperandSpeculationMode child2SpeculationMode = AutomaticOperandSpeculation;
    if (node->child2().useKind() == StringUse)
        child2SpeculationMode = ManualOperandSpeculation;

    JSValueOperand string(this, node->child1());
    JSValueOperand search(this, node->child2(), child2SpeculationMode);
    JSValueOperand replace(this, node->child3());
    JSValueRegs stringRegs = string.jsValueRegs();
    JSValueRegs searchRegs = search.jsValueRegs();
    JSValueRegs replaceRegs = replace.jsValueRegs();

    flushRegisters();
    GPRFlushedCallResult result(this);
    callOperation(operationStringProtoFuncReplaceGeneric, result.gpr(), LinkableConstant::globalObject(*this, node), stringRegs, searchRegs, replaceRegs);
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileStringReplaceString(Node* node)
{
    if (node->child3().useKind() == StringUse) {
        const BoyerMooreHorspoolTable<uint8_t>* tablePointer = nullptr;
        String searchString = node->child2()->tryGetString(m_graph);
        if (!!searchString)
            tablePointer = m_graph.tryAddStringSearchTable8(searchString);

        String replacementString = node->child3()->tryGetString(m_graph);
        if (!!replacementString) {
            if (!replacementString.length()) {
                SpeculateCellOperand string(this, node->child1());
                SpeculateCellOperand search(this, node->child2());
                GPRReg stringGPR = string.gpr();
                GPRReg searchGPR = search.gpr();
                speculateString(node->child1(), stringGPR);
                speculateString(node->child2(), searchGPR);

                flushRegisters();
                GPRFlushedCallResult result(this);
                if (tablePointer)
                    callOperation(operationStringReplaceStringEmptyStringWithTable8, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, searchGPR, TrustedImmPtr(tablePointer));
                else
                    callOperation(operationStringReplaceStringEmptyString, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, searchGPR);
                cellResult(result.gpr(), node);
                return;
            }

            if (replacementString.find('$') == notFound) {
                SpeculateCellOperand string(this, node->child1());
                SpeculateCellOperand search(this, node->child2());
                SpeculateCellOperand replace(this, node->child3());
                GPRReg stringGPR = string.gpr();
                GPRReg searchGPR = search.gpr();
                GPRReg replaceGPR = replace.gpr();
                speculateString(node->child1(), stringGPR);
                speculateString(node->child2(), searchGPR);
                speculateString(node->child3(), replaceGPR);

                flushRegisters();
                GPRFlushedCallResult result(this);
                if (tablePointer)
                    callOperation(operationStringReplaceStringStringWithoutSubstitutionWithTable8, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, searchGPR, replaceGPR, TrustedImmPtr(tablePointer));
                else
                    callOperation(operationStringReplaceStringStringWithoutSubstitution, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, searchGPR, replaceGPR);
                cellResult(result.gpr(), node);
                return;
            }
        }

        SpeculateCellOperand string(this, node->child1());
        SpeculateCellOperand search(this, node->child2());
        SpeculateCellOperand replace(this, node->child3());
        GPRReg stringGPR = string.gpr();
        GPRReg searchGPR = search.gpr();
        GPRReg replaceGPR = replace.gpr();
        speculateString(node->child1(), stringGPR);
        speculateString(node->child2(), searchGPR);
        speculateString(node->child3(), replaceGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        if (tablePointer)
            callOperation(operationStringReplaceStringStringWithTable8, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, searchGPR, replaceGPR, TrustedImmPtr(tablePointer));
        else
            callOperation(operationStringReplaceStringString, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, searchGPR, replaceGPR);
        cellResult(result.gpr(), node);
        return;
    }

    // Otherwise, maybe function. Let's call slow path.
    SpeculateCellOperand string(this, node->child1());
    SpeculateCellOperand search(this, node->child2());
    JSValueOperand replace(this, node->child3());

    GPRReg stringGPR = string.gpr();
    GPRReg searchGPR = search.gpr();
    JSValueRegs replaceRegs = replace.jsValueRegs();
    speculateString(node->child1(), stringGPR);
    speculateString(node->child2(), searchGPR);

    flushRegisters();
    GPRFlushedCallResult result(this);
    callOperation(operationStringReplaceStringGeneric, result.gpr(), LinkableConstant::globalObject(*this, node), stringGPR, searchGPR, replaceRegs);
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileRegExpExecNonGlobalOrSticky(Node* node)
{
    SpeculateCellOperand globalObject(this, node->child1());
    SpeculateCellOperand argument(this, node->child2());
    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg argumentGPR = argument.gpr();

    speculateString(node->child2(), argumentGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(
        operationRegExpExecNonGlobalOrSticky, resultRegs,
        globalObjectGPR, LinkableConstant(*this, node->cellOperand()->cell()), argumentGPR);

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileRegExpMatchFastGlobal(Node* node)
{
    SpeculateCellOperand globalObject(this, node->child1());
    SpeculateCellOperand argument(this, node->child2());
    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg argumentGPR = argument.gpr();

    speculateString(node->child2(), argumentGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(
        operationRegExpMatchFastGlobalString, resultRegs,
        globalObjectGPR, LinkableConstant(*this, node->cellOperand()->cell()), argumentGPR);

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileRegExpMatchFast(Node* node)
{
    SpeculateCellOperand globalObject(this, node->child1());
    SpeculateCellOperand base(this, node->child2());
    SpeculateCellOperand argument(this, node->child3());
    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg baseGPR = base.gpr();
    GPRReg argumentGPR = argument.gpr();
    speculateRegExpObject(node->child2(), baseGPR);
    speculateString(node->child3(), argumentGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(
        operationRegExpMatchFastString, resultRegs,
        globalObjectGPR, baseGPR, argumentGPR);

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileLazyJSConstant(Node* node)
{
    JSValueRegsTemporary result(this);
    JSValueRegs resultRegs = result.regs();
    node->lazyJSValue().emit(*this, resultRegs, m_graph.m_plan);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileMaterializeNewObject(Node* node)
{
    RegisteredStructure structure = node->structureSet().at(0);
    ASSERT(m_graph.varArgChild(node, 0)->dynamicCastConstant<Structure*>() == structure.get());

    ObjectMaterializationData& data = node->objectMaterializationData();
        
    IndexingType indexingType = structure->indexingType();
    bool hasIndexingHeader = hasIndexedProperties(indexingType);
    int32_t publicLength = 0;
    int32_t vectorLength = 0;

    if (hasIndexingHeader) {
        for (unsigned i = data.m_properties.size(); i--;) {
            Edge edge = m_graph.varArgChild(node, 1 + i);
            switch (data.m_properties[i].kind()) {
            case PublicLengthPLoc:
                publicLength = edge->asInt32();
                break;
            case VectorLengthPLoc:
                vectorLength = edge->asInt32();
                break;
            default:
                break;
            }
        }
    }

    GPRTemporary result(this);
    GPRTemporary storage(this);
    GPRReg resultGPR = result.gpr();
    GPRReg storageGPR = storage.gpr();
    
    emitAllocateRawObject(resultGPR, structure, storageGPR, 0, vectorLength);

    // After the allocation, we must not exit until we fill butterfly completely.
    
    store32(
        TrustedImm32(publicLength),
        Address(storageGPR, Butterfly::offsetOfPublicLength()));

    for (unsigned i = data.m_properties.size(); i--;) {
        Edge edge = m_graph.varArgChild(node, 1 + i);
        PromotedLocationDescriptor descriptor = data.m_properties[i];
        switch (descriptor.kind()) {
        case IndexedPropertyPLoc: {
            JSValueOperand value(this, edge);
            storeValue(
                value.jsValueRegs(),
                Address(storageGPR, sizeof(EncodedJSValue) * descriptor.info()));
            break;
        }

        case NamedPropertyPLoc: {
            StringImpl* uid = m_graph.identifiers()[descriptor.info()];
            for (const PropertyTableEntry& entry : structure->getPropertiesConcurrently()) {
                if (uid != entry.key())
                    continue;

                JSValueOperand value(this, edge);
                GPRReg baseGPR = isInlineOffset(entry.offset()) ? resultGPR : storageGPR;
                storeValue(
                    value.jsValueRegs(),
                    Address(baseGPR, offsetRelativeToBase(entry.offset())));
            }
            break;
        }
            
        default:
            break;
        }
    }

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileRecordRegExpCachedResult(Node* node)
{
    Edge globalObjectEdge = m_graph.varArgChild(node, 0);
    Edge regExpEdge = m_graph.varArgChild(node, 1);
    Edge stringEdge = m_graph.varArgChild(node, 2);
    Edge startEdge = m_graph.varArgChild(node, 3);
    Edge endEdge = m_graph.varArgChild(node, 4);

    SpeculateCellOperand globalObject(this, globalObjectEdge);
    SpeculateCellOperand regExp(this, regExpEdge);
    SpeculateCellOperand string(this, stringEdge);
    SpeculateInt32Operand start(this, startEdge);
    SpeculateInt32Operand end(this, endEdge);

    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg regExpGPR = regExp.gpr();
    GPRReg stringGPR = string.gpr();
    GPRReg startGPR = start.gpr();
    GPRReg endGPR = end.gpr();

    ptrdiff_t offset = JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult();

    storePtr(
        regExpGPR,
        Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfLastRegExp()));
    storePtr(
        stringGPR,
        Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfLastInput()));
    store32(
        startGPR,
        Address(
            globalObjectGPR,
            offset + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, start)));
    store32(
        endGPR,
        Address(
            globalObjectGPR,
            offset + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, end)));
    store8(
        TrustedImm32(0),
        Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfReified()));

    noResult(node);
}

void SpeculativeJIT::compileDefineDataProperty(Node* node)
{
#if USE(JSVALUE64)
    static_assert(GPRInfo::numberOfRegisters >= 5, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");
#else
    static_assert(GPRInfo::numberOfRegisters >= 6, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");
#endif

    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    GPRReg baseGPR = base.gpr();

    JSValueOperand value(this, m_graph.varArgChild(node, 2));
    JSValueRegs valueRegs = value.jsValueRegs();

    SpeculateInt32Operand attributes(this, m_graph.varArgChild(node, 3));
    GPRReg attributesGPR = attributes.gpr();

    Edge& propertyEdge = m_graph.varArgChild(node, 1);
    switch (propertyEdge.useKind()) {
    case StringUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRReg propertyGPR = property.gpr();
        speculateString(propertyEdge, propertyGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineDataPropertyString, LinkableConstant::globalObject(*this, node), baseGPR, propertyGPR, valueRegs, attributesGPR);
        break;
    }
    case StringIdentUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRTemporary ident(this);

        GPRReg propertyGPR = property.gpr();
        GPRReg identGPR = ident.gpr();

        speculateString(propertyEdge, propertyGPR);
        speculateStringIdentAndLoadStorage(propertyEdge, propertyGPR, identGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineDataPropertyStringIdent, LinkableConstant::globalObject(*this, node), baseGPR, identGPR, valueRegs, attributesGPR);
        break;
    }
    case SymbolUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRReg propertyGPR = property.gpr();
        speculateSymbol(propertyEdge, propertyGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineDataPropertySymbol, LinkableConstant::globalObject(*this, node), baseGPR, propertyGPR, valueRegs, attributesGPR);
        break;
    }
    case UntypedUse: {
        JSValueOperand property(this, propertyEdge);
        JSValueRegs propertyRegs = property.jsValueRegs();

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineDataProperty, LinkableConstant::globalObject(*this, node), baseGPR, propertyRegs, valueRegs, attributesGPR);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    noResult(node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileDefineAccessorProperty(Node* node)
{
#if USE(JSVALUE64)
    static_assert(GPRInfo::numberOfRegisters >= 5, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");
#else
    static_assert(GPRInfo::numberOfRegisters >= 6, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");
#endif

    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    GPRReg baseGPR = base.gpr();

    SpeculateCellOperand getter(this, m_graph.varArgChild(node, 2));
    GPRReg getterGPR = getter.gpr();

    SpeculateCellOperand setter(this, m_graph.varArgChild(node, 3));
    GPRReg setterGPR = setter.gpr();

    SpeculateInt32Operand attributes(this, m_graph.varArgChild(node, 4));
    GPRReg attributesGPR = attributes.gpr();

    Edge& propertyEdge = m_graph.varArgChild(node, 1);
    switch (propertyEdge.useKind()) {
    case StringUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRReg propertyGPR = property.gpr();
        speculateString(propertyEdge, propertyGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineAccessorPropertyString, LinkableConstant::globalObject(*this, node), baseGPR, propertyGPR, getterGPR, setterGPR, attributesGPR);
        break;
    }
    case StringIdentUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRTemporary ident(this);

        GPRReg propertyGPR = property.gpr();
        GPRReg identGPR = ident.gpr();

        speculateString(propertyEdge, propertyGPR);
        speculateStringIdentAndLoadStorage(propertyEdge, propertyGPR, identGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineAccessorPropertyStringIdent, LinkableConstant::globalObject(*this, node), baseGPR, identGPR, getterGPR, setterGPR, attributesGPR);
        break;
    }
    case SymbolUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRReg propertyGPR = property.gpr();
        speculateSymbol(propertyEdge, propertyGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineAccessorPropertySymbol, LinkableConstant::globalObject(*this, node), baseGPR, propertyGPR, getterGPR, setterGPR, attributesGPR);
        break;
    }
    case UntypedUse: {
        JSValueOperand property(this, propertyEdge);
        JSValueRegs propertyRegs = property.jsValueRegs();

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineAccessorProperty, LinkableConstant::globalObject(*this, node), baseGPR, propertyRegs, getterGPR, setterGPR, attributesGPR);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    noResult(node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::emitAllocateButterfly(GPRReg storageResultGPR, GPRReg sizeGPR, GPRReg scratch1, GPRReg scratch2, GPRReg scratch3, JumpList& slowCases)
{
    RELEASE_ASSERT(RegisterSetBuilder(storageResultGPR, sizeGPR, scratch1, scratch2, scratch3).numberOfSetGPRs() == 5);
    ASSERT((1 << 3) == sizeof(JSValue));
    lshift32(sizeGPR, TrustedImm32(3), scratch1);
    add32(TrustedImm32(sizeof(IndexingHeader)), scratch1, scratch2);
#if ASSERT_ENABLED
    Jump didNotOverflow = branch32(AboveOrEqual, scratch2, sizeGPR);
    abortWithReason(UncheckedOverflow);
    didNotOverflow.link(this);
#endif
    emitAllocateVariableSized(
        storageResultGPR, vm().auxiliarySpace(), scratch2, scratch1, scratch3, slowCases);
    addPtr(TrustedImm32(sizeof(IndexingHeader)), storageResultGPR);
    static_assert(Butterfly::offsetOfPublicLength() + static_cast<ptrdiff_t>(sizeof(uint32_t)) == Butterfly::offsetOfVectorLength());
    storePair32(sizeGPR, sizeGPR, storageResultGPR, TrustedImm32(Butterfly::offsetOfPublicLength()));
}

void SpeculativeJIT::compileNormalizeMapKey(Node* node)
{
    ASSERT(node->child1().useKind() == UntypedUse);
    JSValueOperand key(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, key);
    GPRTemporary scratch(this);
    FPRTemporary doubleValue(this);
    FPRTemporary temp(this);

    JSValueRegs keyRegs = key.jsValueRegs();
    JSValueRegs resultRegs = result.regs();
    GPRReg scratchGPR = scratch.gpr();
    FPRReg doubleValueFPR = doubleValue.fpr();
    FPRReg tempFPR = temp.fpr();

    JumpList passThroughCases;
    JumpList doneCases;

    auto isNotCell = branchIfNotCell(keyRegs);
    passThroughCases.append(branchIfNotHeapBigInt(keyRegs.payloadGPR()));
    auto slowPath = jump();
    isNotCell.link(this);

    passThroughCases.append(branchIfNotNumber(keyRegs, scratchGPR));
    passThroughCases.append(branchIfInt32(keyRegs));

#if USE(JSVALUE64)
    unboxDoubleWithoutAssertions(keyRegs.gpr(), scratchGPR, doubleValueFPR);
#else
    unboxDouble(keyRegs.tagGPR(), keyRegs.payloadGPR(), doubleValueFPR);
#endif
    auto notNaN = branchIfNotNaN(doubleValueFPR);
    moveTrustedValue(jsNaN(), resultRegs);
    doneCases.append(jump());

    notNaN.link(this);
    JumpList failureCases;
    branchConvertDoubleToInt32(doubleValueFPR, scratchGPR, failureCases, tempFPR, /* shouldCheckNegativeZero */ false);
    passThroughCases.append(failureCases);

    boxInt32(scratchGPR, resultRegs);
    doneCases.append(jump());

    passThroughCases.link(this);
    moveValueRegs(keyRegs, resultRegs);
    addSlowPathGenerator(slowPathCall(slowPath, this, operationNormalizeMapKeyHeapBigInt, NeedToSpill, ExceptionCheckRequirement::CheckNotNeeded, resultRegs, TrustedImmPtr(&vm()), keyRegs.payloadGPR()));

    doneCases.link(this);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileLoadMapValue(Node* node)
{
    StorageOperand keySlot(this, node->child1());
    JSValueRegsTemporary result(this);

    GPRReg keySlotGPR = keySlot.gpr();
    JSValueRegs resultRegs = result.regs();

    Jump notPresentInTable = branchIfEmpty(keySlotGPR);
    loadValue(Address(keySlotGPR, sizeof(EncodedJSValue)), resultRegs);
    Jump done = jump();

    notPresentInTable.link(this);
    moveValue(jsUndefined(), resultRegs);

    done.link(this);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileIsEmptyStorage(Node* node)
{
    StorageOperand keySlot(this, node->child1());
    GPRTemporary result(this, Reuse, keySlot);

    GPRReg keySlotGPR = keySlot.gpr();
    GPRReg resultGPR = result.gpr();

    comparePtr(Equal, keySlotGPR, TrustedImm32(0), resultGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileMapStorage(Node* node)
{
    SpeculateCellOperand map(this, node->child1());
    GPRReg mapGPR = map.gpr();

    if (node->child1().useKind() == MapObjectUse)
        speculateMapObject(node->child1(), mapGPR);
    else if (node->child1().useKind() == SetObjectUse)
        speculateSetObject(node->child1(), mapGPR);
    else
        RELEASE_ASSERT_NOT_REACHED();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(node->child1().useKind() == MapObjectUse ? operationMapStorage : operationSetStorage, resultRegs, LinkableConstant::globalObject(*this, node), mapGPR);
    cellResult(resultRegs.payloadGPR(), node);
}

void SpeculativeJIT::compileMapIteratorNext(Node* node)
{
    SpeculateCellOperand mapIterator(this, node->child1());
    GPRReg mapIteratorGPR = mapIterator.gpr();

    if (node->child1().useKind() == MapIteratorObjectUse)
        speculateMapIteratorObject(node->child1(), mapIteratorGPR);
    else if (node->child1().useKind() == SetIteratorObjectUse)
        speculateSetIteratorObject(node->child1(), mapIteratorGPR);
    else
        RELEASE_ASSERT_NOT_REACHED();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(node->child1().useKind() == MapIteratorObjectUse ? operationMapIteratorNext : operationSetIteratorNext, resultRegs, LinkableConstant::globalObject(*this, node), mapIteratorGPR);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileMapIteratorKey(Node* node)
{
    SpeculateCellOperand mapIterator(this, node->child1());
    GPRReg mapIteratorGPR = mapIterator.gpr();

    if (node->child1().useKind() == MapIteratorObjectUse)
        speculateMapIteratorObject(node->child1(), mapIteratorGPR);
    else if (node->child1().useKind() == SetIteratorObjectUse)
        speculateSetIteratorObject(node->child1(), mapIteratorGPR);
    else
        RELEASE_ASSERT_NOT_REACHED();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    auto operation = node->child1().useKind() == MapIteratorObjectUse ? operationMapIteratorKey : operationSetIteratorKey;
    callOperation(operation, resultRegs, LinkableConstant::globalObject(*this, node), mapIteratorGPR);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileMapIteratorValue(Node* node)
{
    SpeculateCellOperand mapIterator(this, node->child1());
    GPRReg mapIteratorGPR = mapIterator.gpr();
    ASSERT(node->child1().useKind() == MapIteratorObjectUse);
    speculateMapIteratorObject(node->child1(), mapIteratorGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationMapIteratorValue, resultRegs, LinkableConstant::globalObject(*this, node), mapIteratorGPR);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileMapIterationNext(Node* node)
{
    SpeculateCellOperand mapStorage(this, node->child1());
    SpeculateInt32Operand entry(this, node->child2());

    GPRReg mapStorageGPR = mapStorage.gpr();
    GPRReg entryGPR = entry.gpr();

    speculateImmutableButterfly(node->child1(), mapStorageGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    if (node->bucketOwnerType() == BucketOwnerType::Map)
        callOperation(operationMapIterationNext, resultRegs, LinkableConstant::globalObject(*this, node), mapStorageGPR, entryGPR);
    else
        callOperation(operationSetIterationNext, resultRegs, LinkableConstant::globalObject(*this, node), mapStorageGPR, entryGPR);
    cellResult(resultRegs.payloadGPR(), node);
}

void SpeculativeJIT::compileMapIterationEntry(Node* node)
{
    SpeculateCellOperand mapStorage(this, node->child1());
    GPRReg mapStorageGPR = mapStorage.gpr();

    speculateImmutableButterfly(node->child1(), mapStorageGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    if (node->bucketOwnerType() == BucketOwnerType::Map)
        callOperation(operationMapIterationEntry, resultRegs, LinkableConstant::globalObject(*this, node), mapStorageGPR);
    else
        callOperation(operationSetIterationEntry, resultRegs, LinkableConstant::globalObject(*this, node), mapStorageGPR);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileMapIterationEntryKey(Node* node)
{
    SpeculateCellOperand mapStorage(this, node->child1());
    GPRReg mapStorageGPR = mapStorage.gpr();

    speculateImmutableButterfly(node->child1(), mapStorageGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    if (node->bucketOwnerType() == BucketOwnerType::Map)
        callOperation(operationMapIterationEntryKey, resultRegs, LinkableConstant::globalObject(*this, node), mapStorageGPR);
    else
        callOperation(operationSetIterationEntryKey, resultRegs, LinkableConstant::globalObject(*this, node), mapStorageGPR);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileMapIterationEntryValue(Node* node)
{
    SpeculateCellOperand mapStorage(this, node->child1());
    GPRReg mapStorageGPR = mapStorage.gpr();

    speculateImmutableButterfly(node->child1(), mapStorageGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationMapIterationEntryValue, resultRegs, LinkableConstant::globalObject(*this, node), mapStorageGPR);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileExtractValueFromWeakMapGet(Node* node)
{
    JSValueOperand value(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, value);

    JSValueRegs valueRegs = value.jsValueRegs();
    JSValueRegs resultRegs = result.regs();

#if USE(JSVALUE64)
    moveValueRegs(valueRegs, resultRegs);
    auto done = branchTestPtr(NonZero, resultRegs.payloadGPR());
    moveValue(jsUndefined(), resultRegs);
    done.link(this);
#else
    auto isEmpty = branchIfEmpty(valueRegs.tagGPR());
    moveValueRegs(valueRegs, resultRegs);
    auto done = jump();

    isEmpty.link(this);
    moveValue(jsUndefined(), resultRegs);

    done.link(this);
#endif

    jsValueResult(resultRegs, node, DataFormatJS);
}

void SpeculativeJIT::compileThrow(Node* node)
{
    JSValueOperand value(this, node->child1());
    JSValueRegs valueRegs = value.jsValueRegs();
    flushRegisters();
    callOperation(operationThrowDFG, LinkableConstant::globalObject(*this, node), valueRegs);
    breakpoint();
    noResult(node);
}

void SpeculativeJIT::compileThrowStaticError(Node* node)
{
    SpeculateCellOperand message(this, node->child1());
    GPRReg messageGPR = message.gpr();
    speculateString(node->child1(), messageGPR);
    flushRegisters();
    callOperation(operationThrowStaticError, LinkableConstant::globalObject(*this, node), messageGPR, node->errorType());
    breakpoint();
    noResult(node);
}

void SpeculativeJIT::compileEnumeratorNextUpdateIndexAndMode(Node* node)
{
    Edge baseEdge = m_graph.varArgChild(node, 0);
    SpeculateStrictInt32Operand index(this, m_graph.varArgChild(node, 1));
    SpeculateStrictInt32Operand mode(this, m_graph.varArgChild(node, 2));
    SpeculateCellOperand enumerator(this, m_graph.varArgChild(node, 3));

    GPRReg indexGPR = index.gpr();
    GPRReg modeGPR = mode.gpr();
    GPRReg enumeratorGPR = enumerator.gpr();

    if (node->enumeratorMetadata() == JSPropertyNameEnumerator::IndexedMode) {
        GPRTemporary newIndex(this, Reuse, index);
        GPRTemporary scratch(this);

        speculationCheck(BadCache, JSValueSource(), node, branch32(NotEqual, Address(enumeratorGPR, JSPropertyNameEnumerator::endGenericPropertyIndexOffset()), TrustedImm32(0)));

        Label incrementLoop;
        Jump done;
        compileHasIndexedProperty(node, operationHasEnumerableIndexedProperty, scopedLambda<std::tuple<GPRReg, GPRReg>()>([&] {
            GPRReg newIndexGPR = newIndex.gpr();
            GPRReg scratchGPR = scratch.gpr();

            // This should always be elided because index is UseDef in the bytecode for enumerator_next but we leave the move here for clarity.
            move(indexGPR, newIndexGPR);
            Jump initMode = branchTest32(Zero, modeGPR);

            incrementLoop = label();
            add32(TrustedImm32(1), newIndexGPR);

            initMode.link(this);
            done = branch32(AboveOrEqual, newIndexGPR, Address(enumeratorGPR, JSPropertyNameEnumerator::indexedLengthOffset()));
            return std::make_pair(newIndexGPR, scratchGPR);
        }));
        branchTest32(Zero, scratch.gpr()).linkTo(incrementLoop, this);

        done.link(this);
        if (m_graph.m_tupleData.at(node->tupleOffset() + 1).refCount)
            move(TrustedImm32(static_cast<unsigned>(JSPropertyNameEnumerator::IndexedMode)), scratch.gpr());

        useChildren(node);
        strictInt32TupleResultWithoutUsingChildren(newIndex.gpr(), node, 0);
        strictInt32TupleResultWithoutUsingChildren(scratch.gpr(), node, 1);
        return;
    }

    if (node->enumeratorMetadata() == JSPropertyNameEnumerator::OwnStructureMode && baseEdge.useKind() == CellUse) {
        SpeculateCellOperand base(this, baseEdge);
        GPRTemporary newIndex(this);
        GPRTemporary newMode(this, Reuse, mode);
        GPRReg baseGPR = base.gpr();


        // Has the same structure as the enumerator.
        load32(Address(baseGPR, JSCell::structureIDOffset()), newIndex.gpr());
        speculationCheck(BadCache, JSValueSource(), node, branch32(NotEqual, newIndex.gpr(), Address(enumeratorGPR, JSPropertyNameEnumerator::cachedStructureIDOffset())));

        load32(Address(enumeratorGPR, JSPropertyNameEnumerator::flagsOffset()), newIndex.gpr());
        and32(TrustedImm32(JSPropertyNameEnumerator::enumerationModeMask), newIndex.gpr());
        speculationCheck(BadCache, JSValueSource(), node, branch32(NotEqual, TrustedImm32(JSPropertyNameEnumerator::OwnStructureMode), newIndex.gpr()));

        move(indexGPR, newIndex.gpr());
        Jump initMode = branchTest32(Zero, modeGPR);

        add32(TrustedImm32(1), newIndex.gpr());

        initMode.link(this);

        if (m_graph.m_tupleData.at(node->tupleOffset() + 1).refCount)
            move(TrustedImm32(static_cast<unsigned>(JSPropertyNameEnumerator::OwnStructureMode)), newMode.gpr());

        useChildren(node);
        strictInt32TupleResultWithoutUsingChildren(newIndex.gpr(), node, 0);
        strictInt32TupleResultWithoutUsingChildren(newMode.gpr(), node, 1);
        return;
    }

    JSValueOperand base(this, baseEdge);
#if USE(JSVALUE64)
    GPRTemporary newMode(this, Reuse, mode);
#endif
    JSValueRegs baseRegs = base.regs();


    flushRegisters();
    GPRFlushedCallResult indexResult(this);
    GPRFlushedCallResult2 modeResult(this);
    setupArguments<decltype(operationEnumeratorNextUpdateIndexAndMode)>(LinkableConstant::globalObject(*this, node), baseRegs, indexGPR, modeGPR, enumeratorGPR);
    appendCallSetResult(operationEnumeratorNextUpdateIndexAndMode, indexResult.gpr(), modeResult.gpr());
    exceptionCheck();

    useChildren(node);
    strictInt32TupleResultWithoutUsingChildren(indexResult.gpr(), node, 0);
    strictInt32TupleResultWithoutUsingChildren(modeResult.gpr(), node, 1);
}

void SpeculativeJIT::compileEnumeratorNextUpdatePropertyName(Node* node)
{
    SpeculateStrictInt32Operand indexOperand(this, node->child1());
    SpeculateStrictInt32Operand modeOperand(this, node->child2());
    SpeculateCellOperand enumeratorOperand(this, node->child3());
    GPRTemporary result(this);

    GPRReg index = indexOperand.gpr();
    GPRReg mode = modeOperand.gpr();
    GPRReg enumerator = enumeratorOperand.gpr();
    GPRReg resultGPR = result.gpr();

    OptionSet seenModes = node->enumeratorMetadata();

    JumpList doneCases;
    Jump operationCall;

    // Make sure we flush on all code paths if we will call the operation.
    // Note: we can't omit the operation because we are not guaranteed EnumeratorUpdateIndexAndMode will speculate on the mode.
    flushRegisters();

    if (seenModes.containsAny({ JSPropertyNameEnumerator::OwnStructureMode, JSPropertyNameEnumerator::GenericMode })) {
        operationCall = branchTest32(NonZero, mode, TrustedImm32(JSPropertyNameEnumerator::IndexedMode));

        auto outOfBounds = branch32(AboveOrEqual, index, Address(enumerator, JSPropertyNameEnumerator::endGenericPropertyIndexOffset()));

        loadPtr(Address(enumerator, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset()), resultGPR);
        loadPtr(BaseIndex(resultGPR, index, ScalePtr), resultGPR);
        doneCases.append(jump());

        outOfBounds.link(this);
        loadLinkableConstant(LinkableConstant(*this, vm().smallStrings.sentinelString()), resultGPR);
        doneCases.append(jump());
        operationCall.link(this);
    }

    callOperation(operationEnumeratorNextUpdatePropertyName, resultGPR, LinkableConstant::globalObject(*this, node), index, mode, enumerator);

    doneCases.link(this);
    cellResult(resultGPR, node);
}

template<typename SlowPathFunctionType>
void SpeculativeJIT::compileEnumeratorHasProperty(Node* node, SlowPathFunctionType slowPathFunction)
{
    Edge baseEdge = m_graph.varArgChild(node, 0);
    auto generate = [&] (JSValueRegs baseRegs) {
        JSValueOperand propertyName(this, m_graph.varArgChild(node, 1));
        SpeculateStrictInt32Operand index(this, m_graph.varArgChild(node, 2));
        SpeculateStrictInt32Operand mode(this, m_graph.varArgChild(node, 3));
        SpeculateCellOperand enumerator(this, m_graph.varArgChild(node, 4));

        JSValueRegs propertyNameRegs = propertyName.regs();
        GPRReg indexGPR = index.gpr();
        GPRReg modeGPR = mode.gpr();
        GPRReg enumeratorGPR = enumerator.gpr();

        flushRegisters();

        JSValueRegsTemporary result(this);
        JSValueRegs resultRegs = result.regs();

        JumpList operationCases;

        if (m_state.forNode(baseEdge).m_type & ~SpecCell)
            operationCases.append(branchIfNotCell(baseRegs));

        // FIXME: We shouldn't generate this code if we know base is not a cell.
        operationCases.append(branchTest32(Zero, modeGPR, TrustedImm32(JSPropertyNameEnumerator::OwnStructureMode)));

        load32(Address(baseRegs.payloadGPR(), JSCell::structureIDOffset()), resultRegs.payloadGPR());
        operationCases.append(branch32(NotEqual, resultRegs.payloadGPR(), Address(enumeratorGPR, JSPropertyNameEnumerator::cachedStructureIDOffset())));

        moveTrueTo(resultRegs.payloadGPR());
        Jump done = jump();

        operationCases.link(this);

        if (baseRegs.tagGPR() == InvalidGPRReg)
            callOperation(slowPathFunction, resultRegs, LinkableConstant::globalObject(*this, node), CellValue(baseRegs.payloadGPR()), propertyNameRegs, indexGPR, modeGPR);
        else
            callOperation(slowPathFunction, resultRegs, LinkableConstant::globalObject(*this, node), baseRegs, propertyNameRegs, indexGPR, modeGPR);

        done.link(this);

        blessedBooleanResult(resultRegs.payloadGPR(), node);
    };

    if (isCell(baseEdge.useKind())) {
        SpeculateCellOperand base(this, baseEdge);
        generate(JSValueRegs::payloadOnly(base.gpr()));
    } else {
        JSValueOperand base(this, baseEdge);
        generate(base.regs());
    }
}

void SpeculativeJIT::compileEnumeratorInByVal(Node* node)
{
    compileEnumeratorHasProperty(node, operationEnumeratorInByVal);
}

void SpeculativeJIT::compileEnumeratorHasOwnProperty(Node* node)
{
    compileEnumeratorHasProperty(node, operationEnumeratorHasOwnProperty);
}

void SpeculativeJIT::compilePutByIdWithThis(Node* node)
{
    JSValueOperand base(this, node->child1());
    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueOperand thisValue(this, node->child2());
    JSValueRegs thisRegs = thisValue.jsValueRegs();
    JSValueOperand value(this, node->child3());
    JSValueRegs valueRegs = value.jsValueRegs();

    flushRegisters();
    callOperation(node->ecmaMode().isStrict() ? operationPutByIdWithThisStrict : operationPutByIdWithThis,
        LinkableConstant::globalObject(*this, node), baseRegs, thisRegs, valueRegs, node->cacheableIdentifier().rawBits());

    noResult(node);
}

void SpeculativeJIT::compileGetByOffset(Node* node)
{
    StorageOperand storage(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, storage);

    GPRReg storageGPR = storage.gpr();
    JSValueRegs resultRegs = result.regs();

    StorageAccessData& storageAccessData = node->storageAccessData();

    loadValue(Address(storageGPR, offsetRelativeToBase(storageAccessData.offset)), resultRegs);

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutByOffset(Node* node)
{
    StorageOperand storage(this, node->child1());
    JSValueOperand value(this, node->child3());

    GPRReg storageGPR = storage.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    speculate(node, node->child2());

    StorageAccessData& storageAccessData = node->storageAccessData();

    storeValue(valueRegs, Address(storageGPR, offsetRelativeToBase(storageAccessData.offset)));

    noResult(node);
}

void SpeculativeJIT::compileMatchStructure(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary temp(this);
    GPRReg baseGPR = base.gpr();
    GPRReg tempGPR = temp.gpr();
    
    load32(Address(baseGPR, JSCell::structureIDOffset()), tempGPR);
    
    auto& variants = node->matchStructureData().variants;
    Vector<int64_t, 16> cases;
    for (MatchStructureVariant& variant : variants)
        cases.append(bitwise_cast<int32_t>(variant.structure->id()));
    
    BinarySwitch binarySwitch(tempGPR, cases.span(), BinarySwitch::Int32);
    JumpList done;
    while (binarySwitch.advance(*this)) {
        boxBooleanPayload(variants[binarySwitch.caseIndex()].result, tempGPR);
        done.append(jump());
    }
    speculationCheck(BadCache, JSValueRegs(), node, binarySwitch.fallThrough());
    
    done.link(this);
    
    blessedBooleanResult(tempGPR, node);
}

void SpeculativeJIT::compileGetPropertyEnumerator(Node* node)
{
    if (node->child1().useKind() == CellUse || node->child1().useKind() == CellOrOtherUse) {
        JSValueOperand base(this, node->child1(), ManualOperandSpeculation);
        GPRTemporary scratch1(this);

        speculate(node, node->child1());

        JSValueRegs baseRegs = base.jsValueRegs();
        GPRReg scratch1GPR = scratch1.gpr();

        JumpList slowCases;
        JumpList doneCases;

        if (node->child1().useKind() == CellOrOtherUse) {
            auto notOther = branchIfNotOther(baseRegs, scratch1GPR);
            loadLinkableConstant(LinkableConstant(*this, vm().emptyPropertyNameEnumerator()), scratch1GPR);
            doneCases.append(jump());
            notOther.link(this);
        }

        // We go to the inlined fast path if the object is UndecidedShape / NoIndexingShape for simplicity.
        static_assert(!NonArray);
        static_assert(ArrayClass == 1);
        static_assert(UndecidedShape == 2);
        static_assert(ArrayWithUndecided == 3);
        static_assert(NonArray <= ArrayWithUndecided);
        static_assert(ArrayClass <= ArrayWithUndecided);
        static_assert(ArrayWithUndecided <= ArrayWithUndecided);

        AbstractValue& baseValue = m_state.forNode(node->child1());
        RegisteredStructure onlyStructure;
        StructureRareData* rareData = nullptr;
        bool skipIndexingMaskCheck = false;
        if (baseValue.isType(SpecObject) && baseValue.m_structure.isFinite()) {
            bool hasIndexing = false;
            baseValue.m_structure.forEach([&] (RegisteredStructure structure) {
                if (structure->indexingType() > ArrayWithUndecided)
                    hasIndexing = true;
            });
            if (!hasIndexing)
                skipIndexingMaskCheck = true;
            onlyStructure = baseValue.m_structure.onlyStructure();
            if (onlyStructure)
                rareData = onlyStructure->tryRareData();
        }

        if (!skipIndexingMaskCheck) {
            load8(Address(baseRegs.payloadGPR(), JSCell::indexingTypeAndMiscOffset()), scratch1GPR);
            and32(TrustedImm32(IndexingTypeMask), scratch1GPR);
            slowCases.append(branch32(Above, scratch1GPR, TrustedImm32(ArrayWithUndecided)));
        }

        if (rareData) {
            FrozenValue* frozenRareData = m_graph.freeze(rareData);
            move(TrustedImmPtr(frozenRareData), scratch1GPR);
            loadPtr(Address(scratch1GPR, StructureRareData::offsetOfCachedPropertyNameEnumeratorAndFlag()), scratch1GPR);
        } else {
            if (onlyStructure)
                move(TrustedImmPtr(onlyStructure), scratch1GPR);
            else
                emitLoadStructure(vm(), baseRegs.payloadGPR(), scratch1GPR);
            loadPtr(Address(scratch1GPR, Structure::previousOrRareDataOffset()), scratch1GPR);
            slowCases.append(branchTestPtr(Zero, scratch1GPR));
            slowCases.append(branchIfStructure(scratch1GPR));
            loadPtr(Address(scratch1GPR, StructureRareData::offsetOfCachedPropertyNameEnumeratorAndFlag()), scratch1GPR);
        }

        slowCases.append(branchTestPtr(Zero, scratch1GPR));
        slowCases.append(branchTestPtr(NonZero, scratch1GPR, TrustedImm32(StructureRareData::cachedPropertyNameEnumeratorIsValidatedViaTraversingFlag)));
        doneCases.append(jump());

        slowCases.link(this);
        callOperationWithSilentSpill(operationGetPropertyEnumeratorCell, scratch1GPR, LinkableConstant::globalObject(*this, node), baseRegs.payloadGPR());

        doneCases.link(this);
        cellResult(scratch1GPR, node);
        return;
    }

    JSValueOperand base(this, node->child1());
    JSValueRegs baseRegs = base.jsValueRegs();

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationGetPropertyEnumerator, resultGPR, LinkableConstant::globalObject(*this, node), baseRegs);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetExecutable(Node* node)
{
    SpeculateCellOperand function(this, node->child1());
    GPRTemporary result(this, Reuse, function);
    speculateFunction(node->child1(), function.gpr());
    getExecutable(*this, function.gpr(), result.gpr());
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileGetGetter(Node* node)
{
    SpeculateCellOperand op1(this, node->child1());
    GPRTemporary result(this, Reuse, op1);

    GPRReg op1GPR = op1.gpr();
    GPRReg resultGPR = result.gpr();

    loadPtr(Address(op1GPR, GetterSetter::offsetOfGetter()), resultGPR);

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetSetter(Node* node)
{
    SpeculateCellOperand op1(this, node->child1());
    GPRTemporary result(this, Reuse, op1);

    GPRReg op1GPR = op1.gpr();
    GPRReg resultGPR = result.gpr();

    loadPtr(Address(op1GPR, GetterSetter::offsetOfSetter()), resultGPR);

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetCallee(Node* node)
{
    GPRTemporary result(this);
    loadPtr(payloadFor(CallFrameSlot::callee), result.gpr());
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileSetCallee(Node* node)
{
    SpeculateCellOperand callee(this, node->child1());
    storeCell(callee.gpr(), payloadFor(CallFrameSlot::callee));
    noResult(node);
}

void SpeculativeJIT::compileGetArgumentCountIncludingThis(Node* node)
{
    GPRTemporary result(this);
    VirtualRegister argumentCountRegister;
    if (InlineCallFrame* inlineCallFrame = node->argumentsInlineCallFrame())
        argumentCountRegister = inlineCallFrame->argumentCountRegister;
    else
        argumentCountRegister = CallFrameSlot::argumentCountIncludingThis;
    load32(payloadFor(argumentCountRegister), result.gpr());
    strictInt32Result(result.gpr(), node);
}

void SpeculativeJIT::compileSetArgumentCountIncludingThis(Node* node)
{
    store32(TrustedImm32(node->argumentCountIncludingThis()), payloadFor(CallFrameSlot::argumentCountIncludingThis));
    noResult(node);
}

void SpeculativeJIT::compileStrCat(Node* node)
{
    JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand op2(this, node->child2(), ManualOperandSpeculation);
    JSValueOperand op3(this, node->child3(), ManualOperandSpeculation);

    JSValueRegs op1Regs = op1.jsValueRegs();
    JSValueRegs op2Regs = op2.jsValueRegs();
    JSValueRegs op3Regs;

    if (node->child3())
        op3Regs = op3.jsValueRegs();

    flushRegisters();

    GPRFlushedCallResult result(this);
    if (node->child3())
        callOperation(operationStrCat3, result.gpr(), LinkableConstant::globalObject(*this, node), op1Regs, op2Regs, op3Regs);
    else
        callOperation(operationStrCat2, result.gpr(), LinkableConstant::globalObject(*this, node), op1Regs, op2Regs);

    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileNewArrayBuffer(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    auto* array = node->castOperand<JSImmutableButterfly*>();

    IndexingType indexingMode = node->indexingMode();
    RegisteredStructure structure = m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingMode));

    if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(indexingMode)) {
        GPRTemporary result(this);
        GPRTemporary scratch1(this);
        GPRTemporary scratch2(this);

        GPRReg resultGPR = result.gpr();
        GPRReg scratch1GPR = scratch1.gpr();
        GPRReg scratch2GPR = scratch2.gpr();

        JumpList slowCases;

        emitAllocateJSObject<JSArray>(resultGPR, TrustedImmPtr(structure), TrustedImmPtr(array->toButterfly()), scratch1GPR, scratch2GPR, slowCases, SlowAllocationResult::UndefinedBehavior);

        addSlowPathGenerator(slowPathCall(slowCases, this, operationNewArrayBuffer, result.gpr(), TrustedImmPtr(&vm()), structure, LinkableConstant(*this, array)));

        DFG_ASSERT(m_graph, node, indexingMode & IsArray, indexingMode);
        cellResult(resultGPR, node);
        return;
    }

    flushRegisters();
    GPRFlushedCallResult result(this);

    callOperation(operationNewArrayBuffer, result.gpr(), TrustedImmPtr(&vm()), structure, TrustedImmPtr(node->cellOperand()));

    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileNewArrayWithSize(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(node->indexingType())) {
        SpeculateStrictInt32Operand size(this, node->child1());
        GPRTemporary result(this);

        GPRReg sizeGPR = size.gpr();
        GPRReg resultGPR = result.gpr();

        compileAllocateNewArrayWithSize(node, resultGPR, sizeGPR, node->indexingType());
        cellResult(resultGPR, node);
        return;
    }

    SpeculateStrictInt32Operand size(this, node->child1());
    GPRReg sizeGPR = size.gpr();
    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    GPRReg structureGPR = selectScratchGPR(sizeGPR);
    Jump bigLength = branch32(AboveOrEqual, sizeGPR, TrustedImm32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH));
    move(TrustedImmPtr(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()))), structureGPR);
    Jump done = jump();
    bigLength.link(this);
    move(TrustedImmPtr(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage))), structureGPR);
    done.link(this);
    callOperation(operationNewArrayWithSize, resultGPR, LinkableConstant::globalObject(*this, node), structureGPR, sizeGPR, nullptr);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewArrayWithConstantSize(Node* node)
{
    ASSERT(m_graph.isWatchingHavingABadTimeWatchpoint(node));
    ASSERT(!hasAnyArrayStorage(node->indexingType()));

    GPRTemporary size(this);
    GPRTemporary result(this);

    GPRReg sizeGPR = size.gpr();
    GPRReg resultGPR = result.gpr();

    move(TrustedImm32(node->newArraySize()), sizeGPR);

    constexpr bool shouldConvertLargeSizeToArrayStorage = false;
    compileAllocateNewArrayWithSize(node, resultGPR, sizeGPR, node->indexingType(), shouldConvertLargeSizeToArrayStorage);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewArrayWithSpecies(Node* node)
{
    if (node->child1().useKind() == Int32Use) {
        SpeculateInt32Operand size(this, node->child1());
        SpeculateCellOperand array(this, node->child2());

        GPRReg sizeGPR = size.gpr();
        GPRReg arrayGPR = array.gpr();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationNewArrayWithSpeciesInt32, resultGPR, LinkableConstant::globalObject(*this, node), sizeGPR, arrayGPR, node->indexingType());

        cellResult(resultGPR, node);
        return;
    }

    JSValueOperand size(this, node->child1());
    SpeculateCellOperand array(this, node->child2());

    JSValueRegs sizeRegs = size.jsValueRegs();
    GPRReg arrayGPR = array.gpr();

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationNewArrayWithSpecies, resultGPR, LinkableConstant::globalObject(*this, node), sizeRegs, arrayGPR, node->indexingType());

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewArrayWithSizeAndStructure(Node* node)
{
    SpeculateStrictInt32Operand size(this, node->child1());
    GPRTemporary result(this);

    GPRReg sizeGPR = size.gpr();
    GPRReg resultGPR = result.gpr();

    speculationCheck(OutOfBounds, JSValueRegs(), nullptr, branch32(AboveOrEqual, sizeGPR, TrustedImm32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)));
    constexpr bool shouldConvertLargeSizeToArrayStorage = false;
    compileAllocateNewArrayWithSize(node, resultGPR, sizeGPR, node->structure(), shouldConvertLargeSizeToArrayStorage);
    cellResult(resultGPR, node);
    return;
}

void SpeculativeJIT::compileNewTypedArray(Node* node)
{
    switch (node->child1().useKind()) {
    case Int32Use:
        compileNewTypedArrayWithSize(node);
        break;
#if USE(LARGE_TYPED_ARRAYS)
    case Int52RepUse:
        compileNewTypedArrayWithInt52Size(node);
        break;
#endif
    case UntypedUse: {
        JSValueOperand argument(this, node->child1());
        JSValueRegs argumentRegs = argument.jsValueRegs();

        flushRegisters();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        callOperation(operationNewTypedArrayWithOneArgumentForType(node->typedArrayType()), resultGPR, LinkableConstant::globalObject(*this, node), argumentRegs);

        cellResult(resultGPR, node);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileToThis(Node* node)
{
    ASSERT(node->child1().useKind() == UntypedUse);
    JSValueOperand thisValue(this, node->child1());
    JSValueRegsTemporary temp(this);

    JSValueRegs thisValueRegs = thisValue.jsValueRegs();
    JSValueRegs tempRegs = temp.regs();

    JumpList slowCases;
    slowCases.append(branchIfNotCell(thisValueRegs));
    slowCases.append(branchIfNotObject(thisValueRegs.payloadGPR()));

    moveValueRegs(thisValueRegs, tempRegs);
    auto notScope = branchIfNotType(thisValueRegs.payloadGPR(), JSC::JSTypeRange { JSType(FirstScopeType), JSType(LastScopeType) });
    if (node->ecmaMode().isStrict())
        moveTrustedValue(jsUndefined(), tempRegs);
    else {
        loadLinkableConstant(LinkableConstant::globalObject(*this, node), tempRegs.payloadGPR());
        loadPtr(Address(tempRegs.payloadGPR(), JSGlobalObject::offsetOfGlobalThis()), tempRegs.payloadGPR());
#if USE(JSVALUE32_64)
        move(TrustedImm32(JSValue::CellTag), tempRegs.tagGPR());
#endif
    }

    auto function = &operationToThis;
    if (node->ecmaMode().isStrict())
        function = operationToThisStrict;
    addSlowPathGenerator(slowPathCall(slowCases, this, function, tempRegs, LinkableConstant::globalObject(*this, node), thisValueRegs));

    notScope.link(this);
    jsValueResult(tempRegs, node);
}

void SpeculativeJIT::compileOwnPropertyKeysVariant(Node* node)
{
    switch (node->child1().useKind()) {
    case ObjectUse: {
        if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
            SpeculateCellOperand object(this, node->child1());
            GPRTemporary structure(this);
            GPRTemporary scratch(this);
            GPRTemporary scratch2(this);
            GPRTemporary scratch3(this);
            GPRTemporary result(this);

            GPRReg objectGPR = object.gpr();
            GPRReg structureGPR = structure.gpr();
            GPRReg scratchGPR = scratch.gpr();
            GPRReg scratch2GPR = scratch2.gpr();
            GPRReg scratch3GPR = scratch3.gpr();
            GPRReg resultGPR = result.gpr();

            speculateObject(node->child1(), objectGPR);

            JumpList slowCases;
            emitLoadStructure(vm(), objectGPR, structureGPR);
            loadPtr(Address(structureGPR, Structure::previousOrRareDataOffset()), scratchGPR);

            slowCases.append(branchTestPtr(Zero, scratchGPR));
            slowCases.append(branchIfStructure(scratchGPR));

            loadPtr(Address(scratchGPR, StructureRareData::offsetOfCachedPropertyNames(node->cachedPropertyNamesKind())), scratchGPR);

            ASSERT(bitwise_cast<uintptr_t>(StructureRareData::cachedPropertyNamesSentinel()) == 1);
            slowCases.append(branchPtr(BelowOrEqual, scratchGPR, TrustedImmPtr(bitwise_cast<void*>(StructureRareData::cachedPropertyNamesSentinel()))));

            JumpList slowButArrayBufferCases;

            JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
            RegisteredStructure arrayStructure = m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(CopyOnWriteArrayWithContiguous));

            move(scratchGPR, scratch3GPR);
            addPtr(TrustedImm32(JSImmutableButterfly::offsetOfData()), scratchGPR);

            emitAllocateJSObject<JSArray>(resultGPR, TrustedImmPtr(arrayStructure), scratchGPR, structureGPR, scratch2GPR, slowButArrayBufferCases, SlowAllocationResult::UndefinedBehavior);

            addSlowPathGenerator(slowPathCall(slowButArrayBufferCases, this, operationNewArrayBuffer, resultGPR, TrustedImmPtr(&vm()), arrayStructure, scratch3GPR));

            addSlowPathGenerator(slowPathCall(slowCases, this, operationOwnPropertyKeysVariantObject(node->op()), resultGPR, LinkableConstant::globalObject(*this, node), objectGPR));

            cellResult(resultGPR, node);
            break;
        }

        SpeculateCellOperand object(this, node->child1());

        GPRReg objectGPR = object.gpr();

        speculateObject(node->child1(), objectGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationOwnPropertyKeysVariantObject(node->op()), resultGPR, LinkableConstant::globalObject(*this, node), objectGPR);

        cellResult(resultGPR, node);
        break;
    }

    case UntypedUse: {
        JSValueOperand object(this, node->child1());

        JSValueRegs objectRegs = object.jsValueRegs();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationOwnPropertyKeysVariant(node->op()), resultGPR, LinkableConstant::globalObject(*this, node), objectRegs);

        cellResult(resultGPR, node);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileObjectAssign(Node* node)
{
    SpeculateCellOperand target(this, node->child1());

    switch (node->child2().useKind()) {
    case ObjectUse: {
        SpeculateCellOperand source(this, node->child2());
        GPRTemporary scratch1(this);

        GPRReg targetGPR = target.gpr();
        GPRReg sourceGPR = source.gpr();
        GPRReg scratch1GPR = scratch1.gpr();

        speculateObject(node->child2(), sourceGPR);

        flushRegisters();

        JumpList doneCases;
        JumpList genericCases;

        genericCases.append(branchIfNotType(sourceGPR, FinalObjectType));
        genericCases.append(branchTest8(NonZero, Address(sourceGPR, JSObject::indexingTypeAndMiscOffset()), CCallHelpers::TrustedImm32(IndexingShapeMask)));
        emitLoadStructure(vm(), sourceGPR, scratch1GPR);
        if constexpr (sizeof(Structure::SeenProperties) == sizeof(void*))
            doneCases.append(branchTestPtr(Zero, Address(scratch1GPR, Structure::seenPropertiesOffset())));
        else
            doneCases.append(branchTest32(Zero, Address(scratch1GPR, Structure::seenPropertiesOffset())));

        genericCases.link(this);
        callOperation(operationObjectAssignObject, LinkableConstant::globalObject(*this, node), targetGPR, sourceGPR);

        doneCases.link(this);
        noResult(node);
        return;
    }
    case UntypedUse: {
        JSValueOperand source(this, node->child2());

        GPRReg targetGPR = target.gpr();
        JSValueRegs sourceRegs = source.jsValueRegs();

        flushRegisters();
        callOperation(operationObjectAssignUntyped, LinkableConstant::globalObject(*this, node), targetGPR, sourceRegs);

        noResult(node);
        return;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        return;
    }
}

void SpeculativeJIT::compileObjectCreate(Node* node)
{
    switch (node->child1().useKind()) {
    case ObjectUse: {
        SpeculateCellOperand prototype(this, node->child1());

        GPRReg prototypeGPR = prototype.gpr();

        speculateObject(node->child1(), prototypeGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationObjectCreateObject, resultGPR, LinkableConstant::globalObject(*this, node), prototypeGPR);

        cellResult(resultGPR, node);
        break;
    }

    case UntypedUse: {
        JSValueOperand prototype(this, node->child1());

        JSValueRegs prototypeRegs = prototype.jsValueRegs();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationObjectCreate, resultGPR, LinkableConstant::globalObject(*this, node), prototypeRegs);

        cellResult(resultGPR, node);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileObjectToString(Node* node)
{
    switch (node->child1().useKind()) {
    case UntypedUse: {
        JSValueOperand argument(this, node->child1());
        JSValueRegs argumentRegs = argument.jsValueRegs();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationObjectToStringUntyped, resultGPR, LinkableConstant::globalObject(*this, node), argumentRegs);

        cellResult(resultGPR, node);
        break;
    }
    case ObjectUse: {
        SpeculateCellOperand argument(this, node->child1());
        GPRTemporary result(this);

        GPRReg argumentGPR = argument.gpr();
        GPRReg resultGPR = result.gpr();

        speculateObject(node->child1(), argumentGPR);

        JumpList slowCases;
        emitLoadStructure(vm(), argumentGPR, resultGPR);
        loadPtr(Address(resultGPR, Structure::previousOrRareDataOffset()), resultGPR);

        slowCases.append(branchTestPtr(Zero, resultGPR));
        slowCases.append(branchIfStructure(resultGPR));

        loadPtr(Address(resultGPR, StructureRareData::offsetOfSpecialPropertyCache()), resultGPR);
        slowCases.append(branchTestPtr(Zero, resultGPR));

        loadPtr(Address(resultGPR, SpecialPropertyCache::offsetOfCache(CachedSpecialPropertyKey::ToStringTag) + SpecialPropertyCacheEntry::offsetOfValue()), resultGPR);
        ASSERT(bitwise_cast<uintptr_t>(JSCell::seenMultipleCalleeObjects()) == 1);
        slowCases.append(branchPtr(BelowOrEqual, resultGPR, TrustedImmPtr(bitwise_cast<void*>(JSCell::seenMultipleCalleeObjects()))));

        addSlowPathGenerator(slowPathCall(slowCases, this, operationObjectToStringObjectSlow, resultGPR, LinkableConstant::globalObject(*this, node), argumentGPR));

        cellResult(resultGPR, node);
        break;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad UseKind");
        break;
    }
}

void SpeculativeJIT::compileCreateThis(Node* node)
{
    // Note that there is not so much profit to speculate here. The only things we
    // speculate on are (1) that it's a cell, since that eliminates cell checks
    // later if the proto is reused, and (2) if we have a FinalObject prediction
    // then we speculate because we want to get recompiled if it isn't (since
    // otherwise we'd start taking slow path a lot).

    SpeculateCellOperand callee(this, node->child1());
    GPRTemporary result(this);
    GPRTemporary allocator(this);
    GPRTemporary structure(this);
    GPRTemporary scratch(this);

    GPRReg calleeGPR = callee.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg allocatorGPR = allocator.gpr();
    GPRReg structureGPR = structure.gpr();
    GPRReg scratchGPR = scratch.gpr();
    // Rare data is only used to access the allocator & structure
    // We can avoid using an additional GPR this way
    GPRReg rareDataGPR = structureGPR;
    GPRReg inlineCapacityGPR = rareDataGPR;

    JumpList slowPath;

    slowPath.append(branchIfNotFunction(calleeGPR));
    loadPtr(Address(calleeGPR, JSFunction::offsetOfExecutableOrRareData()), rareDataGPR);
    slowPath.append(branchTestPtr(Zero, rareDataGPR, TrustedImm32(JSFunction::rareDataTag)));
    loadPtr(Address(rareDataGPR, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfAllocator() - JSFunction::rareDataTag), allocatorGPR);
    loadPtr(Address(rareDataGPR, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfStructure() - JSFunction::rareDataTag), structureGPR);

    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject(resultGPR, JITAllocator::variable(), allocatorGPR, structureGPR, butterfly, scratchGPR, slowPath, SlowAllocationResult::UndefinedBehavior);

    load8(Address(structureGPR, Structure::inlineCapacityOffset()), inlineCapacityGPR);
    emitInitializeInlineStorage(resultGPR, inlineCapacityGPR);
    mutatorFence(vm());

    addSlowPathGenerator(slowPathCall(slowPath, this, operationCreateThis, resultGPR, LinkableConstant::globalObject(*this, node), calleeGPR, node->inlineCapacity()));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileCreatePromise(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

    SpeculateCellOperand callee(this, node->child1());
    GPRTemporary result(this);
    GPRTemporary structure(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);

    GPRReg calleeGPR = callee.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg structureGPR = structure.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    // Rare data is only used to access the allocator & structure
    // We can avoid using an additional GPR this way
    GPRReg rareDataGPR = structureGPR;

    move(TrustedImmPtr(m_graph.registerStructure(node->isInternalPromise() ? globalObject->internalPromiseStructure() : globalObject->promiseStructure())), structureGPR);
    auto fastPromisePath = branchLinkableConstant(Equal, calleeGPR, LinkableConstant(*this, node->isInternalPromise() ? globalObject->internalPromiseConstructor() : globalObject->promiseConstructor()));

    JumpList slowCases;

    slowCases.append(branchIfNotFunction(calleeGPR));
    loadPtr(Address(calleeGPR, JSFunction::offsetOfExecutableOrRareData()), rareDataGPR);
    slowCases.append(branchTestPtr(Zero, rareDataGPR, TrustedImm32(JSFunction::rareDataTag)));
    load32(Address(rareDataGPR, FunctionRareData::offsetOfInternalFunctionAllocationProfile() + InternalFunctionAllocationProfile::offsetOfStructureID() - JSFunction::rareDataTag), structureGPR);
    slowCases.append(branchTest32(Zero, structureGPR));
    emitNonNullDecodeZeroExtendedStructureID(structureGPR, structureGPR);
    move(TrustedImmPtr(node->isInternalPromise() ? JSInternalPromise::info() : JSPromise::info()), scratch1GPR);
    slowCases.append(branchCompactPtr(NotEqual, scratch1GPR, Address(structureGPR, Structure::classInfoOffset()), scratch2GPR));
    loadLinkableConstant(LinkableConstant::globalObject(*this, node), scratch1GPR);
    slowCases.append(branchPtr(NotEqual, scratch1GPR, Address(structureGPR, Structure::globalObjectOffset())));

    fastPromisePath.link(this);
    auto butterfly = TrustedImmPtr(nullptr);
    if (node->isInternalPromise())
        emitAllocateJSObjectWithKnownSize<JSInternalPromise>(resultGPR, structureGPR, butterfly, scratch1GPR, scratch2GPR, slowCases, sizeof(JSInternalPromise), SlowAllocationResult::UndefinedBehavior);
    else
        emitAllocateJSObjectWithKnownSize<JSPromise>(resultGPR, structureGPR, butterfly, scratch1GPR, scratch2GPR, slowCases, sizeof(JSPromise), SlowAllocationResult::UndefinedBehavior);
    storeTrustedValue(jsNumber(static_cast<unsigned>(JSPromise::Status::Pending)), Address(resultGPR, JSInternalFieldObjectImpl<>::offsetOfInternalField(static_cast<unsigned>(JSPromise::Field::Flags))));
    storeTrustedValue(jsUndefined(), Address(resultGPR, JSInternalFieldObjectImpl<>::offsetOfInternalField(static_cast<unsigned>(JSPromise::Field::ReactionsOrResult))));
    mutatorFence(vm());

    addSlowPathGenerator(slowPathCall(slowCases, this, node->isInternalPromise() ? operationCreateInternalPromise : operationCreatePromise, resultGPR, LinkableConstant::globalObject(*this, node), calleeGPR));

    cellResult(resultGPR, node);
}


template<typename JSClass, typename Operation>
void SpeculativeJIT::compileCreateInternalFieldObject(Node* node, Operation operation)
{
    SpeculateCellOperand callee(this, node->child1());
    GPRTemporary result(this);
    GPRTemporary structure(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);

    GPRReg calleeGPR = callee.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg structureGPR = structure.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    // Rare data is only used to access the allocator & structure
    // We can avoid using an additional GPR this way
    GPRReg rareDataGPR = structureGPR;

    JumpList slowCases;

    slowCases.append(branchIfNotFunction(calleeGPR));
    loadPtr(Address(calleeGPR, JSFunction::offsetOfExecutableOrRareData()), rareDataGPR);
    slowCases.append(branchTestPtr(Zero, rareDataGPR, TrustedImm32(JSFunction::rareDataTag)));
    load32(Address(rareDataGPR, FunctionRareData::offsetOfInternalFunctionAllocationProfile() + InternalFunctionAllocationProfile::offsetOfStructureID() - JSFunction::rareDataTag), structureGPR);
    slowCases.append(branchTest32(Zero, structureGPR));
    emitNonNullDecodeZeroExtendedStructureID(structureGPR, structureGPR);
    move(TrustedImmPtr(JSClass::info()), scratch1GPR);
    slowCases.append(branchCompactPtr(NotEqual, scratch1GPR, Address(structureGPR, Structure::classInfoOffset()), scratch2GPR));
    loadLinkableConstant(LinkableConstant::globalObject(*this, node), scratch1GPR);
    slowCases.append(branchPtr(NotEqual, scratch1GPR, Address(structureGPR, Structure::globalObjectOffset())));

    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObjectWithKnownSize<JSClass>(resultGPR, structureGPR, butterfly, scratch1GPR, scratch2GPR, slowCases, sizeof(JSClass), SlowAllocationResult::UndefinedBehavior);
    auto initialValues = JSClass::initialValues();
    ASSERT(initialValues.size() == JSClass::numberOfInternalFields);
    for (unsigned index = 0; index < initialValues.size(); ++index)
        storeTrustedValue(initialValues[index], Address(resultGPR, JSInternalFieldObjectImpl<>::offsetOfInternalField(index)));
    mutatorFence(vm());

    addSlowPathGenerator(slowPathCall(slowCases, this, operation, resultGPR, LinkableConstant::globalObject(*this, node), calleeGPR));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileCreateGenerator(Node* node)
{
    compileCreateInternalFieldObject<JSGenerator>(node, operationCreateGenerator);
}

void SpeculativeJIT::compileCreateAsyncGenerator(Node* node)
{
    compileCreateInternalFieldObject<JSAsyncGenerator>(node, operationCreateAsyncGenerator);
}

void SpeculativeJIT::compileNewObject(Node* node)
{
    GPRTemporary result(this);
    GPRTemporary allocator(this);
    GPRTemporary scratch(this);

    GPRReg resultGPR = result.gpr();
    GPRReg allocatorGPR = allocator.gpr();
    GPRReg scratchGPR = scratch.gpr();

    JumpList slowPath;

    RegisteredStructure structure = node->structure();
    size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
    Allocator allocatorValue = allocatorForConcurrently<JSFinalObject>(vm(), allocationSize, AllocatorForMode::AllocatorIfExists);
    if (!allocatorValue)
        slowPath.append(jump());
    else {
        auto butterfly = TrustedImmPtr(nullptr);
        emitAllocateJSObject(resultGPR, JITAllocator::constant(allocatorValue), allocatorGPR, TrustedImmPtr(structure), butterfly, scratchGPR, slowPath);
        emitInitializeInlineStorage(resultGPR, structure->inlineCapacity(), scratchGPR);
        mutatorFence(vm());
    }

    addSlowPathGenerator(slowPathCall(slowPath, this, operationNewObject, resultGPR, TrustedImmPtr(&vm()), structure));

    cellResult(resultGPR, node);
}

template<typename JSClass, typename Operation>
void SpeculativeJIT::compileNewInternalFieldObjectImpl(Node* node, Operation operation)
{
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);

    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

    JumpList slowCases;

    FrozenValue* structure = m_graph.freezeStrong(node->structure().get());
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObjectWithKnownSize<JSClass>(resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR, slowCases, sizeof(JSClass), SlowAllocationResult::UndefinedBehavior);
    auto initialValues = JSClass::initialValues();
    static_assert(initialValues.size() == JSClass::numberOfInternalFields);
    for (unsigned index = 0; index < initialValues.size(); ++index)
        storeTrustedValue(initialValues[index], Address(resultGPR, JSInternalFieldObjectImpl<>::offsetOfInternalField(index)));
    mutatorFence(vm());

    addSlowPathGenerator(slowPathCall(slowCases, this, operation, resultGPR, TrustedImmPtr(&vm()), TrustedImmPtr(structure)));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewGenerator(Node* node)
{
    compileNewInternalFieldObjectImpl<JSGenerator>(node, operationNewGenerator);
}

void SpeculativeJIT::compileNewAsyncGenerator(Node* node)
{
    compileNewInternalFieldObjectImpl<JSAsyncGenerator>(node, operationNewAsyncGenerator);
}

void SpeculativeJIT::compileNewInternalFieldObject(Node* node)
{
    switch (node->structure()->typeInfo().type()) {
    case JSArrayIteratorType:
        compileNewInternalFieldObjectImpl<JSArrayIterator>(node, operationNewArrayIterator);
        break;
    case JSMapIteratorType:
        compileNewInternalFieldObjectImpl<JSMapIterator>(node, operationNewMapIterator);
        break;
    case JSSetIteratorType:
        compileNewInternalFieldObjectImpl<JSSetIterator>(node, operationNewSetIterator);
        break;
    case JSIteratorHelperType:
        compileNewInternalFieldObjectImpl<JSIteratorHelper>(node, operationNewIteratorHelper);
        break;
    case JSPromiseType: {
        if (node->structure()->classInfoForCells() == JSInternalPromise::info())
            compileNewInternalFieldObjectImpl<JSInternalPromise>(node, operationNewInternalPromise);
        else {
            ASSERT(node->structure()->classInfoForCells() == JSPromise::info());
            compileNewInternalFieldObjectImpl<JSPromise>(node, operationNewPromise);
        }
        break;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad structure");
    }
}

void SpeculativeJIT::compileToPrimitive(Node* node)
{
    DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());
    JSValueOperand argument(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, argument);

    JSValueRegs argumentRegs = argument.jsValueRegs();
    JSValueRegs resultRegs = result.regs();

    argument.use();

    Jump alreadyPrimitive = branchIfNotCell(argumentRegs);
    Jump notPrimitive = branchIfObject(argumentRegs.payloadGPR());

    alreadyPrimitive.link(this);
    moveValueRegs(argumentRegs, resultRegs);

    addSlowPathGenerator(slowPathCall(notPrimitive, this, operationToPrimitive, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs));

    jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileToPropertyKey(Node* node)
{
    DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());
    JSValueOperand argument(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, argument);

    JSValueRegs argumentRegs = argument.jsValueRegs();
    JSValueRegs resultRegs = result.regs();

    argument.use();

    JumpList slowCases;
    slowCases.append(branchIfNotCell(argumentRegs));
    Jump alreadyPropertyKey = branchIfSymbol(argumentRegs.payloadGPR());
    slowCases.append(branchIfNotString(argumentRegs.payloadGPR()));

    alreadyPropertyKey.link(this);
    moveValueRegs(argumentRegs, resultRegs);

    addSlowPathGenerator(slowPathCall(slowCases, this, operationToPropertyKey, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs));

    jsValueResult(resultRegs, node, DataFormatJSCell, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileToPropertyKeyOrNumber(Node* node)
{
    DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());
    JSValueOperand argument(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, argument);
    GPRTemporary temp(this);

    JSValueRegs argumentRegs = argument.jsValueRegs();
    JSValueRegs resultRegs = result.regs();
    GPRReg tempGPR = temp.gpr();

    argument.use();

    JumpList alreadyPropertyKey;
    JumpList slowCases;

    alreadyPropertyKey.append(branchIfNumber(argumentRegs, tempGPR));
    slowCases.append(branchIfNotCell(argumentRegs));
    alreadyPropertyKey.append(branchIfSymbol(argumentRegs.payloadGPR()));
    slowCases.append(branchIfNotString(argumentRegs.payloadGPR()));

    alreadyPropertyKey.link(this);
    moveValueRegs(argumentRegs, resultRegs);

    addSlowPathGenerator(slowPathCall(slowCases, this, operationToPropertyKeyOrNumber, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs));

    jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileToNumeric(Node* node)
{
    DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());
    JSValueOperand argument(this, node->child1());
    JSValueRegsTemporary result(this);
    GPRTemporary temp(this);

    JSValueRegs argumentRegs = argument.jsValueRegs();
    JSValueRegs resultRegs = result.regs();
    GPRReg scratch = temp.gpr();
    // FIXME: add a fast path for BigInt32 here.
    // https://bugs.webkit.org/show_bug.cgi?id=211064

    JumpList slowCases;

    Jump notCell = branchIfNotCell(argumentRegs);
    slowCases.append(branchIfNotHeapBigInt(argumentRegs.payloadGPR()));
    Jump isHeapBigInt = jump();

    notCell.link(this);
    slowCases.append(branchIfNotNumber(argumentRegs, scratch));

    isHeapBigInt.link(this);
    moveValueRegs(argumentRegs, resultRegs);

    addSlowPathGenerator(slowPathCall(slowCases, this, operationToNumeric, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs));

    jsValueResult(resultRegs, node, DataFormatJS);
}

void SpeculativeJIT::compileCallNumberConstructor(Node* node)
{
#if USE(BIGINT32)
    if (node->child1().useKind() == BigInt32Use) {
        SpeculateBigInt32Operand operand(this, node->child1());
        GPRTemporary result(this);

        GPRReg operandGPR = operand.gpr();
        GPRReg resultGPR = result.gpr();

        unboxBigInt32(operandGPR, resultGPR);
        strictInt32Result(resultGPR, node);
        return;
    }
#endif

    DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());
    JSValueOperand argument(this, node->child1());
    JSValueRegsTemporary result(this);
    GPRTemporary temp(this);

    JSValueRegs argumentRegs = argument.jsValueRegs();
    JSValueRegs resultRegs = result.regs();
    GPRReg tempGPR = temp.gpr();
    // FIXME: add a fast path for BigInt32 here.
    // https://bugs.webkit.org/show_bug.cgi?id=211064

    JumpList slowCases;
    slowCases.append(branchIfNotNumber(argumentRegs, tempGPR));
    moveValueRegs(argumentRegs, resultRegs);
    addSlowPathGenerator(slowPathCall(slowCases, this, operationCallNumberConstructor, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs));

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileLogShadowChickenPrologue(Node* node)
{
    flushRegisters();
    prepareForExternalCall();
    emitStoreCodeOrigin(node->origin.semantic);

    GPRTemporary scratch1(this, GPRInfo::nonArgGPR0); // This must be a non-argument GPR.
    GPRReg scratch1Reg = scratch1.gpr();
    GPRTemporary scratch2(this);
    GPRReg scratch2Reg = scratch2.gpr();
    GPRTemporary shadowPacket(this);
    GPRReg shadowPacketReg = shadowPacket.gpr();

    ensureShadowChickenPacket(vm(), shadowPacketReg, scratch1Reg, scratch2Reg);

    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeReg = scope.gpr();

    logShadowChickenProloguePacket(shadowPacketReg, scratch1Reg, scopeReg);
    noResult(node);
}

void SpeculativeJIT::compileLogShadowChickenTail(Node* node)
{
    flushRegisters();
    prepareForExternalCall();
    CallSiteIndex callSiteIndex = emitStoreCodeOrigin(node->origin.semantic);

    GPRTemporary scratch1(this, GPRInfo::nonArgGPR0); // This must be a non-argument GPR.
    GPRReg scratch1Reg = scratch1.gpr();
    GPRTemporary scratch2(this);
    GPRReg scratch2Reg = scratch2.gpr();
    GPRTemporary shadowPacket(this);
    GPRReg shadowPacketReg = shadowPacket.gpr();

    ensureShadowChickenPacket(vm(), shadowPacketReg, scratch1Reg, scratch2Reg);

    JSValueOperand thisValue(this, node->child1());
    JSValueRegs thisRegs = thisValue.jsValueRegs();
    SpeculateCellOperand scope(this, node->child2());
    GPRReg scopeReg = scope.gpr();

    emitGetFromCallFrameHeaderPtr(CallFrameSlot::codeBlock, scratch1Reg);
    logShadowChickenTailPacket(shadowPacketReg, thisRegs, scopeReg, scratch1Reg, callSiteIndex);
    noResult(node);
}

void SpeculativeJIT::compileSetAdd(Node* node)
{
    SpeculateCellOperand set(this, node->child1());
    JSValueOperand key(this, node->child2());
    SpeculateInt32Operand hash(this, node->child3());

    GPRReg setGPR = set.gpr();
    JSValueRegs keyRegs = key.jsValueRegs();
    GPRReg hashGPR = hash.gpr();

    speculateSetObject(node->child1(), setGPR);

    flushRegisters();
    callOperation(operationSetAdd, LinkableConstant::globalObject(*this, node), setGPR, keyRegs, hashGPR);
    noResult(node);
}

void SpeculativeJIT::compileMapSet(Node* node)
{
    SpeculateCellOperand map(this, m_graph.varArgChild(node, 0));
    JSValueOperand key(this, m_graph.varArgChild(node, 1));
    JSValueOperand value(this, m_graph.varArgChild(node, 2));
    SpeculateInt32Operand hash(this, m_graph.varArgChild(node, 3));

    GPRReg mapGPR = map.gpr();
    JSValueRegs keyRegs = key.jsValueRegs();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg hashGPR = hash.gpr();

    speculateMapObject(m_graph.varArgChild(node, 0), mapGPR);

    flushRegisters();
    callOperation(operationMapSet, LinkableConstant::globalObject(*this, node), mapGPR, keyRegs, valueRegs, hashGPR);
    noResult(node);
}

void SpeculativeJIT::compileMapOrSetDelete(Node* node)
{
    SpeculateCellOperand mapOrSet(this, node->child1());
    JSValueOperand key(this, node->child2());
    SpeculateInt32Operand hash(this, node->child3());

    GPRReg mapOrSetGPR = mapOrSet.gpr();
    JSValueRegs keyRegs = key.jsValueRegs();
    GPRReg hashGPR = hash.gpr();

    if (node->child1().useKind() == MapObjectUse)
        speculateMapObject(node->child1(), mapOrSetGPR);
    else if (node->child1().useKind() == SetObjectUse)
        speculateSetObject(node->child1(), mapOrSetGPR);
    else
        RELEASE_ASSERT_NOT_REACHED();

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(node->child1().useKind() == MapObjectUse ? operationMapDelete : operationSetDelete, resultGPR, LinkableConstant::globalObject(*this, node), mapOrSetGPR, keyRegs, hashGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileWeakMapGet(Node* node)
{
    GPRTemporary mask(this);
    GPRTemporary buffer(this);
    JSValueRegsTemporary result(this);

    GPRReg maskGPR = mask.gpr();
    GPRReg bufferGPR = buffer.gpr();
    JSValueRegs resultRegs = result.regs();

    GPRTemporary index;
    GPRReg indexGPR { InvalidGPRReg };
    {
        SpeculateInt32Operand hash(this, node->child3());
        GPRReg hashGPR = hash.gpr();
        index = GPRTemporary(this, Reuse, hash);
        indexGPR = index.gpr();
        move(hashGPR, indexGPR);
    }

    {
        SpeculateCellOperand weakMap(this, node->child1());
        GPRReg weakMapGPR = weakMap.gpr();
        if (node->child1().useKind() == WeakMapObjectUse)
            speculateWeakMapObject(node->child1(), weakMapGPR);
        else
            speculateWeakSetObject(node->child1(), weakMapGPR);

        static_assert(WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfCapacity() == WeakMapImpl<WeakMapBucket<WeakMapBucketDataKeyValue>>::offsetOfCapacity());
        static_assert(WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfBuffer() == WeakMapImpl<WeakMapBucket<WeakMapBucketDataKeyValue>>::offsetOfBuffer());
        load32(Address(weakMapGPR, WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfCapacity()), maskGPR);
        loadPtr(Address(weakMapGPR, WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfBuffer()), bufferGPR);
    }

    SpeculateCellOperand key(this, node->child2());
    GPRReg keyGPR = key.gpr();

    // We are not checking whether Symbol is registered one or not, but it is OK since Symbol and SymbolImpl are one-to-one.
    // If the key is not registered Symbol, it never matches against an element in this WeakMap.
    if (node->child2().useKind() == ObjectUse)
        speculateObject(node->child2(), keyGPR);
    else if (node->child2().useKind() == SymbolUse)
        speculateSymbol(node->child2(), keyGPR);

#if USE(JSVALUE32_64)
    GPRReg bucketGPR = resultRegs.tagGPR();
#else
    GPRTemporary bucket(this);
    GPRReg bucketGPR = bucket.gpr();
#endif

    sub32(TrustedImm32(1), maskGPR);

    Label loop = label();
    and32(maskGPR, indexGPR);
    if (node->child1().useKind() == WeakSetObjectUse) {
        static_assert(sizeof(WeakMapBucket<WeakMapBucketDataKey>) == sizeof(void*));
        zeroExtend32ToWord(indexGPR, bucketGPR);
        lshiftPtr(Imm32(sizeof(void*) == 4 ? 2 : 3), bucketGPR);
        addPtr(bufferGPR, bucketGPR);
    } else {
        ASSERT(node->child1().useKind() == WeakMapObjectUse);
        static_assert(sizeof(WeakMapBucket<WeakMapBucketDataKeyValue>) == 16);
        zeroExtend32ToWord(indexGPR, bucketGPR);
        lshiftPtr(Imm32(4), bucketGPR);
        addPtr(bufferGPR, bucketGPR);
    }

    loadPtr(Address(bucketGPR, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfKey()), resultRegs.payloadGPR());

    // They're definitely the same value, we found the bucket we were looking for!
    // The deleted key comparison is also done with this.
    auto found = branchPtr(Equal, resultRegs.payloadGPR(), keyGPR);

    auto notPresentInTable = branchTestPtr(Zero, resultRegs.payloadGPR());

    add32(TrustedImm32(1), indexGPR);
    jump().linkTo(loop, this);

#if USE(JSVALUE32_64)
    notPresentInTable.link(this);
    moveValue(JSValue(), resultRegs);
    auto notPresentInTableDone = jump();

    found.link(this);
    if (node->child1().useKind() == WeakSetObjectUse)
        move(TrustedImm32(JSValue::CellTag), resultRegs.tagGPR());
    else
        loadValue(Address(bucketGPR, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfValue()), resultRegs);

    notPresentInTableDone.link(this);
#else
    notPresentInTable.link(this);
    found.link(this);

    // In 64bit environment, Empty bucket has JSEmpty value. Empty key is JSEmpty.
    // If empty bucket is found, we can use the same path used for the case of finding a bucket.
    if (node->child1().useKind() == WeakMapObjectUse)
        loadValue(Address(bucketGPR, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfValue()), resultRegs);
#endif

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileWeakSetAdd(Node* node)
{
    SpeculateCellOperand set(this, node->child1());
    SpeculateCellOperand key(this, node->child2());
    SpeculateInt32Operand hash(this, node->child3());

    GPRReg setGPR = set.gpr();
    GPRReg keyGPR = key.gpr();
    GPRReg hashGPR = hash.gpr();

    speculateWeakSetObject(node->child1(), setGPR);
    if (node->child2().useKind() == ObjectUse)
        speculateObject(node->child2(), keyGPR);

    flushRegisters();
    callOperation(operationWeakSetAdd, LinkableConstant::globalObject(*this, node), setGPR, keyGPR, hashGPR);
    noResult(node);
}

void SpeculativeJIT::compileWeakMapSet(Node* node)
{
    SpeculateCellOperand map(this, m_graph.varArgChild(node, 0));
    SpeculateCellOperand key(this, m_graph.varArgChild(node, 1));
    JSValueOperand value(this, m_graph.varArgChild(node, 2));
    SpeculateInt32Operand hash(this, m_graph.varArgChild(node, 3));

    GPRReg mapGPR = map.gpr();
    GPRReg keyGPR = key.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg hashGPR = hash.gpr();

    speculateWeakMapObject(m_graph.varArgChild(node, 0), mapGPR);
    if (m_graph.varArgChild(node, 1).useKind() == ObjectUse)
        speculateObject(m_graph.varArgChild(node, 1), keyGPR);

    flushRegisters();
    callOperation(operationWeakMapSet, LinkableConstant::globalObject(*this, node), mapGPR, keyGPR, valueRegs, hashGPR);
    noResult(node);
}

void SpeculativeJIT::compileGetPrototypeOf(Node* node)
{
    GPRTemporary temp(this);

    GPRReg tempGPR = temp.gpr();

#if USE(JSVALUE64)
    JSValueRegs resultRegs(tempGPR);
#else
    GPRTemporary temp2(this);
    JSValueRegs resultRegs(temp2.gpr(), tempGPR);
#endif

    switch (node->child1().useKind()) {
    case ArrayUse:
    case FunctionUse:
    case FinalObjectUse: {
        SpeculateCellOperand object(this, node->child1());
        GPRReg objectGPR = object.gpr();

        switch (node->child1().useKind()) {
        case ArrayUse:
            speculateArray(node->child1(), objectGPR);
            break;
        case FunctionUse:
            speculateFunction(node->child1(), objectGPR);
            break;
        case FinalObjectUse:
            speculateFinalObject(node->child1(), objectGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        emitLoadStructure(vm(), objectGPR, tempGPR);

        AbstractValue& value = m_state.forNode(node->child1());
        if ((value.m_type && !(value.m_type & ~SpecObject)) && value.m_structure.isFinite()) {
            bool hasPolyProto = false;
            bool hasMonoProto = false;
            value.m_structure.forEach([&] (RegisteredStructure structure) {
                if (structure->hasPolyProto())
                    hasPolyProto = true;
                else
                    hasMonoProto = true;
            });

            if (hasMonoProto && !hasPolyProto) {
                loadValue(Address(tempGPR, Structure::prototypeOffset()), resultRegs);
                jsValueResult(resultRegs, node);
                return;
            }

            if (hasPolyProto && !hasMonoProto) {
                loadValue(Address(objectGPR, offsetRelativeToBase(knownPolyProtoOffset)), resultRegs);
                jsValueResult(resultRegs, node);
                return;
            }
        }

        loadValue(Address(tempGPR, Structure::prototypeOffset()), resultRegs);
        auto hasMonoProto = branchIfNotEmpty(resultRegs);
        loadValue(Address(objectGPR, offsetRelativeToBase(knownPolyProtoOffset)), resultRegs);
        hasMonoProto.link(this);
        jsValueResult(resultRegs, node);
        return;
    }
    case ObjectUse: {
        SpeculateCellOperand object(this, node->child1());
        GPRReg objectGPR = object.gpr();
        speculateObject(node->child1(), objectGPR);

        JumpList slowCases;
        emitLoadPrototype(vm(), objectGPR, resultRegs, slowCases);
        addSlowPathGenerator(slowPathCall(slowCases, this, operationGetPrototypeOfObject,
            resultRegs, LinkableConstant::globalObject(*this, node), objectGPR));

        jsValueResult(resultRegs, node);
        return;
    }
    default: {
        JSValueOperand value(this, node->child1());
        JSValueRegs valueRegs = value.jsValueRegs();

        JumpList slowCases;
        slowCases.append(branchIfNotCell(valueRegs));

        GPRReg valueGPR = valueRegs.payloadGPR();
        slowCases.append(branchIfNotObject(valueGPR));

        emitLoadPrototype(vm(), valueGPR, resultRegs, slowCases);
        addSlowPathGenerator(slowPathCall(slowCases, this, operationGetPrototypeOf,
            resultRegs, LinkableConstant::globalObject(*this, node), valueRegs));

        jsValueResult(resultRegs, node);
        return;
    }
    }
}

void SpeculativeJIT::compileGetWebAssemblyInstanceExports(Node* node)
{
#if ENABLE(WEBASSEMBLY)
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary result(this);

    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();

    loadPtr(Address(baseGPR, JSWebAssemblyInstance::offsetOfModuleRecord()), resultGPR);
    loadPtr(Address(resultGPR, WebAssemblyModuleRecord::offsetOfExportsObject()), resultGPR);

    cellResult(resultGPR, node);
#else
    UNUSED_PARAM(node);
#endif
}

void SpeculativeJIT::compileIdentity(Node* node)
{
    speculate(node, node->child1());
    switch (node->child1().useKind()) {
#if USE(JSVALUE64)
    case DoubleRepAnyIntUse:
#endif
    case DoubleRepUse:
    case DoubleRepRealUse: {
        SpeculateDoubleOperand op(this, node->child1());
        FPRTemporary scratch(this, op);
        moveDouble(op.fpr(), scratch.fpr());
        doubleResult(scratch.fpr(), node);
        break;
    }
#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateInt52Operand op(this, node->child1());
        GPRTemporary result(this, Reuse, op);
        move(op.gpr(), result.gpr());
        int52Result(result.gpr(), node);
        break;
    }
#endif
    default: {
        JSValueOperand op(this, node->child1(), ManualOperandSpeculation);
        JSValueRegsTemporary result(this, Reuse, op);
        JSValueRegs opRegs = op.jsValueRegs();
        JSValueRegs resultRegs = result.regs();
        moveValueRegs(opRegs, resultRegs);
        jsValueResult(resultRegs, node);
        break;
    }
    }
}

void SpeculativeJIT::compileExtractFromTuple(Node* node)
{
    RELEASE_ASSERT(node->child1().useKind() == UntypedUse);

    ASSERT(m_graph.m_tupleData.at(node->tupleIndex()).virtualRegister == node->virtualRegister());
    VirtualRegister virtualRegister = node->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
#if ASSERT_ENABLED
    ASSERT(m_graph.m_tupleData.at(node->tupleIndex()).resultFlags == node->result());
    switch (node->result()) {
    case NodeResultJS:
    case NodeResultNumber:
        ASSERT(info.isFormat(DataFormatJS));
        break;
    case NodeResultDouble:
        ASSERT(info.isFormat(DataFormatDouble) || info.isFormat(DataFormatJSDouble));
        break;
    case NodeResultInt32:
        ASSERT(info.isFormat(DataFormatInt32) || info.isFormat(DataFormatJSInt32));
        break;
    case NodeResultBoolean:
        ASSERT(info.isFormat(DataFormatBoolean) || info.isFormat(DataFormatJSBoolean));
        break;
    case NodeResultStorage:
        ASSERT(info.isFormat(DataFormatStorage));
        break;

    // FIXME: These are not supported because it wasn't exactly clear how to implement them and they are not currently used.
    case NodeResultInt52:
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
#endif
    info.initFromTupleResult(node);
}

void SpeculativeJIT::compileBitwiseStrictEq(Node* node)
{
    JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand op2(this, node->child2(), ManualOperandSpeculation);
    GPRTemporary result(this);

    JSValueRegs op1Regs = op1.jsValueRegs();
    JSValueRegs op2Regs = op2.jsValueRegs();

    speculate(node, node->child1());
    speculate(node, node->child2());

#if USE(JSVALUE64)
    compare64(Equal, op1Regs.payloadGPR(), op2Regs.payloadGPR(), result.gpr());
#else
    move(TrustedImm32(0), result.gpr());
    Jump notEqual = branch32(NotEqual, op1Regs.tagGPR(), op2Regs.tagGPR());
    compare32(Equal, op1Regs.payloadGPR(), op2Regs.payloadGPR(), result.gpr());
    notEqual.link(this);
#endif
    unblessedBooleanResult(result.gpr(), node);
}

void SpeculativeJIT::emitInitializeButterfly(GPRReg storageGPR, GPRReg sizeGPR, JSValueRegs emptyValueRegs, GPRReg scratchGPR)
{
    zeroExtend32ToWord(sizeGPR, scratchGPR);
    Jump done = branchTest32(Zero, scratchGPR);
    Label loop = label();
    sub32(TrustedImm32(1), scratchGPR);
    storeValue(emptyValueRegs, BaseIndex(storageGPR, scratchGPR, TimesEight));
    branchTest32(NonZero, scratchGPR).linkTo(loop, this);
    done.link(this);
}

void SpeculativeJIT::compileAllocateNewArrayWithSize(Node* node, GPRReg resultGPR, GPRReg sizeGPR, RegisteredStructure structure, bool shouldConvertLargeSizeToArrayStorage)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

    GPRTemporary storage(this);
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);

    GPRReg storageGPR = storage.gpr();
    GPRReg scratchGPR = scratch.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

    move(TrustedImmPtr(nullptr), storageGPR);

    JumpList slowCases;
    if (shouldConvertLargeSizeToArrayStorage)
        slowCases.append(branch32(AboveOrEqual, sizeGPR, TrustedImm32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)));
#if ASSERT_ENABLED
    else {
        Jump lengthIsWithinLimits;
        lengthIsWithinLimits = branch32(Below, sizeGPR, TrustedImm32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH));
        abortWithReason(UncheckedOverflow);
        lengthIsWithinLimits.link(this);
    }
#endif // ASSERT_ENABLED

    // We can use resultGPR as a scratch right now.
    emitAllocateButterfly(storageGPR, sizeGPR, scratchGPR, scratch2GPR, resultGPR, slowCases);

#if USE(JSVALUE64)
    JSValueRegs emptyValueRegs(scratchGPR);
    if (hasDouble(structure->indexingType()))
        move(TrustedImm64(bitwise_cast<int64_t>(PNaN)), emptyValueRegs.gpr());
    else
        move(TrustedImm64(JSValue::encode(JSValue())), emptyValueRegs.gpr());
#else
    JSValueRegs emptyValueRegs(scratchGPR, scratch2GPR);
    if (hasDouble(structure->indexingType()))
        moveValue(JSValue(JSValue::EncodeAsDouble, PNaN), emptyValueRegs);
    else
        moveValue(JSValue(), emptyValueRegs);
#endif
    emitInitializeButterfly(storageGPR, sizeGPR, emptyValueRegs, resultGPR);

    emitAllocateJSObject<JSArray>(resultGPR, TrustedImmPtr(structure), storageGPR, scratchGPR, scratch2GPR, slowCases, SlowAllocationResult::UndefinedBehavior);

    mutatorFence(vm());

    addSlowPathGenerator(makeUnique<CallArrayAllocatorWithVariableSizeSlowPathGenerator>(
        slowCases, this, operationNewArrayWithSize, resultGPR,
        LinkableConstant::globalObject(*this, node),
        structure,
        shouldConvertLargeSizeToArrayStorage ? m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage)) : structure,
        sizeGPR, storageGPR));
}

void SpeculativeJIT::compileAllocateNewArrayWithSize(Node* node, GPRReg resultGPR, GPRReg sizeGPR, IndexingType indexingType, bool shouldConvertLargeSizeToArrayStorage)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    compileAllocateNewArrayWithSize(node, resultGPR, sizeGPR, m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType)), shouldConvertLargeSizeToArrayStorage);
}

void SpeculativeJIT::compileHasIndexedProperty(Node* node, S_JITOperation_GCZ slowPathOperation, const ScopedLambda<std::tuple<GPRReg, GPRReg>()>& prefix)
{
    auto baseEdge = m_graph.varArgChild(node, 0);
    SpeculateCellOperand base(this, baseEdge);

    GPRReg baseGPR = base.gpr();
    GPRReg indexGPR = InvalidGPRReg;
    GPRReg resultGPR = InvalidGPRReg;

    if (baseEdge.useKind() == ObjectUse)
        speculateObject(baseEdge, baseGPR);

    JumpList slowCases;
    ArrayMode mode = node->arrayMode();
    switch (mode.type()) {
    case Array::Int32:
    case Array::Contiguous: {
        ASSERT(!!m_graph.varArgChild(node, node->storageChildIndex()));
        StorageOperand storage(this, m_graph.varArgChild(node, node->storageChildIndex()));
        GPRTemporary scratch(this);

        GPRReg storageGPR = storage.gpr();
        GPRReg scratchGPR = scratch.gpr();

        std::tie(indexGPR, resultGPR) = prefix();

        Jump outOfBounds = branch32(AboveOrEqual, indexGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));

        if (mode.isInBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, outOfBounds);
        else
            slowCases.append(outOfBounds);

#if USE(JSVALUE64)
        load64(BaseIndex(storageGPR, indexGPR, TimesEight), scratchGPR);
#else
        load32(BaseIndex(storageGPR, indexGPR, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), scratchGPR);
#endif

        if (mode.isInBoundsSaneChain()) {
            isNotEmpty(scratchGPR, resultGPR);
            break;
        }

        Jump isHole = branchIfEmpty(scratchGPR);
        if (!mode.isInBounds())
            slowCases.append(isHole);
        else
            speculationCheck(LoadFromHole, JSValueRegs(), nullptr, isHole);
        move(TrustedImm32(1), resultGPR);
        break;
    }
    case Array::Double: {
        ASSERT(!!m_graph.varArgChild(node, node->storageChildIndex()));
        StorageOperand storage(this, m_graph.varArgChild(node, node->storageChildIndex()));
        FPRTemporary scratch(this);
        FPRReg scratchFPR = scratch.fpr();
        GPRReg storageGPR = storage.gpr();

        std::tie(indexGPR, resultGPR) = prefix();

        Jump outOfBounds = branch32(AboveOrEqual, indexGPR, Address(storageGPR, Butterfly::offsetOfPublicLength()));

        if (mode.isInBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, outOfBounds);
        else
            slowCases.append(outOfBounds);

        loadDouble(BaseIndex(storageGPR, indexGPR, TimesEight), scratchFPR);

        if (mode.isInBoundsSaneChain()) {
            compareDouble(DoubleEqualAndOrdered, scratchFPR, scratchFPR, resultGPR);
            break;
        }

        Jump isHole = branchIfNaN(scratchFPR);
        if (!mode.isInBounds())
            slowCases.append(isHole);
        else
            speculationCheck(LoadFromHole, JSValueRegs(), nullptr, isHole);
        move(TrustedImm32(1), resultGPR);
        break;
    }
    case Array::ArrayStorage: {
        ASSERT(!!m_graph.varArgChild(node, node->storageChildIndex()));
        StorageOperand storage(this, m_graph.varArgChild(node, node->storageChildIndex()));
        GPRTemporary scratch(this);

        GPRReg storageGPR = storage.gpr();
        GPRReg scratchGPR = scratch.gpr();

        std::tie(indexGPR, resultGPR) = prefix();

        Jump outOfBounds = branch32(AboveOrEqual, indexGPR, Address(storageGPR, ArrayStorage::vectorLengthOffset()));
        if (mode.isInBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, outOfBounds);
        else
            slowCases.append(outOfBounds);

#if USE(JSVALUE64)
        load64(BaseIndex(storageGPR, indexGPR, TimesEight, ArrayStorage::vectorOffset()), scratchGPR);
#else
        load32(BaseIndex(storageGPR, indexGPR, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), scratchGPR);
#endif

        if (mode.isInBoundsSaneChain()) {
            isNotEmpty(scratchGPR, resultGPR);
            break;
        }

        Jump isHole = branchIfEmpty(scratchGPR);
        if (!mode.isInBounds() || mode.isInBoundsSaneChain())
            slowCases.append(isHole);
        else
            speculationCheck(LoadFromHole, JSValueRegs(), nullptr, isHole);
        move(TrustedImm32(1), resultGPR);
        break;
    }
    default: {
        // FIXME: Optimize TypedArrays in HasIndexedProperty IC
        // https://bugs.webkit.org/show_bug.cgi?id=221183
        std::tie(indexGPR, resultGPR) = prefix();
        slowCases.append(jump());
        break;
    }
    }

    addSlowPathGenerator(slowPathCall(slowCases, this, slowPathOperation, resultGPR, LinkableConstant::globalObject(*this, node), baseGPR, indexGPR));
}

void SpeculativeJIT::compileExtractCatchLocal(Node* node)
{
    JSValueRegsTemporary result(this);
    JSValueRegs resultRegs = result.regs();

    JSValue* ptr = &reinterpret_cast<JSValue*>(jitCode()->common.catchOSREntryBuffer->dataBuffer())[node->catchOSREntryIndex()];
    loadValue(ptr, resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileClearCatchLocals(Node* node)
{
    ScratchBuffer* scratchBuffer = jitCode()->common.catchOSREntryBuffer;
    ASSERT(scratchBuffer);
    GPRTemporary scratch(this);
    GPRReg scratchGPR = scratch.gpr();
    move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), scratchGPR);
    storePtr(TrustedImmPtr(nullptr), Address(scratchGPR));
    noResult(node);
}

void SpeculativeJIT::compileProfileType(Node* node)
{
    JSValueOperand value(this, node->child1());
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();

    JumpList jumpToEnd;

    jumpToEnd.append(branchIfEmpty(valueRegs));

    TypeLocation* cachedTypeLocation = node->typeLocation();
    // Compile in a predictive type check, if possible, to see if we can skip writing to the log.
    // These typechecks are inlined to match those of the 64-bit JSValue type checks.
    if (cachedTypeLocation->m_lastSeenType == TypeUndefined)
        jumpToEnd.append(branchIfUndefined(valueRegs));
    else if (cachedTypeLocation->m_lastSeenType == TypeNull)
        jumpToEnd.append(branchIfNull(valueRegs));
    else if (cachedTypeLocation->m_lastSeenType == TypeBoolean)
        jumpToEnd.append(branchIfBoolean(valueRegs, scratch1GPR));
    else if (cachedTypeLocation->m_lastSeenType == TypeAnyInt)
        jumpToEnd.append(branchIfInt32(valueRegs));
    else if (cachedTypeLocation->m_lastSeenType == TypeNumber)
        jumpToEnd.append(branchIfNumber(valueRegs, scratch1GPR));
    else if (cachedTypeLocation->m_lastSeenType == TypeString) {
        Jump isNotCell = branchIfNotCell(valueRegs);
        jumpToEnd.append(branchIfString(valueRegs.payloadGPR()));
        isNotCell.link(this);
    }

    // Load the TypeProfilerLog into Scratch2.
    TypeProfilerLog* cachedTypeProfilerLog = vm().typeProfilerLog();
    move(TrustedImmPtr(cachedTypeProfilerLog), scratch2GPR);

    // Load the next LogEntry into Scratch1.
    loadPtr(Address(scratch2GPR, TypeProfilerLog::currentLogEntryOffset()), scratch1GPR);

    // Store the JSValue onto the log entry.
    storeValue(valueRegs, Address(scratch1GPR, TypeProfilerLog::LogEntry::valueOffset()));

    // Store the structureID of the cell if valueRegs is a cell, otherwise, store 0 on the log entry.
    Jump isNotCell = branchIfNotCell(valueRegs);
    load32(Address(valueRegs.payloadGPR(), JSCell::structureIDOffset()), scratch3GPR);
    store32(scratch3GPR, Address(scratch1GPR, TypeProfilerLog::LogEntry::structureIDOffset()));
    Jump skipIsCell = jump();
    isNotCell.link(this);
    store32(TrustedImm32(0), Address(scratch1GPR, TypeProfilerLog::LogEntry::structureIDOffset()));
    skipIsCell.link(this);

    // Store the typeLocation on the log entry.
    move(TrustedImmPtr(cachedTypeLocation), scratch3GPR);
    storePtr(scratch3GPR, Address(scratch1GPR, TypeProfilerLog::LogEntry::locationOffset()));

    // Increment the current log entry.
    addPtr(TrustedImm32(sizeof(TypeProfilerLog::LogEntry)), scratch1GPR);
    storePtr(scratch1GPR, Address(scratch2GPR, TypeProfilerLog::currentLogEntryOffset()));
    Jump clearLog = branchPtr(Equal, scratch1GPR, TrustedImmPtr(cachedTypeProfilerLog->logEndPtr()));
    addSlowPathGenerator(
        slowPathCall(clearLog, this, operationProcessTypeProfilerLogDFG, NoResult, TrustedImmPtr(&vm())));

    jumpToEnd.link(this);

    noResult(node);
}

void SpeculativeJIT::genericJSValueNonPeepholeCompare(Node* node, RelationalCondition cond, S_JITOperation_GJJ helperFunction)
{
    ASSERT(node->isBinaryUseKind(UntypedUse) || node->isBinaryUseKind(AnyBigIntUse) || node->isBinaryUseKind(HeapBigIntUse));
    JSValueOperand arg1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand arg2(this, node->child2(), ManualOperandSpeculation);
    speculate(node, node->child1());
    speculate(node, node->child2());

    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();

    JumpList slowPath;

    if (isKnownNotInteger(node->child1().node()) || isKnownNotInteger(node->child2().node())) {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();

        flushRegisters();
        callOperation(helperFunction, resultGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs);

        unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
        return;
    }

    GPRTemporary result(this, Reuse, arg1, TagWord);
    GPRReg resultGPR = result.gpr();

    arg1.use();
    arg2.use();

    if (!isKnownInteger(node->child1().node()))
        slowPath.append(branchIfNotInt32(arg1Regs));
    if (!isKnownInteger(node->child2().node()))
        slowPath.append(branchIfNotInt32(arg2Regs));

    compare32(cond, arg1Regs.payloadGPR(), arg2Regs.payloadGPR(), resultGPR);

    if (!isKnownInteger(node->child1().node()) || !isKnownInteger(node->child2().node()))
        addSlowPathGenerator(slowPathCall(slowPath, this, helperFunction, resultGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs));

    unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::genericJSValuePeepholeBranch(Node* node, Node* branchNode, RelationalCondition cond, S_JITOperation_GJJ helperFunction)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    ResultCondition callResultCondition = NonZero;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        cond = invert(cond);
        callResultCondition = Zero;
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    JSValueOperand arg1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand arg2(this, node->child2(), ManualOperandSpeculation);
    speculate(node, node->child1());
    speculate(node, node->child2());

    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();

    JumpList slowPath;

    if (isKnownNotInteger(node->child1().node()) || isKnownNotInteger(node->child2().node())) {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();

        flushRegisters();
        callOperation(helperFunction, resultGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs);

        branchTest32(callResultCondition, resultGPR, taken);
    } else {
        GPRTemporary result(this, Reuse, arg2, TagWord);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();

        if (!isKnownInteger(node->child1().node()))
            slowPath.append(branchIfNotInt32(arg1Regs));
        if (!isKnownInteger(node->child2().node()))
            slowPath.append(branchIfNotInt32(arg2Regs));

        branch32(cond, arg1Regs.payloadGPR(), arg2Regs.payloadGPR(), taken);

        if (!isKnownInteger(node->child1().node()) || !isKnownInteger(node->child2().node())) {
            jump(notTaken, ForceJump);

            slowPath.link(this);

            callOperationWithSilentSpill(helperFunction, resultGPR, LinkableConstant::globalObject(*this, node), arg1Regs, arg2Regs);

            branchTest32(callResultCondition, resultGPR, taken);
        }
    }

    jump(notTaken);

    m_indexInBlock = m_block->size() - 1;
    m_currentNode = branchNode;
}

void SpeculativeJIT::compileHeapBigIntEquality(Node* node)
{
    // FIXME: [ESNext][BigInt] Create specialized version of strict equals for big ints
    // https://bugs.webkit.org/show_bug.cgi?id=182895
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRTemporary result(this, Reuse, left);
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg resultGPR = result.gpr();

    speculateHeapBigInt(node->child1(), leftGPR);
    speculateHeapBigInt(node->child2(), rightGPR);

    left.use();
    right.use();

    Jump notEqualCase = branchPtr(NotEqual, leftGPR, rightGPR);

    move(TrustedImm32(1), resultGPR);

    Jump done = jump();

    notEqualCase.link(this);

    silentSpillAllRegisters(resultGPR);
    callOperationWithoutExceptionCheck(operationCompareStrictEqCell, resultGPR, LinkableConstant::globalObject(*this, node), leftGPR, rightGPR);
    silentFillAllRegisters();

    done.link(this);

    unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileMakeRope(Node* node)
{
    ASSERT(node->child1().useKind() == KnownStringUse);
    ASSERT(node->child2().useKind() == KnownStringUse);
    ASSERT(!node->child3() || node->child3().useKind() == KnownStringUse);

    SpeculateCellOperand op1(this, node->child1());
    SpeculateCellOperand op2(this, node->child2());
    SpeculateCellOperand op3(this, node->child3());
    GPRReg opGPRs[3];
    unsigned numOpGPRs;
    opGPRs[0] = op1.gpr();
    opGPRs[1] = op2.gpr();
    if (node->child3()) {
        opGPRs[2] = op3.gpr();
        numOpGPRs = 3;
    } else {
        opGPRs[2] = InvalidGPRReg;
        numOpGPRs = 2;
    }

#if CPU(ADDRESS64)
    Edge edges[3] = {
        node->child1(),
        node->child2(),
        node->child3()
    };

    GPRTemporary result(this);
    GPRTemporary allocator(this);
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);
    GPRReg resultGPR = result.gpr();
    GPRReg allocatorGPR = allocator.gpr();
    GPRReg scratchGPR = scratch.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

    JumpList slowPath;
    Allocator allocatorValue = allocatorForConcurrently<JSRopeString>(vm(), sizeof(JSRopeString), AllocatorForMode::AllocatorIfExists);
    emitAllocateJSCell(resultGPR, JITAllocator::constant(allocatorValue), allocatorGPR, TrustedImmPtr(m_graph.registerStructure(vm().stringStructure.get())), scratchGPR, slowPath, SlowAllocationResult::UndefinedBehavior);

    // This puts nullptr for the first fiber. It makes visitChildren safe even if this JSRopeString is discarded due to the speculation failure in the following path.
    storePtr(TrustedImmPtr(JSString::isRopeInPointer), Address(resultGPR, JSRopeString::offsetOfFiber0()));

    {
        if (JSString* string = edges[0]->dynamicCastConstant<JSString*>()) {
            move(TrustedImm32(string->is8Bit() ? StringImpl::flagIs8Bit() : 0), scratchGPR);
            move(TrustedImm32(string->length()), allocatorGPR);
        } else {
            bool needsRopeCase = canBeRope(edges[0]);
            loadPtr(Address(opGPRs[0], JSString::offsetOfValue()), scratch2GPR);
            Jump isRope;
            if (needsRopeCase)
                isRope = branchIfRopeStringImpl(scratch2GPR);

            load32(Address(scratch2GPR, StringImpl::flagsOffset()), scratchGPR);
            load32(Address(scratch2GPR, StringImpl::lengthMemoryOffset()), allocatorGPR);

            if (needsRopeCase) {
                auto done = jump();

                isRope.link(this);
                load32(Address(opGPRs[0], JSRopeString::offsetOfFlags()), scratchGPR);
                load32(Address(opGPRs[0], JSRopeString::offsetOfLength()), allocatorGPR);
                done.link(this);
            }
        }

        if (ASSERT_ENABLED) {
            Jump ok = branch32(
                GreaterThanOrEqual, allocatorGPR, TrustedImm32(0));
            abortWithReason(DFGNegativeStringLength);
            ok.link(this);
        }
    }

    for (unsigned i = 1; i < numOpGPRs; ++i) {
        if (JSString* string = edges[i]->dynamicCastConstant<JSString*>()) {
            and32(TrustedImm32(string->is8Bit() ? StringImpl::flagIs8Bit() : 0), scratchGPR);
            speculationCheck(
                Uncountable, JSValueSource(), nullptr,
                branchAdd32(
                    Overflow,
                    TrustedImm32(string->length()), allocatorGPR));
        } else {
            bool needsRopeCase = canBeRope(edges[i]);
            loadPtr(Address(opGPRs[i], JSString::offsetOfValue()), scratch2GPR);
            Jump isRope;
            if (needsRopeCase)
                isRope = branchIfRopeStringImpl(scratch2GPR);

            and32(Address(scratch2GPR, StringImpl::flagsOffset()), scratchGPR);
            speculationCheck(
                Uncountable, JSValueSource(), nullptr,
                branchAdd32(
                    Overflow,
                    Address(scratch2GPR, StringImpl::lengthMemoryOffset()), allocatorGPR));
            if (needsRopeCase) {
                auto done = jump();

                isRope.link(this);
                and32(Address(opGPRs[i], JSRopeString::offsetOfFlags()), scratchGPR);
                load32(Address(opGPRs[i], JSRopeString::offsetOfLength()), scratch2GPR);
                speculationCheck(
                    Uncountable, JSValueSource(), nullptr,
                    branchAdd32(
                        Overflow, scratch2GPR, allocatorGPR));
                done.link(this);
            }
        }
    }

    if (ASSERT_ENABLED) {
        Jump ok = branch32(
            GreaterThanOrEqual, allocatorGPR, TrustedImm32(0));
        abortWithReason(DFGNegativeStringLength);
        ok.link(this);
    }

    static_assert(StringImpl::flagIs8Bit() == JSRopeString::is8BitInPointer);
    and32(TrustedImm32(StringImpl::flagIs8Bit()), scratchGPR);
    orPtr(opGPRs[0], scratchGPR);
    orPtr(TrustedImmPtr(JSString::isRopeInPointer), scratchGPR);
    storePtr(scratchGPR, Address(resultGPR, JSRopeString::offsetOfFiber0()));

#if CPU(ARM64)
    orLeftShift64(allocatorGPR, opGPRs[1], TrustedImm32(32), scratchGPR);
#else
    lshiftPtr(opGPRs[1], TrustedImm32(32), scratchGPR);
    orPtr(allocatorGPR, scratchGPR);
#endif

    if (numOpGPRs == 2) {
        rshiftPtr(opGPRs[1], TrustedImm32(32), scratch2GPR);
        storePairPtr(scratchGPR, scratch2GPR, Address(resultGPR, JSRopeString::offsetOfFiber1()));
    } else {
#if CPU(ARM64)
        rshiftPtr(opGPRs[1], TrustedImm32(32), scratch2GPR);
        orLeftShift64(scratch2GPR, opGPRs[2], TrustedImm32(16), scratch2GPR);
        storePairPtr(scratchGPR, scratch2GPR, Address(resultGPR, JSRopeString::offsetOfFiber1()));
#else
        storePtr(scratchGPR, Address(resultGPR, JSRopeString::offsetOfFiber1()));
        rshiftPtr(opGPRs[1], TrustedImm32(32), scratchGPR);
        lshiftPtr(opGPRs[2], TrustedImm32(16), scratch2GPR);
        orPtr(scratch2GPR, scratchGPR);
        storePtr(scratchGPR, Address(resultGPR, JSRopeString::offsetOfFiber2()));
#endif
    }

    auto isNonEmptyString = branchTest32(NonZero, allocatorGPR);

    loadLinkableConstant(LinkableConstant(*this, jsEmptyString(vm())), resultGPR);

    isNonEmptyString.link(this);
    mutatorFence(vm());

    switch (numOpGPRs) {
    case 2:
        addSlowPathGenerator(slowPathCall(
            slowPath, this, operationMakeRope2, resultGPR, LinkableConstant::globalObject(*this, node), opGPRs[0], opGPRs[1]));
        break;
    case 3:
        addSlowPathGenerator(slowPathCall(
            slowPath, this, operationMakeRope3, resultGPR, LinkableConstant::globalObject(*this, node), opGPRs[0], opGPRs[1], opGPRs[2]));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    cellResult(resultGPR, node);
#else
    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    switch (numOpGPRs) {
    case 2:
        callOperation(operationMakeRope2, resultGPR, LinkableConstant::globalObject(*this, node), opGPRs[0], opGPRs[1]);
        break;
    case 3:
        callOperation(operationMakeRope3, resultGPR, LinkableConstant::globalObject(*this, node), opGPRs[0], opGPRs[1], opGPRs[2]);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    cellResult(resultGPR, node);
#endif
}

void SpeculativeJIT::compileMakeAtomString(Node* node)
{
    SpeculateCellOperand op1(this, node->child1());
    SpeculateCellOperand op2(this, node->child2());
    SpeculateCellOperand op3(this, node->child3());
    GPRReg opGPRs[3] { InvalidGPRReg, InvalidGPRReg, InvalidGPRReg };
    unsigned numOpGPRs;
    opGPRs[0] = op1.gpr();
    if (node->child2()) {
        opGPRs[1] = op2.gpr();
        if (node->child3()) {
            opGPRs[2] = op3.gpr();
            numOpGPRs = 3;
        } else
            numOpGPRs = 2;
    } else
        numOpGPRs = 1;

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    switch (numOpGPRs) {
    case 1:
        callOperation(operationMakeAtomString1, resultGPR, LinkableConstant::globalObject(*this, node), opGPRs[0]);
        break;
    case 2:
        callOperation(operationMakeAtomString2, resultGPR, LinkableConstant::globalObject(*this, node), opGPRs[0], opGPRs[1]);
        break;
    case 3:
        callOperation(operationMakeAtomString3, resultGPR, LinkableConstant::globalObject(*this, node), opGPRs[0], opGPRs[1], opGPRs[2]);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileEnumeratorGetByVal(Node* node)
{
    Edge baseEdge = m_graph.varArgChild(node, 0);
    auto generate = [&] (JSValueRegs baseRegs) {
        JumpList doneCases;
        JSValueRegsTemporary result;
        std::optional<JSValueRegsFlushedCallResult> flushedResult;
        JSValueRegs resultRegs;
        GPRReg indexGPR;
        GPRReg enumeratorGPR;
        JumpList recoverGenericCase;

        compileGetByVal(node, scopedLambda<std::tuple<JSValueRegs, DataFormat>(DataFormat, bool)>([&](DataFormat preferredFormat, bool needsFlush) {
            Edge storageEdge = m_graph.varArgChild(node, 2);
            StorageOperand storage;
            if (storageEdge)
                storage.emplace(this, storageEdge);
            SpeculateStrictInt32Operand index(this, m_graph.varArgChild(node, 3));
            SpeculateStrictInt32Operand mode(this, m_graph.varArgChild(node, 4));
            SpeculateCellOperand enumerator(this, m_graph.varArgChild(node, 5));
            GPRTemporary scratch(this);

            GPRReg modeGPR = mode.gpr();
            GPRReg scratchGPR = scratch.gpr();
            indexGPR = index.gpr();
            enumeratorGPR = enumerator.gpr();

            GPRTemporary storageTemporary;
            GPRReg storageGPR;
            if (storageEdge)
                storageGPR = storage.gpr();
            else {
                storageTemporary = GPRTemporary(this);
                storageGPR = storageTemporary.gpr();
            }

            if (!needsFlush) {
                result = JSValueRegsTemporary(this);
                resultRegs = result.regs();
            } else {
                ASSERT_UNUSED(preferredFormat, preferredFormat == DataFormatJS);
                flushRegisters();
                flushedResult.emplace(this);
                resultRegs = flushedResult->regs();
            }

            JumpList notFastNamedCases;
            // FIXME: Maybe we should have a better way to represent IndexedMode+OwnStructureMode?
            bool indexedAndOwnStructureMode = m_graph.varArgChild(node, 1).node() == m_graph.varArgChild(node, 3).node();
            JumpList& genericOrRecoverCase = indexedAndOwnStructureMode ? recoverGenericCase : notFastNamedCases;

            // FIXME: We shouldn't generate this code if we know base is not an object.
            notFastNamedCases.append(branchTest32(NonZero, modeGPR, TrustedImm32(JSPropertyNameEnumerator::IndexedMode | JSPropertyNameEnumerator::GenericMode)));
            {
                if (!m_state.forNode(baseEdge).isType(SpecCell))
                    genericOrRecoverCase.append(branchIfNotCell(baseRegs));

                // Check the structure
                // FIXME: If we know there's only one structure for base we can just embed it here.
                load32(Address(baseRegs.payloadGPR(), JSCell::structureIDOffset()), scratchGPR);

                auto badStructure = branch32(
                    NotEqual,
                    scratchGPR,
                    Address(
                        enumeratorGPR, JSPropertyNameEnumerator::cachedStructureIDOffset()));
                genericOrRecoverCase.append(badStructure);

                // Compute the offset
                // If index is less than the enumerator's cached inline storage, then it's an inline access
                Jump outOfLineAccess = branch32(AboveOrEqual,
                    indexGPR, Address(enumeratorGPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()));

                loadValue(BaseIndex(baseRegs.payloadGPR(), indexGPR, TimesEight, JSObject::offsetOfInlineStorage()), resultRegs);

                doneCases.append(jump());

                // Otherwise it's out of line
                outOfLineAccess.link(this);
                move(indexGPR, scratchGPR);
                sub32(Address(enumeratorGPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()), scratchGPR);
                neg32(scratchGPR);
                signExtend32ToPtr(scratchGPR, scratchGPR);
                if (!storageEdge)
                    loadPtr(Address(baseRegs.payloadGPR(), JSObject::butterflyOffset()), storageGPR);
                constexpr intptr_t offsetOfFirstProperty = offsetInButterfly(firstOutOfLineOffset) * static_cast<intptr_t>(sizeof(EncodedJSValue));
                loadValue(BaseIndex(storageGPR, scratchGPR, TimesEight, offsetOfFirstProperty), resultRegs);
                doneCases.append(jump());
            }

            notFastNamedCases.link(this);
            return std::tuple { resultRegs, DataFormatJS };
        }));

        // We rely on compileGetByVal to call jsValueResult for us.
        // FIXME: This is kinda hacky...
        ASSERT(generationInfo(node).jsValueRegs() == resultRegs && generationInfo(node).registerFormat() == DataFormatJS);

        if (!recoverGenericCase.empty()) {
            if (baseRegs.tagGPR() == InvalidGPRReg)
                addSlowPathGenerator(slowPathCall(recoverGenericCase, this, operationEnumeratorRecoverNameAndGetByVal, resultRegs, LinkableConstant::globalObject(*this, node), CellValue(baseRegs.payloadGPR()), indexGPR, enumeratorGPR));
            else
                addSlowPathGenerator(slowPathCall(recoverGenericCase, this, operationEnumeratorRecoverNameAndGetByVal, resultRegs, LinkableConstant::globalObject(*this, node), baseRegs, indexGPR, enumeratorGPR));
        }

        doneCases.link(this);
    };

    if (isCell(baseEdge.useKind())) {
        // Use manual operand speculation since Fixup may have picked a UseKind more restrictive than CellUse.
        SpeculateCellOperand base(this, baseEdge, ManualOperandSpeculation);
        speculate(node, baseEdge);
        generate(JSValueRegs::payloadOnly(base.gpr()));
    } else {
        JSValueOperand base(this, baseEdge);
        generate(base.regs());
    }
}

void SpeculativeJIT::compileStringLocaleCompare(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    SpeculateCellOperand argument(this, node->child2());

    GPRReg baseGPR = base.gpr();
    GPRReg argumentGPR = argument.gpr();

    speculateString(node->child1(), baseGPR);
    speculateString(node->child2(), argumentGPR);

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationStringLocaleCompare, resultGPR, LinkableConstant::globalObject(*this, node), baseGPR, argumentGPR);

    strictInt32Result(resultGPR, node);
}

void SpeculativeJIT::compileStringIndexOf(Node* node)
{
    std::optional<UChar> character;
    String searchString = node->child2()->tryGetString(m_graph);
    if (!!searchString) {
        if (searchString.length() == 1)
            character = searchString.characterAt(0);
    }

    if (node->child3()) {
        SpeculateCellOperand base(this, node->child1());
        SpeculateCellOperand argument(this, node->child2());
        SpeculateInt32Operand index(this, node->child3());

        GPRReg baseGPR = base.gpr();
        GPRReg argumentGPR = argument.gpr();
        GPRReg indexGPR = index.gpr();

        speculateString(node->child1(), baseGPR);
        speculateString(node->child2(), argumentGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        if (character)
            callOperation(operationStringIndexOfWithIndexWithOneChar, resultGPR, LinkableConstant::globalObject(*this, node), baseGPR, indexGPR, TrustedImm32(character.value()));
        else
            callOperation(operationStringIndexOfWithIndex, resultGPR, LinkableConstant::globalObject(*this, node), baseGPR, argumentGPR, indexGPR);

        strictInt32Result(resultGPR, node);
        return;
    }

    SpeculateCellOperand base(this, node->child1());
    SpeculateCellOperand argument(this, node->child2());

    GPRReg baseGPR = base.gpr();
    GPRReg argumentGPR = argument.gpr();

    speculateString(node->child1(), baseGPR);
    speculateString(node->child2(), argumentGPR);

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    if (character)
        callOperation(operationStringIndexOfWithOneChar, resultGPR, LinkableConstant::globalObject(*this, node), baseGPR, TrustedImm32(character.value()));
    else
        callOperation(operationStringIndexOf, resultGPR, LinkableConstant::globalObject(*this, node), baseGPR, argumentGPR);

    strictInt32Result(resultGPR, node);
}

void SpeculativeJIT::compileGlobalIsNaN(Node* node)
{
    switch (node->child1().useKind()) {
    case DoubleRepUse: {
        SpeculateDoubleOperand argument(this, node->child1());
        GPRTemporary scratch(this);

        FPRReg argumentFPR = argument.fpr();
        GPRReg scratchGPR = scratch.gpr();

        compareDouble(DoubleNotEqualOrUnordered, argumentFPR, argumentFPR, scratchGPR);
        unblessedBooleanResult(scratchGPR, node);
        break;
    }
    case UntypedUse: {
        JSValueOperand argument(this, node->child1());
        GPRTemporary scratch1(this);

        bool mayBeInt32 = m_interpreter.forNode(node->child1()).m_type & SpecInt32Only;

        JSValueRegs argumentRegs = argument.jsValueRegs();
        GPRReg scratch1GPR = scratch1.gpr();

        flushRegisters();
        Jump isInt32;
        if (mayBeInt32) {
            move(TrustedImm32(0), scratch1GPR);
            isInt32 = branchIfInt32(argumentRegs);
        }
        callOperation(operationIsNaN, scratch1GPR, LinkableConstant::globalObject(*this, node), argumentRegs);
        if (mayBeInt32)
            isInt32.link(this);
        unblessedBooleanResult(scratch1GPR, node);
        break;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::compileNumberIsNaN(Node* node)
{
    switch (node->child1().useKind()) {
    case DoubleRepUse: {
        SpeculateDoubleOperand argument(this, node->child1());
        GPRTemporary scratch(this);

        FPRReg argumentFPR = argument.fpr();
        GPRReg scratchGPR = scratch.gpr();

        compareDouble(DoubleNotEqualOrUnordered, argumentFPR, argumentFPR, scratchGPR);
        unblessedBooleanResult(scratchGPR, node);
        break;
    }
    case UntypedUse: {
        JSValueOperand argument(this, node->child1());
        GPRTemporary scratch1(this);

        bool mayBeInt32 = m_interpreter.forNode(node->child1()).m_type & SpecInt32Only;

        JSValueRegs argumentRegs = argument.jsValueRegs();
        GPRReg scratch1GPR = scratch1.gpr();

        flushRegisters();
        Jump isInt32;
        if (mayBeInt32) {
            move(TrustedImm32(0), scratch1GPR);
            isInt32 = branchIfInt32(argumentRegs);
        }
        callOperation(operationNumberIsNaN, scratch1GPR, argumentRegs);
        if (mayBeInt32)
            isInt32.link(this);
        unblessedBooleanResult(scratch1GPR, node);
        break;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::compileToIntegerOrInfinity(Node* node)
{
    switch (node->child1().useKind()) {
    case DoubleRepUse: {
        SpeculateDoubleOperand argument(this, node->child1());

        FPRReg argumentFPR = argument.fpr();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperationWithoutExceptionCheck(operationToIntegerOrInfinityDouble, resultRegs, argumentFPR);
        jsValueResult(resultRegs, node);
        break;
    }
    case UntypedUse: {
        JSValueOperand argument(this, node->child1());
        JSValueRegsTemporary result(this);

        bool mayBeInt32 = m_interpreter.forNode(node->child1()).m_type & SpecInt32Only;

        JSValueRegs argumentRegs = argument.jsValueRegs();
        JSValueRegs resultRegs = result.regs();

        flushRegisters();
        Jump isInt32;
        if (mayBeInt32) {
            moveValueRegs(argumentRegs, resultRegs);
            isInt32 = branchIfInt32(argumentRegs);
        }
        callOperation(operationToIntegerOrInfinityUntyped, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs);
        if (mayBeInt32)
            isInt32.link(this);
        jsValueResult(resultRegs, node);
        break;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::compileToLength(Node* node)
{
    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateInt32Operand argument(this, node->child1());
        GPRTemporary scratch1(this);

        GPRReg argumentGPR = argument.gpr();
        GPRReg scratch1GPR = scratch1.gpr();

        move(TrustedImm32(0), scratch1GPR);
        moveConditionally32(CCallHelpers::LessThan, argumentGPR, TrustedImm32(0), scratch1GPR, argumentGPR, scratch1GPR);
        zeroExtend32ToWord(scratch1GPR, scratch1GPR);
        strictInt32Result(scratch1GPR, node);
        break;
    }
    case DoubleRepUse: {
        SpeculateDoubleOperand argument(this, node->child1());

        FPRReg argumentFPR = argument.fpr();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperationWithoutExceptionCheck(operationToLengthDouble, resultRegs, argumentFPR);
        jsValueResult(resultRegs, node);
        break;
    }
    case UntypedUse: {
        JSValueOperand argument(this, node->child1());
        JSValueRegsTemporary result(this);

        bool mayBeInt32 = m_interpreter.forNode(node->child1()).m_type & SpecInt32Only;

        JSValueRegs argumentRegs = argument.jsValueRegs();
        JSValueRegs resultRegs = result.regs();

        flushRegisters();
        Jump isNotInt32;
        Jump done;
        if (mayBeInt32) {
            isNotInt32 = branchIfNotInt32(argumentRegs);
            move(TrustedImm32(0), resultRegs.payloadGPR());
            moveConditionally32(CCallHelpers::LessThan, argumentRegs.payloadGPR(), TrustedImm32(0), resultRegs.payloadGPR(), argumentRegs.payloadGPR(), resultRegs.payloadGPR());
            zeroExtend32ToWord(resultRegs.payloadGPR(), resultRegs.payloadGPR());
            boxInt32(resultRegs.payloadGPR(), resultRegs);
            done = jump();
        }

        if (mayBeInt32)
            isNotInt32.link(this);
        callOperation(operationToLengthUntyped, resultRegs, LinkableConstant::globalObject(*this, node), argumentRegs);

        if (mayBeInt32)
            done.link(this);
        jsValueResult(resultRegs, node);
        break;
    }
    default:
        DFG_CRASH(m_graph, node, "Bad use kind");
        break;
    }
}

} } // namespace JSC::DFG

#endif
