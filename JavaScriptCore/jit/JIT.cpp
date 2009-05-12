
/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#include "JIT.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "Interpreter.h"
#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace JSC {

void ctiPatchCallByReturnAddress(MacroAssembler::ProcessorReturnAddress returnAddress, void* newCalleeFunction)
{
    returnAddress.relinkCallerToFunction(newCalleeFunction);
}

void ctiPatchNearCallByReturnAddress(MacroAssembler::ProcessorReturnAddress returnAddress, void* newCalleeFunction)
{
    returnAddress.relinkNearCallerToFunction(newCalleeFunction);
}

JIT::JIT(JSGlobalData* globalData, CodeBlock* codeBlock)
    : m_interpreter(globalData->interpreter)
    , m_globalData(globalData)
    , m_codeBlock(codeBlock)
    , m_labels(codeBlock ? codeBlock->instructions().size() : 0)
    , m_propertyAccessCompilationInfo(codeBlock ? codeBlock->numberOfStructureStubInfos() : 0)
    , m_callStructureStubCompilationInfo(codeBlock ? codeBlock->numberOfCallLinkInfos() : 0)
    , m_lastResultBytecodeRegister(std::numeric_limits<int>::max())
    , m_jumpTargetsPosition(0)
{
}

void JIT::compileOpStrictEq(Instruction* currentInstruction, CompileOpStrictEqType type)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitGetVirtualRegisters(src1, regT0, src2, regT1);

    // Jump to a slow case if either operand is a number, or if both are JSCell*s.
    move(regT0, regT2);
    orPtr(regT1, regT2);
    addSlowCase(emitJumpIfJSCell(regT2));
    addSlowCase(emitJumpIfImmediateNumber(regT2));

    if (type == OpStrictEq)
        set32(Equal, regT1, regT0, regT0);
    else
        set32(NotEqual, regT1, regT0, regT0);
    emitTagAsBoolImmediate(regT0);

    emitPutVirtualRegister(dst);
}

void JIT::emitTimeoutCheck()
{
    Jump skipTimeout = branchSub32(NonZero, Imm32(1), timeoutCheckRegister);
    JITStubCall(this, JITStubs::cti_timeout_check).call(timeoutCheckRegister);
    skipTimeout.link(this);

    killLastResultRegister();
}


#define NEXT_OPCODE(name) \
    m_bytecodeIndex += OPCODE_LENGTH(name); \
    break;

#define DEFINE_BINARY_OP(name) \
    case name: { \
        JITStubCall stubCall(this, JITStubs::cti_##name); \
        stubCall.addArgument(currentInstruction[2].u.operand, regT2); \
        stubCall.addArgument(currentInstruction[3].u.operand, regT2); \
        stubCall.call(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

#define DEFINE_UNARY_OP(name) \
    case name: { \
        JITStubCall stubCall(this, JITStubs::cti_##name); \
        stubCall.addArgument(currentInstruction[2].u.operand, regT2); \
        stubCall.call(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

void JIT::privateCompileMainPass()
{
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    unsigned instructionCount = m_codeBlock->instructions().size();

    m_propertyAccessInstructionIndex = 0;
    m_globalResolveInfoIndex = 0;
    m_callLinkInfoIndex = 0;

    for (m_bytecodeIndex = 0; m_bytecodeIndex < instructionCount; ) {
        Instruction* currentInstruction = instructionsBegin + m_bytecodeIndex;
        ASSERT_WITH_MESSAGE(m_interpreter->isOpcode(currentInstruction->u.opcode), "privateCompileMainPass gone bad @ %d", m_bytecodeIndex);

#if ENABLE(OPCODE_SAMPLING)
        if (m_bytecodeIndex > 0) // Avoid the overhead of sampling op_enter twice.
            sampleInstruction(currentInstruction);
#endif

        if (m_labels[m_bytecodeIndex].isUsed())
            killLastResultRegister();
        
        m_labels[m_bytecodeIndex] = label();
        OpcodeID opcodeID = m_interpreter->getOpcodeID(currentInstruction->u.opcode);

        switch (opcodeID) {
        DEFINE_BINARY_OP(op_del_by_val)
        DEFINE_BINARY_OP(op_div)
        DEFINE_BINARY_OP(op_in)
        DEFINE_BINARY_OP(op_less)
        DEFINE_BINARY_OP(op_lesseq)
        DEFINE_BINARY_OP(op_urshift)
        DEFINE_UNARY_OP(op_get_pnames)
        DEFINE_UNARY_OP(op_is_boolean)
        DEFINE_UNARY_OP(op_is_function)
        DEFINE_UNARY_OP(op_is_number)
        DEFINE_UNARY_OP(op_is_object)
        DEFINE_UNARY_OP(op_is_string)
        DEFINE_UNARY_OP(op_is_undefined)
        DEFINE_UNARY_OP(op_negate)
        DEFINE_UNARY_OP(op_typeof)

        // Arithmetic

        case op_add: {
            emit_op_add(currentInstruction);
            NEXT_OPCODE(op_add);
        }
        case op_sub: {
            emit_op_sub(currentInstruction);
            NEXT_OPCODE(op_sub);
        }
        case op_mul: {
            emit_op_mul(currentInstruction);
            NEXT_OPCODE(op_mul);
        }
        case op_mod: {
            emit_op_mod(currentInstruction);
            NEXT_OPCODE(op_mod);
        }
        case op_bitand: {
            emit_op_bitand(currentInstruction);
            NEXT_OPCODE(op_bitand);
        }
        case op_lshift: {
            emit_op_lshift(currentInstruction);
            NEXT_OPCODE(op_lshift);
        }
        case op_rshift: {
            emit_op_rshift(currentInstruction);
            NEXT_OPCODE(op_rshift);
        }
        case op_pre_inc: {
            emit_op_pre_inc(currentInstruction);
            NEXT_OPCODE(op_pre_inc);
        }
        case op_pre_dec: {
            emit_op_pre_dec(currentInstruction);
            NEXT_OPCODE(op_pre_dec);
        }
        case op_post_inc: {
            emit_op_post_inc(currentInstruction);
            NEXT_OPCODE(op_post_inc);
        }
        case op_post_dec: {
            emit_op_post_dec(currentInstruction);
            NEXT_OPCODE(op_post_dec);
        }

        /* in JITOpcodes */
        case op_bitnot: {
            emit_op_bitnot(currentInstruction);
            NEXT_OPCODE(op_bitnot);
        }
        case op_bitxor: {
            emit_op_bitxor(currentInstruction);
            NEXT_OPCODE(op_bitxor);
        }
        case op_bitor: {
            emit_op_bitor(currentInstruction);
            NEXT_OPCODE(op_bitor);
        }
        case op_not: {
            emit_op_not(currentInstruction);
            NEXT_OPCODE(op_not);
        }
        /* in JITOpcodes */


        // Comparison

        case op_eq: {
            emit_op_eq(currentInstruction);
            NEXT_OPCODE(op_eq);
        }
        case op_neq: {
            emit_op_neq(currentInstruction);
            NEXT_OPCODE(op_neq);
        }
        case op_eq_null: {
            emit_op_eq_null(currentInstruction);
            NEXT_OPCODE(op_eq_null);
        }
        case op_neq_null: {
            emit_op_neq_null(currentInstruction);
            NEXT_OPCODE(op_neq_null);
        }
        case op_stricteq: {
            emit_op_stricteq(currentInstruction);
            NEXT_OPCODE(op_stricteq);
        }
        case op_nstricteq: {
            emit_op_nstricteq(currentInstruction);
            NEXT_OPCODE(op_nstricteq);
        }


        // Jump / Loop

        case op_jnless: {
            emit_op_jnless(currentInstruction);
            NEXT_OPCODE(op_jnless);
        }
        case op_jnlesseq: {
            emit_op_jnlesseq(currentInstruction);
            NEXT_OPCODE(op_jnlesseq);
        }
        case op_jmp: {
            emit_op_jmp(currentInstruction);
            NEXT_OPCODE(op_jmp);
        }
        case op_loop: {
            emit_op_loop(currentInstruction);
            NEXT_OPCODE(op_loop);
        }
        case op_loop_if_less: {
            emit_op_loop_if_less(currentInstruction);
            NEXT_OPCODE(op_loop_if_less);
        }
        case op_loop_if_lesseq: {
            emit_op_loop_if_lesseq(currentInstruction);
            NEXT_OPCODE(op_loop_if_lesseq);
        }
        case op_loop_if_true: {
            emit_op_loop_if_true(currentInstruction);
            NEXT_OPCODE(op_loop_if_true);
        }
        case op_jtrue: {
            emit_op_jtrue(currentInstruction);
            NEXT_OPCODE(op_jtrue);
        }
        case op_jfalse: {
            emit_op_jfalse(currentInstruction);
            NEXT_OPCODE(op_jfalse);
        }
        case op_jeq_null: {
            emit_op_jeq_null(currentInstruction);
            NEXT_OPCODE(op_jeq_null);
        }
        case op_jneq_null: {
            emit_op_jneq_null(currentInstruction);
            NEXT_OPCODE(op_jneq_null);
        }
        case op_jneq_ptr: {
            emit_op_jneq_ptr(currentInstruction);
            NEXT_OPCODE(op_jneq_ptr);
        }


        // Property Access

        case op_get_by_id: {
            emit_op_get_by_id(currentInstruction);
            NEXT_OPCODE(op_get_by_id);
        }
        case op_put_by_id: {
            emit_op_put_by_id(currentInstruction);
            NEXT_OPCODE(op_put_by_id);
        }
        case op_del_by_id: {
            emit_op_del_by_id(currentInstruction);
            NEXT_OPCODE(op_del_by_id);
        }
        case op_get_by_val: {
            emit_op_get_by_val(currentInstruction);
            NEXT_OPCODE(op_get_by_val);
        }
        case op_put_by_val: {
            emit_op_put_by_val(currentInstruction);
            NEXT_OPCODE(op_put_by_val);
        }
        case op_put_by_index: {
            emit_op_put_by_index(currentInstruction);
            NEXT_OPCODE(op_put_by_index);
        }
        case op_put_getter: {
            emit_op_put_getter(currentInstruction);
            NEXT_OPCODE(op_put_getter);
        }
        case op_put_setter: {
            emit_op_put_setter(currentInstruction);
            NEXT_OPCODE(op_put_setter);
        }
        
        // Variables

        case op_get_global_var: {
            emit_op_get_global_var(currentInstruction);
            NEXT_OPCODE(op_get_global_var);
        }
        case op_put_global_var: {
            emit_op_put_global_var(currentInstruction);
            NEXT_OPCODE(op_put_global_var);
        }
        case op_get_scoped_var: {
            emit_op_get_scoped_var(currentInstruction);
            NEXT_OPCODE(op_get_scoped_var);
        }
        case op_put_scoped_var: {
            emit_op_put_scoped_var(currentInstruction);
            NEXT_OPCODE(op_put_scoped_var);
        }

        // Call

        case op_call: {
            emit_op_call(currentInstruction);
            NEXT_OPCODE(op_call);
        }
        case op_call_eval: {
            emit_op_call_eval(currentInstruction);
            NEXT_OPCODE(op_call_eval);
        }
        case op_load_varargs: {
            emit_op_load_varargs(currentInstruction);
            NEXT_OPCODE(op_load_varargs);
        }
        case op_call_varargs: {
            emit_op_call_varargs(currentInstruction);
            NEXT_OPCODE(op_call_varargs);
        }
        case op_construct: {
            emit_op_construct(currentInstruction);
            NEXT_OPCODE(op_construct);
        }
        case op_tear_off_activation: {
            emit_op_tear_off_activation(currentInstruction);
            NEXT_OPCODE(op_tear_off_activation);
        }
        case op_tear_off_arguments: {
            emit_op_tear_off_arguments(currentInstruction);
            NEXT_OPCODE(op_tear_off_arguments);
        }
        case op_ret: {
            emit_op_ret(currentInstruction);
            NEXT_OPCODE(op_ret);
        }


        // Profiling / Debugging

        case op_profile_will_call: {
            emit_op_profile_will_call(currentInstruction);
            NEXT_OPCODE(op_profile_will_call);
        }
        case op_profile_did_call: {
            emit_op_profile_did_call(currentInstruction);
            NEXT_OPCODE(op_profile_did_call);
        }
        case op_debug: {
            emit_op_debug(currentInstruction);
            NEXT_OPCODE(op_debug);
        }


        // Unsorted

        case op_mov: {
            emit_op_mov(currentInstruction);
            NEXT_OPCODE(op_mov);
        }
        case op_end: {
            emit_op_end(currentInstruction);
            NEXT_OPCODE(op_end);
        }
        case op_new_object: {
            emit_op_new_object(currentInstruction);
            NEXT_OPCODE(op_new_object);
        }
        case op_instanceof: {
            emit_op_instanceof(currentInstruction);
            NEXT_OPCODE(op_instanceof);
        }
        case op_new_func: {
            emit_op_new_func(currentInstruction);
            NEXT_OPCODE(op_new_func);
        }
        case op_new_array: {
            emit_op_new_array(currentInstruction);
            NEXT_OPCODE(op_new_array);
        }
        case op_resolve: {
            emit_op_resolve(currentInstruction);
            NEXT_OPCODE(op_resolve);
        }
        case op_construct_verify: {
            emit_op_construct_verify(currentInstruction);
            NEXT_OPCODE(op_construct_verify);
        }
        case op_to_primitive: {
            emit_op_to_primitive(currentInstruction);
            NEXT_OPCODE(op_to_primitive);
        }
        case op_strcat: {
            emit_op_strcat(currentInstruction);
            NEXT_OPCODE(op_strcat);
        }
        case op_resolve_func: {
            emit_op_resolve_func(currentInstruction);
            NEXT_OPCODE(op_resolve_func);
        }
        case op_resolve_base: {
            emit_op_resolve_base(currentInstruction);
            NEXT_OPCODE(op_resolve_base);
        }
        case op_resolve_skip: {
            emit_op_resolve_skip(currentInstruction);
            NEXT_OPCODE(op_resolve_skip);
        }
        case op_resolve_global: {
            emit_op_resolve_global(currentInstruction);
            NEXT_OPCODE(op_resolve_global);
        }
        case op_unexpected_load: {
            emit_op_unexpected_load(currentInstruction);
            NEXT_OPCODE(op_unexpected_load);
        }
        case op_jsr: {
            emit_op_jsr(currentInstruction);
            NEXT_OPCODE(op_jsr);
        }
        case op_sret: {
            emit_op_sret(currentInstruction);
            NEXT_OPCODE(op_sret);
        }
        case op_resolve_with_base: {
            emit_op_resolve_with_base(currentInstruction);
            NEXT_OPCODE(op_resolve_with_base);
        }
        case op_new_func_exp: {
            emit_op_new_func_exp(currentInstruction);
            NEXT_OPCODE(op_new_func_exp);
        }
        case op_new_regexp: {
            emit_op_new_regexp(currentInstruction);
            NEXT_OPCODE(op_new_regexp);
        }
        case op_throw: {
            emit_op_throw(currentInstruction);
            NEXT_OPCODE(op_throw);
        }
        case op_next_pname: {
            emit_op_next_pname(currentInstruction);
            NEXT_OPCODE(op_next_pname);
        }
        case op_push_scope: {
            emit_op_push_scope(currentInstruction);
            NEXT_OPCODE(op_push_scope);
        }
        case op_pop_scope: {
            emit_op_pop_scope(currentInstruction);
            NEXT_OPCODE(op_pop_scope);
        }
        case op_to_jsnumber: {
            emit_op_to_jsnumber(currentInstruction);
            NEXT_OPCODE(op_to_jsnumber);
        }
        case op_push_new_scope: {
            emit_op_push_new_scope(currentInstruction);
            NEXT_OPCODE(op_push_new_scope);
        }
        case op_catch: {
            emit_op_catch(currentInstruction);
            NEXT_OPCODE(op_catch);
        }
        case op_jmp_scopes: {
            emit_op_jmp_scopes(currentInstruction);
            NEXT_OPCODE(op_jmp_scopes);
        }
        case op_switch_imm: {
            emit_op_switch_imm(currentInstruction);
            NEXT_OPCODE(op_switch_imm);
        }
        case op_switch_char: {
            emit_op_switch_char(currentInstruction);
            NEXT_OPCODE(op_switch_char);
        }
        case op_switch_string: {
            emit_op_switch_string(currentInstruction);
            NEXT_OPCODE(op_switch_string);
        }
        case op_new_error: {
            emit_op_new_error(currentInstruction);
            NEXT_OPCODE(op_new_error);
        }
        case op_enter: {
            emit_op_enter(currentInstruction);
            NEXT_OPCODE(op_enter);
        }
        case op_enter_with_activation: {
            emit_op_enter_with_activation(currentInstruction);
            NEXT_OPCODE(op_enter_with_activation);
        }
        case op_create_arguments: {
            emit_op_create_arguments(currentInstruction);
            NEXT_OPCODE(op_create_arguments);
        }
        case op_convert_this: {
            emit_op_convert_this(currentInstruction);
            NEXT_OPCODE(op_convert_this);
        }
        case op_get_array_length:
        case op_get_by_id_chain:
        case op_get_by_id_generic:
        case op_get_by_id_proto:
        case op_get_by_id_proto_list:
        case op_get_by_id_self:
        case op_get_by_id_self_list:
        case op_get_string_length:
        case op_put_by_id_generic:
        case op_put_by_id_replace:
        case op_put_by_id_transition:
            ASSERT_NOT_REACHED();
        }
    }

    ASSERT(m_propertyAccessInstructionIndex == m_codeBlock->numberOfStructureStubInfos());
    ASSERT(m_callLinkInfoIndex == m_codeBlock->numberOfCallLinkInfos());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeIndex = (unsigned)-1;
#endif
}


void JIT::privateCompileLinkPass()
{
    unsigned jmpTableCount = m_jmpTable.size();
    for (unsigned i = 0; i < jmpTableCount; ++i)
        m_jmpTable[i].from.linkTo(m_labels[m_jmpTable[i].toBytecodeIndex], this);
    m_jmpTable.clear();
}

void JIT::privateCompileSlowCases()
{
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();

    m_propertyAccessInstructionIndex = 0;
    m_callLinkInfoIndex = 0;

    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end();) {
        // FIXME: enable peephole optimizations for slow cases when applicable
        killLastResultRegister();

        m_bytecodeIndex = iter->to;
#ifndef NDEBUG
        unsigned firstTo = m_bytecodeIndex;
#endif
        Instruction* currentInstruction = instructionsBegin + m_bytecodeIndex;

        switch (OpcodeID opcodeID = m_interpreter->getOpcodeID(currentInstruction->u.opcode)) {
        case op_add: {
            emitSlow_op_add(currentInstruction, iter);
            NEXT_OPCODE(op_add);
        }
        case op_sub: {
            emitSlow_op_sub(currentInstruction, iter);
            NEXT_OPCODE(op_sub);
        }
        case op_mul: {
            emitSlow_op_mul(currentInstruction, iter);
            NEXT_OPCODE(op_mul);
        }
        case op_mod: {
            emitSlow_op_mod(currentInstruction, iter);
            NEXT_OPCODE(op_mod);
        }
        case op_bitand: {
            emitSlow_op_bitand(currentInstruction, iter);
            NEXT_OPCODE(op_bitand);
        }
        case op_lshift: {
            emitSlow_op_lshift(currentInstruction, iter);
            NEXT_OPCODE(op_lshift);
        }
        case op_rshift: {
            emitSlow_op_rshift(currentInstruction, iter);
            NEXT_OPCODE(op_rshift);
        }
        case op_jnless: {
            emitSlow_op_jnless(currentInstruction, iter);
            NEXT_OPCODE(op_jnless);
        }
        case op_jnlesseq: {
            emitSlow_op_jnlesseq(currentInstruction, iter);
            NEXT_OPCODE(op_jnlesseq);
        }
        case op_pre_dec: {
            emitSlow_op_pre_dec(currentInstruction, iter);
            NEXT_OPCODE(op_pre_dec);
        }
        case op_pre_inc: {
            emitSlow_op_pre_inc(currentInstruction, iter);
            NEXT_OPCODE(op_pre_inc);
        }
        case op_post_inc: {
            emitSlow_op_post_inc(currentInstruction, iter);
            NEXT_OPCODE(op_post_inc);
        }
        case op_post_dec: {
            emitSlow_op_post_dec(currentInstruction, iter);
            NEXT_OPCODE(op_post_dec);
        }
        case op_convert_this: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_convert_this);
            stubCall.addArgument(regT0);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_convert_this);
        }
        case op_construct_verify: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitGetVirtualRegister(currentInstruction[2].u.operand, regT0);
            emitPutVirtualRegister(currentInstruction[1].u.operand);

            NEXT_OPCODE(op_construct_verify);
        }
        case op_to_primitive: {
            linkSlowCase(iter);

            JITStubCall stubCall(this, JITStubs::cti_op_to_primitive);
            stubCall.addArgument(regT0);
            stubCall.call(currentInstruction[1].u.operand);

            NEXT_OPCODE(op_to_primitive);
        }
        case op_get_by_val: {
            // The slow case that handles accesses to arrays (below) may jump back up to here. 
            Label beginGetByValSlow(this);

            Jump notImm = getSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitFastArithIntToImmNoCheck(regT1, regT1);

            notImm.link(this);
            JITStubCall stubCall(this, JITStubs::cti_op_get_by_val);
            stubCall.addArgument(regT0);
            stubCall.addArgument(regT1);
            stubCall.call(currentInstruction[1].u.operand);
            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_get_by_val));

            // This is slow case that handles accesses to arrays above the fast cut-off.
            // First, check if this is an access to the vector
            linkSlowCase(iter);
            branch32(AboveOrEqual, regT1, Address(regT2, FIELD_OFFSET(ArrayStorage, m_vectorLength)), beginGetByValSlow);

            // okay, missed the fast region, but it is still in the vector.  Get the value.
            loadPtr(BaseIndex(regT2, regT1, ScalePtr, FIELD_OFFSET(ArrayStorage, m_vector[0])), regT2);
            // Check whether the value loaded is zero; if so we need to return undefined.
            branchTestPtr(Zero, regT2, beginGetByValSlow);
            move(regT2, regT0);
            emitPutVirtualRegister(currentInstruction[1].u.operand, regT0);

            NEXT_OPCODE(op_get_by_val);
        }
        case op_loop_if_less: {
            unsigned op1 = currentInstruction[1].u.operand;
            unsigned op2 = currentInstruction[2].u.operand;
            unsigned target = currentInstruction[3].u.operand;
            if (isOperandConstantImmediateInt(op2)) {
                linkSlowCase(iter);
                JITStubCall stubCall(this, JITStubs::cti_op_loop_if_less);
                stubCall.addArgument(regT0);
                stubCall.addArgument(op2, regT2);
                stubCall.call();
                emitJumpSlowToHot(branchTest32(NonZero, regT0), target + 3);
            } else if (isOperandConstantImmediateInt(op1)) {
                linkSlowCase(iter);
                JITStubCall stubCall(this, JITStubs::cti_op_loop_if_less);
                stubCall.addArgument(op1, regT1);
                stubCall.addArgument(regT0);
                stubCall.call();
                emitJumpSlowToHot(branchTest32(NonZero, regT0), target + 3);
            } else {
                linkSlowCase(iter);
                linkSlowCase(iter);
                JITStubCall stubCall(this, JITStubs::cti_op_loop_if_less);
                stubCall.addArgument(regT0);
                stubCall.addArgument(regT1);
                stubCall.call();
                emitJumpSlowToHot(branchTest32(NonZero, regT0), target + 3);
            }
            NEXT_OPCODE(op_loop_if_less);
        }
        case op_put_by_id: {
            compilePutByIdSlowCase(currentInstruction[1].u.operand, &(m_codeBlock->identifier(currentInstruction[2].u.operand)), currentInstruction[3].u.operand, iter, m_propertyAccessInstructionIndex++);
            NEXT_OPCODE(op_put_by_id);
        }
        case op_get_by_id: {
            compileGetByIdSlowCase(currentInstruction[1].u.operand, currentInstruction[2].u.operand, &(m_codeBlock->identifier(currentInstruction[3].u.operand)), iter, m_propertyAccessInstructionIndex++);
            NEXT_OPCODE(op_get_by_id);
        }
        case op_loop_if_lesseq: {
            unsigned op2 = currentInstruction[2].u.operand;
            unsigned target = currentInstruction[3].u.operand;
            if (isOperandConstantImmediateInt(op2)) {
                linkSlowCase(iter);
                JITStubCall stubCall(this, JITStubs::cti_op_loop_if_lesseq);
                stubCall.addArgument(regT0);
                stubCall.addArgument(currentInstruction[2].u.operand, regT2);
                stubCall.call();
                emitJumpSlowToHot(branchTest32(NonZero, regT0), target + 3);
            } else {
                linkSlowCase(iter);
                linkSlowCase(iter);
                JITStubCall stubCall(this, JITStubs::cti_op_loop_if_lesseq);
                stubCall.addArgument(regT0);
                stubCall.addArgument(regT1);
                stubCall.call();
                emitJumpSlowToHot(branchTest32(NonZero, regT0), target + 3);
            }
            NEXT_OPCODE(op_loop_if_lesseq);
        }
        case op_put_by_val: {
            // Normal slow cases - either is not an immediate imm, or is an array.
            Jump notImm = getSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitFastArithIntToImmNoCheck(regT1, regT1);

            notImm.link(this); {
                JITStubCall stubCall(this, JITStubs::cti_op_put_by_val);
                stubCall.addArgument(regT0);
                stubCall.addArgument(regT1);
                stubCall.addArgument(currentInstruction[3].u.operand, regT2);
                stubCall.call();
                emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_put_by_val));
            }

            // slow cases for immediate int accesses to arrays
            linkSlowCase(iter);
            linkSlowCase(iter); {
                JITStubCall stubCall(this, JITStubs::cti_op_put_by_val_array);
                stubCall.addArgument(regT0);
                stubCall.addArgument(regT1);
                stubCall.addArgument(currentInstruction[3].u.operand, regT2);
                stubCall.call();
            }
            NEXT_OPCODE(op_put_by_val);
        }
        case op_loop_if_true: {
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_jtrue);
            stubCall.addArgument(regT0);
            stubCall.call();
            emitJumpSlowToHot(branchTest32(NonZero, regT0), currentInstruction[2].u.operand + 2);
            NEXT_OPCODE(op_loop_if_true);
        }
        case op_not: {
            linkSlowCase(iter);
            xorPtr(Imm32(static_cast<int32_t>(JSImmediate::FullTagTypeBool)), regT0);
            JITStubCall stubCall(this, JITStubs::cti_op_not);
            stubCall.addArgument(regT0);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_not);
        }
        case op_jfalse: {
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_jtrue);
            stubCall.addArgument(regT0);
            stubCall.call();
            emitJumpSlowToHot(branchTest32(Zero, regT0), currentInstruction[2].u.operand + 2); // inverted!
            NEXT_OPCODE(op_jfalse);
        }
        case op_bitnot: {
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_bitnot);
            stubCall.addArgument(regT0);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitnot);
        }
        case op_jtrue: {
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_jtrue);
            stubCall.addArgument(regT0);
            stubCall.call();
            emitJumpSlowToHot(branchTest32(NonZero, regT0), currentInstruction[2].u.operand + 2);
            NEXT_OPCODE(op_jtrue);
        }
        case op_bitxor: {
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_bitxor);
            stubCall.addArgument(regT0);
            stubCall.addArgument(regT1);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitxor);
        }
        case op_bitor: {
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_bitor);
            stubCall.addArgument(regT0);
            stubCall.addArgument(regT1);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitor);
        }
        case op_eq: {
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_eq);
            stubCall.addArgument(regT0);
            stubCall.addArgument(regT1);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_eq);
        }
        case op_neq: {
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_neq);
            stubCall.addArgument(regT0);
            stubCall.addArgument(regT1);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_neq);
        }
        case op_stricteq: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_stricteq);
            stubCall.addArgument(regT0);
            stubCall.addArgument(regT1);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_stricteq);
        }
        case op_nstricteq: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_nstricteq);
            stubCall.addArgument(regT0);
            stubCall.addArgument(regT1);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_nstricteq);
        }
        case op_instanceof: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            JITStubCall stubCall(this, JITStubs::cti_op_instanceof);
            stubCall.addArgument(currentInstruction[2].u.operand, regT2);
            stubCall.addArgument(currentInstruction[3].u.operand, regT2);
            stubCall.addArgument(currentInstruction[4].u.operand, regT2);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_instanceof);
        }
        case op_call: {
            compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, opcodeID);
            NEXT_OPCODE(op_call);
        }
        case op_call_eval: {
            compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, opcodeID);
            NEXT_OPCODE(op_call_eval);
        }
        case op_call_varargs: {
            compileOpCallVarargsSlowCase(currentInstruction, iter);
            NEXT_OPCODE(op_call_varargs);
        }
        case op_construct: {
            compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, opcodeID);
            NEXT_OPCODE(op_construct);
        }
        case op_to_jsnumber: {
            linkSlowCaseIfNotJSCell(iter, currentInstruction[2].u.operand);
            linkSlowCase(iter);

            JITStubCall stubCall(this, JITStubs::cti_op_to_jsnumber);
            stubCall.addArgument(regT0);
            stubCall.call(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_to_jsnumber);
        }

        default:
            ASSERT_NOT_REACHED();
        }

        ASSERT_WITH_MESSAGE(iter == m_slowCases.end() || firstTo != iter->to,"Not enough jumps linked in slow case codegen.");
        ASSERT_WITH_MESSAGE(firstTo == (iter - 1)->to, "Too many jumps linked in slow case codegen.");

        emitJumpSlowToHot(jump(), 0);
    }

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    ASSERT(m_propertyAccessInstructionIndex == m_codeBlock->numberOfStructureStubInfos());
#endif
    ASSERT(m_callLinkInfoIndex == m_codeBlock->numberOfCallLinkInfos());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeIndex = (unsigned)-1;
#endif
}

void JIT::privateCompile()
{
    sampleCodeBlock(m_codeBlock);
#if ENABLE(OPCODE_SAMPLING)
    sampleInstruction(m_codeBlock->instructions().begin());
#endif

    // Could use a pop_m, but would need to offset the following instruction if so.
    pop(regT2);
    emitPutToCallFrameHeader(regT2, RegisterFile::ReturnPC);

    Jump slowRegisterFileCheck;
    Label afterRegisterFileCheck;
    if (m_codeBlock->codeType() == FunctionCode) {
        // In the case of a fast linked call, we do not set this up in the caller.
        emitPutImmediateToCallFrameHeader(m_codeBlock, RegisterFile::CodeBlock);

        peek(regT0, FIELD_OFFSET(JITStackFrame, registerFile) / sizeof (void*));
        addPtr(Imm32(m_codeBlock->m_numCalleeRegisters * sizeof(Register)), callFrameRegister, regT1);
        
        slowRegisterFileCheck = branch32(GreaterThan, regT1, Address(regT0, FIELD_OFFSET(RegisterFile, m_end)));
        afterRegisterFileCheck = label();
    }

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();

    if (m_codeBlock->codeType() == FunctionCode) {
        slowRegisterFileCheck.link(this);
        m_bytecodeIndex = 0;
        JITStubCall(this, JITStubs::cti_register_file_check).call();
#ifndef NDEBUG
        m_bytecodeIndex = (unsigned)-1; // Reset this, in order to guard its use with ASSERTs.
#endif
        jump(afterRegisterFileCheck);
    }

    ASSERT(m_jmpTable.isEmpty());

    RefPtr<ExecutablePool> allocator = m_globalData->executableAllocator.poolForSize(m_assembler.size());
    void* code = m_assembler.executableCopy(allocator.get());
    JITCodeRef codeRef(code, allocator);
#ifndef NDEBUG
    codeRef.codeSize = m_assembler.size();
#endif

    PatchBuffer patchBuffer(code);

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (unsigned i = 0; i < m_switches.size(); ++i) {
        SwitchRecord record = m_switches[i];
        unsigned bytecodeIndex = record.bytecodeIndex;

        if (record.type != SwitchRecord::String) {
            ASSERT(record.type == SwitchRecord::Immediate || record.type == SwitchRecord::Character); 
            ASSERT(record.jumpTable.simpleJumpTable->branchOffsets.size() == record.jumpTable.simpleJumpTable->ctiOffsets.size());

            record.jumpTable.simpleJumpTable->ctiDefault = patchBuffer.locationOf(m_labels[bytecodeIndex + 3 + record.defaultOffset]);

            for (unsigned j = 0; j < record.jumpTable.simpleJumpTable->branchOffsets.size(); ++j) {
                unsigned offset = record.jumpTable.simpleJumpTable->branchOffsets[j];
                record.jumpTable.simpleJumpTable->ctiOffsets[j] = offset ? patchBuffer.locationOf(m_labels[bytecodeIndex + 3 + offset]) : record.jumpTable.simpleJumpTable->ctiDefault;
            }
        } else {
            ASSERT(record.type == SwitchRecord::String);

            record.jumpTable.stringJumpTable->ctiDefault = patchBuffer.locationOf(m_labels[bytecodeIndex + 3 + record.defaultOffset]);

            StringJumpTable::StringOffsetTable::iterator end = record.jumpTable.stringJumpTable->offsetTable.end();            
            for (StringJumpTable::StringOffsetTable::iterator it = record.jumpTable.stringJumpTable->offsetTable.begin(); it != end; ++it) {
                unsigned offset = it->second.branchOffset;
                it->second.ctiOffset = offset ? patchBuffer.locationOf(m_labels[bytecodeIndex + 3 + offset]) : record.jumpTable.stringJumpTable->ctiDefault;
            }
        }
    }

    for (size_t i = 0; i < m_codeBlock->numberOfExceptionHandlers(); ++i) {
        HandlerInfo& handler = m_codeBlock->exceptionHandler(i);
        handler.nativeCode = patchBuffer.locationOf(m_labels[handler.target]);
    }

    for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
        if (iter->to)
            patchBuffer.link(iter->from, iter->to);
    }

    if (m_codeBlock->hasExceptionInfo()) {
        m_codeBlock->callReturnIndexVector().reserveCapacity(m_calls.size());
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter)
            m_codeBlock->callReturnIndexVector().append(CallReturnOffsetToBytecodeIndex(patchBuffer.returnAddressOffset(iter->from), iter->bytecodeIndex));
    }

    // Link absolute addresses for jsr
    for (Vector<JSRInfo>::iterator iter = m_jsrSites.begin(); iter != m_jsrSites.end(); ++iter)
        patchBuffer.patch(iter->storeLocation, patchBuffer.locationOf(iter->target).addressForJSR());

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    for (unsigned i = 0; i < m_codeBlock->numberOfStructureStubInfos(); ++i) {
        StructureStubInfo& info = m_codeBlock->structureStubInfo(i);
        info.callReturnLocation = patchBuffer.locationOf(m_propertyAccessCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = patchBuffer.locationOf(m_propertyAccessCompilationInfo[i].hotPathBegin);
    }
#endif
#if ENABLE(JIT_OPTIMIZE_CALL)
    for (unsigned i = 0; i < m_codeBlock->numberOfCallLinkInfos(); ++i) {
        CallLinkInfo& info = m_codeBlock->callLinkInfo(i);
        info.callReturnLocation = patchBuffer.locationOfNearCall(m_callStructureStubCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = patchBuffer.locationOf(m_callStructureStubCompilationInfo[i].hotPathBegin);
        info.hotPathOther = patchBuffer.locationOfNearCall(m_callStructureStubCompilationInfo[i].hotPathOther);
        info.coldPathOther = patchBuffer.locationOf(m_callStructureStubCompilationInfo[i].coldPathOther);
    }
#endif

    m_codeBlock->setJITCode(codeRef);
}

void JIT::privateCompileCTIMachineTrampolines(RefPtr<ExecutablePool>* executablePool, JSGlobalData* globalData, void** ctiArrayLengthTrampoline, void** ctiStringLengthTrampoline, void** ctiVirtualCallPreLink, void** ctiVirtualCallLink, void** ctiVirtualCall, void** ctiNativeCallThunk)
{
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    // (1) The first function provides fast property access for array length
    Label arrayLengthBegin = align();

    // Check eax is an array
    Jump array_failureCases1 = emitJumpIfNotJSCell(regT0);
    Jump array_failureCases2 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr));

    // Checks out okay! - get the length from the storage
    loadPtr(Address(regT0, FIELD_OFFSET(JSArray, m_storage)), regT0);
    load32(Address(regT0, FIELD_OFFSET(ArrayStorage, m_length)), regT0);

    Jump array_failureCases3 = branch32(Above, regT0, Imm32(JSImmediate::maxImmediateInt));

    // regT0 contains a 64 bit value (is positive, is zero extended) so we don't need sign extend here.
    emitFastArithIntToImmNoCheck(regT0, regT0);

    ret();

    // (2) The second function provides fast property access for string length
    Label stringLengthBegin = align();

    // Check eax is a string
    Jump string_failureCases1 = emitJumpIfNotJSCell(regT0);
    Jump string_failureCases2 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr));

    // Checks out okay! - get the length from the Ustring.
    loadPtr(Address(regT0, FIELD_OFFSET(JSString, m_value) + FIELD_OFFSET(UString, m_rep)), regT0);
    load32(Address(regT0, FIELD_OFFSET(UString::Rep, len)), regT0);

    Jump string_failureCases3 = branch32(Above, regT0, Imm32(JSImmediate::maxImmediateInt));

    // regT0 contains a 64 bit value (is positive, is zero extended) so we don't need sign extend here.
    emitFastArithIntToImmNoCheck(regT0, regT0);
    
    ret();
#endif

#if !(PLATFORM(X86) || PLATFORM(X86_64))
#error "This code is less portable than it looks this code assumes that regT3 is callee preserved, which happens to be true on x86/x86-64."
#endif

    // (3) Trampolines for the slow cases of op_call / op_call_eval / op_construct.
    
    Label virtualCallPreLinkBegin = align();

    // Load the callee CodeBlock* into eax
    loadPtr(Address(regT2, FIELD_OFFSET(JSFunction, m_body)), regT3);
    loadPtr(Address(regT3, FIELD_OFFSET(FunctionBodyNode, m_code)), regT0);
    Jump hasCodeBlock1 = branchTestPtr(NonZero, regT0);
    // If m_code is null and m_jitCode is not, then we have a native function, so arity is irrelevant
    loadPtr(Address(regT3, FIELD_OFFSET(FunctionBodyNode, m_jitCode)), regT0);
    Jump isNativeFunc1 = branchTestPtr(NonZero, regT0);
    pop(regT3);
    restoreArgumentReference();
    Call callJSFunction1 = call();
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    push(regT3);
    hasCodeBlock1.link(this);

    // Check argCount matches callee arity.
    Jump arityCheckOkay1 = branch32(Equal, Address(regT0, FIELD_OFFSET(CodeBlock, m_numParameters)), regT1);
    pop(regT3);
    emitPutJITStubArg(regT3, 2);
    emitPutJITStubArg(regT0, 4);
    restoreArgumentReference();
    Call callArityCheck1 = call();
    move(regT1, callFrameRegister);
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    push(regT3);
    arityCheckOkay1.link(this);
    isNativeFunc1.link(this);
    
    compileOpCallInitializeCallFrame();

    pop(regT3);
    emitPutJITStubArg(regT3, 2);
    restoreArgumentReference();
    Call callDontLazyLinkCall = call();
    emitGetJITStubArg(1, regT2);
    push(regT3);

    jump(regT0);

    Label virtualCallLinkBegin = align();

    // Load the callee CodeBlock* into eax
    loadPtr(Address(regT2, FIELD_OFFSET(JSFunction, m_body)), regT3);
    loadPtr(Address(regT3, FIELD_OFFSET(FunctionBodyNode, m_code)), regT0);
    Jump hasCodeBlock2 = branchTestPtr(NonZero, regT0);
    // If m_code is null and m_jitCode is not, then we have a native function, so arity is irrelevant
    loadPtr(Address(regT3, FIELD_OFFSET(FunctionBodyNode, m_jitCode)), regT0);
    Jump isNativeFunc2 = branchTestPtr(NonZero, regT0);
    pop(regT3);
    restoreArgumentReference();
    Call callJSFunction2 = call();
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    push(regT3);
    hasCodeBlock2.link(this);

    // Check argCount matches callee arity.
    Jump arityCheckOkay2 = branch32(Equal, Address(regT0, FIELD_OFFSET(CodeBlock, m_numParameters)), regT1);
    pop(regT3);
    emitPutJITStubArg(regT3, 2);
    emitPutJITStubArg(regT0, 4);
    restoreArgumentReference();
    Call callArityCheck2 = call();
    move(regT1, callFrameRegister);
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    push(regT3);
    arityCheckOkay2.link(this);
    isNativeFunc2.link(this);

    compileOpCallInitializeCallFrame();

    pop(regT3);
    emitPutJITStubArg(regT3, 2);
    restoreArgumentReference();
    Call callLazyLinkCall = call();
    push(regT3);

    jump(regT0);

    Label virtualCallBegin = align();

    // Load the callee CodeBlock* into eax
    loadPtr(Address(regT2, FIELD_OFFSET(JSFunction, m_body)), regT3);
    loadPtr(Address(regT3, FIELD_OFFSET(FunctionBodyNode, m_code)), regT0);
    Jump hasCodeBlock3 = branchTestPtr(NonZero, regT0);
    // If m_code is null and m_jitCode is not, then we have a native function, so arity is irrelevant
    loadPtr(Address(regT3, FIELD_OFFSET(FunctionBodyNode, m_jitCode)), regT0);
    Jump isNativeFunc3 = branchTestPtr(NonZero, regT0);
    pop(regT3);
    restoreArgumentReference();
    Call callJSFunction3 = call();
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    push(regT3);
    hasCodeBlock3.link(this);

    // Check argCount matches callee arity.
    Jump arityCheckOkay3 = branch32(Equal, Address(regT0, FIELD_OFFSET(CodeBlock, m_numParameters)), regT1);
    pop(regT3);
    emitPutJITStubArg(regT3, 2);
    emitPutJITStubArg(regT0, 4);
    restoreArgumentReference();
    Call callArityCheck3 = call();
    move(regT1, callFrameRegister);
    emitGetJITStubArg(1, regT2);
    emitGetJITStubArg(3, regT1);
    push(regT3);
    arityCheckOkay3.link(this);
    // load ctiCode from the new codeBlock.
    loadPtr(Address(regT0, FIELD_OFFSET(CodeBlock, m_jitCode)), regT0);
    
    isNativeFunc3.link(this);

    compileOpCallInitializeCallFrame();
    jump(regT0);

    
    Label nativeCallThunk = align();
    pop(regT0);
    emitPutToCallFrameHeader(regT0, RegisterFile::ReturnPC); // Push return address

    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT1);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT1);
    emitPutToCallFrameHeader(regT1, RegisterFile::ScopeChain);
    

#if PLATFORM(X86_64)
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, X86::ecx);

    // Allocate stack space for our arglist
    subPtr(Imm32(sizeof(ArgList)), stackPointerRegister);
    COMPILE_ASSERT((sizeof(ArgList) & 0xf) == 0, ArgList_should_by_16byte_aligned);
    
    // Set up arguments
    subPtr(Imm32(1), X86::ecx); // Don't include 'this' in argcount

    // Push argcount
    storePtr(X86::ecx, Address(stackPointerRegister, FIELD_OFFSET(ArgList, m_argCount)));

    // Calculate the start of the callframe header, and store in edx
    addPtr(Imm32(-RegisterFile::CallFrameHeaderSize * (int32_t)sizeof(Register)), callFrameRegister, X86::edx);
    
    // Calculate start of arguments as callframe header - sizeof(Register) * argcount (ecx)
    mul32(Imm32(sizeof(Register)), X86::ecx, X86::ecx);
    subPtr(X86::ecx, X86::edx);

    // push pointer to arguments
    storePtr(X86::edx, Address(stackPointerRegister, FIELD_OFFSET(ArgList, m_args)));
    
    // ArgList is passed by reference so is stackPointerRegister
    move(stackPointerRegister, X86::ecx);
    
    // edx currently points to the first argument, edx-sizeof(Register) points to 'this'
    loadPtr(Address(X86::edx, -(int32_t)sizeof(Register)), X86::edx);
    
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86::esi);

    move(callFrameRegister, X86::edi); 

    call(Address(X86::esi, FIELD_OFFSET(JSFunction, m_data)));
    
    addPtr(Imm32(sizeof(ArgList)), stackPointerRegister);
#else
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT0);

    /* We have two structs that we use to describe the stackframe we set up for our
     * call to native code.  NativeCallFrameStructure describes the how we set up the stack
     * in advance of the call.  NativeFunctionCalleeSignature describes the callframe
     * as the native code expects it.  We do this as we are using the fastcall calling
     * convention which results in the callee popping its arguments off the stack, but
     * not the rest of the callframe so we need a nice way to ensure we increment the
     * stack pointer by the right amount after the call.
     */
#if COMPILER(MSVC) || PLATFORM(LINUX)
    struct NativeCallFrameStructure {
      //  CallFrame* callFrame; // passed in EDX
        JSObject* callee;
        JSValue thisValue;
        ArgList* argPointer;
        ArgList args;
        JSValue result;
    };
    struct NativeFunctionCalleeSignature {
        JSObject* callee;
        JSValue thisValue;
        ArgList* argPointer;
    };
#else
    struct NativeCallFrameStructure {
      //  CallFrame* callFrame; // passed in ECX
      //  JSObject* callee; // passed in EDX
        JSValue thisValue;
        ArgList* argPointer;
        ArgList args;
    };
    struct NativeFunctionCalleeSignature {
        JSValue thisValue;
        ArgList* argPointer;
    };
#endif
    const int NativeCallFrameSize = (sizeof(NativeCallFrameStructure) + 15) & ~15;
    // Allocate system stack frame
    subPtr(Imm32(NativeCallFrameSize), stackPointerRegister);

    // Set up arguments
    subPtr(Imm32(1), regT0); // Don't include 'this' in argcount

    // push argcount
    storePtr(regT0, Address(stackPointerRegister, FIELD_OFFSET(NativeCallFrameStructure, args) + FIELD_OFFSET(ArgList, m_argCount)));
    
    // Calculate the start of the callframe header, and store in regT1
    addPtr(Imm32(-RegisterFile::CallFrameHeaderSize * (int)sizeof(Register)), callFrameRegister, regT1);
    
    // Calculate start of arguments as callframe header - sizeof(Register) * argcount (regT0)
    mul32(Imm32(sizeof(Register)), regT0, regT0);
    subPtr(regT0, regT1);
    storePtr(regT1, Address(stackPointerRegister, FIELD_OFFSET(NativeCallFrameStructure, args) + FIELD_OFFSET(ArgList, m_args)));

    // ArgList is passed by reference so is stackPointerRegister + 4 * sizeof(Register)
    addPtr(Imm32(FIELD_OFFSET(NativeCallFrameStructure, args)), stackPointerRegister, regT0);
    storePtr(regT0, Address(stackPointerRegister, FIELD_OFFSET(NativeCallFrameStructure, argPointer)));

    // regT1 currently points to the first argument, regT1 - sizeof(Register) points to 'this'
    loadPtr(Address(regT1, -(int)sizeof(Register)), regT1);
    storePtr(regT1, Address(stackPointerRegister, FIELD_OFFSET(NativeCallFrameStructure, thisValue)));

#if COMPILER(MSVC) || PLATFORM(LINUX)
    // ArgList is passed by reference so is stackPointerRegister + 4 * sizeof(Register)
    addPtr(Imm32(FIELD_OFFSET(NativeCallFrameStructure, result)), stackPointerRegister, X86::ecx);

    // Plant callee
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86::eax);
    storePtr(X86::eax, Address(stackPointerRegister, FIELD_OFFSET(NativeCallFrameStructure, callee)));

    // Plant callframe
    move(callFrameRegister, X86::edx);

    call(Address(X86::eax, FIELD_OFFSET(JSFunction, m_data)));

    // JSValue is a non-POD type
    loadPtr(Address(X86::eax), X86::eax);
#else
    // Plant callee
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86::edx);

    // Plant callframe
    move(callFrameRegister, X86::ecx);
    call(Address(X86::edx, FIELD_OFFSET(JSFunction, m_data)));
#endif

    // We've put a few temporaries on the stack in addition to the actual arguments
    // so pull them off now
    addPtr(Imm32(NativeCallFrameSize - sizeof(NativeFunctionCalleeSignature)), stackPointerRegister);

#endif

    // Check for an exception
    loadPtr(&(globalData->exception), regT2);
    Jump exceptionHandler = branchTestPtr(NonZero, regT2);

    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT1);
    
    // Restore our caller's "r".
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    
    // Return.
    push(regT1);
    ret();

    // Handle an exception
    exceptionHandler.link(this);
    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT1);
    move(ImmPtr(&globalData->exceptionLocation), regT2);
    storePtr(regT1, regT2);
    move(ImmPtr(reinterpret_cast<void*>(ctiVMThrowTrampoline)), regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    poke(callFrameRegister, offsetof(struct JITStackFrame, callFrame) / sizeof (void*));
    push(regT2);
    ret();
    

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    Call array_failureCases1Call = makeTailRecursiveCall(array_failureCases1);
    Call array_failureCases2Call = makeTailRecursiveCall(array_failureCases2);
    Call array_failureCases3Call = makeTailRecursiveCall(array_failureCases3);
    Call string_failureCases1Call = makeTailRecursiveCall(string_failureCases1);
    Call string_failureCases2Call = makeTailRecursiveCall(string_failureCases2);
    Call string_failureCases3Call = makeTailRecursiveCall(string_failureCases3);
#endif

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.
    *executablePool = m_globalData->executableAllocator.poolForSize(m_assembler.size());
    void* code = m_assembler.executableCopy((*executablePool).get());

    PatchBuffer patchBuffer(code);
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    patchBuffer.link(array_failureCases1Call, JITStubs::cti_op_get_by_id_array_fail);
    patchBuffer.link(array_failureCases2Call, JITStubs::cti_op_get_by_id_array_fail);
    patchBuffer.link(array_failureCases3Call, JITStubs::cti_op_get_by_id_array_fail);
    patchBuffer.link(string_failureCases1Call, JITStubs::cti_op_get_by_id_string_fail);
    patchBuffer.link(string_failureCases2Call, JITStubs::cti_op_get_by_id_string_fail);
    patchBuffer.link(string_failureCases3Call, JITStubs::cti_op_get_by_id_string_fail);

    *ctiArrayLengthTrampoline = patchBuffer.trampolineAt(arrayLengthBegin);
    *ctiStringLengthTrampoline = patchBuffer.trampolineAt(stringLengthBegin);
#else
    UNUSED_PARAM(ctiArrayLengthTrampoline);
    UNUSED_PARAM(ctiStringLengthTrampoline);
#endif
    patchBuffer.link(callArityCheck1, JITStubs::cti_op_call_arityCheck);
    patchBuffer.link(callArityCheck2, JITStubs::cti_op_call_arityCheck);
    patchBuffer.link(callArityCheck3, JITStubs::cti_op_call_arityCheck);
    patchBuffer.link(callJSFunction1, JITStubs::cti_op_call_JSFunction);
    patchBuffer.link(callJSFunction2, JITStubs::cti_op_call_JSFunction);
    patchBuffer.link(callJSFunction3, JITStubs::cti_op_call_JSFunction);
    patchBuffer.link(callDontLazyLinkCall, JITStubs::cti_vm_dontLazyLinkCall);
    patchBuffer.link(callLazyLinkCall, JITStubs::cti_vm_lazyLinkCall);

    *ctiVirtualCallPreLink = patchBuffer.trampolineAt(virtualCallPreLinkBegin);
    *ctiVirtualCallLink = patchBuffer.trampolineAt(virtualCallLinkBegin);
    *ctiVirtualCall = patchBuffer.trampolineAt(virtualCallBegin);
    *ctiNativeCallThunk = patchBuffer.trampolineAt(nativeCallThunk);
}

void JIT::emitGetVariableObjectRegister(RegisterID variableObject, int index, RegisterID dst)
{
    loadPtr(Address(variableObject, FIELD_OFFSET(JSVariableObject, d)), dst);
    loadPtr(Address(dst, FIELD_OFFSET(JSVariableObject::JSVariableObjectData, registers)), dst);
    loadPtr(Address(dst, index * sizeof(Register)), dst);
}

void JIT::emitPutVariableObjectRegister(RegisterID src, RegisterID variableObject, int index)
{
    loadPtr(Address(variableObject, FIELD_OFFSET(JSVariableObject, d)), variableObject);
    loadPtr(Address(variableObject, FIELD_OFFSET(JSVariableObject::JSVariableObjectData, registers)), variableObject);
    storePtr(src, Address(variableObject, index * sizeof(Register)));
}

} // namespace JSC

#endif // ENABLE(JIT)
