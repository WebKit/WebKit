/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Bytecodes_h
#define Bytecodes_h

#include <algorithm>
#include <string.h>

#include <wtf/Assertions.h>

namespace JSC {

    #define FOR_EACH_BYTECODE_ID(macro) \
        macro(op_enter) \
        macro(op_enter_with_activation) \
        macro(op_create_arguments) \
        macro(op_convert_this) \
        \
        macro(op_unexpected_load) \
        macro(op_new_object) \
        macro(op_new_array) \
        macro(op_new_regexp) \
        macro(op_mov) \
        \
        macro(op_not) \
        macro(op_eq) \
        macro(op_eq_null) \
        macro(op_neq) \
        macro(op_neq_null) \
        macro(op_stricteq) \
        macro(op_nstricteq) \
        macro(op_less) \
        macro(op_lesseq) \
        \
        macro(op_pre_inc) \
        macro(op_pre_dec) \
        macro(op_post_inc) \
        macro(op_post_dec) \
        macro(op_to_jsnumber) \
        macro(op_negate) \
        macro(op_add) \
        macro(op_mul) \
        macro(op_div) \
        macro(op_mod) \
        macro(op_sub) \
        \
        macro(op_lshift) \
        macro(op_rshift) \
        macro(op_urshift) \
        macro(op_bitand) \
        macro(op_bitxor) \
        macro(op_bitor) \
        macro(op_bitnot) \
        \
        macro(op_instanceof) \
        macro(op_typeof) \
        macro(op_is_undefined) \
        macro(op_is_boolean) \
        macro(op_is_number) \
        macro(op_is_string) \
        macro(op_is_object) \
        macro(op_is_function) \
        macro(op_in) \
        \
        macro(op_resolve) \
        macro(op_resolve_skip) \
        macro(op_resolve_global) \
        macro(op_get_scoped_var) \
        macro(op_put_scoped_var) \
        macro(op_get_global_var) \
        macro(op_put_global_var) \
        macro(op_resolve_base) \
        macro(op_resolve_with_base) \
        macro(op_resolve_func) \
        macro(op_get_by_id) \
        macro(op_get_by_id_self) \
        macro(op_get_by_id_proto) \
        macro(op_get_by_id_chain) \
        macro(op_get_by_id_generic) \
        macro(op_get_array_length) \
        macro(op_get_string_length) \
        macro(op_put_by_id) \
        macro(op_put_by_id_transition) \
        macro(op_put_by_id_replace) \
        macro(op_put_by_id_generic) \
        macro(op_del_by_id) \
        macro(op_get_by_val) \
        macro(op_put_by_val) \
        macro(op_del_by_val) \
        macro(op_put_by_index) \
        macro(op_put_getter) \
        macro(op_put_setter) \
        \
        macro(op_jmp) \
        macro(op_jtrue) \
        macro(op_jfalse) \
        macro(op_jeq_null) \
        macro(op_jneq_null) \
        macro(op_jnless) \
        macro(op_jmp_scopes) \
        macro(op_loop) \
        macro(op_loop_if_true) \
        macro(op_loop_if_less) \
        macro(op_loop_if_lesseq) \
        macro(op_switch_imm) \
        macro(op_switch_char) \
        macro(op_switch_string) \
        \
        macro(op_new_func) \
        macro(op_new_func_exp) \
        macro(op_call) \
        macro(op_call_eval) \
        macro(op_tear_off_activation) \
        macro(op_tear_off_arguments) \
        macro(op_ret) \
        \
        macro(op_construct) \
        macro(op_construct_verify) \
        \
        macro(op_get_pnames) \
        macro(op_next_pname) \
        \
        macro(op_push_scope) \
        macro(op_pop_scope) \
        macro(op_push_new_scope) \
        \
        macro(op_catch) \
        macro(op_throw) \
        macro(op_new_error) \
        \
        macro(op_jsr) \
        macro(op_sret) \
        \
        macro(op_debug) \
        macro(op_profile_will_call) \
        macro(op_profile_did_call) \
        \
        macro(op_end) // end must be the last bytecode in the list

    #define BYTECODE_ID_ENUM(bytecode) bytecode,
        typedef enum { FOR_EACH_BYTECODE_ID(BYTECODE_ID_ENUM) } BytecodeID;
    #undef BYTECODE_ID_ENUM

    const int numBytecodeIDs = op_end + 1;

    #define VERIFY_BYTECODE_ID(id) COMPILE_ASSERT(id <= op_end, ASSERT_THAT_JS_BYTECODE_IDS_ARE_VALID);
        FOR_EACH_BYTECODE_ID(VERIFY_BYTECODE_ID);
    #undef VERIFY_BYTECODE_ID

#if HAVE(COMPUTED_GOTO)
    typedef void* Bytecode;
#else
    typedef BytecodeID Bytecode;
#endif

#if ENABLE(BYTECODE_SAMPLING) || ENABLE(CODEBLOCK_SAMPLING) || ENABLE(BYTECODE_STATS)

#define PADDING_STRING "                                "
#define PADDING_STRING_LENGTH static_cast<unsigned>(strlen(PADDING_STRING))

    extern const char* const bytecodeNames[];

    inline const char* padBytecodeName(BytecodeID op, unsigned width)
    {
        unsigned pad = width - strlen(bytecodeNames[op]);
        pad = std::min(pad, PADDING_STRING_LENGTH);
        return PADDING_STRING + PADDING_STRING_LENGTH - pad;
    }

#undef PADDING_STRING_LENGTH
#undef PADDING_STRING

#endif

#if ENABLE(BYTECODE_STATS)

    struct BytecodeStats {
        BytecodeStats();
        ~BytecodeStats();
        static long long bytecodeCounts[numBytecodeIDs];
        static long long bytecodePairCounts[numBytecodeIDs][numBytecodeIDs];
        static int lastBytecode;

        static void recordInstruction(int bytecode);
        static void resetLastInstruction();
    };

#endif

} // namespace JSC

#endif // Bytecodes_h
