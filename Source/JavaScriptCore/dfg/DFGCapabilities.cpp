/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DFGCapabilities.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGCommon.h"
#include "Options.h"

namespace JSC { namespace DFG {

bool isSupported()
{
    return Options::useDFGJIT() && MacroAssembler::supportsFloatingPoint();
}

bool isSupportedForInlining(CodeBlock* codeBlock)
{
    return codeBlock->ownerExecutable()->isInliningCandidate();
}

bool mightCompileEval(CodeBlock* codeBlock)
{
    return isSupported()
        && codeBlock->bytecodeCost() <= Options::maximumOptimizationCandidateBytecodeCost()
        && codeBlock->ownerExecutable()->isOkToOptimize();
}
bool mightCompileProgram(CodeBlock* codeBlock)
{
    return isSupported()
        && codeBlock->bytecodeCost() <= Options::maximumOptimizationCandidateBytecodeCost()
        && codeBlock->ownerExecutable()->isOkToOptimize();
}
bool mightCompileFunctionForCall(CodeBlock* codeBlock)
{
    return isSupported()
        && codeBlock->bytecodeCost() <= Options::maximumOptimizationCandidateBytecodeCost()
        && codeBlock->ownerExecutable()->isOkToOptimize();
}
bool mightCompileFunctionForConstruct(CodeBlock* codeBlock)
{
    return isSupported()
        && codeBlock->bytecodeCost() <= Options::maximumOptimizationCandidateBytecodeCost()
        && codeBlock->ownerExecutable()->isOkToOptimize();
}

bool mightInlineFunctionForCall(CodeBlock* codeBlock)
{
    return codeBlock->bytecodeCost() <= Options::maximumFunctionForCallInlineCandidateBytecodeCost()
        && isSupportedForInlining(codeBlock);
}
bool mightInlineFunctionForClosureCall(CodeBlock* codeBlock)
{
    return codeBlock->bytecodeCost() <= Options::maximumFunctionForClosureCallInlineCandidateBytecodeCost()
        && isSupportedForInlining(codeBlock);
}
bool mightInlineFunctionForConstruct(CodeBlock* codeBlock)
{
    return codeBlock->bytecodeCost() <= Options::maximumFunctionForConstructInlineCandidateBytecoodeCost()
        && isSupportedForInlining(codeBlock);
}
bool canUseOSRExitFuzzing(CodeBlock* codeBlock)
{
    return codeBlock->ownerExecutable()->canUseOSRExitFuzzing();
}

static bool verboseCapabilities()
{
    return verboseCompilationEnabled() || Options::verboseDFGFailure();
}

inline void debugFail(CodeBlock* codeBlock, OpcodeID opcodeID, CapabilityLevel result)
{
    if (verboseCapabilities() && !canCompile(result))
        dataLog("DFG rejecting opcode in ", *codeBlock, " because of opcode ", opcodeNames[opcodeID], "\n");
}

CapabilityLevel capabilityLevel(OpcodeID opcodeID, CodeBlock* codeBlock, const Instruction* pc)
{
    UNUSED_PARAM(codeBlock); // This function does some bytecode parsing. Ordinarily bytecode parsing requires the owning CodeBlock. It's sort of strange that we don't use it here right now.
    UNUSED_PARAM(pc);
    
    switch (opcodeID) {
    case op_wide16:
    case op_wide32:
        RELEASE_ASSERT_NOT_REACHED();
    case op_enter:
    case op_to_this:
    case op_argument_count:
    case op_check_tdz:
    case op_create_this:
    case op_create_promise:
    case op_create_generator:
    case op_create_async_generator:
    case op_bitnot:
    case op_bitand:
    case op_bitor:
    case op_bitxor:
    case op_rshift:
    case op_lshift:
    case op_urshift:
    case op_unsigned:
    case op_inc:
    case op_dec:
    case op_add:
    case op_sub:
    case op_negate:
    case op_mul:
    case op_mod:
    case op_pow:
    case op_div:
    case op_debug:
    case op_profile_type:
    case op_profile_control_flow:
    case op_mov:
    case op_overrides_has_instance:
    case op_identity_with_profile:
    case op_instanceof:
    case op_instanceof_custom:
    case op_is_empty:
    case op_typeof_is_undefined:
    case op_typeof_is_object:
    case op_typeof_is_function:
    case op_is_undefined_or_null:
    case op_is_boolean:
    case op_is_number:
    case op_is_big_int:
    case op_is_object:
    case op_is_cell_with_type:
    case op_is_callable:
    case op_is_constructor:
    case op_not:
    case op_less:
    case op_lesseq:
    case op_greater:
    case op_greatereq:
    case op_below:
    case op_beloweq:
    case op_eq:
    case op_eq_null:
    case op_stricteq:
    case op_neq:
    case op_neq_null:
    case op_nstricteq:
    case op_get_by_val:
    case op_put_by_val:
    case op_put_by_val_direct:
    case op_try_get_by_id:
    case op_get_by_id:
    case op_get_by_id_with_this:
    case op_get_by_id_direct:
    case op_get_by_val_with_this:
    case op_get_prototype_of:
    case op_put_by_id:
    case op_put_by_id_with_this:
    case op_put_by_val_with_this:
    case op_put_getter_by_id:
    case op_put_setter_by_id:
    case op_put_getter_setter_by_id:
    case op_put_getter_by_val:
    case op_put_setter_by_val:
    case op_define_data_property:
    case op_define_accessor_property:
    case op_del_by_id:
    case op_del_by_val:
    case op_jmp:
    case op_jtrue:
    case op_jfalse:
    case op_jeq_null:
    case op_jneq_null:
    case op_jundefined_or_null:
    case op_jnundefined_or_null:
    case op_jless:
    case op_jlesseq:
    case op_jgreater:
    case op_jgreatereq:
    case op_jnless:
    case op_jnlesseq:
    case op_jngreater:
    case op_jngreatereq:
    case op_jeq:
    case op_jneq:
    case op_jstricteq:
    case op_jnstricteq:
    case op_jbelow:
    case op_jbeloweq:
    case op_loop_hint:
    case op_check_traps:
    case op_nop:
    case op_ret:
    case op_end:
    case op_new_object:
    case op_new_promise:
    case op_new_generator:
    case op_new_array:
    case op_new_array_with_size:
    case op_new_array_buffer:
    case op_new_array_with_spread:
    case op_spread:
    case op_strcat:
    case op_to_primitive:
    case op_throw:
    case op_throw_static_error:
    case op_call:
    case op_tail_call:
    case op_construct:
    case op_call_varargs:
    case op_tail_call_varargs:
    case op_tail_call_forward_arguments:
    case op_construct_varargs:
    case op_create_direct_arguments:
    case op_create_scoped_arguments:
    case op_create_cloned_arguments:
    case op_create_arguments_butterfly:
    case op_get_from_arguments:
    case op_put_to_arguments:
    case op_get_argument:
    case op_jneq_ptr:
    case op_typeof:
    case op_to_number:
    case op_to_numeric:
    case op_to_string:
    case op_to_object:
    case op_switch_imm:
    case op_switch_char:
    case op_in_by_val:
    case op_in_by_id:
    case op_get_scope:
    case op_get_from_scope:
    case op_get_enumerable_length:
    case op_has_generic_property:
    case op_has_structure_property:
    case op_has_own_structure_property:
    case op_in_structure_property:
    case op_has_indexed_property:
    case op_get_direct_pname:
    case op_get_property_enumerator:
    case op_enumerator_structure_pname:
    case op_enumerator_generic_pname:
    case op_to_index_string:
    case op_new_func:
    case op_new_func_exp:
    case op_new_generator_func:
    case op_new_generator_func_exp:
    case op_new_async_generator_func:
    case op_new_async_generator_func_exp:
    case op_new_async_func:
    case op_new_async_func_exp:
    case op_set_function_name:
    case op_create_lexical_environment:
    case op_push_with_scope:
    case op_get_parent_scope:
    case op_catch:
    case op_create_rest:
    case op_get_rest_length:
    case op_iterator_open:
    case op_iterator_next:
    case op_log_shadow_chicken_prologue:
    case op_log_shadow_chicken_tail:
    case op_put_to_scope:
    case op_resolve_scope:
    case op_resolve_scope_for_hoisting_func_decl_in_eval:
    case op_new_regexp:
    case op_get_internal_field:
    case op_put_internal_field:
    case op_to_property_key:
    case op_unreachable:
    case op_super_sampler_begin:
    case op_super_sampler_end:
    case op_get_private_name:
    case op_put_private_name:
        return CanCompileAndInline;

    case op_switch_string: // Don't inline because we don't want to copy string tables in the concurrent JIT.
    case op_call_eval:
        return CanCompile;

    case op_yield:
    case op_create_generator_frame_environment:
    case llint_program_prologue:
    case llint_eval_prologue:
    case llint_module_program_prologue:
    case llint_function_for_call_prologue:
    case llint_function_for_construct_prologue:
    case llint_function_for_call_arity_check:
    case llint_function_for_construct_arity_check:
    case llint_generic_return_point:
    case llint_throw_from_slow_path_trampoline:
    case llint_throw_during_call_trampoline:
    case llint_native_call_trampoline:
    case llint_native_construct_trampoline:
    case llint_internal_function_call_trampoline:
    case llint_internal_function_construct_trampoline:
    case llint_get_host_call_return_value:
    case checkpoint_osr_exit_from_inlined_call_trampoline:
    case checkpoint_osr_exit_trampoline:
    case handleUncaughtException:
    case fuzzer_return_early_from_loop_hint:
    case op_iterator_open_return_location:
    case op_iterator_next_return_location:
    case op_call_return_location:
    case op_construct_return_location:
    case op_call_varargs_slow_return_location:
    case op_construct_varargs_slow_return_location:
    case op_get_by_id_return_location:
    case op_get_by_val_return_location:
    case op_put_by_id_return_location:
    case op_put_by_val_return_location:
    case wasm_function_prologue:
    case wasm_function_prologue_no_tls:
        return CannotCompile;
    }
    return CannotCompile;
}

CapabilityLevel capabilityLevel(CodeBlock* codeBlock)
{
    CapabilityLevel result = CanCompileAndInline;
    
    for (const auto& instruction : codeBlock->instructions()) {
        switch (instruction->opcodeID()) {
#define DEFINE_OP(opcode, length) \
        case opcode: { \
            CapabilityLevel newResult = leastUpperBound(result, capabilityLevel(opcode, codeBlock, instruction.ptr())); \
            if (newResult != result) { \
                debugFail(codeBlock, opcode, newResult); \
                result = newResult; \
            } \
            break; \
        }
            FOR_EACH_OPCODE_ID(DEFINE_OP)
#undef DEFINE_OP
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    return result;
}

} } // namespace JSC::DFG

#endif
