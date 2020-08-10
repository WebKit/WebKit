/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "FTLOSRExitCompiler.h"

#if ENABLE(FTL_JIT)

#include "BytecodeStructs.h"
#include "CheckpointOSRExitSideState.h"
#include "DFGOSRExitCompilerCommon.h"
#include "FTLJITCode.h"
#include "FTLLocation.h"
#include "FTLOSRExit.h"
#include "FTLOperations.h"
#include "FTLSaveRestore.h"
#include "FTLState.h"
#include "JSCJSValueInlines.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "OperandsInlines.h"
#include "ProbeContext.h"

#include <wtf/Scope.h>

namespace JSC { namespace FTL {

using namespace DFG;

static void reboxAccordingToFormat(
    DataFormat format, AssemblyHelpers& jit, GPRReg value, GPRReg scratch1, GPRReg scratch2)
{
    switch (format) {
    case DataFormatInt32: {
        jit.zeroExtend32ToWord(value, value);
        jit.or64(GPRInfo::numberTagRegister, value);
        break;
    }

    case DataFormatInt52: {
        jit.rshift64(AssemblyHelpers::TrustedImm32(JSValue::int52ShiftAmount), value);
        jit.moveDoubleTo64(FPRInfo::fpRegT0, scratch2);
        jit.boxInt52(value, value, scratch1, FPRInfo::fpRegT0);
        jit.move64ToDouble(scratch2, FPRInfo::fpRegT0);
        break;
    }

    case DataFormatStrictInt52: {
        jit.moveDoubleTo64(FPRInfo::fpRegT0, scratch2);
        jit.boxInt52(value, value, scratch1, FPRInfo::fpRegT0);
        jit.move64ToDouble(scratch2, FPRInfo::fpRegT0);
        break;
    }

    case DataFormatBoolean: {
        jit.zeroExtend32ToWord(value, value);
        jit.or32(MacroAssembler::TrustedImm32(JSValue::ValueFalse), value);
        break;
    }

    case DataFormatJS: {
        // Done already!
        break;
    }

    case DataFormatDouble: {
        jit.moveDoubleTo64(FPRInfo::fpRegT0, scratch1);
        jit.move64ToDouble(value, FPRInfo::fpRegT0);
        jit.purifyNaN(FPRInfo::fpRegT0);
        jit.boxDouble(FPRInfo::fpRegT0, value);
        jit.move64ToDouble(scratch1, FPRInfo::fpRegT0);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

static void compileRecovery(
    CCallHelpers& jit, const ExitValue& value,
    Vector<B3::ValueRep>& valueReps,
    char* registerScratch,
    const HashMap<ExitTimeObjectMaterialization*, EncodedJSValue*>& materializationToPointer)
{
    switch (value.kind()) {
    case ExitValueDead:
        jit.move(MacroAssembler::TrustedImm64(JSValue::encode(jsUndefined())), GPRInfo::regT0);
        break;
            
    case ExitValueConstant:
        jit.move(MacroAssembler::TrustedImm64(JSValue::encode(value.constant())), GPRInfo::regT0);
        break;
            
    case ExitValueArgument:
        Location::forValueRep(valueReps[value.exitArgument().argument()]).restoreInto(
            jit, registerScratch, GPRInfo::regT0);
        break;
            
    case ExitValueInJSStack:
    case ExitValueInJSStackAsInt32:
    case ExitValueInJSStackAsInt52:
    case ExitValueInJSStackAsDouble:
        jit.load64(AssemblyHelpers::addressFor(value.virtualRegister()), GPRInfo::regT0);
        break;
            
    case ExitValueMaterializeNewObject:
        jit.loadPtr(materializationToPointer.get(value.objectMaterialization()), GPRInfo::regT0);
        break;
            
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
        
    reboxAccordingToFormat(
        value.dataFormat(), jit, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT2);
}

static void compileStub(VM& vm, unsigned exitID, JITCode* jitCode, OSRExit& exit, CodeBlock* codeBlock)
{
    // This code requires framePointerRegister is the same as callFrameRegister
    static_assert(MacroAssembler::framePointerRegister == GPRInfo::callFrameRegister, "MacroAssembler::framePointerRegister and GPRInfo::callFrameRegister must be the same");

    CCallHelpers jit(codeBlock);

    // The first thing we need to do is restablish our frame in the case of an exception.
    if (exit.isGenericUnwindHandler()) {
        RELEASE_ASSERT(vm.callFrameForCatch); // The first time we hit this exit, like at all other times, this field should be non-null.
        jit.restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(vm.topEntryFrame);
        jit.loadPtr(vm.addressOfCallFrameForCatch(), MacroAssembler::framePointerRegister);
        jit.addPtr(CCallHelpers::TrustedImm32(codeBlock->stackPointerOffset() * sizeof(Register)),
            MacroAssembler::framePointerRegister, CCallHelpers::stackPointerRegister);

        // Do a pushToSave because that's what the exit compiler below expects the stack
        // to look like because that's the last thing the ExitThunkGenerator does. The code
        // below doesn't actually use the value that was pushed, but it does rely on the
        // general shape of the stack being as it is in the non-exception OSR case.
        jit.pushToSaveImmediateWithoutTouchingRegisters(CCallHelpers::TrustedImm32(0xbadbeef));
    }

    // We need scratch space to save all registers, to build up the JS stack, to deal with unwind
    // fixup, pointers to all of the objects we materialize, and the elements inside those objects
    // that we materialize.
    
    // Figure out how much space we need for those object allocations.
    unsigned numMaterializations = 0;
    size_t maxMaterializationNumArguments = 0;
    for (ExitTimeObjectMaterialization* materialization : exit.m_descriptor->m_materializations) {
        numMaterializations++;
        
        maxMaterializationNumArguments = std::max(
            maxMaterializationNumArguments,
            materialization->properties().size());
    }
    
    ScratchBuffer* scratchBuffer = vm.scratchBufferForSize(
        sizeof(EncodedJSValue) * (
            exit.m_descriptor->m_values.size() + numMaterializations + maxMaterializationNumArguments) +
        requiredScratchMemorySizeInBytes() +
        codeBlock->calleeSaveRegisters()->size() * sizeof(uint64_t));
    EncodedJSValue* scratch = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : nullptr;
    EncodedJSValue* materializationPointers = scratch + exit.m_descriptor->m_values.size();
    EncodedJSValue* materializationArguments = materializationPointers + numMaterializations;
    char* registerScratch = bitwise_cast<char*>(materializationArguments + maxMaterializationNumArguments);
    uint64_t* unwindScratch = bitwise_cast<uint64_t*>(registerScratch + requiredScratchMemorySizeInBytes());
    
    HashMap<ExitTimeObjectMaterialization*, EncodedJSValue*> materializationToPointer;
    unsigned materializationCount = 0;
    for (ExitTimeObjectMaterialization* materialization : exit.m_descriptor->m_materializations) {
        materializationToPointer.add(
            materialization, materializationPointers + materializationCount++);
    }

    auto recoverValue = [&] (const ExitValue& value) {
        compileRecovery(
            jit, value,
            exit.m_valueReps,
            registerScratch, materializationToPointer);
    };
    
    // Note that we come in here, the stack used to be as B3 left it except that someone called pushToSave().
    // We don't care about the value they saved. But, we do appreciate the fact that they did it, because we use
    // that slot for saveAllRegisters().

    saveAllRegisters(jit, registerScratch);
    
    if constexpr (validateDFGDoesGC) {
        if (Options::validateDoesGC()) {
            // We're about to exit optimized code. So, there's no longer any optimized
            // code running that expects no GC. We need to set this before object
            // materialization below.

            // Even though we set Heap::m_doesGC in compileFTLOSRExit(), we also need
            // to set it here because compileFTLOSRExit() is only called on the first time
            // we exit from this site, but all subsequent exits will take this compiled
            // ramp without calling compileFTLOSRExit() first.
            jit.store32(CCallHelpers::TrustedImm32(DoesGCCheck::encode(true, DoesGCCheck::Special::FTLOSRExit)), vm.heap.addressOfDoesGC());
        }
    }

    // Bring the stack back into a sane form and assert that it's sane.
    jit.popToRestore(GPRInfo::regT0);
    jit.checkStackPointerAlignment();
    
    if (UNLIKELY(vm.m_perBytecodeProfiler && jitCode->dfgCommon()->compilation)) {
        Profiler::Database& database = *vm.m_perBytecodeProfiler;
        Profiler::Compilation* compilation = jitCode->dfgCommon()->compilation.get();
        
        Profiler::OSRExit* profilerExit = compilation->addOSRExit(
            exitID, Profiler::OriginStack(database, codeBlock, exit.m_codeOrigin),
            exit.m_kind, exit.m_kind == UncountableInvalidation);
        jit.add64(CCallHelpers::TrustedImm32(1), CCallHelpers::AbsoluteAddress(profilerExit->counterAddress()));
    }

    // The remaining code assumes that SP/FP are in the same state that they were in the FTL's
    // call frame.
    
    // Get the call frame and tag thingies.
    // Restore the exiting function's callFrame value into a regT4
    jit.move(MacroAssembler::TrustedImm64(JSValue::NumberTag), GPRInfo::numberTagRegister);
    jit.move(MacroAssembler::TrustedImm64(JSValue::NotCellMask), GPRInfo::notCellMaskRegister);
    
    // Do some value profiling.
    if (exit.m_descriptor->m_profileDataFormat != DataFormatNone) {
        Location::forValueRep(exit.m_valueReps[0]).restoreInto(jit, registerScratch, GPRInfo::regT0);
        reboxAccordingToFormat(
            exit.m_descriptor->m_profileDataFormat, jit, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT2);
        
        if (exit.m_kind == BadCache || exit.m_kind == BadIndexingType) {
            CodeOrigin codeOrigin = exit.m_codeOriginForExitProfile;
            CodeBlock* codeBlock = jit.baselineCodeBlockFor(codeOrigin);
            if (ArrayProfile* arrayProfile = codeBlock->getArrayProfile(codeOrigin.bytecodeIndex())) {
                const Instruction* instruction = codeBlock->instructions().at(codeOrigin.bytecodeIndex()).ptr();
                CCallHelpers::Jump skipProfile;
                if (instruction->is<OpGetById>()) {
                    auto& metadata = instruction->as<OpGetById>().metadata(codeBlock);
                    skipProfile = jit.branch8(CCallHelpers::NotEqual, CCallHelpers::AbsoluteAddress(&metadata.m_modeMetadata.mode), CCallHelpers::TrustedImm32(static_cast<uint8_t>(GetByIdMode::ArrayLength)));
                }

                jit.load32(MacroAssembler::Address(GPRInfo::regT0, JSCell::structureIDOffset()), GPRInfo::regT1);
                jit.store32(GPRInfo::regT1, arrayProfile->addressOfLastSeenStructureID());

                jit.load8(MacroAssembler::Address(GPRInfo::regT0, JSCell::typeInfoTypeOffset()), GPRInfo::regT2);
                jit.sub32(MacroAssembler::TrustedImm32(FirstTypedArrayType), GPRInfo::regT2);
                auto notTypedArray = jit.branch32(MacroAssembler::AboveOrEqual, GPRInfo::regT2, MacroAssembler::TrustedImm32(NumberOfTypedArrayTypesExcludingDataView));
                jit.move(MacroAssembler::TrustedImmPtr(typedArrayModes), GPRInfo::regT1);
                jit.load32(MacroAssembler::BaseIndex(GPRInfo::regT1, GPRInfo::regT2, MacroAssembler::TimesFour), GPRInfo::regT2);
                auto storeArrayModes = jit.jump();

                notTypedArray.link(&jit);
                jit.load8(MacroAssembler::Address(GPRInfo::regT0, JSCell::indexingTypeAndMiscOffset()), GPRInfo::regT1);
                jit.and32(MacroAssembler::TrustedImm32(IndexingModeMask), GPRInfo::regT1);
                jit.move(MacroAssembler::TrustedImm32(1), GPRInfo::regT2);
                jit.lshift32(GPRInfo::regT1, GPRInfo::regT2);
                storeArrayModes.link(&jit);
                jit.or32(GPRInfo::regT2, MacroAssembler::AbsoluteAddress(arrayProfile->addressOfArrayModes()));

                if (skipProfile.isSet())
                    skipProfile.link(&jit);
            }
        }

        if (exit.m_descriptor->m_valueProfile)
            exit.m_descriptor->m_valueProfile.emitReportValue(jit, JSValueRegs(GPRInfo::regT0), GPRInfo::regT1);
    }

    // Materialize all objects. Don't materialize an object until all
    // of the objects it needs have been materialized. We break cycles
    // by populating objects late - we only consider an object as
    // needing another object if the later is needed for the
    // allocation of the former.

    HashSet<ExitTimeObjectMaterialization*> toMaterialize;
    for (ExitTimeObjectMaterialization* materialization : exit.m_descriptor->m_materializations)
        toMaterialize.add(materialization);

    while (!toMaterialize.isEmpty()) {
        unsigned previousToMaterializeSize = toMaterialize.size();

        Vector<ExitTimeObjectMaterialization*> worklist;
        worklist.appendRange(toMaterialize.begin(), toMaterialize.end());
        for (ExitTimeObjectMaterialization* materialization : worklist) {
            // Check if we can do anything about this right now.
            bool allGood = true;
            for (ExitPropertyValue value : materialization->properties()) {
                if (!value.value().isObjectMaterialization())
                    continue;
                if (!value.location().neededForMaterialization())
                    continue;
                if (toMaterialize.contains(value.value().objectMaterialization())) {
                    // Gotta skip this one, since it needs a
                    // materialization that hasn't been materialized.
                    allGood = false;
                    break;
                }
            }
            if (!allGood)
                continue;

            // All systems go for materializing the object. First we
            // recover the values of all of its fields and then we
            // call a function to actually allocate the beast.
            // We only recover the fields that are needed for the allocation.
            for (unsigned propertyIndex = materialization->properties().size(); propertyIndex--;) {
                const ExitPropertyValue& property = materialization->properties()[propertyIndex];
                if (!property.location().neededForMaterialization())
                    continue;

                recoverValue(property.value());
                jit.storePtr(GPRInfo::regT0, materializationArguments + propertyIndex);
            }
            
            static_assert(FunctionTraits<decltype(operationMaterializeObjectInOSR)>::arity < GPRInfo::numberOfArgumentRegisters, "This call assumes that we don't pass arguments on the stack.");
            jit.setupArguments<decltype(operationMaterializeObjectInOSR)>(
                CCallHelpers::TrustedImmPtr(codeBlock->globalObjectFor(materialization->origin())),
                CCallHelpers::TrustedImmPtr(materialization),
                CCallHelpers::TrustedImmPtr(materializationArguments));
            jit.prepareCallOperation(vm);
            jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationMaterializeObjectInOSR)), GPRInfo::nonArgGPR0);
            jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
            jit.storePtr(GPRInfo::returnValueGPR, materializationToPointer.get(materialization));

            // Let everyone know that we're done.
            toMaterialize.remove(materialization);
        }
        
        // We expect progress! This ensures that we crash rather than looping infinitely if there
        // is something broken about this fixpoint. Or, this could happen if we ever violate the
        // "materializations form a DAG" rule.
        RELEASE_ASSERT(toMaterialize.size() < previousToMaterializeSize);
    }

    // Now that all the objects have been allocated, we populate them
    // with the correct values. This time we can recover all the
    // fields, including those that are only needed for the allocation.
    for (ExitTimeObjectMaterialization* materialization : exit.m_descriptor->m_materializations) {
        for (unsigned propertyIndex = materialization->properties().size(); propertyIndex--;) {
            recoverValue(materialization->properties()[propertyIndex].value());
            jit.storePtr(GPRInfo::regT0, materializationArguments + propertyIndex);
        }

        static_assert(FunctionTraits<decltype(operationPopulateObjectInOSR)>::arity < GPRInfo::numberOfArgumentRegisters, "This call assumes that we don't pass arguments on the stack.");
        jit.setupArguments<decltype(operationPopulateObjectInOSR)>(
            CCallHelpers::TrustedImmPtr(codeBlock->globalObjectFor(materialization->origin())),
            CCallHelpers::TrustedImmPtr(materialization),
            CCallHelpers::TrustedImmPtr(materializationToPointer.get(materialization)),
            CCallHelpers::TrustedImmPtr(materializationArguments));
        jit.prepareCallOperation(vm);
        jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationPopulateObjectInOSR)), GPRInfo::nonArgGPR0);
        jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    }

    // Save all state from wherever the exit data tells us it was, into the appropriate place in
    // the scratch buffer. This also does the reboxing.
    
    for (unsigned index = exit.m_descriptor->m_values.size(); index--;) {
        recoverValue(exit.m_descriptor->m_values[index]);
        jit.store64(GPRInfo::regT0, scratch + index);
    }
    
    // Henceforth we make it look like the exiting function was called through a register
    // preservation wrapper. This implies that FP must be nudged down by a certain amount. Then
    // we restore the various things according to either exit.m_descriptor->m_values or by copying from the
    // old frame, and finally we save the various callee-save registers into where the
    // restoration thunk would restore them from.
    
    // Before we start messing with the frame, we need to set aside any registers that the
    // FTL code was preserving.
    for (unsigned i = codeBlock->calleeSaveRegisters()->size(); i--;) {
        RegisterAtOffset entry = codeBlock->calleeSaveRegisters()->at(i);
        jit.load64(
            MacroAssembler::Address(MacroAssembler::framePointerRegister, entry.offset()),
            GPRInfo::regT0);
        jit.store64(GPRInfo::regT0, unwindScratch + i);
    }
    
    CodeBlock* baselineCodeBlock = jit.baselineCodeBlockFor(exit.m_codeOrigin);

    // First set up SP so that our data doesn't get clobbered by signals.
    unsigned conservativeStackDelta =
        (exit.m_descriptor->m_values.numberOfLocals() + baselineCodeBlock->calleeSaveSpaceAsVirtualRegisters()) * sizeof(Register) +
        maxFrameExtentForSlowPathCall;
    conservativeStackDelta = WTF::roundUpToMultipleOf(
        stackAlignmentBytes(), conservativeStackDelta);
    jit.addPtr(
        MacroAssembler::TrustedImm32(-conservativeStackDelta),
        MacroAssembler::framePointerRegister, MacroAssembler::stackPointerRegister);
    jit.checkStackPointerAlignment();

    RegisterSet allFTLCalleeSaves = RegisterSet::ftlCalleeSaveRegisters();
    const RegisterAtOffsetList* baselineCalleeSaves = baselineCodeBlock->calleeSaveRegisters();
    RegisterAtOffsetList* vmCalleeSaves = RegisterSet::vmCalleeSaveRegisterOffsets();
    RegisterSet vmCalleeSavesToSkip = RegisterSet::stackRegisters();
    if (exit.isExceptionHandler()) {
        jit.loadPtr(&vm.topEntryFrame, GPRInfo::regT1);
        jit.addPtr(CCallHelpers::TrustedImm32(EntryFrame::calleeSaveRegistersBufferOffset()), GPRInfo::regT1);
    }

    for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
        if (!allFTLCalleeSaves.get(reg)) {
            if (exit.isExceptionHandler())
                RELEASE_ASSERT(!vmCalleeSaves->find(reg));
            continue;
        }
        unsigned unwindIndex = codeBlock->calleeSaveRegisters()->indexOf(reg);
        const RegisterAtOffset* baselineRegisterOffset = baselineCalleeSaves->find(reg);
        RegisterAtOffset* vmCalleeSave = nullptr; 
        if (exit.isExceptionHandler())
            vmCalleeSave = vmCalleeSaves->find(reg);

        if (reg.isGPR()) {
            GPRReg regToLoad = baselineRegisterOffset ? GPRInfo::regT0 : reg.gpr();
            RELEASE_ASSERT(regToLoad != GPRInfo::regT1);

            if (unwindIndex == UINT_MAX) {
                // The FTL compilation didn't preserve this register. This means that it also
                // didn't use the register. So its value at the beginning of OSR exit should be
                // preserved by the thunk. Luckily, we saved all registers into the register
                // scratch buffer, so we can restore them from there.
                jit.load64(registerScratch + offsetOfReg(reg), regToLoad);
            } else {
                // The FTL compilation preserved the register. Its new value is therefore
                // irrelevant, but we can get the value that was preserved by using the unwind
                // data. We've already copied all unwind-able preserved registers into the unwind
                // scratch buffer, so we can get it from there.
                jit.load64(unwindScratch + unwindIndex, regToLoad);
            }

            if (baselineRegisterOffset)
                jit.store64(regToLoad, MacroAssembler::Address(MacroAssembler::framePointerRegister, baselineRegisterOffset->offset()));
            if (vmCalleeSave && !vmCalleeSavesToSkip.get(vmCalleeSave->reg()))
                jit.store64(regToLoad, MacroAssembler::Address(GPRInfo::regT1, vmCalleeSave->offset()));
        } else {
            FPRReg fpRegToLoad = baselineRegisterOffset ? FPRInfo::fpRegT0 : reg.fpr();

            if (unwindIndex == UINT_MAX)
                jit.loadDouble(MacroAssembler::TrustedImmPtr(registerScratch + offsetOfReg(reg)), fpRegToLoad);
            else
                jit.loadDouble(MacroAssembler::TrustedImmPtr(unwindScratch + unwindIndex), fpRegToLoad);

            if (baselineRegisterOffset)
                jit.storeDouble(fpRegToLoad, MacroAssembler::Address(MacroAssembler::framePointerRegister, baselineRegisterOffset->offset()));
            if (vmCalleeSave && !vmCalleeSavesToSkip.get(vmCalleeSave->reg()))
                jit.storeDouble(fpRegToLoad, MacroAssembler::Address(GPRInfo::regT1, vmCalleeSave->offset()));
        }
    }

    if (exit.isExceptionHandler()) {
        RegisterAtOffset* vmCalleeSave = vmCalleeSaves->find(GPRInfo::numberTagRegister);
        jit.store64(GPRInfo::numberTagRegister, MacroAssembler::Address(GPRInfo::regT1, vmCalleeSave->offset()));

        vmCalleeSave = vmCalleeSaves->find(GPRInfo::notCellMaskRegister);
        jit.store64(GPRInfo::notCellMaskRegister, MacroAssembler::Address(GPRInfo::regT1, vmCalleeSave->offset()));
    }

    size_t baselineVirtualRegistersForCalleeSaves = baselineCodeBlock->calleeSaveSpaceAsVirtualRegisters();

    if (exit.m_codeOrigin.inlineStackContainsActiveCheckpoint()) {
        JSValue* tmpScratch = reinterpret_cast<JSValue*>(scratch + exit.m_descriptor->m_values.tmpIndex(0));
        VM* vmPtr = &vm;
        jit.probe([=] (Probe::Context& context) {
            Vector<std::unique_ptr<CheckpointOSRExitSideState>> sideStates;
            sideStates.reserveInitialCapacity(exit.m_codeOrigin.inlineDepth());
            auto sideStateCommitter = makeScopeExit([&] {
                for (size_t i = sideStates.size(); i--;)
                    vmPtr->pushCheckpointOSRSideState(WTFMove(sideStates[i]));
            });

            auto addSideState = [&] (CallFrame* frame, BytecodeIndex index, size_t tmpOffset) {
                std::unique_ptr<CheckpointOSRExitSideState> sideState = WTF::makeUnique<CheckpointOSRExitSideState>(frame);

                sideState->bytecodeIndex = index;
                for (size_t i = 0; i < maxNumCheckpointTmps; ++i)
                    sideState->tmps[i] = tmpScratch[i + tmpOffset];

                sideStates.append(WTFMove(sideState));
            };

            const CodeOrigin* codeOrigin;
            CallFrame* callFrame = context.gpr<CallFrame*>(GPRInfo::callFrameRegister);
            for (codeOrigin = &exit.m_codeOrigin; codeOrigin && codeOrigin->inlineCallFrame(); codeOrigin = codeOrigin->inlineCallFrame()->getCallerSkippingTailCalls()) {
                BytecodeIndex callBytecodeIndex = codeOrigin->bytecodeIndex();
                if (!callBytecodeIndex.checkpoint())
                    continue;

                auto* inlineCallFrame = codeOrigin->inlineCallFrame();
                addSideState(reinterpret_cast<CallFrame*>(reinterpret_cast<char*>(callFrame) + inlineCallFrame->returnPCOffset() - sizeof(CPURegister)), callBytecodeIndex, inlineCallFrame->tmpOffset);
            }

            if (!codeOrigin)
                return;

            if (BytecodeIndex bytecodeIndex = codeOrigin->bytecodeIndex(); bytecodeIndex.checkpoint())
                addSideState(callFrame, bytecodeIndex, 0);
        });
    }

    // Now get state out of the scratch buffer and place it back into the stack. The values are
    // already reboxed so we just move them.
    for (unsigned index = exit.m_descriptor->m_values.size(); index--;) {
        Operand operand = exit.m_descriptor->m_values.operandForIndex(index);

        if (operand.isTmp())
            continue;

        if (operand.isLocal() && operand.toLocal() < static_cast<int>(baselineVirtualRegistersForCalleeSaves))
            continue;

        jit.load64(scratch + index, GPRInfo::regT0);
        jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand.virtualRegister()));
    }
    
    handleExitCounts(vm, jit, exit);
    reifyInlinedCallFrames(jit, exit);
    adjustAndJumpToTarget(vm, jit, exit);
    
    LinkBuffer patchBuffer(jit, codeBlock);
    exit.m_code = FINALIZE_CODE_IF(
        shouldDumpDisassembly() || Options::verboseOSR() || Options::verboseFTLOSRExit(),
        patchBuffer, OSRExitPtrTag,
        "FTL OSR exit #%u (D@%u, %s, %s) from %s, with operands = %s",
            exitID, exit.m_dfgNodeIndex, toCString(exit.m_codeOrigin).data(),
            exitKindToString(exit.m_kind), toCString(*codeBlock).data(),
            toCString(ignoringContext<DumpContext>(exit.m_descriptor->m_values)).data()
        );
}

extern "C" JIT_OPERATION void* operationCompileFTLOSRExit(CallFrame* callFrame, unsigned exitID)
{
    if (shouldDumpDisassembly() || Options::verboseOSR() || Options::verboseFTLOSRExit())
        dataLog("Compiling OSR exit with exitID = ", exitID, "\n");

    VM& vm = callFrame->deprecatedVM();

    if constexpr (validateDFGDoesGC) {
        // We're about to exit optimized code. So, there's no longer any optimized
        // code running that expects no GC.
        vm.heap.setDoesGCExpectation(true, DoesGCCheck::Special::FTLOSRExit);
    }

    if (vm.callFrameForCatch)
        RELEASE_ASSERT(vm.callFrameForCatch == callFrame);
    
    CodeBlock* codeBlock = callFrame->codeBlock();
    
    ASSERT(codeBlock);
    ASSERT(codeBlock->jitType() == JITType::FTLJIT);
    
    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(vm.heap);

    JITCode* jitCode = codeBlock->jitCode()->ftl();
    OSRExit& exit = jitCode->osrExit[exitID];
    
    if (shouldDumpDisassembly() || Options::verboseOSR() || Options::verboseFTLOSRExit()) {
        dataLog("    Owning block: ", pointerDump(codeBlock), "\n");
        dataLog("    Origin: ", exit.m_codeOrigin, "\n");
        if (exit.m_codeOriginForExitProfile != exit.m_codeOrigin)
            dataLog("    Origin for exit profile: ", exit.m_codeOriginForExitProfile, "\n");
        dataLog("    Current call site index: ", callFrame->callSiteIndex().bits(), "\n");
        dataLog("    Exit is exception handler: ", exit.isExceptionHandler(), "\n");
        dataLog("    Is unwind handler: ", exit.isGenericUnwindHandler(), "\n");
        dataLog("    Exit values: ", exit.m_descriptor->m_values, "\n");
        dataLog("    Value reps: ", listDump(exit.m_valueReps), "\n");
        if (!exit.m_descriptor->m_materializations.isEmpty()) {
            dataLog("    Materializations:\n");
            for (ExitTimeObjectMaterialization* materialization : exit.m_descriptor->m_materializations)
                dataLog("        ", pointerDump(materialization), "\n");
        }
    }

    compileStub(vm, exitID, jitCode, exit, codeBlock);

    MacroAssembler::repatchJump(
        exit.codeLocationForRepatch(codeBlock), CodeLocationLabel<OSRExitPtrTag>(exit.m_code.code()));
    
    return exit.m_code.code().executableAddress();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

