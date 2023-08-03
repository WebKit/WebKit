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

#include "CommonSlowPaths.h"
#include "WasmExceptionType.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

class CallFrame;
struct ProtoCallFrame;

template<typename> struct BaseInstruction;
struct WasmOpcodeTraits;
using WasmInstruction = BaseInstruction<WasmOpcodeTraits>;

namespace Wasm {
class Instance;
}

namespace LLInt {

#define WASM_SLOW_PATH_DECL(name) \
    extern "C" UGPRPair slow_path_wasm_##name(CallFrame* callFrame, const WasmInstruction* pc, Wasm::Instance* instance)

#define WASM_SLOW_PATH_HIDDEN_DECL(name) \
    WASM_SLOW_PATH_DECL(name) REFERENCED_FROM_ASM WTF_INTERNAL

#define WASM_IPINT_EXTERN_CPP_DECL(name, ...) \
    extern "C" UGPRPair ipint_extern_##name(Wasm::Instance* instance, __VA_ARGS__)

#define WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(name, ...) \
    WASM_IPINT_EXTERN_CPP_DECL(name, __VA_ARGS__) REFERENCED_FROM_ASM WTF_INTERNAL

#define WASM_IPINT_EXTERN_CPP_DECL_1P(name) \
    extern "C" UGPRPair ipint_extern_##name(Wasm::Instance* instance)

#define WASM_IPINT_EXTERN_CPP_HIDDEN_DECL_1P(name) \
    WASM_IPINT_EXTERN_CPP_DECL_1P(name) REFERENCED_FROM_ASM WTF_INTERNAL

#if ENABLE(WEBASSEMBLY_B3JIT)
WASM_SLOW_PATH_HIDDEN_DECL(prologue_osr);
WASM_SLOW_PATH_HIDDEN_DECL(loop_osr);
WASM_SLOW_PATH_HIDDEN_DECL(epilogue_osr);
WASM_SLOW_PATH_HIDDEN_DECL(simd_go_straight_to_bbq_osr);
#endif

WASM_SLOW_PATH_HIDDEN_DECL(trace);
WASM_SLOW_PATH_HIDDEN_DECL(out_of_line_jump_target);

WASM_SLOW_PATH_HIDDEN_DECL(ref_func);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(ref_func, unsigned index);
WASM_SLOW_PATH_HIDDEN_DECL(table_get);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_get, unsigned, unsigned);
WASM_SLOW_PATH_HIDDEN_DECL(table_set);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_set, unsigned tableIndex, unsigned index, EncodedJSValue value);
WASM_SLOW_PATH_HIDDEN_DECL(table_init);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_init, uint32_t* metadata, uint32_t dest, uint64_t srcAndLength);
WASM_SLOW_PATH_HIDDEN_DECL(table_fill);
WASM_IPINT_EXTERN_CPP_DECL(table_fill, uint32_t tableIndex, EncodedJSValue fill, int64_t offsetAndSize);
WASM_SLOW_PATH_HIDDEN_DECL(table_grow);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_grow, int32_t tableIndex, EncodedJSValue fill, uint32_t size);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL_1P(current_memory);
WASM_SLOW_PATH_HIDDEN_DECL(grow_memory);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_grow, int32_t);
WASM_SLOW_PATH_HIDDEN_DECL(memory_init);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_init, int32_t, int32_t, int64_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(data_drop, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_copy, int32_t, int32_t, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(memory_fill, int32_t, int32_t, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(elem_drop, int32_t);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(table_copy, int32_t*, int32_t, int64_t);
WASM_IPINT_EXTERN_CPP_DECL(table_size, int32_t);
WASM_SLOW_PATH_HIDDEN_DECL(call);
WASM_SLOW_PATH_HIDDEN_DECL(call_indirect);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(call_indirect, CallFrame* callFrame, unsigned functionIndex, unsigned* metadataEntry);

WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(call, unsigned);

WASM_SLOW_PATH_HIDDEN_DECL(call_ref);
WASM_SLOW_PATH_HIDDEN_DECL(tail_call);
WASM_SLOW_PATH_HIDDEN_DECL(tail_call_indirect);
WASM_SLOW_PATH_HIDDEN_DECL(call_builtin);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(set_global_ref, uint32_t globalIndex, JSValue value);
WASM_SLOW_PATH_HIDDEN_DECL(set_global_ref);

WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(get_global_64, unsigned);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(set_global_64, unsigned, uint64_t);

WASM_SLOW_PATH_HIDDEN_DECL(set_global_ref_portable_binding);
WASM_SLOW_PATH_HIDDEN_DECL(memory_atomic_wait32);
WASM_SLOW_PATH_HIDDEN_DECL(memory_atomic_wait64);
WASM_SLOW_PATH_HIDDEN_DECL(memory_atomic_notify);
WASM_SLOW_PATH_HIDDEN_DECL(throw);
WASM_IPINT_EXTERN_CPP_HIDDEN_DECL(throw, CallFrame*, uint32_t);
WASM_SLOW_PATH_HIDDEN_DECL(rethrow);
WASM_SLOW_PATH_HIDDEN_DECL(retrieve_and_clear_exception);
WASM_SLOW_PATH_HIDDEN_DECL(array_new);
WASM_SLOW_PATH_HIDDEN_DECL(array_get);
WASM_SLOW_PATH_HIDDEN_DECL(array_set);
WASM_SLOW_PATH_HIDDEN_DECL(struct_new);
WASM_SLOW_PATH_HIDDEN_DECL(struct_get);
WASM_SLOW_PATH_HIDDEN_DECL(struct_set);

extern "C" NO_RETURN void wasm_log_crash(CallFrame*, Wasm::Instance* instance) REFERENCED_FROM_ASM WTF_INTERNAL;
extern "C" UGPRPair slow_path_wasm_throw_exception(CallFrame*, const WasmInstruction*, Wasm::Instance* instance, Wasm::ExceptionType) REFERENCED_FROM_ASM WTF_INTERNAL;
extern "C" UGPRPair slow_path_wasm_popcount(const WasmInstruction* pc, uint32_t) REFERENCED_FROM_ASM WTF_INTERNAL;
extern "C" UGPRPair slow_path_wasm_popcountll(const WasmInstruction* pc, uint64_t) REFERENCED_FROM_ASM WTF_INTERNAL;

#if USE(JSVALUE32_64)
WASM_SLOW_PATH_HIDDEN_DECL(f32_ceil);
WASM_SLOW_PATH_HIDDEN_DECL(f32_floor);
WASM_SLOW_PATH_HIDDEN_DECL(f32_trunc);
WASM_SLOW_PATH_HIDDEN_DECL(f32_nearest);
WASM_SLOW_PATH_HIDDEN_DECL(f64_ceil);
WASM_SLOW_PATH_HIDDEN_DECL(f64_floor);
WASM_SLOW_PATH_HIDDEN_DECL(f64_trunc);
WASM_SLOW_PATH_HIDDEN_DECL(f64_nearest);
WASM_SLOW_PATH_HIDDEN_DECL(f32_convert_u_i64);
WASM_SLOW_PATH_HIDDEN_DECL(f32_convert_s_i64);
WASM_SLOW_PATH_HIDDEN_DECL(f64_convert_u_i64);
WASM_SLOW_PATH_HIDDEN_DECL(f64_convert_s_i64);
WASM_SLOW_PATH_HIDDEN_DECL(i64_trunc_u_f32);
WASM_SLOW_PATH_HIDDEN_DECL(i64_trunc_s_f32);
WASM_SLOW_PATH_HIDDEN_DECL(i64_trunc_u_f64);
WASM_SLOW_PATH_HIDDEN_DECL(i64_trunc_s_f64);
WASM_SLOW_PATH_HIDDEN_DECL(i64_trunc_sat_f32_u);
WASM_SLOW_PATH_HIDDEN_DECL(i64_trunc_sat_f32_s);
WASM_SLOW_PATH_HIDDEN_DECL(i64_trunc_sat_f64_u);
WASM_SLOW_PATH_HIDDEN_DECL(i64_trunc_sat_f64_s);
#endif

} } // namespace JSC::LLInt

#endif // ENABLE(WEBASSEMBLY)
