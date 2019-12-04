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

void JIT_OPERATION operationWasmTriggerOSREntryNow(Probe::Context&) WTF_INTERNAL;
void JIT_OPERATION operationWasmTriggerTierUpNow(Instance*, uint32_t functionIndex) WTF_INTERNAL;
void JIT_OPERATION operationWasmThrowBadI64(JSWebAssemblyInstance*) WTF_INTERNAL;
void JIT_OPERATION operationWasmUnwind(CallFrame*) WTF_INTERNAL;

double JIT_OPERATION operationConvertToF64(CallFrame*, JSValue) WTF_INTERNAL;
int32_t JIT_OPERATION operationConvertToI32(CallFrame*, JSValue) WTF_INTERNAL;
float JIT_OPERATION operationConvertToF32(CallFrame*, JSValue) WTF_INTERNAL;

void JIT_OPERATION operationIterateResults(CallFrame*, Instance*, const Signature*, JSValue, uint64_t*, uint64_t*) WTF_INTERNAL;
JSArray* JIT_OPERATION operationAllocateResultsArray(CallFrame*, Wasm::Instance*, const Signature*, IndexingType, JSValue*) WTF_INTERNAL;

void JIT_OPERATION operationWasmWriteBarrierSlowPath(JSCell*, VM*) WTF_INTERNAL;
uint32_t JIT_OPERATION operationPopcount32(int32_t) WTF_INTERNAL;
uint64_t JIT_OPERATION operationPopcount64(int64_t) WTF_INTERNAL;
int32_t JIT_OPERATION operationGrowMemory(void*, Instance*, int32_t) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationGetWasmTableElement(Instance*, unsigned, int32_t) WTF_INTERNAL;
bool JIT_OPERATION operationSetWasmTableElement(Instance*, unsigned, int32_t, EncodedJSValue encValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationWasmRefFunc(Instance*, uint32_t) WTF_INTERNAL;
int32_t JIT_OPERATION operationWasmTableGrow(Instance*, unsigned, EncodedJSValue fill, int32_t delta) WTF_INTERNAL;
bool JIT_OPERATION operationWasmTableFill(Instance*, unsigned, int32_t offset, EncodedJSValue fill, int32_t count) WTF_INTERNAL;
int32_t JIT_OPERATION operationGetWasmTableSize(Instance*, unsigned) WTF_INTERNAL;

void* JIT_OPERATION operationWasmToJSException(CallFrame*, Wasm::ExceptionType, Instance*) WTF_INTERNAL;

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
