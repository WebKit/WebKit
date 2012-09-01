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

#if ENABLE(LLINT_C_LOOP)

namespace JSC {

// The following is a minimal set of alias for the opcode names. This is needed
// because there is code (e.g. in GetByIdStatus.cpp and PutByIdStatus.cpp)
// which refers to the opcodes expecting them to be prefixed with "llint_".
// In the CLoop implementation, the 2 are equivalent. Hence, we set up this
// alias here.
//
// Note: we don't just do this for all opcodes because we only need a few,
// and currently, FOR_EACH_OPCODE_ID() includes the llint and JIT opcode
// extensions which we definitely don't want to add an alias for. With some
// minor refactoring, we can use FOR_EACH_OPCODE_ID() to automatically
// generate a llint_ alias for all opcodes, but that is not needed at this
// time.

const OpcodeID llint_op_call = op_call;
const OpcodeID llint_op_call_eval = op_call_eval;
const OpcodeID llint_op_call_varargs = op_call_varargs;
const OpcodeID llint_op_construct = op_construct;
const OpcodeID llint_op_catch = op_catch;
const OpcodeID llint_op_get_by_id = op_get_by_id;
const OpcodeID llint_op_get_by_id_out_of_line = op_get_by_id_out_of_line;
const OpcodeID llint_op_put_by_id = op_put_by_id;
const OpcodeID llint_op_put_by_id_out_of_line = op_put_by_id_out_of_line;

const OpcodeID llint_op_put_by_id_transition_direct =
    op_put_by_id_transition_direct;
const OpcodeID llint_op_put_by_id_transition_direct_out_of_line =
    op_put_by_id_transition_direct_out_of_line;
const OpcodeID llint_op_put_by_id_transition_normal =
    op_put_by_id_transition_normal;
const OpcodeID llint_op_put_by_id_transition_normal_out_of_line =
    op_put_by_id_transition_normal_out_of_line;

const OpcodeID llint_op_method_check = op_method_check;

} // namespace JSC

#else // !ENABLE(LLINT_C_LOOP)

#define LLINT_INSTRUCTION_DECL(opcode, length) extern "C" void llint_##opcode();
    FOR_EACH_OPCODE_ID(LLINT_INSTRUCTION_DECL);
#undef LLINT_INSTRUCTION_DECL

#define DECLARE_LLINT_NATIVE_HELPER(name, length) extern "C" void name();
    FOR_EACH_LLINT_NATIVE_HELPER(DECLARE_LLINT_NATIVE_HELPER)
#undef DECLARE_LLINT_NATIVE_HELPER

#endif // !ENABLE(LLINT_C_LOOP)

#endif // ENABLE(LLINT)

#endif // LowLevelInterpreter_h
