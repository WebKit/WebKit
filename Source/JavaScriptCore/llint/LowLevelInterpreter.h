/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef LowLevelInterpreter_h
#define LowLevelInterpreter_h

#include <wtf/Platform.h>

#if ENABLE(LLINT)

#include "Opcode.h"

#define LLINT_INSTRUCTION_DECL(opcode, length) extern "C" void llint_##opcode();
    FOR_EACH_OPCODE_ID(LLINT_INSTRUCTION_DECL);
#undef LLINT_INSTRUCTION_DECL

extern "C" void llint_begin();
extern "C" void llint_end();
extern "C" void llint_program_prologue();
extern "C" void llint_eval_prologue();
extern "C" void llint_function_for_call_prologue();
extern "C" void llint_function_for_construct_prologue();
extern "C" void llint_function_for_call_arity_check();
extern "C" void llint_function_for_construct_arity_check();
extern "C" void llint_generic_return_point();
extern "C" void llint_throw_from_slow_path_trampoline();
extern "C" void llint_throw_during_call_trampoline();

// Native call trampolines
extern "C" void llint_native_call_trampoline();
extern "C" void llint_native_construct_trampoline();

#endif // ENABLE(LLINT)

#endif // LowLevelInterpreter_h
