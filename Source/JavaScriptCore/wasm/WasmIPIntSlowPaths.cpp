/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WasmInstance.h"
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

#define WASM_THROW(exceptionType) do { \
        callFrame->setArgumentCountIncludingThis(static_cast<int>(exceptionType)); \
        WASM_RETURN_TWO(LLInt::wasmExceptionInstructions(), 0); \
    } while (false)

#define WASM_CALL_RETURN(targetInstance, callTarget, callTargetTag) do { \
        WASM_RETURN_TWO((retagCodePtr<callTargetTag, JSEntrySlowPathPtrTag>(callTarget)), targetInstance); \
    } while (false)

#define IPINT_CALLEE() \
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

static inline bool jitCompileAndSetHeuristics(Wasm::IPIntCallee* callee, Wasm::Instance* instance)
{
    ASSERT(!instance->module().moduleInformation().usesSIMD(callee->functionIndex()));

    Wasm::IPIntTierUpCounter& tierUpCounter = callee->tierUpCounter();
    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        return false;
    }

    if (callee->replacement(instance->memory()->mode()))  {
        dataLogLnIf(Options::verboseOSR(), "    Code was already compiled.");
        tierUpCounter.optimizeSoon();
        return true;
    }

    bool compile = false;
    {
        Locker locker { tierUpCounter.m_lock };
        switch (tierUpCounter.m_compilationStatus) {
        case Wasm::IPIntTierUpCounter::CompilationStatus::NotCompiled:
            compile = true;
            tierUpCounter.m_compilationStatus = Wasm::IPIntTierUpCounter::CompilationStatus::Compiling;
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
            plan = adoptRef(*new Wasm::OMGPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), functionIndex, callee->hasExceptionHandlers(), instance->memory()->mode(), Wasm::Plan::dontFinalize()));

        Wasm::ensureWorklist().enqueue(*plan);
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUpCounter.optimizeAfterWarmUp();
    }

    return !!callee->replacement(instance->memory()->mode());
}

WASM_IPINT_EXTERN_CPP_DECL(prologue_osr, CallFrame* callFrame)
{
    Wasm::IPIntCallee* callee = IPINT_CALLEE();

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
    Wasm::IPIntCallee* callee = IPINT_CALLEE();
    Wasm::IPIntTierUpCounter& tierUpCounter = callee->tierUpCounter();

    if (!Options::useWebAssemblyOSR() || !Options::useWasmIPIntLoopOSR() || !shouldJIT(callee, RequiredWasmJIT::OMG)) {
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

        if (auto* osrEntryCallee = callee->osrEntryCallee(instance->memory()->mode()))
            return doOSREntry(osrEntryCallee);

        bool compile = false;
        {
            Locker locker { tierUpCounter.m_lock };
            switch (tierUpCounter.m_loopCompilationStatus) {
            case Wasm::IPIntTierUpCounter::CompilationStatus::NotCompiled:
                compile = true;
                tierUpCounter.m_loopCompilationStatus = Wasm::IPIntTierUpCounter::CompilationStatus::Compiling;
                break;
            case Wasm::IPIntTierUpCounter::CompilationStatus::Compiling:
                tierUpCounter.optimizeAfterWarmUp();
                break;
            case Wasm::IPIntTierUpCounter::CompilationStatus::Compiled:
                break;
            }
        }

        if (compile) {
            Ref<Wasm::Plan> plan = adoptRef(*static_cast<Wasm::Plan*>(new Wasm::OSREntryPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), Ref<Wasm::Callee>(*callee), callee->functionIndex(), callee->hasExceptionHandlers(), osrEntryData.loopIndex, instance->memory()->mode(), Wasm::Plan::dontFinalize())));
            Wasm::ensureWorklist().enqueue(plan.copyRef());
            if (UNLIKELY(!Options::useConcurrentJIT()))
                plan->waitForCompletion();
            else
                tierUpCounter.optimizeAfterWarmUp();
        }

        if (auto* osrEntryCallee = callee->osrEntryCallee(instance->memory()->mode()))
            return doOSREntry(osrEntryCallee);

        WASM_RETURN_TWO(nullptr, nullptr);
    }
}

WASM_IPINT_EXTERN_CPP_DECL(epilogue_osr, CallFrame* callFrame)
{
    Wasm::IPIntCallee* callee = IPINT_CALLEE();

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

    Wasm::IPIntCallee* callee = IPINT_CALLEE();
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

WASM_IPINT_EXTERN_CPP_DECL(throw_exception, CallFrame* callFrame, v128_t* stack, unsigned exceptionIndex)
{
    VM& vm = instance->vm();
    SlowPathFrameTracer tracer(vm, callFrame);

    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!throwScope.exception());

    JSGlobalObject* globalObject = instance->globalObject();
    const Wasm::Tag& tag = instance->tag(exceptionIndex);

    size_t size = tag.parameterCount();
    FixedVector<uint64_t> values(size);
    for (unsigned i = 0; i < size; ++i)
        values[i] = stack[size - i - 1].u64x2[1];

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
    WASM_RETURN_TWO(vm.targetMachinePCForThrow, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(rethrow_exception, CallFrame* callFrame, uint64_t* pl, unsigned tryDepth)
{
    SlowPathFrameTracer tracer(instance->vm(), callFrame);

    JSGlobalObject* globalObject = instance->globalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    Wasm::IPIntCallee* callee = IPINT_CALLEE();
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

WASM_IPINT_EXTERN_CPP_DECL(table_init, uint32_t* metadata, uint32_t dest, uint64_t srcAndLength)
{
#if CPU(ARM64) || CPU(X86_64)
    uint32_t dstOffset = dest;
    uint32_t srcOffset = srcAndLength >> 32;
    uint32_t length = srcAndLength & 0xffffffff;
    if (!Wasm::tableInit(instance, metadata[0], metadata[1], dstOffset, srcOffset, length))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(metadata);
    UNUSED_PARAM(dest);
    UNUSED_PARAM(srcAndLength);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_fill, uint32_t tableIndex, EncodedJSValue fill, int64_t offsetAndSize)
{
#if CPU(ARM64) || CPU(X86_64)
    uint32_t offset = offsetAndSize >> 32;
    uint32_t size = offsetAndSize & 0xffffffff;
    if (!Wasm::tableFill(instance, tableIndex, offset, fill, size))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tableIndex);
    UNUSED_PARAM(fill);
    UNUSED_PARAM(offsetAndSize);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_grow, int32_t tableIndex, EncodedJSValue fill, uint32_t size)
{
#if CPU(ARM64) || CPU(X86_64)
    WASM_RETURN_TWO(bitwise_cast<void*>(Wasm::tableGrow(instance, tableIndex, fill, size)), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tableIndex);
    UNUSED_PARAM(fill);
    UNUSED_PARAM(size);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL_1P(current_memory)
{
#if CPU(ARM64) || CPU(X86_64)
    size_t size = instance->memory()->handle().size() >> 16;
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

WASM_IPINT_EXTERN_CPP_DECL(table_copy, int32_t* metadata, int32_t dst, int64_t srcAndCount)
{
#if CPU(ARM64) || CPU(X86_64)
    if (!Wasm::tableCopy(instance, metadata[0], metadata[1], dst, srcAndCount >> 32, srcAndCount & 0xffffffff))
        WASM_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), bitwise_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(metadata);
    UNUSED_PARAM(dst);
    UNUSED_PARAM(srcAndCount);
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

static inline UGPRPair doWasmCall(Wasm::Instance* instance, unsigned functionIndex)
{
    uint32_t importFunctionCount = instance->module().moduleInformation().importFunctionCount();

    CodePtr<WasmEntryPtrTag> codePtr;

    if (functionIndex < importFunctionCount) {
        Wasm::Instance::ImportFunctionInfo* functionInfo = instance->importFunctionInfo(functionIndex);
        codePtr = functionInfo->importFunctionStub;
    } else {
        // Target is a wasm function within the same instance
        codePtr = *instance->calleeGroup()->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
    }

    WASM_CALL_RETURN(instance, codePtr.taggedPtr(), WasmEntryPtrTag);
}

WASM_IPINT_EXTERN_CPP_DECL(call, unsigned functionIndex)
{
    return doWasmCall(instance, functionIndex);
}

WASM_IPINT_EXTERN_CPP_DECL(call_indirect, CallFrame* callFrame, unsigned functionIndex, unsigned* metadataEntry)
{
    unsigned tableIndex = metadataEntry[0];
    unsigned typeIndex = metadataEntry[1];
    Wasm::FuncRefTable* table = instance->table(tableIndex)->asFuncrefTable();

    if (functionIndex >= table->length())
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsCallIndirect);

    const Wasm::FuncRefTable::Function& function = table->function(functionIndex);

    if (function.m_function.typeIndex == Wasm::TypeDefinition::invalidIndex)
        WASM_THROW(Wasm::ExceptionType::NullTableEntry);

    const auto& callSignature = static_cast<Wasm::IPIntCallee*>(callFrame->callee().asNativeCallee())->signature(typeIndex);
    if (callSignature.index() != function.m_function.typeIndex)
        WASM_THROW(Wasm::ExceptionType::BadSignature);

    WASM_CALL_RETURN(function.m_instance, function.m_function.entrypointLoadLocation->taggedPtr(), WasmEntryPtrTag);
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

