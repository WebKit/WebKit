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

#define WASM_CALL_RETURN(targetInstance, callTarget) do { \
        static_assert(callTarget.getTag() == WasmEntryPtrTag); \
        callTarget.validate(); \
        WASM_RETURN_TWO(callTarget.taggedPtr(), targetInstance); \
    } while (false)

#define IPINT_CALLEE(callFrame) \
    static_cast<Wasm::IPIntCallee*>(callFrame->callee().asNativeCallee())

#if ENABLE(WEBASSEMBLY_BBQJIT)

static inline bool shouldJIT(Wasm::IPIntCallee* callee)
{
    if (!Options::useBBQJIT() || !Wasm::BBQPlan::ensureGlobalBBQAllowlist().containsWasmFunction(callee->functionIndex()))
        return false;
    if (!Options::wasmFunctionIndexRangeToCompile().isInRange(callee->functionIndex()))
        return false;
    return true;
}

enum class OSRFor { Call, Loop };

static inline Wasm::JITCallee* jitCompileAndSetHeuristics(Wasm::IPIntCallee* callee, JSWebAssemblyInstance* instance, OSRFor osrFor)
{
    Wasm::IPIntTierUpCounter& tierUpCounter = callee->tierUpCounter();
    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        return nullptr;
    }

    MemoryMode memoryMode = instance->memory()->mode();
    Wasm::CalleeGroup& calleeGroup = *instance->calleeGroup();
    auto getReplacement = [&] () -> Wasm::JITCallee* {
        Locker locker { calleeGroup.m_lock };
        if (osrFor == OSRFor::Call)
            return calleeGroup.replacement(locker, callee->index());
        return calleeGroup.tryGetBBQCalleeForLoopOSR(locker, instance->vm(), callee->functionIndex());
    };

    if (auto* replacement = getReplacement()) {
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
        }
    }

    if (compile) {
        Wasm::FunctionCodeIndex functionIndex = callee->functionIndex();
        if (Wasm::BBQPlan::ensureGlobalBBQAllowlist().containsWasmFunction(functionIndex)) {
            auto plan = Wasm::BBQPlan::create(instance->vm(), const_cast<Wasm::ModuleInformation&>(instance->module().moduleInformation()), functionIndex, callee->hasExceptionHandlers(), Ref(*instance->calleeGroup()), Wasm::Plan::dontFinalize());
            Wasm::ensureWorklist().enqueue(plan.get());
            if (UNLIKELY(!Options::useConcurrentJIT() || !Options::useWasmIPInt()))
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

    if (auto* replacement = jitCompileAndSetHeuristics(callee, instance, OSRFor::Call))
        WASM_RETURN_TWO(replacement->entrypoint().taggedPtr(), nullptr);
    WASM_RETURN_TWO(nullptr, nullptr);
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
    auto* bbqCallee = static_cast<Wasm::BBQCallee*>(jitCompileAndSetHeuristics(callee, instance, OSRFor::Loop));
    if (!bbqCallee)
        WASM_RETURN_TWO(nullptr, nullptr);

    ASSERT(bbqCallee->compilationMode() == Wasm::CompilationMode::BBQMode);
    size_t osrEntryScratchBufferSize = bbqCallee->osrEntryScratchBufferSize();
    RELEASE_ASSERT(osrEntryScratchBufferSize >= callee->numLocals() + osrEntryData.numberOfStackValues + osrEntryData.tryDepth);

    uint64_t* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryScratchBufferSize);
    if (!buffer)
        WASM_RETURN_TWO(nullptr, nullptr);

    uint32_t index = 0;
    buffer[index++] = osrEntryData.loopIndex;
    for (uint32_t i = 0; i < callee->numLocals(); ++i)
        buffer[index++] = pl[i].i64;

    // If there's no rethrow slots just 0 fill the buffer.
    ASSERT(osrEntryData.tryDepth <= callee->rethrowSlots() || !callee->rethrowSlots());
    for (uint32_t i = 0; i < osrEntryData.tryDepth; ++i)
        buffer[index++] = callee->rethrowSlots() ? pl[callee->localSizeToAlloc() + i].i64 : 0;

    for (uint32_t i = 0; i < osrEntryData.numberOfStackValues; ++i) {
        pl -= 1;
        buffer[index++] = pl->i64;
    }

    auto sharedLoopEntrypoint = bbqCallee->sharedLoopEntrypoint();
    RELEASE_ASSERT(sharedLoopEntrypoint);

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

    jitCompileAndSetHeuristics(callee, instance, OSRFor::Call);
    WASM_RETURN_TWO(nullptr, nullptr);
}
#endif

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

        ASSERT(wasmException->payload().size() == wasmException->tag().parameterCount());
        uint64_t size = wasmException->payload().size();

        for (unsigned i = 0; i < size; ++i)
            stackPointer[size - 1 - i].i64 = wasmException->payload()[i];
    }

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    throwScope.clearException();

    WASM_RETURN_TWO(nullptr, nullptr);
}

WASM_IPINT_EXTERN_CPP_DECL(throw_exception, CallFrame* callFrame, IPIntStackEntry* arguments, unsigned exceptionIndex)
{
    VM& vm = instance->vm();
    SlowPathFrameTracer tracer(vm, callFrame);

    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!throwScope.exception());

    JSGlobalObject* globalObject = instance->globalObject();
    const Wasm::Tag& tag = instance->tag(exceptionIndex);

    FixedVector<uint64_t> values(tag.parameterBufferSize());
    for (unsigned i = 0; i < tag.parameterBufferSize(); ++i)
        values[tag.parameterBufferSize() - 1 - i] = arguments[i].i64;

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
    RELEASE_ASSERT(tryDepth <= callee->rethrowSlots());
    JSWebAssemblyException* exception = reinterpret_cast<JSWebAssemblyException**>(pl)[callee->localSizeToAlloc() + tryDepth - 1];
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
        WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<uintptr_t>(1)), std::bit_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(std::bit_cast<void*>(result), 0);
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
        WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<uintptr_t>(1)), std::bit_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tableIndex);
    UNUSED_PARAM(index);
    UNUSED_PARAM(value);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_init, IPIntStackEntry* sp, TableInitMetadata* metadata)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t n = sp[0].i32;
    int32_t src = sp[1].i32;
    int32_t dst = sp[2].i32;

    if (!Wasm::tableInit(instance, metadata->elementIndex, metadata->tableIndex, dst, src, n))
        WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<uintptr_t>(1)), std::bit_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(sp);
    UNUSED_PARAM(metadata);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_fill, IPIntStackEntry* sp, TableFillMetadata* metadata)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t n = sp[0].i32;
    EncodedJSValue fill = sp[1].ref;
    int32_t offset = sp[2].i32;

    if (!Wasm::tableFill(instance, metadata->tableIndex, offset, fill, n))
        WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<uintptr_t>(1)), std::bit_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(sp);
    UNUSED_PARAM(metadata);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_grow, IPIntStackEntry* sp, TableGrowMetadata* metadata)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t n = sp[0].i32;
    EncodedJSValue fill = sp[1].ref;

    WASM_RETURN_TWO(std::bit_cast<void*>(Wasm::tableGrow(instance, metadata->tableIndex, fill, n)), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(sp);
    UNUSED_PARAM(metadata);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL_1P(current_memory)
{
#if CPU(ARM64) || CPU(X86_64)
    size_t size = instance->memory()->memory().handle().size() >> 16;
    WASM_RETURN_TWO(std::bit_cast<void*>(size), 0);
#else
    UNUSED_PARAM(instance);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(memory_grow, int32_t delta)
{
    WASM_RETURN_TWO(reinterpret_cast<void*>(Wasm::growMemory(instance, delta)), 0);
}

WASM_IPINT_EXTERN_CPP_DECL(memory_init, int32_t dataIndex, IPIntStackEntry* sp)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t n = sp[0].i32;
    int32_t s = sp[1].i32;
    int32_t d = sp[2].i32;

    if (!Wasm::memoryInit(instance, dataIndex, d, s, n))
        WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<uintptr_t>(1)), std::bit_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsMemoryAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(dataIndex);
    UNUSED_PARAM(sp);
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
        WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<uintptr_t>(1)), std::bit_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsMemoryAccess)));
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
        WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<uintptr_t>(1)), std::bit_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsMemoryAccess)));
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

WASM_IPINT_EXTERN_CPP_DECL(table_copy, IPIntStackEntry* sp, TableCopyMetadata* metadata)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t n = sp[0].i32;
    int32_t src = sp[1].i32;
    int32_t dst = sp[2].i32;

    if (!Wasm::tableCopy(instance, metadata->dstTableIndex, metadata->srcTableIndex, dst, src, n))
        WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<uintptr_t>(1)), std::bit_cast<void*>(static_cast<uintptr_t>(Wasm::ExceptionType::OutOfBoundsTableAccess)));
    WASM_RETURN_TWO(0, 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(sp);
    UNUSED_PARAM(metadata);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

WASM_IPINT_EXTERN_CPP_DECL(table_size, int32_t tableIndex)
{
#if CPU(ARM64) || CPU(X86_64)
    int32_t result = Wasm::tableSize(instance, tableIndex);
    WASM_RETURN_TWO(std::bit_cast<void*>(static_cast<EncodedJSValue>(result)), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(tableIndex);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

static inline UGPRPair doWasmCall(JSWebAssemblyInstance* instance, Wasm::FunctionSpaceIndex functionIndex, EncodedJSValue* callee)
{
    uint32_t importFunctionCount = instance->module().moduleInformation().importFunctionCount();

    CodePtr<WasmEntryPtrTag> codePtr;
    EncodedJSValue boxedCallee = CalleeBits::encodeNullCallee();

    if (functionIndex < importFunctionCount) {
        JSWebAssemblyInstance::ImportFunctionInfo* functionInfo = instance->importFunctionInfo(functionIndex);
        codePtr = functionInfo->importFunctionStub;
#if USE(JSVALUE64)
        *callee = std::bit_cast<uint64_t>(functionInfo->boxedTargetCalleeLoadLocation);
#else
        *callee = std::bit_cast<uint32_t>(functionInfo->boxedTargetCalleeLoadLocation);
#endif
    } else {
        // Target is a wasm function within the same instance
        codePtr = *instance->calleeGroup()->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
        boxedCallee = CalleeBits::encodeNativeCallee(
            instance->calleeGroup()->wasmCalleeFromFunctionIndexSpace(functionIndex));
        *callee = boxedCallee;
    }

    RELEASE_ASSERT(WTF::isTaggedWith<WasmEntryPtrTag>(codePtr));

    WASM_CALL_RETURN(instance, codePtr);
}

WASM_IPINT_EXTERN_CPP_DECL(prepare_call, unsigned functionIndex, EncodedJSValue* callee)
{
    return doWasmCall(instance, Wasm::FunctionSpaceIndex(functionIndex), callee);
}

WASM_IPINT_EXTERN_CPP_DECL(prepare_call_indirect, CallFrame* callFrame, Wasm::FunctionSpaceIndex* functionIndex, CallIndirectMetadata* call)
{
    unsigned tableIndex = call->tableIndex;
    unsigned typeIndex = call->typeIndex;

    Wasm::FuncRefTable* table = instance->table(tableIndex)->asFuncrefTable();

    if (*functionIndex >= table->length())
        WASM_THROW(callFrame, Wasm::ExceptionType::OutOfBoundsCallIndirect);

    const Wasm::FuncRefTable::Function& function = table->function(*functionIndex);

    if (function.m_function.typeIndex == Wasm::TypeDefinition::invalidIndex)
        WASM_THROW(callFrame, Wasm::ExceptionType::NullTableEntry);

    const auto& callSignature = static_cast<Wasm::IPIntCallee*>(callFrame->callee().asNativeCallee())->signature(typeIndex);
    if (callSignature.index() != function.m_function.typeIndex)
        WASM_THROW(callFrame, Wasm::ExceptionType::BadSignature);

    EncodedJSValue* calleeReturn = std::bit_cast<EncodedJSValue*>(functionIndex);
    EncodedJSValue boxedCallee = CalleeBits::encodeNullCallee();
    if (function.m_function.boxedWasmCalleeLoadLocation)
        boxedCallee = CalleeBits::encodeBoxedNativeCallee(reinterpret_cast<void*>(*function.m_function.boxedWasmCalleeLoadLocation));
    else
        boxedCallee = CalleeBits::encodeNativeCallee(
            function.m_instance->calleeGroup()->wasmCalleeFromFunctionIndexSpace(*functionIndex));
    *calleeReturn = boxedCallee;

    auto callTarget = *function.m_function.entrypointLoadLocation;
    WASM_CALL_RETURN(function.m_instance, callTarget);
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
#if CPU(ARM64) || CPU(X86_64)
    WASM_RETURN_TWO(std::bit_cast<void*>(Wasm::refFunc(instance, index)), 0);
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(index);
    RELEASE_ASSERT_NOT_REACHED("IPInt only supported on ARM64 and X86_64 (for now)");
#endif
}

} } // namespace JSC::IPInt

#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
