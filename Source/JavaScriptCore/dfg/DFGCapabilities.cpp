/*
 * Copyright (C) 2011, 2013, 2014 Apple Inc. All rights reserved.
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
#include "Interpreter.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

bool mightCompileEval(CodeBlock* codeBlock)
{
    return codeBlock->instructionCount() <= Options::maximumOptimizationCandidateInstructionCount();
}
bool mightCompileProgram(CodeBlock* codeBlock)
{
    return codeBlock->instructionCount() <= Options::maximumOptimizationCandidateInstructionCount();
}
bool mightCompileFunctionForCall(CodeBlock* codeBlock)
{
    return codeBlock->instructionCount() <= Options::maximumOptimizationCandidateInstructionCount();
}
bool mightCompileFunctionForConstruct(CodeBlock* codeBlock)
{
    return codeBlock->instructionCount() <= Options::maximumOptimizationCandidateInstructionCount();
}

bool mightInlineFunctionForCall(CodeBlock* codeBlock)
{
    return codeBlock->instructionCount() <= Options::maximumFunctionForCallInlineCandidateInstructionCount()
        && !codeBlock->ownerExecutable()->needsActivation()
        && codeBlock->ownerExecutable()->isInliningCandidate();
}
bool mightInlineFunctionForClosureCall(CodeBlock* codeBlock)
{
    return codeBlock->instructionCount() <= Options::maximumFunctionForClosureCallInlineCandidateInstructionCount()
        && !codeBlock->ownerExecutable()->needsActivation()
        && codeBlock->ownerExecutable()->isInliningCandidate();
}
bool mightInlineFunctionForConstruct(CodeBlock* codeBlock)
{
    return codeBlock->instructionCount() <= Options::maximumFunctionForConstructInlineCandidateInstructionCount()
        && !codeBlock->ownerExecutable()->needsActivation()
        && codeBlock->ownerExecutable()->isInliningCandidate();
}

inline void debugFail(CodeBlock* codeBlock, OpcodeID opcodeID, CapabilityLevel result)
{
    if (Options::verboseCompilation() && !canCompile(result))
        dataLog("Cannot compile code block ", *codeBlock, " because of opcode ", opcodeNames[opcodeID], "\n");
}

CapabilityLevel capabilityLevel(OpcodeID opcodeID, CodeBlock* codeBlock, Instruction* pc)
{
    switch (opcodeID) {
    case op_enter:
    case op_touch_entry:
    case op_to_this:
    case op_create_this:
    case op_get_callee:
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
    case op_div:
    case op_debug:
    case op_profile_will_call:
    case op_profile_did_call:
    case op_mov:
    case op_captured_mov:
    case op_check_has_instance:
    case op_instanceof:
    case op_is_undefined:
    case op_is_boolean:
    case op_is_number:
    case op_is_string:
    case op_is_object:
    case op_is_function:
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
    case op_put_by_val_direct:
    case op_get_by_id:
    case op_get_by_id_out_of_line:
    case op_get_array_length:
    case op_put_by_id:
    case op_put_by_id_out_of_line:
    case op_put_by_id_transition_direct:
    case op_put_by_id_transition_direct_out_of_line:
    case op_put_by_id_transition_normal:
    case op_put_by_id_transition_normal_out_of_line:
    case op_init_global_const_nop:
    case op_init_global_const:
    case op_jmp:
    case op_jtrue:
    case op_jfalse:
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
    case op_ret:
    case op_end:
    case op_new_object:
    case op_new_array:
    case op_new_array_with_size:
    case op_new_array_buffer:
    case op_strcat:
    case op_to_primitive:
    case op_throw:
    case op_throw_static_error:
    case op_call:
    case op_construct:
    case op_init_lazy_reg:
    case op_create_arguments:
    case op_tear_off_arguments:
    case op_get_argument_by_val:
    case op_get_arguments_length:
    case op_jneq_ptr:
    case op_typeof:
    case op_to_number:
    case op_switch_imm:
    case op_switch_char:
    case op_in:
    case op_get_from_scope:
        return CanCompileAndInline;

    case op_put_to_scope: {
        ResolveType resolveType = ResolveModeAndType(pc[4].u.operand).type();
        // If we're writing to a readonly property we emit a Dynamic put that
        // the DFG can't currently handle.
        if (resolveType == Dynamic)
            return CannotCompile;
        return CanCompileAndInline;
    }

    case op_resolve_scope: {
        // We don't compile 'catch' or 'with', so there's no point in compiling variable resolution within them.
        ResolveType resolveType = ResolveModeAndType(pc[3].u.operand).type();
        if (resolveType == Dynamic)
            return CannotCompile;
        return CanCompileAndInline;
    }

    case op_call_varargs:
        if (codeBlock->usesArguments() && pc[4].u.operand == codeBlock->argumentsRegister().offset())
            return CanInline;
        // FIXME: We should handle this.
        // https://bugs.webkit.org/show_bug.cgi?id=127626
        return CannotCompile;

    case op_new_regexp: 
    case op_create_activation:
    case op_tear_off_activation:
    case op_new_func:
    case op_new_captured_func:
    case op_new_func_exp:
    case op_switch_string: // Don't inline because we don't want to copy string tables in the concurrent JIT.
        return CanCompile;

    default:
        return CannotCompile;
    }
}

CapabilityLevel capabilityLevel(CodeBlock* codeBlock)
{
    Interpreter* interpreter = codeBlock->vm()->interpreter;
    Instruction* instructionsBegin = codeBlock->instructions().begin();
    unsigned instructionCount = codeBlock->instructions().size();
    CapabilityLevel result = CanCompileAndInline;
    
    for (unsigned bytecodeOffset = 0; bytecodeOffset < instructionCount; ) {
        switch (interpreter->getOpcodeID(instructionsBegin[bytecodeOffset].u.opcode)) {
#define DEFINE_OP(opcode, length) \
        case opcode: { \
            CapabilityLevel newResult = leastUpperBound(result, capabilityLevel(opcode, codeBlock, instructionsBegin + bytecodeOffset)); \
            if (newResult != result) { \
                debugFail(codeBlock, opcode, newResult); \
                result = newResult; \
            } \
            bytecodeOffset += length; \
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
