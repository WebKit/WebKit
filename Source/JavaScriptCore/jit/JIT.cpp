/*
 * Copyright (C) 2008, 2009, 2012, 2013 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)
#include "JIT.h"

// This probably does not belong here; adding here for now as a quick Windows build fix.
#if ENABLE(ASSEMBLER) && CPU(X86) && !OS(MAC_OS_X)
#include "MacroAssembler.h"
JSC::MacroAssemblerX86Common::SSE2CheckState JSC::MacroAssemblerX86Common::s_sse2CheckState = NotCheckedSSE2;
#endif

#include "ArityCheckFailReturnThunks.h"
#include "CodeBlock.h"
#include "DFGCapabilities.h"
#include "Interpreter.h"
#include "JITInlines.h"
#include "JITOperations.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "Operations.h"
#include "RepatchBuffer.h"
#include "ResultType.h"
#include "SamplingTool.h"
#include "SlowPathCall.h"
#include "StackAlignment.h"
#include <wtf/CryptographicallyRandomNumber.h>

using namespace std;

namespace JSC {

void ctiPatchNearCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, MacroAssemblerCodePtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchBuffer.relinkNearCallerToTrampoline(returnAddress, newCalleeFunction);
}

void ctiPatchCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, MacroAssemblerCodePtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchBuffer.relinkCallerToTrampoline(returnAddress, newCalleeFunction);
}

void ctiPatchCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, FunctionPtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchBuffer.relinkCallerToFunction(returnAddress, newCalleeFunction);
}

JIT::JIT(VM* vm, CodeBlock* codeBlock)
    : JSInterfaceJIT(vm, codeBlock)
    , m_interpreter(vm->interpreter)
    , m_labels(codeBlock ? codeBlock->numberOfInstructions() : 0)
    , m_bytecodeOffset((unsigned)-1)
    , m_getByIdIndex(UINT_MAX)
    , m_putByIdIndex(UINT_MAX)
    , m_byValInstructionIndex(UINT_MAX)
    , m_callLinkInfoIndex(UINT_MAX)
    , m_randomGenerator(cryptographicallyRandomNumber())
    , m_canBeOptimized(false)
    , m_shouldEmitProfiling(false)
{
}

#if ENABLE(DFG_JIT)
void JIT::emitEnterOptimizationCheck()
{
    if (!canBeOptimized())
        return;

    JumpList skipOptimize;
    
    skipOptimize.append(branchAdd32(Signed, TrustedImm32(Options::executionCounterIncrementForEntry()), AbsoluteAddress(m_codeBlock->addressOfJITExecuteCounter())));
    ASSERT(!m_bytecodeOffset);
    callOperation(operationOptimize, m_bytecodeOffset);
    skipOptimize.append(branchTestPtr(Zero, returnValueGPR));
    move(returnValueGPR2, stackPointerRegister);
    jump(returnValueGPR);
    skipOptimize.link(this);
}
#endif

#define NEXT_OPCODE(name) \
    m_bytecodeOffset += OPCODE_LENGTH(name); \
    break;

#define DEFINE_SLOW_OP(name) \
    case op_##name: { \
        JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_##name); \
        slowPathCall.call(); \
        NEXT_OPCODE(op_##name); \
    }

#define DEFINE_OP(name) \
    case name: { \
        emit_##name(currentInstruction); \
        NEXT_OPCODE(name); \
    }

#define DEFINE_SLOWCASE_OP(name) \
    case name: { \
        emitSlow_##name(currentInstruction, iter); \
        NEXT_OPCODE(name); \
    }

void JIT::privateCompileMainPass()
{
    jitAssertTagsInPlace();
    jitAssertArgumentCountSane();
    
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    unsigned instructionCount = m_codeBlock->instructions().size();

    m_callLinkInfoIndex = 0;

    for (m_bytecodeOffset = 0; m_bytecodeOffset < instructionCount; ) {
        if (m_disassembler)
            m_disassembler->setForBytecodeMainPath(m_bytecodeOffset, label());
        Instruction* currentInstruction = instructionsBegin + m_bytecodeOffset;
        ASSERT_WITH_MESSAGE(m_interpreter->isOpcode(currentInstruction->u.opcode), "privateCompileMainPass gone bad @ %d", m_bytecodeOffset);

#if ENABLE(OPCODE_SAMPLING)
        if (m_bytecodeOffset > 0) // Avoid the overhead of sampling op_enter twice.
            sampleInstruction(currentInstruction);
#endif

        m_labels[m_bytecodeOffset] = label();

#if ENABLE(JIT_VERBOSE)
        dataLogF("Old JIT emitting code for bc#%u at offset 0x%lx.\n", m_bytecodeOffset, (long)debugOffset());
#endif
        
        OpcodeID opcodeID = m_interpreter->getOpcodeID(currentInstruction->u.opcode);

        if (m_compilation) {
            add64(
                TrustedImm32(1),
                AbsoluteAddress(m_compilation->executionCounterFor(Profiler::OriginStack(Profiler::Origin(
                    m_compilation->bytecodes(), m_bytecodeOffset)))->address()));
        }

        switch (opcodeID) {
        DEFINE_SLOW_OP(del_by_val)
        DEFINE_SLOW_OP(in)
        DEFINE_SLOW_OP(less)
        DEFINE_SLOW_OP(lesseq)
        DEFINE_SLOW_OP(greater)
        DEFINE_SLOW_OP(greatereq)
        DEFINE_SLOW_OP(is_function)
        DEFINE_SLOW_OP(is_object)
        DEFINE_SLOW_OP(typeof)

        DEFINE_OP(op_touch_entry)
        DEFINE_OP(op_add)
        DEFINE_OP(op_bitand)
        DEFINE_OP(op_bitor)
        DEFINE_OP(op_bitxor)
        DEFINE_OP(op_call)
        DEFINE_OP(op_call_eval)
        DEFINE_OP(op_call_varargs)
        DEFINE_OP(op_catch)
        DEFINE_OP(op_construct)
        DEFINE_OP(op_get_callee)
        DEFINE_OP(op_create_this)
        DEFINE_OP(op_to_this)
        DEFINE_OP(op_init_lazy_reg)
        DEFINE_OP(op_create_arguments)
        DEFINE_OP(op_debug)
        DEFINE_OP(op_del_by_id)
        DEFINE_OP(op_div)
        DEFINE_OP(op_end)
        DEFINE_OP(op_enter)
        DEFINE_OP(op_create_activation)
        DEFINE_OP(op_eq)
        DEFINE_OP(op_eq_null)
        case op_get_by_id_out_of_line:
        case op_get_array_length:
        DEFINE_OP(op_get_by_id)
        DEFINE_OP(op_get_arguments_length)
        DEFINE_OP(op_get_by_val)
        DEFINE_OP(op_get_argument_by_val)
        DEFINE_OP(op_get_by_pname)
        DEFINE_OP(op_get_pnames)
        DEFINE_OP(op_check_has_instance)
        DEFINE_OP(op_instanceof)
        DEFINE_OP(op_is_undefined)
        DEFINE_OP(op_is_boolean)
        DEFINE_OP(op_is_number)
        DEFINE_OP(op_is_string)
        DEFINE_OP(op_jeq_null)
        DEFINE_OP(op_jfalse)
        DEFINE_OP(op_jmp)
        DEFINE_OP(op_jneq_null)
        DEFINE_OP(op_jneq_ptr)
        DEFINE_OP(op_jless)
        DEFINE_OP(op_jlesseq)
        DEFINE_OP(op_jgreater)
        DEFINE_OP(op_jgreatereq)
        DEFINE_OP(op_jnless)
        DEFINE_OP(op_jnlesseq)
        DEFINE_OP(op_jngreater)
        DEFINE_OP(op_jngreatereq)
        DEFINE_OP(op_jtrue)
        DEFINE_OP(op_loop_hint)
        DEFINE_OP(op_lshift)
        DEFINE_OP(op_mod)
        DEFINE_OP(op_captured_mov)
        DEFINE_OP(op_mov)
        DEFINE_OP(op_mul)
        DEFINE_OP(op_negate)
        DEFINE_OP(op_neq)
        DEFINE_OP(op_neq_null)
        DEFINE_OP(op_new_array)
        DEFINE_OP(op_new_array_with_size)
        DEFINE_OP(op_new_array_buffer)
        DEFINE_OP(op_new_func)
        DEFINE_OP(op_new_captured_func)
        DEFINE_OP(op_new_func_exp)
        DEFINE_OP(op_new_object)
        DEFINE_OP(op_new_regexp)
        DEFINE_OP(op_next_pname)
        DEFINE_OP(op_not)
        DEFINE_OP(op_nstricteq)
        DEFINE_OP(op_pop_scope)
        DEFINE_OP(op_dec)
        DEFINE_OP(op_inc)
        DEFINE_OP(op_profile_did_call)
        DEFINE_OP(op_profile_will_call)
        DEFINE_OP(op_push_name_scope)
        DEFINE_OP(op_push_with_scope)
        case op_put_by_id_out_of_line:
        case op_put_by_id_transition_direct:
        case op_put_by_id_transition_normal:
        case op_put_by_id_transition_direct_out_of_line:
        case op_put_by_id_transition_normal_out_of_line:
        DEFINE_OP(op_put_by_id)
        DEFINE_OP(op_put_by_index)
        case op_put_by_val_direct:
        DEFINE_OP(op_put_by_val)
        DEFINE_OP(op_put_getter_setter)
        case op_init_global_const_nop:
            NEXT_OPCODE(op_init_global_const_nop);
        DEFINE_OP(op_init_global_const)

        DEFINE_OP(op_ret)
        DEFINE_OP(op_ret_object_or_this)
        DEFINE_OP(op_rshift)
        DEFINE_OP(op_unsigned)
        DEFINE_OP(op_urshift)
        DEFINE_OP(op_strcat)
        DEFINE_OP(op_stricteq)
        DEFINE_OP(op_sub)
        DEFINE_OP(op_switch_char)
        DEFINE_OP(op_switch_imm)
        DEFINE_OP(op_switch_string)
        DEFINE_OP(op_tear_off_activation)
        DEFINE_OP(op_tear_off_arguments)
        DEFINE_OP(op_throw)
        DEFINE_OP(op_throw_static_error)
        DEFINE_OP(op_to_number)
        DEFINE_OP(op_to_primitive)

        DEFINE_OP(op_resolve_scope)
        DEFINE_OP(op_get_from_scope)
        DEFINE_OP(op_put_to_scope)

        case op_get_by_id_chain:
        case op_get_by_id_generic:
        case op_get_by_id_proto:
        case op_get_by_id_self:
        case op_get_by_id_getter_chain:
        case op_get_by_id_getter_proto:
        case op_get_by_id_getter_self:
        case op_get_by_id_custom_chain:
        case op_get_by_id_custom_proto:
        case op_get_by_id_custom_self:
        case op_get_string_length:
        case op_put_by_id_generic:
        case op_put_by_id_replace:
        case op_put_by_id_transition:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    RELEASE_ASSERT(m_callLinkInfoIndex == m_callStructureStubCompilationInfo.size());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeOffset = (unsigned)-1;
#endif
}

void JIT::privateCompileLinkPass()
{
    unsigned jmpTableCount = m_jmpTable.size();
    for (unsigned i = 0; i < jmpTableCount; ++i)
        m_jmpTable[i].from.linkTo(m_labels[m_jmpTable[i].toBytecodeOffset], this);
    m_jmpTable.clear();
}

void JIT::privateCompileSlowCases()
{
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();

    m_getByIdIndex = 0;
    m_putByIdIndex = 0;
    m_byValInstructionIndex = 0;
    m_callLinkInfoIndex = 0;
    
    // Use this to assert that slow-path code associates new profiling sites with existing
    // ValueProfiles rather than creating new ones. This ensures that for a given instruction
    // (say, get_by_id) we get combined statistics for both the fast-path executions of that
    // instructions and the slow-path executions. Furthermore, if the slow-path code created
    // new ValueProfiles then the ValueProfiles would no longer be sorted by bytecode offset,
    // which would break the invariant necessary to use CodeBlock::valueProfileForBytecodeOffset().
    unsigned numberOfValueProfiles = m_codeBlock->numberOfValueProfiles();

    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end();) {
        m_bytecodeOffset = iter->to;

        unsigned firstTo = m_bytecodeOffset;

        Instruction* currentInstruction = instructionsBegin + m_bytecodeOffset;
        
        RareCaseProfile* rareCaseProfile = 0;
        if (shouldEmitProfiling())
            rareCaseProfile = m_codeBlock->addRareCaseProfile(m_bytecodeOffset);

#if ENABLE(JIT_VERBOSE)
        dataLogF("Old JIT emitting slow code for bc#%u at offset 0x%lx.\n", m_bytecodeOffset, (long)debugOffset());
#endif
        
        if (m_disassembler)
            m_disassembler->setForBytecodeSlowPath(m_bytecodeOffset, label());

        switch (m_interpreter->getOpcodeID(currentInstruction->u.opcode)) {
        DEFINE_SLOWCASE_OP(op_add)
        DEFINE_SLOWCASE_OP(op_bitand)
        DEFINE_SLOWCASE_OP(op_bitor)
        DEFINE_SLOWCASE_OP(op_bitxor)
        DEFINE_SLOWCASE_OP(op_call)
        DEFINE_SLOWCASE_OP(op_call_eval)
        DEFINE_SLOWCASE_OP(op_call_varargs)
        DEFINE_SLOWCASE_OP(op_construct)
        DEFINE_SLOWCASE_OP(op_to_this)
        DEFINE_SLOWCASE_OP(op_create_this)
        DEFINE_SLOWCASE_OP(op_captured_mov)
        DEFINE_SLOWCASE_OP(op_div)
        DEFINE_SLOWCASE_OP(op_eq)
        DEFINE_SLOWCASE_OP(op_get_callee)
        case op_get_by_id_out_of_line:
        case op_get_array_length:
        DEFINE_SLOWCASE_OP(op_get_by_id)
        DEFINE_SLOWCASE_OP(op_get_arguments_length)
        DEFINE_SLOWCASE_OP(op_get_by_val)
        DEFINE_SLOWCASE_OP(op_get_argument_by_val)
        DEFINE_SLOWCASE_OP(op_get_by_pname)
        DEFINE_SLOWCASE_OP(op_check_has_instance)
        DEFINE_SLOWCASE_OP(op_instanceof)
        DEFINE_SLOWCASE_OP(op_jfalse)
        DEFINE_SLOWCASE_OP(op_jless)
        DEFINE_SLOWCASE_OP(op_jlesseq)
        DEFINE_SLOWCASE_OP(op_jgreater)
        DEFINE_SLOWCASE_OP(op_jgreatereq)
        DEFINE_SLOWCASE_OP(op_jnless)
        DEFINE_SLOWCASE_OP(op_jnlesseq)
        DEFINE_SLOWCASE_OP(op_jngreater)
        DEFINE_SLOWCASE_OP(op_jngreatereq)
        DEFINE_SLOWCASE_OP(op_jtrue)
        DEFINE_SLOWCASE_OP(op_loop_hint)
        DEFINE_SLOWCASE_OP(op_lshift)
        DEFINE_SLOWCASE_OP(op_mod)
        DEFINE_SLOWCASE_OP(op_mul)
        DEFINE_SLOWCASE_OP(op_negate)
        DEFINE_SLOWCASE_OP(op_neq)
        DEFINE_SLOWCASE_OP(op_new_object)
        DEFINE_SLOWCASE_OP(op_not)
        DEFINE_SLOWCASE_OP(op_nstricteq)
        DEFINE_SLOWCASE_OP(op_dec)
        DEFINE_SLOWCASE_OP(op_inc)
        case op_put_by_id_out_of_line:
        case op_put_by_id_transition_direct:
        case op_put_by_id_transition_normal:
        case op_put_by_id_transition_direct_out_of_line:
        case op_put_by_id_transition_normal_out_of_line:
        DEFINE_SLOWCASE_OP(op_put_by_id)
        case op_put_by_val_direct:
        DEFINE_SLOWCASE_OP(op_put_by_val)
        DEFINE_SLOWCASE_OP(op_rshift)
        DEFINE_SLOWCASE_OP(op_unsigned)
        DEFINE_SLOWCASE_OP(op_urshift)
        DEFINE_SLOWCASE_OP(op_stricteq)
        DEFINE_SLOWCASE_OP(op_sub)
        DEFINE_SLOWCASE_OP(op_to_number)
        DEFINE_SLOWCASE_OP(op_to_primitive)

        DEFINE_SLOWCASE_OP(op_resolve_scope)
        DEFINE_SLOWCASE_OP(op_get_from_scope)
        DEFINE_SLOWCASE_OP(op_put_to_scope)

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        RELEASE_ASSERT_WITH_MESSAGE(iter == m_slowCases.end() || firstTo != iter->to, "Not enough jumps linked in slow case codegen.");
        RELEASE_ASSERT_WITH_MESSAGE(firstTo == (iter - 1)->to, "Too many jumps linked in slow case codegen.");
        
        if (shouldEmitProfiling())
            add32(TrustedImm32(1), AbsoluteAddress(&rareCaseProfile->m_counter));

        emitJumpSlowToHot(jump(), 0);
    }

    RELEASE_ASSERT(m_getByIdIndex == m_getByIds.size());
    RELEASE_ASSERT(m_putByIdIndex == m_putByIds.size());
    RELEASE_ASSERT(m_callLinkInfoIndex == m_callStructureStubCompilationInfo.size());
    RELEASE_ASSERT(numberOfValueProfiles == m_codeBlock->numberOfValueProfiles());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeOffset = (unsigned)-1;
#endif
}

CompilationResult JIT::privateCompile(JITCompilationEffort effort)
{
    DFG::CapabilityLevel level = m_codeBlock->capabilityLevel();
    switch (level) {
    case DFG::CannotCompile:
        m_canBeOptimized = false;
        m_canBeOptimizedOrInlined = false;
        m_shouldEmitProfiling = false;
        break;
    case DFG::CanInline:
        m_canBeOptimized = false;
        m_canBeOptimizedOrInlined = true;
        m_shouldEmitProfiling = true;
        break;
    case DFG::CanCompile:
    case DFG::CanCompileAndInline:
        m_canBeOptimized = true;
        m_canBeOptimizedOrInlined = true;
        m_shouldEmitProfiling = true;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    switch (m_codeBlock->codeType()) {
    case GlobalCode:
    case EvalCode:
        m_codeBlock->m_shouldAlwaysBeInlined = false;
        break;
    case FunctionCode:
        // We could have already set it to false because we detected an uninlineable call.
        // Don't override that observation.
        m_codeBlock->m_shouldAlwaysBeInlined &= canInline(level) && DFG::mightInlineFunction(m_codeBlock);
        break;
    }
    
    if (Options::showDisassembly() || m_vm->m_perBytecodeProfiler)
        m_disassembler = adoptPtr(new JITDisassembler(m_codeBlock));
    if (m_vm->m_perBytecodeProfiler) {
        m_compilation = adoptRef(
            new Profiler::Compilation(
                m_vm->m_perBytecodeProfiler->ensureBytecodesFor(m_codeBlock),
                Profiler::Baseline));
        m_compilation->addProfiledBytecodes(*m_vm->m_perBytecodeProfiler, m_codeBlock);
    }
    
    if (m_disassembler)
        m_disassembler->setStartOfCode(label());

    // Just add a little bit of randomness to the codegen
    if (m_randomGenerator.getUint32() & 1)
        nop();

    emitFunctionPrologue();
    emitPutImmediateToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);

    Label beginLabel(this);

    sampleCodeBlock(m_codeBlock);
#if ENABLE(OPCODE_SAMPLING)
    sampleInstruction(m_codeBlock->instructions().begin());
#endif

    Jump stackOverflow;
    if (m_codeBlock->codeType() == FunctionCode) {
        ASSERT(m_bytecodeOffset == (unsigned)-1);
        if (shouldEmitProfiling()) {
            for (int argument = 0; argument < m_codeBlock->numParameters(); ++argument) {
                // If this is a constructor, then we want to put in a dummy profiling site (to
                // keep things consistent) but we don't actually want to record the dummy value.
                if (m_codeBlock->m_isConstructor && !argument)
                    continue;
                int offset = CallFrame::argumentOffsetIncludingThis(argument) * static_cast<int>(sizeof(Register));
#if USE(JSVALUE64)
                load64(Address(callFrameRegister, offset), regT0);
#elif USE(JSVALUE32_64)
                load32(Address(callFrameRegister, offset + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
                load32(Address(callFrameRegister, offset + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);
#endif
                emitValueProfilingSite(m_codeBlock->valueProfileForArgument(argument));
            }
        }

        addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, regT1);
        stackOverflow = branchPtr(Above, AbsoluteAddress(m_vm->addressOfJSStackLimit()), regT1);
    }

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();
    
    if (m_disassembler)
        m_disassembler->setEndOfSlowPath(label());

    Label arityCheck;
    if (m_codeBlock->codeType() == FunctionCode) {
        stackOverflow.link(this);
        m_bytecodeOffset = 0;
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(-maxFrameExtentForSlowPathCall), stackPointerRegister);
        callOperationWithCallFrameRollbackOnException(operationThrowStackOverflowError, m_codeBlock);

        arityCheck = label();
        store8(TrustedImm32(0), &m_codeBlock->m_shouldAlwaysBeInlined);
        emitFunctionPrologue();
        emitPutImmediateToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);

        load32(payloadFor(JSStack::ArgumentCount), regT1);
        branch32(AboveOrEqual, regT1, TrustedImm32(m_codeBlock->m_numParameters)).linkTo(beginLabel, this);

        m_bytecodeOffset = 0;

        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(-maxFrameExtentForSlowPathCall), stackPointerRegister);
        callOperationWithCallFrameRollbackOnException(m_codeBlock->m_isConstructor ? operationConstructArityCheck : operationCallArityCheck);
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(maxFrameExtentForSlowPathCall), stackPointerRegister);
        if (returnValueGPR != regT0)
            move(returnValueGPR, regT0);
        branchTest32(Zero, regT0).linkTo(beginLabel, this);
        move(TrustedImmPtr(m_vm->arityCheckFailReturnThunks->returnPCsFor(*m_vm, m_codeBlock->numParameters())), regT5);
        loadPtr(BaseIndex(regT5, regT0, timesPtr()), regT5);
        emitNakedCall(m_vm->getCTIStub(arityFixup).code());

#if !ASSERT_DISABLED
        m_bytecodeOffset = (unsigned)-1; // Reset this, in order to guard its use with ASSERTs.
#endif

        jump(beginLabel);
    }

    ASSERT(m_jmpTable.isEmpty());
    
    privateCompileExceptionHandlers();
    
    if (m_disassembler)
        m_disassembler->setEndOfCode(label());

    LinkBuffer patchBuffer(*m_vm, this, m_codeBlock, effort);
    if (patchBuffer.didFailToAllocate())
        return CompilationFailed;

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (unsigned i = 0; i < m_switches.size(); ++i) {
        SwitchRecord record = m_switches[i];
        unsigned bytecodeOffset = record.bytecodeOffset;

        if (record.type != SwitchRecord::String) {
            ASSERT(record.type == SwitchRecord::Immediate || record.type == SwitchRecord::Character); 
            ASSERT(record.jumpTable.simpleJumpTable->branchOffsets.size() == record.jumpTable.simpleJumpTable->ctiOffsets.size());

            record.jumpTable.simpleJumpTable->ctiDefault = patchBuffer.locationOf(m_labels[bytecodeOffset + record.defaultOffset]);

            for (unsigned j = 0; j < record.jumpTable.simpleJumpTable->branchOffsets.size(); ++j) {
                unsigned offset = record.jumpTable.simpleJumpTable->branchOffsets[j];
                record.jumpTable.simpleJumpTable->ctiOffsets[j] = offset ? patchBuffer.locationOf(m_labels[bytecodeOffset + offset]) : record.jumpTable.simpleJumpTable->ctiDefault;
            }
        } else {
            ASSERT(record.type == SwitchRecord::String);

            record.jumpTable.stringJumpTable->ctiDefault = patchBuffer.locationOf(m_labels[bytecodeOffset + record.defaultOffset]);

            StringJumpTable::StringOffsetTable::iterator end = record.jumpTable.stringJumpTable->offsetTable.end();            
            for (StringJumpTable::StringOffsetTable::iterator it = record.jumpTable.stringJumpTable->offsetTable.begin(); it != end; ++it) {
                unsigned offset = it->value.branchOffset;
                it->value.ctiOffset = offset ? patchBuffer.locationOf(m_labels[bytecodeOffset + offset]) : record.jumpTable.stringJumpTable->ctiDefault;
            }
        }
    }

    for (size_t i = 0; i < m_codeBlock->numberOfExceptionHandlers(); ++i) {
        HandlerInfo& handler = m_codeBlock->exceptionHandler(i);
        handler.nativeCode = patchBuffer.locationOf(m_labels[handler.target]);
    }

    for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
        if (iter->to)
            patchBuffer.link(iter->from, FunctionPtr(iter->to));
    }

    for (unsigned i = m_getByIds.size(); i--;)
        m_getByIds[i].finalize(patchBuffer);
    for (unsigned i = m_putByIds.size(); i--;)
        m_putByIds[i].finalize(patchBuffer);

    m_codeBlock->setNumberOfByValInfos(m_byValCompilationInfo.size());
    for (unsigned i = 0; i < m_byValCompilationInfo.size(); ++i) {
        CodeLocationJump badTypeJump = CodeLocationJump(patchBuffer.locationOf(m_byValCompilationInfo[i].badTypeJump));
        CodeLocationLabel doneTarget = patchBuffer.locationOf(m_byValCompilationInfo[i].doneTarget);
        CodeLocationLabel slowPathTarget = patchBuffer.locationOf(m_byValCompilationInfo[i].slowPathTarget);
        CodeLocationCall returnAddress = patchBuffer.locationOf(m_byValCompilationInfo[i].returnAddress);
        
        m_codeBlock->byValInfo(i) = ByValInfo(
            m_byValCompilationInfo[i].bytecodeIndex,
            badTypeJump,
            m_byValCompilationInfo[i].arrayMode,
            differenceBetweenCodePtr(badTypeJump, doneTarget),
            differenceBetweenCodePtr(returnAddress, slowPathTarget));
    }
    m_codeBlock->setNumberOfCallLinkInfos(m_callStructureStubCompilationInfo.size());
    for (unsigned i = 0; i < m_codeBlock->numberOfCallLinkInfos(); ++i) {
        CallLinkInfo& info = m_codeBlock->callLinkInfo(i);
        info.callType = m_callStructureStubCompilationInfo[i].callType;
        info.codeOrigin = CodeOrigin(m_callStructureStubCompilationInfo[i].bytecodeIndex);
        info.callReturnLocation = patchBuffer.locationOfNearCall(m_callStructureStubCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = patchBuffer.locationOf(m_callStructureStubCompilationInfo[i].hotPathBegin);
        info.hotPathOther = patchBuffer.locationOfNearCall(m_callStructureStubCompilationInfo[i].hotPathOther);
        info.calleeGPR = regT0;
    }

    CompactJITCodeMap::Encoder jitCodeMapEncoder;
    for (unsigned bytecodeOffset = 0; bytecodeOffset < m_labels.size(); ++bytecodeOffset) {
        if (m_labels[bytecodeOffset].isSet())
            jitCodeMapEncoder.append(bytecodeOffset, patchBuffer.offsetOf(m_labels[bytecodeOffset]));
    }
    m_codeBlock->setJITCodeMap(jitCodeMapEncoder.finish());

    MacroAssemblerCodePtr withArityCheck;
    if (m_codeBlock->codeType() == FunctionCode)
        withArityCheck = patchBuffer.locationOf(arityCheck);

    if (Options::showDisassembly())
        m_disassembler->dump(patchBuffer);
    if (m_compilation) {
        m_disassembler->reportToProfiler(m_compilation.get(), patchBuffer);
        m_vm->m_perBytecodeProfiler->addCompilation(m_compilation);
    }
    
    CodeRef result = patchBuffer.finalizeCodeWithoutDisassembly();
    
    m_vm->machineCodeBytesPerBytecodeWordForBaselineJIT.add(
        static_cast<double>(result.size()) /
        static_cast<double>(m_codeBlock->instructions().size()));
    
    m_codeBlock->shrinkToFit(CodeBlock::LateShrink);
    m_codeBlock->setJITCode(
        adoptRef(new DirectJITCode(result, withArityCheck, JITCode::BaselineJIT)));
    
#if ENABLE(JIT_VERBOSE)
    dataLogF("JIT generated code for %p at [%p, %p).\n", m_codeBlock, result.executableMemory()->start(), result.executableMemory()->end());
#endif
    
    return CompilationSuccessful;
}

void JIT::privateCompileExceptionHandlers()
{
    if (m_exceptionChecks.empty() && m_exceptionChecksWithCallFrameRollback.empty())
        return;

    Jump doLookup;

    if (!m_exceptionChecksWithCallFrameRollback.empty()) {
        m_exceptionChecksWithCallFrameRollback.link(this);
        emitGetCallerFrameFromCallFrameHeaderPtr(GPRInfo::argumentGPR1);
        doLookup = jump();
    }

    if (!m_exceptionChecks.empty())
        m_exceptionChecks.link(this);
    
    // lookupExceptionHandler is passed two arguments, the VM and the exec (the CallFrame*).
    move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);

    if (doLookup.isSet())
        doLookup.link(this);

    move(TrustedImmPtr(vm()), GPRInfo::argumentGPR0);

#if CPU(X86)
    // FIXME: should use the call abstraction, but this is currently in the SpeculativeJIT layer!
    poke(GPRInfo::argumentGPR0);
    poke(GPRInfo::argumentGPR1, 1);
#endif
    m_calls.append(CallRecord(call(), (unsigned)-1, FunctionPtr(lookupExceptionHandler).value()));
    jumpToExceptionHandler();
}

unsigned JIT::frameRegisterCountFor(CodeBlock* codeBlock)
{
    ASSERT(static_cast<unsigned>(codeBlock->m_numCalleeRegisters) == WTF::roundUpToMultipleOf(stackAlignmentRegisters(), static_cast<unsigned>(codeBlock->m_numCalleeRegisters)));

    return roundLocalRegisterCountForFramePointerOffset(codeBlock->m_numCalleeRegisters + maxFrameExtentForSlowPathCallInRegisters);
}

int JIT::stackPointerOffsetFor(CodeBlock* codeBlock)
{
    return virtualRegisterForLocal(frameRegisterCountFor(codeBlock) - 1).offset();
}

} // namespace JSC

#endif // ENABLE(JIT)
