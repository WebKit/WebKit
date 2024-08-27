/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#include "WasmIPIntSlowPaths.h"

#if ENABLE(WEBASSEMBLY)

#include "BytecodeStructs.h"
#include "FrameTracers.h"
#include "JITExceptions.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "WasmBBQPlan.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmFunctionCodeBlockGenerator.h"
#include "WasmIPIntGenerator.h"
#include "WasmLLIntBuiltin.h"
#include "WasmLLIntGenerator.h"
#include "WasmModuleInformation.h"
#include "WasmOMGPlan.h"
#include "WasmOSREntryPlan.h"
#include "WasmOperationsInlines.h"
#include "WasmTypeDefinitionInlines.h"
#include "WasmWorklist.h"
#include "WebAssemblyFunction.h"
#include <bit>

namespace JSC { namespace IPInt {

#define WASM_RETURN_TWO(first, second) do { \
        return encodeResult(first, second); \
    } while (false)

#define WASM_THROW(callFrame, exceptionType) do { \
        callFrame->setArgumentCountIncludingThis(static_cast<int>(exceptionType)); \
        WASM_RETURN_TWO(LLInt::wasmExceptionInstructions(), 0); \
    } while (false)

#define WASM_CALL_RETURN(callee, callTarget, callTargetTag) do { \
        callee.entrypoint = retagCodePtr<callTargetTag, JSEntrySlowPathPtrTag>(callTarget); \
        WASM_RETURN_TWO(&callee, callee.instance); \
    } while (false)

#define IPINT_CALLEE(callFrame) \
    static_cast<Wasm::IPIntCallee*>(callFrame->callee().asNativeCallee())

#if ENABLE(WEBASSEMBLY_BBQJIT)
#if ENABLE(WEBASSEMBLY_OMGJIT)
enum class RequiredWasmJIT { Any, OMG };

static inline bool shouldJIT(Wasm::IPIntCallee* callee, RequiredWasmJIT requiredJIT = RequiredWasmJIT::Any)
{
    if (requiredJIT == RequiredWasmJIT::OMG) {
        if (!Options::useOMGJIT() || !Wasm::OMGPlan::ensureGlobalOMGAllowlist().containsWasmFunction(callee->functionIndex()))
            return false;
    } else {
        if (Options::wasmIPIntTiersUpToBBQ()
            && (!Options::useBBQJIT() || !Wasm::BBQPlan::ensureGlobalBBQAllowlist().containsWasmFunction(callee->functionIndex())))
            return false;
        if (!Options::wasmIPIntTiersUpToOMG()
            && (!Options::useOMGJIT() || !Wasm::OMGPlan::ensureGlobalOMGAllowlist().containsWasmFunction(callee->functionIndex())))
            return false;
    }
    if (!Options::wasmFunctionIndexRangeToCompile().isInRange(callee->functionIndex()))
        return false;
    return true;
}

static inline bool jitCompileAndSetHeuristics(Wasm::IPIntCallee* callee, JSWebAssemblyInstance* instance)
{
    Wasm::IPIntTierUpCounter& tierUpCounter = callee->tierUpCounter();
    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        return false;
    }

    MemoryMode memoryMode = instance->memory()->mode();
    if (callee->replacement(memoryMode))  {
        dataLogLnIf(Options::verboseOSR(), "    Code was already compiled.");
        tierUpCounter.optimizeSoon();
        return true;
    }

    bool compile = false;
    {
        Locker locker { tierUpCounter.m_lock };
        switch (tierUpCounter.compilationStatus(memoryMode)) {
        case Wasm::IPIntTierUpCounter::CompilationStatus::NotCompiled:
            compile = true;
            tierUpCounter.setCompilationStatus(memoryMode, Wasm::IPIntTierUpCounter::CompilationStatus::Compiling);
            break;
        case Wasm::IPIntTierUpCounter::CompilationStatus::Compiling:
            tierUpCounter.optimizeAfterWarmUp();
            break;
        case Wasm::IPIntTierUpCounter::CompilationStatus::Compiled:
            break;
        }
    }

    if (compile) {
        uint32_t functionIndex = callee->functionIndex();
        RefPtr<Wasm::Plan> plan;
        if (Options::wasmIPIntTiersUpToBBQ() && Wasm::BBQPlan::ensureGlobalBBQAllowlist().containsWasmFunction(functionIndex))
            plan = adoptRef(*new Wasm::BBQPlan(instance->vm(), const_cast<Wasm::ModuleInformation&>(instance->module().moduleInformation()), functionIndex, callee->hasExceptionHandlers(), instance->calleeGroup(), Wasm::Plan::dontFinalize()));
        else // No need to check OMG allow list: if we didn't want to compile this function, shouldJIT should have returned false.
            plan = adoptRef(*new Wasm::OMGPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), functionIndex, callee->hasExceptionHandlers(), memoryMode, Wasm::Plan::dontFinalize()));

        Wasm::ensureWorklist().enqueue(*plan);
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUpCounter.optimizeAfterWarmUp();
    }

    return !!callee->replacement(memoryMode);
}

WASM_IPINT_EXTERN_CPP_DECL(prologue_osr, CallFrame* callFrame)
{
    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);

    if (!shouldJIT(callee)) {
        callee->tierUpCounter().deferIndefinitely();
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    if (!Options::useWasmIPIntPrologueOSR())
        WASM_RETURN_TWO(nullptr, nullptr);

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered prologue_osr with tierUpCounter = ", callee->tierUpCounter());

    if (!jitCompileAndSetHeuristics(callee, instance))
        WASM_RETURN_TWO(nullptr, nullptr);
    WASM_RETURN_TWO(callee->replacement(instance->memory()->mode())->entrypoint().taggedPtr(), nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(loop_osr, CallFrame* callFrame, uint32_t pc, uint64_t* pl)
{
    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    Wasm::IPIntTierUpCounter& tierUpCounter = callee->tierUpCounter();

    if (!Options::useWasmOSR() || !Options::useWasmIPIntLoopOSR() || !shouldJIT(callee, RequiredWasmJIT::OMG)) {
        ipint_extern_prologue_osr(instance, callFrame);
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered loop_osr with tierUpCounter = ", callee->tierUpCounter());

    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    unsigned loopOSREntryBytecodeOffset = pc;
    const auto& osrEntryData = tierUpCounter.osrEntryDataForLoop(loopOSREntryBytecodeOffset);

    if (Options::wasmIPIntTiersUpToBBQ() && Options::useBBQJIT()) {
        if (!jitCompileAndSetHeuristics(callee, instance))
            WASM_RETURN_TWO(nullptr, nullptr);

        Wasm::BBQCallee* bbqCallee;
        {
            Locker locker { instance->calleeGroup()->m_lock };
            bbqCallee = instance->calleeGroup()->bbqCallee(locker, callee->functionIndex());
        }
        RELEASE_ASSERT(bbqCallee);

        size_t osrEntryScratchBufferSize = bbqCallee->osrEntryScratchBufferSize();
        RELEASE_ASSERT(osrEntryScratchBufferSize >= callee->m_numLocals + osrEntryData.numberOfStackValues + osrEntryData.tryDepth);

        uint64_t* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryScratchBufferSize);
        if (!buffer)
            WASM_RETURN_TWO(nullptr, nullptr);

        uint32_t index = 0;
        buffer[index++] = osrEntryData.loopIndex;
        for (uint32_t i = 0; i < callee->m_numLocals; ++i)
            buffer[index++] = pl[i];

        // If there's no rethrow slots just 0 fill the buffer.
        ASSERT(osrEntryData.tryDepth <= callee->m_numRethrowSlotsToAlloc || !callee->m_numRethrowSlotsToAlloc);
        for (uint32_t i = 0; i < osrEntryData.tryDepth; ++i)
            buffer[index++] = callee->m_numRethrowSlotsToAlloc ? pl[callee->m_localSizeToAlloc + i] : 0;

        for (uint32_t i = 0; i < osrEntryData.numberOfStackValues; ++i) {
            pl -= 2; // each stack slot is 16B
            buffer[index++] = *pl;
        }

        auto sharedLoopEntrypoint = bbqCallee->sharedLoopEntrypoint();
        RELEASE_ASSERT(sharedLoopEntrypoint);
        WASM_RETURN_TWO(buffer, sharedLoopEntrypoint->taggedPtr());
    } else {
        const auto doOSREntry = [&](Wasm::OSREntryCallee* osrEntryCallee) {
            if (osrEntryCallee->loopIndex() != osrEntryData.loopIndex)
                WASM_RETURN_TWO(nullptr, nullptr);

            size_t osrEntryScratchBufferSize = osrEntryCallee->osrEntryScratchBufferSize();
            RELEASE_ASSERT(osrEntryScratchBufferSize == callee->m_numLocals + osrEntryData.numberOfStackValues + osrEntryData.tryDepth);

            uint64_t* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryScratchBufferSize);
            if (!buffer)
                WASM_RETURN_TWO(nullptr, nullptr);

            uint32_t index = 0;
            for (uint32_t i = 0; i < callee->m_numLocals; ++i)
                buffer[index++] = pl[i];

            // If there's no rethrow slots just 0 fill the buffer.
            ASSERT(osrEntryData.tryDepth <= callee->m_numRethrowSlotsToAlloc || !callee->m_numRethrowSlotsToAlloc);
            for (uint32_t i = 0; i < osrEntryData.tryDepth; ++i)
                buffer[index++] = callee->m_numRethrowSlotsToAlloc ? pl[callee->m_localSizeToAlloc + i] : 0;

            for (uint32_t i = 0; i < osrEntryData.numberOfStackValues; ++i) {
                pl -= 2; // each stack slot is 16B
                buffer[index++] = *pl;
            }

            WASM_RETURN_TWO(buffer, osrEntryCallee->entrypoint().taggedPtr());
        };

        MemoryMode memoryMode = instance->memory()->mode();
        if (auto* osrEntryCallee = callee->osrEntryCallee(memoryMode))
            return doOSREntry(osrEntryCallee);

        bool compile = false;
        {
            Locker locker { tierUpCounter.m_lock };
            switch (tierUpCounter.loopCompilationStatus(memoryMode)) {
            case Wasm::IPIntTierUpCounter::CompilationStatus::NotCompiled:
                compile = true;
                tierUpCounter.setLoopCompilationStatus(memoryMode, Wasm::IPIntTierUpCounter::CompilationStatus::Compiling);
                break;
            case Wasm::IPIntTierUpCounter::CompilationStatus::Compiling:
                tierUpCounter.optimizeAfterWarmUp();
                break;
            case Wasm::IPIntTierUpCounter::CompilationStatus::Compiled:
                break;
            }
        }

        if (compile) {
            Ref<Wasm::Plan> plan = adoptRef(*static_cast<Wasm::Plan*>(new Wasm::OSREntryPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), Ref<Wasm::Callee>(*callee), callee->functionIndex(), callee->hasExceptionHandlers(), osrEntryData.loopIndex, memoryMode, Wasm::Plan::dontFinalize())));
            Wasm::ensureWorklist().enqueue(plan.copyRef());
            if (UNLIKELY(!Options::useConcurrentJIT()))
                plan->waitForCompletion();
            else
                tierUpCounter.optimizeAfterWarmUp();
        }

        if (auto* osrEntryCallee = callee->osrEntryCallee(memoryMode))
            return doOSREntry(osrEntryCallee);

        WASM_RETURN_TWO(nullptr, nullptr);
    }
}

WASM_IPINT_EXTERN_CPP_DECL(epilogue_osr, CallFrame* callFrame)
{
    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);

    if (!shouldJIT(callee)) {
        callee->tierUpCounter().deferIndefinitely();
        WASM_RETURN_TWO(nullptr, nullptr);
    }
    if (!Options::useWasmIPIntEpilogueOSR())
        WASM_RETURN_TWO(nullptr, nullptr);

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered epilogue_osr with tierUpCounter = ", callee->tierUpCounter());

    jitCompileAndSetHeuristics(callee, instance);
    WASM_RETURN_TWO(nullptr, nullptr);
}
#endif
#endif

WASM_IPINT_EXTERN_CPP_DECL(retrieve_and_clear_exception, CallFrame* callFrame, v128_t* stackPointer, uint64_t* pl)
{
    VM& vm = instance->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!!throwScope.exception());

    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    if (callee->m_numRethrowSlotsToAlloc) {
        RELEASE_ASSERT(vm.targetTryDepthForThrow <= callee->m_numRethrowSlotsToAlloc);
        pl[callee->m_localSizeToAlloc + vm.targetTryDepthForThrow - 1] = bitwise_cast<uint64_t>(throwScope.exception()->value());
    }

    if (stackPointer) {
        // We only have a stack pointer if we're doing a catch not a catch_all
        Exception* exception = throwScope.exception();
        auto* wasmException = jsSecureCast<JSWebAssemblyException*>(exception->value());

        ASSERT(wasmException->payload().size() == wasmException->tag().parameterCount());
        uint64_t size = wasmException->payload().size();

#if ASSERT_ENABLED
        constexpr uint64_t hole = 0x1BADBEEF;
#else
        constexpr uint64_t hole = 0;
#endif
        for (unsigned i = 0; i < size; ++i)
            stackPointer[size - i - 1] = { hole, wasmException->payload()[i] };
    }

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    throwScope.clearException();

    WASM_RETURN_TWO(nullptr, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(throw_exception, CallFrame* callFrame, uint64_t* arguments, unsigned exceptionIndex)
{
    VM& vm = instance->vm();
    SlowPathFrameTracer tracer(vm, callFrame);

    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!throwScope.exception());

    JSGlobalObject* globalObject = instance->globalObject();
    const Wasm::Tag& tag = instance->tag(exceptionIndex);

    FixedVector<uint64_t> values(tag.parameterBufferSize());
    for (unsigned i = 0; i < tag.parameterBufferSize(); ++i)
        values[i] = arguments[i];

    ASSERT(tag.type().returnsVoid());
    JSWebAssemblyException* exception = JSWebAssemblyException::create(vm, globalObject->webAssemblyExceptionStructure(), tag, WTFMove(values));
    throwException(globalObject, throwScope, exception);

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    WASM_RETURN_TWO(vm.targetMachinePCForThrow, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(rethrow_exception, CallFrame* callFrame, uint64_t* pl, unsigned tryDepth)
{
    SlowPathFrameTracer tracer(instance->vm(), callFrame);

    JSGlobalObject* globalObject = instance->globalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    RELEASE_ASSERT(tryDepth <= callee->m_numRethrowSlotsToAlloc);
    JSWebAssemblyException* exception = reinterpret_cast<JSWebAssemblyException**>(pl)[callee->m_localSizeToAlloc + tryDepth - 1];
    RELEASE_ASSERT(exception);
    throwException(globalObject, throwScope, exception);

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    WASM_RETURN_TWO(vm.targetMachinePCForThrow, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(table_get, unsigned tableIndex, unsigned index)
{
#if CPU(ARM64) || CPU(X86_64)
    EncodedJSValue result = Wasm::tableGet(instance, tableIndex, index);
    if (!result)
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(bitwise_cast<void*>(result), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tableIndex);
    UNUSED_PARAM(index);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARMv8 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_set, unsigned tableIndex, unsigned index, EncodedJSValue value)
{
#if CPU(ARM64) || CPU(X86_64)
    if (!Wasm::tableSet(instance, tableIndex, index, value))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tableIndex);
    UNUSED_PARAM(index);
    UNUSED_PARAM(value);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_init, tableInitMetadata* metadata)
{
#if CPU(ARM64) || CPU(X86_64)
    if (!Wasm::tableInit(instance, metadata->elementIndex, metadata->tableIndex, metadata->dst, metadata->src, metadata->length))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(metadata);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_fill, tableFillMetadata* metadata)
{
#if CPU(ARM64) || CPU(X86_64)
    if (!Wasm::tableFill(instance, metadata->tableIndex, metadata->offset, metadata->fill, metadata->length))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(metadata);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_grow, tableGrowMetadata* metadata)
{
#if CPU(ARM64) || CPU(X86_64)
    WASM_RETURN_TWO(bitwise_cast<void*>(Wasm::tableGrow(instance, metadata->tableIndex, metadata->fill, metadata->length)), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(metadata);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL_1P(current_memory)
{
#if CPU(ARM64) || CPU(X86_64)
    size_t size = instance->memory()->memory().handle().size() >> 16;
    WASM_RETURN_TWO(bitwise_cast<void*>(size), 0);
#else
    UNUSED_PARAM(instance);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(memory_grow, int32_t delta)
{
    WASM_RETURN_TWO(reinterpret_cast<void*>(Wasm::growMemory(instance, delta)), 0);
}

WASM_IPINT_EXTERN_CPP_DECL(memory_init, int32_t dataIndex, int32_t dst, int64_t srcAndLength)
{
#if CPU(ARM64) || CPU(X86_64)
    if (!Wasm::memoryInit(instance, dataIndex, dst, srcAndLength >> 32, srcAndLength & 0xffffffff))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsMemoryAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(dataIndex);
    UNUSED_PARAM(dst);
    UNUSED_PARAM(srcAndLength);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(data_drop, int32_t dataIndex)
{
    Wasm::dataDrop(instance, dataIndex);
    WASM_RETURN_TWO(0, 0);
}

WASM_IPINT_EXTERN_CPP_DECL(memory_copy, int32_t dst, int32_t src, int32_t count)
{
#if CPU(ARM64) || CPU(X86_64)
    if (!Wasm::memoryCopy(instance, dst, src, count))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsMemoryAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(dst);
    UNUSED_PARAM(src);
    UNUSED_PARAM(count);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(memory_fill, int32_t dst, int32_t targetValue, int32_t count)
{
#if CPU(ARM64) || CPU(X86_64)
    if (!Wasm::memoryFill(instance, dst, targetValue, count))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsMemoryAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(dst);
    UNUSED_PARAM(targetValue);
    UNUSED_PARAM(count);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(elem_drop, int32_t dataIndex)
{
    Wasm::elemDrop(instance, dataIndex);
    WASM_RETURN_TWO(0, 0);
}

WASM_IPINT_EXTERN_CPP_DECL(table_copy, tableCopyMetadata* metadata)
{
#if CPU(ARM64) || CPU(X86_64)
    if (!Wasm::tableCopy(instance, metadata->dstTableIndex, metadata->srcTableIndex, metadata->dstOffset, metadata->srcOffset, metadata->length))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(metadata);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_size, int32_t tableIndex)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t result = Wasm::tableSize(instance, tableIndex);
    WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<EncodedJSValue>(result)), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tableIndex);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

static inline UGPRPair doWasmCall(JSWebAssemblyInstance* instance, callMetadata* call)
{
    uint32_t importFunctionCount = instance->module().moduleInformation().importFunctionCount();

    CodePtr<WasmEntryPtrTag> codePtr;
    EncodedJSValue boxedCallee = CalleeBits::encodeNullCallee();

    auto functionIndex = call->functionIndex;
    if (functionIndex < importFunctionCount) {
        JSWebAssemblyInstance::ImportFunctionInfo* functionInfo = instance->importFunctionInfo(functionIndex);
        codePtr = functionInfo->importFunctionStub;
    } else {
        // Target is a wasm function within the same instance
        codePtr = *instance->calleeGroup()->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
        boxedCallee = CalleeBits::encodeNativeCallee(
            instance->calleeGroup()->wasmCalleeFromFunctionIndexSpace(functionIndex));
    }

    call->callee.boxedCallee = boxedCallee;
    call->callee.instance = instance;

    WASM_CALL_RETURN(call->callee, codePtr.taggedPtr(), WasmEntryPtrTag);
}

WASM_IPINT_EXTERN_CPP_DECL(call, callMetadata* call)
{
    return doWasmCall(instance, call);
}

WASM_IPINT_EXTERN_CPP_DECL(call_indirect, callIndirectMetadata* call)
{
    Wasm::IPIntCallee* caller = IPINT_CALLEE(call->callFrame);
    unsigned functionIndex = call->functionRef;
    unsigned tableIndex = call->tableIndex;
    unsigned typeIndex = call->typeIndex;
    Wasm::FuncRefTable* table = instance->table(tableIndex)->asFuncrefTable();

    if (functionIndex >= table->length())
        WASM_THROW(call->callFrame, Wasm::ExceptionType::OutOfBoundsCallIndirect);

    const Wasm::FuncRefTable::Function& function = table->function(functionIndex);

    if (function.m_function.typeIndex == Wasm::TypeDefinition::invalidIndex)
        WASM_THROW(call->callFrame, Wasm::ExceptionType::NullTableEntry);

    const auto& callSignature = caller->signature(typeIndex);
    if (callSignature.index() != function.m_function.typeIndex)
        WASM_THROW(call->callFrame, Wasm::ExceptionType::BadSignature);

    if (function.m_function.boxedWasmCalleeLoadLocation)
        call->callee.boxedCallee = CalleeBits::encodeBoxedNativeCallee(reinterpret_cast<void*>(*function.m_function.boxedWasmCalleeLoadLocation));
    else
        call->callee.boxedCallee = CalleeBits::encodeNullCallee();
    call->callee.instance = function.m_instance;

    WASM_CALL_RETURN(call->callee, function.m_function.entrypointLoadLocation->taggedPtr(), WasmEntryPtrTag);
}

WASM_IPINT_EXTERN_CPP_DECL(set_global_ref, uint32_t globalIndex, JSValue value)
{
    instance->setGlobal(globalIndex, value);
    WASM_RETURN_TWO(0, 0);
}

WASM_IPINT_EXTERN_CPP_DECL(set_global_64, unsigned index, uint64_t value)
{
    instance->setGlobal(index, value);
    WASM_RETURN_TWO(0, 0);
}

WASM_IPINT_EXTERN_CPP_DECL(get_global_64, unsigned index)
{
#if CPU(ARM64) || CPU(X86_64)
    WASM_RETURN_TWO(bitwise_cast<void*>(instance->loadI64Global(index)), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(index);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(memory_atomic_wait32, uint64_t pointerWithOffset, uint32_t value, uint64_t timeout)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t result = Wasm::memoryAtomicWait32(instance, pointerWithOffset, value, timeout);
    WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<intptr_t>(result)), nullptr);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(pointerWithOffset);
    UNUSED_PARAM(value);
    UNUSED_PARAM(timeout);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(memory_atomic_wait64, uint64_t pointerWithOffset, uint64_t value, uint64_t timeout)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t result = Wasm::memoryAtomicWait64(instance, pointerWithOffset, value, timeout);
    WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<intptr_t>(result)), nullptr);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(pointerWithOffset);
    UNUSED_PARAM(value);
    UNUSED_PARAM(timeout);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(memory_atomic_notify, unsigned base, unsigned offset, int32_t count)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t result = Wasm::memoryAtomicNotify(instance, base, offset, count);
    WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<intptr_t>(result)), nullptr);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(base);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(count);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(ref_func, unsigned index)
{
#if CPU(ARM64) || CPU(X86_64)
    WASM_RETURN_TWO(bitwise_cast<void*>(Wasm::refFunc(instance, index)), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(index);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

} } // namespace JSC::IPInt

#endif

