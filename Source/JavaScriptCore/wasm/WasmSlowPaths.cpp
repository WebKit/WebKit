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
#include "JITExceptions.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntData.h"
#include "WasmBBQPlan.h"
#include "WasmCallee.h"
#include "WasmFunctionCodeBlockGenerator.h"
#include "WasmInstance.h"
#include "WasmModuleInformation.h"
#include "WasmOMGPlan.h"
#include "WasmOSREntryPlan.h"
#include "WasmOperations.h"
#include "WasmTypeDefinitionInlines.h"
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

#define CALLEE() \
    static_cast<Wasm::LLIntCallee*>(callFrame->callee().asWasmCallee())

#define READ(virtualRegister) \
    (virtualRegister.isConstant() \
        ? JSValue::decode(CALLEE()->getConstant(virtualRegister)) \
        : callFrame->r(virtualRegister))

#if ENABLE(WEBASSEMBLY_B3JIT)
enum class RequiredWasmJIT { Any, OMG };

inline bool shouldJIT(Wasm::LLIntCallee* callee, RequiredWasmJIT requiredJIT = RequiredWasmJIT::Any)
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
    if (!Options::wasmFunctionIndexRangeToCompile().isInRange(callee->functionIndex()))
        return false;
    return true;
}

inline bool jitCompileAndSetHeuristics(Wasm::LLIntCallee* callee, Wasm::Instance* instance)
{
    Wasm::LLIntTierUpCounter& tierUpCounter = callee->tierUpCounter();
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
        uint32_t functionIndex = callee->functionIndex();
        RefPtr<Wasm::Plan> plan;
        if (Options::wasmLLIntTiersUpToBBQ())
            plan = adoptRef(*new Wasm::BBQPlan(instance->vm(), const_cast<Wasm::ModuleInformation&>(instance->module().moduleInformation()), functionIndex, callee->hasExceptionHandlers(), instance->calleeGroup(), Wasm::Plan::dontFinalize()));
        else
            plan = adoptRef(*new Wasm::OMGPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), functionIndex, callee->hasExceptionHandlers(), instance->memory()->mode(), Wasm::Plan::dontFinalize()));

        Wasm::ensureWorklist().enqueue(*plan);
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUpCounter.optimizeAfterWarmUp();
    }

    return !!callee->replacement(instance->memory()->mode());
}

WASM_SLOW_PATH_DECL(prologue_osr)
{
    UNUSED_PARAM(pc);

    Wasm::LLIntCallee* callee = CALLEE();

    if (!shouldJIT(callee)) {
        callee->tierUpCounter().deferIndefinitely();
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    if (!Options::useWasmLLIntPrologueOSR())
        WASM_RETURN_TWO(nullptr, nullptr);

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered prologue_osr with tierUpCounter = ", callee->tierUpCounter());

    if (!jitCompileAndSetHeuristics(callee, instance))
        WASM_RETURN_TWO(nullptr, nullptr);

    WASM_RETURN_TWO(callee->replacement(instance->memory()->mode())->entrypoint().taggedPtr(), nullptr);
}

WASM_SLOW_PATH_DECL(loop_osr)
{
    Wasm::LLIntCallee* callee = CALLEE();
    Wasm::LLIntTierUpCounter& tierUpCounter = callee->tierUpCounter();

    if (!Options::useWebAssemblyOSR() || !Options::useWasmLLIntLoopOSR() || !shouldJIT(callee, RequiredWasmJIT::OMG)) {
        slow_path_wasm_prologue_osr(callFrame, pc, instance);
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered loop_osr with tierUpCounter = ", callee->tierUpCounter());

    if (!tierUpCounter.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        WASM_RETURN_TWO(nullptr, nullptr);
    }

    unsigned loopOSREntryBytecodeOffset = callee->bytecodeOffset(pc);
    const auto& osrEntryData = tierUpCounter.osrEntryDataForLoop(loopOSREntryBytecodeOffset);

    if (Options::wasmLLIntTiersUpToBBQ() && Wasm::BBQPlan::planGeneratesLoopOSREntrypoints(instance->module().moduleInformation())) {
        if (!jitCompileAndSetHeuristics(callee, instance))
            WASM_RETURN_TWO(nullptr, nullptr);

        Wasm::BBQCallee* bbqCallee;
        {
            Locker locker { instance->calleeGroup()->m_lock };
            bbqCallee = instance->calleeGroup()->bbqCallee(locker, callee->functionIndex());
        }
        RELEASE_ASSERT(bbqCallee);

        size_t osrEntryScratchBufferSize = bbqCallee->osrEntryScratchBufferSize();
        RELEASE_ASSERT(osrEntryScratchBufferSize >= osrEntryData.values.size());
        uint64_t* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryScratchBufferSize);
        if (!buffer)
            WASM_RETURN_TWO(nullptr, nullptr);
        RELEASE_ASSERT(osrEntryData.loopIndex < bbqCallee->loopEntrypoints().size());

        uint32_t index = 0;
        for (VirtualRegister reg : osrEntryData.values)
            buffer[index++] = READ(reg).encodedJSValue();

        WASM_RETURN_TWO(buffer, bbqCallee->loopEntrypoints()[osrEntryData.loopIndex].taggedPtr());
    } else {
        const auto doOSREntry = [&](Wasm::OSREntryCallee* osrEntryCallee) {
            if (osrEntryCallee->loopIndex() != osrEntryData.loopIndex)
                WASM_RETURN_TWO(nullptr, nullptr);

            size_t osrEntryScratchBufferSize = osrEntryCallee->osrEntryScratchBufferSize();
            RELEASE_ASSERT(osrEntryScratchBufferSize == osrEntryData.values.size());
            uint64_t* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryScratchBufferSize);
            if (!buffer)
                WASM_RETURN_TWO(nullptr, nullptr);

            uint32_t index = 0;
            for (VirtualRegister reg : osrEntryData.values)
                buffer[index++] = READ(reg).encodedJSValue();

            WASM_RETURN_TWO(buffer, osrEntryCallee->entrypoint().taggedPtr());
        };

        if (auto* osrEntryCallee = callee->osrEntryCallee(instance->memory()->mode()))
            return doOSREntry(osrEntryCallee);

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

WASM_SLOW_PATH_DECL(epilogue_osr)
{
    Wasm::LLIntCallee* callee = CALLEE();

    if (!shouldJIT(callee)) {
        callee->tierUpCounter().deferIndefinitely();
        WASM_END_IMPL();
    }
    if (!Options::useWasmLLIntEpilogueOSR())
        WASM_END_IMPL();

    dataLogLnIf(Options::verboseOSR(), *callee, ": Entered epilogue_osr with tierUpCounter = ", callee->tierUpCounter());

    jitCompileAndSetHeuristics(callee, instance);
    WASM_END_IMPL();
}
#endif

WASM_SLOW_PATH_DECL(trace)
{
    UNUSED_PARAM(instance);

    if (!Options::traceLLIntExecution())
        WASM_END_IMPL();

    WasmOpcodeID opcodeID = pc->opcodeID();
    dataLogF("<%p> %p / %p: executing bc#%zu, %s, pc = %p\n",
        &Thread::current(),
        CALLEE(),
        callFrame,
        static_cast<intptr_t>(CALLEE()->bytecodeOffset(pc)),
        pc->name(),
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

    pc = CALLEE()->outOfLineJumpTarget(pc);
    WASM_END_IMPL();
}

WASM_SLOW_PATH_DECL(ref_func)
{
    auto instruction = pc->as<WasmRefFunc>();
    WASM_RETURN(Wasm::operationWasmRefFunc(instance, instruction.m_functionIndex));
}

WASM_SLOW_PATH_DECL(array_new)
{
    auto instruction = pc->as<WasmArrayNew>();
    uint32_t size = READ(instruction.m_size).unboxedUInt32();
    EncodedJSValue value = READ(instruction.m_value).encodedJSValue();
    WASM_RETURN(Wasm::operationWasmArrayNew(instance, instruction.m_typeIndex, size, value));
}

WASM_SLOW_PATH_DECL(array_new_default)
{
    auto instruction = pc->as<WasmArrayNewDefault>();
    uint32_t size = READ(instruction.m_size).unboxedUInt32();

    Wasm::TypeDefinition& arraySignature = instance->module().moduleInformation().typeSignatures[instruction.m_typeIndex];
    ASSERT(arraySignature.is<Wasm::ArrayType>());
    Wasm::Type elementType = arraySignature.as<Wasm::ArrayType>()->elementType().type;

    EncodedJSValue value = 0;
    if (Wasm::isRefType(elementType))
        value = JSValue::encode(jsNull());

    WASM_RETURN(Wasm::operationWasmArrayNew(instance, instruction.m_typeIndex, size, value));
}

WASM_SLOW_PATH_DECL(array_get)
{
    auto instruction = pc->as<WasmArrayGet>();
    EncodedJSValue arrayref = READ(instruction.m_arrayref).encodedJSValue();
    if (JSValue::decode(arrayref).isNull())
        WASM_THROW(Wasm::ExceptionType::NullArrayGet);
    uint32_t index = READ(instruction.m_index).unboxedUInt32();

    JSValue arrayValue = JSValue::decode(arrayref);
    ASSERT(arrayValue.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayValue.getObject());
    if (index >= arrayObject->size())
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsArrayGet);

    WASM_RETURN(Wasm::operationWasmArrayGet(instance, instruction.m_typeIndex, arrayref, index));
}

WASM_SLOW_PATH_DECL(array_set)
{
    auto instruction = pc->as<WasmArraySet>();
    EncodedJSValue arrayref = READ(instruction.m_arrayref).encodedJSValue();
    if (JSValue::decode(arrayref).isNull())
        WASM_THROW(Wasm::ExceptionType::NullArraySet);
    uint32_t index = READ(instruction.m_index).unboxedUInt32();
    EncodedJSValue value = READ(instruction.m_value).encodedJSValue();

    JSValue arrayValue = JSValue::decode(arrayref);
    ASSERT(arrayValue.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayValue.getObject());
    if (index >= arrayObject->size())
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsArraySet);

    Wasm::operationWasmArraySet(instance, instruction.m_typeIndex, arrayref, index, value);
    WASM_END_IMPL();
}

WASM_SLOW_PATH_DECL(struct_new)
{
    auto instruction = pc->as<WasmStructNew>();
    ASSERT(instruction.m_typeIndex < instance->module().moduleInformation().typeCount());

    ASSERT(!instruction.m_firstValue.isConstant());
    WASM_RETURN(Wasm::operationWasmStructNew(instance, instruction.m_typeIndex, reinterpret_cast<uint64_t*>(&callFrame->r(instruction.m_firstValue))));
}

WASM_SLOW_PATH_DECL(struct_get)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmStructGet>();
    auto structReference = READ(instruction.m_structReference).encodedJSValue();
    WASM_RETURN(Wasm::operationWasmStructGet(structReference, instruction.m_fieldIndex));
}

WASM_SLOW_PATH_DECL(struct_set)
{
    auto instruction = pc->as<WasmStructSet>();
    auto structReference = READ(instruction.m_structReference).encodedJSValue();
    auto value = READ(instruction.m_value).encodedJSValue();
    Wasm::operationWasmStructSet(instance, structReference, instruction.m_fieldIndex, value);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_get)
{
    auto instruction = pc->as<WasmTableGet>();
    int32_t index = READ(instruction.m_index).unboxedInt32();
    EncodedJSValue result = Wasm::operationGetWasmTableElement(instance, instruction.m_tableIndex, index);
    if (!result)
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_RETURN(result);
}

WASM_SLOW_PATH_DECL(table_set)
{
    auto instruction = pc->as<WasmTableSet>();
    uint32_t index = READ(instruction.m_index).unboxedUInt32();
    EncodedJSValue value = READ(instruction.m_value).encodedJSValue();
    if (!Wasm::operationSetWasmTableElement(instance, instruction.m_tableIndex, index, value))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_init)
{
    auto instruction = pc->as<WasmTableInit>();
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

    auto instruction = pc->as<WasmElemDrop>();
    Wasm::operationWasmElemDrop(instance, instruction.m_elementIndex);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_size)
{
    auto instruction = pc->as<WasmTableSize>();
    WASM_RETURN(Wasm::operationGetWasmTableSize(instance, instruction.m_tableIndex));
}

WASM_SLOW_PATH_DECL(table_fill)
{
    auto instruction = pc->as<WasmTableFill>();
    uint32_t offset = READ(instruction.m_offset).unboxedUInt32();
    EncodedJSValue fill = READ(instruction.m_fill).encodedJSValue();
    uint32_t size = READ(instruction.m_size).unboxedUInt32();
    if (!Wasm::operationWasmTableFill(instance, instruction.m_tableIndex, offset, fill, size))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_copy)
{
    auto instruction = pc->as<WasmTableCopy>();
    int32_t dstOffset = READ(instruction.m_dstOffset).unboxedInt32();
    int32_t srcOffset = READ(instruction.m_srcOffset).unboxedInt32();
    int32_t length = READ(instruction.m_length).unboxedInt32();
    if (!Wasm::operationWasmTableCopy(instance, instruction.m_dstTableIndex, instruction.m_srcTableIndex, dstOffset, srcOffset, length))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTableAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(table_grow)
{
    auto instruction = pc->as<WasmTableGrow>();
    EncodedJSValue fill = READ(instruction.m_fill).encodedJSValue();
    uint32_t size = READ(instruction.m_size).unboxedUInt32();
    WASM_RETURN(Wasm::operationWasmTableGrow(instance, instruction.m_tableIndex, fill, size));
}

WASM_SLOW_PATH_DECL(grow_memory)
{
    auto instruction = pc->as<WasmGrowMemory>();
    int32_t delta = READ(instruction.m_delta).unboxedInt32();
    WASM_RETURN(Wasm::operationGrowMemory(callFrame, instance, delta));
}

WASM_SLOW_PATH_DECL(memory_fill)
{
    auto instruction = pc->as<WasmMemoryFill>();
    uint32_t dstAddress = READ(instruction.m_dstAddress).unboxedUInt32();
    uint32_t targetValue = READ(instruction.m_targetValue).unboxedUInt32();
    uint32_t count = READ(instruction.m_count).unboxedUInt32();
    if (!Wasm::operationWasmMemoryFill(instance, dstAddress, targetValue, count))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(memory_copy)
{
    auto instruction = pc->as<WasmMemoryCopy>();
    uint32_t dstAddress = READ(instruction.m_dstAddress).unboxedUInt32();
    uint32_t srcAddress = READ(instruction.m_srcAddress).unboxedUInt32();
    uint32_t count = READ(instruction.m_count).unboxedUInt32();
    if (!Wasm::operationWasmMemoryCopy(instance, dstAddress, srcAddress, count))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    WASM_END();
}

WASM_SLOW_PATH_DECL(memory_init)
{
    auto instruction = pc->as<WasmMemoryInit>();
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

    auto instruction = pc->as<WasmDataDrop>();
    Wasm::operationWasmDataDrop(instance, instruction.m_dataSegmentIndex);
    WASM_END();
}

inline SlowPathReturnType doWasmCall(Wasm::Instance* instance, unsigned functionIndex)
{
    uint32_t importFunctionCount = instance->module().moduleInformation().importFunctionCount();

    CodePtr<WasmEntryPtrTag> codePtr;

    if (functionIndex < importFunctionCount) {
        Wasm::Instance::ImportFunctionInfo* functionInfo = instance->importFunctionInfo(functionIndex);
        if (functionInfo->targetInstance) {
            // target is a wasm function from a different instance
            codePtr = instance->calleeGroup()->wasmToWasmExitStub(functionIndex);
        } else {
            // target is JS
            codePtr = functionInfo->wasmToEmbedderStub;
        }
    } else {
        // Target is a wasm function within the same instance
        codePtr = *instance->calleeGroup()->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
    }

    WASM_CALL_RETURN(instance, codePtr.taggedPtr(), WasmEntryPtrTag);
}

WASM_SLOW_PATH_DECL(call)
{
    UNUSED_PARAM(callFrame);

    auto instruction = pc->as<WasmCall>();
    return doWasmCall(instance, instruction.m_functionIndex);
}

WASM_SLOW_PATH_DECL(call_no_tls)
{
    UNUSED_PARAM(callFrame);

    auto instruction = pc->as<WasmCallNoTls>();
    return doWasmCall(instance, instruction.m_functionIndex);
}

inline SlowPathReturnType doWasmCallIndirect(CallFrame* callFrame, Wasm::Instance* instance, unsigned functionIndex, unsigned tableIndex, unsigned typeIndex)
{
    Wasm::FuncRefTable* table = instance->table(tableIndex)->asFuncrefTable();

    if (functionIndex >= table->length())
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsCallIndirect);

    Wasm::Instance* targetInstance = table->instance(functionIndex);
    const Wasm::WasmToWasmImportableFunction& function = table->function(functionIndex);

    if (function.typeIndex == Wasm::TypeDefinition::invalidIndex)
        WASM_THROW(Wasm::ExceptionType::NullTableEntry);

    const auto& callSignature = CALLEE()->signature(typeIndex);
    if (callSignature.index() != function.typeIndex)
        WASM_THROW(Wasm::ExceptionType::BadSignature);

    if (targetInstance != instance)
        targetInstance->setCachedStackLimit(instance->cachedStackLimit());

    WASM_CALL_RETURN(targetInstance, function.entrypointLoadLocation->taggedPtr(), WasmEntryPtrTag);
}

WASM_SLOW_PATH_DECL(call_indirect)
{
    auto instruction = pc->as<WasmCallIndirect>();
    unsigned functionIndex = READ(instruction.m_functionIndex).unboxedInt32();
    return doWasmCallIndirect(callFrame, instance, functionIndex, instruction.m_tableIndex, instruction.m_typeIndex);
}

WASM_SLOW_PATH_DECL(call_indirect_no_tls)
{
    auto instruction = pc->as<WasmCallIndirectNoTls>();
    unsigned functionIndex = READ(instruction.m_functionIndex).unboxedInt32();
    return doWasmCallIndirect(callFrame, instance, functionIndex, instruction.m_tableIndex, instruction.m_typeIndex);
}

inline SlowPathReturnType doWasmCallRef(CallFrame* callFrame, Wasm::Instance* callerInstance, JSValue targetReference, unsigned typeIndex)
{
    UNUSED_PARAM(callFrame);

    if (targetReference.isNull())
        WASM_THROW(Wasm::ExceptionType::NullReference);

    ASSERT(targetReference.isObject());
    JSObject* referenceAsObject = jsCast<JSObject*>(targetReference);

    ASSERT(referenceAsObject->inherits<WebAssemblyFunctionBase>());
    auto* wasmFunction = jsCast<WebAssemblyFunctionBase*>(referenceAsObject);
    Wasm::WasmToWasmImportableFunction function = wasmFunction->importableFunction();
    Wasm::Instance* calleeInstance = &wasmFunction->instance()->instance();

    if (calleeInstance != callerInstance)
        calleeInstance->setCachedStackLimit(callerInstance->cachedStackLimit());

    ASSERT(function.typeIndex == CALLEE()->signature(typeIndex).index());
    UNUSED_PARAM(typeIndex);
    WASM_CALL_RETURN(calleeInstance, function.entrypointLoadLocation->taggedPtr(), WasmEntryPtrTag);
}

WASM_SLOW_PATH_DECL(call_ref)
{
    auto instruction = pc->as<WasmCallRef>();
    JSValue reference = JSValue::decode(READ(instruction.m_functionReference).encodedJSValue());
    return doWasmCallRef(callFrame, instance, reference, instruction.m_typeIndex);
}

WASM_SLOW_PATH_DECL(call_ref_no_tls)
{
    auto instruction = pc->as<WasmCallRefNoTls>();
    JSValue reference = JSValue::decode(READ(instruction.m_functionReference).encodedJSValue());
    return doWasmCallRef(callFrame, instance, reference, instruction.m_typeIndex);
}

WASM_SLOW_PATH_DECL(set_global_ref)
{
    auto instruction = pc->as<WasmSetGlobalRef>();
    instance->setGlobal(instruction.m_globalIndex, READ(instruction.m_value).jsValue());
    WASM_END_IMPL();
}

WASM_SLOW_PATH_DECL(set_global_ref_portable_binding)
{
    auto instruction = pc->as<WasmSetGlobalRefPortableBinding>();
    instance->setGlobal(instruction.m_globalIndex, READ(instruction.m_value).jsValue());
    WASM_END_IMPL();
}

WASM_SLOW_PATH_DECL(memory_atomic_wait32)
{
    auto instruction = pc->as<WasmMemoryAtomicWait32>();
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
    auto instruction = pc->as<WasmMemoryAtomicWait64>();
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
    auto instruction = pc->as<WasmMemoryAtomicNotify>();
    unsigned base = READ(instruction.m_pointer).unboxedInt32();
    unsigned offset = instruction.m_offset;
    int32_t count = READ(instruction.m_count).unboxedInt32();
    int32_t result = Wasm::operationMemoryAtomicNotify(instance, base, offset, count);
    if (result < 0)
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsMemoryAccess);
    WASM_RETURN(result);
}

WASM_SLOW_PATH_DECL(throw)
{
    instance->storeTopCallFrame(callFrame);

    JSWebAssemblyInstance* jsInstance = instance->owner<JSWebAssemblyInstance>();
    JSGlobalObject* globalObject = jsInstance->globalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto instruction = pc->as<WasmThrow>();
    const Wasm::Tag& tag = instance->tag(instruction.m_exceptionIndex);

    FixedVector<uint64_t> values(tag.parameterCount());
    for (unsigned i = 0; i < tag.parameterCount(); ++i)
        values[i] = READ((instruction.m_firstValue - i)).encodedJSValue();

    JSWebAssemblyException* exception = JSWebAssemblyException::create(vm, globalObject->webAssemblyExceptionStructure(), tag, WTFMove(values));
    throwException(globalObject, throwScope, exception);

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
    vm.calleeForWasmCatch = callFrame->callee();
    Register* calleeSlot = bitwise_cast<Register*>(callFrame) + static_cast<int>(CallFrameSlot::callee);
    *calleeSlot = bitwise_cast<JSCell*>(jsInstance->module());
    WASM_RETURN_TWO(vm.targetMachinePCForThrow, nullptr);
}

WASM_SLOW_PATH_DECL(rethrow)
{
    instance->storeTopCallFrame(callFrame);

    JSWebAssemblyInstance* jsInstance = instance->owner<JSWebAssemblyInstance>();
    JSGlobalObject* globalObject = jsInstance->globalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto instruction = pc->as<WasmRethrow>();
    JSValue exception = READ(instruction.m_exception).jsValue();
    throwException(globalObject, throwScope, exception);

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
    vm.calleeForWasmCatch = callFrame->callee();
    Register* calleeSlot = bitwise_cast<Register*>(callFrame) + static_cast<int>(CallFrameSlot::callee);
    *calleeSlot = bitwise_cast<JSCell*>(jsInstance->module());
    WASM_RETURN_TWO(vm.targetMachinePCForThrow, nullptr);
}

WASM_SLOW_PATH_DECL(retrieve_and_clear_exception)
{
    UNUSED_PARAM(callFrame);
    JSWebAssemblyInstance* jsInstance = instance->owner<JSWebAssemblyInstance>();
    JSGlobalObject* globalObject = jsInstance->globalObject();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(!!throwScope.exception());

    Exception* exception = throwScope.exception();
    JSValue thrownValue = exception->value();
    void* payload = nullptr;

    const auto& handleCatchAll = [&](const auto& instruction) {
        callFrame->uncheckedR(instruction.m_exception) = thrownValue;
    };

    const auto& handleCatch = [&](const auto& instruction) {
        JSWebAssemblyException* wasmException = jsDynamicCast<JSWebAssemblyException*>(thrownValue);
        RELEASE_ASSERT(!!wasmException);
        payload = bitwise_cast<void*>(wasmException->payload().data());
        callFrame->uncheckedR(instruction.m_exception) = thrownValue;
    };

    if (pc->is<WasmCatch>())
        handleCatch(pc->as<WasmCatch>());
    else if (pc->is<WasmCatchAll>())
        handleCatchAll(pc->as<WasmCatchAll>());
    else if (pc->is<WasmCatchNoTls>())
        handleCatch(pc->as<WasmCatchNoTls>());
    else if (pc->is<WasmCatchAllNoTls>())
        handleCatchAll(pc->as<WasmCatchAllNoTls>());
    else
        RELEASE_ASSERT_NOT_REACHED();

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    throwScope.clearException();
    WASM_RETURN_TWO(pc, payload);
}

#if USE(JSVALUE32_64)
WASM_SLOW_PATH_DECL(f32_ceil)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF32Ceil>();
    float operand = READ(instruction.m_operand).unboxedFloat();
    WASM_RETURN(JSValue::encode(wasmUnboxedFloat(std::ceil(operand))));
}

WASM_SLOW_PATH_DECL(f32_floor)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF32Floor>();
    float operand = READ(instruction.m_operand).unboxedFloat();
    WASM_RETURN(JSValue::encode(wasmUnboxedFloat(std::floor(operand))));
}

WASM_SLOW_PATH_DECL(f32_trunc)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF32Trunc>();
    float operand = READ(instruction.m_operand).unboxedFloat();
    WASM_RETURN(JSValue::encode(wasmUnboxedFloat(std::trunc(operand))));
}

WASM_SLOW_PATH_DECL(f32_nearest)
{
    static_assert(std::numeric_limits<float>::round_style == std::round_to_nearest);
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF32Nearest>();
    float operand = READ(instruction.m_operand).unboxedFloat();
    WASM_RETURN(JSValue::encode(wasmUnboxedFloat(std::nearbyint(operand))));
}

WASM_SLOW_PATH_DECL(f64_ceil)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF64Ceil>();
    double operand = READ(instruction.m_operand).unboxedDouble();
    WASM_RETURN(JSValue::encode(jsDoubleNumber(std::ceil(operand))));
}

WASM_SLOW_PATH_DECL(f64_floor)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF64Floor>();
    double operand = READ(instruction.m_operand).unboxedDouble();
    WASM_RETURN(JSValue::encode(jsDoubleNumber(std::floor(operand))));
}

WASM_SLOW_PATH_DECL(f64_trunc)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF64Trunc>();
    double operand = READ(instruction.m_operand).unboxedDouble();
    WASM_RETURN(JSValue::encode(jsDoubleNumber(std::trunc(operand))));
}

WASM_SLOW_PATH_DECL(f64_nearest)
{
    static_assert(std::numeric_limits<float>::round_style == std::round_to_nearest);
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF64Nearest>();
    double operand = READ(instruction.m_operand).unboxedDouble();
    WASM_RETURN(JSValue::encode(jsDoubleNumber(std::nearbyint(operand))));
}

WASM_SLOW_PATH_DECL(f32_convert_u_i64)
{
    static_assert(std::numeric_limits<float>::round_style == std::round_to_nearest);
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF32ConvertUI64>();
    uint64_t operand = READ(instruction.m_operand).unboxedInt64();
    WASM_RETURN(JSValue::encode(wasmUnboxedFloat(static_cast<float>(operand))));
}

WASM_SLOW_PATH_DECL(f32_convert_s_i64)
{
    static_assert(std::numeric_limits<float>::round_style == std::round_to_nearest);
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF32ConvertSI64>();
    int64_t operand = READ(instruction.m_operand).unboxedInt64();
    WASM_RETURN(JSValue::encode(wasmUnboxedFloat(static_cast<float>(operand))));
}

WASM_SLOW_PATH_DECL(f64_convert_u_i64)
{
    static_assert(std::numeric_limits<float>::round_style == std::round_to_nearest);
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF64ConvertUI64>();
    uint64_t operand = READ(instruction.m_operand).unboxedInt64();
    WASM_RETURN(JSValue::encode(jsDoubleNumber(static_cast<double>(operand))));
}

WASM_SLOW_PATH_DECL(f64_convert_s_i64)
{
    static_assert(std::numeric_limits<float>::round_style == std::round_to_nearest);
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmF64ConvertSI64>();
    int64_t operand = READ(instruction.m_operand).unboxedInt64();
    WASM_RETURN(JSValue::encode(jsDoubleNumber(static_cast<double>(operand))));
}

WASM_SLOW_PATH_DECL(i64_trunc_u_f32)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmI64TruncUF32>();
    float operand = READ(instruction.m_operand).unboxedFloat();
    if (std::isnan(operand) || operand <= -1.0f || operand >= -2.0f * static_cast<float>(INT64_MIN))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTrunc);
    WASM_RETURN(static_cast<uint64_t>(operand));
}

WASM_SLOW_PATH_DECL(i64_trunc_s_f32)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmI64TruncSF32>();
    float operand = READ(instruction.m_operand).unboxedFloat();
    if (std::isnan(operand) || operand < static_cast<float>(INT64_MIN) || operand >= -static_cast<float>(INT64_MIN))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTrunc);
    WASM_RETURN(static_cast<int64_t>(operand));
}

WASM_SLOW_PATH_DECL(i64_trunc_u_f64)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmI64TruncUF64>();
    double operand = READ(instruction.m_operand).unboxedDouble();
    if (std::isnan(operand) || operand <= -1.0 || operand >= -2.0 * static_cast<double>(INT64_MIN))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTrunc);
    WASM_RETURN(static_cast<uint64_t>(operand));
}

WASM_SLOW_PATH_DECL(i64_trunc_s_f64)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmI64TruncSF64>();
    double operand = READ(instruction.m_operand).unboxedDouble();
    if (std::isnan(operand) || operand < static_cast<double>(INT64_MIN) || operand >= -static_cast<double>(INT64_MIN))
        WASM_THROW(Wasm::ExceptionType::OutOfBoundsTrunc);
    WASM_RETURN(static_cast<int64_t>(operand));
}

WASM_SLOW_PATH_DECL(i64_trunc_sat_f32_u)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmI64TruncSatF32U>();
    float operand = READ(instruction.m_operand).unboxedFloat();
    uint64_t result;
    if (std::isnan(operand) || operand <= -1.0f)
        result = 0;
    else if (operand >= -2.0f * static_cast<float>(INT64_MIN))
        result = UINT64_MAX;
    else
        result = static_cast<uint64_t>(operand);
    WASM_RETURN(result);
}

WASM_SLOW_PATH_DECL(i64_trunc_sat_f32_s)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmI64TruncSatF32S>();
    float operand = READ(instruction.m_operand).unboxedFloat();
    int64_t result;
    if (std::isnan(operand))
        result = 0;
    else if (operand < static_cast<float>(INT64_MIN))
        result = INT64_MIN;
    else if (operand >= -static_cast<float>(INT64_MIN))
        result = INT64_MAX;
    else
        result = static_cast<int64_t>(operand);
    WASM_RETURN(result);
}

WASM_SLOW_PATH_DECL(i64_trunc_sat_f64_u)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmI64TruncSatF64U>();
    double operand = READ(instruction.m_operand).unboxedDouble();
    uint64_t result;
    if (std::isnan(operand) || operand <= -1.0)
        result = 0;
    else if (operand >= -2.0 * static_cast<double>(INT64_MIN))
        result = UINT64_MAX;
    else
        result = static_cast<uint64_t>(operand);
    WASM_RETURN(result);
}

WASM_SLOW_PATH_DECL(i64_trunc_sat_f64_s)
{
    UNUSED_PARAM(instance);
    auto instruction = pc->as<WasmI64TruncSatF64S>();
    double operand = READ(instruction.m_operand).unboxedDouble();
    int64_t result;
    if (std::isnan(operand))
        result = 0;
    else if (operand < static_cast<double>(INT64_MIN))
        result = INT64_MIN;
    else if (operand >= -static_cast<double>(INT64_MIN))
        result = INT64_MAX;
    else
        result = static_cast<int64_t>(operand);
    WASM_RETURN(result);
}
#endif

extern "C" SlowPathReturnType slow_path_wasm_throw_exception(CallFrame* callFrame, const WasmInstruction* pc, Wasm::Instance* instance, Wasm::ExceptionType exceptionType)
{
    UNUSED_PARAM(pc);
    WASM_RETURN_TWO(operationWasmToJSException(callFrame, exceptionType, instance), nullptr);
}

extern "C" SlowPathReturnType slow_path_wasm_popcount(const WasmInstruction* pc, uint32_t x)
{
    void* result = bitwise_cast<void*>(static_cast<size_t>(__builtin_popcount(x)));
    WASM_RETURN_TWO(pc, result);
}

extern "C" SlowPathReturnType slow_path_wasm_popcountll(const WasmInstruction* pc, uint64_t x)
{
    void* result = bitwise_cast<void*>(static_cast<size_t>(__builtin_popcountll(x)));
    WASM_RETURN_TWO(pc, result);
}

#if USE(JSVALUE32_64)
// Note: All these div and rem ops perform exception checks in asm
extern "C" int32_t slow_path_wasm_i32_div_s(int32_t a, int32_t b) { return a / b; }
extern "C" uint32_t slow_path_wasm_i32_div_u(uint32_t a, uint32_t b) { return a / b; }
extern "C" int32_t slow_path_wasm_i32_rem_s(int32_t a, int32_t b) { return a % b; }
extern "C" uint32_t slow_path_wasm_i32_rem_u(uint32_t a, uint32_t b) { return a % b; }
extern "C" int64_t slow_path_wasm_i64_div_s(int64_t a, int64_t b) { return a / b; }
extern "C" uint64_t slow_path_wasm_i64_div_u(uint64_t a, uint64_t b) { return a / b; }
extern "C" int64_t slow_path_wasm_i64_rem_s(int64_t a, int64_t b) { return a % b; }
extern "C" uint64_t slow_path_wasm_i64_rem_u(uint64_t a, uint64_t b) { return a % b; }
#endif

} } // namespace JSC::LLInt

#endif // ENABLE(WEBASSEMBLY)
