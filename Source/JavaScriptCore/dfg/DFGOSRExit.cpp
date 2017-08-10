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
#include "DFGOSRExit.h"

#if ENABLE(DFG_JIT)

#include "AssemblyHelpers.h"
#include "DFGGraph.h"
#include "DFGMayExit.h"
#include "DFGOSRExitCompilerCommon.h"
#include "DFGOSRExitPreparation.h"
#include "DFGOperations.h"
#include "DFGSpeculativeJIT.h"
#include "FrameTracers.h"
#include "JSCInlines.h"
#include "OperandsInlines.h"

namespace JSC { namespace DFG {

OSRExit::OSRExit(ExitKind kind, JSValueSource jsValueSource, MethodOfGettingAValueProfile valueProfile, SpeculativeJIT* jit, unsigned streamIndex, unsigned recoveryIndex)
    : OSRExitBase(kind, jit->m_origin.forExit, jit->m_origin.semantic, jit->m_origin.wasHoisted)
    , m_jsValueSource(jsValueSource)
    , m_valueProfile(valueProfile)
    , m_recoveryIndex(recoveryIndex)
    , m_streamIndex(streamIndex)
{
    bool canExit = jit->m_origin.exitOK;
    if (!canExit && jit->m_currentNode) {
        ExitMode exitMode = mayExit(jit->m_jit.graph(), jit->m_currentNode);
        canExit = exitMode == ExitMode::Exits || exitMode == ExitMode::ExitsForExceptions;
    }
    DFG_ASSERT(jit->m_jit.graph(), jit->m_currentNode, canExit);
}

void OSRExit::setPatchableCodeOffset(MacroAssembler::PatchableJump check)
{
    m_patchableCodeOffset = check.m_jump.m_label.m_offset;
}

MacroAssembler::Jump OSRExit::getPatchableCodeOffsetAsJump() const
{
    return MacroAssembler::Jump(AssemblerLabel(m_patchableCodeOffset));
}

CodeLocationJump OSRExit::codeLocationForRepatch(CodeBlock* dfgCodeBlock) const
{
    return CodeLocationJump(dfgCodeBlock->jitCode()->dataAddressAtOffset(m_patchableCodeOffset));
}

void OSRExit::correctJump(LinkBuffer& linkBuffer)
{
    MacroAssembler::Label label;
    label.m_label.m_offset = m_patchableCodeOffset;
    m_patchableCodeOffset = linkBuffer.offsetOf(label);
}

void OSRExit::emitRestoreArguments(CCallHelpers& jit, const Operands<ValueRecovery>& operands)
{
    HashMap<MinifiedID, int> alreadyAllocatedArguments; // Maps phantom arguments node ID to operand.
    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];
        int operand = operands.operandForIndex(index);

        if (recovery.technique() != DirectArgumentsThatWereNotCreated
            && recovery.technique() != ClonedArgumentsThatWereNotCreated)
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
                AssemblyHelpers::addressFor(stackOffset + CallFrameSlot::callee),
                GPRInfo::regT0);
        } else {
            jit.move(
                AssemblyHelpers::TrustedImmPtr(inlineCallFrame->calleeRecovery.constant().asCell()),
                GPRInfo::regT0);
        }

        if (!inlineCallFrame || inlineCallFrame->isVarargs()) {
            jit.load32(
                AssemblyHelpers::payloadFor(stackOffset + CallFrameSlot::argumentCount),
                GPRInfo::regT1);
        } else {
            jit.move(
                AssemblyHelpers::TrustedImm32(inlineCallFrame->arguments.size()),
                GPRInfo::regT1);
        }

        jit.setupArgumentsWithExecState(
            AssemblyHelpers::TrustedImmPtr(inlineCallFrame), GPRInfo::regT0, GPRInfo::regT1);
        switch (recovery.technique()) {
        case DirectArgumentsThatWereNotCreated:
            jit.move(AssemblyHelpers::TrustedImmPtr(bitwise_cast<void*>(operationCreateDirectArgumentsDuringExit)), GPRInfo::nonArgGPR0);
            break;
        case ClonedArgumentsThatWereNotCreated:
            jit.move(AssemblyHelpers::TrustedImmPtr(bitwise_cast<void*>(operationCreateClonedArgumentsDuringExit)), GPRInfo::nonArgGPR0);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        jit.call(GPRInfo::nonArgGPR0);
        jit.storeCell(GPRInfo::returnValueGPR, AssemblyHelpers::addressFor(operand));

        alreadyAllocatedArguments.add(id, operand);
    }
}

void JIT_OPERATION OSRExit::compileOSRExit(ExecState* exec)
{
    VM* vm = &exec->vm();
    auto scope = DECLARE_THROW_SCOPE(*vm);

    if (vm->callFrameForCatch)
        RELEASE_ASSERT(vm->callFrameForCatch == exec);

    CodeBlock* codeBlock = exec->codeBlock();
    ASSERT(codeBlock);
    ASSERT(codeBlock->jitType() == JITCode::DFGJIT);

    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(vm->heap);

    uint32_t exitIndex = vm->osrExitIndex;
    OSRExit& exit = codeBlock->jitCode()->dfg()->osrExit[exitIndex];

    if (vm->callFrameForCatch)
        ASSERT(exit.m_kind == GenericUnwind);
    if (exit.isExceptionHandler())
        ASSERT_UNUSED(scope, !!scope.exception());
    
    prepareCodeOriginForOSRExit(exec, exit.m_codeOrigin);

    // Compute the value recoveries.
    Operands<ValueRecovery> operands;
    codeBlock->jitCode()->dfg()->variableEventStream.reconstruct(codeBlock, exit.m_codeOrigin, codeBlock->jitCode()->dfg()->minifiedDFG, exit.m_streamIndex, operands);

    SpeculationRecovery* recovery = 0;
    if (exit.m_recoveryIndex != UINT_MAX)
        recovery = &codeBlock->jitCode()->dfg()->speculationRecovery[exit.m_recoveryIndex];

    {
        CCallHelpers jit(codeBlock);

        if (exit.m_kind == GenericUnwind) {
            // We are acting as a defacto op_catch because we arrive here from genericUnwind().
            // So, we must restore our call frame and stack pointer.
            jit.restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(*vm);
            jit.loadPtr(vm->addressOfCallFrameForCatch(), GPRInfo::callFrameRegister);
        }
        jit.addPtr(
            CCallHelpers::TrustedImm32(codeBlock->stackPointerOffset() * sizeof(Register)),
            GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

        jit.jitAssertHasValidCallFrame();

        if (UNLIKELY(vm->m_perBytecodeProfiler && codeBlock->jitCode()->dfgCommon()->compilation)) {
            Profiler::Database& database = *vm->m_perBytecodeProfiler;
            Profiler::Compilation* compilation = codeBlock->jitCode()->dfgCommon()->compilation.get();

            Profiler::OSRExit* profilerExit = compilation->addOSRExit(
                exitIndex, Profiler::OriginStack(database, codeBlock, exit.m_codeOrigin),
                exit.m_kind, exit.m_kind == UncountableInvalidation);
            jit.add64(CCallHelpers::TrustedImm32(1), CCallHelpers::AbsoluteAddress(profilerExit->counterAddress()));
        }

        compileExit(jit, *vm, exit, operands, recovery);

        LinkBuffer patchBuffer(jit, codeBlock);
        exit.m_code = FINALIZE_CODE_IF(
            shouldDumpDisassembly() || Options::verboseOSR() || Options::verboseDFGOSRExit(),
            patchBuffer,
            ("DFG OSR exit #%u (%s, %s) from %s, with operands = %s",
                exitIndex, toCString(exit.m_codeOrigin).data(),
                exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
                toCString(ignoringContext<DumpContext>(operands)).data()));
    }

    MacroAssembler::repatchJump(exit.codeLocationForRepatch(codeBlock), CodeLocationLabel(exit.m_code.code()));

    vm->osrExitJumpDestination = exit.m_code.code().executableAddress();
}

void OSRExit::compileExit(CCallHelpers& jit, VM& vm, const OSRExit& exit, const Operands<ValueRecovery>& operands, SpeculationRecovery* recovery)
{
    jit.jitAssertTagsInPlace();

    // Pro-forma stuff.
    if (Options::printEachOSRExit()) {
        SpeculationFailureDebugInfo* debugInfo = new SpeculationFailureDebugInfo;
        debugInfo->codeBlock = jit.codeBlock();
        debugInfo->kind = exit.m_kind;
        debugInfo->bytecodeOffset = exit.m_codeOrigin.bytecodeIndex;

        jit.debugCall(vm, debugOperationPrintSpeculationFailure, debugInfo);
    }

    // Perform speculation recovery. This only comes into play when an operation
    // starts mutating state before verifying the speculation it has already made.

    if (recovery) {
        switch (recovery->type()) {
        case SpeculativeAdd:
            jit.sub32(recovery->src(), recovery->dest());
#if USE(JSVALUE64)
            jit.or64(GPRInfo::tagTypeNumberRegister, recovery->dest());
#endif
            break;

        case SpeculativeAddImmediate:
            jit.sub32(AssemblyHelpers::Imm32(recovery->immediate()), recovery->dest());
#if USE(JSVALUE64)
            jit.or64(GPRInfo::tagTypeNumberRegister, recovery->dest());
#endif
            break;

        case BooleanSpeculationCheck:
#if USE(JSVALUE64)
            jit.xor64(AssemblyHelpers::TrustedImm32(static_cast<int32_t>(ValueFalse)), recovery->dest());
#endif
            break;

        default:
            break;
        }
    }

    // Refine some array and/or value profile, if appropriate.

    if (!!exit.m_jsValueSource) {
        if (exit.m_kind == BadCache || exit.m_kind == BadIndexingType) {
            // If the instruction that this originated from has an array profile, then
            // refine it. If it doesn't, then do nothing. The latter could happen for
            // hoisted checks, or checks emitted for operations that didn't have array
            // profiling - either ops that aren't array accesses at all, or weren't
            // known to be array acceses in the bytecode. The latter case is a FIXME
            // while the former case is an outcome of a CheckStructure not knowing why
            // it was emitted (could be either due to an inline cache of a property
            // property access, or due to an array profile).

            CodeOrigin codeOrigin = exit.m_codeOriginForExitProfile;
            if (ArrayProfile* arrayProfile = jit.baselineCodeBlockFor(codeOrigin)->getArrayProfile(codeOrigin.bytecodeIndex)) {
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
#if USE(JSVALUE64)
                jit.load8(AssemblyHelpers::Address(value, JSCell::indexingTypeAndMiscOffset()), scratch1);
#else
                jit.load8(AssemblyHelpers::Address(scratch1, Structure::indexingTypeIncludingHistoryOffset()), scratch1);
#endif
                jit.move(AssemblyHelpers::TrustedImm32(1), scratch2);
                jit.lshift32(scratch1, scratch2);
                jit.or32(scratch2, AssemblyHelpers::AbsoluteAddress(arrayProfile->addressOfArrayModes()));

                if (isARM64()) {
                    jit.popToRestore(scratch2);
                    jit.popToRestore(scratch1);
                } else {
                    jit.pop(scratch2);
                    jit.pop(scratch1);
                }
            }
        }

        if (MethodOfGettingAValueProfile profile = exit.m_valueProfile) {
#if USE(JSVALUE64)
            if (exit.m_jsValueSource.isAddress()) {
                // We can't be sure that we have a spare register. So use the tagTypeNumberRegister,
                // since we know how to restore it.
                jit.load64(AssemblyHelpers::Address(exit.m_jsValueSource.asAddress()), GPRInfo::tagTypeNumberRegister);
                profile.emitReportValue(jit, JSValueRegs(GPRInfo::tagTypeNumberRegister));
                jit.move(AssemblyHelpers::TrustedImm64(TagTypeNumber), GPRInfo::tagTypeNumberRegister);
            } else
                profile.emitReportValue(jit, JSValueRegs(exit.m_jsValueSource.gpr()));
#else // not USE(JSVALUE64)
            if (exit.m_jsValueSource.isAddress()) {
                // Save a register so we can use it.
                GPRReg scratchPayload = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.base());
                GPRReg scratchTag = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.base(), scratchPayload);
                jit.pushToSave(scratchPayload);
                jit.pushToSave(scratchTag);

                JSValueRegs scratch(scratchTag, scratchPayload);
                
                jit.loadValue(exit.m_jsValueSource.asAddress(), scratch);
                profile.emitReportValue(jit, scratch);
                
                jit.popToRestore(scratchTag);
                jit.popToRestore(scratchPayload);
            } else if (exit.m_jsValueSource.hasKnownTag()) {
                GPRReg scratchTag = AssemblyHelpers::selectScratchGPR(exit.m_jsValueSource.payloadGPR());
                jit.pushToSave(scratchTag);
                jit.move(AssemblyHelpers::TrustedImm32(exit.m_jsValueSource.tag()), scratchTag);
                JSValueRegs value(scratchTag, exit.m_jsValueSource.payloadGPR());
                profile.emitReportValue(jit, value);
                jit.popToRestore(scratchTag);
            } else
                profile.emitReportValue(jit, exit.m_jsValueSource.regs());
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
    EncodedJSValue* scratch = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : 0;

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
            jit.store32(
                recovery.tagGPR(),
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag);
            jit.store32(
                recovery.payloadGPR(),
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload);
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

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];

        switch (recovery.technique()) {
        case DisplacedInJSStack:
        case CellDisplacedInJSStack:
        case BooleanDisplacedInJSStack:
        case Int32DisplacedInJSStack:
        case DoubleDisplacedInJSStack:
#if USE(JSVALUE64)
        case Int52DisplacedInJSStack:
        case StrictInt52DisplacedInJSStack:
            jit.load64(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, scratch + index);
            break;
#else
            jit.load32(
                AssemblyHelpers::tagFor(recovery.virtualRegister()),
                GPRInfo::regT0);
            jit.load32(
                AssemblyHelpers::payloadFor(recovery.virtualRegister()),
                GPRInfo::regT1);
            jit.store32(
                GPRInfo::regT0,
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag);
            jit.store32(
                GPRInfo::regT1,
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload);
            break;
#endif

        default:
            break;
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
    jit.emitRestoreCalleeSaves();
    jit.emitSaveCalleeSavesFor(jit.baselineCodeBlock());

    // The tag registers are needed to materialize recoveries below.
    jit.emitMaterializeTagCheckRegisters();

    if (exit.isExceptionHandler())
        jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(vm);

    // Do all data format conversions and store the results into the stack.

    for (size_t index = 0; index < operands.size(); ++index) {
        const ValueRecovery& recovery = operands[index];
        VirtualRegister reg = operands.virtualRegisterForIndex(index);

        if (reg.isLocal() && reg.toLocal() < static_cast<int>(jit.baselineCodeBlock()->calleeSaveSpaceAsVirtualRegisters()))
            continue;

        int operand = reg.offset();

        switch (recovery.technique()) {
        case DisplacedInJSStack:
        case InFPR:
#if USE(JSVALUE64)
        case InGPR:
        case UnboxedCellInGPR:
        case CellDisplacedInJSStack:
        case BooleanDisplacedInJSStack:
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
            break;
#else // not USE(JSVALUE64)
        case InPair:
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.tag,
                GPRInfo::regT0);
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload,
                GPRInfo::regT1);
            jit.store32(
                GPRInfo::regT0,
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                GPRInfo::regT1,
                AssemblyHelpers::payloadFor(operand));
            break;

        case UnboxedCellInGPR:
        case CellDisplacedInJSStack:
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload,
                GPRInfo::regT0);
            jit.store32(
                AssemblyHelpers::TrustedImm32(JSValue::CellTag),
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                GPRInfo::regT0,
                AssemblyHelpers::payloadFor(operand));
            break;

        case UnboxedBooleanInGPR:
        case BooleanDisplacedInJSStack:
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload,
                GPRInfo::regT0);
            jit.store32(
                AssemblyHelpers::TrustedImm32(JSValue::BooleanTag),
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                GPRInfo::regT0,
                AssemblyHelpers::payloadFor(operand));
            break;
#endif // USE(JSVALUE64)

        case UnboxedInt32InGPR:
        case Int32DisplacedInJSStack:
#if USE(JSVALUE64)
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.zeroExtend32ToPtr(GPRInfo::regT0, GPRInfo::regT0);
            jit.or64(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
#else
            jit.load32(
                &bitwise_cast<EncodedValueDescriptor*>(scratch + index)->asBits.payload,
                GPRInfo::regT0);
            jit.store32(
                AssemblyHelpers::TrustedImm32(JSValue::Int32Tag),
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                GPRInfo::regT0,
                AssemblyHelpers::payloadFor(operand));
#endif
            break;

#if USE(JSVALUE64)
        case UnboxedInt52InGPR:
        case Int52DisplacedInJSStack:
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.rshift64(
                AssemblyHelpers::TrustedImm32(JSValue::int52ShiftAmount), GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
            break;

        case UnboxedStrictInt52InGPR:
        case StrictInt52DisplacedInJSStack:
            jit.load64(scratch + index, GPRInfo::regT0);
            jit.boxInt52(GPRInfo::regT0, GPRInfo::regT0, GPRInfo::regT1, FPRInfo::fpRegT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
            break;
#endif

        case UnboxedDoubleInFPR:
        case DoubleDisplacedInJSStack:
            jit.move(AssemblyHelpers::TrustedImmPtr(scratch + index), GPRInfo::regT0);
            jit.loadDouble(MacroAssembler::Address(GPRInfo::regT0), FPRInfo::fpRegT0);
            jit.purifyNaN(FPRInfo::fpRegT0);
#if USE(JSVALUE64)
            jit.boxDouble(FPRInfo::fpRegT0, GPRInfo::regT0);
            jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
#else
            jit.storeDouble(FPRInfo::fpRegT0, AssemblyHelpers::addressFor(operand));
#endif
            break;

        case Constant:
#if USE(JSVALUE64)
            jit.store64(
                AssemblyHelpers::TrustedImm64(JSValue::encode(recovery.constant())),
                AssemblyHelpers::addressFor(operand));
#else
            jit.store32(
                AssemblyHelpers::TrustedImm32(recovery.constant().tag()),
                AssemblyHelpers::tagFor(operand));
            jit.store32(
                AssemblyHelpers::TrustedImm32(recovery.constant().payload()),
                AssemblyHelpers::payloadFor(operand));
#endif
            break;

        case DirectArgumentsThatWereNotCreated:
        case ClonedArgumentsThatWereNotCreated:
            // Don't do this, yet.
            break;

        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    // Now that things on the stack are recovered, do the arguments recovery. We assume that arguments
    // recoveries don't recursively refer to each other. But, we don't try to assume that they only
    // refer to certain ranges of locals. Hence why we need to do this here, once the stack is sensible.
    // Note that we also roughly assume that the arguments might still be materialized outside of its
    // inline call frame scope - but for now the DFG wouldn't do that.

    emitRestoreArguments(jit, operands);

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

    handleExitCounts(jit, exit);

    // Reify inlined call frames.

    reifyInlinedCallFrames(jit, exit);

    // And finish.
    adjustAndJumpToTarget(vm, jit, exit);
}

void JIT_OPERATION OSRExit::debugOperationPrintSpeculationFailure(ExecState* exec, void* debugInfoRaw, void* scratch)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    SpeculationFailureDebugInfo* debugInfo = static_cast<SpeculationFailureDebugInfo*>(debugInfoRaw);
    CodeBlock* codeBlock = debugInfo->codeBlock;
    CodeBlock* alternative = codeBlock->alternative();
    dataLog("Speculation failure in ", *codeBlock);
    dataLog(" @ exit #", vm->osrExitIndex, " (bc#", debugInfo->bytecodeOffset, ", ", exitKindToString(debugInfo->kind), ") with ");
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
