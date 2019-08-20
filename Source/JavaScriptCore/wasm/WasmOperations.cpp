/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ProbeContext.h"
#include "WasmCallee.h"
#include "WasmContextInlines.h"
#include "WasmInstance.h"
#include "WasmMemory.h"
#include "WasmNameSection.h"
#include "WasmOMGForOSREntryPlan.h"
#include "WasmOMGPlan.h"
#include "WasmOSREntryData.h"
#include "WasmSignatureInlines.h"
#include "WasmWorklist.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

static bool shouldTriggerOMGCompile(TierUpCount& tierUp, OMGCallee* replacement, uint32_t functionIndex)
{
    if (!replacement && !tierUp.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "delayOMGCompile counter = ", tierUp, " for ", functionIndex);
        dataLogLnIf(Options::verboseOSR(), "Choosing not to OMG-optimize ", functionIndex, " yet.");
        return false;
    }
    return true;
}

static void triggerOMGReplacementCompile(TierUpCount& tierUp, OMGCallee* replacement, Instance* instance, Wasm::CodeBlock& codeBlock, uint32_t functionIndex)
{
    if (replacement) {
        tierUp.optimizeSoon(functionIndex);
        return;
    }

    bool compile = false;
    {
        auto locker = holdLock(tierUp.getLock());
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
        Ref<Plan> plan = adoptRef(*new OMGPlan(instance->context(), Ref<Wasm::Module>(instance->module()), functionIndex, codeBlock.mode(), Plan::dontFinalize()));
        ensureWorklist().enqueue(plan.copyRef());
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
    }
}

SUPPRESS_ASAN
static void doOSREntry(Instance* instance, Probe::Context& context, BBQCallee& callee, OMGForOSREntryCallee& osrEntryCallee, OSREntryData& osrEntryData)
{
    auto returnWithoutOSREntry = [&] {
        context.gpr(GPRInfo::argumentGPR0) = 0;
    };

    uint64_t* buffer = instance->context()->scratchBufferForSize(osrEntryCallee.osrEntryScratchBufferSize());
    if (!buffer)
        return returnWithoutOSREntry();

    dataLogLnIf(Options::verboseOSR(), osrEntryData.functionIndex(), ":OMG OSR entry: got entry callee ", RawPointer(&osrEntryCallee));

    // 1. Place required values in scratch buffer.
    for (unsigned index = 0; index < osrEntryData.values().size(); ++index) {
        const OSREntryValue& value = osrEntryData.values()[index];
        dataLogLnIf(Options::verboseOSR(), "OMG OSR entry values[", index, "] ", value.type(), " ", value);
        if (value.isGPR()) {
            switch (value.type().kind()) {
            case B3::Float:
            case B3::Double:
                RELEASE_ASSERT_NOT_REACHED();
            default:
                *bitwise_cast<uint64_t*>(buffer + index) = context.gpr(value.gpr());
            }
        } else if (value.isFPR()) {
            switch (value.type().kind()) {
            case B3::Float:
            case B3::Double:
                *bitwise_cast<double*>(buffer + index) = context.fpr(value.fpr());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else if (value.isConstant()) {
            switch (value.type().kind()) {
            case B3::Float:
                *bitwise_cast<float*>(buffer + index) = value.floatValue();
                break;
            case B3::Double:
                *bitwise_cast<double*>(buffer + index) = value.doubleValue();
                break;
            default:
                *bitwise_cast<uint64_t*>(buffer + index) = value.value();
            }
        } else if (value.isStack()) {
            switch (value.type().kind()) {
            case B3::Float:
                *bitwise_cast<float*>(buffer + index) = *bitwise_cast<float*>(bitwise_cast<uint8_t*>(context.fp()) + value.offsetFromFP());
                break;
            case B3::Double:
                *bitwise_cast<double*>(buffer + index) = *bitwise_cast<double*>(bitwise_cast<uint8_t*>(context.fp()) + value.offsetFromFP());
                break;
            default:
                *bitwise_cast<uint64_t*>(buffer + index) = *bitwise_cast<uint64_t*>(bitwise_cast<uint8_t*>(context.fp()) + value.offsetFromFP());
                break;
            }
        } else if (value.isStackArgument()) {
            switch (value.type().kind()) {
            case B3::Float:
                *bitwise_cast<float*>(buffer + index) = *bitwise_cast<float*>(bitwise_cast<uint8_t*>(context.sp()) + value.offsetFromSP());
                break;
            case B3::Double:
                *bitwise_cast<double*>(buffer + index) = *bitwise_cast<double*>(bitwise_cast<uint8_t*>(context.sp()) + value.offsetFromSP());
                break;
            default:
                *bitwise_cast<uint64_t*>(buffer + index) = *bitwise_cast<uint64_t*>(bitwise_cast<uint8_t*>(context.sp()) + value.offsetFromSP());
                break;
            }
        } else
            RELEASE_ASSERT_NOT_REACHED();
    }

    // 2. Restore callee saves.
    RegisterSet dontRestoreRegisters = RegisterSet::stackRegisters();
    for (const RegisterAtOffset& entry : *callee.calleeSaveRegisters()) {
        if (dontRestoreRegisters.get(entry.reg()))
            continue;
        if (entry.reg().isGPR())
            context.gpr(entry.reg().gpr()) = *bitwise_cast<UCPURegister*>(bitwise_cast<uint8_t*>(context.fp()) + entry.offset());
        else
            context.fpr(entry.reg().fpr()) = *bitwise_cast<double*>(bitwise_cast<uint8_t*>(context.fp()) + entry.offset());
    }

    // 3. Function epilogue, like a tail-call.
    UCPURegister* framePointer = bitwise_cast<UCPURegister*>(context.fp());
#if CPU(X86_64)
    // move(framePointerRegister, stackPointerRegister);
    // pop(framePointerRegister);
    context.fp() = bitwise_cast<UCPURegister*>(*framePointer);
    context.sp() = framePointer + 1;
    static_assert(AssemblyHelpers::prologueStackPointerDelta() == sizeof(void*) * 1);
#elif CPU(ARM64E) || CPU(ARM64)
    // move(framePointerRegister, stackPointerRegister);
    // popPair(framePointerRegister, linkRegister);
    context.fp() = bitwise_cast<UCPURegister*>(*framePointer);
    context.gpr(ARM64Registers::lr) = bitwise_cast<UCPURegister>(*(framePointer + 1));
    context.sp() = framePointer + 2;
    static_assert(AssemblyHelpers::prologueStackPointerDelta() == sizeof(void*) * 2);
#if CPU(ARM64E)
    // LR needs to be untagged since OSR entry function prologue will tag it with SP. This is similar to tail-call.
    context.gpr(ARM64Registers::lr) = bitwise_cast<UCPURegister>(untagCodePtr(context.gpr<void*>(ARM64Registers::lr), bitwise_cast<PtrTag>(context.sp())));
#endif
#else
#error Unsupported architecture.
#endif
    // 4. Configure argument registers to jump to OSR entry from the caller of this runtime function.
    context.gpr(GPRInfo::argumentGPR0) = bitwise_cast<UCPURegister>(buffer);
    context.gpr(GPRInfo::argumentGPR1) = bitwise_cast<UCPURegister>(osrEntryCallee.entrypoint().executableAddress<>());
}

void JIT_OPERATION triggerOSREntryNow(Probe::Context& context)
{
    OSREntryData& osrEntryData = *context.arg<OSREntryData*>();
    uint32_t functionIndex = osrEntryData.functionIndex();
    uint32_t loopIndex = osrEntryData.loopIndex();
    Instance* instance = Wasm::Context::tryLoadInstanceFromTLS();
    if (!instance)
        instance = context.gpr<Instance*>(Wasm::PinnedRegisterInfo::get().wasmContextInstancePointer);

    auto returnWithoutOSREntry = [&] {
        context.gpr(GPRInfo::argumentGPR0) = 0;
    };

    Wasm::CodeBlock& codeBlock = *instance->codeBlock();
    ASSERT(instance->memory()->mode() == codeBlock.mode());

    uint32_t functionIndexInSpace = functionIndex + codeBlock.functionImportCount();
    ASSERT(codeBlock.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace).compilationMode() == Wasm::CompilationMode::BBQMode);
    BBQCallee& callee = static_cast<BBQCallee&>(codeBlock.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace));
    TierUpCount& tierUp = *callee.tierUpCount();
    dataLogLnIf(Options::verboseOSR(), "Consider OMGForOSREntryPlan for [", functionIndex, "] loopIndex#", loopIndex, " with executeCounter = ", tierUp, " ", RawPointer(callee.replacement()));

    if (!Options::useWebAssemblyOSR()) {
        if (shouldTriggerOMGCompile(tierUp, callee.replacement(), functionIndex))
            triggerOMGReplacementCompile(tierUp, callee.replacement(), instance, codeBlock, functionIndex);

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
        auto locker = holdLock(tierUp.getLock());
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
        auto locker = holdLock(tierUp.getLock());
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

    if (OMGForOSREntryCallee* osrEntryCallee = callee.osrEntryCallee()) {
        if (osrEntryCallee->loopIndex() == loopIndex)
            return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);
    }

    if (!shouldTriggerOMGCompile(tierUp, callee.replacement(), functionIndex) && !triggeredSlowPathToStartCompilation)
        return returnWithoutOSREntry();

    if (!triggeredSlowPathToStartCompilation) {
        triggerOMGReplacementCompile(tierUp, callee.replacement(), instance, codeBlock, functionIndex);

        if (!callee.replacement())
            return returnWithoutOSREntry();
    }

    if (OMGForOSREntryCallee* osrEntryCallee = callee.osrEntryCallee()) {
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
            auto locker = holdLock(tierUp.getLock());

            // We already started OMGForOSREntryPlan.
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
        auto locker = holdLock(tierUp.getLock());
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
        Ref<Plan> plan = adoptRef(*new OMGForOSREntryPlan(instance->context(), Ref<Wasm::Module>(instance->module()), Ref<Wasm::BBQCallee>(callee), functionIndex, loopIndex, codeBlock.mode(), Plan::dontFinalize()));
        ensureWorklist().enqueue(plan.copyRef());
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
    }

    OMGForOSREntryCallee* osrEntryCallee = callee.osrEntryCallee();
    if (!osrEntryCallee) {
        tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
        return returnWithoutOSREntry();
    }

    if (osrEntryCallee->loopIndex() == loopIndex)
        return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);

    tierUp.dontOptimizeAnytimeSoon(functionIndex);
    return returnWithoutOSREntry();
}

void JIT_OPERATION triggerTierUpNow(Instance* instance, uint32_t functionIndex)
{
    Wasm::CodeBlock& codeBlock = *instance->codeBlock();
    ASSERT(instance->memory()->mode() == codeBlock.mode());

    uint32_t functionIndexInSpace = functionIndex + codeBlock.functionImportCount();
    ASSERT(codeBlock.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace).compilationMode() == Wasm::CompilationMode::BBQMode);
    BBQCallee& callee = static_cast<BBQCallee&>(codeBlock.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace));
    TierUpCount& tierUp = *callee.tierUpCount();
    dataLogLnIf(Options::verboseOSR(), "Consider OMGPlan for [", functionIndex, "] with executeCounter = ", tierUp, " ", RawPointer(callee.replacement()));

    if (shouldTriggerOMGCompile(tierUp, callee.replacement(), functionIndex))
        triggerOMGReplacementCompile(tierUp, callee.replacement(), instance, codeBlock, functionIndex);

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

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
