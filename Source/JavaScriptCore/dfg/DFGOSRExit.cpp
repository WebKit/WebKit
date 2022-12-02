/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
#include "DFGOSRExit.h"

#if ENABLE(DFG_JIT)

#include "AssemblyHelpersSpoolers.h"
#include "BytecodeStructs.h"
#include "CheckpointOSRExitSideState.h"
#include "DFGGraph.h"
#include "DFGMayExit.h"
#include "DFGOSRExitCompilerCommon.h"
#include "DFGOperations.h"
#include "DFGSpeculativeJIT.h"
#include "FrameTracers.h"
#include "InlineCallFrame.h"
#include "JSCJSValueInlines.h"
#include "OperandsInlines.h"
#include "ProbeContext.h"
#include "VMInlines.h"

#include <wtf/Scope.h>

namespace JSC { namespace DFG {

OSRExit::OSRExit(ExitKind kind, JSValueSource jsValueSource, MethodOfGettingAValueProfile valueProfile, SpeculativeJIT* jit, unsigned streamIndex, unsigned recoveryIndex)
    : OSRExitBase(kind, jit->m_origin.forExit, jit->m_origin.semantic, jit->m_origin.wasHoisted, jit->m_currentNode ? jit->m_currentNode->index() : 0)
    , m_jsValueSource(jsValueSource)
    , m_valueProfile(valueProfile)
    , m_recoveryIndex(recoveryIndex)
    , m_streamIndex(streamIndex)
{
    bool canExit = jit->m_origin.exitOK;
    if (!canExit && jit->m_currentNode) {
        ExitMode exitMode = mayExit(jit->m_graph, jit->m_currentNode);
        canExit = exitMode == ExitMode::Exits || exitMode == ExitMode::ExitsForExceptions;
    }
    DFG_ASSERT(jit->m_graph, jit->m_currentNode, canExit);
}

void OSRExit::emitRestoreArguments(CCallHelpers& jit, VM& vm, const Operands<ValueRecovery>& operands)
{
    HashMap<MinifiedID, VirtualRegister> alreadyAllocatedArguments; // Maps phantom arguments node ID to operand.
    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];

        if (recovery.technique() != DirectArgumentsThatWereNotCreated
            && recovery.technique() != ClonedArgumentsThatWereNotCreated)
            continue;

        Operand operand = operands.operandForIndex(index);
        if (operand.isTmp())
            continue;

        MinifiedID id = recovery.nodeID();
        auto iter = alreadyAllocatedArguments.find(id);
        if (iter != alreadyAllocatedArguments.end()) {
            JSValueRegs regs = JSValueRegs::withTwoAvailableRegs(GPRInfo::regT0, GPRInfo::regT1);
            jit.loadValue(CCallHelpers::addressFor(iter->value), regs);
            jit.storeValue(regs, CCallHelpers::addressFor(operand));
            continue;
        }

        InlineCallFrame* inlineCallFrame =
            jit.codeBlock()->jitCode()->dfg()->minifiedDFG.at(id)->inlineCallFrame();

        int stackOffset;
        if (inlineCallFrame)
            stackOffset = inlineCallFrame->stackOffset;
        else
            stackOffset = 0;

        if (!inlineCallFrame || inlineCallFrame->isClosureCall) {
            jit.loadPtr(
                AssemblyHelpers::addressFor(VirtualRegister(stackOffset + CallFrameSlot::callee)),
                GPRInfo::regT0);
        } else {
            jit.move(
                AssemblyHelpers::TrustedImmPtr(inlineCallFrame->calleeRecovery.constant().asCell()),
                GPRInfo::regT0);
        }

        if (!inlineCallFrame || inlineCallFrame->isVarargs()) {
            jit.load32(
                AssemblyHelpers::payloadFor(VirtualRegister(stackOffset + CallFrameSlot::argumentCountIncludingThis)),
                GPRInfo::regT1);
        } else {
            jit.move(
                AssemblyHelpers::TrustedImm32(inlineCallFrame->argumentCountIncludingThis),
                GPRInfo::regT1);
        }

        static_assert(std::is_same<decltype(operationCreateDirectArgumentsDuringExit), decltype(operationCreateClonedArgumentsDuringExit)>::value, "We assume these functions have the same signature below.");
        jit.setupArguments<decltype(operationCreateDirectArgumentsDuringExit)>(
            AssemblyHelpers::TrustedImmPtr(&vm), AssemblyHelpers::TrustedImmPtr(inlineCallFrame), GPRInfo::regT0, GPRInfo::regT1);
        jit.prepareCallOperation(vm);
        switch (recovery.technique()) {
        case DirectArgumentsThatWereNotCreated:
            jit.move(AssemblyHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationCreateDirectArgumentsDuringExit)), GPRInfo::nonArgGPR0);
            break;
        case ClonedArgumentsThatWereNotCreated:
            jit.move(AssemblyHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationCreateClonedArgumentsDuringExit)), GPRInfo::nonArgGPR0);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
        jit.storeCell(GPRInfo::returnValueGPR, AssemblyHelpers::addressFor(operand));

        alreadyAllocatedArguments.add(id, operand.virtualRegister());
    }
}

JSC_DEFINE_JIT_OPERATION(operationCompileOSRExit, void, (CallFrame* callFrame, void* bufferToPreserve))
{
    VM& vm = callFrame->deprecatedVM();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ActiveScratchBufferScope activeScratchBufferScope(ScratchBuffer::fromData(bufferToPreserve), GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters);

    if constexpr (validateDFGDoesGC) {
        // We're about to exit optimized code. So, there's no longer any optimized
        // code running that expects no GC.
        vm.setDoesGCExpectation(true, DoesGCCheck::Special::DFGOSRExit);
    }

    if (vm.callFrameForCatch)
        RELEASE_ASSERT(vm.callFrameForCatch == callFrame);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock);
    ASSERT(codeBlock->jitType() == JITType::DFGJIT);

    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(vm);

    uint32_t exitIndex = vm.osrExitIndex;
    OSRExit& exit = codeBlock->jitCode()->dfg()->m_osrExit[exitIndex];

    ASSERT(!vm.callFrameForCatch || exit.m_kind == GenericUnwind);
    EXCEPTION_ASSERT_UNUSED(scope, !!scope.exception() || !exit.isExceptionHandler());
    
    // Compute the value recoveries.
    Operands<ValueRecovery> operands;
    codeBlock->jitCode()->dfg()->variableEventStream.reconstruct(codeBlock, exit.m_codeOrigin, codeBlock->jitCode()->dfg()->minifiedDFG, exit.m_streamIndex, operands);

    SpeculationRecovery* recovery = nullptr;
    if (exit.m_recoveryIndex != UINT_MAX)
        recovery = &codeBlock->jitCode()->dfg()->m_speculationRecovery[exit.m_recoveryIndex];

    MacroAssemblerCodeRef<OSRExitPtrTag> exitCode;
    {
        CCallHelpers jit(codeBlock);

        if (exit.m_kind == GenericUnwind) {
            // We are acting as a defacto op_catch because we arrive here from genericUnwind().
            // So, we must restore our call frame and stack pointer.
            jit.restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(vm.topEntryFrame);
            jit.loadPtr(vm.addressOfCallFrameForCatch(), GPRInfo::callFrameRegister);
        }
        jit.addPtr(
            CCallHelpers::TrustedImm32(codeBlock->stackPointerOffset() * sizeof(Register)),
            GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

        jit.jitAssertHasValidCallFrame();

        if (UNLIKELY(vm.m_perBytecodeProfiler && codeBlock->jitCode()->dfgCommon()->compilation)) {
            Profiler::Database& database = *vm.m_perBytecodeProfiler;
            Profiler::Compilation* compilation = codeBlock->jitCode()->dfgCommon()->compilation.get();

            Profiler::OSRExit* profilerExit = compilation->addOSRExit(
                exitIndex, Profiler::OriginStack(database, codeBlock, exit.m_codeOrigin),
                exit.m_kind, exit.m_kind == UncountableInvalidation);
            jit.add64(CCallHelpers::TrustedImm32(1), CCallHelpers::AbsoluteAddress(profilerExit->counterAddress()));
        }

        OSRExit::compileExit(jit, vm, exit, operands, recovery);

        LinkBuffer patchBuffer(jit, codeBlock, LinkBuffer::Profile::DFGOSRExit);
        exitCode = FINALIZE_CODE_IF(
            shouldDumpDisassembly() || Options::verboseOSR() || Options::verboseDFGOSRExit(),
            patchBuffer, OSRExitPtrTag,
            "DFG OSR exit #%u (D@%u, %s, %s) from %s, with operands = %s",
                exitIndex, exit.m_dfgNodeIndex, toCString(exit.m_codeOrigin).data(),
                exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
                toCString(ignoringContext<DumpContext>(operands)).data());
        codeBlock->dfgJITData()->setExitCode(exitIndex, exitCode);
    }

    if (exit.codeLocationForRepatch())
        MacroAssembler::repatchJump(exit.codeLocationForRepatch(), CodeLocationLabel<OSRExitPtrTag>(exitCode.code()));

    vm.osrExitJumpDestination = exitCode.code().taggedPtr();
}

IGNORE_WARNINGS_BEGIN("frame-address")

JSC_DEFINE_JIT_OPERATION(operationMaterializeOSRExitSideState, void, (VM* vmPointer, const OSRExitBase* exitPointer, EncodedJSValue* tmpScratch))
{
    const OSRExitBase& exit = *exitPointer;
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);

    Vector<std::unique_ptr<CheckpointOSRExitSideState>, VM::expectedMaxActiveSideStateCount> sideStates;
    sideStates.reserveInitialCapacity(exit.m_codeOrigin.inlineDepth());
    auto sideStateCommitter = makeScopeExit([&] {
        for (size_t i = sideStates.size(); i--;)
            vm.pushCheckpointOSRSideState(WTFMove(sideStates[i]));
    });

    auto addSideState = [&] (CallFrame* frame, BytecodeIndex index, size_t tmpOffset) {
        std::unique_ptr<CheckpointOSRExitSideState> sideState = WTF::makeUnique<CheckpointOSRExitSideState>(frame);

        sideState->bytecodeIndex = index;
        for (size_t i = 0; i < maxNumCheckpointTmps; ++i)
            sideState->tmps[i] = JSValue::decode(tmpScratch[i + tmpOffset]);

        sideStates.append(WTFMove(sideState));
    };

    const CodeOrigin* codeOrigin;
    for (codeOrigin = &exit.m_codeOrigin; codeOrigin && codeOrigin->inlineCallFrame(); codeOrigin = codeOrigin->inlineCallFrame()->getCallerSkippingTailCalls()) {
        BytecodeIndex callBytecodeIndex = codeOrigin->bytecodeIndex();
        if (!callBytecodeIndex.checkpoint())
            continue;

        auto* inlineCallFrame = codeOrigin->inlineCallFrame();
        addSideState(reinterpret_cast_ptr<CallFrame*>(reinterpret_cast<char*>(callFrame) + inlineCallFrame->returnPCOffset() - sizeof(CPURegister)), callBytecodeIndex, inlineCallFrame->tmpOffset);
    }

    if (!codeOrigin)
        return;

    if (BytecodeIndex bytecodeIndex = codeOrigin->bytecodeIndex(); bytecodeIndex.checkpoint())
        addSideState(callFrame, bytecodeIndex, 0);
}

IGNORE_WARNINGS_END

void OSRExit::compileExit(CCallHelpers& jit, VM& vm, const OSRExit& exit, const Operands<ValueRecovery>& operands, SpeculationRecovery* recovery)
{
    jit.jitAssertTagsInPlace();

    // Pro-forma stuff.
    if (UNLIKELY(Options::printEachOSRExit())) {
        SpeculationFailureDebugInfo* debugInfo = new SpeculationFailureDebugInfo;
        debugInfo->codeBlock = jit.codeBlock();
        debugInfo->kind = exit.m_kind;
        debugInfo->bytecodeIndex = exit.m_codeOrigin.bytecodeIndex();

        jit.debugCall(vm, operationDebugPrintSpeculationFailure, debugInfo);
    }

    // Perform speculation recovery. This only comes into play when an operation
    // starts mutating state before verifying the speculation it has already made.

    if (recovery) {
        switch (recovery->type()) {
        case SpeculativeAdd:
            jit.sub32(recovery->src(), recovery->dest());
#if USE(JSVALUE64)
            jit.or64(GPRInfo::numberTagRegister, recovery->dest());
#endif
            break;

        case SpeculativeAddSelf:
            // If A + A = A (int32_t) overflows, A can be recovered by ((static_cast<int32_t>(A) >> 1) ^ 0x8000000).
            jit.rshift32(AssemblyHelpers::TrustedImm32(1), recovery->dest());
            jit.xor32(AssemblyHelpers::TrustedImm32(0x80000000), recovery->dest());
#if USE(JSVALUE64)
            jit.or64(GPRInfo::numberTagRegister, recovery->dest());
#endif
            break;

        case SpeculativeAddImmediate:
            jit.sub32(AssemblyHelpers::Imm32(recovery->immediate()), recovery->dest());
#if USE(JSVALUE64)
            jit.or64(GPRInfo::numberTagRegister, recovery->dest());
#endif
            break;

        case BooleanSpeculationCheck:
#if USE(JSVALUE64)
            jit.xor64(AssemblyHelpers::TrustedImm32(JSValue::ValueFalse), recovery->dest());
#endif
            break;

        default:
            break;
        }
    }

    // Refine some array and/or value profile, if appropriate.

    if (!!exit.m_jsValueSource) {
        if (exit.m_kind == BadCache || exit.m_kind == BadIndexingType || exit.m_kind == UnexpectedResizableArrayBufferView) {
            // If the instruction that this originated from has an array profile, then
            // refine it. If it doesn't, then do nothing. The latter could happen for
            // hoisted checks, or checks emitted for operations that didn't have array
            // profiling - either ops that aren't array accesses at all, or weren't
            // known to be array acceses in the bytecode. The latter case is a FIXME
            // while the former case is an outcome of a CheckStructure not knowing why
            // it was emitted (could be either due to an inline cache of a property
            // property access, or due to an array profile).

            CodeOrigin codeOrigin = exit.m_codeOriginForExitProfile;
            CodeBlock* codeBlock = jit.baselineCodeBlockFor(codeOrigin);
            if (ArrayProfile* arrayProfile = codeBlock->getArrayProfile(ConcurrentJSLocker(codeBlock->m_lock), codeOrigin.bytecodeIndex())) {
                const auto* instruction = codeBlock->instructions().at(codeOrigin.bytecodeIndex()).ptr();
                CCallHelpers::Jump skipProfile;
                if (instruction->is<OpGetById>()) {
                    auto& metadata = instruction->as<OpGetById>().metadata(codeBlock);
                    skipProfile = jit.branch8(CCallHelpers::NotEqual, CCallHelpers::AbsoluteAddress(&metadata.m_modeMetadata.mode), CCallHelpers::TrustedImm32(static_cast<uint8_t>(GetByIdMode::ArrayLength)));
                }

#if USE(JSVALUE64)
                GPRReg usedRegister;
                if (exit.m_jsValueSource.isAddress())
                    usedRegister = exit.m_jsValueSource.base();
                else
                    usedRegister = exit.m_jsValueSource.gpr();
#else
                GPRReg usedRegister1;
                GPRReg usedRegister2;
                if (exit.m_jsValueSource.isAddress()) {
                    usedRegister1 = exit.m_jsValueSource.base();
                    usedRegister2 = InvalidGPRReg;
                } else {
                    usedRegister1 = exit.m_jsValueSource.payloadGPR();
                    if (exit.m_jsValueSource.hasKnownTag())
                        usedRegister2 = InvalidGPRReg;
                    else
                        usedRegister2 = exit.m_jsValueSource.tagGPR();
                }
#endif

                GPRReg scratch1;
                GPRReg scratch2;
#if USE(JSVALUE64)
                scratch1 = AssemblyHelpers::selectScratchGPR(usedRegister);
                scratch2 = AssemblyHelpers::selectScratchGPR(usedRegister, scratch1);
#else
                scratch1 = AssemblyHelpers::selectScratchGPR(usedRegister1, usedRegister2);
                scratch2 = AssemblyHelpers::selectScratchGPR(usedRegister1, usedRegister2, scratch1);
#endif

                if (isARM64()) {
                    jit.pushToSave(scratch1);
                    jit.pushToSave(scratch2);
                } else {
                    jit.push(scratch1);
                    jit.push(scratch2);
                }

                GPRReg value;
                if (exit.m_jsValueSource.isAddress()) {
                    value = scratch1;
                    jit.loadPtr(AssemblyHelpers::Address(exit.m_jsValueSource.asAddress()), value);
                } else
                    value = exit.m_jsValueSource.payloadGPR();

                jit.load32(AssemblyHelpers::Address(value, JSCell::structureIDOffset()), scratch1);
                jit.store32(scratch1, arrayProfile->addressOfLastSeenStructureID());

                jit.load8(AssemblyHelpers::Address(value, JSCell::typeInfoTypeOffset()), scratch2);
                jit.sub32(AssemblyHelpers::TrustedImm32(FirstTypedArrayType), scratch2);
                auto notTypedArray = jit.branch32(MacroAssembler::AboveOrEqual, scratch2, AssemblyHelpers::TrustedImm32(NumberOfTypedArrayTypesExcludingDataView));
                jit.move(AssemblyHelpers::TrustedImmPtr(typedArrayModes), scratch1);
                jit.load32(AssemblyHelpers::BaseIndex(scratch1, scratch2, AssemblyHelpers::TimesFour), scratch2);
                auto storeArrayModes = jit.jump();

                notTypedArray.link(&jit);
#if USE(JSVALUE64)
                jit.load8(AssemblyHelpers::Address(value, JSCell::indexingTypeAndMiscOffset()), scratch1);
#else
                jit.load8(AssemblyHelpers::Address(scratch1, Structure::indexingModeIncludingHistoryOffset()), scratch1);
#endif
                jit.and32(AssemblyHelpers::TrustedImm32(IndexingModeMask), scratch1);
                jit.move(AssemblyHelpers::TrustedImm32(1), scratch2);
                jit.lshift32(scratch1, scratch2);
                storeArrayModes.link(&jit);
                jit.or32(scratch2, AssemblyHelpers::AbsoluteAddress(arrayProfile->addressOfArrayModes()));

                if (isARM64()) {
                    jit.popToRestore(scratch2);
                    jit.popToRestore(scratch1);
                } else {
                    jit.pop(scratch2);
                    jit.pop(scratch1);
                }

                if (skipProfile.isSet())
                    skipProfile.link(&jit);
            }
        }

        if (MethodOfGettingAValueProfile profile = exit.m_valueProfile) {
#if USE(JSVALUE64)
            if (exit.m_jsValueSource.isAddress()) {
                // We can't be sure that we have a spare register. So use the numberTagRegister,
                // since we know how to restore it.
                jit.load64(AssemblyHelpers::Address(exit.m_jsValueSource.asAddress()), GPRInfo::numberTagRegister);
                // We also use the notCellMaskRegister as the scratch register, for the same reason.
                // FIXME: find a less gross way of doing this, maybe through delaying these operations until we actually have some spare registers around?
                profile.emitReportValue(jit, jit.codeBlock(), JSValueRegs(GPRInfo::numberTagRegister), GPRInfo::notCellMaskRegister, DoNotHaveTagRegisters);
                jit.emitMaterializeTagCheckRegisters();
            } else {
                profile.emitReportValue(jit, jit.codeBlock(), JSValueRegs(exit.m_jsValueSource.gpr()), GPRInfo::notCellMaskRegister, DoNotHaveTagRegisters);
                jit.move(AssemblyHelpers::TrustedImm64(JSValue::NotCellMask), GPRInfo::notCellMaskRegister);
            }
#else // not USE(JSVALUE64)
            if (exit.m_jsValueSource.isAddress()) {
                // Save a register so we can use it.
                GPRReg scratchPayload = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.base());
                GPRReg scratchTag = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.base(), scratchPayload);
                jit.pushToSave(scratchPayload);
                jit.pushToSave(scratchTag);

                JSValueRegs scratch(scratchTag, scratchPayload);
                
                jit.loadValue(exit.m_jsValueSource.asAddress(), scratch);
                profile.emitReportValue(jit, jit.codeBlock(), scratch, InvalidGPRReg);
                
                jit.popToRestore(scratchTag);
                jit.popToRestore(scratchPayload);
            } else if (exit.m_jsValueSource.hasKnownTag()) {
                GPRReg scratchTag = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.payloadGPR());
                jit.pushToSave(scratchTag);
                jit.move(AssemblyHelpers::TrustedImm32(exit.m_jsValueSource.tag()), scratchTag);
                JSValueRegs value(scratchTag, exit.m_jsValueSource.payloadGPR());
                profile.emitReportValue(jit, jit.codeBlock(), value, InvalidGPRReg);
                jit.popToRestore(scratchTag);
            } else
                profile.emitReportValue(jit, jit.codeBlock(), exit.m_jsValueSource.regs(), InvalidGPRReg);
#endif // USE(JSVALUE64)
        }
    }

    // What follows is an intentionally simple OSR exit implementation that generates
    // fairly poor code but is very easy to hack. In particular, it dumps all state that
    // needs conversion into a scratch buffer so that in step 6, where we actually do the
    // conversions, we know that all temp registers are free to use and the variable is
    // definitely in a well-known spot in the scratch buffer regardless of whether it had
    // originally been in a register or spilled. This allows us to decouple "where was
    // the variable" from "how was it represented". Consider that the
    // Int32DisplacedInJSStack recovery: it tells us that the value is in a
    // particular place and that that place holds an unboxed int32. We have two different
    // places that a value could be (displaced, register) and a bunch of different
    // ways of representing a value. The number of recoveries is two * a bunch. The code
    // below means that we have to have two + a bunch cases rather than two * a bunch.
    // Once we have loaded the value from wherever it was, the reboxing is the same
    // regardless of its location. Likewise, before we do the reboxing, the way we get to
    // the value (i.e. where we load it from) is the same regardless of its type. Because
    // the code below always dumps everything into a scratch buffer first, the two
    // questions become orthogonal, which simplifies adding new types and adding new
    // locations.
    //
    // This raises the question: does using such a suboptimal implementation of OSR exit,
    // where we always emit code to dump all state into a scratch buffer only to then
    // dump it right back into the stack, hurt us in any way? The asnwer is that OSR exits
    // are rare. Our tiering strategy ensures this. This is because if an OSR exit is
    // taken more than ~100 times, we jettison the DFG code block along with all of its
    // exits. It is impossible for an OSR exit - i.e. the code we compile below - to
    // execute frequently enough for the codegen to matter that much. It probably matters
    // enough that we don't want to turn this into some super-slow function call, but so
    // long as we're generating straight-line code, that code can be pretty bad. Also
    // because we tend to exit only along one OSR exit from any DFG code block - that's an
    // empirical result that we're extremely confident about - the code size of this
    // doesn't matter much. Hence any attempt to optimize the codegen here is just purely
    // harmful to the system: it probably won't reduce either net memory usage or net
    // execution time. It will only prevent us from cleanly decoupling "where was the
    // variable" from "how was it represented", which will make it more difficult to add
    // features in the future and it will make it harder to reason about bugs.

    // Save all state from GPRs into the scratch buffer.

    ScratchBuffer* scratchBuffer = vm.scratchBufferForSize(sizeof(EncodedJSValue) * operands.size());
    EncodedJSValue* scratch = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : nullptr;

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];

        switch (recovery.technique()) {
        case UnboxedInt32InGPR:
        case UnboxedCellInGPR:
#if USE(JSVALUE64)
        case InGPR:
        case UnboxedInt52InGPR:
        case UnboxedStrictInt52InGPR:
            jit.store64(recovery.gpr(), scratch + index);
            break;
#else
        case UnboxedBooleanInGPR:
            jit.store32(
                recovery.gpr(),
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload);
            break;
            
        case InPair:
            jit.storeValue(recovery.jsValueRegs(), scratch + index);
            break;
#endif

        default:
            break;
        }
    }

    // And voila, all GPRs are free to reuse.

    // Save all state from FPRs into the scratch buffer.

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];

        switch (recovery.technique()) {
        case UnboxedDoubleInFPR:
        case InFPR:
            jit.move(AssemblyHelpers::TrustedImmPtr(scratch + index), GPRInfo::regT0);
            jit.storeDouble(recovery.fpr(), MacroAssembler::Address(GPRInfo::regT0));
            break;

        default:
            break;
        }
    }

    // Now, all FPRs are also free.

    // Save all state from the stack into the scratch buffer. For simplicity we
    // do this even for state that's already in the right place on the stack.
    // It makes things simpler later.

    bool inlineStackContainsActiveCheckpoint = exit.m_codeOrigin.inlineStackContainsActiveCheckpoint();
    size_t firstTmpToRestoreEarly = operands.size() - operands.numberOfTmps();
    if (!inlineStackContainsActiveCheckpoint)
        firstTmpToRestoreEarly = operands.size(); // Don't eagerly restore.

    // The tag registers are needed to materialize recoveries below.
    jit.emitMaterializeTagCheckRegisters();

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];

        auto currentTechnique = recovery.technique();
        switch (currentTechnique) {
        case DisplacedInJSStack:
#if USE(JSVALUE64)
        case CellDisplacedInJSStack:
        case BooleanDisplacedInJSStack:
#endif
            jit.loadValue(AssemblyHelpers::addressFor(recovery.virtualRegister()), JSRInfo::jsRegT10);
            jit.storeValue(JSRInfo::jsRegT10, scratch + index);
            break;

        case Constant: {
#if USE(JSVALUE64)
            if (index >= firstTmpToRestoreEarly) {
                ASSERT(operands.operandForIndex(index).isTmp());
                jit.move(AssemblyHelpers::TrustedImm64(JSValue::encode(recovery.constant())), GPRInfo::regT0);
                jit.store64(GPRInfo::regT0, scratch + index);
            }
#else // not USE(JSVALUE64)
            UNUSED_VARIABLE(firstTmpToRestoreEarly);
            jit.storeValue(recovery.constant(), scratch + index, JSRInfo::jsRegT10);
#endif
            break;
        }

        case UnboxedInt32InGPR:
#if USE(JSVALUE64)
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.zeroExtend32ToWord(GPRInfo::regT0, GPRInfo::regT0);
            jit.or64(GPRInfo::numberTagRegister, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, scratch + index);
#else
            jit.store32(
                AssemblyHelpers::TrustedImm32(JSValue::Int32Tag),
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag);
#endif
            break;

        case Int32DisplacedInJSStack:
#if USE(JSVALUE64)
            jit.load64(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::regT0);
            jit.zeroExtend32ToWord(GPRInfo::regT0, GPRInfo::regT0);
            jit.or64(GPRInfo::numberTagRegister, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, scratch + index);
#else
            jit.load32(
                AssemblyHelpers::payloadFor(recovery.virtualRegister()),
                JSRInfo::jsRegT10.payloadGPR());
            jit.move(AssemblyHelpers::TrustedImm32(JSValue::Int32Tag), JSRInfo::jsRegT10.tagGPR());
            jit.storeValue(JSRInfo::jsRegT10, scratch + index);
#endif
            break;

#if USE(JSVALUE32_64)
        case UnboxedBooleanInGPR:
            jit.store32(
                AssemblyHelpers::TrustedImm32(JSValue::BooleanTag),
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag);
            break;

        case BooleanDisplacedInJSStack:
            jit.load32(
                AssemblyHelpers::payloadFor(recovery.virtualRegister()),
                JSRInfo::jsRegT10.payloadGPR());
            jit.move(AssemblyHelpers::TrustedImm32(JSValue::BooleanTag), JSRInfo::jsRegT10.tagGPR());
            jit.storeValue(JSRInfo::jsRegT10, scratch + index);
            break;

        case UnboxedCellInGPR:
            jit.storeCell(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag);
            break;

        case CellDisplacedInJSStack:
            jit.load32(
                AssemblyHelpers::payloadFor(recovery.virtualRegister()),
                JSRInfo::jsRegT10.payloadGPR());
            jit.storeCell(JSRInfo::jsRegT10, scratch + index);
            break;
#endif

        case UnboxedDoubleInFPR:
            jit.move(AssemblyHelpers::TrustedImmPtr(scratch + index), GPRInfo::regT1);
            jit.loadDouble(MacroAssembler::Address(GPRInfo::regT1), FPRInfo::fpRegT0);
            jit.purifyNaN(FPRInfo::fpRegT0);
#if USE(JSVALUE64)
            jit.boxDouble(FPRInfo::fpRegT0, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, MacroAssembler::Address(GPRInfo::regT1));
#else
            jit.storeDouble(FPRInfo::fpRegT0, MacroAssembler::Address(GPRInfo::regT1));
#endif
            break;

        case DoubleDisplacedInJSStack:
            jit.move(AssemblyHelpers::TrustedImmPtr(scratch + index), GPRInfo::regT1);
            jit.loadDouble(AssemblyHelpers::addressFor(recovery.virtualRegister()), FPRInfo::fpRegT0);
            jit.purifyNaN(FPRInfo::fpRegT0);
#if USE(JSVALUE64)
            jit.boxDouble(FPRInfo::fpRegT0, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, MacroAssembler::Address(GPRInfo::regT1));
#else
            jit.storeDouble(FPRInfo::fpRegT0, MacroAssembler::Address(GPRInfo::regT1));
#endif
            break;

#if USE(JSVALUE64)
        case UnboxedInt52InGPR:
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.rshift64(AssemblyHelpers::TrustedImm32(JSValue::int52ShiftAmount), GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            jit.store64(GPRInfo::regT0, scratch + index);
            break;

        case Int52DisplacedInJSStack:
            jit.load64(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::regT0);
            jit.rshift64(AssemblyHelpers::TrustedImm32(JSValue::int52ShiftAmount), GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            jit.store64(GPRInfo::regT0, scratch + index);
            break;

        case UnboxedStrictInt52InGPR:
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            jit.store64(GPRInfo::regT0, scratch + index);
            break;

        case StrictInt52DisplacedInJSStack:
            jit.load64(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            jit.store64(GPRInfo::regT0, scratch + index);
            break;
#endif

        default:
            break;
        }
    }

    if constexpr (validateDFGDoesGC) {
        if (Options::validateDoesGC()) {
            // We're about to exit optimized code. So, there's no longer any optimized
            // code running that expects no GC. We need to set this before arguments
            // materialization below (see emitRestoreArguments()).

            // Even though we set Heap::m_doesGC in compileOSRExit(), we also need
            // to set it here because compileOSRExit() is only called on the first time
            // we exit from this site, but all subsequent exits will take this compiled
            // ramp without calling compileOSRExit() first.
            DoesGCCheck check;
            check.u.encoded = DoesGCCheck::encode(true, DoesGCCheck::Special::DFGOSRExit);
#if USE(JSVALUE64)
            jit.store64(CCallHelpers::TrustedImm64(check.u.encoded), vm.addressOfDoesGC());
#else
            jit.store32(CCallHelpers::TrustedImm32(check.u.other), &vm.addressOfDoesGC()->u.other);
            jit.store32(CCallHelpers::TrustedImm32(check.u.nodeIndex), &vm.addressOfDoesGC()->u.nodeIndex);
#endif
        }
    }
    
    // Need to ensure that the stack pointer accounts for the worst-case stack usage at exit. This
    // could toast some stack that the DFG used. We need to do it before storing to stack offsets
    // used by baseline.
    jit.addPtr(
        CCallHelpers::TrustedImm32(
            -jit.codeBlock()->jitCode()->dfgCommon()->requiredRegisterCountForExit * sizeof(Register)),
        CCallHelpers::framePointerRegister, CCallHelpers::stackPointerRegister);

    // Restore the DFG callee saves and then save the ones the baseline JIT uses.
    jit.emitRestoreCalleeSavesFor(jit.codeBlock()->jitCode()->calleeSaveRegisters());
    jit.emitSaveCalleeSavesFor(jit.baselineCodeBlock()->jitCode()->calleeSaveRegisters());

    // The tag registers are needed to materialize recoveries below.
    jit.emitMaterializeTagCheckRegisters();

    if (inlineStackContainsActiveCheckpoint) {
        EncodedJSValue* tmpScratch = scratch + operands.tmpIndex(0);
        jit.setupArguments<decltype(operationMaterializeOSRExitSideState)>(CCallHelpers::TrustedImmPtr(&vm), CCallHelpers::TrustedImmPtr(&exit), CCallHelpers::TrustedImmPtr(tmpScratch));
        jit.prepareCallOperation(vm);
        jit.move(AssemblyHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationMaterializeOSRExitSideState)), GPRInfo::nonArgGPR0);
        jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    }

    // Do all data format conversions and store the results into the stack.

#if USE(JSVALUE64)
    constexpr GPRReg srcBufferGPR = GPRInfo::regT2;
    constexpr GPRReg destBufferGPR = GPRInfo::regT3;
    constexpr GPRReg undefinedGPR = GPRInfo::regT4;
    bool undefinedGPRIsInitialized = false;

    jit.move(CCallHelpers::TrustedImmPtr(scratch), srcBufferGPR);
    jit.move(CCallHelpers::framePointerRegister, destBufferGPR);
    CCallHelpers::CopySpooler spooler(CCallHelpers::CopySpooler::BufferRegs::AllowModification, jit, srcBufferGPR, destBufferGPR, GPRInfo::regT0, GPRInfo::regT1);
#endif
    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];
        Operand operand = operands.operandForIndex(index);
        if (operand.isTmp())
            continue;

        if (operand.isLocal() && operand.toLocal() < static_cast<int>(CodeBlock::calleeSaveSpaceAsVirtualRegisters(*jit.baselineCodeBlock()->jitCode()->calleeSaveRegisters())))
            continue;

        switch (recovery.technique()) {
        case Constant: {
#if USE(JSVALUE64)
            EncodedJSValue currentConstant = JSValue::encode(recovery.constant());
            if (currentConstant == encodedJSUndefined()) {
                if (UNLIKELY(!undefinedGPRIsInitialized)) {
                    jit.move(CCallHelpers::TrustedImm64(encodedJSUndefined()), undefinedGPR);
                    undefinedGPRIsInitialized = true;
                }
                spooler.copyGPR(undefinedGPR);
            } else
                spooler.moveConstant(currentConstant);
            spooler.storeGPR(operand.virtualRegister().offset() * sizeof(CPURegister));
            break;
#else
            FALLTHROUGH;
#endif
        }
        case DisplacedInJSStack:
        case BooleanDisplacedInJSStack:
        case Int32DisplacedInJSStack:
        case CellDisplacedInJSStack:
        case DoubleDisplacedInJSStack:
        case UnboxedBooleanInGPR:
        case UnboxedInt32InGPR:
        case UnboxedCellInGPR:
        case UnboxedDoubleInFPR:
        case InFPR:
#if USE(JSVALUE64)
        case InGPR:
        case UnboxedInt52InGPR:
        case Int52DisplacedInJSStack:
        case UnboxedStrictInt52InGPR:
        case StrictInt52DisplacedInJSStack:
            spooler.loadGPR(index * sizeof(CPURegister));
            spooler.storeGPR(operand.virtualRegister().offset() * sizeof(CPURegister));
            break;
#else // not USE(JSVALUE64)
        case InPair:
            jit.loadValue(scratch + index, JSRInfo::jsRegT10);
            jit.storeValue(JSRInfo::jsRegT10, AssemblyHelpers::addressFor(operand));
            break;
#endif // USE(JSVALUE64)

        case DirectArgumentsThatWereNotCreated:
        case ClonedArgumentsThatWereNotCreated:
            // Don't do this, yet.
            break;

        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
#if USE(JSVALUE64)
    spooler.finalizeGPR();
#endif

    // Now that things on the stack are recovered, do the arguments recovery. We assume that arguments
    // recoveries don't recursively refer to each other. But, we don't try to assume that they only
    // refer to certain ranges of locals. Hence why we need to do this here, once the stack is sensible.
    // Note that we also roughly assume that the arguments might still be materialized outside of its
    // inline call frame scope - but for now the DFG wouldn't do that.

    emitRestoreArguments(jit, vm, operands);

    // Adjust the old JIT's execute counter. Since we are exiting OSR, we know
    // that all new calls into this code will go to the new JIT, so the execute
    // counter only affects call frames that performed OSR exit and call frames
    // that were still executing the old JIT at the time of another call frame's
    // OSR exit. We want to ensure that the following is true:
    //
    // (a) Code the performs an OSR exit gets a chance to reenter optimized
    //     code eventually, since optimized code is faster. But we don't
    //     want to do such reentery too aggressively (see (c) below).
    //
    // (b) If there is code on the call stack that is still running the old
    //     JIT's code and has never OSR'd, then it should get a chance to
    //     perform OSR entry despite the fact that we've exited.
    //
    // (c) Code the performs an OSR exit should not immediately retry OSR
    //     entry, since both forms of OSR are expensive. OSR entry is
    //     particularly expensive.
    //
    // (d) Frequent OSR failures, even those that do not result in the code
    //     running in a hot loop, result in recompilation getting triggered.
    //
    // To ensure (c), we'd like to set the execute counter to
    // counterValueForOptimizeAfterWarmUp(). This seems like it would endanger
    // (a) and (b), since then every OSR exit would delay the opportunity for
    // every call frame to perform OSR entry. Essentially, if OSR exit happens
    // frequently and the function has few loops, then the counter will never
    // become non-negative and OSR entry will never be triggered. OSR entry
    // will only happen if a loop gets hot in the old JIT, which does a pretty
    // good job of ensuring (a) and (b). But that doesn't take care of (d),
    // since each speculation failure would reset the execute counter.
    // So we check here if the number of speculation failures is significantly
    // larger than the number of successes (we want 90% success rate), and if
    // there have been a large enough number of failures. If so, we set the
    // counter to 0; otherwise we set the counter to
    // counterValueForOptimizeAfterWarmUp().

    handleExitCounts(vm, jit, exit);

    // Reify inlined call frames.

    reifyInlinedCallFrames(jit, exit);

    // And finish.
    adjustAndJumpToTarget(vm, jit, exit);
}

JSC_DEFINE_JIT_OPERATION(operationDebugPrintSpeculationFailure, void, (CallFrame* callFrame, void* debugInfoRaw, void* scratch))
{
    VM& vm = callFrame->deprecatedVM();
    NativeCallFrameTracer tracer(vm, callFrame);
    ActiveScratchBufferScope activeScratchBufferScope(ScratchBuffer::fromData(scratch), GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters);

    SpeculationFailureDebugInfo* debugInfo = static_cast<SpeculationFailureDebugInfo*>(debugInfoRaw);
    CodeBlock* codeBlock = debugInfo->codeBlock;
    CodeBlock* alternative = codeBlock->alternative();
    dataLog("Speculation failure in ", *codeBlock);
    dataLog(" @ exit #", vm.osrExitIndex, " (", debugInfo->bytecodeIndex, ", ", exitKindToString(debugInfo->kind), ") with ");
    if (alternative) {
        dataLog(
            "executeCounter = ", alternative->jitExecuteCounter(),
            ", reoptimizationRetryCounter = ", alternative->reoptimizationRetryCounter(),
            ", optimizationDelayCounter = ", alternative->optimizationDelayCounter());
    } else
        dataLog("no alternative code block (i.e. we've been jettisoned)");
    dataLog(", osrExitCounter = ", codeBlock->osrExitCounter(), "\n");
    dataLog("    GPRs at time of exit:");
    char* scratchPointer = static_cast<char*>(scratch);
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
        GPRReg gpr = GPRInfo::toRegister(i);
        dataLog(" ", GPRInfo::debugName(gpr), ":", RawPointer(*reinterpret_cast_ptr<void**>(scratchPointer)));
        scratchPointer += sizeof(EncodedJSValue);
    }
    dataLog("\n");
    dataLog("    FPRs at time of exit:");
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        FPRReg fpr = FPRInfo::toRegister(i);
        dataLog(" ", FPRInfo::debugName(fpr), ":");
        uint64_t bits = *reinterpret_cast_ptr<uint64_t*>(scratchPointer);
        double value = *reinterpret_cast_ptr<double*>(scratchPointer);
        dataLogF("%llx:%lf", static_cast<long long>(bits), value);
        scratchPointer += sizeof(EncodedJSValue);
    }
    dataLog("\n");
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
