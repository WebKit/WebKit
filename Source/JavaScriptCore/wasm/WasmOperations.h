/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

class Instance;
class TypeDefinition;

typedef int64_t EncodedWasmValue;

#if ENABLE(WEBASSEMBLY_OMGJIT)
void loadValuesIntoBuffer(Probe::Context&, const StackMap&, uint64_t* buffer, SavedFPWidth);
JSC_DECLARE_JIT_OPERATION(operationWasmTriggerTierUpNow, void, (Instance*, uint32_t functionIndex));
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
JSC_DECLARE_JIT_OPERATION(operationWasmTriggerOSREntryNow, void, (Probe::Context&));
JSC_DECLARE_JIT_OPERATION(operationWasmLoopOSREnterBBQJIT, void, (Probe::Context&));
#endif
JSC_DECLARE_JIT_OPERATION(operationWasmUnwind, void*, (Instance*));

JSC_DECLARE_JIT_OPERATION(operationConvertToI64, int64_t, (Instance*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationConvertToF64, double, (Instance*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationConvertToI32, int32_t, (Instance*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationConvertToF32, float, (Instance*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationConvertToFuncref, EncodedJSValue, (Instance*, const TypeDefinition*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationConvertToAnyref, EncodedJSValue, (Instance*, const TypeDefinition*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationConvertToBigInt, EncodedJSValue, (Instance*, EncodedWasmValue));

JSC_DECLARE_JIT_OPERATION(operationIterateResults, void, (Instance*, const TypeDefinition*, EncodedJSValue, uint64_t*, uint64_t*));
JSC_DECLARE_JIT_OPERATION(operationAllocateResultsArray, JSArray*, (Wasm::Instance*, const TypeDefinition*, IndexingType, JSValue*));

JSC_DECLARE_JIT_OPERATION(operationWasmWriteBarrierSlowPath, void, (JSCell*, VM*));
JSC_DECLARE_JIT_OPERATION(operationPopcount32, uint32_t, (int32_t));
JSC_DECLARE_JIT_OPERATION(operationPopcount64, uint64_t, (int64_t));
JSC_DECLARE_JIT_OPERATION(operationGrowMemory, int32_t, (Instance*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmMemoryFill, UCPUStrictInt32, (Instance*, uint32_t dstAddress, uint32_t targetValue, uint32_t count));
JSC_DECLARE_JIT_OPERATION(operationWasmMemoryCopy, UCPUStrictInt32, (Instance*, uint32_t dstAddress, uint32_t srcAddress, uint32_t count));

JSC_DECLARE_JIT_OPERATION(operationGetWasmTableElement, EncodedJSValue, (Instance*, unsigned, int32_t));
JSC_DECLARE_JIT_OPERATION(operationSetWasmTableElement, UCPUStrictInt32, (Instance*, unsigned, uint32_t, EncodedJSValue encValue));
JSC_DECLARE_JIT_OPERATION(operationWasmRefFunc, EncodedJSValue, (Instance*, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmTableInit, UCPUStrictInt32, (Instance*, unsigned elementIndex, unsigned tableIndex, uint32_t dstOffset, uint32_t srcOffset, uint32_t length));
JSC_DECLARE_JIT_OPERATION(operationWasmElemDrop, void, (Instance*, unsigned elementIndex));
JSC_DECLARE_JIT_OPERATION(operationWasmTableGrow, int32_t, (Instance*, unsigned, EncodedJSValue fill, uint32_t delta));
JSC_DECLARE_JIT_OPERATION(operationWasmTableFill, UCPUStrictInt32, (Instance*, unsigned, uint32_t offset, EncodedJSValue fill, uint32_t count));
JSC_DECLARE_JIT_OPERATION(operationWasmTableCopy, UCPUStrictInt32, (Instance*, unsigned dstTableIndex, unsigned srcTableIndex, int32_t dstOffset, int32_t srcOffset, int32_t length));
JSC_DECLARE_JIT_OPERATION(operationGetWasmTableSize, int32_t, (Instance*, unsigned));

JSC_DECLARE_JIT_OPERATION(operationMemoryAtomicWait32, int32_t, (Instance* instance, unsigned base, unsigned offset, int32_t value, int64_t timeout));
JSC_DECLARE_JIT_OPERATION(operationMemoryAtomicWait64, int32_t, (Instance* instance, unsigned base, unsigned offset, int64_t value, int64_t timeout));
JSC_DECLARE_JIT_OPERATION(operationMemoryAtomicNotify, int32_t, (Instance*, unsigned, unsigned, int32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmMemoryInit, UCPUStrictInt32, (Instance*, unsigned dataSegmentIndex, uint32_t dstAddress, uint32_t srcAddress, uint32_t length));
JSC_DECLARE_JIT_OPERATION(operationWasmDataDrop, void, (Instance*, unsigned dataSegmentIndex));

JSC_DECLARE_JIT_OPERATION(operationWasmStructNew, EncodedJSValue, (Instance*, uint32_t, bool, uint64_t*));
JSC_DECLARE_JIT_OPERATION(operationWasmStructNewEmpty, EncodedJSValue, (Instance*, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmStructGet, EncodedJSValue, (EncodedJSValue, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmStructSet, void, (Instance*, EncodedJSValue, uint32_t, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationWasmThrow, void*, (Instance*, unsigned exceptionIndex, uint64_t*));
JSC_DECLARE_JIT_OPERATION(operationWasmRethrow, void*, (Instance*, EncodedJSValue thrownValue));

JSC_DECLARE_JIT_OPERATION(operationWasmToJSException, void*, (Instance*, Wasm::ExceptionType));
JSC_DECLARE_JIT_OPERATION(operationCrashDueToBBQStackOverflow, void, ());
JSC_DECLARE_JIT_OPERATION(operationCrashDueToOMGStackOverflow, void, ());

struct ThrownExceptionInfo {
    EncodedJSValue thrownValue;
    void* payload;
};

JSC_DECLARE_JIT_OPERATION(operationWasmArrayNew, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t size, uint64_t value));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayNewVector, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t size, uint64_t lane0, uint64_t lane1));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayNewData, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t dataSegmentIndex, uint32_t arraySize, uint32_t offset));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayNewElem, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t elemSegmentIndex, uint32_t arraySize, uint32_t offset));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayNewEmpty, EncodedJSValue, (Instance* instance, uint32_t typeIndex, uint32_t size));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayGet, EncodedJSValue, (Instance* instance, uint32_t typeIndex, EncodedJSValue encValue, uint32_t index));
JSC_DECLARE_JIT_OPERATION(operationWasmArraySet, void, (Instance* instance, uint32_t typeIndex, EncodedJSValue encValue, uint32_t index, EncodedJSValue value));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayFill, UCPUStrictInt32, (Instance*, EncodedJSValue, uint32_t, uint64_t, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayFillVector, UCPUStrictInt32, (Instance*, EncodedJSValue, uint32_t, uint64_t, uint64_t, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayCopy, UCPUStrictInt32, (Instance*, EncodedJSValue, uint32_t, EncodedJSValue, uint32_t, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayInitElem, UCPUStrictInt32, (Instance*, EncodedJSValue, uint32_t, uint32_t, uint32_t, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmArrayInitData, UCPUStrictInt32, (Instance*, EncodedJSValue, uint32_t, uint32_t, uint32_t, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmIsSubRTT, bool, (Wasm::RTT*, Wasm::RTT*));
JSC_DECLARE_JIT_OPERATION(operationWasmAnyConvertExtern, EncodedJSValue, (EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationWasmRefTest, int32_t, (Instance*, EncodedJSValue, uint32_t, int32_t, bool));
JSC_DECLARE_JIT_OPERATION(operationWasmRefCast, EncodedJSValue, (Instance*, EncodedJSValue, uint32_t, int32_t));

#if USE(JSVALUE64)
JSC_DECLARE_JIT_OPERATION(operationWasmRetrieveAndClearExceptionIfCatchable, ThrownExceptionInfo, (Instance*));
#else
/* On 32-bit platforms, we can't return both an EncodedJSValue and the payload
 * together (not enough return registers are available in the ABI and we don't
 * handle stack arguments when calling C functions), so, instead, return the
 * payload and return the thrown value via an out pointer */
JSC_DECLARE_JIT_OPERATION(operationWasmRetrieveAndClearExceptionIfCatchable, void*, (Instance*, EncodedJSValue*));
#endif // USE(JSVALUE64)

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
