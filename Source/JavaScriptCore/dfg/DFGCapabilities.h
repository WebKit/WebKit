/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef DFGCapabilities_h
#define DFGCapabilities_h

#include "Interpreter.h"
#include <wtf/Platform.h>

namespace JSC { namespace DFG {

#if ENABLE(DFG_JIT)
// Fast check functions; if they return true it is still necessary to
// check opcodes.
inline bool mightCompileEval(CodeBlock*) { return true; }
inline bool mightCompileProgram(CodeBlock*) { return true; }
inline bool mightCompileFunctionForCall(CodeBlock*) { return true; }
inline bool mightCompileFunctionForConstruct(CodeBlock*) { return true; }

// Opcode checking.
inline bool canCompileOpcode(OpcodeID opcodeID)
{
    switch (opcodeID) {
    case op_enter:
    case op_convert_this:
    case op_create_this:
    case op_get_callee:
    case op_bitand:
    case op_bitor:
    case op_bitxor:
    case op_rshift:
    case op_lshift:
    case op_urshift:
    case op_pre_inc:
    case op_post_inc:
    case op_pre_dec:
    case op_post_dec:
    case op_add:
    case op_sub:
    case op_mul:
    case op_mod:
    case op_div:
#if ENABLE(DEBUG_WITH_BREAKPOINT)
    case op_debug:
#endif
    case op_mov:
    case op_check_has_instance:
    case op_instanceof:
    case op_not:
    case op_less:
    case op_lesseq:
    case op_greater:
    case op_greatereq:
    case op_eq:
    case op_eq_null:
    case op_stricteq:
    case op_neq:
    case op_neq_null:
    case op_nstricteq:
    case op_get_by_val:
    case op_put_by_val:
    case op_method_check:
    case op_get_scoped_var:
    case op_put_scoped_var:
    case op_get_by_id:
    case op_put_by_id:
    case op_get_global_var:
    case op_put_global_var:
    case op_jmp:
    case op_loop:
    case op_jtrue:
    case op_jfalse:
    case op_loop_if_true:
    case op_loop_if_false:
    case op_jeq_null:
    case op_jneq_null:
    case op_jless:
    case op_jlesseq:
    case op_jgreater:
    case op_jgreatereq:
    case op_jnless:
    case op_jnlesseq:
    case op_jngreater:
    case op_jngreatereq:
    case op_loop_hint:
    case op_loop_if_less:
    case op_loop_if_lesseq:
    case op_loop_if_greater:
    case op_loop_if_greatereq:
    case op_ret:
    case op_end:
#if USE(JSVALUE64)
    case op_call:
    case op_construct:
#endif
    case op_call_put_result:
    case op_resolve:
    case op_resolve_base:
    case op_resolve_global:
    case op_strcat:
    case op_to_primitive:
    case op_throw:
    case op_throw_reference_error:
        return true;
    default:
        return false;
    }
}

bool canCompileOpcodes(CodeBlock*);
#else // ENABLE(DFG_JIT)
inline bool mightCompileEval(CodeBlock*) { return false; }
inline bool mightCompileProgram(CodeBlock*) { return false; }
inline bool mightCompileFunctionForCall(CodeBlock*) { return false; }
inline bool mightCompileFunctionForConstruct(CodeBlock*) { return false; }
inline bool canCompileOpcode(OpcodeID) { return false; }
inline bool canCompileOpcodes(CodeBlock*) { return false; }
#endif // ENABLE(DFG_JIT)

inline bool canCompileEval(CodeBlock* codeBlock)
{
    return mightCompileEval(codeBlock) && canCompileOpcodes(codeBlock);
}

inline bool canCompileProgram(CodeBlock* codeBlock)
{
    return mightCompileProgram(codeBlock) && canCompileOpcodes(codeBlock);
}

inline bool canCompileFunctionForCall(CodeBlock* codeBlock)
{
    return mightCompileFunctionForCall(codeBlock) && canCompileOpcodes(codeBlock);
}

inline bool canCompileFunctionForConstruct(CodeBlock* codeBlock)
{
    return mightCompileFunctionForConstruct(codeBlock) && canCompileOpcodes(codeBlock);
}

} } // namespace JSC::DFG

#endif // DFGCapabilities_h

