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
#include "JSCJSValue.h"
#include "SlowPathReturnType.h"
#include "WasmExceptionType.h"

namespace JSC {

class JSArray;
class JSWebAssemblyInstance;

namespace Probe {
class Context;
} // namespace JSC::Probe
namespace Wasm {

class Instance;
class Signature;

JSC_DECLARE_JIT_OPERATION(operationWasmTriggerOSREntryNow, void, (Probe::Context&));
JSC_DECLARE_JIT_OPERATION(operationWasmTriggerTierUpNow, void, (Instance*, uint32_t functionIndex));
JSC_DECLARE_JIT_OPERATION(operationWasmThrowBadI64, void, (JSWebAssemblyInstance*));
JSC_DECLARE_JIT_OPERATION(operationWasmUnwind, void, (CallFrame*));

JSC_DECLARE_JIT_OPERATION(operationConvertToF64, double, (CallFrame*, JSValue));
JSC_DECLARE_JIT_OPERATION(operationConvertToI32, int32_t, (CallFrame*, JSValue));
JSC_DECLARE_JIT_OPERATION(operationConvertToF32, float, (CallFrame*, JSValue));

JSC_DECLARE_JIT_OPERATION(operationIterateResults, void, (CallFrame*, Instance*, const Signature*, JSValue, uint64_t*, uint64_t*));
JSC_DECLARE_JIT_OPERATION(operationAllocateResultsArray, JSArray*, (CallFrame*, Wasm::Instance*, const Signature*, IndexingType, JSValue*));

JSC_DECLARE_JIT_OPERATION(operationWasmWriteBarrierSlowPath, void, (JSCell*, VM*));
JSC_DECLARE_JIT_OPERATION(operationPopcount32, uint32_t, (int32_t));
JSC_DECLARE_JIT_OPERATION(operationPopcount64, uint64_t, (int64_t));
JSC_DECLARE_JIT_OPERATION(operationGrowMemory, int32_t, (void*, Instance*, int32_t));

JSC_DECLARE_JIT_OPERATION(operationGetWasmTableElement, EncodedJSValue, (Instance*, unsigned, int32_t));
JSC_DECLARE_JIT_OPERATION(operationSetWasmTableElement, bool, (Instance*, unsigned, int32_t, EncodedJSValue encValue));
JSC_DECLARE_JIT_OPERATION(operationWasmRefFunc, EncodedJSValue, (Instance*, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationWasmTableGrow, int32_t, (Instance*, unsigned, EncodedJSValue fill, int32_t delta));
JSC_DECLARE_JIT_OPERATION(operationWasmTableFill, bool, (Instance*, unsigned, int32_t offset, EncodedJSValue fill, int32_t count));
JSC_DECLARE_JIT_OPERATION(operationGetWasmTableSize, int32_t, (Instance*, unsigned));

JSC_DECLARE_JIT_OPERATION(operationWasmToJSException, void*, (CallFrame*, Wasm::ExceptionType, Instance*));

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
