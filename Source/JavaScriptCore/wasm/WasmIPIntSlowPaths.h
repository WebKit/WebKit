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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "CommonSlowPaths.h"
#include "WasmExceptionType.h"
#include "WasmIPIntGenerator.h"
#include "WasmTypeDefinition.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

class JSWebAssemblyInstance;

namespace IPInt {

#define WASM_IPINT_EXTERN_CPP_DECL(name, ...) \
    extern "C" UGPRPair ipint_extern_##name(JSWebAssemblyInstance* instance, __VA_ARGS__)

#define WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(name, ...) \
    WASM_IPINT_EXTERN_CPP_DECL(name, __VA_ARGS__) REFERENCED_FROM_ASM WTF_INTERNAL

#define WASM_IPINT_EXTERN_CPP_DECL_1P(name) \
    extern "C" UGPRPair ipint_extern_##name(JSWebAssemblyInstance* instance)

#define WASM_IPINT_EXTERN_CPP_HIDDEN_DECL_1P(name) \
    WASM_IPINT_EXTERN_CPP_DECL_1P(name) REFERENCED_FROM_ASM WTF_INTERNAL

#if ENABLE(WEBASSEMBLY_BBQJIT)
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(prologue_osr, CallFrame* callFrame);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(loop_osr, CallFrame* callFrame, uint8_t* pc, IPIntLocal* pl);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(epilogue_osr, CallFrame* callFrame);
#endif

WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(retrieve_and_clear_exception, CallFrame*, IPIntStackEntry* stack, IPIntLocal* pl);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(throw_exception, CallFrame*, IPIntStackEntry* arguments, unsigned exceptionIndex);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(rethrow_exception, CallFrame*, uint64_t* pl, unsigned tryDepth);

WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(ref_func, unsigned index);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_get, unsigned, unsigned);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_set, unsigned tableIndex, unsigned index, EncodedJSValue value);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_init, IPIntStackEntry* sp, TableInitMetadata* metadata);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_fill, IPIntStackEntry* sp, TableFillMetadata* metadata);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_grow, IPIntStackEntry* sp, TableGrowMetadata* metadata);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL_1P(current_memory);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_grow, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_init, int32_t, IPIntStackEntry*);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(data_drop, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_copy, int32_t, int32_t, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_fill, int32_t, int32_t, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(elem_drop, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_copy, IPIntStackEntry* sp, TableCopyMetadata* metadata);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_size, int32_t);

WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(call_indirect, CallFrame* callFrame, Wasm::FunctionSpaceIndex* functionIndex, CallIndirectMetadata* call);
// We can't use FunctionSpaceIndex here since ARMv7 ABI always passes structs on th stack...
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(call, unsigned functionSpaceIndex, EncodedJSValue* callee);

WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(set_global_ref, uint32_t globalIndex, JSValue value);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(get_global_64, unsigned);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(set_global_64, unsigned, uint64_t);

WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_atomic_wait32, uint64_t, uint32_t, uint64_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_atomic_wait64, uint64_t, uint64_t, uint64_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_atomic_notify, unsigned, unsigned, int32_t);


} } // namespace JSC::IPInt

#endif
