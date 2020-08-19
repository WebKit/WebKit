/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "BytecodeGraph.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGCapabilities.h"
#include "JITInlines.h"
#include "JITOperations.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "ModuleProgramCodeBlock.h"
#include "PCToCodeOriginMap.h"
#include "ProbeContext.h"
#include "ProfilerDatabase.h"
#include "ProgramCodeBlock.h"
#include "SlowPathCall.h"
#include "StackAlignment.h"
#include "ThunkGenerators.h"
#include "TypeProfilerLog.h"
#include <wtf/GraphNodeWorklist.h>
#include <wtf/SimpleStats.h>

namespace JSC {
namespace JITInternal {
static constexpr const bool verbose = false;
}

Seconds totalBaselineCompileTime;
Seconds totalDFGCompileTime;
Seconds totalFTLCompileTime;
Seconds totalFTLDFGCompileTime;
Seconds totalFTLB3CompileTime;

void ctiPatchCallByReturnAddress(ReturnAddressPtr returnAddress, FunctionPtr<CFunctionPtrTag> newCalleeFunction)
{
    MacroAssembler::repatchCall(
        CodeLocationCall<NoPtrTag>(MacroAssemblerCodePtr<NoPtrTag>(returnAddress)),
        newCalleeFunction.retagged<OperationPtrTag>());
}

JIT::JIT(VM& vm, CodeBlock* codeBlock, BytecodeIndex loopOSREntryBytecodeIndex)
    : JSInterfaceJIT(&vm, codeBlock)
    , m_interpreter(vm.interpreter)
    , m_labels(codeBlock ? codeBlock->instructions().size() : 0)
    , m_pcToCodeOriginMapBuilder(vm)
    , m_canBeOptimized(false)
    , m_shouldEmitProfiling(false)
    , m_loopOSREntryBytecodeIndex(loopOSREntryBytecodeIndex)
{
}

JIT::~JIT()
{
}

#if ENABLE(DFG_JIT)
void JIT::emitEnterOptimizationCheck()
{
    if (!canBeOptimized())
        return;

    JumpList skipOptimize;
    
    skipOptimize.append(branchAdd32(Signed, TrustedImm32(Options::executionCounterIncrementForEntry()), AbsoluteAddress(m_codeBlock->addressOfJITExecuteCounter())));
    ASSERT(!m_bytecodeIndex.offset());

    copyCalleeSavesFromFrameOrRegisterToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

    callOperation(operationOptimize, &vm(), m_bytecodeIndex.asBits());
    skipOptimize.append(branchTestPtr(Zero, returnValueGPR));
    farJump(returnValueGPR, GPRInfo::callFrameRegister);
    skipOptimize.link(this);
}
#endif

void JIT::emitNotifyWrite(WatchpointSet* set)
{
    if (!set || set->state() == IsInvalidated) {
        addSlowCase(Jump());
        return;
    }
    
    addSlowCase(branch8(NotEqual, AbsoluteAddress(set->addressOfState()), TrustedImm32(IsInvalidated)));
}

void JIT::emitNotifyWrite(GPRReg pointerToSet)
{
    addSlowCase(branch8(NotEqual, Address(pointerToSet, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
}

void JIT::assertStackPointerOffset()
{
    if (!ASSERT_ENABLED)
        return;
    
    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, regT0);
    Jump ok = branchPtr(Equal, regT0, stackPointerRegister);
    breakpoint();
    ok.link(this);
}

#define NEXT_OPCODE(name) \
    m_bytecodeIndex = BytecodeIndex(m_bytecodeIndex.offset() + currentInstruction->size()); \
    break;

#define NEXT_OPCODE_IN_MAIN(name) \
    if (previousSlowCasesSize != m_slowCases.size()) \
        ++m_bytecodeCountHavingSlowCase; \
    NEXT_OPCODE(name)

#define DEFINE_SLOW_OP(name) \
    case op_##name: { \
        if (m_bytecodeIndex >= startBytecodeIndex) { \
            JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_##name); \
            slowPathCall.call(); \
        } \
        NEXT_OPCODE_IN_MAIN(op_##name); \
    }

#define DEFINE_OP(name) \
    case name: { \
        if (m_bytecodeIndex >= startBytecodeIndex) { \
            emit_##name(currentInstruction); \
        } \
        NEXT_OPCODE_IN_MAIN(name); \
    }

#define DEFINE_SLOWCASE_OP(name) \
    case name: { \
        emitSlow_##name(currentInstruction, iter); \
        NEXT_OPCODE(name); \
    }

#define DEFINE_SLOWCASE_SLOW_OP(name) \
    case op_##name: { \
        emitSlowCaseCall(currentInstruction, iter, slow_path_##name); \
        NEXT_OPCODE(op_##name); \
    }

void JIT::emitSlowCaseCall(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter, SlowPathFunction stub)
{
    linkAllSlowCases(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, stub);
    slowPathCall.call();
}

void JIT::privateCompileMainPass()
{
    if (JITInternal::verbose)
        dataLog("Compiling ", *m_codeBlock, "\n");
    
    jitAssertTagsInPlace();
    jitAssertArgumentCountSane();
    
    auto& instructions = m_codeBlock->instructions();
    unsigned instructionCount = m_codeBlock->instructions().size();

    m_callLinkInfoIndex = 0;

    VM& vm = m_codeBlock->vm();
    BytecodeIndex startBytecodeIndex(0);
    if (m_loopOSREntryBytecodeIndex && (m_codeBlock->inherits<ProgramCodeBlock>(vm) || m_codeBlock->inherits<ModuleProgramCodeBlock>(vm))) {
        // We can only do this optimization because we execute ProgramCodeBlock's exactly once.
        // This optimization would be invalid otherwise. When the LLInt determines it wants to
        // do OSR entry into the baseline JIT in a loop, it will pass in the bytecode offset it
        // was executing at when it kicked off our compilation. We only need to compile code for
        // anything reachable from that bytecode offset.

        // We only bother building the bytecode graph if it could save time and executable
        // memory. We pick an arbitrary offset where we deem this is profitable.
        if (m_loopOSREntryBytecodeIndex.offset() >= 200) {
            // As a simplification, we don't find all bytecode ranges that are unreachable.
            // Instead, we just find the minimum bytecode offset that is reachable, and
            // compile code from that bytecode offset onwards.

            BytecodeGraph graph(m_codeBlock, m_codeBlock->instructions());
            BytecodeBasicBlock* block = graph.findBasicBlockForBytecodeOffset(m_loopOSREntryBytecodeIndex.offset());
            RELEASE_ASSERT(block);

            GraphNodeWorklist<BytecodeBasicBlock*> worklist;
            startBytecodeIndex = BytecodeIndex();
            worklist.push(block);

            while (BytecodeBasicBlock* block = worklist.pop()) {
                startBytecodeIndex = BytecodeIndex(std::min(startBytecodeIndex.offset(), block->leaderOffset()));
                for (unsigned successorIndex : block->successors())
                    worklist.push(&graph[successorIndex]);

                // Also add catch blocks for bytecodes that throw.
                if (m_codeBlock->numberOfExceptionHandlers()) {
                    for (unsigned bytecodeOffset = block->leaderOffset(); bytecodeOffset < block->leaderOffset() + block->totalLength();) {
                        auto instruction = instructions.at(bytecodeOffset);
                        if (auto* handler = m_codeBlock->handlerForBytecodeIndex(BytecodeIndex(bytecodeOffset)))
                            worklist.push(graph.findBasicBlockWithLeaderOffset(handler->target));

                        bytecodeOffset += instruction->size();
                    }
                }
            }
        }
    }

    m_bytecodeCountHavingSlowCase = 0;
    for (m_bytecodeIndex = BytecodeIndex(0); m_bytecodeIndex.offset() < instructionCount; ) {
        unsigned previousSlowCasesSize = m_slowCases.size();
        if (m_bytecodeIndex == startBytecodeIndex && startBytecodeIndex.offset() > 0) {
            // We've proven all bytecode instructions up until here are unreachable.
            // Let's ensure that by crashing if it's ever hit.
            breakpoint();
        }

        if (m_disassembler)
            m_disassembler->setForBytecodeMainPath(m_bytecodeIndex.offset(), label());
        const Instruction* currentInstruction = instructions.at(m_bytecodeIndex).ptr();
        ASSERT(currentInstruction->size());

        m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(m_bytecodeIndex));

#if ENABLE(OPCODE_SAMPLING)
        if (m_bytecodeIndex > 0) // Avoid the overhead of sampling op_enter twice.
            sampleInstruction(currentInstruction);
#endif

        m_labels[m_bytecodeIndex.offset()] = label();

        if (JITInternal::verbose)
            dataLogLn("Baseline JIT emitting code for ", m_bytecodeIndex, " at offset ", (long)debugOffset());

        OpcodeID opcodeID = currentInstruction->opcodeID();

        if (UNLIKELY(m_compilation)) {
            add64(
                TrustedImm32(1),
                AbsoluteAddress(m_compilation->executionCounterFor(Profiler::OriginStack(Profiler::Origin(
                    m_compilation->bytecodes(), m_bytecodeIndex)))->address()));
        }
        
        if (Options::eagerlyUpdateTopCallFrame())
            updateTopCallFrame();

        unsigned bytecodeOffset = m_bytecodeIndex.offset();
#if ENABLE(MASM_PROBE)
        if (UNLIKELY(Options::traceBaselineJITExecution())) {
            CodeBlock* codeBlock = m_codeBlock;
            probe([=] (Probe::Context& ctx) {
                dataLogLn("JIT [", bytecodeOffset, "] ", opcodeNames[opcodeID], " cfr ", RawPointer(ctx.fp()), " @ ", codeBlock);
            });
        }
#endif

        switch (opcodeID) {
        DEFINE_SLOW_OP(in_by_val)
        DEFINE_SLOW_OP(less)
        DEFINE_SLOW_OP(lesseq)
        DEFINE_SLOW_OP(greater)
        DEFINE_SLOW_OP(greatereq)
        DEFINE_SLOW_OP(is_callable)
        DEFINE_SLOW_OP(is_constructor)
        DEFINE_SLOW_OP(typeof)
        DEFINE_SLOW_OP(typeof_is_object)
        DEFINE_SLOW_OP(typeof_is_function)
        DEFINE_SLOW_OP(strcat)
        DEFINE_SLOW_OP(push_with_scope)
        DEFINE_SLOW_OP(create_lexical_environment)
        DEFINE_SLOW_OP(get_by_val_with_this)
        DEFINE_SLOW_OP(put_by_id_with_this)
        DEFINE_SLOW_OP(put_by_val_with_this)
        DEFINE_SLOW_OP(resolve_scope_for_hoisting_func_decl_in_eval)
        DEFINE_SLOW_OP(define_data_property)
        DEFINE_SLOW_OP(define_accessor_property)
        DEFINE_SLOW_OP(unreachable)
        DEFINE_SLOW_OP(throw_static_error)
        DEFINE_SLOW_OP(new_array_with_spread)
        DEFINE_SLOW_OP(new_array_buffer)
        DEFINE_SLOW_OP(spread)
        DEFINE_SLOW_OP(get_enumerable_length)
        DEFINE_SLOW_OP(has_generic_property)
        DEFINE_SLOW_OP(get_property_enumerator)
        DEFINE_SLOW_OP(to_index_string)
        DEFINE_SLOW_OP(create_direct_arguments)
        DEFINE_SLOW_OP(create_scoped_arguments)
        DEFINE_SLOW_OP(create_cloned_arguments)
        DEFINE_SLOW_OP(create_arguments_butterfly)
        DEFINE_SLOW_OP(create_rest)
        DEFINE_SLOW_OP(create_promise)
        DEFINE_SLOW_OP(new_promise)
        DEFINE_SLOW_OP(create_generator)
        DEFINE_SLOW_OP(create_async_generator)
        DEFINE_SLOW_OP(new_generator)
        DEFINE_SLOW_OP(pow)

        DEFINE_OP(op_add)
        DEFINE_OP(op_bitnot)
        DEFINE_OP(op_bitand)
        DEFINE_OP(op_bitor)
        DEFINE_OP(op_bitxor)
        DEFINE_OP(op_call)
        DEFINE_OP(op_tail_call)
        DEFINE_OP(op_call_eval)
        DEFINE_OP(op_call_varargs)
        DEFINE_OP(op_tail_call_varargs)
        DEFINE_OP(op_tail_call_forward_arguments)
        DEFINE_OP(op_construct_varargs)
        DEFINE_OP(op_catch)
        DEFINE_OP(op_construct)
        DEFINE_OP(op_create_this)
        DEFINE_OP(op_to_this)
        DEFINE_OP(op_get_argument)
        DEFINE_OP(op_argument_count)
        DEFINE_OP(op_get_rest_length)
        DEFINE_OP(op_check_tdz)
        DEFINE_OP(op_identity_with_profile)
        DEFINE_OP(op_debug)
        DEFINE_OP(op_del_by_id)
        DEFINE_OP(op_del_by_val)
        DEFINE_OP(op_div)
        DEFINE_OP(op_end)
        DEFINE_OP(op_enter)
        DEFINE_OP(op_get_scope)
        DEFINE_OP(op_eq)
        DEFINE_OP(op_eq_null)
        DEFINE_OP(op_below)
        DEFINE_OP(op_beloweq)
        DEFINE_OP(op_try_get_by_id)
        DEFINE_OP(op_in_by_id)
        DEFINE_OP(op_get_by_id)
        DEFINE_OP(op_get_by_id_with_this)
        DEFINE_OP(op_get_by_id_direct)
        DEFINE_OP(op_get_by_val)
        DEFINE_OP(op_get_private_name)
        DEFINE_OP(op_get_prototype_of)
        DEFINE_OP(op_overrides_has_instance)
        DEFINE_OP(op_instanceof)
        DEFINE_OP(op_instanceof_custom)
        DEFINE_OP(op_is_empty)
        DEFINE_OP(op_typeof_is_undefined)
        DEFINE_OP(op_is_undefined_or_null)
        DEFINE_OP(op_is_boolean)
        DEFINE_OP(op_is_number)
        DEFINE_OP(op_is_big_int)
        DEFINE_OP(op_is_object)
        DEFINE_OP(op_is_cell_with_type)
        DEFINE_OP(op_jeq_null)
        DEFINE_OP(op_jfalse)
        DEFINE_OP(op_jmp)
        DEFINE_OP(op_jneq_null)
        DEFINE_OP(op_jundefined_or_null)
        DEFINE_OP(op_jnundefined_or_null)
        DEFINE_OP(op_jneq_ptr)
        DEFINE_OP(op_jless)
        DEFINE_OP(op_jlesseq)
        DEFINE_OP(op_jgreater)
        DEFINE_OP(op_jgreatereq)
        DEFINE_OP(op_jnless)
        DEFINE_OP(op_jnlesseq)
        DEFINE_OP(op_jngreater)
        DEFINE_OP(op_jngreatereq)
        DEFINE_OP(op_jeq)
        DEFINE_OP(op_jneq)
        DEFINE_OP(op_jstricteq)
        DEFINE_OP(op_jnstricteq)
        DEFINE_OP(op_jbelow)
        DEFINE_OP(op_jbeloweq)
        DEFINE_OP(op_jtrue)
        DEFINE_OP(op_loop_hint)
        DEFINE_OP(op_check_traps)
        DEFINE_OP(op_nop)
        DEFINE_OP(op_super_sampler_begin)
        DEFINE_OP(op_super_sampler_end)
        DEFINE_OP(op_lshift)
        DEFINE_OP(op_mod)
        DEFINE_OP(op_mov)
        DEFINE_OP(op_mul)
        DEFINE_OP(op_negate)
        DEFINE_OP(op_neq)
        DEFINE_OP(op_neq_null)
        DEFINE_OP(op_new_array)
        DEFINE_OP(op_new_array_with_size)
        DEFINE_OP(op_new_func)
        DEFINE_OP(op_new_func_exp)
        DEFINE_OP(op_new_generator_func)
        DEFINE_OP(op_new_generator_func_exp)
        DEFINE_OP(op_new_async_func)
        DEFINE_OP(op_new_async_func_exp)
        DEFINE_OP(op_new_async_generator_func)
        DEFINE_OP(op_new_async_generator_func_exp)
        DEFINE_OP(op_new_object)
        DEFINE_OP(op_new_regexp)
        DEFINE_OP(op_not)
        DEFINE_OP(op_nstricteq)
        DEFINE_OP(op_dec)
        DEFINE_OP(op_inc)
        DEFINE_OP(op_profile_type)
        DEFINE_OP(op_profile_control_flow)
        DEFINE_OP(op_get_parent_scope)
        DEFINE_OP(op_put_by_id)
        DEFINE_OP(op_put_by_val_direct)
        DEFINE_OP(op_put_by_val)
        DEFINE_OP(op_put_getter_by_id)
        DEFINE_OP(op_put_setter_by_id)
        DEFINE_OP(op_put_getter_setter_by_id)
        DEFINE_OP(op_put_getter_by_val)
        DEFINE_OP(op_put_setter_by_val)
        DEFINE_OP(op_to_property_key)

        DEFINE_OP(op_get_internal_field)
        DEFINE_OP(op_put_internal_field)

        DEFINE_OP(op_iterator_open)
        DEFINE_OP(op_iterator_next)

        DEFINE_OP(op_ret)
        DEFINE_OP(op_rshift)
        DEFINE_OP(op_unsigned)
        DEFINE_OP(op_urshift)
        DEFINE_OP(op_set_function_name)
        DEFINE_OP(op_stricteq)
        DEFINE_OP(op_sub)
        DEFINE_OP(op_switch_char)
        DEFINE_OP(op_switch_imm)
        DEFINE_OP(op_switch_string)
        DEFINE_OP(op_throw)
        DEFINE_OP(op_to_number)
        DEFINE_OP(op_to_numeric)
        DEFINE_OP(op_to_string)
        DEFINE_OP(op_to_object)
        DEFINE_OP(op_to_primitive)

        DEFINE_OP(op_resolve_scope)
        DEFINE_OP(op_get_from_scope)
        DEFINE_OP(op_put_to_scope)
        DEFINE_OP(op_get_from_arguments)
        DEFINE_OP(op_put_to_arguments)

        DEFINE_OP(op_has_structure_property)
        DEFINE_OP(op_has_own_structure_property)
        DEFINE_OP(op_in_structure_property)
        DEFINE_OP(op_has_indexed_property)
        DEFINE_OP(op_get_direct_pname)
        DEFINE_OP(op_enumerator_structure_pname)
        DEFINE_OP(op_enumerator_generic_pname)
            
        DEFINE_OP(op_log_shadow_chicken_prologue)
        DEFINE_OP(op_log_shadow_chicken_tail)

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (JITInternal::verbose)
            dataLog("At ", bytecodeOffset, ": ", m_slowCases.size(), "\n");
    }

    RELEASE_ASSERT(m_callLinkInfoIndex == m_callCompilationInfo.size());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeIndex = BytecodeIndex();
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
    m_getByIdIndex = 0;
    m_getByValIndex = 0;
    m_getByIdWithThisIndex = 0;
    m_putByIdIndex = 0;
    m_inByIdIndex = 0;
    m_delByValIndex = 0;
    m_delByIdIndex = 0;
    m_instanceOfIndex = 0;
    m_byValInstructionIndex = 0;
    m_callLinkInfoIndex = 0;

    RefCountedArray<RareCaseProfile> rareCaseProfiles;
    if (shouldEmitProfiling())
        rareCaseProfiles = RefCountedArray<RareCaseProfile>(m_bytecodeCountHavingSlowCase);
    
    unsigned bytecodeCountHavingSlowCase = 0;
    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end();) {
        m_bytecodeIndex = iter->to;

        m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(m_bytecodeIndex));

        BytecodeIndex firstTo = m_bytecodeIndex;

        const Instruction* currentInstruction = m_codeBlock->instructions().at(m_bytecodeIndex).ptr();
        
        RareCaseProfile* rareCaseProfile = nullptr;
        if (shouldEmitProfiling())
            rareCaseProfile = &rareCaseProfiles.at(bytecodeCountHavingSlowCase);

        if (JITInternal::verbose)
            dataLogLn("Baseline JIT emitting slow code for ", m_bytecodeIndex, " at offset ", (long)debugOffset());

        if (m_disassembler)
            m_disassembler->setForBytecodeSlowPath(m_bytecodeIndex.offset(), label());

#if ENABLE(MASM_PROBE)
        if (UNLIKELY(Options::traceBaselineJITExecution())) {
            OpcodeID opcodeID = currentInstruction->opcodeID();
            unsigned bytecodeOffset = m_bytecodeIndex.offset();
            CodeBlock* codeBlock = m_codeBlock;
            probe([=] (Probe::Context& ctx) {
                dataLogLn("JIT [", bytecodeOffset, "] SLOW ", opcodeNames[opcodeID], " cfr ", RawPointer(ctx.fp()), " @ ", codeBlock);
            });
        }
#endif

        switch (currentInstruction->opcodeID()) {
        DEFINE_SLOWCASE_OP(op_add)
        DEFINE_SLOWCASE_OP(op_call)
        DEFINE_SLOWCASE_OP(op_tail_call)
        DEFINE_SLOWCASE_OP(op_call_eval)
        DEFINE_SLOWCASE_OP(op_call_varargs)
        DEFINE_SLOWCASE_OP(op_tail_call_varargs)
        DEFINE_SLOWCASE_OP(op_tail_call_forward_arguments)
        DEFINE_SLOWCASE_OP(op_construct_varargs)
        DEFINE_SLOWCASE_OP(op_construct)
        DEFINE_SLOWCASE_OP(op_eq)
        DEFINE_SLOWCASE_OP(op_try_get_by_id)
        DEFINE_SLOWCASE_OP(op_in_by_id)
        DEFINE_SLOWCASE_OP(op_get_by_id)
        DEFINE_SLOWCASE_OP(op_get_by_id_with_this)
        DEFINE_SLOWCASE_OP(op_get_by_id_direct)
        DEFINE_SLOWCASE_OP(op_get_by_val)
        DEFINE_SLOWCASE_OP(op_get_private_name)
        DEFINE_SLOWCASE_OP(op_instanceof)
        DEFINE_SLOWCASE_OP(op_instanceof_custom)
        DEFINE_SLOWCASE_OP(op_jless)
        DEFINE_SLOWCASE_OP(op_jlesseq)
        DEFINE_SLOWCASE_OP(op_jgreater)
        DEFINE_SLOWCASE_OP(op_jgreatereq)
        DEFINE_SLOWCASE_OP(op_jnless)
        DEFINE_SLOWCASE_OP(op_jnlesseq)
        DEFINE_SLOWCASE_OP(op_jngreater)
        DEFINE_SLOWCASE_OP(op_jngreatereq)
        DEFINE_SLOWCASE_OP(op_jeq)
        DEFINE_SLOWCASE_OP(op_jneq)
        DEFINE_SLOWCASE_OP(op_jstricteq)
        DEFINE_SLOWCASE_OP(op_jnstricteq)
        DEFINE_SLOWCASE_OP(op_loop_hint)
        DEFINE_SLOWCASE_OP(op_check_traps)
        DEFINE_SLOWCASE_OP(op_mod)
        DEFINE_SLOWCASE_OP(op_mul)
        DEFINE_SLOWCASE_OP(op_negate)
        DEFINE_SLOWCASE_OP(op_neq)
        DEFINE_SLOWCASE_OP(op_new_object)
        DEFINE_SLOWCASE_OP(op_put_by_id)
        case op_put_by_val_direct:
        DEFINE_SLOWCASE_OP(op_put_by_val)
        DEFINE_SLOWCASE_OP(op_del_by_val)
        DEFINE_SLOWCASE_OP(op_del_by_id)
        DEFINE_SLOWCASE_OP(op_sub)
        DEFINE_SLOWCASE_OP(op_has_indexed_property)
        DEFINE_SLOWCASE_OP(op_get_from_scope)
        DEFINE_SLOWCASE_OP(op_put_to_scope)

        DEFINE_SLOWCASE_OP(op_iterator_open)
        DEFINE_SLOWCASE_OP(op_iterator_next)

        DEFINE_SLOWCASE_SLOW_OP(unsigned)
        DEFINE_SLOWCASE_SLOW_OP(inc)
        DEFINE_SLOWCASE_SLOW_OP(dec)
        DEFINE_SLOWCASE_SLOW_OP(bitnot)
        DEFINE_SLOWCASE_SLOW_OP(bitand)
        DEFINE_SLOWCASE_SLOW_OP(bitor)
        DEFINE_SLOWCASE_SLOW_OP(bitxor)
        DEFINE_SLOWCASE_SLOW_OP(lshift)
        DEFINE_SLOWCASE_SLOW_OP(rshift)
        DEFINE_SLOWCASE_SLOW_OP(urshift)
        DEFINE_SLOWCASE_SLOW_OP(div)
        DEFINE_SLOWCASE_SLOW_OP(create_this)
        DEFINE_SLOWCASE_SLOW_OP(create_promise)
        DEFINE_SLOWCASE_SLOW_OP(create_generator)
        DEFINE_SLOWCASE_SLOW_OP(create_async_generator)
        DEFINE_SLOWCASE_SLOW_OP(to_this)
        DEFINE_SLOWCASE_SLOW_OP(to_primitive)
        DEFINE_SLOWCASE_SLOW_OP(to_number)
        DEFINE_SLOWCASE_SLOW_OP(to_numeric)
        DEFINE_SLOWCASE_SLOW_OP(to_string)
        DEFINE_SLOWCASE_SLOW_OP(to_object)
        DEFINE_SLOWCASE_SLOW_OP(not)
        DEFINE_SLOWCASE_SLOW_OP(stricteq)
        DEFINE_SLOWCASE_SLOW_OP(nstricteq)
        DEFINE_SLOWCASE_SLOW_OP(get_direct_pname)
        DEFINE_SLOWCASE_SLOW_OP(get_prototype_of)
        DEFINE_SLOWCASE_SLOW_OP(has_structure_property)
        DEFINE_SLOWCASE_SLOW_OP(has_own_structure_property)
        DEFINE_SLOWCASE_SLOW_OP(in_structure_property)
        DEFINE_SLOWCASE_SLOW_OP(resolve_scope)
        DEFINE_SLOWCASE_SLOW_OP(check_tdz)
        DEFINE_SLOWCASE_SLOW_OP(to_property_key)
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (JITInternal::verbose)
            dataLog("At ", firstTo, " slow: ", iter - m_slowCases.begin(), "\n");

        RELEASE_ASSERT_WITH_MESSAGE(iter == m_slowCases.end() || firstTo.offset() != iter->to.offset(), "Not enough jumps linked in slow case codegen.");
        RELEASE_ASSERT_WITH_MESSAGE(firstTo.offset() == (iter - 1)->to.offset(), "Too many jumps linked in slow case codegen.");
        
        if (shouldEmitProfiling())
            add32(TrustedImm32(1), AbsoluteAddress(&rareCaseProfile->m_counter));

        emitJumpSlowToHot(jump(), 0);
        ++bytecodeCountHavingSlowCase;
    }

    RELEASE_ASSERT(bytecodeCountHavingSlowCase == m_bytecodeCountHavingSlowCase);
    RELEASE_ASSERT(m_getByIdIndex == m_getByIds.size());
    RELEASE_ASSERT(m_getByIdWithThisIndex == m_getByIdsWithThis.size());
    RELEASE_ASSERT(m_putByIdIndex == m_putByIds.size());
    RELEASE_ASSERT(m_inByIdIndex == m_inByIds.size());
    RELEASE_ASSERT(m_instanceOfIndex == m_instanceOfs.size());
    RELEASE_ASSERT(m_callLinkInfoIndex == m_callCompilationInfo.size());

    if (shouldEmitProfiling())
        m_codeBlock->setRareCaseProfiles(WTFMove(rareCaseProfiles));

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeIndex = BytecodeIndex();
#endif
}

void JIT::compileWithoutLinking(JITCompilationEffort effort)
{
    MonotonicTime before { };
    if (UNLIKELY(computeCompileTimes()))
        before = MonotonicTime::now();
    
    DFG::CapabilityLevel level = m_codeBlock->capabilityLevel();
    switch (level) {
    case DFG::CannotCompile:
        m_canBeOptimized = false;
        m_canBeOptimizedOrInlined = false;
        m_shouldEmitProfiling = false;
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
    case ModuleCode:
    case EvalCode:
        m_codeBlock->m_shouldAlwaysBeInlined = false;
        break;
    case FunctionCode:
        // We could have already set it to false because we detected an uninlineable call.
        // Don't override that observation.
        m_codeBlock->m_shouldAlwaysBeInlined &= canInline(level) && DFG::mightInlineFunction(m_codeBlock);
        break;
    }

    if (UNLIKELY(Options::dumpDisassembly() || (m_vm->m_perBytecodeProfiler && Options::disassembleBaselineForProfiler())))
        m_disassembler = makeUnique<JITDisassembler>(m_codeBlock);
    if (UNLIKELY(m_vm->m_perBytecodeProfiler)) {
        m_compilation = adoptRef(
            new Profiler::Compilation(
                m_vm->m_perBytecodeProfiler->ensureBytecodesFor(m_codeBlock),
                Profiler::Baseline));
        m_compilation->addProfiledBytecodes(*m_vm->m_perBytecodeProfiler, m_codeBlock);
    }
    
    m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(BytecodeIndex(0)));

    Label entryLabel(this);
    if (m_disassembler)
        m_disassembler->setStartOfCode(entryLabel);

    // Just add a little bit of randomness to the codegen
    if (random() & 1)
        nop();

    emitFunctionPrologue();
    emitPutToCallFrameHeader(m_codeBlock, CallFrameSlot::codeBlock);

    Label beginLabel(this);

    sampleCodeBlock(m_codeBlock);
#if ENABLE(OPCODE_SAMPLING)
    sampleInstruction(m_codeBlock->instructions().begin());
#endif

    int frameTopOffset = stackPointerOffsetFor(m_codeBlock) * sizeof(Register);
    unsigned maxFrameSize = -frameTopOffset;
    addPtr(TrustedImm32(frameTopOffset), callFrameRegister, regT1);
    JumpList stackOverflow;
    if (UNLIKELY(maxFrameSize > Options::reservedZoneSize()))
        stackOverflow.append(branchPtr(Above, regT1, callFrameRegister));
    stackOverflow.append(branchPtr(Above, AbsoluteAddress(m_vm->addressOfSoftStackLimit()), regT1));

    move(regT1, stackPointerRegister);
    checkStackPointerAlignment();

    emitSaveCalleeSaves();
    emitMaterializeTagCheckRegisters();

    if (m_codeBlock->codeType() == FunctionCode) {
        ASSERT(!m_bytecodeIndex);
        if (shouldEmitProfiling()) {
            for (int argument = 0; argument < m_codeBlock->numParameters(); ++argument) {
                // If this is a constructor, then we want to put in a dummy profiling site (to
                // keep things consistent) but we don't actually want to record the dummy value.
                if (m_codeBlock->isConstructor() && !argument)
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
    }
    
    RELEASE_ASSERT(!JITCode::isJIT(m_codeBlock->jitType()));

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();
    
    if (m_disassembler)
        m_disassembler->setEndOfSlowPath(label());
    m_pcToCodeOriginMapBuilder.appendItem(label(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

    stackOverflow.link(this);
    m_bytecodeIndex = BytecodeIndex(0);
    if (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);
    callOperationWithCallFrameRollbackOnException(operationThrowStackOverflowError, m_codeBlock);

    // If the number of parameters is 1, we never require arity fixup.
    bool requiresArityFixup = m_codeBlock->m_numParameters != 1;
    if (m_codeBlock->codeType() == FunctionCode && requiresArityFixup) {
        m_arityCheck = label();
        store8(TrustedImm32(0), &m_codeBlock->m_shouldAlwaysBeInlined);
        emitFunctionPrologue();
        emitPutToCallFrameHeader(m_codeBlock, CallFrameSlot::codeBlock);

        load32(payloadFor(CallFrameSlot::argumentCountIncludingThis), regT1);
        branch32(AboveOrEqual, regT1, TrustedImm32(m_codeBlock->m_numParameters)).linkTo(beginLabel, this);

        m_bytecodeIndex = BytecodeIndex(0);

        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);
        callOperationWithCallFrameRollbackOnException(m_codeBlock->isConstructor() ? operationConstructArityCheck : operationCallArityCheck, m_codeBlock->globalObject());
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(maxFrameExtentForSlowPathCall), stackPointerRegister);
        branchTest32(Zero, returnValueGPR).linkTo(beginLabel, this);
        move(returnValueGPR, GPRInfo::argumentGPR0);
        emitNakedCall(m_vm->getCTIStub(arityFixupGenerator).retaggedCode<NoPtrTag>());

#if ASSERT_ENABLED
        m_bytecodeIndex = BytecodeIndex(); // Reset this, in order to guard its use with ASSERTs.
#endif

        jump(beginLabel);
    } else
        m_arityCheck = entryLabel; // Never require arity fixup.

    ASSERT(m_jmpTable.isEmpty());
    
    privateCompileExceptionHandlers();
    
    if (m_disassembler)
        m_disassembler->setEndOfCode(label());
    m_pcToCodeOriginMapBuilder.appendItem(label(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

    m_linkBuffer = std::unique_ptr<LinkBuffer>(new LinkBuffer(*this, m_codeBlock, effort));

    MonotonicTime after { };
    if (UNLIKELY(computeCompileTimes())) {
        after = MonotonicTime::now();

        if (Options::reportTotalCompileTimes())
            totalBaselineCompileTime += after - before;
    }
    if (UNLIKELY(reportCompileTimes())) {
        CString codeBlockName = toCString(*m_codeBlock);
        
        dataLog("Optimized ", codeBlockName, " with Baseline JIT into ", m_linkBuffer->size(), " bytes in ", (after - before).milliseconds(), " ms.\n");
    }
}

CompilationResult JIT::link()
{
    LinkBuffer& patchBuffer = *m_linkBuffer;
    
    if (patchBuffer.didFailToAllocate())
        return CompilationFailed;

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (auto& record : m_switches) {
        unsigned bytecodeOffset = record.bytecodeIndex.offset();

        if (record.type != SwitchRecord::String) {
            ASSERT(record.type == SwitchRecord::Immediate || record.type == SwitchRecord::Character); 
            ASSERT(record.jumpTable.simpleJumpTable->branchOffsets.size() == record.jumpTable.simpleJumpTable->ctiOffsets.size());

            auto* simpleJumpTable = record.jumpTable.simpleJumpTable;
            simpleJumpTable->ctiDefault = patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + record.defaultOffset]);

            for (unsigned j = 0; j < record.jumpTable.simpleJumpTable->branchOffsets.size(); ++j) {
                unsigned offset = record.jumpTable.simpleJumpTable->branchOffsets[j];
                simpleJumpTable->ctiOffsets[j] = offset
                    ? patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + offset])
                    : simpleJumpTable->ctiDefault;
            }
        } else {
            ASSERT(record.type == SwitchRecord::String);

            auto* stringJumpTable = record.jumpTable.stringJumpTable;
            stringJumpTable->ctiDefault =
                patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + record.defaultOffset]);

            for (auto& location : stringJumpTable->offsetTable.values()) {
                unsigned offset = location.branchOffset;
                location.ctiOffset = offset
                    ? patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + offset])
                    : stringJumpTable->ctiDefault;
            }
        }
    }

    for (size_t i = 0; i < m_codeBlock->numberOfExceptionHandlers(); ++i) {
        HandlerInfo& handler = m_codeBlock->exceptionHandler(i);
        // FIXME: <rdar://problem/39433318>.
        handler.nativeCode = patchBuffer.locationOf<ExceptionHandlerPtrTag>(m_labels[handler.target]);
    }

    for (auto& record : m_calls) {
        if (record.callee)
            patchBuffer.link(record.from, record.callee);
    }
    
    finalizeInlineCaches(m_getByIds, patchBuffer);
    finalizeInlineCaches(m_getByVals, patchBuffer);
    finalizeInlineCaches(m_getByIdsWithThis, patchBuffer);
    finalizeInlineCaches(m_putByIds, patchBuffer);
    finalizeInlineCaches(m_delByIds, patchBuffer);
    finalizeInlineCaches(m_delByVals, patchBuffer);
    finalizeInlineCaches(m_inByIds, patchBuffer);
    finalizeInlineCaches(m_instanceOfs, patchBuffer);

    if (m_byValCompilationInfo.size()) {
        CodeLocationLabel<ExceptionHandlerPtrTag> exceptionHandler = patchBuffer.locationOf<ExceptionHandlerPtrTag>(m_exceptionHandler);

        for (const auto& byValCompilationInfo : m_byValCompilationInfo) {
            PatchableJump patchableNotIndexJump = byValCompilationInfo.notIndexJump;
            auto notIndexJump = CodeLocationJump<JSInternalPtrTag>();
            if (Jump(patchableNotIndexJump).isSet())
                notIndexJump = CodeLocationJump<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(patchableNotIndexJump));
            auto badTypeJump = CodeLocationJump<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(byValCompilationInfo.badTypeJump));
            auto doneTarget = CodeLocationLabel<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(byValCompilationInfo.doneTarget));
            auto nextHotPathTarget = CodeLocationLabel<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(byValCompilationInfo.nextHotPathTarget));
            auto slowPathTarget = CodeLocationLabel<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(byValCompilationInfo.slowPathTarget));

            *byValCompilationInfo.byValInfo = ByValInfo(
                byValCompilationInfo.bytecodeIndex,
                notIndexJump,
                badTypeJump,
                exceptionHandler,
                byValCompilationInfo.arrayMode,
                byValCompilationInfo.arrayProfile,
                doneTarget,
                nextHotPathTarget,
                slowPathTarget);
        }
    }

    for (auto& compilationInfo : m_callCompilationInfo) {
        CallLinkInfo& info = *compilationInfo.callLinkInfo;
        info.setCallLocations(
            CodeLocationLabel<JSInternalPtrTag>(patchBuffer.locationOfNearCall<JSInternalPtrTag>(compilationInfo.callReturnLocation)),
            CodeLocationLabel<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(compilationInfo.hotPathBegin)),
            patchBuffer.locationOfNearCall<JSInternalPtrTag>(compilationInfo.hotPathOther));
    }

    {
        JITCodeMapBuilder jitCodeMapBuilder;
        for (unsigned bytecodeOffset = 0; bytecodeOffset < m_labels.size(); ++bytecodeOffset) {
            if (m_labels[bytecodeOffset].isSet())
                jitCodeMapBuilder.append(BytecodeIndex(bytecodeOffset), patchBuffer.locationOf<JSEntryPtrTag>(m_labels[bytecodeOffset]));
        }
        m_codeBlock->setJITCodeMap(jitCodeMapBuilder.finalize());
    }

    MacroAssemblerCodePtr<JSEntryPtrTag> withArityCheck = patchBuffer.locationOf<JSEntryPtrTag>(m_arityCheck);

    if (UNLIKELY(Options::dumpDisassembly())) {
        m_disassembler->dump(patchBuffer);
        patchBuffer.didAlreadyDisassemble();
    }
    if (UNLIKELY(m_compilation)) {
        if (Options::disassembleBaselineForProfiler())
            m_disassembler->reportToProfiler(m_compilation.get(), patchBuffer);
        m_vm->m_perBytecodeProfiler->addCompilation(m_codeBlock, *m_compilation);
    }

    if (m_pcToCodeOriginMapBuilder.didBuildMapping())
        m_codeBlock->setPCToCodeOriginMap(makeUnique<PCToCodeOriginMap>(WTFMove(m_pcToCodeOriginMapBuilder), patchBuffer));
    
    CodeRef<JSEntryPtrTag> result = FINALIZE_CODE(
        patchBuffer, JSEntryPtrTag,
        "Baseline JIT code for %s", toCString(CodeBlockWithJITType(m_codeBlock, JITType::BaselineJIT)).data());
    
    m_vm->machineCodeBytesPerBytecodeWordForBaselineJIT->add(
        static_cast<double>(result.size()) /
        static_cast<double>(m_codeBlock->instructionsSize()));

    {
        ConcurrentJSLocker locker(m_codeBlock->m_lock);
        m_codeBlock->shrinkToFit(locker, CodeBlock::ShrinkMode::LateShrink);
    }
    m_codeBlock->setJITCode(
        adoptRef(*new DirectJITCode(result, withArityCheck, JITType::BaselineJIT)));

    if (JITInternal::verbose)
        dataLogF("JIT generated code for %p at [%p, %p).\n", m_codeBlock, result.executableMemory()->start().untaggedPtr(), result.executableMemory()->end().untaggedPtr());

    return CompilationSuccessful;
}

CompilationResult JIT::privateCompile(JITCompilationEffort effort)
{
    doMainThreadPreparationBeforeCompile();
    compileWithoutLinking(effort);
    return link();
}

void JIT::privateCompileExceptionHandlers()
{
    if (!m_exceptionChecksWithCallFrameRollback.empty()) {
        m_exceptionChecksWithCallFrameRollback.link(this);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

        // operationLookupExceptionHandlerFromCallerFrame is passed one argument, the VM*.
        move(TrustedImmPtr(&vm()), GPRInfo::argumentGPR0);
        prepareCallOperation(vm());
        m_calls.append(CallRecord(call(OperationPtrTag), BytecodeIndex(), FunctionPtr<OperationPtrTag>(operationLookupExceptionHandlerFromCallerFrame)));
        jumpToExceptionHandler(vm());
    }

    if (!m_exceptionChecks.empty() || m_byValCompilationInfo.size()) {
        m_exceptionHandler = label();
        m_exceptionChecks.link(this);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

        // operationLookupExceptionHandler is passed one argument, the VM*.
        move(TrustedImmPtr(&vm()), GPRInfo::argumentGPR0);
        prepareCallOperation(vm());
        m_calls.append(CallRecord(call(OperationPtrTag), BytecodeIndex(), FunctionPtr<OperationPtrTag>(operationLookupExceptionHandler)));
        jumpToExceptionHandler(vm());
    }
}

void JIT::doMainThreadPreparationBeforeCompile()
{
    // This ensures that we have the most up to date type information when performing typecheck optimizations for op_profile_type.
    if (m_vm->typeProfiler())
        m_vm->typeProfilerLog()->processLogEntries(*m_vm, "Preparing for JIT compilation."_s);
}

unsigned JIT::frameRegisterCountFor(CodeBlock* codeBlock)
{
    ASSERT(static_cast<unsigned>(codeBlock->numCalleeLocals()) == WTF::roundUpToMultipleOf(stackAlignmentRegisters(), static_cast<unsigned>(codeBlock->numCalleeLocals())));

    return roundLocalRegisterCountForFramePointerOffset(codeBlock->numCalleeLocals() + maxFrameExtentForSlowPathCallInRegisters);
}

int JIT::stackPointerOffsetFor(CodeBlock* codeBlock)
{
    return virtualRegisterForLocal(frameRegisterCountFor(codeBlock) - 1).offset();
}

bool JIT::reportCompileTimes()
{
    return Options::reportCompileTimes() || Options::reportBaselineCompileTimes();
}

bool JIT::computeCompileTimes()
{
    return reportCompileTimes() || Options::reportTotalCompileTimes();
}

HashMap<CString, Seconds> JIT::compileTimeStats()
{
    HashMap<CString, Seconds> result;
    if (Options::reportTotalCompileTimes()) {
        result.add("Total Compile Time", totalBaselineCompileTime + totalDFGCompileTime + totalFTLCompileTime);
        result.add("Baseline Compile Time", totalBaselineCompileTime);
#if ENABLE(DFG_JIT)
        result.add("DFG Compile Time", totalDFGCompileTime);
#if ENABLE(FTL_JIT)
        result.add("FTL Compile Time", totalFTLCompileTime);
        result.add("FTL (DFG) Compile Time", totalFTLDFGCompileTime);
        result.add("FTL (B3) Compile Time", totalFTLB3CompileTime);
#endif // ENABLE(FTL_JIT)
#endif // ENABLE(DFG_JIT)
    }
    return result;
}

Seconds JIT::totalCompileTime()
{
    return totalBaselineCompileTime + totalDFGCompileTime + totalFTLCompileTime;
}

} // namespace JSC

#endif // ENABLE(JIT)
