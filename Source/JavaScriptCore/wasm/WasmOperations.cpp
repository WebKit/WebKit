/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyRuntimeError.h"
#include "ProbeContext.h"
#include "ReleaseHeapAccessScope.h"
#include "TypedArrayController.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmInstance.h"
#include "WasmMemory.h"
#include "WasmOMGForOSREntryPlan.h"
#include "WasmOMGPlan.h"
#include "WasmOSREntryData.h"
#include "WasmWorklist.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

IGNORE_WARNINGS_BEGIN("frame-address")

namespace JSC { namespace Wasm {

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

    RELEASE_ASSERT(osrEntryCallee.osrEntryScratchBufferSize() == osrEntryData.values().size());

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
    context.gpr(ARM64Registers::lr) = bitwise_cast<UCPURegister>(untagCodePtrWithStackPointerForJITCall(context.gpr<void*>(ARM64Registers::lr), context.sp()));
#endif
#else
#error Unsupported architecture.
#endif
    // 4. Configure argument registers to jump to OSR entry from the caller of this runtime function.
    context.gpr(GPRInfo::argumentGPR0) = bitwise_cast<UCPURegister>(buffer);
    context.gpr(GPRInfo::argumentGPR1) = bitwise_cast<UCPURegister>(osrEntryCallee.entrypoint().executableAddress<>());
}

inline bool shouldJIT(unsigned functionIndex)
{
    if (!Options::useOMGJIT())
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

    if (!shouldJIT(functionIndex)) {
        tierUp.deferIndefinitely();
        return returnWithoutOSREntry();
    }

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

JSC_DEFINE_JIT_OPERATION(operationWasmTriggerTierUpNow, void, (Instance* instance, uint32_t functionIndex))
{
    Wasm::CodeBlock& codeBlock = *instance->codeBlock();
    ASSERT(instance->memory()->mode() == codeBlock.mode());

    uint32_t functionIndexInSpace = functionIndex + codeBlock.functionImportCount();
    ASSERT(codeBlock.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace).compilationMode() == Wasm::CompilationMode::BBQMode);
    BBQCallee& callee = static_cast<BBQCallee&>(codeBlock.wasmBBQCalleeFromFunctionIndexSpace(functionIndexInSpace));
    TierUpCount& tierUp = *callee.tierUpCount();

    if (!shouldJIT(functionIndex)) {
        tierUp.deferIndefinitely();
        return;
    }

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
#endif

JSC_DEFINE_JIT_OPERATION(operationWasmUnwind, void, (CallFrame* callFrame))
{
    // FIXME: Consider passing JSWebAssemblyInstance* instead.
    // https://bugs.webkit.org/show_bug.cgi?id=203206
    VM& vm = callFrame->deprecatedVM();
    NativeCallFrameTracer tracer(vm, callFrame);
    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
}

JSC_DEFINE_JIT_OPERATION(operationConvertToI64, int64_t, (CallFrame* callFrame, JSValue v))
{
    // FIXME: Consider passing JSWebAssemblyInstance* instead.
    // https://bugs.webkit.org/show_bug.cgi?id=203206
    VM& vm = callFrame->deprecatedVM();
    NativeCallFrameTracer tracer(vm, callFrame);
    return v.toBigInt64(callFrame->lexicalGlobalObject(vm));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToF64, double, (CallFrame* callFrame, JSValue v))
{
    // FIXME: Consider passing JSWebAssemblyInstance* instead.
    // https://bugs.webkit.org/show_bug.cgi?id=203206
    VM& vm = callFrame->deprecatedVM();
    NativeCallFrameTracer tracer(vm, callFrame);
    return v.toNumber(callFrame->lexicalGlobalObject(vm));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToI32, int32_t, (CallFrame* callFrame, JSValue v))
{
    // FIXME: Consider passing JSWebAssemblyInstance* instead.
    // https://bugs.webkit.org/show_bug.cgi?id=203206
    VM& vm = callFrame->deprecatedVM();
    NativeCallFrameTracer tracer(vm, callFrame);
    return v.toInt32(callFrame->lexicalGlobalObject(vm));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToF32, float, (CallFrame* callFrame, JSValue v))
{
    // FIXME: Consider passing JSWebAssemblyInstance* instead.
    // https://bugs.webkit.org/show_bug.cgi?id=203206
    VM& vm = callFrame->deprecatedVM();
    NativeCallFrameTracer tracer(vm, callFrame);
    return static_cast<float>(v.toNumber(callFrame->lexicalGlobalObject(vm)));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToBigInt, EncodedJSValue, (CallFrame* callFrame, Instance* instance, int64_t value))
{
    JSWebAssemblyInstance* jsInstance = instance->owner<JSWebAssemblyInstance>();
    JSGlobalObject* globalObject = jsInstance->globalObject();
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(JSBigInt::makeHeapBigIntOrBigInt32(globalObject, value));
}

// https://webassembly.github.io/multi-value/js-api/index.html#run-a-host-function
JSC_DEFINE_JIT_OPERATION(operationIterateResults, void, (CallFrame* callFrame, Instance* instance, const Signature* signature, JSValue result, uint64_t* registerResults, uint64_t* calleeFramePointer))
{
    // FIXME: Consider passing JSWebAssemblyInstance* instead.
    // https://bugs.webkit.org/show_bug.cgi?id=203206
    JSWebAssemblyInstance* jsInstance = instance->owner<JSWebAssemblyInstance>();
    JSGlobalObject* globalObject = jsInstance->globalObject();
    VM& vm = globalObject->vm();
    NativeCallFrameTracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto wasmCallInfo = wasmCallingConvention().callInformationFor(*signature, CallRole::Callee);
    RegisterAtOffsetList registerResultOffsets = wasmCallInfo.computeResultsOffsetList();

    unsigned iterationCount = 0;
    MarkedArgumentBuffer buffer;
    forEachInIterable(globalObject, result, [&] (VM&, JSGlobalObject*, JSValue value) -> void {
        if (buffer.size() < signature->returnCount())
            buffer.append(value);
        ++iterationCount;
    });
    RETURN_IF_EXCEPTION(scope, void());

    if (buffer.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope, "JS results to Wasm are too large");
        return;
    }

    if (iterationCount != signature->returnCount()) {
        throwVMTypeError(globalObject, scope, "Incorrect number of values returned to Wasm from JS");
        return;
    }

    for (unsigned index = 0; index < buffer.size(); ++index) {
        JSValue value = buffer.at(index);

        uint64_t unboxedValue = 0;
        switch (signature->returnType(index)) {
        case I32:
            unboxedValue = value.toInt32(globalObject);
            break;
        case I64:
            unboxedValue = value.toBigInt64(globalObject);
            break;
        case F32:
            unboxedValue = bitwise_cast<uint32_t>(value.toFloat(globalObject));
            break;
        case F64:
            unboxedValue = bitwise_cast<uint64_t>(value.toNumber(globalObject));
            break;
        case Funcref:
            if (!value.isCallable(vm)) {
                throwTypeError(globalObject, scope, "Funcref value is not a function"_s);
                return;
            }
            FALLTHROUGH;
        case Externref:
            unboxedValue = bitwise_cast<uint64_t>(value);
            RELEASE_ASSERT(Options::useWebAssemblyReferences());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        RETURN_IF_EXCEPTION(scope, void());

        auto rep = wasmCallInfo.results[index];
        if (rep.isReg())
            registerResults[registerResultOffsets.find(rep.reg())->offset() / sizeof(uint64_t)] = unboxedValue;
        else
            calleeFramePointer[rep.offsetFromFP() / sizeof(uint64_t)] = unboxedValue;
    }
}

// FIXME: It would be much easier to inline this when we have a global GC, which could probably mean we could avoid
// spilling the results onto the stack.
// Saved result registers should be placed on the stack just above the last stack result.
JSC_DEFINE_JIT_OPERATION(operationAllocateResultsArray, JSArray*, (CallFrame* callFrame, Wasm::Instance* instance, const Signature* signature, IndexingType indexingType, JSValue* stackPointerFromCallee))
{
    JSWebAssemblyInstance* jsInstance = instance->owner<JSWebAssemblyInstance>();
    VM& vm = jsInstance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);

    JSGlobalObject* globalObject = jsInstance->globalObject();
    ObjectInitializationScope initializationScope(globalObject->vm());
    JSArray* result = JSArray::tryCreateUninitializedRestricted(initializationScope, nullptr, globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType), signature->returnCount());

    // FIXME: Handle allocation failure...
    RELEASE_ASSERT(result);

    auto wasmCallInfo = wasmCallingConvention().callInformationFor(*signature);
    RegisterAtOffsetList registerResults = wasmCallInfo.computeResultsOffsetList();

    static_assert(sizeof(JSValue) == sizeof(CPURegister), "The code below relies on this.");
    for (unsigned i = 0; i < signature->returnCount(); ++i) {
        ValueLocation loc = wasmCallInfo.results[i];
        JSValue value;
        if (loc.isReg())
            value = stackPointerFromCallee[(registerResults.find(loc.reg())->offset() + wasmCallInfo.headerAndArgumentStackSizeInBytes) / sizeof(JSValue)];
        else
            value = stackPointerFromCallee[loc.offsetFromSP() / sizeof(JSValue)];
        result->initializeIndex(initializationScope, i, value);
    }

    ASSERT(result->indexingType() == indexingType);
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationWasmWriteBarrierSlowPath, void, (JSCell* cell, VM* vmPointer))
{
    ASSERT(cell);
    ASSERT(vmPointer);
    VM& vm = *vmPointer;
    vm.heap.writeBarrierSlowPath(cell);
}

JSC_DEFINE_JIT_OPERATION(operationPopcount32, uint32_t, (int32_t value))
{
    return __builtin_popcount(value);
}

JSC_DEFINE_JIT_OPERATION(operationPopcount64, uint64_t, (int64_t value))
{
    return __builtin_popcountll(value);
}

JSC_DEFINE_JIT_OPERATION(operationGrowMemory, int32_t, (void* callFrame, Instance* instance, int32_t delta))
{
    instance->storeTopCallFrame(callFrame);

    if (delta < 0)
        return -1;

    auto grown = instance->memory()->grow(PageCount(delta));
    if (!grown) {
        switch (grown.error()) {
        case Memory::GrowFailReason::InvalidDelta:
        case Memory::GrowFailReason::InvalidGrowSize:
        case Memory::GrowFailReason::WouldExceedMaximum:
        case Memory::GrowFailReason::OutOfMemory:
            return -1;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    return grown.value().pageCount();
}

JSC_DEFINE_JIT_OPERATION(operationWasmMemoryFill, bool, (Instance* instance, uint32_t dstAddress, uint32_t targetValue, uint32_t count))
{
    return instance->memory()->fill(dstAddress, static_cast<uint8_t>(targetValue), count);
}

JSC_DEFINE_JIT_OPERATION(operationWasmMemoryCopy, bool, (Instance* instance, uint32_t dstAddress, uint32_t srcAddress, uint32_t count))
{
    return instance->memory()->copy(dstAddress, srcAddress, count);
}

JSC_DEFINE_JIT_OPERATION(operationGetWasmTableElement, EncodedJSValue, (Instance* instance, unsigned tableIndex, int32_t signedIndex))
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());
    if (signedIndex < 0)
        return 0;

    uint32_t index = signedIndex;
    if (index >= instance->table(tableIndex)->length())
        return 0;

    return JSValue::encode(instance->table(tableIndex)->get(index));
}

static bool setWasmTableElement(Instance* instance, unsigned tableIndex, uint32_t index, EncodedJSValue encValue)
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());

    if (index >= instance->table(tableIndex)->length())
        return false;

    JSValue value = JSValue::decode(encValue);
    if (instance->table(tableIndex)->type() == Wasm::TableElementType::Externref)
        instance->table(tableIndex)->set(index, value);
    else if (instance->table(tableIndex)->type() == Wasm::TableElementType::Funcref) {
        WebAssemblyFunction* wasmFunction;
        WebAssemblyWrapperFunction* wasmWrapperFunction;

        if (isWebAssemblyHostFunction(instance->owner<JSObject>()->vm(), value, wasmFunction, wasmWrapperFunction)) {
            ASSERT(!!wasmFunction || !!wasmWrapperFunction);
            if (wasmFunction)
                instance->table(tableIndex)->asFuncrefTable()->setFunction(index, jsCast<JSObject*>(value), wasmFunction->importableFunction(), &wasmFunction->instance()->instance());
            else
                instance->table(tableIndex)->asFuncrefTable()->setFunction(index, jsCast<JSObject*>(value), wasmWrapperFunction->importableFunction(), &wasmWrapperFunction->instance()->instance());
        } else if (value.isNull())
            instance->table(tableIndex)->clear(index);
        else
            ASSERT_NOT_REACHED();
    } else
        ASSERT_NOT_REACHED();

    return true;
}

JSC_DEFINE_JIT_OPERATION(operationSetWasmTableElement, bool, (Instance* instance, unsigned tableIndex, uint32_t signedIndex, EncodedJSValue encValue))
{
    return setWasmTableElement(instance, tableIndex, signedIndex, encValue);
}

JSC_DEFINE_JIT_OPERATION(operationWasmTableInit, bool, (Instance* instance, unsigned elementIndex, unsigned tableIndex, uint32_t dstOffset, uint32_t srcOffset, uint32_t length))
{
    ASSERT(elementIndex < instance->module().moduleInformation().elementCount());
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());

    if (WTF::sumOverflows<uint32_t>(srcOffset, length))
        return false;

    if (WTF::sumOverflows<uint32_t>(dstOffset, length))
        return false;

    if (dstOffset + length > instance->table(tableIndex)->length())
        return false;

    const uint32_t lengthOfElementSegment = instance->elementAt(elementIndex) ? instance->elementAt(elementIndex)->length() : 0U;
    if (srcOffset + length > lengthOfElementSegment)
        return false;

    if (!lengthOfElementSegment)
        return true;

    instance->tableInit(dstOffset, srcOffset, length, elementIndex, tableIndex);
    return true;
}

JSC_DEFINE_JIT_OPERATION(operationWasmElemDrop, void, (Instance* instance, unsigned elementIndex))
{
    ASSERT(elementIndex < instance->module().moduleInformation().elementCount());
    instance->elemDrop(elementIndex);
}

JSC_DEFINE_JIT_OPERATION(operationWasmTableGrow, int32_t, (Instance* instance, unsigned tableIndex, EncodedJSValue fill, uint32_t delta))
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());
    auto oldSize = instance->table(tableIndex)->length();
    auto newSize = instance->table(tableIndex)->grow(delta, jsNull());
    if (!newSize)
        return -1;

    for (unsigned i = oldSize; i < instance->table(tableIndex)->length(); ++i)
        setWasmTableElement(instance, tableIndex, i, fill);

    return oldSize;
}

JSC_DEFINE_JIT_OPERATION(operationWasmTableFill, bool, (Instance* instance, unsigned tableIndex, uint32_t offset, EncodedJSValue fill, uint32_t count))
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());

    if (WTF::sumOverflows<uint32_t>(offset, count))
        return false;

    if (offset + count > instance->table(tableIndex)->length())
        return false;

    for (uint32_t index = 0; index < count; ++index)
        setWasmTableElement(instance, tableIndex, offset + index, fill);

    return true;
}

JSC_DEFINE_JIT_OPERATION(operationWasmTableCopy, bool, (Instance* instance, unsigned dstTableIndex, unsigned srcTableIndex, int32_t dstOffset, int32_t srcOffset, int32_t length))
{
    ASSERT(dstTableIndex < instance->module().moduleInformation().tableCount());
    ASSERT(srcTableIndex < instance->module().moduleInformation().tableCount());
    const Table* dstTable = instance->table(dstTableIndex);
    const Table* srcTable = instance->table(srcTableIndex);
    ASSERT(dstTable->type() == srcTable->type());

    if ((srcOffset < 0) || (dstOffset < 0) || (length < 0))
        return false;

    Checked<uint32_t, RecordOverflow> lastDstElementIndexChecked = static_cast<uint32_t>(dstOffset);
    lastDstElementIndexChecked += static_cast<uint32_t>(length);

    uint32_t lastDstElementIndex;
    if (lastDstElementIndexChecked.safeGet(lastDstElementIndex) == CheckedState::DidOverflow)
        return false;

    if (lastDstElementIndex > dstTable->length())
        return false;

    Checked<uint32_t, RecordOverflow> lastSrcElementIndexChecked = static_cast<uint32_t>(srcOffset);
    lastSrcElementIndexChecked += static_cast<uint32_t>(length);

    uint32_t lastSrcElementIndex;
    if (lastSrcElementIndexChecked.safeGet(lastSrcElementIndex) == CheckedState::DidOverflow)
        return false;

    if (lastSrcElementIndex > srcTable->length())
        return false;

    instance->tableCopy(dstOffset, srcOffset, length, dstTableIndex, srcTableIndex);
    return true;
}

JSC_DEFINE_JIT_OPERATION(operationWasmRefFunc, EncodedJSValue, (Instance* instance, uint32_t index))
{
    JSValue value = instance->getFunctionWrapper(index);
    ASSERT(value.isCallable(instance->owner<JSObject>()->vm()));
    return JSValue::encode(value);
}

JSC_DEFINE_JIT_OPERATION(operationGetWasmTableSize, int32_t, (Instance* instance, unsigned tableIndex))
{
    return instance->table(tableIndex)->length();
}

template<typename ValueType>
static int32_t wait(VM& vm, ValueType* pointer, ValueType expectedValue, int64_t timeoutInNanoseconds)
{
    Seconds timeout = Seconds::infinity();
    if (timeoutInNanoseconds >= 0) {
        int64_t timeoutInMilliseconds = timeoutInNanoseconds / 1000;
        timeout = Seconds::fromMilliseconds(timeoutInMilliseconds);
    }
    bool didPassValidation = false;
    ParkingLot::ParkResult result;
    {
        ReleaseHeapAccessScope releaseHeapAccessScope(vm.heap);
        result = ParkingLot::parkConditionally(
            pointer,
            [&] () -> bool {
                didPassValidation = WTF::atomicLoad(pointer) == expectedValue;
                return didPassValidation;
            },
            [] () { },
            MonotonicTime::now() + timeout);
    }
    if (!didPassValidation)
        return 1;
    if (!result.wasUnparked)
        return 2;
    return 0;
}

JSC_DEFINE_JIT_OPERATION(operationMemoryAtomicWait32, int32_t, (Instance* instance, unsigned base, unsigned offset, uint32_t value, int64_t timeoutInNanoseconds))
{
    VM& vm = instance->owner<JSWebAssemblyInstance>()->vm();
    uint64_t offsetInMemory = static_cast<uint64_t>(base) + offset;
    if (offsetInMemory & (0x4 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return -1;
    if (!vm.m_typedArrayController->isAtomicsWaitAllowedOnCurrentThread())
        return -1;
    uint32_t* pointer = bitwise_cast<uint32_t*>(instance->memory()->memory()) + offsetInMemory;
    return wait<uint32_t>(vm, pointer, value, timeoutInNanoseconds);
}

JSC_DEFINE_JIT_OPERATION(operationMemoryAtomicWait64, int32_t, (Instance* instance, unsigned base, unsigned offset, uint64_t value, int64_t timeoutInNanoseconds))
{
    VM& vm = instance->owner<JSWebAssemblyInstance>()->vm();
    uint64_t offsetInMemory = static_cast<uint64_t>(base) + offset;
    if (offsetInMemory & (0x8 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return -1;
    if (!vm.m_typedArrayController->isAtomicsWaitAllowedOnCurrentThread())
        return -1;
    uint64_t* pointer = bitwise_cast<uint64_t*>(instance->memory()->memory()) + offsetInMemory;
    return wait<uint64_t>(vm, pointer, value, timeoutInNanoseconds);
}

JSC_DEFINE_JIT_OPERATION(operationMemoryAtomicNotify, int32_t, (Instance* instance, unsigned base, unsigned offset, int32_t countValue))
{
    uint64_t offsetInMemory = static_cast<uint64_t>(base) + offset;
    if (offsetInMemory & (0x4 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return 0;
    uint8_t* pointer = bitwise_cast<uint8_t*>(instance->memory()->memory()) + offsetInMemory;
    unsigned count = UINT_MAX;
    if (countValue >= 0)
        count = static_cast<unsigned>(countValue);
    return ParkingLot::unparkCount(pointer, count);
}

JSC_DEFINE_JIT_OPERATION(operationWasmMemoryInit, bool, (Instance* instance, unsigned dataSegmentIndex, uint32_t dstAddress, uint32_t srcAddress, uint32_t length))
{
    ASSERT(dataSegmentIndex < instance->module().moduleInformation().dataSegmentsCount());
    return instance->memoryInit(dstAddress, srcAddress, length, dataSegmentIndex);
}

JSC_DEFINE_JIT_OPERATION(operationWasmDataDrop, void, (Instance* instance, unsigned dataSegmentIndex))
{
    ASSERT(dataSegmentIndex < instance->module().moduleInformation().dataSegmentsCount());
    instance->dataDrop(dataSegmentIndex);
}

JSC_DEFINE_JIT_OPERATION(operationWasmToJSException, void*, (CallFrame* callFrame, Wasm::ExceptionType type, Instance* wasmInstance))
{
    wasmInstance->storeTopCallFrame(callFrame);
    JSWebAssemblyInstance* instance = wasmInstance->owner<JSWebAssemblyInstance>();
    JSGlobalObject* globalObject = instance->globalObject();

    // Do not retrieve VM& from CallFrame since CallFrame's callee is not a JSCell.
    VM& vm = globalObject->vm();

    {
        auto throwScope = DECLARE_THROW_SCOPE(vm);

        JSObject* error;
        if (type == ExceptionType::StackOverflow)
            error = createStackOverflowError(globalObject);
        else
            error = JSWebAssemblyRuntimeError::create(globalObject, vm, globalObject->webAssemblyRuntimeErrorStructure(), Wasm::errorMessageForExceptionType(type));
        throwException(globalObject, throwScope, error);
    }

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    // FIXME: We could make this better:
    // This is a total hack, but the llint (both op_catch and llint_handle_uncaught_exception)
    // require a cell in the callee field to load the VM. (The baseline JIT does not require
    // this since it is compiled with a constant VM pointer.) We could make the calling convention
    // for exceptions first load callFrameForCatch info call frame register before jumping
    // to the exception handler. If we did this, we could remove this terrible hack.
    // https://bugs.webkit.org/show_bug.cgi?id=170440
    bitwise_cast<uint64_t*>(callFrame)[static_cast<int>(CallFrameSlot::callee)] = bitwise_cast<uint64_t>(instance->module());
    return vm.targetMachinePCForThrow;
}

} } // namespace JSC::Wasm

IGNORE_WARNINGS_END

#endif // ENABLE(WEBASSEMBLY)
