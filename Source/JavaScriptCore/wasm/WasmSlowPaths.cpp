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
#include "WasmSlowPaths.h"

#if ENABLE(WEBASSEMBLY)

#include "BytecodeStructs.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntData.h"
#include "WasmBBQPlan.h"
#include "WasmCallee.h"
#include "WasmFunctionCodeBlock.h"
#include "WasmInstance.h"
#include "WasmModuleInformation.h"
#include "WasmOMGForOSREntryPlan.h"
#include "WasmOMGPlan.h"
#include "WasmOperations.h"
#include "WasmSignatureInlines.h"
#include "WasmWorklist.h"
#include "WebAssemblyFunction.h"

namespace JSC { namespace LLInt {

#define WASM_RETURN_TWO(first, second) do { \
        return encodeResult(first, second); \
    } while (false)

#define WASM_END_IMPL() WASM_RETURN_TWO(pc, 0)

#define WASM_THROW(exceptionType) do { \
        callFrame->setArgumentCountIncludingThis(static_cast<int>(exceptionType)); \
        WASM_RETURN_TWO(LLInt::wasmExceptionInstructions(), 0); \
    } while (false)

#define WASM_END() do { \
        WASM_END_IMPL(); \
    } while (false)

#define WASM_RETURN(value) do { \
        callFrame->uncheckedR(instruction.m_dst) = static_cast<EncodedJSValue>(value); \
        WASM_END_IMPL(); \
    } while (false)

#define WASM_CALL_RETURN(targetInstance, callTarget, callTargetTag) do { \
        WASM_RETURN_TWO((retagCodePtr<callTargetTag, JSEntrySlowPathPtrTag>(callTarget)), targetInstance); \
    } while (false)

#define CODE_BLOCK() \
    bitwise_cast<Wasm::FunctionCodeBlock*>(callFrame->codeBlock())

#define READ(virtualRegister) \
    (virtualRegister.isConstant() \
        ? JSValue::decode(CODE_BLOCK()->getConstant(virtualRegister)) \
        : callFrame->r(virtualRegister))

#if ENABLE(WEBASSEMBLY_B3JIT)
enum class RequiredWasmJIT { Any, OMG };

inline bool shouldJIT(Wasm::FunctionCodeBlock* codeBlock, RequiredWasmJIT requiredJIT = RequiredWasmJIT::Any)
{
    if (requiredJIT == RequiredWasmJIT::OMG) {
        if (!Options::useOMGJIT())
            return false;
    } else {
        if (Options::wasmLLIntTiersUpToBBQ() && !Options::useBBQJIT())
            return false;
        if (!Options::wasmLLIntTiersUpToBBQ() && !Options::useOMGJIT())
            return false;
    }
    if (!Options::wasmFunctionIndexRangeToCompile().isInRange(codeBlock->functionIndex()))
        return false;
    return true;
}

inline bool jitCompileAndSetHeuristics(Wasm::LLIntCallee* callee, Wasm::FunctionCodeBlock* codeBlock, Wasm::Instance* instance)
{
    Wasm::LLIntTierUpCounter& tierUpCounter = codeBlock->tierUpCounter();
    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        return false;
    }

    if (callee->replacement())  {
        dataLogLnIf(Options::verboseOSR(), "    Code was already compiled.");
        tierUpCounter.optimizeSoon();
        return true;
    }

    bool compile = false;
    {
        Locker locker { tierUpCounter.m_lock };
        switch (tierUpCounter.m_compilationStatus) {
        case Wasm::LLIntTierUpCounter::CompilationStatus::NotCompiled:
            compile = true;
            tierUpCounter.m_compilationStatus = Wasm::LLIntTierUpCounter::CompilationStatus::Compiling;
            break;
        case Wasm::LLIntTierUpCounter::CompilationStatus::Compiling:
            tierUpCounter.optimizeAfterWarmUp();
            break;
        case Wasm::LLIntTierUpCounter::CompilationStatus::Compiled:
            break;
        }
    }

    if (compile) {
        uint32_t functionIndex = codeBlock->functionIndex();
        RefPtr<Wasm::Plan> plan;
        if (Options::wasmLLIntTiersUpToBBQ())
            plan = adoptRef(*new Wasm::BBQPlan(instance->context(), makeRef(const_cast<Wasm::ModuleInformation&>(instance->module().moduleInformation())), functionIndex, instance->codeBlock(), Wasm::Plan::dontFinalize()));
        else
            plan = adoptRef(*new Wasm::OMGPlan(instance->context(), Ref<Wasm::Module>(instance->module()), functionIndex, instance->memory()->mode(), Wasm::Plan::dontFinalize()));

        Wasm::ensureWorklist().enqueue(makeRef(*plan));
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUpCounter.optimizeAfterWarmUp();
    }

    return !!callee->replacement();
}

WASM_SLOW_PATH_DECL(prologue_osr)
{
    UNUSED_PARAM(pc);

    Wasm::LLIntCallee* callee = static_cast<Wasm::LLIntCallee*>(callFrame->callee().asWasmCallee());
    Wasm::FunctionCodeBlock* codeBlock = CODE_BLOCK();

    if (!shouldJIT(codeBlock)) {
        codeBlock->tierUpCounter().deferIndefinitely();
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    if (!Options::useWasmLLIntPrologueOSR())
        WASM_RETURN_TWO(nullptr, nullptr);

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered prologue_osr with tierUpCounter = ", codeBlock->tierUpCounter());

    if (!jitCompileAndSetHeuristics(callee, codeBlock, instance))
        WASM_RETURN_TWO(nullptr, nullptr);

    WASM_RETURN_TWO(callee->replacement()->entrypoint().executableAddress(), nullptr);
}

WASM_SLOW_PATH_DECL(loop_osr)
{
    Wasm::LLIntCallee* callee = static_cast<Wasm::LLIntCallee*>(callFrame->callee().asWasmCallee());
    Wasm::FunctionCodeBlock* codeBlock = CODE_BLOCK();
    Wasm::LLIntTierUpCounter& tierUpCounter = codeBlock->tierUpCounter();

    if (!Options::useWebAssemblyOSR() || !Options::useWasmLLIntLoopOSR() || !shouldJIT(codeBlock, RequiredWasmJIT::OMG)) {
        slow_path_wasm_prologue_osr(callFrame, pc, instance);
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered loop_osr with tierUpCounter = ", codeBlock->tierUpCounter());

    unsigned loopOSREntryBytecodeOffset = codeBlock->bytecodeOffset(pc);
    const auto& osrEntryData = tierUpCounter.osrEntryDataForLoop(loopOSREntryBytecodeOffset);

    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    const auto doOSREntry = [&] {
        Wasm::OMGForOSREntryCallee* osrEntryCallee = callee->osrEntryCallee();
        if (osrEntryCallee->loopIndex() != osrEntryData.loopIndex)
            WASM_RETURN_TWO(nullptr, nullptr);

        size_t osrEntryScratchBufferSize = osrEntryCallee->osrEntryScratchBufferSize();
        RELEASE_ASSERT(osrEntryScratchBufferSize == osrEntryData.values.size());
        uint64_t* buffer = instance->context()->scratchBufferForSize(osrEntryScratchBufferSize);
        if (!buffer)
            WASM_RETURN_TWO(nullptr, nullptr);

        uint32_t index = 0;
        for (VirtualRegister reg : osrEntryData.values)
            buffer[index++] = READ(reg).encodedJSValue();

        WASM_RETURN_TWO(buffer, osrEntryCallee->entrypoint().executableAddress());
    };

    if (callee->osrEntryCallee())
        return doOSREntry();

    bool compile = false;
    {
        Locker locker { tierUpCounter.m_lock };
        switch (tierUpCounter.m_loopCompilationStatus) {
        case Wasm::LLIntTierUpCounter::CompilationStatus::NotCompiled:
            compile = true;
            tierUpCounter.m_loopCompilationStatus = Wasm::LLIntTierUpCounter::CompilationStatus::Compiling;
            break;
        case Wasm::LLIntTierUpCounter::CompilationStatus::Compiling:
            tierUpCounter.optimizeAfterWarmUp();
            break;
        case Wasm::LLIntTierUpCounter::CompilationStatus::Compiled:
            break;
        }
    }

    if (compile) {
        Ref<Wasm::Plan> plan = adoptRef(*static_cast<Wasm::Plan*>(new Wasm::OMGForOSREntryPlan(instance->context(), Ref<Wasm::Module>(instance->module()), Ref<Wasm::Callee>(*callee), codeBlock->functionIndex(), osrEntryData.loopIndex, instance->memory()->mode(), Wasm::Plan::dontFinalize())));
        Wasm::ensureWorklist().enqueue(plan.copyRef());
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUpCounter.optimizeAfterWarmUp();
    }

    if (callee->osrEntryCallee())
        return doOSREntry();

    WASM_RETURN_TWO(nullptr, nullptr);
}

WASM_SLOW_PATH_DECL(epilogue_osr)
{
    Wasm::LLIntCallee* callee = static_cast<Wasm::LLIntCallee*>(callFrame->callee().asWasmCallee());
    Wasm::FunctionCodeBlock* codeBlock = CODE_BLOCK();

    if (!shouldJIT(codeBlock)) {
        codeBlock->tierUpCounter().deferIndefinitely();
        WASM_END_IMPL();
    }
    if (!Options::useWasmLLIntEpilogueOSR())
        WASM_END_IMPL();

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered epilogue_osr with tierUpCounter = ", codeBlock->tierUpCounter());

    jitCompileAndSetHeuristics(callee, codeBlock, instance);
    WASM_END_IMPL();
}
#endif

WASM_SLOW_PATH_DECL(trace)
{
    UNUSED_PARAM(instance);

    if (!Options::traceLLIntExecution())
        WASM_END_IMPL();

    WasmOpcodeID opcodeID = pc->opcodeID<WasmOpcodeTraits>();
    dataLogF("<%p> %p / %p: executing bc#%zu, %s, pc = %p\n",
        &Thread::current(),
        callFrame->codeBlock(),
        callFrame,
        static_cast<intptr_t>(CODE_BLOCK()->bytecodeOffset(pc)),
        pc->name<WasmOpcodeTraits>(),
        pc);
    if (opcodeID == wasm_enter) {
        dataLogF("Frame will eventually return to %p\n", callFrame->returnPC().value());
        *removeCodePtrTag<volatile char*>(callFrame->returnPC().value());
    }
    if (opcodeID == wasm_ret) {
        dataLogF("Will be returning to %p\n", callFrame->returnPC().value());
        dataLogF("The new cfr will be %p\n", callFrame->callerFrame());
    }
    WASM_END_IMPL();
}

WASM_SLOW_PATH_DECL(out_of_line_jump_target)
{
    UNUSED_PARAM(instance);

    pc = CODE_BLOCK()->outOfLineJumpTarget(pc);
    WASM_END_IMPL();
}

WASM_SLOW_PATH_DECL(ref_func)
{
    auto instruction = pc->as<WasmRefFunc, WasmOpcodeTraits>();
    WASM_RETURN(Wasm::operationWasmRefFunc(instance, instruction.m_functionIndex));
}

WASM_SLOW_PATH_DECL(table_get)
{
    auto instruction = pc->as<WasmTableGet, WasmOpcodeTraits>();
    int32_t index = READ(instruction.m_index).unboxedInt32();
    EncodedJSValue result = Wasm::operationGetWasmTableElement(instance, instruction.m_tableIndex, index);
    if (!result)
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_RETURN(result);
}

WASM_SLOW_PATH_DECL(table_set)
{
    auto instruction = pc->as<WasmTableSet, WasmOpcodeTraits>();
    uint32_t index = READ(instruction.m_index).unboxedUInt32();
    EncodedJSValue value = READ(instruction.m_value).encodedJSValue();
    if (!Wasm::operationSetWasmTableElement(instance, instruction.m_tableIndex, index, value))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_init)
{
    auto instruction = pc->as<WasmTableInit, WasmOpcodeTraits>();
    uint32_t dstOffset = READ(instruction.m_dstOffset).unboxedUInt32();
    uint32_t srcOffset = READ(instruction.m_srcOffset).unboxedUInt32();
    uint32_t length = READ(instruction.m_length).unboxedUInt32();
    if (!Wasm::operationWasmTableInit(instance, instruction.m_elementIndex, instruction.m_tableIndex, dstOffset, srcOffset, length))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(elem_drop)
{
    UNUSED_PARAM(callFrame);

    auto instruction = pc->as<WasmElemDrop, WasmOpcodeTraits>();
    Wasm::operationWasmElemDrop(instance, instruction.m_elementIndex);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_size)
{
    auto instruction = pc->as<WasmTableSize, WasmOpcodeTraits>();
    WASM_RETURN(Wasm::operationGetWasmTableSize(instance, instruction.m_tableIndex));
}

WASM_SLOW_PATH_DECL(table_fill)
{
    auto instruction = pc->as<WasmTableFill, WasmOpcodeTraits>();
    uint32_t offset = READ(instruction.m_offset).unboxedUInt32();
    EncodedJSValue fill = READ(instruction.m_fill).encodedJSValue();
    uint32_t size = READ(instruction.m_size).unboxedUInt32();
    if (!Wasm::operationWasmTableFill(instance, instruction.m_tableIndex, offset, fill, size))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_copy)
{
    auto instruction = pc->as<WasmTableCopy, WasmOpcodeTraits>();
    int32_t dstOffset = READ(instruction.m_dstOffset).unboxedInt32();
    int32_t srcOffset = READ(instruction.m_srcOffset).unboxedInt32();
    int32_t length = READ(instruction.m_length).unboxedInt32();
    if (!Wasm::operationWasmTableCopy(instance, instruction.m_dstTableIndex, instruction.m_srcTableIndex, dstOffset, srcOffset, length))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_grow)
{
    auto instruction = pc->as<WasmTableGrow, WasmOpcodeTraits>();
    EncodedJSValue fill = READ(instruction.m_fill).encodedJSValue();
    uint32_t size = READ(instruction.m_size).unboxedUInt32();
    WASM_RETURN(Wasm::operationWasmTableGrow(instance, instruction.m_tableIndex, fill, size));
}

WASM_SLOW_PATH_DECL(grow_memory)
{
    auto instruction = pc->as<WasmGrowMemory, WasmOpcodeTraits>();
    int32_t delta = READ(instruction.m_delta).unboxedInt32();
    WASM_RETURN(Wasm::operationGrowMemory(callFrame, instance, delta));
}

WASM_SLOW_PATH_DECL(memory_fill)
{
    auto instruction = pc->as<WasmMemoryFill, WasmOpcodeTraits>();
    uint32_t dstAddress = READ(instruction.m_dstAddress).unboxedUInt32();
    uint32_t targetValue = READ(instruction.m_targetValue).unboxedUInt32();
    uint32_t count = READ(instruction.m_count).unboxedUInt32();
    if (!Wasm::operationWasmMemoryFill(instance, dstAddress, targetValue, count))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(memory_copy)
{
    auto instruction = pc->as<WasmMemoryCopy, WasmOpcodeTraits>();
    uint32_t dstAddress = READ(instruction.m_dstAddress).unboxedUInt32();
    uint32_t srcAddress = READ(instruction.m_srcAddress).unboxedUInt32();
    uint32_t count = READ(instruction.m_count).unboxedUInt32();
    if (!Wasm::operationWasmMemoryCopy(instance, dstAddress, srcAddress, count))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(memory_init)
{
    auto instruction = pc->as<WasmMemoryInit, WasmOpcodeTraits>();
    uint32_t dstAddress = READ(instruction.m_dstAddress).unboxedUInt32();
    uint32_t srcAddress = READ(instruction.m_srcAddress).unboxedUInt32();
    uint32_t length = READ(instruction.m_length).unboxedUInt32();
    if (!Wasm::operationWasmMemoryInit(instance, instruction.m_dataSegmentIndex, dstAddress, srcAddress, length))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(data_drop)
{
    UNUSED_PARAM(callFrame);

    auto instruction = pc->as<WasmDataDrop, WasmOpcodeTraits>();
    Wasm::operationWasmDataDrop(instance, instruction.m_dataSegmentIndex);
    WASM_END();
}

inline SlowPathReturnType doWasmCall(Wasm::Instance* instance, unsigned functionIndex)
{
    uint32_t importFunctionCount = instance->module().moduleInformation().importFunctionCount();

    MacroAssemblerCodePtr<WasmEntryPtrTag> codePtr;

    if (functionIndex < importFunctionCount) {
        Wasm::Instance::ImportFunctionInfo* functionInfo = instance->importFunctionInfo(functionIndex);
        if (functionInfo->targetInstance) {
            // target is a wasm function from a different instance
            codePtr = instance->codeBlock()->wasmToWasmExitStub(functionIndex);
        } else {
            // target is JS
            codePtr = functionInfo->wasmToEmbedderStub;
        }
    } else {
        // Target is a wasm function within the same instance
        codePtr = *instance->codeBlock()->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
    }

    WASM_CALL_RETURN(instance, codePtr.executableAddress(), WasmEntryPtrTag);
}

WASM_SLOW_PATH_DECL(call)
{
    UNUSED_PARAM(callFrame);

    auto instruction = pc->as<WasmCall, WasmOpcodeTraits>();
    return doWasmCall(instance, instruction.m_functionIndex);
}

WASM_SLOW_PATH_DECL(call_no_tls)
{
    UNUSED_PARAM(callFrame);

    auto instruction = pc->as<WasmCallNoTls, WasmOpcodeTraits>();
    return doWasmCall(instance, instruction.m_functionIndex);
}

inline SlowPathReturnType doWasmCallIndirect(CallFrame* callFrame, Wasm::Instance* instance, unsigned functionIndex, unsigned tableIndex, unsigned signatureIndex)
{
    Wasm::FuncRefTable* table = instance->table(tableIndex)->asFuncrefTable();

    if (functionIndex >= table->length())
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsCallIndirect);

    Wasm::Instance* targetInstance = table->instance(functionIndex);
    const Wasm::WasmToWasmImportableFunction& function = table->function(functionIndex);

    if (function.signatureIndex == Wasm::Signature::invalidIndex)
        WASM_THROW(Wasm::ExceptionType::NullTableEntry);

    const Wasm::Signature& callSignature = CODE_BLOCK()->signature(signatureIndex);
    if (function.signatureIndex != Wasm::SignatureInformation::get(callSignature))
        WASM_THROW(Wasm::ExceptionType::BadSignature);

    if (targetInstance != instance)
        targetInstance->setCachedStackLimit(instance->cachedStackLimit());

    WASM_CALL_RETURN(targetInstance, function.entrypointLoadLocation->executableAddress(), WasmEntryPtrTag);
}

WASM_SLOW_PATH_DECL(call_indirect)
{
    auto instruction = pc->as<WasmCallIndirect, WasmOpcodeTraits>();
    unsigned functionIndex = READ(instruction.m_functionIndex).unboxedInt32();
    return doWasmCallIndirect(callFrame, instance, functionIndex, instruction.m_tableIndex, instruction.m_signatureIndex);
}

WASM_SLOW_PATH_DECL(call_indirect_no_tls)
{
    auto instruction = pc->as<WasmCallIndirectNoTls, WasmOpcodeTraits>();
    unsigned functionIndex = READ(instruction.m_functionIndex).unboxedInt32();
    return doWasmCallIndirect(callFrame, instance, functionIndex, instruction.m_tableIndex, instruction.m_signatureIndex);
}

inline SlowPathReturnType doWasmCallRef(CallFrame* callFrame, Wasm::Instance* callerInstance, JSValue targetReference, unsigned signatureIndex)
{
    UNUSED_PARAM(callFrame);

    ASSERT(targetReference.isObject());
    JSObject* referenceAsObject = jsCast<JSObject*>(targetReference);

    ASSERT(referenceAsObject->inherits<WebAssemblyFunctionBase>(callerInstance->owner<JSObject>()->vm()));
    auto* wasmFunction = jsCast<WebAssemblyFunctionBase*>(referenceAsObject);
    Wasm::WasmToWasmImportableFunction function = wasmFunction->importableFunction();
    Wasm::Instance* calleeInstance = &wasmFunction->instance()->instance();

    if (calleeInstance != callerInstance)
        calleeInstance->setCachedStackLimit(callerInstance->cachedStackLimit());

    ASSERT(function.signatureIndex == Wasm::SignatureInformation::get(CODE_BLOCK()->signature(signatureIndex)));
    UNUSED_PARAM(signatureIndex);
    WASM_CALL_RETURN(calleeInstance, function.entrypointLoadLocation->executableAddress(), WasmEntryPtrTag);
}

WASM_SLOW_PATH_DECL(call_ref)
{
    auto instruction = pc->as<WasmCallRef, WasmOpcodeTraits>();
    JSValue reference = JSValue::decode(READ(instruction.m_functionReference).encodedJSValue());
    return doWasmCallRef(callFrame, instance, reference, instruction.m_signatureIndex);
}

WASM_SLOW_PATH_DECL(call_ref_no_tls)
{
    auto instruction = pc->as<WasmCallRefNoTls, WasmOpcodeTraits>();
    JSValue reference = JSValue::decode(READ(instruction.m_functionReference).encodedJSValue());
    return doWasmCallRef(callFrame, instance, reference, instruction.m_signatureIndex);
}

WASM_SLOW_PATH_DECL(set_global_ref)
{
    auto instruction = pc->as<WasmSetGlobalRef, WasmOpcodeTraits>();
    instance->setGlobal(instruction.m_globalIndex, READ(instruction.m_value).jsValue());
    WASM_END_IMPL();
}

WASM_SLOW_PATH_DECL(set_global_ref_portable_binding)
{
    auto instruction = pc->as<WasmSetGlobalRefPortableBinding, WasmOpcodeTraits>();
    instance->setGlobal(instruction.m_globalIndex, READ(instruction.m_value).jsValue());
    WASM_END_IMPL();
}

WASM_SLOW_PATH_DECL(memory_atomic_wait32)
{
    auto instruction = pc->as<WasmMemoryAtomicWait32, WasmOpcodeTraits>();
    unsigned base = READ(instruction.m_pointer).unboxedInt32();
    unsigned offset = instruction.m_offset;
    uint32_t value = READ(instruction.m_value).unboxedInt32();
    int64_t timeout = READ(instruction.m_timeout).unboxedInt64();
    int32_t result = Wasm::operationMemoryAtomicWait32(instance, base, offset, value, timeout);
    if (result < 0)
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    WASM_RETURN(result);
}

WASM_SLOW_PATH_DECL(memory_atomic_wait64)
{
    auto instruction = pc->as<WasmMemoryAtomicWait64, WasmOpcodeTraits>();
    unsigned base = READ(instruction.m_pointer).unboxedInt32();
    unsigned offset = instruction.m_offset;
    uint64_t value = READ(instruction.m_value).unboxedInt64();
    int64_t timeout = READ(instruction.m_timeout).unboxedInt64();
    int32_t result = Wasm::operationMemoryAtomicWait64(instance, base, offset, value, timeout);
    if (result < 0)
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    WASM_RETURN(result);
}

WASM_SLOW_PATH_DECL(memory_atomic_notify)
{
    auto instruction = pc->as<WasmMemoryAtomicNotify, WasmOpcodeTraits>();
    unsigned base = READ(instruction.m_pointer).unboxedInt32();
    unsigned offset = instruction.m_offset;
    int32_t count = READ(instruction.m_count).unboxedInt32();
    int32_t result = Wasm::operationMemoryAtomicNotify(instance, base, offset, count);
    if (result < 0)
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    WASM_RETURN(result);
}

extern "C" SlowPathReturnType slow_path_wasm_throw_exception(CallFrame* callFrame, const Instruction* pc, Wasm::Instance* instance, Wasm::ExceptionType exceptionType)
{
    UNUSED_PARAM(pc);
    WASM_RETURN_TWO(operationWasmToJSException(callFrame, exceptionType, instance), nullptr);
}

extern "C" SlowPathReturnType slow_path_wasm_popcount(const Instruction* pc, uint32_t x)
{
    void* result = bitwise_cast<void*>(static_cast<uint64_t>(__builtin_popcount(x)));
    WASM_RETURN_TWO(pc, result);
}

extern "C" SlowPathReturnType slow_path_wasm_popcountll(const Instruction* pc, uint64_t x)
{
    void* result = bitwise_cast<void*>(static_cast<uint64_t>(__builtin_popcountll(x)));
    WASM_RETURN_TWO(pc, result);
}

} } // namespace JSC::LLInt

#endif // ENABLE(WEBASSEMBLY)
