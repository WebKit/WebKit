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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Wasm {

namespace WasmOperationsInternal {
static constexpr bool verbose = false;
}

JSC_DEFINE_JIT_OPERATION(operationJSToWasmEntryWrapperBuildFrame, JSEntrypointCallee*, (void* sp, CallFrame* callFrame, WebAssemblyFunction* function))
{
    dataLogLnIf(WasmOperationsInternal::verbose, "operationJSToWasmEntryWrapperBuildFrame sp: ", RawPointer(sp), " fp: ", RawPointer(callFrame));

    auto* globalObject = function->globalObject();
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    auto* callee = function->m_jsToWasmCallee.ptr();
    ASSERT(function);
    ASSERT(callee->ident() == 0xBF);
    ASSERT(callee->typeIndex() == function->typeIndex());
    ASSERT(callee->frameSize() + JSEntrypointCallee::SpillStackSpaceAligned == (reinterpret_cast<uintptr_t>(callFrame) - reinterpret_cast<uintptr_t>(sp)));
    dataLogLnIf(WasmOperationsInternal::verbose, "operationJSToWasmEntryWrapperBuildFrame setting callee: ", RawHex(CalleeBits::encodeNativeCallee(callee)));
    dataLogLnIf(WasmOperationsInternal::verbose, "operationJSToWasmEntryWrapperBuildFrame wasm callee: ", RawHex(callee->wasmCallee()));

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto calleeSPOffsetFromFP = -(static_cast<intptr_t>(callee->frameSize()) + JSEntrypointCallee::SpillStackSpaceAligned - JSEntrypointCallee::RegisterStackSpaceAligned);

    const TypeDefinition& signature = TypeInformation::get(function->typeIndex()).expand();
    const FunctionSignature& functionSignature = *signature.as<FunctionSignature>();
    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);

    if (UNLIKELY(functionSignature.argumentsOrResultsIncludeV128())) {
        throwVMTypeError(globalObject, scope, Wasm::errorMessageForExceptionType(Wasm::ExceptionType::TypeErrorInvalidV128Use));
        OPERATION_RETURN(scope, callee);
    }

    auto access = [sp, callFrame]<typename V>(auto* arr, int i) -> V* {
        dataLogLnIf(WasmOperationsInternal::verbose, "fp[", (&reinterpret_cast<uint8_t*>(arr)[i / sizeof(uint8_t)] - reinterpret_cast<uint8_t*>(callFrame)), "] sp[", (&reinterpret_cast<uint8_t*>(arr)[i / sizeof(uint8_t)] - reinterpret_cast<uint8_t*>(sp)), "](", RawHex(reinterpret_cast<V*>(arr)[i / sizeof(V)]), ")");
        return &reinterpret_cast<V*>(arr)[i / sizeof(V)];
    };

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
                *access.operator()<uint32_t>(registerSpace, dst) = static_cast<uint32_t>(value);
            else
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
    NativeCallFrameTracer tracer(vm, callFrame);

    uint64_t* registerSpace = reinterpret_cast<uint64_t*>(sp);
    auto* callee = static_cast<JSEntrypointCallee*>(callFrame->callee().asNativeCallee());
    ASSERT(callee->ident() == 0xBF);

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
    ObjectInitializationScope initializationScope(vm);
    DeferGCForAWhile deferGCForAWhile(vm);

    JSArray* resultArray = JSArray::tryCreateUninitializedRestricted(initializationScope, nullptr, instance->globalObject()->arrayStructureForIndexingTypeDuringAllocation(indexingType), functionSignature.returnCount());
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto calleeSPOffsetFromFP = -(static_cast<intptr_t>(callee->frameSize()) + JSEntrypointCallee::SpillStackSpaceAligned - JSEntrypointCallee::RegisterStackSpaceAligned);

    for (unsigned i = 0; i < functionSignature.returnCount(); ++i) {
        ValueLocation loc = wasmFrameConvention.results[i].location;
        Type type = functionSignature.returnType(i);
        if (loc.isGPR() || loc.isFPR()) {
            JSValue result;
            switch (type.kind) {
            case TypeKind::I32:
                result = jsNumber(*access.operator()<int32_t>(registerSpace, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) * sizeof(UCPURegister)));
                break;
            case TypeKind::I64:
                result = JSBigInt::makeHeapBigIntOrBigInt32(instance->globalObject(), *access.operator()<int64_t>(registerSpace, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) * sizeof(UCPURegister)));
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
            resultArray->initializeIndex(initializationScope, i, result);
        } else {
            JSValue result;
            switch (type.kind) {
            case TypeKind::I32:
                result = jsNumber(*access.operator()<int32_t>(callFrame, calleeSPOffsetFromFP + loc.offsetFromSP()));
                break;
            case TypeKind::I64:
                result = JSBigInt::makeHeapBigIntOrBigInt32(instance->globalObject(), *access.operator()<int64_t>(callFrame, calleeSPOffsetFromFP + loc.offsetFromSP()));
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
            resultArray->initializeIndex(initializationScope, i, result);
        }
    }

    OPERATION_RETURN(scope, JSValue::encode(resultArray));
}

JSC_DEFINE_JIT_OPERATION(operationGetWasmCalleeStackSize, EncodedJSValue, (JSWebAssemblyInstance* instance, Wasm::Callee* callee))
{
    auto& module = instance->module();

    auto typeIndex = module.moduleInformation().typeIndexFromFunctionIndexSpace(callee->index());
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

    VM& vm = instance->vm();

    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, stackOffset);
}

JSC_DEFINE_JIT_OPERATION(operationWasmToJSExitMarshalArguments, bool, (void* sp, CallFrame* cfr, void* argumentRegisters, JSWebAssemblyInstance* instance))
{
    auto access = []<typename V>(auto* arr, int i) -> V* {
        return &reinterpret_cast<V*>(arr)[i / sizeof(V)];
    };

    CallFrame* calleeFrame = std::bit_cast<CallFrame*>(reinterpret_cast<uintptr_t>(sp) - sizeof(CallerFrameAndPC));
    ASSERT(instance);
    ASSERT(instance->globalObject());
    VM& vm = instance->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Wasm::Callee* callee = *access.operator()<Wasm::Callee*>(cfr, -0x10);
    auto functionIndex = callee->index();
    auto& module = instance->module();
    auto typeIndex = module.moduleInformation().typeIndexFromFunctionIndexSpace(functionIndex);
    const TypeDefinition& typeDefinition = TypeInformation::get(typeIndex).expand();
    const auto& signature = *typeDefinition.as<FunctionSignature>();
    unsigned argCount = signature.argumentCount();

    const auto& wasmCC = wasmCallingConvention().callInformationFor(typeDefinition, CallRole::Caller);
    const auto& jsCC = jsCallingConvention().callInformationFor(typeDefinition, CallRole::Callee);

    if (Options::useWasmSIMD() && (wasmCC.argumentsOrResultsIncludeV128))
        OPERATION_RETURN(scope, false);

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
        case TypeKind::Nullref:
        case TypeKind::Nullfuncref:
        case TypeKind::Nullexternref:
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
        case TypeKind::Exn:
        case TypeKind::I32: {
            if (wasmParam.isStackArgument()) {
                uint64_t raw = *access.operator()<uint64_t>(cfr, wasmParam.offsetFromSP() + sizeof(CallerFrameAndPC));
#if USE(JSVALUE64)
                if (argType.isI32())
                    *access.operator()<uint64_t>(calleeFrame, dst) = static_cast<uint32_t>(raw) | JSValue::NumberTag;
#else
                if (false);
#endif
                else
                    *access.operator()<uint64_t>(calleeFrame, dst) = raw;
            } else {
                auto raw = *access.operator()<uint64_t>(argumentRegisters, GPRInfo::toArgumentIndex(wasmParam.jsr().payloadGPR()) * sizeof(UCPURegister));
#if USE(JSVALUE64)
                if (argType.isI32())
                    *access.operator()<uint64_t>(calleeFrame, dst) = static_cast<uint32_t>(raw) | JSValue::NumberTag;
                else
                    *access.operator()<uint64_t>(calleeFrame, dst) = raw;
#else
                *access.operator()<uint32_t>(calleeFrame, dst) = raw;
#endif
            }
            break;
        }
        case TypeKind::I64: {
            if (wasmParam.isStackArgument()) {
                auto result = JSBigInt::makeHeapBigIntOrBigInt32(instance->globalObject(), *access.operator()<int64_t>(cfr, wasmParam.offsetFromSP() + sizeof(CallerFrameAndPC)));
                OPERATION_RETURN_IF_EXCEPTION(scope, false);
                *access.operator()<uint64_t>(calleeFrame, dst) = JSValue::encode(result);
            } else {
                auto result = JSBigInt::makeHeapBigIntOrBigInt32(instance->globalObject(), *access.operator()<int64_t>(argumentRegisters, GPRInfo::toArgumentIndex(wasmParam.jsr().payloadGPR()) * bytesForWidth(Width::Width64)));
                OPERATION_RETURN_IF_EXCEPTION(scope, false);
                *access.operator()<uint64_t>(calleeFrame, dst) = JSValue::encode(result);
            }
            break;
        }
        case TypeKind::F32: {
            float val;
            if (wasmParam.isStackArgument())
                val = *access.operator()<float>(cfr, wasmParam.offsetFromSP() + sizeof(CallerFrameAndPC));
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
            if (wasmParam.isStackArgument())
                val = *access.operator()<double>(cfr, wasmParam.offsetFromSP() + sizeof(CallerFrameAndPC));
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
    *access.operator()<uint64_t>(calleeFrame, CallFrameSlot::thisArgument* static_cast<int>(sizeof(Register))) = JSValue::encode(jsUndefined());

    // materializeImportJSCell and store
    auto* newCallee = instance->importFunctionInfo(functionIndex);
    *access.operator()<uintptr_t>(calleeFrame, CallFrameSlot::callee * static_cast<int>(sizeof(Register))) = std::bit_cast<uintptr_t>(newCallee->importFunction);
    *access.operator()<uint32_t>(calleeFrame, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset) = argCount + 1; // including this = +1

    // set up codeblock
    auto singletonCallee = CalleeBits::boxNativeCallee(&WasmToJSCallee::singleton());
    *access.operator()<uintptr_t>(cfr, CallFrameSlot::codeBlock * sizeof(Register)) = std::bit_cast<uintptr_t>(instance);
    *access.operator()<uintptr_t>(cfr, CallFrameSlot::callee * sizeof(Register)) = std::bit_cast<uintptr_t>(singletonCallee);

    OPERATION_RETURN(scope, true);
}

JSC_DEFINE_JIT_OPERATION(operationWasmToJSExitMarshalReturnValues, void, (void* sp, CallFrame* cfr, JSWebAssemblyInstance* instance))
{
    auto access = []<typename V>(auto* arr, int i) -> V* {
        return &reinterpret_cast<V*>(arr)[i / sizeof(V)];
    };

    void* registerSpace = sp;
    void* callerStackFrame = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(cfr) + sizeof(CallerFrameAndPC));

    auto scope = DECLARE_THROW_SCOPE(instance->vm());

    Wasm::Callee* callee = *access.operator()<Wasm::Callee*>(cfr, -0x10);
    auto functionIndex = callee->index();
    auto& module = instance->module();
    auto typeIndex = module.moduleInformation().typeIndexFromFunctionIndexSpace(functionIndex);
    const TypeDefinition& typeDefinition = TypeInformation::get(typeIndex).expand();
    const auto& signature = *typeDefinition.as<FunctionSignature>();

    auto* globalObject = instance->globalObject();

    auto wasmCC = wasmCallingConvention().callInformationFor(typeDefinition, CallRole::Caller);
    auto jsCC = jsCallingConvention().callInformationFor(typeDefinition, CallRole::Callee);

    JSValue returned = *(reinterpret_cast<JSValue*>(registerSpace));

    if (signature.returnCount() == 1) {
        const auto& returnType = signature.returnType(0);
        switch (returnType.kind) {
        case TypeKind::I32: {
            if (!returned.isNumber() || !returned.isInt32()) {
                // slow path
                uint32_t result = JSValue::decode(std::bit_cast<EncodedJSValue>(returned)).toInt32(globalObject);
                *access.operator()<uint32_t>(registerSpace, 0) = result;
            } else {
                uint64_t result = static_cast<uint64_t>(*access.operator()<uint32_t>(registerSpace, 0));
                *access.operator()<uint32_t>(registerSpace, 0) = result;
            }
            break;
        }
        case TypeKind::I64: {
            uint64_t result = JSValue::decode(std::bit_cast<EncodedJSValue>(returned)).toBigInt64(globalObject);
            *access.operator()<uint64_t>(registerSpace, 0) = result;
            break;
        }
        case TypeKind::F32: {
            FPRReg dest = wasmCC.results[0].location.fpr();
            auto offset = GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(dest) * bytesForWidth(Width::Width64);
            if (returned.isNumber()) {
                if (returned.isInt32()) {
                    float result = static_cast<float>(*access.operator()<uint32_t>(registerSpace, 0));
                    *access.operator()<float>(registerSpace, offset) = result;
                } else {
#if USE(JSVALUE64)
                    uint64_t intermediate = *access.operator()<uint64_t>(registerSpace, 0) + JSValue::NumberTag;
#else
                    uint64_t intermediate = *access.operator()<uint64_t>(registerSpace, 0);
#endif
                    double d = std::bit_cast<double>(intermediate);
                    *access.operator()<float>(registerSpace, offset) = static_cast<float>(d);
                }
            } else {
                float result = static_cast<float>(JSValue::decode(std::bit_cast<EncodedJSValue>(returned)).toNumber(globalObject));
                *access.operator()<float>(registerSpace, offset) = result;
            }
            break;
        }
        case TypeKind::F64: {
            FPRReg dest = wasmCC.results[0].location.fpr();
            auto offset = GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(dest) * bytesForWidth(Width::Width64);
            if (returned.isNumber()) {
                if (returned.isInt32()) {
                    double result = static_cast<float>(*access.operator()<uint32_t>(registerSpace, 0));
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
                *access.operator()<double>(registerSpace, offset) = result;
            }
            break;
        }
        default:  {
            if (Wasm::isRefType(returnType)) {
                if (Wasm::isExternref(returnType)) {
                    // Do nothing.
                } else if (Wasm::isFuncref(returnType) || (!Options::useWasmGC() && isRefWithTypeIndex(returnType))) {
                    // operationConvertToFuncref
                    JSValue value = JSValue::decode(std::bit_cast<EncodedJSValue>(returned));
                    WebAssemblyFunction* wasmFunction = nullptr;
                    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                    if (UNLIKELY(!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull())) {
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
                } else {
                    // operationConvertToAnyref
                    JSValue value = JSValue::decode(std::bit_cast<EncodedJSValue>(returned));
                    value = Wasm::internalizeExternref(value);
                    if (UNLIKELY(!Wasm::TypeInformation::castReference(value, returnType.isNullable(), returnType.index))) {
                        throwTypeError(globalObject, scope, "Argument value did not match reference type"_s);
                        OPERATION_RETURN(scope);
                    }
                }
                // do nothing, the register is already there
            } else
                RELEASE_ASSERT_NOT_REACHED();
        }
        }
    } else if (signature.returnCount() > 1) {
        // operationIterateResults
        JSValue result = JSValue::decode(std::bit_cast<EncodedJSValue>(returned));

        unsigned iterationCount = 0;
        MarkedArgumentBuffer buffer;
        buffer.ensureCapacity(signature.returnCount());
        forEachInIterable(globalObject, result, [&](VM&, JSGlobalObject*, JSValue value) -> void {
            if (buffer.size() < signature.returnCount()) {
                buffer.append(value);
                if (UNLIKELY(buffer.hasOverflowed()))
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
                unboxedValue = value.toInt32(globalObject);
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
                    } else if (isFuncref(returnType) || (!Options::useWasmGC() && isRefWithTypeIndex(returnType))) {
                        WebAssemblyFunction* wasmFunction = nullptr;
                        WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                        if (UNLIKELY(!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull())) {
                            throwTypeError(globalObject, scope, "Funcref value is not a function"_s);
                            OPERATION_RETURN(scope);
                        }
                        if (Wasm::isRefWithTypeIndex(returnType) && !value.isNull()) {
                            Wasm::TypeIndex paramIndex = returnType.index;
                            Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
                            if (paramIndex != argIndex) {
                                throwTypeError(globalObject, scope, "Argument function did not match the reference type"_s);
                                OPERATION_RETURN(scope);
                            }
                        }
                    } else {
                        ASSERT(Options::useWasmGC());
                        value = Wasm::internalizeExternref(value);
                        if (UNLIKELY(!Wasm::TypeInformation::castReference(value, returnType.isNullable(), returnType.index))) {
                            throwTypeError(globalObject, scope, "Argument value did not match reference type"_s);
                            OPERATION_RETURN(scope);
                        }
                    }
                } else
                    RELEASE_ASSERT_NOT_REACHED();
                unboxedValue = std::bit_cast<uint64_t>(value);
            }
            }
            OPERATION_RETURN_IF_EXCEPTION(scope);

            auto rep = wasmCC.results[index];
            if (rep.location.isGPR())
                *access.operator()<uint64_t>(registerSpace, GPRInfo::toArgumentIndex(rep.location.jsr().payloadGPR()) * sizeof(UCPURegister)) = unboxedValue;
            else if (rep.location.isFPR())
                *access.operator()<uint64_t>(registerSpace, GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister) + FPRInfo::toArgumentIndex(rep.location.fpr()) * bytesForWidth(Width::Width64)) = unboxedValue;
            else
                *access.operator()<uint64_t>(callerStackFrame, rep.location.offsetFromSP()) = unboxedValue;
        }
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

static void triggerOMGReplacementCompile(TierUpCount& tierUp, OMGCallee* replacement, JSWebAssemblyInstance* instance, Wasm::CalleeGroup& calleeGroup, FunctionCodeIndex functionIndex, std::optional<bool> hasExceptionHandlers)
{
    if (replacement) {
        tierUp.optimizeSoon(functionIndex);
        return;
    }

    MemoryMode memoryMode = instance->memory()->mode();
    bool compile = false;
    {
        Locker locker { tierUp.getLock() };
        switch (tierUp.compilationStatusForOMG(memoryMode)) {
        case TierUpCount::CompilationStatus::StartCompilation:
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
            return;
        case TierUpCount::CompilationStatus::NotCompiled:
            compile = true;
            tierUp.setCompilationStatusForOMG(memoryMode, TierUpCount::CompilationStatus::StartCompilation);
            break;
        default:
            break;
        }
    }

    if (compile) {
        dataLogLnIf(Options::verboseOSR(), "\ttriggerOMGReplacement for ", functionIndex);
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
    ASSERT(Options::useWasmSIMD() || savedFPWidth == SavedFPWidth::DontSaveVectors);
    unsigned valueSize = (savedFPWidth == SavedFPWidth::SaveVectors) ? 2 : 1;

    constexpr bool verbose = false || WasmOperationsInternal::verbose;
    dataLogLnIf(verbose, "loadValuesIntoBuffer: valueSize = ", valueSize, "; values.size() = ", values.size());
    for (unsigned index = 0; index < values.size(); ++index) {
        const OSREntryValue& value = values[index];
        dataLogLnIf(Options::verboseOSR() || verbose, "OMG OSR entry values[", index, "] ", value.type(), " ", value);
#if USE(JSVALUE32_64)
        if (value.isRegPair(B3::ValueRep::OSRValueRep)) {
            std::bit_cast<uint32_t*>(buffer + index * valueSize)[0] = context.gpr(value.gprLo(B3::ValueRep::OSRValueRep));
            std::bit_cast<uint32_t*>(buffer + index * valueSize)[1] = context.gpr(value.gprHi(B3::ValueRep::OSRValueRep));
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
                *std::bit_cast<uint64_t*>(buffer + index * valueSize) = context.gpr(value.gpr());
            }
            dataLogLnIf(verbose, "GPR for value ", index, " ", value.gpr(), " = ", context.gpr(value.gpr()));
        } else if (value.isFPR()) {
            switch (value.type().kind()) {
            case B3::Float:
            case B3::Double:
                dataLogLnIf(verbose, "FPR for value ", index, " ", value.fpr(), " = ", context.fpr(value.fpr(), savedFPWidth));
                *std::bit_cast<double*>(buffer + index * valueSize) = context.fpr(value.fpr(), savedFPWidth);
                break;
            case B3::V128:
                RELEASE_ASSERT(valueSize == 2);
#if CPU(X86_64) || CPU(ARM64)
                dataLogLnIf(verbose, "Vector FPR for value ", index, " ", value.fpr(), " = ", context.vector(value.fpr()));
                *std::bit_cast<v128_t*>(buffer + index * valueSize) = context.vector(value.fpr());
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
                *std::bit_cast<float*>(buffer + index * valueSize) = value.floatValue();
                break;
            case B3::Double:
                *std::bit_cast<double*>(buffer + index * valueSize) = value.doubleValue();
                break;
            case B3::V128:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            default:
                *std::bit_cast<uint64_t*>(buffer + index * valueSize) = value.value();
            }
        } else if (value.isStack()) {
            auto* baseLoad = std::bit_cast<uint8_t*>(context.fp()) + value.offsetFromFP();
            auto* baseStore = std::bit_cast<uint8_t*>(buffer + index * valueSize);

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

    unsigned valueSize = (callee.savedFPWidth() == SavedFPWidth::SaveVectors) ? 2 : 1;
    RELEASE_ASSERT(osrEntryCallee.osrEntryScratchBufferSize() == valueSize * osrEntryData.values().size());

    uint64_t* buffer = instance->vm().wasmContext.scratchBufferForSize(osrEntryCallee.osrEntryScratchBufferSize());
    if (!buffer)
        return returnWithoutOSREntry();

    dataLogLnIf(Options::verboseOSR(), callee, ": OMG OSR entry: functionCodeIndex=", osrEntryData.functionIndex(), " got entry callee ", RawPointer(&osrEntryCallee));

    // 1. Place required values in scratch buffer.
    loadValuesIntoBuffer(context, osrEntryData.values(), buffer, callee.savedFPWidth());

    // 2. Restore callee saves.
    auto dontRestoreRegisters = RegisterSetBuilder::stackRegisters();
    for (const RegisterAtOffset& entry : *callee.calleeSaveRegisters()) {
        if (dontRestoreRegisters.contains(entry.reg(), IgnoreVectors))
            continue;
        if (entry.reg().isGPR())
            context.gpr(entry.reg().gpr()) = *std::bit_cast<UCPURegister*>(std::bit_cast<uint8_t*>(context.fp()) + entry.offset());
        else
            context.fpr(entry.reg().fpr(), callee.savedFPWidth()) = *std::bit_cast<double*>(std::bit_cast<uint8_t*>(context.fp()) + entry.offset());
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
    BBQCallee& callee = *static_cast<BBQCallee*>(callFrame->callee().asNativeCallee());
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

    OMGCallee* replacement;
    {
        Locker locker { calleeGroup.m_lock };
        replacement = calleeGroup.omgCallee(locker, functionIndex);
    }
    dataLogLnIf(Options::verboseOSR(), callee, ": Consider OMGPlan for functionCodeIndex=", functionIndex, " with executeCounter = ", tierUp, " ", RawPointer(replacement));

    if (shouldTriggerOMGCompile(tierUp, replacement, functionIndex))
        triggerOMGReplacementCompile(tierUp, replacement, instance, calleeGroup, functionIndex, callee.hasExceptionHandlers());

    // We already have an OMG replacement.
    if (replacement) {
        // No OSR entry points. Just defer indefinitely.
        if (tierUp.osrEntryTriggers().isEmpty()) {
            dataLogLnIf(Options::verboseOSR(), "\tdelayOMGCompile replacement in place, delaying indefinitely for ", functionIndex);
            tierUp.dontOptimizeAnytimeSoon(functionIndex);
            return;
        }

        // Found one OSR entry point. Since we do not have a way to jettison Wasm::Callee right now, this means that tierUp function is now meaningless.
        // Not call it as much as possible.
        if (callee.osrEntryCallee()) {
            dataLogLnIf(Options::verboseOSR(), "\tdelayOMGCompile trigger in place, delaying indefinitely for ", functionIndex);
            tierUp.dontOptimizeAnytimeSoon(functionIndex);
            return;
        }
    }
}
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT)
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmTriggerOSREntryNow, void, (Probe::Context& context))
{
    OSREntryData& osrEntryData = *context.arg<OSREntryData*>();
    auto functionIndex = osrEntryData.functionIndex();
    uint32_t loopIndex = osrEntryData.loopIndex();
    JSWebAssemblyInstance* instance = context.gpr<JSWebAssemblyInstance*>(GPRInfo::wasmContextInstancePointer);
    BBQCallee& callee = *static_cast<BBQCallee*>(context.gpr<CallFrame*>(MacroAssembler::framePointerRegister)->callee().asNativeCallee());
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
        if (UNLIKELY(stackExtent >= stackPointer || stackExtent <= stackLimit)) {
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

    OMGCallee* replacement;
    {
        Locker locker { calleeGroup.m_lock };
        replacement = calleeGroup.omgCallee(locker, functionIndex);
    }
    dataLogLnIf(Options::verboseOSR(), callee, ": Consider OSREntryPlan for functionCodeIndex=", osrEntryData.functionIndex(),  " loopIndex#", loopIndex, " with executeCounter = ", tierUp, " ", RawPointer(replacement));

    if (!Options::useWasmOSR()) {
        if (shouldTriggerOMGCompile(tierUp, replacement, functionIndex))
            triggerOMGReplacementCompile(tierUp, replacement, instance, calleeGroup, functionIndex, callee.hasExceptionHandlers());

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
        tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
        return returnWithoutOSREntry();
    }

    if (OMGOSREntryCallee* osrEntryCallee = callee.osrEntryCallee()) {
        if (osrEntryCallee->loopIndex() == loopIndex) {
            if (!doStackCheck(osrEntryCallee))
                return returnWithoutOSREntry();
            return doOSREntry(instance, context, callee, *osrEntryCallee, osrEntryData);
        }
    }

    if (!shouldTriggerOMGCompile(tierUp, replacement, functionIndex) && !triggeredSlowPathToStartCompilation)
        return returnWithoutOSREntry();

    if (!triggeredSlowPathToStartCompilation) {
        triggerOMGReplacementCompile(tierUp, replacement, instance, calleeGroup, functionIndex, callee.hasExceptionHandlers());

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
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
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
        Ref<Plan> plan = adoptRef(*new OSREntryPlan(instance->vm(), Ref<Wasm::Module>(instance->module()), Ref<Wasm::BBQCallee>(callee), functionIndex, callee.hasExceptionHandlers(), loopIndex, calleeGroup.mode(), Plan::dontFinalize()));
        ensureWorklist().enqueue(plan.copyRef());
        if (UNLIKELY(!Options::useConcurrentJIT()))
            plan->waitForCompletion();
        else
            tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
    }

    OMGOSREntryCallee* osrEntryCallee = callee.osrEntryCallee();
    if (!osrEntryCallee) {
        tierUp.setOptimizationThresholdBasedOnCompilationResult(functionIndex, CompilationDeferred);
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
    uint64_t* osrEntryScratchBuffer = std::bit_cast<uint64_t*>(context.gpr(GPRInfo::argumentGPR0));
    unsigned loopIndex = osrEntryScratchBuffer[0]; // First entry in scratch buffer is the loop index when tiering up to BBQ.

    // We just populated the callee in the frame before we entered this operation, so let's use it.
    BBQCallee& callee = *static_cast<BBQCallee*>(context.fp<CallFrame*>()->callee().asNativeCallee());
    ASSERT(callee.compilationMode() == Wasm::CompilationMode::BBQMode);
    ASSERT(callee.refCount());
    OSREntryData& entryData = callee.tierUpCounter().osrEntryData(loopIndex);
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
            auto* baseStore = std::bit_cast<uint8_t*>(context.fp()) + value.offsetFromFP();
            switch (type.kind()) {
            case B3::Int32:
                *std::bit_cast<uint32_t*>(baseStore) = static_cast<uint32_t>(encodedValue);
                break;
            case B3::Int64:
                *std::bit_cast<uint64_t*>(baseStore) = encodedValue;
                break;
            case B3::Float:
                *std::bit_cast<float*>(baseStore) = std::bit_cast<float>(static_cast<uint32_t>(encodedValue));
                break;
            case B3::Double:
                *std::bit_cast<double*>(baseStore) = std::bit_cast<double>(encodedValue);
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
        writeValueToRep(osrEntryScratchBuffer[indexInScratchBuffer++], entry);

    context.gpr(GPRInfo::nonPreservedNonArgumentGPR0) = std::bit_cast<UCPURegister>(callee.loopEntrypoints()[loopIndex].taggedPtr());
}

#endif


ALWAYS_INLINE void assertCalleeIsReferenced(CallFrame* frame, JSWebAssemblyInstance* instance)
{
#if ASSERT_ENABLED
    CalleeGroup& calleeGroup = *instance->calleeGroup();
    Wasm::Callee* callee = static_cast<Wasm::Callee*>(frame->callee().asNativeCallee());
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


JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmUnwind, void*, (JSWebAssemblyInstance* instance))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationConvertToI64, int64_t, (JSWebAssemblyInstance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::decode(v).toBigInt64(globalObject);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationConvertToF64, double, (JSWebAssemblyInstance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::decode(v).toNumber(globalObject);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationConvertToI32, int32_t, (JSWebAssemblyInstance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::decode(v).toInt32(globalObject);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationConvertToF32, float, (JSWebAssemblyInstance* instance, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return static_cast<float>(JSValue::decode(v).toNumber(globalObject));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationConvertToFuncref, EncodedJSValue, (JSWebAssemblyInstance* instance, const TypeDefinition* type, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(v);
    WebAssemblyFunction* wasmFunction = nullptr;
    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
    if (UNLIKELY(!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull())) {
        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
        return { };
    }

    const FunctionSignature* signature = type->as<FunctionSignature>();
    ASSERT(signature->returnCount() == 1);
    Type resultType = signature->returnType(0);

    if (isRefWithTypeIndex(resultType) && !value.isNull()) {
        Wasm::TypeIndex paramIndex = resultType.index;
        Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
        if (paramIndex != argIndex)
            return throwVMTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
    }
    return v;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationConvertToAnyref, EncodedJSValue, (JSWebAssemblyInstance* instance, const TypeDefinition* type, EncodedJSValue v))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const FunctionSignature* signature = type->as<FunctionSignature>();
    ASSERT(signature->returnCount() == 1);
    Type resultType = signature->returnType(0);

    JSValue value = JSValue::decode(v);
    value = Wasm::internalizeExternref(value);
    if (UNLIKELY(!Wasm::TypeInformation::castReference(value, resultType.isNullable(), resultType.index))) {
        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
        return { };
    }
    return JSValue::encode(value);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationConvertToBigInt, EncodedJSValue, (JSWebAssemblyInstance* instance, EncodedWasmValue value))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(JSBigInt::makeHeapBigIntOrBigInt32(globalObject, value));
}

// https://webassembly.github.io/multi-value/js-api/index.html#run-a-host-function
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationIterateResults, void, (JSWebAssemblyInstance* instance, const TypeDefinition* type, EncodedJSValue encResult, uint64_t* registerResults, uint64_t* calleeFramePointer))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
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
            unboxedValue = std::bit_cast<uint32_t>(value.toFloat(globalObject));
            break;
        case TypeKind::F64:
            unboxedValue = std::bit_cast<uint64_t>(value.toNumber(globalObject));
            break;
        default: {
            if (Wasm::isRefType(returnType)) {
                if (isExternref(returnType)) {
                    // Do nothing.
                } else if (isFuncref(returnType) || (!Options::useWasmGC() && isRefWithTypeIndex(returnType))) {
                    WebAssemblyFunction* wasmFunction = nullptr;
                    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                    if (UNLIKELY(!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && !value.isNull())) {
                        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                        return;
                    }
                    if (Wasm::isRefWithTypeIndex(returnType) && !value.isNull()) {
                        Wasm::TypeIndex paramIndex = returnType.index;
                        Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
                        if (paramIndex != argIndex) {
                            throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                            return;
                        }
                    }
                } else {
                    ASSERT(Options::useWasmGC());
                    value = Wasm::internalizeExternref(value);
                    if (UNLIKELY(!Wasm::TypeInformation::castReference(value, returnType.isNullable(), returnType.index))) {
                        throwTypeError(globalObject, scope, "Argument value did not match the reference type"_s);
                        return;
                    }
                }
            } else
                RELEASE_ASSERT_NOT_REACHED();
            unboxedValue = std::bit_cast<uint64_t>(value);
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
JSC_DEFINE_JIT_OPERATION(operationAllocateResultsArray, JSArray*, (JSWebAssemblyInstance* instance, const FunctionSignature* signature, IndexingType indexingType, JSValue* stackPointerFromCallee))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    JSGlobalObject* globalObject = instance->globalObject();
    NativeCallFrameTracer tracer(vm, callFrame);
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

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationPopcount32, uint32_t, (int32_t value))
{
    return std::popcount(static_cast<uint32_t>(value));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationPopcount64, uint64_t, (int64_t value))
{
    return std::popcount(static_cast<uint64_t>(value));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationGrowMemory, int32_t, (JSWebAssemblyInstance* instance, int32_t delta))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return growMemory(instance, delta);
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

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmTableGrow, int32_t, (JSWebAssemblyInstance* instance, unsigned tableIndex, EncodedJSValue fill, uint32_t delta))
{
    return tableGrow(instance, tableIndex, fill, delta);
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

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmStructNew, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, bool useDefault, uint64_t* arguments))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return structNew(instance, typeIndex, useDefault, arguments);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmStructNewEmpty, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    JSGlobalObject* globalObject = instance->globalObject();
    auto structRTT = instance->module().moduleInformation().rtts[typeIndex];
    auto newStruct = JSWebAssemblyStruct::tryCreate(globalObject, globalObject->webAssemblyStructStructure(), instance, typeIndex, structRTT);
    return JSValue::encode(newStruct ? newStruct : jsNull());
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmStructGet, EncodedJSValue, (EncodedJSValue encodedStructReference, uint32_t fieldIndex))
{
    return structGet(encodedStructReference, fieldIndex);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmStructSet, void, (JSWebAssemblyInstance* instance, EncodedJSValue encodedStructReference, uint32_t fieldIndex, EncodedJSValue argument))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return structSet(encodedStructReference, fieldIndex, argument);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationGetWasmTableSize, int32_t, (JSWebAssemblyInstance* instance, unsigned tableIndex))
{
    return tableSize(instance, tableIndex);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicWait32, int32_t, (JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int32_t value, int64_t timeoutInNanoseconds))
{
    return memoryAtomicWait32(instance, base, offset, value, timeoutInNanoseconds);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicWait64, int32_t, (JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int64_t value, int64_t timeoutInNanoseconds))
{
    return memoryAtomicWait64(instance, base, offset, value, timeoutInNanoseconds);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicNotify, int32_t, (JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int32_t countValue))
{
    return memoryAtomicNotify(instance, base, offset, countValue);
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
    NativeCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

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
    return vm.targetMachinePCForThrow;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRethrow, void*, (JSWebAssemblyInstance* instance, EncodedJSValue thrownValue))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
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

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmToJSException, void*, (JSWebAssemblyInstance* instance, Wasm::ExceptionType type))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
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
    throwScope.clearException();

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
    throwScope.clearException();

    return jumpTarget;
}
#endif // USE(JSVALUE64)

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNew, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size, uint64_t value))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(arrayNew(instance, typeIndex, size, value));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewVector, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size, uint64_t lane0, uint64_t lane1))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(arrayNew(instance, typeIndex, size, v128_t { lane0, lane1 }));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewData, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t dataSegmentIndex, uint32_t arraySize, uint32_t offset))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return arrayNewData(instance, typeIndex, dataSegmentIndex, arraySize, offset);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewElem, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t elemSegmentIndex, uint32_t arraySize, uint32_t offset))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);

    return arrayNewElem(instance, typeIndex, elemSegmentIndex, arraySize, offset);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewEmpty, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);

    JSGlobalObject* globalObject = instance->globalObject();
    auto arrayRTT = instance->module().moduleInformation().rtts[typeIndex];

    ASSERT(typeIndex < instance->module().moduleInformation().typeCount());
    const Wasm::TypeDefinition& arraySignature = instance->module().moduleInformation().typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::FieldType fieldType = arraySignature.as<ArrayType>()->elementType();

    size_t elementSize = fieldType.type.elementSize();
    if (UNLIKELY(productOverflows<uint32_t>(elementSize, size) || elementSize * size > maxArraySizeInBytes))
        return JSValue::encode(jsNull());

    // Create a default-initialized array with the right element type and length
    JSWebAssemblyArray* array = nullptr;
    if (fieldType.type.is<PackedType>()) {
        switch (fieldType.type.as<PackedType>()) {
        case Wasm::PackedType::I8: {
            FixedVector<uint8_t> v(size);
            v.fill(0); // Prevent GC from tracing uninitialized array slots
            array = JSWebAssemblyArray::tryCreate(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
            break;
        }
        case Wasm::PackedType::I16: {
            FixedVector<uint16_t> v(size);
            v.fill(0);
            array = JSWebAssemblyArray::tryCreate(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
            break;
        }
        }
        return JSValue::encode(array);
    }

    ASSERT(fieldType.type.is<Type>());
    switch (fieldType.type.as<Type>().kind) {
    case Wasm::TypeKind::I32:
    case Wasm::TypeKind::F32: {
        FixedVector<uint32_t> v(size);
        v.fill(0);
        array = JSWebAssemblyArray::tryCreate(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
        break;
    }
    case Wasm::TypeKind::I64:
    case Wasm::TypeKind::F64: {
        FixedVector<uint64_t> v(size);
        v.fill(0);
        array = JSWebAssemblyArray::tryCreate(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
        break;
    }
    case Wasm::TypeKind::Ref:
    case Wasm::TypeKind::RefNull: {
        FixedVector<uint64_t> v(size);
        v.fill(JSValue::encode(jsNull()));
        array = JSWebAssemblyArray::tryCreate(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
        break;
    }
    case Wasm::TypeKind::V128: {
        FixedVector<v128_t> v(size);
        v.fill(vectorAllZeros());
        array = JSWebAssemblyArray::tryCreate(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(v), arrayRTT);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return JSValue::encode(array ? array : jsNull());
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayGet, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, EncodedJSValue arrayValue, uint32_t index))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return arrayGet(instance, typeIndex, arrayValue, index);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArraySet, void, (JSWebAssemblyInstance* instance, uint32_t typeIndex, EncodedJSValue arrayValue, uint32_t index, EncodedJSValue value))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return arraySet(instance, typeIndex, arrayValue, index, value);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayFill, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue arrayValue, uint32_t offset, uint64_t value, uint32_t size))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return toUCPUStrictInt32(arrayFill(arrayValue, offset, value, size));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmArrayFillVector, UCPUStrictInt32, (JSWebAssemblyInstance* instance, EncodedJSValue arrayValue, uint32_t offset, uint64_t lane0, uint64_t lane1, uint32_t size))
{
    CallFrame* callFrame = DECLARE_WASM_CALL_FRAME(instance);
    assertCalleeIsReferenced(callFrame, instance);
    VM& vm = instance->vm();
    NativeCallFrameTracer tracer(vm, callFrame);
    return toUCPUStrictInt32(arrayFill(arrayValue, offset, v128_t { lane0, lane1 }, size));
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

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmIsSubRTT, bool, (RTT * maybeSubRTT, RTT* targetRTT))
{
    ASSERT(maybeSubRTT && targetRTT);
    return maybeSubRTT->isSubRTT(*targetRTT);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmAnyConvertExtern, EncodedJSValue, (EncodedJSValue reference))
{
    return externInternalize(reference);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRefTest, int32_t, (JSWebAssemblyInstance* instance, EncodedJSValue reference, uint32_t allowNull, int32_t heapType, bool shouldNegate))
{
    Wasm::TypeIndex typeIndex;
    if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(heapType)))
        typeIndex = static_cast<Wasm::TypeIndex>(heapType);
    else
        typeIndex = instance->module().moduleInformation().typeSignatures[heapType]->index();
    int32_t truth = shouldNegate ? 0 : 1;
    int32_t falsity = shouldNegate ? 1 : 0;
    // Explicitly return 1 or 0 because bool in C++ only reqiures that the bottom bit match the other bits can be anything.
    return Wasm::refCast(reference, static_cast<bool>(allowNull), typeIndex) ? truth : falsity;
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWasmRefCast, EncodedJSValue, (JSWebAssemblyInstance* instance, EncodedJSValue reference, uint32_t allowNull, int32_t heapType))
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

}
} // namespace JSC::Wasm

IGNORE_WARNINGS_END

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
