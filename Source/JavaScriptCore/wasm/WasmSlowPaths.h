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
struct Instruction;
struct ProtoCallFrame;

namespace Wasm {
class Instance;
}

namespace LLInt {

#define WASM_SLOW_PATH_DECL(name) \
    extern "C" SlowPathReturnType slow_path_wasm_##name(CallFrame* callFrame, const Instruction* pc, Wasm::Instance* instance)

#define WASM_SLOW_PATH_HIDDEN_DECL(name) \
    WASM_SLOW_PATH_DECL(name) WTF_INTERNAL

WASM_SLOW_PATH_HIDDEN_DECL(prologue_osr);
WASM_SLOW_PATH_HIDDEN_DECL(loop_osr);
WASM_SLOW_PATH_HIDDEN_DECL(epilogue_osr);

WASM_SLOW_PATH_HIDDEN_DECL(trace);
WASM_SLOW_PATH_HIDDEN_DECL(out_of_line_jump_target);

WASM_SLOW_PATH_HIDDEN_DECL(ref_func);
WASM_SLOW_PATH_HIDDEN_DECL(table_get);
WASM_SLOW_PATH_HIDDEN_DECL(table_set);
WASM_SLOW_PATH_HIDDEN_DECL(table_size);
WASM_SLOW_PATH_HIDDEN_DECL(table_fill);
WASM_SLOW_PATH_HIDDEN_DECL(table_grow);
WASM_SLOW_PATH_HIDDEN_DECL(grow_memory);
WASM_SLOW_PATH_HIDDEN_DECL(call);
WASM_SLOW_PATH_HIDDEN_DECL(call_no_tls);
WASM_SLOW_PATH_HIDDEN_DECL(call_indirect);
WASM_SLOW_PATH_HIDDEN_DECL(call_indirect_no_tls);
WASM_SLOW_PATH_HIDDEN_DECL(set_global_ref);
WASM_SLOW_PATH_HIDDEN_DECL(set_global_ref_portable_binding);

extern "C" SlowPathReturnType slow_path_wasm_throw_exception(CallFrame*, const Instruction*, Wasm::Instance* instance, Wasm::ExceptionType) WTF_INTERNAL;
extern "C" SlowPathReturnType slow_path_wasm_popcount(const Instruction* pc, uint32_t) WTF_INTERNAL;
extern "C" SlowPathReturnType slow_path_wasm_popcountll(const Instruction* pc, uint64_t) WTF_INTERNAL;

} } // namespace JSC::LLInt

#endif // ENABLE(WEBASSEMBLY)
