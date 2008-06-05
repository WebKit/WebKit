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

#ifndef Opcodes_h
#define Opcodes_h

#include <wtf/Assertions.h>

namespace KJS {

#define DUMP_OPCODE_STATS 0

    #define FOR_EACH_OPCODE_ID(macro) \
        macro(op_load) \
        macro(op_new_object) \
        macro(op_new_array) \
        macro(op_new_regexp) \
        macro(op_mov) \
        \
        macro(op_not) \
        macro(op_eq) \
        macro(op_neq) \
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
        macro(op_in) \
        \
        macro(op_resolve) \
        macro(op_resolve_skip) \
        macro(op_get_scoped_var) \
        macro(op_put_scoped_var) \
        macro(op_resolve_base) \
        macro(op_resolve_with_base) \
        macro(op_resolve_func) \
        macro(op_get_by_id) \
        macro(op_put_by_id) \
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
        macro(op_jmp_scopes) \
        \
        macro(op_new_func) \
        macro(op_new_func_exp) \
        macro(op_call) \
        macro(op_call_eval) \
        macro(op_ret) \
        \
        macro(op_construct) \
        \
        macro(op_get_pnames) \
        macro(op_next_pname) \
        \
        macro(op_push_scope) \
        macro(op_pop_scope) \
        \
        macro(op_catch) \
        macro(op_throw) \
        macro(op_new_error) \
        \
        macro(op_jsr) \
        macro(op_sret) \
        \
        macro(op_debug) \
        \
        macro(op_end) // end must be the last opcode in the list
        
    #define OPCODE_ID_ENUM(opcode) opcode,
        typedef enum { FOR_EACH_OPCODE_ID(OPCODE_ID_ENUM) } OpcodeID;
    #undef OPCODE_ID_ENUM

    const int numOpcodeIDs = op_end + 1;

    #define VERIFY_OPCODE_ID(id) COMPILE_ASSERT(id <= op_end, ASSERT_THAT_JS_OPCODE_IDS_ARE_VALID);
        FOR_EACH_OPCODE_ID(VERIFY_OPCODE_ID);
    #undef VERIFY_OPCODE_ID

#if HAVE(COMPUTED_GOTO)
    typedef void* Opcode;
#else
    typedef OpcodeID Opcode;
#endif

#if DUMP_OPCODE_STATS

    struct OpcodeStats {
        OpcodeStats();
        ~OpcodeStats();
        static unsigned opcodeCounts[numOpcodeIDs];
        static void recordInstruction(int opcode);
    };

#endif

} // namespace KJS

#endif // Opcodes_h
