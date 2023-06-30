/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "WasmOperations.h"

#if ENABLE(WEBASSEMBLY)

#include "ButterflyInlines.h"
#include "FrameTracers.h"
#include "IteratorOperations.h"
#include "JITExceptions.h"
#include "JSArrayBufferViewInlines.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyRuntimeError.h"
#include "JSWebAssemblyStruct.h"
#include "ProbeContext.h"
#include "ReleaseHeapAccessScope.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmInstance.h"
#include "WasmLLIntGenerator.h"
#include "WasmMemory.h"
#include "WasmModuleInformation.h"
#include "WasmOMGPlan.h"
#include "WasmOSREntryData.h"
#include "WasmOSREntryPlan.h"
#include "WasmOperationsInlines.h"
#include "WasmWorklist.h"
#include <bit>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

IGNORE_WARNINGS_BEGIN("frame-address")

namespace JSC { namespace Wasm {

namespace WasmOperationsInternal {
    static constexpr bool verbose = false;
}

#if ENABLE(WEBASSEMBLY_B3JIT)
static bool shouldTriggerOMGCompile(TierUpCount& tierUp, OMGCallee* replacement, uint32_t functionIndex)
{
    if (!replacement && !tierUp.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "delayOMGCompile counter = ", tierUp, " for ", functionIndex);
        dataLogLnIf(Options::verboseOSR(), "Choosing not to OMG-optimize ", functionIndex, " yet.");
        return false;
    }
    return true;
}

static void triggerOMGReplacementCompile(TierUpCount& tierUp, OMGCallee* replacement, Instance* instance, Wasm::CalleeGroup& calleeGroup, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers)
{
    if (replacement) {
        tierUp.optimizeSoon(functionIndex);
        return;
    }

    bool compile = false;
    {
        Locker locker { tierUp.getLock() };
        switch (tierUp.m_compilationStatusForOMG) {
        case TierUpCount::CompilationStatus::StartCompilation:
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
            return;
        case TierUpCount::CompilationStatus::NotCompiled:
            compile = true;
            tierUp.m_compilationStatusForOMG = TierUpCount::CompilationStatus::StartCompilation;
            break;
        default:
            break;
        }
    }

    if (compile) {
        dataLogLnIf(Options::verboseOSR(), "triggerOMGReplacement for ", functionIndex);
        // We need to compile the code.
        Ref<Plan> plan = adoptRef(*new OMGPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), functionIndex, hasExceptionHandlers, calleeGroup.mode(), Plan::dontFinalize()));
        ensureWorklist().enqueue(plan.copyRef());
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
    }
}

void loadValuesIntoBuffer(Probe::Context& context, const StackMap& values, uint64_t* buffer, SavedFPWidth savedFPWidth)
{
    ASSERT(Options::useWebAssemblySIMD() || savedFPWidth == SavedFPWidth::DontSaveVectors);
    unsigned valueSize = (savedFPWidth == SavedFPWidth::SaveVectors) ? 2 : 1;

    dataLogLnIf(WasmOperationsInternal::verbose, "loadValuesIntoBuffer: valueSize = ", valueSize, "; values.size() = ", values.size());
    for (unsigned index = 0; index < values.size(); ++index) {
        const OSREntryValue& value = values[index];
        dataLogLnIf(Options::verboseOSR() || WasmOperationsInternal::verbose, "OMG OSR entry values[", index, "] ", value.type(), " ", value);
        if (value.isGPR()) {
            switch (value.type().kind()) {
            case B3::Float:
            case B3::Double:
                RELEASE_ASSERT_NOT_REACHED();
            default:
                *bitwise_cast<uint64_t*>(buffer + index * valueSize) = context.gpr(value.gpr());
            }
            dataLogLnIf(WasmOperationsInternal::verbose, "GPR for value ", index, " ", value.gpr(), " = ", context.gpr(value.gpr()));
        } else if (value.isFPR()) {
            switch (value.type().kind()) {
            case B3::Float:
            case B3::Double:
                dataLogLnIf(WasmOperationsInternal::verbose, "FPR for value ", index, " ", value.fpr(), " = ", context.fpr(value.fpr(), savedFPWidth));
                *bitwise_cast<double*>(buffer + index * valueSize) = context.fpr(value.fpr(), savedFPWidth);
                break;
            case B3::V128:
                RELEASE_ASSERT(valueSize == 2);
#if CPU(X86_64) || CPU(ARM64)
                dataLogLnIf(WasmOperationsInternal::verbose, "Vector FPR for value ", index, " ", value.fpr(), " = ", context.vector(value.fpr()));
                *bitwise_cast<v128_t*>(buffer + index * valueSize) = context.vector(value.fpr());
#else
                UNREACHABLE_FOR_PLATFORM();
#endif
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else if (value.isConstant()) {
            switch (value.type().kind()) {
            case B3::Float:
                *bitwise_cast<float*>(buffer + index * valueSize) = value.floatValue();
                break;
            case B3::Double:
                *bitwise_cast<double*>(buffer + index * valueSize) = value.doubleValue();
                break;
            case B3::V128:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            default:
                *bitwise_cast<uint64_t*>(buffer + index * valueSize) = value.value();
            }
        } else if (value.isStack()) {
            auto* baseLoad = bitwise_cast<uint8_t*>(context.fp()) + value.offsetFromFP();
            auto* baseStore = bitwise_cast<uint8_t*>(buffer + index * valueSize);

            if (WasmOperationsInternal::verbose && (value.type().isFloat() || value.type().isVector())) {
                dataLogLn("Stack float or vector for value ", index, " fp offset ", value.offsetFromFP(), " = ",
                    *bitwise_cast<v128_t*>(baseLoad),
                    " or double ", *bitwise_cast<double*>(baseLoad));
            }

            switch (value.type().kind()) {
            case B3::Float:
                *bitwise_cast<float*>(baseStore) = *bitwise_cast<float*>(baseLoad);
                break;
            case B3::Double:
                *bitwise_cast<double*>(baseStore) = *bitwise_cast<double*>(baseLoad);
                break;
            case B3::V128:
                *bitwise_cast<v128_t*>(baseStore) = *bitwise_cast<v128_t*>(baseLoad);
                break;
            default:
                *bitwise_cast<uint64_t*>(baseStore) = *bitwise_cast<uint64_t*>(baseLoad);
                break;
            }
        } else
            RELEASE_ASSERT_NOT_REACHED();
    }
}

SUPPRESS_ASAN
static void doOSREntry(Instance* instance, Probe::Context& context, BBQCallee& callee, OSREntryCallee& osrEntryCallee, OSREntryData& osrEntryData)
{
    auto returnWithoutOSREntry = [&] {
        context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = 0;
    };

    unsigned valueSize = (callee.savedFPWidth() == SavedFPWidth::SaveVectors) ? 2 : 1;
    RELEASE_ASSERT(osrEntryCallee.osrEntryScratchBufferSize() == valueSize * osrEntryData.values().size());

    uint64_t* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryCallee.osrEntryScratchBufferSize());
    if (!buffer)
        return returnWithoutOSREntry();

    dataLogLnIf(Options::verboseOSR(), osrEntryData.functionIndex(), ":OMG OSR entry: got entry callee ", RawPointer(&osrEntryCallee));

    // 1. Place required values in scratch buffer.
    loadValuesIntoBuffer(context, osrEntryData.values(), buffer, callee.savedFPWidth());

    // 2. Restore callee saves.
    auto dontRestoreRegisters = RegisterSetBuilder::stackRegisters();
    for (const RegisterAtOffset& entry : *callee.calleeSaveRegisters()) {
        if (dontRestoreRegisters.contains(entry.reg(), IgnoreVectors))
            continue;
        if (entry.reg().isGPR())
            context.gpr(entry.reg().gpr()) = *bitwise_cast<UCPURegister*>(bitwise_cast<uint8_t*>(context.fp()) + entry.offset());
        else
            context.fpr(entry.reg().fpr(), callee.savedFPWidth()) = *bitwise_cast<double*>(bitwise_cast<uint8_t*>(context.fp()) + entry.offset());
    }

    // 3. Function epilogue, like a tail-call.
    UCPURegister* framePointer = bitwise_cast<UCPURegister*>(context.fp());
#if CPU(X86_64)
    // move(framePointerRegister, stackPointerRegister);
    // pop(framePointerRegister);
    context.fp() = bitwise_cast<UCPURegister*>(*framePointer);
    context.sp() = framePointer + 1;
    static_assert(prologueStackPointerDelta() == sizeof(void*) * 1);
#elif CPU(ARM64E) || CPU(ARM64)
    // move(framePointerRegister, stackPointerRegister);
    // popPair(framePointerRegister, linkRegister);
    context.fp() = bitwise_cast<UCPURegister*>(*framePointer);
    context.gpr(ARM64Registers::lr) = bitwise_cast<UCPURegister>(*(framePointer + 1));
    context.sp() = framePointer + 2;
    static_assert(prologueStackPointerDelta() == sizeof(void*) * 2);
#if CPU(ARM64E)
    // LR needs to be untagged since OSR entry function prologue will tag it with SP. This is similar to tail-call.
    context.gpr(ARM64Registers::lr) = bitwise_cast<UCPURegister>(untagCodePtrWithStackPointerForJITCall(context.gpr<void*>(ARM64Registers::lr), context.sp()));
#endif
#elif CPU(RISCV64)
    // move(framePointerRegister, stackPointerRegister);
    // popPair(framePointerRegister, linkRegister);
    context.fp() = bitwise_cast<UCPURegister*>(*framePointer);
    context.gpr(RISCV64Registers::ra) = bitwise_cast<UCPURegister>(*(framePointer + 1));
    context.sp() = framePointer + 2;
    static_assert(prologueStackPointerDelta() == sizeof(void*) * 2);
#elif CPU(ARM)
    UNUSED_VARIABLE(framePointer);
    UNREACHABLE_FOR_PLATFORM(); // Should not try to tier up yet
#else
#error Unsupported architecture.
#endif
    // 4. Configure argument registers to jump to OSR entry from the caller of this runtime function.
    context.gpr(GPRInfo::argumentGPR0) = bitwise_cast<UCPURegister>(buffer); // Modify this only when we definitely tier up.
    context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = bitwise_cast<UCPURegister>(osrEntryCallee.entrypoint().taggedPtr<>());
}

inline bool shouldJIT(unsigned functionIndex)
{
    if (!Options::useOMGJIT() || !OMGPlan::ensureGlobalOMGAllowlist().containsWasmFunction(functionIndex))
        return false;
    if (!Options::wasmFunctionIndexRangeToCompile().isInRange(functionIndex))
        return false;
    return true;
}

JSC_DEFINE_JIT_OPERATION(operationWasmTriggerOSREntryNow, void, (Probe::Context& context))
{
    OSREntryData& osrEntryData = *context.arg<OSREntryData*>();
    uint32_t functionIndex = osrEntryData.functionIndex();
    uint32_t loopIndex = osrEntryData.loopIndex();
    Instance* instance = context.gpr<Instance*>(GPRInfo::wasmContextInstancePointer);

    auto returnWithoutOSREntry = [&] {
        context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = 0;
    };

    Wasm::CalleeGroup& calleeGroup = *instance->calleeGroup();
    ASSERT(instance->memory()->mode() == calleeGroup.mode());

    uint32_t functionIndexInSpace = functionIndex + calleeGroup.functionImportCount();
    ASSERT(calleeGroup.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace).compilationMode() == Wasm::CompilationMode::BBQMode);
    BBQCallee& callee = static_cast<BBQCallee&>(calleeGroup.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace));
    TierUpCount& tierUp = *callee.tierUpCount();

    if (!shouldJIT(functionIndex)) {
        tierUp.deferIndefinitely();
        return returnWithoutOSREntry();
    }

    dataLogLnIf(Options::verboseOSR(), "Consider OSREntryPlan for [", functionIndex, "] loopIndex#", loopIndex, " with executeCounter = ", tierUp, " ", RawPointer(callee.replacement()));

    if (!Options::useWebAssemblyOSR()) {
        if (shouldTriggerOMGCompile(tierUp, callee.replacement(), functionIndex))
            triggerOMGReplacementCompile(tierUp, callee.replacement(), instance, calleeGroup, functionIndex, callee.hasExceptionHandlers());

        // We already have an OMG replacement.
        if (callee.replacement()) {
            // No OSR entry points. Just defer indefinitely.
            if (tierUp.osrEntryTriggers().isEmpty()) {
                tierUp.dontOptimizeAnytimeSoon(functionIndex);
                return;
            }

            // Found one OSR entry point. Since we do not have a way to jettison Wasm::Callee right now, this means that tierUp function is now meaningless.
            // Not call it as much as possible.
            if (callee.osrEntryCallee()) {
                tierUp.dontOptimizeAnytimeSoon(functionIndex);
                return;
            }
        }
        return returnWithoutOSREntry();
    }

    TierUpCount::CompilationStatus compilationStatus = TierUpCount::CompilationStatus::NotCompiled;
    {
        Locker locker { tierUp.getLock() };
        compilationStatus = tierUp.m_compilationStatusForOMGForOSREntry;
    }

    bool triggeredSlowPathToStartCompilation = false;
    switch (tierUp.osrEntryTriggers()[loopIndex]) {
    case TierUpCount::TriggerReason::DontTrigger:
        // The trigger isn't set, we entered because the counter reached its
        // threshold.
        break;
    case TierUpCount::TriggerReason::CompilationDone:
        // The trigger was set because compilation completed. Don't unset it
        // so that further BBQ executions OSR enter as well.
        break;
    case TierUpCount::TriggerReason::StartCompilation: {
        // We were asked to enter as soon as possible and start compiling an
        // entry for the current loopIndex. Unset this trigger so we
        // don't continually enter.
        Locker locker { tierUp.getLock() };
        TierUpCount::TriggerReason reason = tierUp.osrEntryTriggers()[loopIndex];
        if (reason == TierUpCount::TriggerReason::StartCompilation) {
            tierUp.osrEntryTriggers()[loopIndex] = TierUpCount::TriggerReason::DontTrigger;
            triggeredSlowPathToStartCompilation = true;
        }
        break;
    }
    }

    if (compilationStatus == TierUpCount::CompilationStatus::StartCompilation) {
        dataLogLnIf(Options::verboseOSR(), "delayOMGCompile still compiling for ", functionIndex);
        tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
        return returnWithoutOSREntry();
    }

    if (OSREntryCallee* osrEntryCallee = callee.osrEntryCallee()) {
        if (osrEntryCallee->loopIndex() == loopIndex)
            return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);
    }

    if (!shouldTriggerOMGCompile(tierUp, callee.replacement(), functionIndex) && !triggeredSlowPathToStartCompilation)
        return returnWithoutOSREntry();

    if (!triggeredSlowPathToStartCompilation) {
        triggerOMGReplacementCompile(tierUp, callee.replacement(), instance, calleeGroup, functionIndex, callee.hasExceptionHandlers());

        if (!callee.replacement())
            return returnWithoutOSREntry();
    }

    if (OSREntryCallee* osrEntryCallee = callee.osrEntryCallee()) {
        if (osrEntryCallee->loopIndex() == loopIndex)
            return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);
        tierUp.dontOptimizeAnytimeSoon(functionIndex);
        return returnWithoutOSREntry();
    }

    // Instead of triggering OSR entry compilation in inner loop, try outer loop's trigger immediately effective (setting TriggerReason::StartCompilation) and
    // let outer loop attempt to compile.
    if (!triggeredSlowPathToStartCompilation) {
        // An inner loop didn't specifically ask for us to kick off a compilation. This means the counter
        // crossed its threshold. We either fall through and kick off a compile for originBytecodeIndex,
        // or we flag an outer loop to immediately try to compile itself. If there are outer loops,
        // we first try to make them compile themselves. But we will eventually fall back to compiling
        // a progressively inner loop if it takes too long for control to reach an outer loop.

        auto tryTriggerOuterLoopToCompile = [&] {
            // We start with the outermost loop and make our way inwards (hence why we iterate the vector in reverse).
            // Our policy is that we will trigger an outer loop to compile immediately when program control reaches it.
            // If program control is taking too long to reach that outer loop, we progressively move inwards, meaning,
            // we'll eventually trigger some loop that is executing to compile. We start with trying to compile outer
            // loops since we believe outer loop compilations reveal the best opportunities for optimizing code.
            uint32_t currentLoopIndex = tierUp.outerLoops()[loopIndex];
            Locker locker { tierUp.getLock() };

            // We already started OSREntryPlan.
            if (callee.didStartCompilingOSREntryCallee())
                return false;

            while (currentLoopIndex != UINT32_MAX) {
                if (tierUp.osrEntryTriggers()[currentLoopIndex] == TierUpCount::TriggerReason::StartCompilation) {
                    // This means that we already asked this loop to compile. If we've reached here, it
                    // means program control has not yet reached that loop. So it's taking too long to compile.
                    // So we move on to asking the inner loop of this loop to compile itself.
                    currentLoopIndex = tierUp.outerLoops()[currentLoopIndex];
                    continue;
                }

                // This is where we ask the outer to loop to immediately compile itself if program
                // control reaches it.
                dataLogLnIf(Options::verboseOSR(), "Inner-loop loopIndex#", loopIndex, " in ", functionIndex, " setting parent loop loopIndex#", currentLoopIndex, "'s trigger and backing off.");
                tierUp.osrEntryTriggers()[currentLoopIndex] = TierUpCount::TriggerReason::StartCompilation;
                return true;
            }
            return false;
        };

        if (tryTriggerOuterLoopToCompile()) {
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
            return returnWithoutOSREntry();
        }
    }

    bool startOSREntryCompilation = false;
    {
        Locker locker { tierUp.getLock() };
        if (tierUp.m_compilationStatusForOMGForOSREntry == TierUpCount::CompilationStatus::NotCompiled) {
            tierUp.m_compilationStatusForOMGForOSREntry = TierUpCount::CompilationStatus::StartCompilation;
            startOSREntryCompilation = true;
            // Currently, we do not have a way to jettison wasm code. This means that once we decide to compile OSR entry code for a particular loopIndex,
            // we cannot throw the compiled code so long as Wasm module is live. We immediately disable all the triggers.
            for (auto& trigger : tierUp.osrEntryTriggers())
                trigger = TierUpCount::TriggerReason::DontTrigger;
        }
    }

    if (startOSREntryCompilation) {
        dataLogLnIf(Options::verboseOSR(), "triggerOMGOSR for ", functionIndex);
        Ref<Plan> plan = adoptRef(*new OSREntryPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), Ref<Wasm::BBQCallee>(callee), functionIndex, callee.hasExceptionHandlers(), loopIndex, calleeGroup.mode(), Plan::dontFinalize()));
        ensureWorklist().enqueue(plan.copyRef());
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
    }

    OSREntryCallee* osrEntryCallee = callee.osrEntryCallee();
    if (!osrEntryCallee) {
        tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
        return returnWithoutOSREntry();
    }

    if (osrEntryCallee->loopIndex() == loopIndex)
        return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);

    tierUp.dontOptimizeAnytimeSoon(functionIndex);
    return returnWithoutOSREntry();
}

JSC_DEFINE_JIT_OPERATION(operationWasmLoopOSREnterBBQJIT, void, (Probe::Context& context))
{
    TierUpCount& tierUp = *context.arg<TierUpCount*>();
    uint64_t* osrEntryScratchBuffer = bitwise_cast<uint64_t*>(context.gpr(GPRInfo::argumentGPR0));
    unsigned loopIndex = osrEntryScratchBuffer[0]; // First entry in scratch buffer is the loop index when tiering up to BBQ.

    // We just populated the callee in the frame before we entered this operation, so let's use it.
    CalleeBits calleeBits = *bitwise_cast<CalleeBits*>(bitwise_cast<uint8_t*>(context.fp()) + (unsigned)CallFrameSlot::callee * sizeof(Register));
    BBQCallee& callee = *bitwise_cast<BBQCallee*>(calleeBits.asWasmCallee());

    OSREntryData& entryData = tierUp.osrEntryData(loopIndex);
    RELEASE_ASSERT(entryData.loopIndex() == loopIndex);
    
    const StackMap& stackMap = entryData.values();
    auto writeValueToRep = [&](uint64_t encodedValue, const OSREntryValue& value) {
        B3::Type type = value.type();
        if (value.isGPR()) {
            ASSERT(!type.isFloat() && !type.isVector());
            context.gpr(value.gpr()) = encodedValue;
        } else if (value.isFPR()) {
            ASSERT(type.isFloat()); // We don't expect vectors from LLInt right now.
            context.fpr(value.fpr()) = encodedValue;
        } else if (value.isStack()) {
            auto* baseStore = bitwise_cast<uint8_t*>(context.fp()) + value.offsetFromFP();
            switch (type.kind()) {
            case B3::Int32:
                *bitwise_cast<uint32_t*>(baseStore) = static_cast<uint32_t>(encodedValue);
                break;
            case B3::Int64:
                *bitwise_cast<uint64_t*>(baseStore) = encodedValue;
                break;
            case B3::Float:
                *bitwise_cast<float*>(baseStore) = bitwise_cast<float>(static_cast<uint32_t>(encodedValue));
                break;
            case B3::Double:
                *bitwise_cast<double*>(baseStore) = bitwise_cast<double>(encodedValue);
                break;
            case B3::V128:
                RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("We shouldn't be receiving v128 values when tiering up from LLInt into BBQ.");
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
    };

    unsigned indexInScratchBuffer = BBQCallee::extraOSRValuesForLoopIndex;
    for (const auto& entry : stackMap)
        writeValueToRep(osrEntryScratchBuffer[indexInScratchBuffer ++], entry);

    context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = bitwise_cast<UCPURegister>(callee.loopEntrypoints()[loopIndex].taggedPtr());
}

JSC_DEFINE_JIT_OPERATION(operationWasmTriggerTierUpNow, void, (Instance* instance, uint32_t functionIndex))
{
    Wasm::CalleeGroup& calleeGroup = *instance->calleeGroup();
    ASSERT(instance->memory()->mode() == calleeGroup.mode());

    uint32_t functionIndexInSpace = functionIndex + calleeGroup.functionImportCount();
    ASSERT(calleeGroup.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace).compilationMode() == Wasm::CompilationMode::BBQMode);
    BBQCallee& callee = static_cast<BBQCallee&>(calleeGroup.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace));
    TierUpCount& tierUp = *callee.tierUpCount();

    if (!shouldJIT(functionIndex)) {
        tierUp.deferIndefinitely();
        return;
    }

    dataLogLnIf(Options::verboseOSR(), "Consider OMGPlan for [", functionIndex, "] with executeCounter = ", tierUp, " ", RawPointer(callee.replacement()));

    if (shouldTriggerOMGCompile(tierUp, callee.replacement(), functionIndex))
        triggerOMGReplacementCompile(tierUp, callee.replacement(), instance, calleeGroup, functionIndex, callee.hasExceptionHandlers());

    // We already have an OMG replacement.
    if (callee.replacement()) {
        // No OSR entry points. Just defer indefinitely.
        if (tierUp.osrEntryTriggers().isEmpty()) {
            dataLogLnIf(Options::verboseOSR(), "delayOMGCompile replacement in place, delaying indefinitely for ", functionIndex);
            tierUp.dontOptimizeAnytimeSoon(functionIndex);
            return;
        }

        // Found one OSR entry point. Since we do not have a way to jettison Wasm::Callee right now, this means that tierUp function is now meaningless.
        // Not call it as much as possible.
        if (callee.osrEntryCallee()) {
            dataLogLnIf(Options::verboseOSR(), "delayOMGCompile trigger in place, delaying indefinitely for ", functionIndex);
            tierUp.dontOptimizeAnytimeSoon(functionIndex);
            return;
        }
    }
}
#endif

JSC_DEFINE_JIT_OPERATION(operationWasmUnwind, void*, (Instance* instance))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

JSC_DEFINE_JIT_OPERATION(operationConvertToI64, int64_t, (Instance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::decode(v).toBigInt64(globalObject);
}

JSC_DEFINE_JIT_OPERATION(operationConvertToF64, double, (Instance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::decode(v).toNumber(globalObject);
}

JSC_DEFINE_JIT_OPERATION(operationConvertToI32, int32_t, (Instance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::decode(v).toInt32(globalObject);
}

JSC_DEFINE_JIT_OPERATION(operationConvertToF32, float, (Instance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return static_cast<float>(JSValue::decode(v).toNumber(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToFuncref, EncodedJSValue, (Instance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(v);
    WebAssemblyFunction* wasmFunction = nullptr;
    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
    if (UNLIKELY(!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull())) {
        throwTypeError(globalObject, scope, "Funcref value is not a function"_s);
        return { };
    }
    return v;
}

JSC_DEFINE_JIT_OPERATION(operationConvertToBigInt, EncodedJSValue, (Instance* instance, EncodedWasmValue value))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(JSBigInt::makeHeapBigIntOrBigInt32(globalObject, value));
}

// https://webassembly.github.io/multi-value/js-api/index.html#run-a-host-function
JSC_DEFINE_JIT_OPERATION(operationIterateResults, void, (Instance* instance, const TypeDefinition* type, EncodedJSValue encResult, uint64_t* registerResults, uint64_t* calleeFramePointer))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const FunctionSignature* signature = type->as<FunctionSignature>();

    auto wasmCallInfo = wasmCallingConvention().callInformationFor(*type, CallRole::Callee);
    RegisterAtOffsetList registerResultOffsets = wasmCallInfo.computeResultsOffsetList();

    unsigned iterationCount = 0;
    MarkedArgumentBuffer buffer;
    JSValue result = JSValue::decode(encResult);
    forEachInIterable(globalObject, result, [&] (VM&, JSGlobalObject*, JSValue value) -> void {
        if (buffer.size() < signature->returnCount()) {
            buffer.append(value);
            if (UNLIKELY(buffer.hasOverflowed()))
                throwOutOfMemoryError(globalObject, scope);
        }
        ++iterationCount;
    });
    RETURN_IF_EXCEPTION(scope, void());

    if (buffer.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope, "JS results to Wasm are too large"_s);
        return;
    }

    if (iterationCount != signature->returnCount()) {
        throwVMTypeError(globalObject, scope, "Incorrect number of values returned to Wasm from JS"_s);
        return;
    }

    for (unsigned index = 0; index < buffer.size(); ++index) {
        JSValue value = buffer.at(index);

        uint64_t unboxedValue = 0;
        const auto& returnType = signature->returnType(index);
        switch (returnType.kind) {
        case TypeKind::I32:
            unboxedValue = value.toInt32(globalObject);
            break;
        case TypeKind::I64:
            unboxedValue = value.toBigInt64(globalObject);
            break;
        case TypeKind::F32:
            unboxedValue = bitwise_cast<uint32_t>(value.toFloat(globalObject));
            break;
        case TypeKind::F64:
            unboxedValue = bitwise_cast<uint64_t>(value.toNumber(globalObject));
            break;
        default: {
            if (isExternref(returnType))
                ASSERT(returnType.isNullable());
            else if (isFuncref(returnType)) {
                ASSERT(returnType.isNullable());
                WebAssemblyFunction* wasmFunction = nullptr;
                WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                if (UNLIKELY(!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull())) {
                    throwTypeError(globalObject, scope, "Funcref value is not a function"_s);
                    return;
                }
            } else
                RELEASE_ASSERT_NOT_REACHED();
            unboxedValue = bitwise_cast<uint64_t>(value);
        }
        }
        RETURN_IF_EXCEPTION(scope, void());

        auto rep = wasmCallInfo.results[index];
        if (rep.location.isGPR())
            registerResults[registerResultOffsets.find(rep.location.jsr().payloadGPR())->offset() / sizeof(uint64_t)] = unboxedValue;
        else if (rep.location.isFPR())
            registerResults[registerResultOffsets.find(rep.location.fpr())->offset() / sizeof(uint64_t)] = unboxedValue;
        else
            calleeFramePointer[rep.location.offsetFromFP() / sizeof(uint64_t)] = unboxedValue;
    }
}

// FIXME: It would be much easier to inline this when we have a global GC, which could probably mean we could avoid
// spilling the results onto the stack.
// Saved result registers should be placed on the stack just above the last stack result.
JSC_DEFINE_JIT_OPERATION(operationAllocateResultsArray, JSArray*, (Wasm::Instance* instance, const TypeDefinition* type, IndexingType indexingType, JSValue* stackPointerFromCallee))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);

    ObjectInitializationScope initializationScope(vm);
    const FunctionSignature* signature = type->as<FunctionSignature>();
    JSArray* result = JSArray::tryCreateUninitializedRestricted(initializationScope, nullptr, globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType), signature->returnCount());

    // FIXME: Handle allocation failure...
    RELEASE_ASSERT(result);

    auto wasmCallInfo = wasmCallingConvention().callInformationFor(*type);
    RegisterAtOffsetList registerResults = wasmCallInfo.computeResultsOffsetList();

    for (unsigned i = 0; i < signature->returnCount(); ++i) {
        ValueLocation loc = wasmCallInfo.results[i].location;
        JSValue value;
        if (loc.isGPR()) {
#if USE(JSVALE32_64)
            ASSERT(registerResults.find(loc.jsr().payloadGPR())->offset() + 4 == registerResults.find(loc.jsr().tagGPR())->offset());
#endif
            value = stackPointerFromCallee[(registerResults.find(loc.jsr().payloadGPR())->offset() + wasmCallInfo.headerAndArgumentStackSizeInBytes) / sizeof(JSValue)];
        } else if (loc.isFPR())
            value = stackPointerFromCallee[(registerResults.find(loc.fpr())->offset() + wasmCallInfo.headerAndArgumentStackSizeInBytes) / sizeof(JSValue)];
        else
            value = stackPointerFromCallee[loc.offsetFromSP() / sizeof(JSValue)];
        result->initializeIndex(initializationScope, i, value);
    }

    return result;
}

JSC_DEFINE_JIT_OPERATION(operationWasmWriteBarrierSlowPath, void, (JSCell* cell, VM* vmPointer))
{
    ASSERT(cell);
    ASSERT(vmPointer);
    VM& vm = *vmPointer;
    vm.writeBarrierSlowPath(cell);
}

JSC_DEFINE_JIT_OPERATION(operationPopcount32, uint32_t, (int32_t value))
{
    return std::popcount(static_cast<uint32_t>(value));
}

JSC_DEFINE_JIT_OPERATION(operationPopcount64, uint64_t, (int64_t value))
{
    return std::popcount(static_cast<uint64_t>(value));
}

JSC_DEFINE_JIT_OPERATION(operationGrowMemory, int32_t, (Instance* instance, int32_t delta))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return growMemory(instance, delta);
}

JSC_DEFINE_JIT_OPERATION(operationWasmMemoryFill, UCPUStrictInt32, (Instance* instance, uint32_t dstAddress, uint32_t targetValue, uint32_t count))
{
    return toUCPUStrictInt32(memoryFill(instance, dstAddress, targetValue, count));
}

JSC_DEFINE_JIT_OPERATION(operationWasmMemoryCopy, UCPUStrictInt32, (Instance* instance, uint32_t dstAddress, uint32_t srcAddress, uint32_t count))
{
    return toUCPUStrictInt32(memoryCopy(instance, dstAddress, srcAddress, count));
}

JSC_DEFINE_JIT_OPERATION(operationGetWasmTableElement, EncodedJSValue, (Instance* instance, unsigned tableIndex, int32_t signedIndex))
{
    return tableGet(instance, tableIndex, signedIndex);
}

JSC_DEFINE_JIT_OPERATION(operationSetWasmTableElement, UCPUStrictInt32, (Instance* instance, unsigned tableIndex, uint32_t signedIndex, EncodedJSValue encValue))
{
    return toUCPUStrictInt32(tableSet(instance, tableIndex, signedIndex, encValue));
}

JSC_DEFINE_JIT_OPERATION(operationWasmTableInit, UCPUStrictInt32, (Instance* instance, unsigned elementIndex, unsigned tableIndex, uint32_t dstOffset, uint32_t srcOffset, uint32_t length))
{
    return toUCPUStrictInt32(tableInit(instance, elementIndex, tableIndex, dstOffset, srcOffset, length));
}

JSC_DEFINE_JIT_OPERATION(operationWasmElemDrop, void, (Instance* instance, unsigned elementIndex))
{
    return elemDrop(instance, elementIndex);
}

JSC_DEFINE_JIT_OPERATION(operationWasmTableGrow, int32_t, (Instance* instance, unsigned tableIndex, EncodedJSValue fill, uint32_t delta))
{
    return tableGrow(instance, tableIndex, fill, delta);
}

JSC_DEFINE_JIT_OPERATION(operationWasmTableFill, UCPUStrictInt32, (Instance* instance, unsigned tableIndex, uint32_t offset, EncodedJSValue fill, uint32_t count))
{
    return toUCPUStrictInt32(tableFill(instance, tableIndex, offset, fill, count));
}

JSC_DEFINE_JIT_OPERATION(operationWasmTableCopy, UCPUStrictInt32, (Instance* instance, unsigned dstTableIndex, unsigned srcTableIndex, int32_t dstOffset, int32_t srcOffset, int32_t length))
{
    return toUCPUStrictInt32(tableCopy(instance, dstTableIndex, srcTableIndex, dstOffset, srcOffset, length));
}

JSC_DEFINE_JIT_OPERATION(operationWasmRefFunc, EncodedJSValue, (Instance* instance, uint32_t index))
{
    return refFunc(instance, index);
}

JSC_DEFINE_JIT_OPERATION(operationWasmStructNew, EncodedJSValue, (Instance* instance, uint32_t typeIndex, bool useDefault, uint64_t* arguments))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return structNew(instance, typeIndex, useDefault, arguments);
}

JSC_DEFINE_JIT_OPERATION(operationWasmStructNewEmpty, EncodedJSValue, (Instance* instance, uint32_t typeIndex))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    JSWebAssemblyInstance* jsInstance = instance->owner();
    JSGlobalObject* globalObject = instance->globalObject();
    auto structRTT = instance->module().moduleInformation().rtts[typeIndex];
    return JSValue::encode(JSWebAssemblyStruct::tryCreate(globalObject, globalObject->webAssemblyStructStructure(), jsInstance, typeIndex, structRTT));
}

JSC_DEFINE_JIT_OPERATION(operationWasmStructGet, EncodedJSValue, (EncodedJSValue encodedStructReference, uint32_t fieldIndex))
{
    return structGet(encodedStructReference, fieldIndex);
}

JSC_DEFINE_JIT_OPERATION(operationWasmStructSet, void, (Instance* instance, EncodedJSValue encodedStructReference, uint32_t fieldIndex, EncodedJSValue argument))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return structSet(instance, encodedStructReference, fieldIndex, argument);
}

JSC_DEFINE_JIT_OPERATION(operationGetWasmTableSize, int32_t, (Instance* instance, unsigned tableIndex))
{
    return tableSize(instance, tableIndex);
}

JSC_DEFINE_JIT_OPERATION(operationMemoryAtomicWait32, int32_t, (Instance* instance, unsigned base, unsigned offset, int32_t value, int64_t timeoutInNanoseconds))
{
    return memoryAtomicWait32(instance, base, offset, value, timeoutInNanoseconds);
}

JSC_DEFINE_JIT_OPERATION(operationMemoryAtomicWait64, int32_t, (Instance* instance, unsigned base, unsigned offset, int64_t value, int64_t timeoutInNanoseconds))
{
    return memoryAtomicWait64(instance, base, offset, value, timeoutInNanoseconds);
}

JSC_DEFINE_JIT_OPERATION(operationMemoryAtomicNotify, int32_t, (Instance* instance, unsigned base, unsigned offset, int32_t countValue))
{
    return memoryAtomicNotify(instance, base, offset, countValue);
}

JSC_DEFINE_JIT_OPERATION(operationWasmMemoryInit, UCPUStrictInt32, (Instance* instance, unsigned dataSegmentIndex, uint32_t dstAddress, uint32_t srcAddress, uint32_t length))
{
    return toUCPUStrictInt32(memoryInit(instance, dataSegmentIndex, dstAddress, srcAddress, length));
}

JSC_DEFINE_JIT_OPERATION(operationWasmDataDrop, void, (Instance* instance, unsigned dataSegmentIndex))
{
    return dataDrop(instance, dataSegmentIndex);
}

JSC_DEFINE_JIT_OPERATION(operationWasmThrow, void*, (Instance* instance, unsigned exceptionIndex, uint64_t* arguments))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSGlobalObject* globalObject = instance->globalObject();

    const Wasm::Tag& tag = instance->tag(exceptionIndex);

    FixedVector<uint64_t> values(tag.parameterCount());
    for (unsigned i = 0; i < tag.parameterCount(); ++i)
        values[i] = arguments[i];

    ASSERT(tag.type().returnsVoid());
    if (tag.type().numVectors()) {
        // Note: the spec is still in flux on what to do here, so we conservatively just disallow throwing any vectors.
        throwException(globalObject, throwScope, createTypeError(globalObject, errorMessageForExceptionType(Wasm::ExceptionType::TypeErrorInvalidV128Use)));
    } else {
        JSWebAssemblyException* exception = JSWebAssemblyException::create(vm, globalObject->webAssemblyExceptionStructure(), tag, WTFMove(values));
        throwException(globalObject, throwScope, exception);
    }

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

JSC_DEFINE_JIT_OPERATION(operationWasmRethrow, void*, (Instance* instance, EncodedJSValue thrownValue))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSGlobalObject* globalObject = instance->globalObject();

    throwException(globalObject, throwScope, JSValue::decode(thrownValue));

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

JSC_DEFINE_JIT_OPERATION(operationWasmToJSException, void*, (Instance* instance, Wasm::ExceptionType type))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return throwWasmToJSException(callFrame, type, instance);
}

#if USE(JSVALUE64)
JSC_DEFINE_JIT_OPERATION(operationWasmRetrieveAndClearExceptionIfCatchable, ThrownExceptionInfo, (Instance* instance))
{
    VM& vm = instance->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(!!throwScope.exception());

    vm.callFrameForCatch = nullptr;
    auto* jumpTarget = std::exchange(vm.targetMachinePCAfterCatch, nullptr);

    Exception* exception = throwScope.exception();
    JSValue thrownValue = exception->value();

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    throwScope.clearException();

    return { JSValue::encode(thrownValue), jumpTarget };
}
#else
static ThrownExceptionInfo retrieveAndClearExceptionIfCatchableNonSharedImpl(Instance* instance)
{
    VM& vm = instance->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(!!throwScope.exception());

    vm.callFrameForCatch = nullptr;
    vm.targetMachinePCForThrow = nullptr;

    Exception* exception = throwScope.exception();
    JSValue thrownValue = exception->value();

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    throwScope.clearException();

    void* payload = nullptr;
    if (JSWebAssemblyException* wasmException = jsDynamicCast<JSWebAssemblyException*>(thrownValue))
        payload = bitwise_cast<void*>(wasmException->payload().data());

    return { JSValue::encode(thrownValue), payload };
}

JSC_DEFINE_JIT_OPERATION(operationWasmRetrieveAndClearExceptionIfCatchable32, void*, (Instance* instance, EncodedJSValue* encodedThrownValue))
{
    auto info = retrieveAndClearExceptionIfCatchableNonSharedImpl(instance);
    *encodedThrownValue = info.thrownValue;
    return info.payload;
}
#endif // USE(JSVALUE64)

JSC_DEFINE_JIT_OPERATION(operationWasmArrayNew, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t size, EncodedJSValue encValue))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return arrayNew(instance, typeIndex, size, encValue);
}

JSC_DEFINE_JIT_OPERATION(operationWasmArrayNewData, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t dataSegmentIndex, uint32_t arraySize, uint32_t offset))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return arrayNewData(instance, typeIndex, dataSegmentIndex, arraySize, offset);
}

JSC_DEFINE_JIT_OPERATION(operationWasmArrayNewElem, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t elemSegmentIndex, uint32_t arraySize, uint32_t offset))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);

    return arrayNewElem(instance, typeIndex, elemSegmentIndex, arraySize, offset);
}

JSC_DEFINE_JIT_OPERATION(operationWasmArrayNewEmpty, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t size))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);

    JSGlobalObject* globalObject = instance->globalObject();
    auto arrayRTT = instance->module().moduleInformation().rtts[typeIndex];

    // Get the element type
    ASSERT(typeIndex < instance->module().moduleInformation().typeCount());
    const Wasm::TypeDefinition& arraySignature = instance->module().moduleInformation().typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::FieldType fieldType = arraySignature.as<ArrayType>()->elementType();

    // Create a zero-initialized array with the right element type and length
    JSWebAssemblyArray* array = nullptr;
    switch (fieldType.type.elementSize()) {
    case sizeof(uint8_t): {
        FixedVector<uint8_t> v(size);
        v.fill(0); // Prevent GC from tracing uninitialized array slots
        array = JSWebAssemblyArray::create(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
        break;
    }
    case sizeof(uint16_t): {
        FixedVector<uint16_t> v(size);
        v.fill(0);
        array = JSWebAssemblyArray::create(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
        break;
    }
    case sizeof(uint32_t): {
        FixedVector<uint32_t> v(size);
        v.fill(0);
        array = JSWebAssemblyArray::create(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
        break;
    }
    case sizeof(uint64_t): {
        FixedVector<uint64_t> v(size);
        v.fill(0);
        array = JSWebAssemblyArray::create(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return JSValue::encode(array);
}

JSC_DEFINE_JIT_OPERATION(operationWasmArrayGet, EncodedJSValue, (Instance* instance, uint32_t typeIndex, EncodedJSValue arrayValue, uint32_t index))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return arrayGet(instance, typeIndex, arrayValue, index);
}

JSC_DEFINE_JIT_OPERATION(operationWasmArraySet, void, (Instance* instance, uint32_t typeIndex, EncodedJSValue arrayValue, uint32_t index, EncodedJSValue value))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return arraySet(instance, typeIndex, arrayValue, index, value);
}

JSC_DEFINE_JIT_OPERATION(operationWasmIsSubRTT, bool, (RTT* maybeSubRTT, RTT* targetRTT))
{
    ASSERT(maybeSubRTT && targetRTT);
    return maybeSubRTT->isSubRTT(*targetRTT);
}

JSC_DEFINE_JIT_OPERATION(operationWasmExternInternalize, EncodedJSValue, (EncodedJSValue reference))
{
    return externInternalize(reference);
}

JSC_DEFINE_JIT_OPERATION(operationWasmRefTest, int32_t, (Instance* instance, EncodedJSValue reference, uint32_t allowNull, int32_t heapType))
{
    Wasm::TypeIndex typeIndex;
    if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(heapType)))
        typeIndex = static_cast<Wasm::TypeIndex>(heapType);
    else
        typeIndex = instance->module().moduleInformation().typeSignatures[heapType]->index();
    // Explicitly return 1 or 0 because bool in C++ only reqiures that the bottom bit match the other bits can be anything.
    return Wasm::refCast(reference, static_cast<bool>(allowNull), typeIndex) ? 1 : 0;
}

JSC_DEFINE_JIT_OPERATION(operationWasmRefCast, EncodedJSValue, (Instance* instance, EncodedJSValue reference, uint32_t allowNull, int32_t heapType))
{
    Wasm::TypeIndex typeIndex;
    if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(heapType)))
        typeIndex = static_cast<Wasm::TypeIndex>(heapType);
    else
        typeIndex = instance->module().moduleInformation().typeSignatures[heapType]->index();
    if (!Wasm::refCast(reference, static_cast<bool>(allowNull), typeIndex))
        return encodedJSValue();
    return reference;
}

} } // namespace JSC::Wasm

IGNORE_WARNINGS_END

#endif // ENABLE(WEBASSEMBLY)
