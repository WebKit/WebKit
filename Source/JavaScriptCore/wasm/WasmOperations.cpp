/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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
#include "DeferGC.h"
#include "ObjectAllocationProfile.h"

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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Wasm {

namespace WasmOperationsInternal {
static constexpr bool verbose = false;
}

JSC_DEFINE_JIT_OPERATION(operationJSToWasmEntryWrapperBuildFrame, JSToWasmCallee*, (void* sp, CallFrame* callFrame, WebAssemblyFunction* function))
{
    dataLogLnIf(WasmOperationsInternal::verbose, "operationJSToWasmEntryWrapperBuildFrame sp: ", RawPointer(sp), " fp: ", RawPointer(callFrame));

    auto* globalObject = function->globalObject();
    VM& vm = globalObject->vm();

    if (function->taintedness() >= SourceTaintedOrigin::IndirectlyTainted)
        vm.setMightBeExecutingTaintedCode();

    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto* callee = function->jsToWasmCallee();
    ASSERT(function);
    ASSERT(callee->compilationMode() == CompilationMode::JSToWasmMode);
    ASSERT(callee->typeIndex() == function->typeIndex());
    ASSERT(callee->frameSize() + JSToWasmCallee::SpillStackSpaceAligned == (reinterpret_cast<uintptr_t>(callFrame) - reinterpret_cast<uintptr_t>(sp)));
    dataLogLnIf(WasmOperationsInternal::verbose, "operationJSToWasmEntryWrapperBuildFrame setting callee: ", RawHex(CalleeBits::encodeNativeCallee(callee)));
    dataLogLnIf(WasmOperationsInternal::verbose, "operationJSToWasmEntryWrapperBuildFrame wasm callee: ", RawHex(callee->wasmCallee().encodedBits()));

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto calleeSPOffsetFromFP = -(static_cast<intptr_t>(callee->frameSize()) + JSToWasmCallee::SpillStackSpaceAligned - JSToWasmCallee::RegisterStackSpaceAligned);

    const TypeDefinition& signature = TypeInformation::get(function->typeIndex()).expand();
    const FunctionSignature& functionSignature = *signature.as<FunctionSignature>();

    if (functionSignature.argumentsOrResultsIncludeV128() || functionSignature.argumentsOrResultsIncludeExnref()) [[unlikely]] {
        throwVMTypeError(globalObject, scope, Wasm::errorMessageForExceptionType(Wasm::ExceptionType::TypeErrorInvalidValueUse));
        OPERATION_RETURN(scope, callee);
    }

    auto access = [sp, callFrame]<typename V>(auto* arr, int i) -> V* {
        dataLogLnIf(WasmOperationsInternal::verbose, "fp[", (&reinterpret_cast<uint8_t*>(arr)[i / sizeof(uint8_t)] - reinterpret_cast<uint8_t*>(callFrame)), "] sp[", (&reinterpret_cast<uint8_t*>(arr)[i / sizeof(uint8_t)] - reinterpret_cast<uint8_t*>(sp)), "](", RawHex(reinterpret_cast<V*>(arr)[i / sizeof(V)]), ")");
        return &reinterpret_cast<V*>(arr)[i / sizeof(V)];
    };

    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    uint64_t* registerSpace = reinterpret_cast<uint64_t*>(sp);
    for (unsigned i = 0; i < functionSignature.argumentCount(); ++i) {
        JSValue jsArg = callFrame->argument(i);
        Type type = functionSignature.argumentType(i);

        dataLogLnIf(WasmOperationsInternal::verbose, "Arg ", i, " ", wasmFrameConvention.params[i].location);

        uint64_t value = toWebAssemblyValue(globalObject, type, jsArg);
        OPERATION_RETURN_IF_EXCEPTION(scope, callee);

        if (wasmFrameConvention.params[i].location.isStackArgument()) {
            auto dst = wasmFrameConvention.params[i].location.offsetFromSP() + calleeSPOffsetFromFP;
            if (type.isI32() || type.isF32())
                *access.operator()<uint32_t>(callFrame, dst) = static_cast<uint32_t>(value);
            else
                *access.operator()<uint64_t>(callFrame, dst) = value;
        } else {
            int dst = 0;
            if (wasmFrameConvention.params[i].location.isFPR())
                dst = GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(wasmFrameConvention.params[i].location.fpr()) * bytesForWidth(Width::Width64);
            else
                dst = GPRInfo::toArgumentIndex(wasmFrameConvention.params[i].location.jsr().payloadGPR()) * sizeof(UCPURegister);
            ASSERT(dst >= 0);

            dataLogLnIf(WasmOperationsInternal::verbose, "* Register Arg ", i, " ", dst);

            if (type.isI32() || type.isF32())
                value = static_cast<uint64_t>(static_cast<uint32_t>(value));
            *access.operator()<uint64_t>(registerSpace, dst) = value;
        }
    }

    OPERATION_RETURN(scope, callee);
}

// We don't actually return anything, but we can't compile with a ExceptionOperationResult<void> as the return type.
JSC_DEFINE_JIT_OPERATION(operationJSToWasmEntryWrapperBuildReturnFrame, EncodedJSValue, (void* sp, CallFrame* callFrame))
{
    dataLogLnIf(WasmOperationsInternal::verbose, "operationJSToWasmEntryWrapperBuildReturnFrame sp: ", RawPointer(sp), " fp: ", RawPointer(callFrame));

    auto* instance = callFrame->wasmInstance();
    ASSERT(instance);
    ASSERT(instance->globalObject());
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);

    uint64_t* registerSpace = reinterpret_cast<uint64_t*>(sp);
    auto* callee = uncheckedDowncast<JSToWasmCallee>(uncheckedDowncast<Wasm::Callee>(callFrame->callee().asNativeCallee()));
    ASSERT(callee->compilationMode() == CompilationMode::JSToWasmMode);

    auto scope = DECLARE_THROW_SCOPE(vm);

    const TypeDefinition& signature = TypeInformation::get(callee->typeIndex()).expand();
    const FunctionSignature& functionSignature = *signature.as<FunctionSignature>();

    auto access = [sp, callFrame]<typename V>(auto* arr, int i) -> V* {
        dataLogLnIf(WasmOperationsInternal::verbose, "fp[", (&reinterpret_cast<uint8_t*>(arr)[i / sizeof(uint8_t)] - reinterpret_cast<uint8_t*>(callFrame)), "] sp[", (&reinterpret_cast<uint8_t*>(arr)[i / sizeof(uint8_t)] - reinterpret_cast<uint8_t*>(sp)), "](", reinterpret_cast<V*>(arr)[i / sizeof(V)], ")");
        return &reinterpret_cast<V*>(arr)[i / sizeof(V)];
    };

    if (functionSignature.returnsVoid())
        OPERATION_RETURN(scope, JSValue::encode(jsUndefined()));

    if (functionSignature.returnCount() == 1) {
        if constexpr (WasmOperationsInternal::verbose) {
            CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
            dataLogLn("* Register Return ", wasmFrameConvention.results[0].location);
        }

        JSValue result;
        if (functionSignature.returnType(0).isI32())
            result = jsNumber(*access.operator()<int32_t>(registerSpace, 0));
        else if (functionSignature.returnType(0).isI64()) {
            result = JSBigInt::makeHeapBigIntOrBigInt32(instance->globalObject(), *access.operator()<int64_t>(registerSpace, 0));
            OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
        } else if (functionSignature.returnType(0).isF32())
            result = jsNumber(purifyNaN(*access.operator()<float>(registerSpace, GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + 0)));
        else if (functionSignature.returnType(0).isF64())
            result = jsNumber(purifyNaN(*access.operator()<double>(registerSpace, GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + 0)));
        else if (isRefType(functionSignature.returnType(0)))
            result = *access.operator()<JSValue>(registerSpace, 0);
        else
            // the JIT thunk emits a breakpoint here, so we can just fail our assertion as well
            RELEASE_ASSERT_NOT_REACHED();

        OPERATION_RETURN(scope, JSValue::encode(result));
    }

    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);
    IndexingType indexingType = ArrayWithUndecided;
    for (unsigned i = 0; i < functionSignature.returnCount(); ++i) {
        Type type = functionSignature.returnType(i);
        switch (type.kind) {
        case TypeKind::I32:
            indexingType = leastUpperBoundOfIndexingTypes(indexingType, ArrayWithInt32);
            break;
        case TypeKind::F32:
        case TypeKind::F64:
            indexingType = leastUpperBoundOfIndexingTypes(indexingType, ArrayWithDouble);
            break;
        default:
            indexingType = leastUpperBoundOfIndexingTypes(indexingType, ArrayWithContiguous);
            break;
        }
    }

    JSGlobalObject* globalObject = instance->globalObject();
    JSArray* resultArray = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType), functionSignature.returnCount());
    if (!resultArray) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        OPERATION_RETURN(scope, encodedJSValue());
    }

    auto calleeSPOffsetFromFP = -(static_cast<intptr_t>(callee->frameSize()) + JSToWasmCallee::SpillStackSpaceAligned - JSToWasmCallee::RegisterStackSpaceAligned);

    for (unsigned i = 0; i < functionSignature.returnCount(); ++i) {
        ValueLocation loc = wasmFrameConvention.results[i].location;
        Type type = functionSignature.returnType(i);
        JSValue result;
        if (loc.isGPR() || loc.isFPR()) {
            switch (type.kind) {
            case TypeKind::I32:
                result = jsNumber(*access.operator()<int32_t>(registerSpace, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) * sizeof(UCPURegister)));
                break;
            case TypeKind::I64:
                result = JSBigInt::makeHeapBigIntOrBigInt32(globalObject, *access.operator()<int64_t>(registerSpace, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) * sizeof(UCPURegister)));
                OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
                break;
            case TypeKind::F32:
                result = jsNumber(purifyNaN(*access.operator()<float>(registerSpace, GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(loc.fpr()) * bytesForWidth(Width::Width64))));
                break;
            case TypeKind::F64:
                result = jsNumber(purifyNaN(*access.operator()<double>(registerSpace, GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(loc.fpr()) * bytesForWidth(Width::Width64))));
                break;
            default:
                result = *access.operator()<JSValue>(registerSpace, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) * sizeof(UCPURegister));
                break;
            }
        } else {
            switch (type.kind) {
            case TypeKind::I32:
                result = jsNumber(*access.operator()<int32_t>(callFrame, calleeSPOffsetFromFP + loc.offsetFromSP()));
                break;
            case TypeKind::I64:
                result = JSBigInt::makeHeapBigIntOrBigInt32(globalObject, *access.operator()<int64_t>(callFrame, calleeSPOffsetFromFP + loc.offsetFromSP()));
                OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
                break;
            case TypeKind::F32:
                result = jsNumber(purifyNaN(*access.operator()<float>(callFrame, calleeSPOffsetFromFP + loc.offsetFromSP())));
                break;
            case TypeKind::F64:
                result = jsNumber(purifyNaN(*access.operator()<double>(callFrame, calleeSPOffsetFromFP + loc.offsetFromSP())));
                break;
            default:
                result = *access.operator()<JSValue>(callFrame, calleeSPOffsetFromFP + loc.offsetFromSP());
                break;
            }
        }
        resultArray->putDirectIndex(globalObject, i, result);
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    OPERATION_RETURN(scope, JSValue::encode(resultArray));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationGetWasmCalleeStackSize, UCPUStrictInt32, (JSWebAssemblyInstance*, WasmCallableFunction* functionInfo))
{
    auto typeIndex = static_cast<WasmOrJSImportableFunctionCallLinkInfo*>(functionInfo)->typeIndex;
    const TypeDefinition& typeDefinition = TypeInformation::get(typeIndex).expand();
    const auto& signature = *typeDefinition.as<FunctionSignature>();
    unsigned argCount = signature.argumentCount();
    const auto& wasmCC = wasmCallingConvention();
    CallInformation wasmCallInfo = wasmCC.callInformationFor(typeDefinition, CallRole::Callee);
    RegisterAtOffsetList savedResultRegisters = wasmCallInfo.computeResultsOffsetList();

    const unsigned numberOfParameters = argCount + 1; // There is a "this" argument.
    const unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
    const unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);
    const unsigned numberOfBytesForSavedResults = savedResultRegisters.sizeOfAreaInBytes();
    const unsigned stackOffset = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(std::max(numberOfBytesForCall, numberOfBytesForSavedResults));

    return toUCPUStrictInt32(stackOffset);
}

JSC_DEFINE_JIT_OPERATION(operationWasmToJSExitMarshalArguments, void, (void* sp, CallFrame* callFrame, void* argumentRegisters, JSWebAssemblyInstance* instance))
{
    auto access = []<typename V>(auto* arr, int i) -> V* {
        return &reinterpret_cast<V*>(arr)[i / sizeof(V)];
    };

    // We need to set up them immediately before potentially throwing anything.
    auto singletonCallee = CalleeBits::boxNativeCallee(&WasmToJSCallee::singleton());
    *access.operator()<uintptr_t>(callFrame, CallFrameSlot::codeBlock * sizeof(Register)) = std::bit_cast<uintptr_t>(instance);
    *access.operator()<uintptr_t>(callFrame, CallFrameSlot::callee * sizeof(Register)) = std::bit_cast<uintptr_t>(singletonCallee);
#if USE(JSVALUE32_64)
    *access.operator()<uintptr_t>(callFrame, CallFrameSlot::callee * sizeof(Register) + TagOffset) = JSValue::NativeCalleeTag;
#endif

    CallFrame* calleeFrame = std::bit_cast<CallFrame*>(reinterpret_cast<uintptr_t>(sp) - sizeof(CallerFrameAndPC));
    ASSERT(instance);
    JSGlobalObject* globalObject = instance->globalObject();
    ASSERT(globalObject);
    VM& vm = instance->vm();

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* importableFunction = *access.operator()<WasmOrJSImportableFunctionCallLinkInfo*>(callFrame, WasmToJSCallableFunctionSlot);
    auto typeIndex = importableFunction->typeIndex;
    const TypeDefinition& typeDefinition = TypeInformation::get(typeIndex).expand();
    const auto& signature = *typeDefinition.as<FunctionSignature>();
    unsigned argCount = signature.argumentCount();

    if (signature.argumentsOrResultsIncludeV128() || signature.argumentsOrResultsIncludeExnref()) [[unlikely]] {
        throwVMTypeError(globalObject, scope, Wasm::errorMessageForExceptionType(Wasm::ExceptionType::TypeErrorInvalidValueUse));
        OPERATION_RETURN(scope);
    }

    const auto& wasmCC = wasmCallingConvention().callInformationFor(typeDefinition, CallRole::Callee);
    const auto& jsCC = jsCallingConvention().callInformationFor(typeDefinition, CallRole::Callee);

    for (unsigned argNum = 0; argNum < argCount; ++argNum) {
        Type argType = signature.argumentType(argNum);
        auto wasmParam = wasmCC.params[argNum].location;
        auto dst = jsCC.params[argNum].location.offsetFromFP();

        switch (argType.kind) {
        case TypeKind::Void:
        case TypeKind::Func:
        case TypeKind::Struct:
        case TypeKind::Structref:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::Eqref:
        case TypeKind::Anyref:
        case TypeKind::Noexnref:
        case TypeKind::Noneref:
        case TypeKind::Nofuncref:
        case TypeKind::Noexternref:
        case TypeKind::I31ref:
        case TypeKind::Rec:
        case TypeKind::Sub:
        case TypeKind::Subfinal:
        case TypeKind::V128:
            RELEASE_ASSERT_NOT_REACHED();
        case TypeKind::RefNull:
        case TypeKind::Ref:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::Exnref:
        case TypeKind::I32: {
            if (wasmParam.isStack()) {
                uint64_t raw = *access.operator()<UCPURegister>(callFrame, wasmParam.offsetFromFP());
#if USE(JSVALUE64)
                if (argType.isI32())
                    *access.operator()<uint64_t>(calleeFrame, dst) = static_cast<uint32_t>(raw) | JSValue::NumberTag;
#else
                if (argType.isI32()) {
                    *access.operator()<uint32_t>(calleeFrame, dst + PayloadOffset) = static_cast<uint32_t>(raw);
                    *access.operator()<uint32_t>(calleeFrame, dst + TagOffset) = JSValue::Int32Tag;
                }
#endif
                else
                    *access.operator()<uint64_t>(calleeFrame, dst) = raw;
            } else {
                auto raw = *access.operator()<UCPURegister>(argumentRegisters, GPRInfo::toArgumentIndex(wasmParam.jsr().payloadGPR()) * sizeof(UCPURegister));
#if USE(JSVALUE64)
                if (argType.isI32())
                    *access.operator()<uint64_t>(calleeFrame, dst) = static_cast<uint32_t>(raw) | JSValue::NumberTag;
                else
                    *access.operator()<uint64_t>(calleeFrame, dst) = raw;
#else
                if (argType.isI32()) {
                    *access.operator()<uint32_t>(calleeFrame, dst + PayloadOffset) = static_cast<uint32_t>(raw);
                    *access.operator()<uint32_t>(calleeFrame, dst + TagOffset) = JSValue::Int32Tag;
                } else
                    *access.operator()<uint32_t>(calleeFrame, dst) = raw;
#endif
            }
            break;
        }
        case TypeKind::I64: {
            if (wasmParam.isStack()) {
                auto result = JSBigInt::makeHeapBigIntOrBigInt32(globalObject, *access.operator()<int64_t>(callFrame, wasmParam.offsetFromFP()));
                OPERATION_RETURN_IF_EXCEPTION(scope);
                *access.operator()<uint64_t>(calleeFrame, dst) = JSValue::encode(result);
            } else {
                auto result = JSBigInt::makeHeapBigIntOrBigInt32(globalObject, *access.operator()<int64_t>(argumentRegisters, GPRInfo::toArgumentIndex(wasmParam.jsr().payloadGPR()) * sizeof(UCPURegister)));
                OPERATION_RETURN_IF_EXCEPTION(scope);
                *access.operator()<uint64_t>(calleeFrame, dst) = JSValue::encode(result);
            }
            break;
        }
        case TypeKind::F32: {
            float val;
            if (wasmParam.isStack())
                val = *access.operator()<float>(callFrame, wasmParam.offsetFromFP());
            else
                val = *access.operator()<float>(argumentRegisters, GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(wasmParam.fpr()) * bytesForWidth(Width::Width64));

            double marshalled = purifyNaN(val);
            uint64_t raw = std::bit_cast<uint64_t>(marshalled);
#if USE(JSVALUE64)
            raw += JSValue::DoubleEncodeOffset;
#endif
            *access.operator()<uint64_t>(calleeFrame, dst) = raw;
            break;
        }
        case TypeKind::F64:
            double val;
            if (wasmParam.isStack())
                val = *access.operator()<double>(callFrame, wasmParam.offsetFromFP());
            else
                val = *access.operator()<double>(argumentRegisters, GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(wasmParam.fpr()) * bytesForWidth(Width::Width64));

            double marshalled = purifyNaN(val);
            uint64_t raw = std::bit_cast<uint64_t>(marshalled);
#if USE(JSVALUE64)
            raw += JSValue::DoubleEncodeOffset;
#endif
            *access.operator()<uint64_t>(calleeFrame, dst) = raw;
            break;
        }
    }

    // store this argument
    *access.operator()<uint64_t>(calleeFrame, CallFrameSlot::thisArgument * static_cast<int>(sizeof(Register))) = JSValue::encode(jsUndefined());

    // materializeImportJSCell and store
    *access.operator()<uint64_t>(calleeFrame, CallFrameSlot::callee * sizeof(Register)) = JSValue::encode(importableFunction->importFunction.get());
    *access.operator()<uint32_t>(calleeFrame, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset) = argCount + 1; // including this = +1

    OPERATION_RETURN(scope);
}

ALWAYS_INLINE void assertCalleeIsReferenced(CallFrame* frame, JSWebAssemblyInstance* instance)
{
#if ASSERT_ENABLED
    CalleeGroup& calleeGroup = *instance->calleeGroup();
    Wasm::Callee* callee = uncheckedDowncast<Wasm::Callee>(frame->callee().asNativeCallee());
    TriState status;
    {
        Locker locker { calleeGroup.m_lock };
        status = calleeGroup.calleeIsReferenced(locker, callee);
    }
    if (status == TriState::Indeterminate)
        ASSERT(instance->vm().heap.isWasmCalleePendingDestruction(*callee));
    else
        ASSERT(status == TriState::True);
#else
    UNUSED_PARAM(frame);
    UNUSED_PARAM(instance);
#endif
}

JSC_DEFINE_JIT_OPERATION(operationWasmToJSExitMarshalReturnValues, void, (void* sp, CallFrame* callFrame, JSWebAssemblyInstance* instance))
{
    auto access = []<typename V>(auto* arr, int i) -> V* {
        return &reinterpret_cast<V*>(arr)[i / sizeof(V)];
    };

    void* registerSpace = sp;

    assertCalleeIsReferenced(callFrame, instance);
    WasmOperationPrologueCallFrameTracer tracer(instance->vm(), callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(instance->vm());

    auto* importableFunction = *access.operator()<WasmOrJSImportableFunctionCallLinkInfo*>(callFrame, WasmToJSCallableFunctionSlot);
    auto typeIndex = importableFunction->typeIndex;
    const TypeDefinition& typeDefinition = TypeInformation::get(typeIndex).expand();
    const auto& signature = *typeDefinition.as<FunctionSignature>();

    auto* globalObject = instance->globalObject();

    auto wasmCC = wasmCallingConvention().callInformationFor(typeDefinition, CallRole::Callee);

    JSValue returned = *(reinterpret_cast<JSValue*>(registerSpace));

    if (!signature.returnCount())
        OPERATION_RETURN(scope);

    if (signature.returnCount() == 1) {
        const auto& returnType = signature.returnType(0);
        switch (returnType.kind) {
        case TypeKind::I32: {
            if (!returned.isNumber() || !returned.isInt32()) {
                // slow path
                uint32_t result = JSValue::decode(std::bit_cast<EncodedJSValue>(returned)).toInt32(globalObject);
                OPERATION_RETURN_IF_EXCEPTION(scope);
                *access.operator()<uint64_t>(registerSpace, 0) = static_cast<uint64_t>(result);
            } else {
                uint64_t result = static_cast<uint64_t>(*access.operator()<uint32_t>(registerSpace, 0));
                *access.operator()<uint64_t>(registerSpace, 0) = result;
            }
            break;
        }
        case TypeKind::I64: {
            uint64_t result = JSValue::decode(std::bit_cast<EncodedJSValue>(returned)).toBigInt64(globalObject);
            OPERATION_RETURN_IF_EXCEPTION(scope);
            *access.operator()<uint64_t>(registerSpace, 0) = result;
            break;
        }
        case TypeKind::F32: {
            FPRReg dest = wasmCC.results[0].location.fpr();
            auto offset = GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(dest) * bytesForWidth(Width::Width64);
            if (returned.isNumber()) {
                if (returned.isInt32()) {
                    float result = static_cast<float>(*access.operator()<int32_t>(registerSpace, 0));
                    *access.operator()<float>(registerSpace, offset) = result;
                } else {
#if USE(JSVALUE64)
                    uint64_t intermediate = *access.operator()<uint64_t>(registerSpace, 0) + JSValue::NumberTag;
#else
                    uint64_t intermediate = *access.operator()<uint64_t>(registerSpace, 0);
#endif
                    double d = std::bit_cast<double>(intermediate);
                    *access.operator()<uint64_t>(registerSpace, offset) = static_cast<uint64_t>(std::bit_cast<uint32_t>(static_cast<float>(d)));
                }
            } else {
                float result = static_cast<float>(JSValue::decode(std::bit_cast<EncodedJSValue>(returned)).toNumber(globalObject));
                OPERATION_RETURN_IF_EXCEPTION(scope);
                *access.operator()<uint64_t>(registerSpace, offset) = static_cast<uint64_t>(std::bit_cast<uint32_t>(result));
            }
            break;
        }
        case TypeKind::F64: {
            FPRReg dest = wasmCC.results[0].location.fpr();
            auto offset = GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(dest) * bytesForWidth(Width::Width64);
            if (returned.isNumber()) {
                if (returned.isInt32()) {
                    double result = static_cast<double>(*access.operator()<int32_t>(registerSpace, 0));
                    *access.operator()<double>(registerSpace, offset) = result;
                } else {
#if USE(JSVALUE64)
                    uint64_t intermediate = *access.operator()<uint64_t>(registerSpace, 0) + JSValue::NumberTag;
#else
                    uint64_t intermediate = *access.operator()<uint64_t>(registerSpace, 0);
#endif
                    double d = std::bit_cast<double>(intermediate);
                    *access.operator()<double>(registerSpace, offset) = d;
                }
            } else {
                double result = static_cast<double>(JSValue::decode(std::bit_cast<EncodedJSValue>(returned)).toNumber(globalObject));
                OPERATION_RETURN_IF_EXCEPTION(scope);
                *access.operator()<double>(registerSpace, offset) = result;
            }
            break;
        }
        default:  {
            if (Wasm::isRefType(returnType)) {
                if (Wasm::isExternref(returnType)) {
                    // Do nothing.
                } else if (Wasm::isFuncref(returnType)) {
                    // operationConvertToFuncref
                    JSValue value = JSValue::decode(std::bit_cast<EncodedJSValue>(returned));
                    WebAssemblyFunction* wasmFunction = nullptr;
                    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                    if (!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull()) [[unlikely]] {
                        throwTypeError(globalObject, scope, "Funcref value is not a function"_s);
                        OPERATION_RETURN(scope);
                    }

                    if (isRefWithTypeIndex(returnType) && !value.isNull()) {
                        Wasm::TypeIndex paramIndex = returnType.index;
                        Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
                        if (paramIndex != argIndex) {
                            throwVMTypeError(globalObject, scope, "Argument function did not match the reference type"_s);
                            OPERATION_RETURN(scope);
                        }
                    }
                    // Validation only: value not modified so write back not needed.
                } else {
                    // operationConvertToAnyref
                    JSValue value = JSValue::decode(std::bit_cast<EncodedJSValue>(returned));
                    value = Wasm::internalizeExternref(value);
                    if (!Wasm::TypeInformation::isReferenceValueAssignable(value, returnType.isNullable(), returnType.index)) [[unlikely]] {
                        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                        OPERATION_RETURN(scope);
                    }
                    *access.operator()<uint64_t>(registerSpace, 0) = std::bit_cast<uint64_t>(value);
                }
            } else
                RELEASE_ASSERT_NOT_REACHED();
        }
        }

        OPERATION_RETURN(scope);
    }

    unsigned iterationCount = 0;
    MarkedArgumentBuffer buffer;
    buffer.ensureCapacity(signature.returnCount());
    forEachInIterable(globalObject, returned, [&](VM&, JSGlobalObject*, JSValue value) -> void {
        if (buffer.size() < signature.returnCount()) {
            buffer.append(value);
            if (buffer.hasOverflowed()) [[unlikely]]
                throwOutOfMemoryError(globalObject, scope);
        }
        ++iterationCount;
    });
    OPERATION_RETURN_IF_EXCEPTION(scope);

    if (buffer.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope, "JS results to Wasm are too large"_s);
        OPERATION_RETURN(scope);
    }

    if (iterationCount != signature.returnCount()) {
        throwVMTypeError(globalObject, scope, "Incorrect number of values returned to Wasm from JS"_s);
        OPERATION_RETURN(scope);
    }

    for (unsigned index = 0; index < buffer.size(); ++index) {
        JSValue value = buffer.at(index);

        uint64_t unboxedValue = 0;
        const auto& returnType = signature.returnType(index);
        switch (returnType.kind) {
        case TypeKind::I32:
            unboxedValue = static_cast<uint32_t>(value.toInt32(globalObject));
            OPERATION_RETURN_IF_EXCEPTION(scope);
            break;
        case TypeKind::I64:
            unboxedValue = value.toBigInt64(globalObject);
            OPERATION_RETURN_IF_EXCEPTION(scope);
            break;
        case TypeKind::F32:
            unboxedValue = std::bit_cast<uint32_t>(value.toFloat(globalObject));
            OPERATION_RETURN_IF_EXCEPTION(scope);
            break;
        case TypeKind::F64:
            unboxedValue = std::bit_cast<uint64_t>(value.toNumber(globalObject));
            OPERATION_RETURN_IF_EXCEPTION(scope);
            break;
        default: {
            if (Wasm::isRefType(returnType)) {
                if (isExternref(returnType)) {
                    // Do nothing.
                } else if (isFuncref(returnType)) {
                    WebAssemblyFunction* wasmFunction = nullptr;
                    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                    if (!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull()) [[unlikely]] {
                        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                        OPERATION_RETURN(scope);
                    }
                    if (Wasm::isRefWithTypeIndex(returnType) && !value.isNull()) {
                        Wasm::TypeIndex paramIndex = returnType.index;
                        Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
                        if (paramIndex != argIndex) {
                            throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                            OPERATION_RETURN(scope);
                        }
                    }
                } else {
                    value = Wasm::internalizeExternref(value);
                    if (!Wasm::TypeInformation::isReferenceValueAssignable(value, returnType.isNullable(), returnType.index)) [[unlikely]] {
                        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                        OPERATION_RETURN(scope);
                    }
                }
            } else
                RELEASE_ASSERT_NOT_REACHED();
            unboxedValue = std::bit_cast<uint64_t>(value);
        }
        }

        auto rep = wasmCC.results[index];
        if (rep.location.isGPR()) {
            auto offset = GPRInfo::toArgumentIndex(rep.location.jsr().payloadGPR()) * sizeof(UCPURegister);
            *access.operator()<uint64_t>(registerSpace, offset) = unboxedValue;
        } else if (rep.location.isFPR()) {
            auto offset = GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(rep.location.fpr()) * bytesForWidth(Width::Width64);
            *access.operator()<uint64_t>(registerSpace, offset) = unboxedValue;
        } else
            *access.operator()<uint64_t>(callFrame, rep.location.offsetFromFP()) = unboxedValue;
    }

    OPERATION_RETURN(scope);
}

#if ENABLE(WEBASSEMBLY_OMGJIT)
static bool shouldTriggerOMGCompile(TierUpCount& tierUp, OMGCallee* replacement, FunctionCodeIndex functionIndex)
{
    if (!OMGPlan::ensureGlobalOMGAllowlist().containsWasmFunction(functionIndex)) {
        dataLogLnIf(Options::verboseOSR(), "\tNot optimizing ", functionIndex, " as it's not in the allow list.");
        tierUp.deferIndefinitely();
        return false;
    }

    if (!replacement && !tierUp.checkIfOptimizationThresholdReached()) {
        dataLogLnIf(Options::verboseOSR(), "\tdelayOMGCompile counter = ", tierUp, " for ", functionIndex);
        dataLogLnIf(Options::verboseOSR(), "\tChoosing not to OMG-optimize ", functionIndex, " yet.");
        return false;
    }
    return true;
}

static void triggerOMGReplacementCompile(TierUpCount& tierUp, JSWebAssemblyInstance* instance, Wasm::CalleeGroup& calleeGroup, FunctionCodeIndex functionIndex)
{
    MemoryMode memoryMode = instance->memory()->mode();
    bool compile = false;
    {
        Locker locker { tierUp.getLock() };
        switch (tierUp.compilationStatusForOMG(memoryMode)) {
        case TierUpCount::CompilationStatus::StartCompilation:
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationResult::CompilationDeferred);
            return;
        case TierUpCount::CompilationStatus::NotCompiled:
            compile = true;
            tierUp.setCompilationStatusForOMG(memoryMode, TierUpCount::CompilationStatus::StartCompilation);
            break;
        case TierUpCount::CompilationStatus::Compiled:
            tierUp.optimizeSoon(functionIndex);
            return;
        default:
            break;
        }
    }

    if (compile) {
        dataLogLnIf(Options::verboseOSR(), "\ttriggerOMGReplacement for ", functionIndex);
        // We need to compile the code.
        Ref<Plan> plan = adoptRef(*new OMGPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), functionIndex, calleeGroup.mode(), Plan::dontFinalize()));
        ensureWorklist().enqueue(plan.copyRef());
        if (!Options::useConcurrentJIT()) [[unlikely]]
            plan->waitForCompletion();
        else
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationResult::CompilationDeferred);
    }
}

// loadValuesIntoBuffer assumes context saved vector-width FPRs.
void loadValuesIntoBuffer(Probe::Context& context, const StackMap& values, Wasm::Context::ScratchBufferEntry* buffer)
{
    constexpr bool verbose = false || WasmOperationsInternal::verbose;
    dataLogLnIf(verbose, "loadValuesIntoBuffer: values.size() = ", values.size());
    for (unsigned index = 0; index < values.size(); ++index) {
        const OSREntryValue& value = values[index];
        dataLogLnIf(Options::verboseOSR() || verbose, "OMG OSR entry values[", index, "] ", value.type(), " ", value);
#if USE(JSVALUE32_64)
        if (value.isRegPair(B3::ValueRep::OSRValueRep)) {
            std::bit_cast<uint32_t*>(buffer + index)[0] = context.gpr(value.gprLo(B3::ValueRep::OSRValueRep));
            std::bit_cast<uint32_t*>(buffer + index)[1] = context.gpr(value.gprHi(B3::ValueRep::OSRValueRep));
            dataLogLnIf(verbose, "GPR Pair for value ", index, " ",
                value.gprLo(B3::ValueRep::OSRValueRep), " = ", context.gpr(value.gprLo(B3::ValueRep::OSRValueRep)), " ",
                value.gprHi(B3::ValueRep::OSRValueRep), " = ", context.gpr(value.gprHi(B3::ValueRep::OSRValueRep)));
        } else
#endif

        if (value.isGPR()) {
            switch (value.type().kind()) {
            case B3::Float:
            case B3::Double:
                RELEASE_ASSERT_NOT_REACHED();
            default:
                *std::bit_cast<uint64_t*>(buffer + index) = context.gpr(value.gpr());
            }
            dataLogLnIf(verbose, "GPR for value ", index, " ", value.gpr(), " = ", context.gpr(value.gpr()));
        } else if (value.isFPR()) {
            switch (value.type().kind()) {
            case B3::Float:
            case B3::Double:
                dataLogLnIf(verbose, "FPR for value ", index, " ", value.fpr(), " = ", context.fpr(value.fpr()));
                *std::bit_cast<double*>(buffer + index) = context.fpr(value.fpr());
                break;
            case B3::V128:
#if CPU(X86_64) || CPU(ARM64)
                dataLogLnIf(verbose, "Vector FPR for value ", index, " ", value.fpr(), " = ", context.vector(value.fpr()));
                *std::bit_cast<v128_t*>(buffer + index) = context.vector(value.fpr());
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
                *std::bit_cast<float*>(buffer + index) = value.floatValue();
                break;
            case B3::Double:
                *std::bit_cast<double*>(buffer + index) = value.doubleValue();
                break;
            case B3::V128:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            default:
                *std::bit_cast<uint64_t*>(buffer + index) = value.value();
            }
        } else if (value.isStack()) {
            auto* baseLoad = std::bit_cast<uint8_t*>(context.fp()) + value.offsetFromFP();
            auto* baseStore = std::bit_cast<uint8_t*>(buffer + index);

            if (value.type().isFloat() || value.type().isVector()) {
                dataLogLnIf(verbose, "Stack float or vector for value ", index, " fp offset ", value.offsetFromFP(), " = ",
                    *std::bit_cast<v128_t*>(baseLoad),
                    " or double ", *std::bit_cast<double*>(baseLoad));
            } else
                dataLogLnIf(verbose, "Stack for value ", index, " fp[", value.offsetFromFP(), "] = ", RawHex(*std::bit_cast<uint64_t*>(baseLoad)), " ", static_cast<int32_t>(*std::bit_cast<uint64_t*>(baseLoad)), " ", *std::bit_cast<int64_t*>(baseLoad));

            switch (value.type().kind()) {
            case B3::Float:
                *std::bit_cast<float*>(baseStore) = *std::bit_cast<float*>(baseLoad);
                break;
            case B3::Double:
                *std::bit_cast<double*>(baseStore) = *std::bit_cast<double*>(baseLoad);
                break;
            case B3::V128:
                *std::bit_cast<v128_t*>(baseStore) = *std::bit_cast<v128_t*>(baseLoad);
                break;
            default:
                *std::bit_cast<uint64_t*>(baseStore) = *std::bit_cast<uint64_t*>(baseLoad);
                break;
            }
        } else
            RELEASE_ASSERT_NOT_REACHED();
    }
}

SUPPRESS_ASAN
static void doOSREntry(JSWebAssemblyInstance* instance, Probe::Context& context, BBQCallee& callee, OMGOSREntryCallee& osrEntryCallee, OSREntryData& osrEntryData)
{
    auto returnWithoutOSREntry = [&] {
        context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = 0;
    };

    RELEASE_ASSERT(osrEntryCallee.osrEntryScratchBufferSize() == osrEntryData.values().size());

    auto* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryCallee.osrEntryScratchBufferSize());
    if (!buffer)
        return returnWithoutOSREntry();

    dataLogLnIf(Options::verboseOSR(), callee, ": OMG OSR entry: functionCodeIndex=", osrEntryData.functionIndex(), " got entry callee ", RawPointer(&osrEntryCallee));

    // 1. Place required values in scratch buffer.
    loadValuesIntoBuffer(context, osrEntryData.values(), buffer);

    // 2. Restore callee saves.
    auto dontRestoreRegisters = RegisterSetBuilder::stackRegisters();
    for (const RegisterAtOffset& entry : *callee.calleeSaveRegisters()) {
        if (dontRestoreRegisters.contains(entry.reg(), IgnoreVectors))
            continue;
        if (entry.reg().isGPR())
            context.gpr(entry.reg().gpr()) = *std::bit_cast<UCPURegister*>(std::bit_cast<uint8_t*>(context.fp()) + entry.offset());
        else
            context.fpr(entry.reg().fpr()) = *std::bit_cast<double*>(std::bit_cast<uint8_t*>(context.fp()) + entry.offset());
    }

    // 3. Function epilogue, like a tail-call.
    UCPURegister* framePointer = std::bit_cast<UCPURegister*>(context.fp());
#if CPU(X86_64)
    // move(framePointerRegister, stackPointerRegister);
    // pop(framePointerRegister);
    context.fp() = std::bit_cast<UCPURegister*>(*framePointer);
    context.sp() = framePointer + 1;
    static_assert(prologueStackPointerDelta() == sizeof(void*) * 1);
#elif CPU(ARM64E) || CPU(ARM64)
    // move(framePointerRegister, stackPointerRegister);
    // popPair(framePointerRegister, linkRegister);
    context.fp() = std::bit_cast<UCPURegister*>(*framePointer);
    context.gpr(ARM64Registers::lr) = std::bit_cast<UCPURegister>(*(framePointer + 1));
    context.sp() = framePointer + 2;
    static_assert(prologueStackPointerDelta() == sizeof(void*) * 2);
#if CPU(ARM64E)
    // LR needs to be untagged since OSR entry function prologue will tag it with SP. This is similar to tail-call.
    context.gpr(ARM64Registers::lr) = std::bit_cast<UCPURegister>(untagCodePtrWithStackPointerForJITCall(context.gpr<void*>(ARM64Registers::lr), context.sp()));
#endif
#elif CPU(RISCV64)
    // move(framePointerRegister, stackPointerRegister);
    // popPair(framePointerRegister, linkRegister);
    context.fp() = std::bit_cast<UCPURegister*>(*framePointer);
    context.gpr(RISCV64Registers::ra) = std::bit_cast<UCPURegister>(*(framePointer + 1));
    context.sp() = framePointer + 2;
    static_assert(prologueStackPointerDelta() == sizeof(void*) * 2);
#elif CPU(ARM)
    context.fp() = std::bit_cast<UCPURegister*>(*framePointer);
    context.gpr(ARMRegisters::lr) = std::bit_cast<UCPURegister>(*(framePointer + 1));
    context.sp() = framePointer + 2;
    static_assert(prologueStackPointerDelta() == sizeof(void*) * 2);
#else
#error Unsupported architecture.
#endif
    // 4. Configure argument registers to jump to OSR entry from the caller of this runtime function.
    context.gpr(GPRInfo::argumentGPR0) = std::bit_cast<UCPURegister>(buffer); // Modify this only when we definitely tier up.
    context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = std::bit_cast<UCPURegister>(osrEntryCallee.entrypoint().taggedPtr<>());
}

inline bool shouldOMGJIT(JSWebAssemblyInstance* instance, unsigned functionIndex)
{
    auto& info = instance->module().moduleInformation();
    if (info.functions[functionIndex].data.size() > Options::maximumOMGCandidateCost())
        return false;
    if (!Options::wasmFunctionIndexRangeToCompile().isInRange(functionIndex))
        return false;
    return true;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmTriggerTierUpNow, void, (CallFrame* callFrame, JSWebAssemblyInstance* instance))
{
    BBQCallee& callee = uncheckedDowncast<BBQCallee>(uncheckedDowncast<Wasm::Callee>(*callFrame->callee().asNativeCallee()));
    ASSERT(callee.compilationMode() == CompilationMode::BBQMode);

    Wasm::CalleeGroup& calleeGroup = *instance->calleeGroup();
    ASSERT(instance->memory()->mode() == calleeGroup.mode());

    FunctionSpaceIndex functionIndexInSpace = callee.index();
    FunctionCodeIndex functionIndex = calleeGroup.toCodeIndex(functionIndexInSpace);
    TierUpCount& tierUp = callee.tierUpCounter();
    if (!shouldOMGJIT(instance, functionIndex)) {
        tierUp.deferIndefinitely();
        return;
    }

    if (shouldTriggerOMGCompile(tierUp, nullptr, functionIndex))
        triggerOMGReplacementCompile(tierUp, instance, calleeGroup, functionIndex);
}
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT)
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmTriggerOSREntryNow, void, (Probe::Context& context))
{
    OSREntryData& osrEntryData = *context.arg<OSREntryData*>();
    auto functionIndex = osrEntryData.functionIndex();
    uint32_t loopIndex = osrEntryData.loopIndex();
    JSWebAssemblyInstance* instance = context.gpr<JSWebAssemblyInstance*>(GPRInfo::wasmContextInstancePointer);
    BBQCallee& callee = uncheckedDowncast<BBQCallee>(uncheckedDowncast<Wasm::Callee>(*context.gpr<CallFrame*>(MacroAssembler::framePointerRegister)->callee().asNativeCallee()));
    ASSERT(callee.compilationMode() == Wasm::CompilationMode::BBQMode);
    ASSERT(callee.refCount());
    Wasm::CalleeGroup& calleeGroup = *instance->calleeGroup();

    auto returnWithoutOSREntry = [&] {
        context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = 0;
    };

    auto doStackCheck = [instance](OMGOSREntryCallee* callee) -> bool {
        uintptr_t stackPointer = reinterpret_cast<uintptr_t>(currentStackPointer());
        ASSERT(callee->stackCheckSize());
        if (callee->stackCheckSize() == stackCheckNotNeeded)
            return true;
        uintptr_t stackExtent = stackPointer - callee->stackCheckSize();
        uintptr_t stackLimit = reinterpret_cast<uintptr_t>(instance->softStackLimit());
        if (stackExtent >= stackPointer || stackExtent <= stackLimit) [[unlikely]] {
            dataLogLnIf(Options::verboseOSR(), "\tSkipping OMG loop tier up due to stack check; ", RawHex(stackPointer), " -> ", RawHex(stackExtent), " is past soft limit ", RawHex(stackLimit));
            return false;
        }
        return true;
    };

    MemoryMode memoryMode = instance->memory()->mode();
    ASSERT(memoryMode == calleeGroup.mode());

    TierUpCount& tierUp = callee.tierUpCounter();
    if (!shouldOMGJIT(instance, functionIndex)) {
        tierUp.deferIndefinitely();
        return returnWithoutOSREntry();
    }

    RefPtr<OMGCallee> replacement = calleeGroup.tryGetOMGCalleeConcurrently(functionIndex);
    dataLogLnIf(Options::verboseOSR(), callee, ": Consider OSREntryPlan for functionCodeIndex=", osrEntryData.functionIndex(),  " loopIndex#", loopIndex, " with executeCounter = ", tierUp, " ", RawPointer(replacement.get()));

    if (!Options::useWasmOSR()) {
        if (shouldTriggerOMGCompile(tierUp, replacement.get(), functionIndex))
            triggerOMGReplacementCompile(tierUp, instance, calleeGroup, functionIndex);

        // We already have an OMG replacement.
        if (replacement) {
            // No OSR entry points. Just defer indefinitely.
            if (tierUp.osrEntryTriggers().isEmpty()) {
                tierUp.dontOptimizeAnytimeSoon(functionIndex);
                return;
            }

            // FIXME: Is this ever taken?
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
        compilationStatus = tierUp.compilationStatusForOMGForOSREntry(memoryMode);
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
        dataLogLnIf(Options::verboseOSR(), "\tdelayOMGCompile still compiling for ", functionIndex);
        tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationResult::CompilationDeferred);
        return returnWithoutOSREntry();
    }

    if (OMGOSREntryCallee* osrEntryCallee = callee.osrEntryCallee()) {
        if (osrEntryCallee->loopIndex() == loopIndex) {
            if (!doStackCheck(osrEntryCallee))
                return returnWithoutOSREntry();
            return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);
        }
    }

    if (!shouldTriggerOMGCompile(tierUp, replacement.get(), functionIndex) && !triggeredSlowPathToStartCompilation)
        return returnWithoutOSREntry();

    if (!triggeredSlowPathToStartCompilation) {
        triggerOMGReplacementCompile(tierUp, instance, calleeGroup, functionIndex);

        if (!replacement)
            return returnWithoutOSREntry();
    }

    if (OMGOSREntryCallee* osrEntryCallee = callee.osrEntryCallee()) {
        if (osrEntryCallee->loopIndex() == loopIndex) {
            if (!doStackCheck(osrEntryCallee))
                return returnWithoutOSREntry();
            return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);
        }
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
                dataLogLnIf(Options::verboseOSR(), "\tInner-loop loopIndex#", loopIndex, " in ", functionIndex, " setting parent loop loopIndex#", currentLoopIndex, "'s trigger and backing off.");
                tierUp.osrEntryTriggers()[currentLoopIndex] = TierUpCount::TriggerReason::StartCompilation;
                return true;
            }
            return false;
        };

        if (tryTriggerOuterLoopToCompile()) {
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationResult::CompilationDeferred);
            return returnWithoutOSREntry();
        }
    }

    bool startOSREntryCompilation = false;
    {
        Locker locker { tierUp.getLock() };
        if (tierUp.compilationStatusForOMGForOSREntry(memoryMode) == TierUpCount::CompilationStatus::NotCompiled) {
            tierUp.setCompilationStatusForOMGForOSREntry(memoryMode, TierUpCount::CompilationStatus::StartCompilation);
            startOSREntryCompilation = true;
            // Currently, we do not have a way to jettison wasm code. This means that once we decide to compile OSR entry code for a particular loopIndex,
            // we cannot throw the compiled code so long as Wasm module is live. We immediately disable all the triggers.
            for (auto& trigger : tierUp.osrEntryTriggers())
                trigger = TierUpCount::TriggerReason::DontTrigger;
        }
    }

    if (startOSREntryCompilation) {
        dataLogLnIf(Options::verboseOSR(), "\ttriggerOMGOSR for ", functionIndex);
        Ref<Plan> plan = adoptRef(*new OSREntryPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), Ref<Wasm::BBQCallee>(callee), functionIndex, loopIndex, calleeGroup.mode(), Plan::dontFinalize()));
        ensureWorklist().enqueue(plan.copyRef());
        if (!Options::useConcurrentJIT()) [[unlikely]]
            plan->waitForCompletion();
        else
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationResult::CompilationDeferred);
    }

    OMGOSREntryCallee* osrEntryCallee = callee.osrEntryCallee();
    if (!osrEntryCallee) {
        tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationResult::CompilationDeferred);
        return returnWithoutOSREntry();
    }

    if (osrEntryCallee->loopIndex() == loopIndex) {
        if (!doStackCheck(osrEntryCallee))
            return returnWithoutOSREntry();
        return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);
    }

    tierUp.dontOptimizeAnytimeSoon(functionIndex);
    return returnWithoutOSREntry();
}

#endif
#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmLoopOSREnterBBQJIT, void, (Probe::Context & context))
{
    // We just populated the callee in the frame before we entered this operation, so let's use it.
    BBQCallee& callee = uncheckedDowncast<BBQCallee>(uncheckedDowncast<Wasm::Callee>(*context.fp<CallFrame*>()->callee().asNativeCallee()));
    ASSERT(callee.compilationMode() == Wasm::CompilationMode::BBQMode);
    ASSERT(callee.refCount());

    auto* osrEntryScratchBuffer = std::bit_cast<Wasm::Context::ScratchBufferEntry*>(context.gpr(GPRInfo::argumentGPR0));
    unsigned loopIndex = *std::bit_cast<uint64_t*>(osrEntryScratchBuffer); // First entry in scratch buffer is the loop index when tiering up to BBQ.

    OSREntryData& entryData = callee.tierUpCounter().osrEntryData(loopIndex);
    RELEASE_ASSERT(entryData.loopIndex() == loopIndex);

    const StackMap& stackMap = entryData.values();
    auto writeValueToRep = [&](Wasm::Context::ScratchBufferEntry* bufferEntry, const OSREntryValue& value) {
        uint64_t* bufferSlot = std::bit_cast<uint64_t*>(bufferEntry);
        B3::Type type = value.type();
        // Void signifies an unused exception slot in `try` (since we can't have an exception at that time)
        if (type.kind() == B3::TypeKind::Void)
            return;
        if (value.isGPR()) {
            ASSERT(!type.isFloat() && !type.isVector());
            context.gpr(value.gpr()) = *bufferSlot;
#if USE(JSVALUE32_64)
        } else if (value.isRegPair(B3::ValueRep::OSRValueRep)) {
            uint64_t encodedValue = *bufferSlot;
            context.gpr(value.gprHi(B3::ValueRep::OSRValueRep)) = (encodedValue >> 32) & 0xffffffff;
            context.gpr(value.gprLo(B3::ValueRep::OSRValueRep)) = encodedValue & 0xffffffff;
#endif
        } else if (value.isFPR()) {
            if (type.isVector()) {
#if CPU(X86_64) || CPU(ARM64)
                context.vector(value.fpr()) = *std::bit_cast<v128_t*>(bufferSlot);
#else
                UNREACHABLE_FOR_PLATFORM();
#endif
            } else
                context.fpr(value.fpr()) = *std::bit_cast<double*>(bufferSlot);
        } else if (value.isStack()) {
            auto* baseStore = std::bit_cast<uint8_t*>(context.fp()) + value.offsetFromFP();
            switch (type.kind()) {
            case B3::Int32:
                *std::bit_cast<uint32_t*>(baseStore) = static_cast<uint32_t>(*bufferSlot);
                break;
            case B3::Int64:
                *std::bit_cast<uint64_t*>(baseStore) = *bufferSlot;
                break;
            case B3::Float:
                *std::bit_cast<float*>(baseStore) = std::bit_cast<float>(static_cast<uint32_t>(*bufferSlot));
                break;
            case B3::Double:
                *std::bit_cast<double*>(baseStore) = std::bit_cast<double>(*bufferSlot);
                break;
            case B3::V128:
                *std::bit_cast<v128_t*>(baseStore) = *std::bit_cast<v128_t*>(bufferSlot);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        } else
            RELEASE_ASSERT_NOT_REACHED();
    };

    unsigned indexInScratchBuffer = BBQCallee::extraOSRValuesForLoopIndex;
    for (const auto& entry : stackMap)
        writeValueToRep(&osrEntryScratchBuffer[indexInScratchBuffer++], entry);

    context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = std::bit_cast<UCPURegister>(callee.loopEntrypoints()[loopIndex].taggedPtr());
}

#endif

#if ENABLE(WEBASSEMBLY_BBQJIT)
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmMaterializeBaselineData, void, (CallFrame* callFrame, JSWebAssemblyInstance* instance))
{
    BBQCallee& callee = uncheckedDowncast<BBQCallee>(uncheckedDowncast<Wasm::Callee>(*callFrame->callee().asNativeCallee()));
    ASSERT(callee.compilationMode() == CompilationMode::BBQMode);

    Wasm::CalleeGroup& calleeGroup = *instance->calleeGroup();
    ASSERT(instance->memory()->mode() == calleeGroup.mode());

    FunctionSpaceIndex functionIndexInSpace = callee.index();
    FunctionCodeIndex functionIndex = calleeGroup.toCodeIndex(functionIndexInSpace);
    instance->ensureBaselineData(functionIndex);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmMaterializePolymorphicCallee, WasmOrJSImportableFunction*, (CallProfile* callProfile, WasmOrJSImportableFunction* importableFunction))
{
    callProfile->observeCallIndirect(importableFunction->boxedCallee.encodedBits());
    return importableFunction;
}
#endif

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmUnwind, void*, (JSWebAssemblyInstance* instance))
{
    ASSERT(instance->type() == WebAssemblyInstanceType);
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
#if ASSERT_ENABLED
    // JS -> Wasm might throw before it's transitioned to a NativeCallee. It should have restored any callee saves at this point already though.
    if (callFrame->callee().isNativeCallee())
        assertCalleeIsReferenced(callFrame, instance);
#endif
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

JSC_DEFINE_JIT_OPERATION(operationConvertToI64, int64_t, (JSWebAssemblyInstance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::decode(v).toBigInt64(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToF64, double, (JSWebAssemblyInstance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::decode(v).toNumber(globalObject));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToI32, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, toUCPUStrictInt32(JSValue::decode(v).toInt32(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToF32, float, (JSWebAssemblyInstance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, static_cast<float>(JSValue::decode(v).toNumber(globalObject)));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToFuncref, EncodedJSValue, (JSWebAssemblyInstance* instance, const TypeDefinition* type, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(v);
    WebAssemblyFunction* wasmFunction = nullptr;
    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
    if (!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull()) [[unlikely]]
        OPERATION_RETURN(scope, throwVMTypeError(globalObject, scope, "Argument value did not match the reference type"_s));

    const FunctionSignature* signature = type->as<FunctionSignature>();
    ASSERT(signature->returnCount() == 1);
    Type resultType = signature->returnType(0);

    if (isRefWithTypeIndex(resultType) && !value.isNull()) {
        Wasm::TypeIndex paramIndex = resultType.index;
        Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
        if (paramIndex != argIndex)
            OPERATION_RETURN(scope, throwVMTypeError(globalObject, scope, "Argument value did not match the reference type"_s));
    }

    OPERATION_RETURN(scope, v);
}

JSC_DEFINE_JIT_OPERATION(operationConvertToAnyref, EncodedJSValue, (JSWebAssemblyInstance* instance, const TypeDefinition* type, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const FunctionSignature* signature = type->as<FunctionSignature>();
    ASSERT(signature->returnCount() == 1);
    Type resultType = signature->returnType(0);

    JSValue value = JSValue::decode(v);
    value = Wasm::internalizeExternref(value);
    if (!Wasm::TypeInformation::isReferenceValueAssignable(value, resultType.isNullable(), resultType.index)) [[unlikely]]
        OPERATION_RETURN(scope, throwVMTypeError(globalObject, scope, "Argument value did not match the reference type"_s));

    OPERATION_RETURN(scope, JSValue::encode(value));
}

JSC_DEFINE_JIT_OPERATION(operationConvertToBigInt, EncodedJSValue, (JSWebAssemblyInstance* instance, EncodedWasmValue value))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(JSBigInt::makeHeapBigIntOrBigInt32(globalObject, value)));
}

// https://webassembly.github.io/multi-value/js-api/index.html#run-a-host-function
JSC_DEFINE_JIT_OPERATION(operationIterateResults, void, (JSWebAssemblyInstance* instance, const TypeDefinition* type, EncodedJSValue encResult, uint64_t* registerResults, uint64_t* calleeFramePointer))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const FunctionSignature* signature = type->as<FunctionSignature>();

    auto wasmCallInfo = wasmCallingConvention().callInformationFor(*type, CallRole::Callee);
    RegisterAtOffsetList registerResultOffsets = wasmCallInfo.computeResultsOffsetList();

    unsigned iterationCount = 0;
    MarkedArgumentBuffer buffer;
    buffer.ensureCapacity(signature->returnCount());
    JSValue result = JSValue::decode(encResult);
    forEachInIterable(globalObject, result, [&](VM&, JSGlobalObject*, JSValue value) -> void {
        if (buffer.size() < signature->returnCount()) {
            buffer.append(value);
            if (buffer.hasOverflowed()) [[unlikely]]
                throwOutOfMemoryError(globalObject, scope);
        }
        ++iterationCount;
    });
    OPERATION_RETURN_IF_EXCEPTION(scope);

    if (buffer.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope, "JS results to Wasm are too large"_s);
        OPERATION_RETURN(scope);
    }

    if (iterationCount != signature->returnCount()) {
        throwVMTypeError(globalObject, scope, "Incorrect number of values returned to Wasm from JS"_s);
        OPERATION_RETURN(scope);
    }

    for (unsigned index = 0; index < buffer.size(); ++index) {
        JSValue value = buffer.at(index);

        uint64_t unboxedValue = 0;
        const auto& returnType = signature->returnType(index);
        switch (returnType.kind) {
        case TypeKind::I32:
            unboxedValue = static_cast<uint32_t>(value.toInt32(globalObject));
            break;
        case TypeKind::I64:
            unboxedValue = value.toBigInt64(globalObject);
            break;
        case TypeKind::F32:
            unboxedValue = std::bit_cast<uint32_t>(value.toFloat(globalObject));
            break;
        case TypeKind::F64:
            unboxedValue = std::bit_cast<uint64_t>(value.toNumber(globalObject));
            break;
        default: {
            if (Wasm::isRefType(returnType)) {
                if (isExternref(returnType)) {
                    // Do nothing.
                } else if (isFuncref(returnType)) {
                    WebAssemblyFunction* wasmFunction = nullptr;
                    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                    if (!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull()) [[unlikely]] {
                        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                        OPERATION_RETURN(scope);
                    }
                    if (Wasm::isRefWithTypeIndex(returnType) && !value.isNull()) {
                        Wasm::TypeIndex paramIndex = returnType.index;
                        Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
                        if (paramIndex != argIndex) {
                            throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                            OPERATION_RETURN(scope);
                        }
                    }
                } else {
                    value = Wasm::internalizeExternref(value);
                    if (!Wasm::TypeInformation::isReferenceValueAssignable(value, returnType.isNullable(), returnType.index)) [[unlikely]] {
                        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                        OPERATION_RETURN(scope);
                    }
                }
            } else
                RELEASE_ASSERT_NOT_REACHED();
            unboxedValue = std::bit_cast<uint64_t>(value);
        }
        }
        OPERATION_RETURN_IF_EXCEPTION(scope);

        auto rep = wasmCallInfo.results[index];
        if (rep.location.isGPR())
            registerResults[registerResultOffsets.find(rep.location.jsr().payloadGPR())->offset() / sizeof(uint64_t)] = unboxedValue;
        else if (rep.location.isFPR())
            registerResults[registerResultOffsets.find(rep.location.fpr())->offset() / sizeof(uint64_t)] = unboxedValue;
        else
            calleeFramePointer[rep.location.offsetFromFP() / sizeof(uint64_t)] = unboxedValue;
    }
    OPERATION_RETURN(scope);
}

// FIXME: It would be much easier to inline this when we have a global GC, which could probably mean we could avoid
// spilling the results onto the stack.
// Saved result registers should be placed on the stack just above the last stack result.
JSC_DEFINE_JIT_OPERATION(operationAllocateResultsArray, JSArray*, (JSWebAssemblyInstance* instance, const FunctionSignature* signature, IndexingType indexingType, JSValue* stackPointerFromCallee))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ObjectInitializationScope initializationScope(vm);
    JSArray* result = JSArray::tryCreateUninitializedRestricted(initializationScope, nullptr, globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType), signature->returnCount());

    if (!result)
        throwOutOfMemoryError(globalObject, scope);

    auto wasmCallInfo = wasmCallingConvention().callInformationFor(*signature);
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

    OPERATION_RETURN(scope, result);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmWriteBarrierSlowPath, void, (JSCell * cell, VM* vmPointer))
{
    ASSERT(cell);
    ASSERT(vmPointer);
    VM& vm = *vmPointer;
    vm.writeBarrierSlowPath(cell);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationPopcount32, UCPUStrictInt32, (int32_t value))
{
    return toUCPUStrictInt32(std::popcount(static_cast<uint32_t>(value)));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationPopcount64, uint64_t, (int64_t value))
{
    return std::popcount(static_cast<uint64_t>(value));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationGrowMemory, UCPUStrictInt32, (JSWebAssemblyInstance* instance, int32_t delta))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    return toUCPUStrictInt32(growMemory(instance, delta));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmMemoryFill, UCPUStrictInt32, (JSWebAssemblyInstance* instance, uint32_t dstAddress, uint32_t targetValue, uint32_t count))
{
    return toUCPUStrictInt32(memoryFill(instance, dstAddress, targetValue, count));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmMemoryCopy, UCPUStrictInt32, (JSWebAssemblyInstance* instance, uint32_t dstAddress, uint32_t srcAddress, uint32_t count))
{
    return toUCPUStrictInt32(memoryCopy(instance, dstAddress, srcAddress, count));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationGetWasmTableElement, EncodedJSValue, (JSWebAssemblyInstance* instance, unsigned tableIndex, int32_t signedIndex))
{
    return tableGet(instance, tableIndex, signedIndex);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationSetWasmTableElement, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned tableIndex, uint32_t signedIndex, EncodedJSValue encValue))
{
    return toUCPUStrictInt32(tableSet(instance, tableIndex, signedIndex, encValue));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmTableInit, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned elementIndex, unsigned tableIndex, uint32_t dstOffset, uint32_t srcOffset, uint32_t length))
{
    return toUCPUStrictInt32(tableInit(instance, elementIndex, tableIndex, dstOffset, srcOffset, length));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmElemDrop, void, (JSWebAssemblyInstance* instance, unsigned elementIndex))
{
    return elemDrop(instance, elementIndex);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmTableGrow, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned tableIndex, EncodedJSValue fill, uint32_t delta))
{
    return toUCPUStrictInt32(tableGrow(instance, tableIndex, fill, delta));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmTableFill, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned tableIndex, uint32_t offset, EncodedJSValue fill, uint32_t count))
{
    return toUCPUStrictInt32(tableFill(instance, tableIndex, offset, fill, count));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmTableCopy, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned dstTableIndex, unsigned srcTableIndex, int32_t dstOffset, int32_t srcOffset, int32_t length))
{
    return toUCPUStrictInt32(tableCopy(instance, dstTableIndex, srcTableIndex, dstOffset, srcOffset, length));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRefFunc, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t index))
{
    return refFunc(instance, index);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmStructNewEmpty, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    WebAssemblyGCStructure* structure = instance->gcObjectStructure(typeIndex);
    auto* result = JSWebAssemblyStruct::tryCreate(vm, structure);
    if (!result) [[unlikely]]
        return JSValue::encode(jsNull());
    return JSValue::encode(result);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationGetWasmTableSize, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned tableIndex))
{
    return toUCPUStrictInt32(tableSize(instance, tableIndex));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicWait32, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int32_t value, int64_t timeoutInNanoseconds))
{
    return toUCPUStrictInt32(memoryAtomicWait32(instance, base, offset, value, timeoutInNanoseconds));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicWait64, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int64_t value, int64_t timeoutInNanoseconds))
{
    return toUCPUStrictInt32(memoryAtomicWait64(instance, base, offset, value, timeoutInNanoseconds));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicNotify, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int32_t countValue))
{
    return toUCPUStrictInt32(memoryAtomicNotify(instance, base, offset, countValue));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmMemoryInit, UCPUStrictInt32, (JSWebAssemblyInstance* instance, unsigned dataSegmentIndex, uint32_t dstAddress, uint32_t srcAddress, uint32_t length))
{
    return toUCPUStrictInt32(memoryInit(instance, dataSegmentIndex, dstAddress, srcAddress, length));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmDataDrop, void, (JSWebAssemblyInstance* instance, unsigned dataSegmentIndex))
{
    return dataDrop(instance, dataSegmentIndex);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmThrow, void*, (JSWebAssemblyInstance* instance, unsigned exceptionIndex, uint64_t* arguments))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSGlobalObject* globalObject = instance->globalObject();

    Ref<const Wasm::Tag> tag = instance->tag(exceptionIndex);

    FixedVector<uint64_t> values(tag->parameterBufferSize());
    for (unsigned i = 0; i < tag->parameterBufferSize(); ++i)
        values[i] = arguments[i];

    ASSERT(tag->type().returnsVoid());
    JSWebAssemblyException* exception = JSWebAssemblyException::create(vm, globalObject->webAssemblyExceptionStructure(), WTF::move(tag), WTF::move(values));
    throwException(globalObject, throwScope, exception);

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRethrow, void*, (JSWebAssemblyInstance* instance, EncodedJSValue encodedThrownValue))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSGlobalObject* globalObject = instance->globalObject();

    JSValue thrownValue = JSValue::decode(encodedThrownValue);
    throwException(globalObject, throwScope, thrownValue);

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmToJSException, void*, (JSWebAssemblyInstance* instance, Wasm::ExceptionType type))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    return throwWasmToJSException(callFrame, type, instance);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationThrowExceptionFromOMG, void*, (JSWebAssemblyInstance* instance, Wasm::ExceptionType type, void* returnAddress))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    // This operation is called from a thunk. Thus we need a return address from OMG callee.
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, removeCodePtrTag(returnAddress));
    return throwWasmToJSException(callFrame, type, instance);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationCrashDueToBBQStackOverflow, void, ())
{
    // We have crashed because of a mismatch between the stack check in the wasm slow path loop_osr and the BBQ JIT LoopOSREntrypoint.
    // This really should never happen. We make this separate operation to have a clean crash log.
    bool hiddenReturn = true;
    if (hiddenReturn)
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(false);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationCrashDueToOMGStackOverflow, void, ())
{
    bool hiddenReturn = true;
    if (hiddenReturn)
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(false);
}

#if USE(JSVALUE64)
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRetrieveAndClearExceptionIfCatchable, ThrownExceptionInfo, (JSWebAssemblyInstance* instance))
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
    bool didClear = throwScope.tryClearException();
    // If we had a pending termination exception it should have propagated past any wasm catch handlers.
    ASSERT_UNUSED(didClear, didClear);

    return { JSValue::encode(thrownValue), jumpTarget };
}
#else
// Same as JSVALUE64 version, but returns thrownValue on stack
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRetrieveAndClearExceptionIfCatchable, void*, (JSWebAssemblyInstance* instance, EncodedJSValue* thrownValue))
{
    VM& vm = instance->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(!!throwScope.exception());

    vm.callFrameForCatch = nullptr;
    auto* jumpTarget = std::exchange(vm.targetMachinePCAfterCatch, nullptr);

    Exception* exception = throwScope.exception();
    *thrownValue = JSValue::encode(exception->value());

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    (void)throwScope.tryClearException();

    return jumpTarget;
}
#endif // USE(JSVALUE64)

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNew, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size, uint64_t value))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto* structure = instance->gcObjectStructure(typeIndex);
    return JSValue::encode(arrayNew(instance, structure, size, value));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewVector, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size, uint64_t lane0, uint64_t lane1))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    auto* structure = instance->gcObjectStructure(typeIndex);
    return JSValue::encode(arrayNew(instance, structure, size, v128_t { lane0, lane1 }));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewData, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t dataSegmentIndex, uint32_t arraySize, uint32_t offset))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    return arrayNewData(instance, typeIndex, dataSegmentIndex, arraySize, offset);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewElem, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t elemSegmentIndex, uint32_t arraySize, uint32_t offset))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);

    return arrayNewElem(instance, typeIndex, elemSegmentIndex, arraySize, offset);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewEmpty, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);

    ASSERT(typeIndex < instance->module().moduleInformation().typeCount());
    WebAssemblyGCStructure* structure = instance->gcObjectStructure(typeIndex);
    auto* array = JSWebAssemblyArray::tryCreate(vm, structure, size);
    if (!array) [[unlikely]]
        return JSValue::encode(jsNull());

    // Create a default-initialized array with the right element type and length
    return JSValue::encode(array);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayFill, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue arrayValue, uint32_t offset, uint64_t value, uint32_t size))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    return toUCPUStrictInt32(arrayFill(vm, arrayValue, offset, value, size));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayFillVector, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue arrayValue, uint32_t offset, uint64_t lane0, uint64_t lane1, uint32_t size))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    WasmOperationPrologueCallFrameTracer tracer(vm, callFrame, OUR_RETURN_ADDRESS);
    return toUCPUStrictInt32(arrayFill(vm, arrayValue, offset, v128_t { lane0, lane1 }, size));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayCopy, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue dst, uint32_t dstOffset, EncodedJSValue src, uint32_t srcOffset, uint32_t size))
{
    return toUCPUStrictInt32(arrayCopy(instance, dst, dstOffset, src, srcOffset, size));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayInitElem, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue dst, uint32_t dstOffset, uint32_t srcElementIndex, uint32_t srcOffset, uint32_t size))
{
    return toUCPUStrictInt32(arrayInitElem(instance, dst, dstOffset, srcElementIndex, srcOffset, size));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayInitData, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue dst, uint32_t dstOffset, uint32_t srcDataIndex, uint32_t srcOffset, uint32_t size))
{
    return toUCPUStrictInt32(arrayInitData(instance, dst, dstOffset, srcDataIndex, srcOffset, size));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmAnyConvertExtern, EncodedJSValue, (EncodedJSValue reference))
{
    return externInternalize(reference);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRefTest, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue reference, uint32_t allowNull, int32_t heapType, bool shouldNegate))
{
    // Explicitly return 1 or 0 because bool in C++ only reqiures that the bottom bit match the other bits can be anything.
    int32_t truth = shouldNegate ? 0 : 1;
    int32_t falsity = shouldNegate ? 1 : 0;

    if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(heapType))) {
        bool result = Wasm::refCast(reference, static_cast<bool>(allowNull), static_cast<Wasm::TypeIndex>(heapType), nullptr);
        return toUCPUStrictInt32(result ? truth : falsity);
    }

    auto& info = instance->module().moduleInformation();
    bool result = Wasm::refCast(reference, static_cast<bool>(allowNull), info.typeSignatures[heapType]->index(), info.rtts[heapType].ptr());
    return toUCPUStrictInt32(result ? truth : falsity);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRefCast, EncodedJSValue, (JSWebAssemblyInstance* instance, EncodedJSValue reference, uint32_t allowNull, int32_t heapType))
{
    if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(heapType))) {
        if (!Wasm::refCast(reference, static_cast<bool>(allowNull), static_cast<Wasm::TypeIndex>(heapType), nullptr)) [[unlikely]]
            return encodedJSValue();
        return reference;
    }

    auto& info = instance->module().moduleInformation();
    if (!Wasm::refCast(reference, static_cast<bool>(allowNull), info.typeSignatures[heapType]->index(), info.rtts[heapType].ptr())) [[unlikely]]
        return encodedJSValue();
    return reference;
}

}
} // namespace JSC::Wasm

IGNORE_WARNINGS_END

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
