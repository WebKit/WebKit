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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "IndexingType.h"
#include "JITOperationValidation.h"
#include "JSCJSValue.h"
#include "OperationResult.h"
#include "WasmExceptionType.h"
#include "WasmOSREntryData.h"
#include "WasmTypeDefinition.h"

namespace JSC {

class JSArray;
class JSWebAssemblyInstance;

namespace Probe {
class Context;
} // namespace JSC::Probe
namespace Wasm {

class TypeDefinition;

typedef int64_t EncodedWasmValue;

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationJSToWasmEntryWrapperBuildFrame, void, (void*, CallFrame*));
JSC_DECLARE_JIT_OPERATION(operationJSToWasmEntryWrapperBuildReturnFrame, EncodedJSValue, (void*, CallFrame*));
JSC_DECLARE_JIT_OPERATION(operationGetWasmCalleeStackSize, EncodedJSValue, (JSWebAssemblyInstance*, Wasm::Callee*));
JSC_DECLARE_JIT_OPERATION(operationWasmToJSExitMarshalArguments, EncodedJSValue, (void*, CallFrame*, void*, JSWebAssemblyInstance*));
JSC_DECLARE_JIT_OPERATION(operationWasmToJSExitMarshalReturnValues, EncodedJSValue, (void* sp, CallFrame* cfr, JSWebAssemblyInstance*));

#if ENABLE(WEBASSEMBLY_OMGJIT)
void loadValuesIntoBuffer(Probe::Context&, const StackMap&, uint64_t* buffer, SavedFPWidth);
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmTriggerTierUpNow, void, (JSWebAssemblyInstance*, uint32_t functionIndex));
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmTriggerOSREntryNow, void, (Probe::Context&));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmLoopOSREnterBBQJIT, void, (Probe::Context&));
#endif
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmUnwind, void*, (JSWebAssemblyInstance*));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationConvertToI64, int64_t, (JSWebAssemblyInstance*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationConvertToF64, double, (JSWebAssemblyInstance*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationConvertToI32, int32_t, (JSWebAssemblyInstance*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationConvertToF32, float, (JSWebAssemblyInstance*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationConvertToFuncref, EncodedJSValue, (JSWebAssemblyInstance*, const TypeDefinition*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationConvertToAnyref, EncodedJSValue, (JSWebAssemblyInstance*, const TypeDefinition*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationConvertToBigInt, EncodedJSValue, (JSWebAssemblyInstance*, EncodedWasmValue));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationIterateResults, void, (JSWebAssemblyInstance*, const TypeDefinition*, EncodedJSValue, uint64_t*, uint64_t*));
JSC_DECLARE_JIT_OPERATION(operationAllocateResultsArray, JSArray*, (JSWebAssemblyInstance*, const TypeDefinition*, IndexingType, JSValue*));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmWriteBarrierSlowPath, void, (JSCell*, VM*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationPopcount32, uint32_t, (int32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationPopcount64, uint64_t, (int64_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationGrowMemory, int32_t, (JSWebAssemblyInstance*, int32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmMemoryFill, UCPUStrictInt32, (JSWebAssemblyInstance*, uint32_t dstAddress, uint32_t targetValue, uint32_t count));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmMemoryCopy, UCPUStrictInt32, (JSWebAssemblyInstance*, uint32_t dstAddress, uint32_t srcAddress, uint32_t count));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationGetWasmTableElement, EncodedJSValue, (JSWebAssemblyInstance*, unsigned, int32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationSetWasmTableElement, UCPUStrictInt32, (JSWebAssemblyInstance*, unsigned, uint32_t, EncodedJSValue encValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmRefFunc, EncodedJSValue, (JSWebAssemblyInstance*, uint32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmTableInit, UCPUStrictInt32, (JSWebAssemblyInstance*, unsigned elementIndex, unsigned tableIndex, uint32_t dstOffset, uint32_t srcOffset, uint32_t length));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmElemDrop, void, (JSWebAssemblyInstance*, unsigned elementIndex));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmTableGrow, int32_t, (JSWebAssemblyInstance*, unsigned, EncodedJSValue fill, uint32_t delta));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmTableFill, UCPUStrictInt32, (JSWebAssemblyInstance*, unsigned, uint32_t offset, EncodedJSValue fill, uint32_t count));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmTableCopy, UCPUStrictInt32, (JSWebAssemblyInstance*, unsigned dstTableIndex, unsigned srcTableIndex, int32_t dstOffset, int32_t srcOffset, int32_t length));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationGetWasmTableSize, int32_t, (JSWebAssemblyInstance*, unsigned));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicWait32, int32_t, (JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int32_t value, int64_t timeout));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicWait64, int32_t, (JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int64_t value, int64_t timeout));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationMemoryAtomicNotify, int32_t, (JSWebAssemblyInstance*, unsigned, unsigned, int32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmMemoryInit, UCPUStrictInt32, (JSWebAssemblyInstance*, unsigned dataSegmentIndex, uint32_t dstAddress, uint32_t srcAddress, uint32_t length));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmDataDrop, void, (JSWebAssemblyInstance*, unsigned dataSegmentIndex));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmStructNew, EncodedJSValue, (JSWebAssemblyInstance*, uint32_t, bool, uint64_t*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmStructNewEmpty, EncodedJSValue, (JSWebAssemblyInstance*, uint32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmStructGet, EncodedJSValue, (EncodedJSValue, uint32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmStructSet, void, (JSWebAssemblyInstance*, EncodedJSValue, uint32_t, EncodedJSValue));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmThrow, void*, (JSWebAssemblyInstance*, unsigned exceptionIndex, uint64_t*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmRethrow, void*, (JSWebAssemblyInstance*, EncodedJSValue thrownValue));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmToJSException, void*, (JSWebAssemblyInstance*, Wasm::ExceptionType));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationCrashDueToBBQStackOverflow, void, ());
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationCrashDueToOMGStackOverflow, void, ());

struct ThrownExceptionInfo {
    EncodedJSValue thrownValue;
    void* payload;
};
#if USE(JSVALUE64)
static_assert(sizeof(ThrownExceptionInfo) == sizeof(UCPURegister) * 2, "ThrownExceptionInfo should fit in two machine registers");
#endif

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNew, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size, uint64_t value));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewVector, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size, uint64_t lane0, uint64_t lane1));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewData, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t dataSegmentIndex, uint32_t arraySize, uint32_t offset));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewElem, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t elemSegmentIndex, uint32_t arraySize, uint32_t offset));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayNewEmpty, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t size));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayGet, EncodedJSValue, (JSWebAssemblyInstance* instance, uint32_t typeIndex, EncodedJSValue encValue, uint32_t index));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArraySet, void, (JSWebAssemblyInstance* instance, uint32_t typeIndex, EncodedJSValue encValue, uint32_t index, EncodedJSValue value));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayFill, UCPUStrictInt32, (JSWebAssemblyInstance*, EncodedJSValue, uint32_t, uint64_t, uint32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayFillVector, UCPUStrictInt32, (JSWebAssemblyInstance*, EncodedJSValue, uint32_t, uint64_t, uint64_t, uint32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayCopy, UCPUStrictInt32, (JSWebAssemblyInstance*, EncodedJSValue, uint32_t, EncodedJSValue, uint32_t, uint32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayInitElem, UCPUStrictInt32, (JSWebAssemblyInstance*, EncodedJSValue, uint32_t, uint32_t, uint32_t, uint32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmArrayInitData, UCPUStrictInt32, (JSWebAssemblyInstance*, EncodedJSValue, uint32_t, uint32_t, uint32_t, uint32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmIsSubRTT, bool, (Wasm::RTT*, Wasm::RTT*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmAnyConvertExtern, EncodedJSValue, (EncodedJSValue));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmRefTest, int32_t, (JSWebAssemblyInstance*, EncodedJSValue, uint32_t, int32_t, bool));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmRefCast, EncodedJSValue, (JSWebAssemblyInstance*, EncodedJSValue, uint32_t, int32_t));

#if USE(JSVALUE64)
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmRetrieveAndClearExceptionIfCatchable, ThrownExceptionInfo, (JSWebAssemblyInstance*));
#else
/* On 32-bit platforms, we can't return both an EncodedJSValue and the payload
 * together (not enough return registers are available in the ABI and we don't
 * handle stack arguments when calling C functions), so, instead, return the
 * payload and return the thrown value via an out pointer */
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationWasmRetrieveAndClearExceptionIfCatchable, void*, (JSWebAssemblyInstance*, EncodedJSValue*));
#endif // USE(JSVALUE64)

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
