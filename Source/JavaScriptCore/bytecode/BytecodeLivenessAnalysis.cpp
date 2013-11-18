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

#include "config.h"
#include "BytecodeLivenessAnalysis.h"

#include "BytecodeLivenessAnalysisInlines.h"
#include "CodeBlock.h"
#include "FullBytecodeLiveness.h"
#include "PreciseJumpTargets.h"

namespace JSC {

BytecodeLivenessAnalysis::BytecodeLivenessAnalysis(CodeBlock* codeBlock)
    : m_codeBlock(codeBlock)
{
    ASSERT(m_codeBlock);
    compute();
}

static bool isValidRegisterForLiveness(CodeBlock* codeBlock, int operand)
{
    VirtualRegister virtualReg(operand);
    return !codeBlock->isConstantRegisterIndex(operand) // Don't care about constants.
        && virtualReg.isLocal() // Don't care about arguments.
        && (!codeBlock->captureCount() // If we have no captured variables, we're good to go.
            || (virtualReg.offset() > codeBlock->captureStart() || (virtualReg.offset() <= codeBlock->captureEnd())));
}

static void setForOperand(CodeBlock* codeBlock, FastBitVector& bits, int operand)
{
    ASSERT(isValidRegisterForLiveness(codeBlock, operand));
    VirtualRegister virtualReg(operand);
    if (virtualReg.offset() > codeBlock->captureStart())
        bits.set(virtualReg.toLocal());
    else
        bits.set(virtualReg.toLocal() - codeBlock->captureCount());
}

static void computeUsesForBytecodeOffset(CodeBlock* codeBlock, unsigned bytecodeOffset, FastBitVector& uses)
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
        return;
    // First argument.
    case op_new_func:
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
        if (isValidRegisterForLiveness(codeBlock, instruction[1].u.operand))
            setForOperand(codeBlock, uses, instruction[1].u.operand);
        return;
    }
    // First and second arguments.
    case op_del_by_id:
    case op_ret_object_or_this:
    case op_jlesseq:
    case op_jgreater:
    case op_jgreatereq:
    case op_jnless:
    case op_jnlesseq:
    case op_jngreater:
    case op_jngreatereq:
    case op_jless: {
        if (isValidRegisterForLiveness(codeBlock, instruction[1].u.operand))
            setForOperand(codeBlock, uses, instruction[1].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        return;
    }
    // First, second, and third arguments.
    case op_del_by_val:
    case op_put_by_val_direct:
    case op_put_by_val: {
        if (isValidRegisterForLiveness(codeBlock, instruction[1].u.operand))
            setForOperand(codeBlock, uses, instruction[1].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[3].u.operand))
            setForOperand(codeBlock, uses, instruction[3].u.operand);
        return;
    }
    // First and third arguments.
    case op_put_by_index:
    case op_put_by_id_replace:
    case op_put_by_id_transition:
    case op_put_by_id_transition_direct:
    case op_put_by_id_transition_direct_out_of_line:
    case op_put_by_id_transition_normal:
    case op_put_by_id_transition_normal_out_of_line:
    case op_put_by_id_generic:
    case op_put_by_id_out_of_line:
    case op_put_by_id:
    case op_put_to_scope: {
        if (isValidRegisterForLiveness(codeBlock, instruction[1].u.operand))
            setForOperand(codeBlock, uses, instruction[1].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[3].u.operand))
            setForOperand(codeBlock, uses, instruction[3].u.operand);
        return;
    }
    // First, third, and fourth arguments.
    case op_put_getter_setter: {
        if (isValidRegisterForLiveness(codeBlock, instruction[1].u.operand))
            setForOperand(codeBlock, uses, instruction[1].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[3].u.operand))
            setForOperand(codeBlock, uses, instruction[3].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[4].u.operand))
            setForOperand(codeBlock, uses, instruction[4].u.operand);
        return;
    }
    // Second argument.
    case op_init_global_const_nop:
    case op_init_global_const:
    case op_push_name_scope:
    case op_get_from_scope:
    case op_to_primitive:
    case op_get_by_id:
    case op_get_by_id_out_of_line:
    case op_get_by_id_self:
    case op_get_by_id_proto:
    case op_get_by_id_chain:
    case op_get_by_id_getter_self:
    case op_get_by_id_getter_proto:
    case op_get_by_id_getter_chain:
    case op_get_by_id_custom_self:
    case op_get_by_id_custom_proto:
    case op_get_by_id_custom_chain:
    case op_get_by_id_generic:
    case op_get_array_length:
    case op_get_string_length:
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
    case op_new_array_with_size:
    case op_create_this: {
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        return;
    }
    // Second and third arguments.
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
    case op_eq: {
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[3].u.operand))
            setForOperand(codeBlock, uses, instruction[3].u.operand);
        return;
    }
    // Second, third, and fourth arguments.
    case op_call_varargs:
    case op_get_pnames: {
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[3].u.operand))
            setForOperand(codeBlock, uses, instruction[3].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[4].u.operand))
            setForOperand(codeBlock, uses, instruction[4].u.operand);
        return;
    }
    // Second, third, fourth, and fifth arguments.
    case op_next_pname: {
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[3].u.operand))
            setForOperand(codeBlock, uses, instruction[3].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[4].u.operand))
            setForOperand(codeBlock, uses, instruction[4].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[5].u.operand))
            setForOperand(codeBlock, uses, instruction[5].u.operand);
        return;
    }
    // Second, third, fourth, fifth, and sixth arguments. 
    case op_get_by_pname: {
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[3].u.operand))
            setForOperand(codeBlock, uses, instruction[3].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[4].u.operand))
            setForOperand(codeBlock, uses, instruction[4].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[5].u.operand))
            setForOperand(codeBlock, uses, instruction[5].u.operand);
        if (isValidRegisterForLiveness(codeBlock, instruction[6].u.operand))
            setForOperand(codeBlock, uses, instruction[6].u.operand);
        return;
    }
    // Third argument.
    case op_switch_string:
    case op_switch_char:
    case op_switch_imm: {
        if (isValidRegisterForLiveness(codeBlock, instruction[3].u.operand))
            setForOperand(codeBlock, uses, instruction[3].u.operand);
        return;
    }
    // Variable number of arguments.
    case op_new_array:
    case op_strcat: {
        int base = instruction[2].u.operand;
        int count = instruction[3].u.operand;
        for (int i = 0; i < count; i++) {
            if (isValidRegisterForLiveness(codeBlock, base - i))
                setForOperand(codeBlock, uses, base - i);
        }
        return;
    }
    // Crazy stuff for calls.
    case op_construct:
    case op_call_eval:
    case op_call: {
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        int argCount = instruction[3].u.operand;
        int registerOffset = -instruction[4].u.operand;
        int lastArg = registerOffset + CallFrame::thisArgumentOffset();
        for (int i = opcodeID == op_construct ? 1 : 0; i < argCount; i++) {
            if (isValidRegisterForLiveness(codeBlock, lastArg + i))
                setForOperand(codeBlock, uses, lastArg + i);
        }
        return;
    }
    // Special stuff for tear off arguments.
    case op_tear_off_arguments: {
        if (isValidRegisterForLiveness(codeBlock, instruction[1].u.operand))
            setForOperand(codeBlock, uses, instruction[1].u.operand);
        if (isValidRegisterForLiveness(codeBlock, unmodifiedArgumentsRegister(VirtualRegister(instruction[1].u.operand)).offset()))
            setForOperand(codeBlock, uses, unmodifiedArgumentsRegister(VirtualRegister(instruction[1].u.operand)).offset());
        if (isValidRegisterForLiveness(codeBlock, instruction[2].u.operand))
            setForOperand(codeBlock, uses, instruction[2].u.operand);
        return;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

static void computeDefsForBytecodeOffset(CodeBlock* codeBlock, unsigned bytecodeOffset, FastBitVector& defs)
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
    case op_put_by_id_replace:
    case op_put_by_id_transition:
    case op_put_by_id_transition_direct:
    case op_put_by_id_transition_direct_out_of_line:
    case op_put_by_id_transition_normal:
    case op_put_by_id_transition_normal_out_of_line:
    case op_put_by_id_generic:
    case op_put_getter_setter:
    case op_put_by_val:
    case op_put_by_val_direct:
    case op_put_by_index:
    case op_del_by_id:
    case op_del_by_val:
#define LLINT_HELPER_OPCODES(opcode, length) case opcode:
        FOR_EACH_LLINT_OPCODE_EXTENSION(LLINT_HELPER_OPCODES);
#undef LLINT_HELPER_OPCODES
        return;
    // These all have a single destination for the first argument.
    case op_next_pname:
    case op_get_pnames:
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
    case op_new_func_exp:
    case op_call_varargs:
    case op_get_from_scope:
    case op_call:
    case op_call_eval:
    case op_construct:
    case op_get_by_id:
    case op_get_by_id_out_of_line:
    case op_get_by_id_self:
    case op_get_by_id_proto:
    case op_get_by_id_chain:
    case op_get_by_id_getter_self:
    case op_get_by_id_getter_proto:
    case op_get_by_id_getter_chain:
    case op_get_by_id_custom_self:
    case op_get_by_id_custom_proto:
    case op_get_by_id_custom_chain:
    case op_get_by_id_generic:
    case op_get_array_length:
    case op_get_string_length:
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
    case op_new_object:
    case op_to_this:
    case op_get_callee:
    case op_init_lazy_reg:
    case op_create_activation:
    case op_create_arguments: {
        if (isValidRegisterForLiveness(codeBlock, instruction[1].u.operand))
            setForOperand(codeBlock, defs, instruction[1].u.operand);
        return;
    }
    case op_tear_off_arguments: {
        if (isValidRegisterForLiveness(codeBlock, instruction[1].u.operand - 1))
            setForOperand(codeBlock, defs, instruction[1].u.operand - 1);
        return;
    }
    case op_enter: {
        defs.setAll();
        return;
    } }
}

static unsigned getLeaderOffsetForBasicBlock(RefPtr<BytecodeBasicBlock>* basicBlock)
{
    return (*basicBlock)->leaderBytecodeOffset();
}

static BytecodeBasicBlock* findBasicBlockWithLeaderOffset(Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks, unsigned leaderOffset)
{
    return (*tryBinarySearch<RefPtr<BytecodeBasicBlock>, unsigned>(basicBlocks, basicBlocks.size(), leaderOffset, getLeaderOffsetForBasicBlock)).get();
}

static bool blockContainsBytecodeOffset(BytecodeBasicBlock* block, unsigned bytecodeOffset)
{
    unsigned leaderOffset = block->leaderBytecodeOffset();
    return bytecodeOffset >= leaderOffset && bytecodeOffset < leaderOffset + block->totalBytecodeLength();
}

static BytecodeBasicBlock* findBasicBlockForBytecodeOffset(Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks, unsigned bytecodeOffset)
{
/*
    for (unsigned i = 0; i < basicBlocks.size(); i++) {
        if (blockContainsBytecodeOffset(basicBlocks[i].get(), bytecodeOffset))
            return basicBlocks[i].get();
    }
    return 0;
*/
    RefPtr<BytecodeBasicBlock>* basicBlock = approximateBinarySearch<RefPtr<BytecodeBasicBlock>, unsigned>(
        basicBlocks, basicBlocks.size(), bytecodeOffset, getLeaderOffsetForBasicBlock);
    // We found the block we were looking for.
    if (blockContainsBytecodeOffset((*basicBlock).get(), bytecodeOffset))
        return (*basicBlock).get();

    // Basic block is to the left of the returned block.
    if (bytecodeOffset < (*basicBlock)->leaderBytecodeOffset()) {
        ASSERT(basicBlock - 1 >= basicBlocks.data());
        ASSERT(blockContainsBytecodeOffset(basicBlock[-1].get(), bytecodeOffset));
        return basicBlock[-1].get();
    }

    // Basic block is to the right of the returned block.
    ASSERT(&basicBlock[1] <= &basicBlocks.last());
    ASSERT(blockContainsBytecodeOffset(basicBlock[1].get(), bytecodeOffset));
    return basicBlock[1].get();
}

static void stepOverInstruction(CodeBlock* codeBlock, Vector<RefPtr<BytecodeBasicBlock>>& basicBlocks, unsigned bytecodeOffset, FastBitVector& uses, FastBitVector& defs, FastBitVector& out)
{
    uses.clearAll();
    defs.clearAll();
    
    computeUsesForBytecodeOffset(codeBlock, bytecodeOffset, uses);
    computeDefsForBytecodeOffset(codeBlock, bytecodeOffset, defs);
    
    out.exclude(defs);
    out.merge(uses);
    
    // If we have an exception handler, we want the live-in variables of the 
    // exception handler block to be included in the live-in of this particular bytecode.
    if (HandlerInfo* handler = codeBlock->handlerForBytecodeOffset(bytecodeOffset)) {
        BytecodeBasicBlock* handlerBlock = findBasicBlockWithLeaderOffset(basicBlocks, handler->target);
        ASSERT(handlerBlock);
        out.merge(handlerBlock->in());
    }
}

static void computeLocalLivenessForBytecodeOffset(CodeBlock* codeBlock, BytecodeBasicBlock* block, Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks, unsigned targetOffset, FastBitVector& result)
{
    ASSERT(!block->isExitBlock());
    ASSERT(!block->isEntryBlock());

    FastBitVector out = block->out();

    FastBitVector uses;
    FastBitVector defs;
    uses.resize(out.numBits());
    defs.resize(out.numBits());

    for (int i = block->bytecodeOffsets().size() - 1; i >= 0; i--) {
        unsigned bytecodeOffset = block->bytecodeOffsets()[i];
        if (targetOffset > bytecodeOffset)
            break;
        
        stepOverInstruction(codeBlock, basicBlocks, bytecodeOffset, uses, defs, out);
    }

    result.set(out);
}

static void computeLocalLivenessForBlock(CodeBlock* codeBlock, BytecodeBasicBlock* block, Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks)
{
    if (block->isExitBlock() || block->isEntryBlock())
        return;
    computeLocalLivenessForBytecodeOffset(codeBlock, block, basicBlocks, block->leaderBytecodeOffset(), block->in());
}

void BytecodeLivenessAnalysis::runLivenessFixpoint()
{
    UnlinkedCodeBlock* unlinkedCodeBlock = m_codeBlock->unlinkedCodeBlock();
    unsigned numberOfVariables = unlinkedCodeBlock->m_numVars + 
        unlinkedCodeBlock->m_numCalleeRegisters - m_codeBlock->captureCount();

    for (unsigned i = 0; i < m_basicBlocks.size(); i++) {
        BytecodeBasicBlock* block = m_basicBlocks[i].get();
        block->in().resize(numberOfVariables);
        block->out().resize(numberOfVariables);
    }

    bool changed;
    m_basicBlocks.last()->in().clearAll();
    m_basicBlocks.last()->out().clearAll();
    FastBitVector newOut;
    newOut.resize(m_basicBlocks.last()->out().numBits());
    do {
        changed = false;
        for (int i = m_basicBlocks.size() - 2; i >= 0; i--) {
            BytecodeBasicBlock* block = m_basicBlocks[i].get();
            newOut.clearAll();
            for (unsigned j = 0; j < block->successors().size(); j++)
                newOut.merge(block->successors()[j]->in());
            bool outDidChange = block->out().setAndCheck(newOut);
            computeLocalLivenessForBlock(m_codeBlock, block, m_basicBlocks);
            changed |= outDidChange;
        }
    } while (changed);
}

void BytecodeLivenessAnalysis::getLivenessInfoForNonCapturedVarsAtBytecodeOffset(unsigned bytecodeOffset, FastBitVector& result)
{
    BytecodeBasicBlock* block = findBasicBlockForBytecodeOffset(m_basicBlocks, bytecodeOffset);
    ASSERT(block);
    ASSERT(!block->isEntryBlock());
    ASSERT(!block->isExitBlock());
    result.resize(block->out().numBits());
    computeLocalLivenessForBytecodeOffset(m_codeBlock, block, m_basicBlocks, bytecodeOffset, result);
}

bool BytecodeLivenessAnalysis::operandIsLiveAtBytecodeOffset(int operand, unsigned bytecodeOffset)
{
    if (operandIsAlwaysLive(m_codeBlock, operand))
        return true;
    FastBitVector result;
    getLivenessInfoForNonCapturedVarsAtBytecodeOffset(bytecodeOffset, result);
    return operandThatIsNotAlwaysLiveIsLive(m_codeBlock, result, operand);
}

FastBitVector getLivenessInfo(CodeBlock* codeBlock, const FastBitVector& out)
{
    FastBitVector result;

    unsigned numCapturedVars = codeBlock->captureCount();
    if (numCapturedVars) {
        int firstCapturedLocal = VirtualRegister(codeBlock->captureStart()).toLocal();
        result.resize(out.numBits() + numCapturedVars);
        for (unsigned i = 0; i < numCapturedVars; ++i)
            result.set(firstCapturedLocal + i);
    } else
        result.resize(out.numBits());

    int outLength = out.numBits();
    ASSERT(outLength >= 0);
    for (int i = 0; i < outLength; i++) {
        if (!out.get(i))
            continue;

        if (!numCapturedVars) {
            result.set(i);
            continue;
        }

        if (virtualRegisterForLocal(i).offset() > codeBlock->captureStart())
            result.set(i);
        else 
            result.set(numCapturedVars + i);
    }
    return result;
}

FastBitVector BytecodeLivenessAnalysis::getLivenessInfoAtBytecodeOffset(unsigned bytecodeOffset)
{
    FastBitVector out;
    getLivenessInfoForNonCapturedVarsAtBytecodeOffset(bytecodeOffset, out);
    return getLivenessInfo(m_codeBlock, out);
}

void BytecodeLivenessAnalysis::computeFullLiveness(FullBytecodeLiveness& result)
{
    FastBitVector out;
    FastBitVector uses;
    FastBitVector defs;
    
    result.m_codeBlock = m_codeBlock;
    result.m_map.clear();
    
    for (unsigned i = m_basicBlocks.size(); i--;) {
        BytecodeBasicBlock* block = m_basicBlocks[i].get();
        if (block->isEntryBlock() || block->isExitBlock())
            continue;
        
        out = block->out();
        uses.resize(out.numBits());
        defs.resize(out.numBits());
        
        for (unsigned i = block->bytecodeOffsets().size(); i--;) {
            unsigned bytecodeOffset = block->bytecodeOffsets()[i];
            stepOverInstruction(m_codeBlock, m_basicBlocks, bytecodeOffset, uses, defs, out);
            result.m_map.add(bytecodeOffset, out);
        }
    }
}

void BytecodeLivenessAnalysis::dumpResults()
{
    Interpreter* interpreter = m_codeBlock->vm()->interpreter;
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    for (unsigned i = 0; i < m_basicBlocks.size(); i++) {
        BytecodeBasicBlock* block = m_basicBlocks[i].get();
        dataLogF("\nBytecode basic block %u: %p (offset: %u, length: %u)\n", i, block, block->leaderBytecodeOffset(), block->totalBytecodeLength());
        dataLogF("Predecessors: ");
        for (unsigned j = 0; j < block->predecessors().size(); j++) {
            BytecodeBasicBlock* predecessor = block->predecessors()[j];
            dataLogF("%p ", predecessor);
        }
        dataLogF("\n");
        dataLogF("Successors: ");
        for (unsigned j = 0; j < block->successors().size(); j++) {
            BytecodeBasicBlock* successor = block->successors()[j];
            dataLogF("%p ", successor);
        }
        dataLogF("\n");
        if (block->isEntryBlock()) {
            dataLogF("Entry block %p\n", block);
            continue;
        }
        if (block->isExitBlock()) {
            dataLogF("Exit block: %p\n", block);
            continue;
        }
        for (unsigned bytecodeOffset = block->leaderBytecodeOffset(); bytecodeOffset < block->leaderBytecodeOffset() + block->totalBytecodeLength();) {
            const Instruction* currentInstruction = &instructionsBegin[bytecodeOffset];

            dataLogF("Live variables: ");
            FastBitVector liveBefore = getLivenessInfoAtBytecodeOffset(bytecodeOffset);
            for (unsigned j = 0; j < liveBefore.numBits(); j++) {
                if (liveBefore.get(j))
                    dataLogF("%u ", j);
            }
            dataLogF("\n");
            m_codeBlock->dumpBytecode(WTF::dataFile(), m_codeBlock->globalObject()->globalExec(), instructionsBegin, currentInstruction);

            OpcodeID opcodeID = interpreter->getOpcodeID(instructionsBegin[bytecodeOffset].u.opcode);
            unsigned opcodeLength = opcodeLengths[opcodeID];
            bytecodeOffset += opcodeLength;
        }

        dataLogF("Live variables: ");
        FastBitVector liveAfter = block->out();
        for (unsigned j = 0; j < liveAfter.numBits(); j++) {
            if (liveAfter.get(j))
                dataLogF("%u ", j);
        }
        dataLogF("\n");
    }
}

void BytecodeLivenessAnalysis::compute()
{
    computeBytecodeBasicBlocks(m_codeBlock, m_basicBlocks);
    ASSERT(m_basicBlocks.size());
    runLivenessFixpoint();

    if (Options::dumpBytecodeLivenessResults())
        dumpResults();
}

} // namespace JSC
