/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BytecodeUseDef_h
#define BytecodeUseDef_h

#include "CodeBlock.h"

namespace JSC {

template<typename Functor>
void computeUsesForBytecodeOffset(
    CodeBlock* codeBlock, unsigned bytecodeOffset, Functor& functor)
{
    Interpreter* interpreter = codeBlock->vm()->interpreter;
    Instruction* instructionsBegin = codeBlock->instructions().begin();
    Instruction* instruction = &instructionsBegin[bytecodeOffset];
    OpcodeID opcodeID = interpreter->getOpcodeID(instruction->u.opcode);
    switch (opcodeID) {
    // No uses.
    case op_new_regexp:
    case op_new_array_buffer:
    case op_throw_static_error:
    case op_debug:
    case op_resolve_scope:
    case op_pop_scope:
    case op_jneq_ptr:
    case op_new_func_exp:
    case op_loop_hint:
    case op_jmp:
    case op_new_object:
    case op_init_lazy_reg:
    case op_get_callee:
    case op_enter:
    case op_catch:
    case op_touch_entry:
        return;
    case op_new_func:
    case op_new_captured_func:
    case op_create_activation: 
    case op_create_arguments:
    case op_to_this:
    case op_tear_off_activation:
    case op_profile_will_call:
    case op_profile_did_call:
    case op_throw:
    case op_push_with_scope:
    case op_end:
    case op_ret:
    case op_jtrue:
    case op_jfalse:
    case op_jeq_null:
    case op_jneq_null:
    case op_dec:
    case op_inc: {
        functor(codeBlock, instruction, opcodeID, instruction[1].u.operand);
        return;
    }
    case op_ret_object_or_this:
    case op_jlesseq:
    case op_jgreater:
    case op_jgreatereq:
    case op_jnless:
    case op_jnlesseq:
    case op_jngreater:
    case op_jngreatereq:
    case op_jless: {
        functor(codeBlock, instruction, opcodeID, instruction[1].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        return;
    }
    case op_put_by_val_direct:
    case op_put_by_val: {
        functor(codeBlock, instruction, opcodeID, instruction[1].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        return;
    }
    case op_put_by_index:
    case op_put_by_id_transition_direct:
    case op_put_by_id_transition_direct_out_of_line:
    case op_put_by_id_transition_normal:
    case op_put_by_id_transition_normal_out_of_line:
    case op_put_by_id_out_of_line:
    case op_put_by_id:
    case op_put_to_scope: {
        functor(codeBlock, instruction, opcodeID, instruction[1].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        return;
    }
    case op_put_getter_setter: {
        functor(codeBlock, instruction, opcodeID, instruction[1].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[4].u.operand);
        return;
    }
    case op_init_global_const_nop:
    case op_init_global_const:
    case op_push_name_scope:
    case op_get_from_scope:
    case op_to_primitive:
    case op_get_by_id:
    case op_get_by_id_out_of_line:
    case op_get_array_length:
    case op_get_arguments_length:
    case op_typeof:
    case op_is_undefined:
    case op_is_boolean:
    case op_is_number:
    case op_is_string:
    case op_is_object:
    case op_is_function:
    case op_to_number:
    case op_negate:
    case op_neq_null:
    case op_eq_null:
    case op_not:
    case op_mov:
    case op_captured_mov:
    case op_new_array_with_size:
    case op_create_this:
    case op_get_pnames:
    case op_del_by_id:
    case op_unsigned: {
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        return;
    }
    case op_get_by_val:
    case op_get_argument_by_val:
    case op_in:
    case op_instanceof:
    case op_check_has_instance:
    case op_add:
    case op_mul:
    case op_div:
    case op_mod:
    case op_sub:
    case op_lshift:
    case op_rshift:
    case op_urshift:
    case op_bitand:
    case op_bitxor:
    case op_bitor:
    case op_less:
    case op_lesseq:
    case op_greater:
    case op_greatereq:
    case op_nstricteq:
    case op_stricteq:
    case op_neq:
    case op_eq:
    case op_del_by_val: {
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        return;
    }
    case op_call_varargs: {
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[4].u.operand);
        return;
    }
    case op_next_pname: {
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[4].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[5].u.operand);
        return;
    }
    case op_get_by_pname: {
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[4].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[5].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[6].u.operand);
        return;
    }
    case op_switch_string:
    case op_switch_char:
    case op_switch_imm: {
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        return;
    }
    case op_new_array:
    case op_strcat: {
        int base = instruction[2].u.operand;
        int count = instruction[3].u.operand;
        for (int i = 0; i < count; i++)
            functor(codeBlock, instruction, opcodeID, base - i);
        return;
    }
    case op_construct:
    case op_call_eval:
    case op_call: {
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        int argCount = instruction[3].u.operand;
        int registerOffset = -instruction[4].u.operand;
        int lastArg = registerOffset + CallFrame::thisArgumentOffset();
        for (int i = opcodeID == op_construct ? 1 : 0; i < argCount; i++)
            functor(codeBlock, instruction, opcodeID, lastArg + i);
        return;
    }
    case op_tear_off_arguments: {
        functor(codeBlock, instruction, opcodeID, instruction[1].u.operand);
        functor(codeBlock, instruction, opcodeID, unmodifiedArgumentsRegister(VirtualRegister(instruction[1].u.operand)).offset());
        functor(codeBlock, instruction, opcodeID, instruction[2].u.operand);
        return;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

template<typename Functor>
void computeDefsForBytecodeOffset(CodeBlock* codeBlock, unsigned bytecodeOffset, Functor& functor)
{
    Interpreter* interpreter = codeBlock->vm()->interpreter;
    Instruction* instructionsBegin = codeBlock->instructions().begin();
    Instruction* instruction = &instructionsBegin[bytecodeOffset];
    OpcodeID opcodeID = interpreter->getOpcodeID(instruction->u.opcode);
    switch (opcodeID) {
    // These don't define anything.
    case op_init_global_const:
    case op_init_global_const_nop:
    case op_push_name_scope:
    case op_push_with_scope:
    case op_put_to_scope:
    case op_pop_scope:
    case op_end:
    case op_profile_will_call:
    case op_profile_did_call:
    case op_throw:
    case op_throw_static_error:
    case op_debug:
    case op_ret:
    case op_ret_object_or_this:
    case op_jmp:
    case op_jtrue:
    case op_jfalse:
    case op_jeq_null:
    case op_jneq_null:
    case op_jneq_ptr:
    case op_jless:
    case op_jlesseq:
    case op_jgreater:
    case op_jgreatereq:
    case op_jnless:
    case op_jnlesseq:
    case op_jngreater:
    case op_jngreatereq:
    case op_loop_hint:
    case op_switch_imm:
    case op_switch_char:
    case op_switch_string:
    case op_put_by_id:
    case op_put_by_id_out_of_line:
    case op_put_by_id_transition_direct:
    case op_put_by_id_transition_direct_out_of_line:
    case op_put_by_id_transition_normal:
    case op_put_by_id_transition_normal_out_of_line:
    case op_put_getter_setter:
    case op_put_by_val:
    case op_put_by_val_direct:
    case op_put_by_index:
    case op_tear_off_arguments:
    case op_touch_entry:
#define LLINT_HELPER_OPCODES(opcode, length) case opcode:
        FOR_EACH_LLINT_OPCODE_EXTENSION(LLINT_HELPER_OPCODES);
#undef LLINT_HELPER_OPCODES
        return;
    // These all have a single destination for the first argument.
    case op_next_pname:
    case op_resolve_scope:
    case op_strcat:
    case op_tear_off_activation:
    case op_to_primitive:
    case op_catch:
    case op_create_this:
    case op_new_array:
    case op_new_array_buffer:
    case op_new_array_with_size:
    case op_new_regexp:
    case op_new_func:
    case op_new_captured_func:
    case op_new_func_exp:
    case op_call_varargs:
    case op_get_from_scope:
    case op_call:
    case op_call_eval:
    case op_construct:
    case op_get_by_id:
    case op_get_by_id_out_of_line:
    case op_get_array_length:
    case op_check_has_instance:
    case op_instanceof:
    case op_get_by_val:
    case op_get_argument_by_val:
    case op_get_by_pname:
    case op_get_arguments_length:
    case op_typeof:
    case op_is_undefined:
    case op_is_boolean:
    case op_is_number:
    case op_is_string:
    case op_is_object:
    case op_is_function:
    case op_in:
    case op_to_number:
    case op_negate:
    case op_add:
    case op_mul:
    case op_div:
    case op_mod:
    case op_sub:
    case op_lshift:
    case op_rshift:
    case op_urshift:
    case op_bitand:
    case op_bitxor:
    case op_bitor:
    case op_inc:
    case op_dec:
    case op_eq:
    case op_neq:
    case op_stricteq:
    case op_nstricteq:
    case op_less:
    case op_lesseq:
    case op_greater:
    case op_greatereq:
    case op_neq_null:
    case op_eq_null:
    case op_not:
    case op_mov:
    case op_captured_mov:
    case op_new_object:
    case op_to_this:
    case op_get_callee:
    case op_init_lazy_reg:
    case op_create_activation:
    case op_create_arguments:
    case op_del_by_id:
    case op_del_by_val:
    case op_unsigned: {
        functor(codeBlock, instruction, opcodeID, instruction[1].u.operand);
        return;
    }
    case op_get_pnames: {
        functor(codeBlock, instruction, opcodeID, instruction[1].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[3].u.operand);
        functor(codeBlock, instruction, opcodeID, instruction[4].u.operand);
        return;
    }
    case op_enter: {
        for (unsigned i = codeBlock->m_numVars; i--;)
            functor(codeBlock, instruction, opcodeID, virtualRegisterForLocal(i).offset());
        return;
    } }
}

} // namespace JSC

#endif // BytecodeUseDef_h

