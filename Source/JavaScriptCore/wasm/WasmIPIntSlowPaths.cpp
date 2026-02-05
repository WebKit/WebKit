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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(WEBASSEMBLY)

#include "BytecodeStructs.h"
#include "FrameTracers.h"
#include "JITExceptions.h"
#include "JSWebAssemblyArrayInlines.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "WasmBBQPlan.h"
#include "WasmBaselineData.h"
#include "WasmCallProfile.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmDebugServer.h"
#include "WasmExecutionHandler.h"
#include "WasmIPIntGenerator.h"
#include "WasmModuleInformation.h"
#include "WasmOSREntryPlan.h"
#include "WasmOperationsInlines.h"
#include "WasmTypeDefinitionInlines.h"
#include "WasmWorklist.h"
#include "WebAssemblyFunction.h"
#include <bit>
#include <wtf/LEBDecoder.h>

namespace JSC { namespace IPInt {

#define WASM_RETURN_TWO(first, second) do { \
        return encodeResult(first, second); \
    } while (false)

#define WASM_CALL_RETURN(targetInstance, callTarget) do { \
        static_assert(callTarget.getTag() == WasmEntryPtrTag); \
        callTarget.validate(); \
        WASM_RETURN_TWO(callTarget.taggedPtr(), targetInstance); \
    } while (false)

#define IPINT_CALLEE(callFrame) \
    (uncheckedDowncast<Wasm::IPIntCallee>(uncheckedDowncast<Wasm::Callee>(callFrame->callee().asNativeCallee())))

#if ENABLE(WEBASSEMBLY_DEBUGGER)
// Sets a breakpoint at the callee entry when stepping into a call.
// Should Call this before WASM_CALL_RETURN in prepare_call* functions.
#define IPINT_HANDLE_STEP_INTO_CALL(callerVM, boxedCallee, calleeInstance) do { \
        if (Options::enableWasmDebugger()) [[unlikely]] { \
            Wasm::DebugServer& debugServer = Wasm::DebugServer::singleton(); \
            if (debugServer.isConnected()) \
                debugServer.execution().setStepIntoBreakpointForCall((callerVM), (boxedCallee), (calleeInstance)); \
        } \
    } while (false)

// Sets a breakpoint at the exception handler when stepping into a throw.
// Should Call this after genericUnwind() in throw/rethrow/throw_ref functions.
#define IPINT_HANDLE_STEP_INTO_THROW(throwVM) do { \
        if (Options::enableWasmDebugger()) [[unlikely]] { \
            Wasm::DebugServer& debugServer = Wasm::DebugServer::singleton(); \
            if (debugServer.isConnected()) \
                debugServer.execution().setStepIntoBreakpointForThrow((throwVM)); \
        } \
    } while (false)
#else
#define IPINT_HANDLE_STEP_INTO_CALL(callerVM, boxedCallee, calleeInstance) do { \
        UNUSED_PARAM(callerVM); \
        UNUSED_PARAM(boxedCallee); \
        UNUSED_PARAM(calleeInstance); \
    } while (false)
#define IPINT_HANDLE_STEP_INTO_THROW(throwVM) do { \
        UNUSED_PARAM(throwVM); \
    } while (false)
#endif


// For operation calls that may throw an exception, we return (<val>, 0)
// if it is fine, and (<exception value>, SlowPathExceptionTag) if it is not

#define EXCEPTION_VALUE(type) \
    std::bit_cast<void*>(static_cast<uintptr_t>(type))

#define IPINT_THROW(type) \
    WASM_RETURN_TWO(EXCEPTION_VALUE(type), std::bit_cast<void*>(SlowPathExceptionTag))

#define IPINT_END() WASM_RETURN_TWO(0, 0);

#if CPU(ADDRESS64)
#define IPINT_RETURN(value) \
    WASM_RETURN_TWO(std::bit_cast<void*>(value), 0);
#else
#define IPINT_RETURN(value) \
    WASM_RETURN_TWO(std::bit_cast<void*>(JSValue::decode(value).payload()), std::bit_cast<void*>(JSValue::decode(value).tag()));
#endif

#if ENABLE(WEBASSEMBLY_BBQJIT)

static inline bool shouldJIT(Wasm::IPIntCallee* callee)
{
    if (!Options::useBBQJIT() || !Wasm::BBQPlan::ensureGlobalBBQAllowlist().containsWasmFunction(callee->functionIndex()))
        return false;
    if (!Options::wasmFunctionIndexRangeToCompile().isInRange(callee->functionIndex()))
        return false;
    return true;
}

enum class OSRFor { Prologue, Epilogue, Loop };

static inline RefPtr<Wasm::JITCallee> jitCompileAndSetHeuristics(Wasm::IPIntCallee& callee, JSWebAssemblyInstance* instance, OSRFor osrFor)
{
    Wasm::IPIntTierUpCounter& tierUpCounter = callee.tierUpCounter();
    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        return nullptr;
    }

    MemoryMode memoryMode = instance->memory()->mode();
    Wasm::CalleeGroup& calleeGroup = *instance->calleeGroup();
    ASSERT(instance->memoryMode() == memoryMode);
    ASSERT(memoryMode == calleeGroup.mode());

    Wasm::FunctionCodeIndex functionIndex = callee.functionIndex();
    const Wasm::ModuleInformation& moduleInformation = instance->module().moduleInformation();
    bool needsSIMDReplacement = !Options::useWasmIPIntSIMD() && moduleInformation.usesSIMD(functionIndex);

    auto getReplacement = [&] () -> RefPtr<Wasm::JITCallee> {
        switch (osrFor) {
        case OSRFor::Prologue: {
            if (!Options::useWasmIPInt() || needsSIMDReplacement) [[unlikely]]
                return calleeGroup.tryGetReplacementConcurrently(functionIndex);
            return nullptr;
        }
        case OSRFor::Epilogue: {
            return nullptr;
        }
        case OSRFor::Loop: {
            return calleeGroup.tryGetBBQCalleeForLoopOSRConcurrently(instance->vm(), functionIndex);
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    };

    if (RefPtr replacement = getReplacement()) {
        dataLogLnIf(Options::verboseOSR(), "    Code was already compiled.");
        // FIXME: This should probably be some optimizeNow() for calls or checkIfOptimizationThresholdReached() should have a different threshold for calls.
        tierUpCounter.optimizeSoon();
        return replacement;
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
        case Wasm::IPIntTierUpCounter::CompilationStatus::Failed:
            return nullptr;
        }
    }

    if (compile) {
        if (Wasm::BBQPlan::ensureGlobalBBQAllowlist().containsWasmFunction(functionIndex)) {
            auto plan = Wasm::BBQPlan::create(instance->vm(), const_cast<Wasm::ModuleInformation&>(moduleInformation), functionIndex, Ref { callee }, Ref { instance->module() }, Ref(*instance->calleeGroup()), Wasm::Plan::dontFinalize());
            Wasm::ensureWorklist().enqueue(plan.get());
            if (!Options::useConcurrentJIT() || !Options::useWasmIPInt() || needsSIMDReplacement) [[unlikely]]
                plan->waitForCompletion();
            else
                tierUpCounter.optimizeAfterWarmUp();
        }
    }

    return getReplacement();
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

    if (RefPtr replacement = jitCompileAndSetHeuristics(*callee, instance, OSRFor::Prologue)) {
        instance->ensureBaselineData(callee->functionIndex());
        WASM_RETURN_TWO(replacement->entrypoint().taggedPtr(), nullptr);
    }
    WASM_RETURN_TWO(nullptr, nullptr);
}

// This needs to be kept in sync with BBQJIT::makeStackMap.
static ALWAYS_INLINE Wasm::Context::ScratchBufferEntry* buildEntryBufferForLoopOSR(Wasm::IPIntCallee* ipintCallee, Wasm::BBQCallee* bbqCallee, JSWebAssemblyInstance* instance, const Wasm::IPIntTierUpCounter::OSREntryData& osrEntryData, IPIntLocal* pl)
{
    ASSERT(bbqCallee->compilationMode() == Wasm::CompilationMode::BBQMode);
    size_t osrEntryScratchBufferSize = bbqCallee->osrEntryScratchBufferSize();

    RELEASE_ASSERT(osrEntryScratchBufferSize >= ipintCallee->numLocals() + osrEntryData.numberOfStackValues + osrEntryData.tryDepth + Wasm::BBQCallee::extraOSRValuesForLoopIndex);

    auto* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryScratchBufferSize);
    if (!buffer)
        return nullptr;
    auto* currentEntry = buffer;
    auto copyValueToBuffer = [&](const IPIntLocal& local) ALWAYS_INLINE_LAMBDA {
        *std::bit_cast<v128_t*>(currentEntry++) = local.v128;
    };

    // The loop index isn't really an IPIntLocal value, but it occupies the first slot of the OSR scratch buffer
    IPIntLocal loopIndexLocal = { };
    loopIndexLocal.v128.u64x2[0] = osrEntryData.loopIndex;
    loopIndexLocal.v128.u64x2[1] = 0;
    copyValueToBuffer(loopIndexLocal);

    for (uint32_t i = 0; i < ipintCallee->numLocals(); ++i)
        copyValueToBuffer(pl[i]);

    if (ipintCallee->rethrowSlots()) {
        ASSERT(osrEntryData.tryDepth <= ipintCallee->rethrowSlots());
        for (uint32_t i = 0; i < osrEntryData.tryDepth; ++i)
            copyValueToBuffer(pl[ipintCallee->localSizeToAlloc() + i]);
    } else {
        // If there's no rethrow slots just 0 fill the buffer.
        IPIntLocal zeroValue = { };
        zeroValue.v128 = vectorAllZeros();
        for (uint32_t i = 0; i < osrEntryData.tryDepth; ++i)
            copyValueToBuffer(zeroValue);
    }

    for (uint32_t i = 0; i < osrEntryData.numberOfStackValues; ++i) {
        pl -= 1;
        copyValueToBuffer(*pl);
    }
    return buffer;
}


WASM_IPINT_EXTERN_CPP_DECL(loop_osr, CallFrame* callFrame, uint8_t* pc, IPIntLocal* pl)
{
    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    Wasm::IPIntTierUpCounter& tierUpCounter = callee->tierUpCounter();

    if (!Options::useWasmOSR() || !Options::useWasmIPIntLoopOSR() || !shouldJIT(callee)) {
        ipint_extern_prologue_osr(instance, callFrame);
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered loop_osr with tierUpCounter = ", callee->tierUpCounter());

    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    unsigned loopOSREntryBytecodeOffset = pc - callee->bytecode();
    const auto& osrEntryData = tierUpCounter.osrEntryDataForLoop(loopOSREntryBytecodeOffset);

    if (!Options::useBBQJIT())
        WASM_RETURN_TWO(nullptr, nullptr);
    RefPtr compiledCallee = jitCompileAndSetHeuristics(*callee, instance, OSRFor::Loop);
    if (!compiledCallee)
        WASM_RETURN_TWO(nullptr, nullptr);

    auto* bbqCallee = uncheckedDowncast<Wasm::BBQCallee>(compiledCallee.get());
    ASSERT(bbqCallee->compilationMode() == Wasm::CompilationMode::BBQMode);

    auto* buffer = buildEntryBufferForLoopOSR(callee, bbqCallee, instance, osrEntryData, pl);
    if (!buffer)
        WASM_RETURN_TWO(nullptr, nullptr);

    auto sharedLoopEntrypoint = bbqCallee->sharedLoopEntrypoint();
    RELEASE_ASSERT(sharedLoopEntrypoint);

    instance->ensureBaselineData(callee->functionIndex());
    WASM_RETURN_TWO(buffer, sharedLoopEntrypoint->taggedPtr());
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

    jitCompileAndSetHeuristics(*callee, instance, OSRFor::Epilogue);
    WASM_RETURN_TWO(nullptr, nullptr);
}
#endif

static void copyExceptionStackToPayload(const Wasm::FunctionSignature& tagType, const IPIntStackEntry* stackPointer, FixedVector<uint64_t>& payload)
{
    unsigned payloadIndex = payload.size();
    for (unsigned i = 0; i < tagType.argumentCount(); ++i) {
        unsigned argIndex = tagType.argumentCount() - i - 1;
        if (tagType.argumentType(argIndex).isV128()) {
            payload[--payloadIndex] = stackPointer[i].v128.u64x2[1];
            payload[--payloadIndex] = stackPointer[i].v128.u64x2[0];
        } else
            payload[--payloadIndex] = stackPointer[i].i64;
    }
    ASSERT(!payloadIndex);
}

static void copyExceptionPayloadToStack(const Wasm::FunctionSignature& tagType, const FixedVector<uint64_t>& payload, IPIntStackEntry* stackPointer)
{
    unsigned payloadIndex = payload.size();
    for (unsigned i = 0; i < tagType.argumentCount(); ++i) {
        unsigned argIndex = tagType.argumentCount() - i - 1;
        if (tagType.argumentType(argIndex).isV128()) {
            stackPointer[i].v128.u64x2[1] = payload[--payloadIndex];
            stackPointer[i].v128.u64x2[0] = payload[--payloadIndex];
        } else
            stackPointer[i].i64 = payload[--payloadIndex];
    }
    ASSERT(!payloadIndex);
}

WASM_IPINT_EXTERN_CPP_DECL(retrieve_and_clear_exception, CallFrame* callFrame, IPIntStackEntry* stackPointer, IPIntLocal* pl)
{
    VM& vm = instance->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!!throwScope.exception());

    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    if (callee->rethrowSlots()) {
        RELEASE_ASSERT(vm.targetTryDepthForThrow <= callee->rethrowSlots());
        pl[callee->localSizeToAlloc() + vm.targetTryDepthForThrow - 1].i64 = std::bit_cast<uint64_t>(throwScope.exception()->value());
    }

    if (stackPointer) {
        // We only have a stack pointer if we're doing a catch not a catch_all
        Exception* exception = throwScope.exception();
        auto* wasmException = jsSecureCast<JSWebAssemblyException*>(exception->value());
        copyExceptionPayloadToStack(wasmException->tag().type(), wasmException->payload(), stackPointer);
    }

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    (void)throwScope.tryClearException();

    WASM_RETURN_TWO(nullptr, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(retrieve_clear_and_push_exception, CallFrame* callFrame, IPIntStackEntry* stackPointer, IPIntLocal* pl)
{
    VM& vm = instance->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!!throwScope.exception());

    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    if (callee->rethrowSlots()) {
        RELEASE_ASSERT(vm.targetTryDepthForThrow <= callee->rethrowSlots());
        pl[callee->localSizeToAlloc() + vm.targetTryDepthForThrow - 1].i64 = std::bit_cast<uint64_t>(throwScope.exception()->value());
    }

    Exception* exception = throwScope.exception();
    stackPointer[0].ref = JSValue::encode(exception->value());

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    (void)throwScope.tryClearException();

    WASM_RETURN_TWO(nullptr, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(retrieve_clear_and_push_exception_and_arguments, CallFrame* callFrame, IPIntStackEntry* stackPointer, IPIntLocal* pl)
{
    VM& vm = instance->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!!throwScope.exception());

    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    if (callee->rethrowSlots()) {
        RELEASE_ASSERT(vm.targetTryDepthForThrow <= callee->rethrowSlots());
        pl[callee->localSizeToAlloc() + vm.targetTryDepthForThrow - 1].i64 = std::bit_cast<uint64_t>(throwScope.exception()->value());
    }

    Exception* exception = throwScope.exception();
    auto* wasmException = jsSecureCast<JSWebAssemblyException*>(exception->value());

    ASSERT(wasmException->payload().size() == wasmException->tag().parameterBufferSize());

    stackPointer[0].ref = JSValue::encode(exception->value());
    copyExceptionPayloadToStack(wasmException->tag().type(), wasmException->payload(), stackPointer + 1);

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    (void)throwScope.tryClearException();

    WASM_RETURN_TWO(nullptr, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(throw_exception, CallFrame* callFrame, IPIntStackEntry* arguments, unsigned exceptionIndex)
{
    VM& vm = instance->vm();
    SlowPathFrameTracer tracer(vm, callFrame);

    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!throwScope.exception());

    JSGlobalObject* globalObject = instance->globalObject();
    Ref<const Wasm::Tag> tag = instance->tag(exceptionIndex);

    FixedVector<uint64_t> values(tag->parameterBufferSize());
    copyExceptionStackToPayload(tag->type(), arguments, values);

    ASSERT(tag->type().returnsVoid());
    JSWebAssemblyException* exception = JSWebAssemblyException::create(vm, globalObject->webAssemblyExceptionStructure(), WTF::move(tag), WTF::move(values));
    throwException(globalObject, throwScope, exception);

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);

    IPINT_HANDLE_STEP_INTO_THROW(vm);
    WASM_RETURN_TWO(vm.targetMachinePCForThrow, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(rethrow_exception, CallFrame* callFrame, IPIntStackEntry* pl, unsigned tryDepth)
{
    SlowPathFrameTracer tracer(instance->vm(), callFrame);

    JSGlobalObject* globalObject = instance->globalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    RELEASE_ASSERT(tryDepth <= callee->rethrowSlots());
#if CPU(ADDRESS64)
    JSWebAssemblyException* exception = std::bit_cast<JSWebAssemblyException*>(pl[callee->localSizeToAlloc() + tryDepth - 1].i64);
#else
    JSWebAssemblyException* exception = std::bit_cast<JSWebAssemblyException*>(pl[callee->localSizeToAlloc() + tryDepth - 1].i32);
#endif
    RELEASE_ASSERT(exception);
    throwException(globalObject, throwScope, exception);

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);

    IPINT_HANDLE_STEP_INTO_THROW(vm);
    WASM_RETURN_TWO(vm.targetMachinePCForThrow, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(throw_ref, CallFrame* callFrame, EncodedJSValue exnref)
{
    SlowPathFrameTracer tracer(instance->vm(), callFrame);

    JSGlobalObject* globalObject = instance->globalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto* exception = jsSecureCast<JSWebAssemblyException*>(JSValue::decode(exnref));
    RELEASE_ASSERT(exception);
    throwException(globalObject, throwScope, exception);

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);

    IPINT_HANDLE_STEP_INTO_THROW(vm);
    WASM_RETURN_TWO(vm.targetMachinePCForThrow, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(table_get, unsigned tableIndex, unsigned index)
{
    EncodedJSValue result = Wasm::tableGet(instance, tableIndex, index);
    if (!result)
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    IPINT_RETURN(result);
}

WASM_IPINT_EXTERN_CPP_DECL(table_set, unsigned tableIndex, unsigned index, EncodedJSValue value)
{
    if (!Wasm::tableSet(instance, tableIndex, index, value))
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(table_init, IPIntStackEntry* sp, TableInitMetadata* metadata)
{
    int32_t n = sp[0].i32;
    int32_t src = sp[1].i32;
    int32_t dst = sp[2].i32;

    if (!Wasm::tableInit(instance, metadata->elementIndex, metadata->tableIndex, dst, src, n))
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(table_fill, IPIntStackEntry* sp, TableFillMetadata* metadata)
{
    int32_t n = sp[0].i32;
    EncodedJSValue fill = sp[1].ref;
    int32_t offset = sp[2].i32;

    if (!Wasm::tableFill(instance, metadata->tableIndex, offset, fill, n))
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(table_grow, IPIntStackEntry* sp, TableGrowMetadata* metadata)
{
    int32_t n = sp[0].i32;
    EncodedJSValue fill = sp[1].ref;

    WASM_RETURN_TWO(std::bit_cast<void*>(Wasm::tableGrow(instance, metadata->tableIndex, fill, n)), 0);
}

WASM_IPINT_EXTERN_CPP_DECL(memory_grow, int64_t delta)
{
    WASM_RETURN_TWO(reinterpret_cast<void*>(Wasm::growMemory(instance, delta)), 0);
}

WASM_IPINT_EXTERN_CPP_DECL(memory_init, int32_t dataIndex, IPIntStackEntry* sp)
{
    int32_t n = sp[0].i32;
    int32_t s = sp[1].i32;
    int64_t d = sp[2].i64;

    if (!Wasm::memoryInit(instance, dataIndex, d, s, n))
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(data_drop, int32_t dataIndex)
{
    Wasm::dataDrop(instance, dataIndex);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(memory_copy, int64_t dst, int64_t src, int64_t count)
{
    if (!Wasm::memoryCopy(instance, dst, src, count))
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(memory_fill, int64_t dst, int32_t targetValue, int64_t count)
{
    if (!Wasm::memoryFill(instance, dst, targetValue, count))
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(elem_drop, int32_t dataIndex)
{
    Wasm::elemDrop(instance, dataIndex);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(table_copy, IPIntStackEntry* sp, TableCopyMetadata* metadata)
{
    int32_t n = sp[0].i32;
    int32_t src = sp[1].i32;
    int32_t dst = sp[2].i32;

    if (!Wasm::tableCopy(instance, metadata->dstTableIndex, metadata->srcTableIndex, dst, src, n))
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(table_size, int32_t tableIndex)
{
    int32_t result = Wasm::tableSize(instance, tableIndex);
    WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<size_t>(result)), 0);
}

// Wasm-GC
WASM_IPINT_EXTERN_CPP_DECL(struct_new, uint32_t type, IPIntStackEntry* sp)
{
    WasmSlowPathWithoutCallFrameTracer tracer(instance->vm());
    WebAssemblyGCStructure* structure = instance->gcObjectStructure(type);
    JSValue result = Wasm::structNew(instance, structure, false, sp);
    if (result.isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadStructNew);
    IPINT_RETURN(JSValue::encode(result));
}

WASM_IPINT_EXTERN_CPP_DECL(struct_new_default, uint32_t type)
{
    WasmSlowPathWithoutCallFrameTracer tracer(instance->vm());
    WebAssemblyGCStructure* structure = instance->gcObjectStructure(type);
    JSValue result = Wasm::structNew(instance, structure, true, nullptr);
    if (result.isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadStructNew);
    IPINT_RETURN(JSValue::encode(result));
}

WASM_IPINT_EXTERN_CPP_DECL(struct_get, EncodedJSValue object, uint32_t fieldIndex, IPIntStackEntry* result)
{
    UNUSED_PARAM(instance);
    if (JSValue::decode(object).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullAccess);

    Wasm::structGet(object, fieldIndex, result);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(struct_get_s, EncodedJSValue object, uint32_t fieldIndex, IPIntStackEntry* result)
{
    UNUSED_PARAM(instance);
    if (JSValue::decode(object).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullAccess);

    Wasm::structGet(object, fieldIndex, result);

    // sign extension
    JSWebAssemblyStruct* structObject = jsCast<JSWebAssemblyStruct*>(JSValue::decode(object).getObject());
    Wasm::StorageType type = structObject->fieldType(fieldIndex).type;
    ASSERT(type.is<Wasm::PackedType>());
    size_t elementSize = type.as<Wasm::PackedType>() == Wasm::PackedType::I8 ? sizeof(uint8_t) : sizeof(uint16_t);
    uint8_t bitShift = (sizeof(uint32_t) - elementSize) * 8;
    int32_t value = static_cast<int32_t>(result->i64);
    value = value << bitShift;

    result->i64 = static_cast<EncodedJSValue>(value >> bitShift);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(struct_set, EncodedJSValue object, uint32_t fieldIndex, IPIntStackEntry* sp)
{
    UNUSED_PARAM(instance);
    if (JSValue::decode(object).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullAccess);
    Wasm::structSet(object, fieldIndex, sp);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(array_new, uint32_t type, uint32_t size, IPIntStackEntry* defaultValue)
{
    WasmSlowPathWithoutCallFrameTracer tracer(instance->vm());
    WebAssemblyGCStructure* structure = instance->gcObjectStructure(type);
    const Wasm::TypeDefinition& arraySignature = structure->typeDefinition();
    Wasm::StorageType elementType = arraySignature.as<Wasm::ArrayType>()->elementType().type;

    JSValue result;
    if (elementType.unpacked().isV128())
        result = Wasm::arrayNew(instance, structure, size, defaultValue->v128);
    else
        result = Wasm::arrayNew(instance, structure, size, defaultValue->i64);
    if (result.isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadArrayNew);
    IPINT_RETURN(JSValue::encode(result));
}

WASM_IPINT_EXTERN_CPP_DECL(array_new_default, uint32_t type, uint32_t size)
{
    WasmSlowPathWithoutCallFrameTracer tracer(instance->vm());
    UNUSED_PARAM(instance);
    WebAssemblyGCStructure* structure = instance->gcObjectStructure(type);
    const Wasm::TypeDefinition& arraySignature = structure->typeDefinition();
    Wasm::StorageType elementType = arraySignature.as<Wasm::ArrayType>()->elementType().type;
    EncodedJSValue defaultValue = 0;

    if (Wasm::isRefType(elementType)) {
        defaultValue = JSValue::encode(jsNull());
    } else if (elementType.unpacked().isV128()) {
        JSValue result = Wasm::arrayNew(instance, structure, size, vectorAllZeros());
        if (result.isNull()) [[unlikely]]
            IPINT_THROW(Wasm::ExceptionType::BadArrayNew);
        IPINT_RETURN(JSValue::encode(result));
    }

    JSValue result = Wasm::arrayNew(instance, structure, size, defaultValue);
    if (result.isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadArrayNew);
    IPINT_RETURN(JSValue::encode(result));
}

WASM_IPINT_EXTERN_CPP_DECL(array_new_fixed, uint32_t type, uint32_t size, IPIntStackEntry* arguments)
{
    WasmSlowPathWithoutCallFrameTracer tracer(instance->vm());
    WebAssemblyGCStructure* structure = instance->gcObjectStructure(type);

    JSValue result = Wasm::arrayNewFixed(instance, structure, size, arguments);
    if (result.isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadArrayNew);

    IPINT_RETURN(JSValue::encode(result));
}

WASM_IPINT_EXTERN_CPP_DECL(array_new_data, IPInt::ArrayNewDataMetadata* metadata, uint32_t offset, uint32_t size)
{
    WasmSlowPathWithoutCallFrameTracer tracer(instance->vm());
    EncodedJSValue result = Wasm::arrayNewData(instance, metadata->type, metadata->dataSegmentIndex, size, offset);
    if (JSValue::decode(result).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadArrayNewInitData);

    IPINT_RETURN(result);
}

WASM_IPINT_EXTERN_CPP_DECL(array_new_elem, IPInt::ArrayNewElemMetadata* metadata, uint32_t offset, uint32_t size)
{
    WasmSlowPathWithoutCallFrameTracer tracer(instance->vm());
    EncodedJSValue result = Wasm::arrayNewElem(instance, metadata->type, metadata->elemSegmentIndex, size, offset);
    if (JSValue::decode(result).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadArrayNewInitElem);

    IPINT_RETURN(result);
}

WASM_IPINT_EXTERN_CPP_DECL(array_get, uint32_t type, IPIntStackEntry* sp)
{
    // sp[1] = array / result
    // sp[0] = index (i32)

    EncodedJSValue array = sp[1].ref;
    uint32_t index = sp[0].i32;
    IPIntStackEntry* result = &sp[1];

    if (JSValue::decode(array).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullAccess);
    JSValue arrayValue = JSValue::decode(array);
    ASSERT(arrayValue.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayValue.getObject());
    if (index >= arrayObject->size()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsArrayGet);
    Wasm::arrayGet(instance, type, array, index, result);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(array_get_s, uint32_t type, IPIntStackEntry* sp)
{
    // sp[1] = array / result
    // sp[0] = index (i32)

    EncodedJSValue array = sp[1].ref;
    uint32_t index = sp[0].i32;
    IPIntStackEntry* result = &sp[1];

    if (JSValue::decode(array).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullAccess);
    JSValue arrayValue = JSValue::decode(array);
    ASSERT(arrayValue.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayValue.getObject());
    if (index >= arrayObject->size()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsArrayGet);

    Wasm::arrayGet(instance, type, array, index, result);

    // sign extension
    Wasm::StorageType elementType = arrayObject->elementType().type;
    ASSERT(elementType.is<Wasm::PackedType>());
    size_t elementSize = elementType.as<Wasm::PackedType>() == Wasm::PackedType::I8 ? sizeof(uint8_t) : sizeof(uint16_t);
    uint8_t bitShift = (sizeof(uint32_t) - elementSize) * 8;
    int32_t value = static_cast<int32_t>(result->i64);
    value = value << bitShift;

    result->i64 = static_cast<EncodedJSValue>(value >> bitShift);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(array_set, uint32_t type, IPIntStackEntry* sp)
{
    // sp[0] = value
    // sp[1] = index
    // sp[2] = array ref
    if (JSValue::decode(sp[2].ref).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullAccess);

    JSValue arrayValue = JSValue::decode(sp[2].ref);
    ASSERT(arrayValue.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayValue.getObject());
    uint32_t index = static_cast<uint32_t>(sp[1].i32);

    if (index >= arrayObject->size()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsArraySet);

    Wasm::arraySet(instance, type, sp[2].ref, index, &sp[0]);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(array_fill, IPIntStackEntry* sp)
{
    // sp[0] = size
    // sp[1] = value
    // sp[2] = offset
    // sp[3] = array

    EncodedJSValue arrayref = sp[3].ref;
    JSValue arrayValue = JSValue::decode(arrayref);
    if (arrayValue.isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullArrayFill);

    ASSERT(arrayValue.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayValue.getObject());

    uint32_t offset = sp[2].i32;
    IPIntStackEntry* value = &sp[1];
    uint32_t size = sp[0].i32;

    bool success;
    if (arrayObject->elementType().type.unpacked().isV128())
        success = Wasm::arrayFill(instance->vm(), arrayref, offset, value->v128, size);
    else
        success = Wasm::arrayFill(instance->vm(), arrayref, offset, value->i64, size);

    if (!success) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsArrayFill);

    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(array_copy, IPIntStackEntry* sp)
{
    // sp[0] = size
    // sp[1] = src_offset
    // sp[2] = src
    // sp[3] = dest_offset
    // sp[4] = dest

    EncodedJSValue dst = sp[4].ref;
    uint32_t dstOffset = sp[3].i32;
    EncodedJSValue src = sp[2].ref;
    uint32_t srcOffset = sp[1].i32;
    uint32_t size = sp[0].i32;

    if (JSValue::decode(dst).isNull() || JSValue::decode(src).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullArrayCopy);

    if (!Wasm::arrayCopy(instance, dst, dstOffset, src, srcOffset, size)) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsArrayCopy);
    IPINT_END();
}

namespace {
ASCIILiteral opcodeName(uint8_t* pc)
{
    Wasm::OpType opcode = static_cast<Wasm::OpType>(*pc);

    size_t extOffset = 1;
    switch (opcode) {
    case Wasm::OpType::ExtGC: {
        unsigned extOpcode;
        if (!WTF::LEBDecoder::decodeUInt(unsafeMakeSpan(pc, 2), extOffset, extOpcode))
            return "! bad gc opcode !"_s;
        return makeString(static_cast<Wasm::ExtGCOpType>(extOpcode));
    }
    case Wasm::OpType::Ext1: {
        unsigned extOpcode;
        if (!WTF::LEBDecoder::decodeUInt(unsafeMakeSpan(pc, 2), extOffset, extOpcode))
            return "! bad ext1 opcode !"_s;
        return makeString(static_cast<Wasm::Ext1OpType>(extOpcode));
    }
    case Wasm::OpType::ExtSIMD: {
        unsigned extOpcode;
        if (!WTF::LEBDecoder::decodeUInt(unsafeMakeSpan(pc, 3), extOffset, extOpcode))
            return "! bad simd opcode !"_s;
        return makeString(static_cast<Wasm::ExtSIMDOpType>(extOpcode));
    }
    case Wasm::OpType::ExtAtomic: {
        unsigned extOpcode;
        if (!WTF::LEBDecoder::decodeUInt(unsafeMakeSpan(pc, 2), extOffset, extOpcode))
            return "! bad atomic opcode !"_s;
        return makeString(static_cast<Wasm::ExtAtomicOpType>(extOpcode));
    }
    default:
        return makeString(opcode);
    }
}
} // namespace

WASM_IPINT_EXTERN_CPP_DECL(trace, CallFrame* callFrame, uint8_t* pc, uint8_t* mc)
{
    if (!Options::traceWasmIPIntExecution())
        IPINT_END();

    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    dataLogF("<%p> %p%s / %p: executing +0x%x, %s, pc = %p, mc = %p\n",
        &Thread::currentSingleton(),
        instance,
        makeString(callee->indexOrName()).ascii().data(),
        callFrame,
        static_cast<uint32_t>(pc - callee->bytecode()),
        opcodeName(pc).characters(),
        pc, mc);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(array_init_data, uint32_t dataIndex, IPIntStackEntry* sp)
{
    // sp[0] = size
    // sp[1] = src_offset
    // sp[2] = dst_offset
    // sp[3] = dst

    EncodedJSValue dst = sp[3].ref;
    uint32_t dstOffset = sp[2].i32;
    uint32_t srcOffset = sp[1].i32;
    uint32_t size = sp[0].i32;

    if (JSValue::decode(dst).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullArrayInitData);
    if (!Wasm::arrayInitData(instance, dst, dstOffset, dataIndex, srcOffset, size)) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsArrayInitData);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(array_init_elem, uint32_t elemIndex, IPIntStackEntry* sp)
{
    // sp[0] = size
    // sp[1] = src_offset
    // sp[2] = dst_offset
    // sp[3] = dst

    EncodedJSValue dst = sp[3].ref;
    uint32_t dstOffset = sp[2].i32;
    uint32_t srcOffset = sp[1].i32;
    uint32_t size = sp[0].i32;

    if (JSValue::decode(dst).isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullArrayInitElem);
    if (!Wasm::arrayInitElem(instance, dst, dstOffset, elemIndex, srcOffset, size)) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::OutOfBoundsArrayInitElem);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(any_convert_extern, EncodedJSValue value)
{
    UNUSED_PARAM(instance);
    IPINT_RETURN(Wasm::externInternalize(value));
}

WASM_IPINT_EXTERN_CPP_DECL(ref_test, int32_t heapType, bool allowNull, EncodedJSValue value)
{
    if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(heapType))) {
        bool result = Wasm::refCast(value, allowNull, static_cast<Wasm::TypeIndex>(heapType), nullptr);
        IPINT_RETURN(static_cast<uint64_t>(result));
    }

    auto& info = instance->module().moduleInformation();
    bool result = Wasm::refCast(value, allowNull, info.typeSignatures[heapType]->index(), info.rtts[heapType].ptr());
    IPINT_RETURN(static_cast<uint64_t>(result));
}

WASM_IPINT_EXTERN_CPP_DECL(ref_cast, int32_t heapType, bool allowNull, EncodedJSValue value)
{
    if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(heapType))) {
        if (!Wasm::refCast(value, allowNull, static_cast<Wasm::TypeIndex>(heapType), nullptr)) [[unlikely]]
            IPINT_THROW(Wasm::ExceptionType::CastFailure);
        IPINT_RETURN(value);
    }

    auto& info = instance->module().moduleInformation();
    if (!Wasm::refCast(value, allowNull, info.typeSignatures[heapType]->index(), info.rtts[heapType].ptr())) [[unlikely]] {
        if (!allowNull && JSValue::decode(value).isNull())
            IPINT_THROW(Wasm::ExceptionType::NullAccess);
        IPINT_THROW(Wasm::ExceptionType::CastFailure);
    }
    IPINT_RETURN(value);
}

WASM_IPINT_EXTERN_CPP_DECL(prepare_function_body, CallFrame* callFrame)
{
    Wasm::IPIntCallee* callee = IPINT_CALLEE(callFrame);
    instance->ensureBaselineData(callee->functionIndex()).incrementTotalCount();
    WASM_RETURN_TWO(callee, nullptr);
}

/**
 * Given a function index, determine the pointer to its executable code.
 * Return a pair of the wasm instance pointer received as the first argument and the code pointer.
 * Additionally, store the following into the 'calleeAndWasmInstanceReturn':
 *
 *  - calleeAndWasmInstanceReturn[0] - the callee to use, goes into the 'callee' slot of the CallFrame.
 *  - calleeAndWasmInstanceReturn[1] - the wasm instance to use, goes into the 'codeBlock' slot of the CallFrame.
 */
WASM_IPINT_EXTERN_CPP_DECL(prepare_call, CallFrame* callFrame, CallMetadata* call, Register* calleeAndWasmInstanceReturn)
{
    auto* callee = IPINT_CALLEE(callFrame);
    instance->ensureBaselineData(callee->functionIndex()).at(call->callProfileIndex).incrementCount();

    Wasm::FunctionSpaceIndex functionIndex = call->functionIndex;

    uint32_t importFunctionCount = instance->module().moduleInformation().importFunctionCount();

    Register& calleeReturn = calleeAndWasmInstanceReturn[0];
    Register& wasmInstanceReturn = calleeAndWasmInstanceReturn[1];
    CodePtr<WasmEntryPtrTag> codePtr;
    bool isJSCallee = false;
    if (functionIndex < importFunctionCount) {
        auto* functionInfo = instance->importFunctionInfo(functionIndex);
        codePtr = functionInfo->importFunctionStub;
        calleeReturn = functionInfo->boxedCallee.encodedBits();
        if (functionInfo->isJS()) {
            isJSCallee = true;
            wasmInstanceReturn = reinterpret_cast<uintptr_t>(functionInfo);
        } else
            wasmInstanceReturn = functionInfo->targetInstance.get();
    } else {
        // Target is a wasm function within the same instance
        codePtr = *instance->calleeGroup()->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
        auto callee = instance->calleeGroup()->wasmCalleeFromFunctionIndexSpace(functionIndex);
        calleeReturn = CalleeBits::encodeNativeCallee(callee.get());
        wasmInstanceReturn = instance;
    }

    JSWebAssemblyInstance* targetInstance = isJSCallee ? nullptr : jsDynamicCast<JSWebAssemblyInstance*>(wasmInstanceReturn.unboxedCell());
    IPINT_HANDLE_STEP_INTO_CALL(instance->vm(), CalleeBits(calleeReturn.encodedJSValue()), targetInstance);

    RELEASE_ASSERT(WTF::isTaggedWith<WasmEntryPtrTag>(codePtr));

    WASM_CALL_RETURN(instance, codePtr);
}

WASM_IPINT_EXTERN_CPP_DECL(prepare_call_indirect, CallFrame* callFrame, Wasm::FunctionSpaceIndex* functionIndex, CallIndirectMetadata* call)
{
    auto* callee = IPINT_CALLEE(callFrame);
    auto& callProfile = instance->ensureBaselineData(callee->functionIndex()).at(call->callProfileIndex);
    callProfile.incrementCount();

    unsigned tableIndex = call->tableIndex;
    const Wasm::FuncRefTable::Function* function = nullptr;
    if (!tableIndex) {
        if (*functionIndex >= instance->cachedTable0Length()) [[unlikely]]
            IPINT_THROW(Wasm::ExceptionType::OutOfBoundsCallIndirect);
        function = &instance->cachedTable0Buffer()[*functionIndex];
    } else {
        Wasm::FuncRefTable* table = instance->table(tableIndex)->asFuncrefTable();
        if (*functionIndex >= table->length()) [[unlikely]]
            IPINT_THROW(Wasm::ExceptionType::OutOfBoundsCallIndirect);
        function = &table->function(*functionIndex);
    }

    if (!function->m_function.rtt) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadSignature);

    if (!function->m_function.rtt->isSubRTT(*call->rtt)) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::BadSignature);

    auto boxedCallee = function->m_function.boxedCallee.encodedBits();
    Register* calleeReturn = std::bit_cast<Register*>(functionIndex);
    *calleeReturn = boxedCallee;

    Register& functionInfoSlot = calleeReturn[1];
    if (function->m_function.isJS())
        functionInfoSlot = reinterpret_cast<uintptr_t>(jsCast<WebAssemblyFunctionBase*>(function->m_value.get())->callLinkInfo());
    else {
        auto* targetInstance = function->m_function.targetInstance.get();
        functionInfoSlot = targetInstance;
        if (instance != targetInstance)
            callProfile.observeCrossInstanceCall();
        else
            callProfile.observeCallIndirect(boxedCallee);
    }

    IPINT_HANDLE_STEP_INTO_CALL(instance->vm(), function->m_function.boxedCallee, function->m_function.targetInstance.get());

    auto callTarget = *function->m_function.entrypointLoadLocation;
    WASM_CALL_RETURN(function->m_function.targetInstance.get(), callTarget);
}

WASM_IPINT_EXTERN_CPP_DECL(prepare_call_ref, CallFrame* callFrame, CallRefMetadata* call, IPIntStackEntry* sp)
{
    auto* callee = IPINT_CALLEE(callFrame);
    auto& callProfile = instance->ensureBaselineData(callee->functionIndex()).at(call->callProfileIndex);
    callProfile.incrementCount();

    JSValue targetReference = JSValue::decode(sp->ref);

    if (targetReference.isNull()) [[unlikely]]
        IPINT_THROW(Wasm::ExceptionType::NullReference);

    ASSERT(targetReference.isObject());
    JSObject* referenceAsObject = jsCast<JSObject*>(targetReference);

    ASSERT(referenceAsObject->inherits<WebAssemblyFunctionBase>());
    auto* wasmFunction = jsCast<WebAssemblyFunctionBase*>(referenceAsObject);
    auto& function = wasmFunction->importableFunction();
    JSWebAssemblyInstance* calleeInstance = wasmFunction->instance();
    auto boxedCallee = function.boxedCallee.encodedBits();
    sp->ref = boxedCallee;
    Register& functionInfoSlot = std::bit_cast<Register*>(sp)[1];
    if (function.isJS())
        functionInfoSlot = reinterpret_cast<uintptr_t>(wasmFunction->callLinkInfo());
    else {
        auto* targetInstance = function.targetInstance.get();
        functionInfoSlot = targetInstance;
        if (instance != targetInstance)
            callProfile.observeCrossInstanceCall();
        else
            callProfile.observeCallIndirect(boxedCallee);
    }

    IPINT_HANDLE_STEP_INTO_CALL(instance->vm(), function.boxedCallee, calleeInstance);

    auto callTarget = *function.entrypointLoadLocation;
    WASM_CALL_RETURN(calleeInstance, callTarget);
}

WASM_IPINT_EXTERN_CPP_DECL(set_global_ref, uint32_t globalIndex, JSValue value)
{
    instance->setGlobal(globalIndex, value);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(set_global_64, unsigned index, uint64_t value)
{
    instance->setGlobal(index, value);
    IPINT_END();
}

WASM_IPINT_EXTERN_CPP_DECL(get_global_64, unsigned index)
{
#if CPU(ARM64) || CPU(X86_64)
    WASM_RETURN_TWO(std::bit_cast<void*>(instance->loadI64Global(index)), 0);
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
    WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<intptr_t>(result)), nullptr);
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
    WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<intptr_t>(result)), nullptr);
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
    WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<intptr_t>(result)), nullptr);
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
    IPINT_RETURN(Wasm::refFunc(instance, index));
}

extern "C" void SYSV_ABI wasm_log_crash(CallFrame*, JSWebAssemblyInstance* instance)
{
    dataLogLn("Reached IPInt code that should never have been executed.");
    dataLogLn("Module internal function count: ", instance->module().moduleInformation().internalFunctionCount());
    RELEASE_ASSERT_NOT_REACHED();
}

extern "C" UGPRPair SYSV_ABI slow_path_wasm_throw_exception(CallFrame* callFrame, JSWebAssemblyInstance* instance, Wasm::ExceptionType exceptionType)
{
    // FaultPC is the exact PC causing the fault. When using it as a returnPC, we should point one next instruction instead.
    WasmOperationPrologueCallFrameTracer tracer(instance->vm(), callFrame, std::bit_cast<void*>(std::bit_cast<uintptr_t>(instance->faultPC()) + 1));
    instance->setFaultPC(Wasm::ExceptionType::Termination, nullptr);
    WASM_RETURN_TWO(Wasm::throwWasmToJSException(callFrame, exceptionType, instance), nullptr);
}

// Similar logic to 'slow_path_wasm_throw_exception', but the exception is already sitting
// in the VM. We don't throw, we only unwind and go to the handler.
extern "C" UCPURegister SYSV_ABI slow_path_wasm_unwind_exception(CallFrame* callFrame, JSWebAssemblyInstance* instance)
{
    VM& vm = instance->vm();
    // FaultPC is the exact PC causing the fault. When using it as a returnPC, we should point one next instruction instead.
    WasmOperationPrologueCallFrameTracer tracer(instance->vm(), callFrame, std::bit_cast<void*>(std::bit_cast<uintptr_t>(instance->faultPC()) + 1));
    instance->setFaultPC(Wasm::ExceptionType::Termination, nullptr);
    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return reinterpret_cast<UCPURegister>(vm.targetMachinePCForThrow);
}

extern "C" UGPRPair SYSV_ABI slow_path_wasm_popcount(const void* pc, uint32_t x)
{
    void* result = std::bit_cast<void*>(static_cast<size_t>(std::popcount(x)));
    WASM_RETURN_TWO(pc, result);
}

extern "C" UGPRPair SYSV_ABI slow_path_wasm_popcountll(const void* pc, uint64_t x)
{
    void* result = std::bit_cast<void*>(static_cast<size_t>(std::popcount(x)));
    WASM_RETURN_TWO(pc, result);
}

WASM_IPINT_EXTERN_CPP_DECL(check_stack_and_vm_traps, void* candidateNewStackPointer, Wasm::IPIntCallee* callee)
{
    VM& vm = instance->vm();

#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]]
        vm.debugState()->setPrologueStopData(instance, callee);
#else
    UNUSED_PARAM(callee);
#endif

    if (vm.traps().handleTrapsIfNeeded()) {
        if (vm.hasPendingTerminationException())
            IPINT_THROW(Wasm::ExceptionType::Termination);
        ASSERT(!vm.exceptionForInspection());
    }

    // Redo stack check because we may really have gotten here due to an imminent StackOverflow.
    if (vm.softStackLimit() <= candidateNewStackPointer)
        IPINT_RETURN(encodedJSValue()); // No stack overflow. Carry on.

    IPINT_THROW(Wasm::ExceptionType::StackOverflow);
}

#if ENABLE(WEBASSEMBLY_DEBUGGER)
static UNUSED_FUNCTION void displayWasmDebugState(JSWebAssemblyInstance* instance, Wasm::IPIntCallee* callee, IPIntStackEntry* sp, IPIntLocal* pl)
{
    dataLogLn("=== WASM Debug State ===");

    uint32_t numLocals = callee->numLocals();
    dataLogLn("WASM Locals (", numLocals, " entries):");
    auto functionIndex = callee->functionIndex();
    const auto& moduleInfo = instance->module().moduleInformation();
    const Vector<Wasm::Type>& localTypes = moduleInfo.debugInfo->ensureFunctionDebugInfo(functionIndex).locals;
    for (uint32_t i = 0; i < numLocals; ++i)
        logWasmLocalValue(i,  pl[i], localTypes[i]);

    constexpr size_t STACK_ENTRY_SIZE = 16;
    if (sp && pl && sp <= reinterpret_cast<IPIntStackEntry*>(pl)) {
        size_t stackDepth = (reinterpret_cast<uint8_t*>(pl) - reinterpret_cast<uint8_t*>(sp)) / STACK_ENTRY_SIZE;
        dataLogLn("WASM Stack (", stackDepth, " entries - showing all type interpretations):");

        IPIntStackEntry* currentEntry = sp;
        for (size_t i = 0; i < stackDepth; ++i) {
            dataLogLn("  Stack[", i, "]: i32=", currentEntry->i32, ", i64=", currentEntry->i64, ", f32=", currentEntry->f32, ", f64=", currentEntry->f64, ", ref=", currentEntry->ref);
            currentEntry++;
        }
    } else
        dataLogLn("WASM Stack: Invalid stack pointers");
    dataLogLn("=== End WASM Debug State ===");
}
#endif

WASM_IPINT_EXTERN_CPP_DECL(unreachable_breakpoint_handler, CallFrame* callFrame, Register* sp)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][unreachable] Start");
    bool breakpointHandled = false;
#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]] {
        Wasm::DebugServer& debugServer = Wasm::DebugServer::singleton();
        if (debugServer.needToHandleBreakpoints()) {
            uint8_t* pc = static_cast<uint8_t*>(sp[2].pointer());
            uint8_t* mc = static_cast<uint8_t*>(sp[3].pointer());
            IPIntLocal* pl = static_cast<IPIntLocal*>(sp[0].pointer());
            Wasm::IPIntCallee* callee = static_cast<Wasm::IPIntCallee*>(sp[1].pointer());

            IPIntStackEntry* stackPointer = reinterpret_cast<IPIntStackEntry*>(sp + 4);
            if (Options::verboseWasmDebugger())
                displayWasmDebugState(instance, callee, stackPointer, pl);
            breakpointHandled = debugServer.execution().hitBreakpoint(callFrame, instance, callee, pc, mc, pl, stackPointer);
        }
    }
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(callFrame);
    UNUSED_PARAM(sp);
#endif
    dataLogLnIf(Options::verboseWasmDebugger(), "[Code][unreachable] Done with breakpointHandled=", breakpointHandled);
    IPINT_RETURN(static_cast<EncodedJSValue>(static_cast<int32_t>(breakpointHandled)));
}

} } // namespace JSC::IPInt

#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
