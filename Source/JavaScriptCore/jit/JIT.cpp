/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "JITSizeStatistics.h"
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

#if ENABLE(EXTRA_CTI_THUNKS)
#if CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS))
// These are supported ports.
#else
// This is a courtesy reminder (and warning) that the implementation of EXTRA_CTI_THUNKS can
// use up to 6 argument registers and/or 6/7 temp registers, and make use of ARM64 like
// features. Hence, it may not work for many other ports without significant work. If you
// plan on adding EXTRA_CTI_THUNKS support for your port, please remember to search the
// EXTRA_CTI_THUNKS code for CPU(ARM64) and CPU(X86_64) conditional code, and add support
// for your port there as well.
#error "unsupported architecture"
#endif
#endif // ENABLE(EXTRA_CTI_THUNKS)

Seconds totalBaselineCompileTime;
Seconds totalDFGCompileTime;
Seconds totalFTLCompileTime;
Seconds totalFTLDFGCompileTime;
Seconds totalFTLB3CompileTime;

void ctiPatchCallByReturnAddress(ReturnAddressPtr returnAddress, FunctionPtr<CFunctionPtrTag> newCalleeFunction)
{
    MacroAssembler::repatchCall(
        CodeLocationCall<ReturnAddressPtrTag>(MacroAssemblerCodePtr<ReturnAddressPtrTag>(returnAddress)),
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

#if ENABLE(DFG_JIT) && !ENABLE(EXTRA_CTI_THUNKS)
void JIT::emitEnterOptimizationCheck()
{
    if (!canBeOptimized())
        return;

    JumpList skipOptimize;
    
    skipOptimize.append(branchAdd32(Signed, TrustedImm32(Options::executionCounterIncrementForEntry()), AbsoluteAddress(m_codeBlock->addressOfJITExecuteCounter())));
    ASSERT(!m_bytecodeIndex.offset());

    copyLLIntBaselineCalleeSavesFromFrameOrRegisterToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

    callOperationNoExceptionCheck(operationOptimize, &vm(), m_bytecodeIndex.asBits());
    skipOptimize.append(branchTestPtr(Zero, returnValueGPR));
    farJump(returnValueGPR, GPRInfo::callFrameRegister);
    skipOptimize.link(this);
}
#endif // ENABLE(DFG_JIT) && !ENABLE(EXTRA_CTI_THUNKS)(

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

void JIT::emitVarReadOnlyCheck(ResolveType resolveType)
{
    if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks)
        addSlowCase(branch8(Equal, AbsoluteAddress(m_codeBlock->globalObject()->varReadOnlyWatchpoint()->addressOfState()), TrustedImm32(IsInvalidated)));
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

        m_labels[m_bytecodeIndex.offset()] = label();

        if (JITInternal::verbose)
            dataLogLn("Baseline JIT emitting code for ", m_bytecodeIndex, " at offset ", (long)debugOffset());

        OpcodeID opcodeID = currentInstruction->opcodeID();

        std::optional<JITSizeStatistics::Marker> sizeMarker;
        if (UNLIKELY(m_bytecodeIndex >= startBytecodeIndex && Options::dumpBaselineJITSizeStatistics())) {
            String id = makeString("Baseline_fast_", opcodeNames[opcodeID]);
            sizeMarker = m_vm->jitSizeStatistics->markStart(id, *this);
        }

        if (UNLIKELY(m_compilation)) {
            add64(
                TrustedImm32(1),
                AbsoluteAddress(m_compilation->executionCounterFor(Profiler::OriginStack(Profiler::Origin(
                    m_compilation->bytecodes(), m_bytecodeIndex)))->address()));
        }
        
        if (Options::eagerlyUpdateTopCallFrame())
            updateTopCallFrame();

        unsigned bytecodeOffset = m_bytecodeIndex.offset();
        if (UNLIKELY(Options::traceBaselineJITExecution())) {
            CodeBlock* codeBlock = m_codeBlock;
            probeDebug([=] (Probe::Context& ctx) {
                dataLogLn("JIT [", bytecodeOffset, "] ", opcodeNames[opcodeID], " cfr ", RawPointer(ctx.fp()), " @ ", codeBlock);
            });
        }

        switch (opcodeID) {
        DEFINE_SLOW_OP(has_private_name)
        DEFINE_SLOW_OP(has_private_brand)
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
        DEFINE_SLOW_OP(has_enumerable_property)
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
        DEFINE_OP(op_in_by_val)
        DEFINE_OP(op_get_by_id)
        DEFINE_OP(op_get_by_id_with_this)
        DEFINE_OP(op_get_by_id_direct)
        DEFINE_OP(op_get_by_val)
        DEFINE_OP(op_get_private_name)
        DEFINE_OP(op_set_private_brand)
        DEFINE_OP(op_check_private_brand)
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
        DEFINE_OP(op_put_private_name)
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

        DEFINE_OP(op_has_enumerable_indexed_property)
        DEFINE_OP(op_has_enumerable_structure_property)
        DEFINE_OP(op_has_own_structure_property)
        DEFINE_OP(op_in_structure_property)
        DEFINE_OP(op_get_direct_pname)
        DEFINE_OP(op_enumerator_structure_pname)
        DEFINE_OP(op_enumerator_generic_pname)
            
        DEFINE_OP(op_log_shadow_chicken_prologue)
        DEFINE_OP(op_log_shadow_chicken_tail)

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (UNLIKELY(sizeMarker))
            m_vm->jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this);

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
    m_inByValIndex = 0;
    m_delByIdIndex = 0;
    m_delByValIndex = 0;
    m_instanceOfIndex = 0;
    m_privateBrandAccessIndex = 0;
    m_byValInstructionIndex = 0;
    m_callLinkInfoIndex = 0;

    unsigned bytecodeCountHavingSlowCase = 0;
    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end();) {
        m_bytecodeIndex = iter->to;

        m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(m_bytecodeIndex));

        BytecodeIndex firstTo = m_bytecodeIndex;

        const Instruction* currentInstruction = m_codeBlock->instructions().at(m_bytecodeIndex).ptr();
        
        if (JITInternal::verbose)
            dataLogLn("Baseline JIT emitting slow code for ", m_bytecodeIndex, " at offset ", (long)debugOffset());

        if (m_disassembler)
            m_disassembler->setForBytecodeSlowPath(m_bytecodeIndex.offset(), label());

        OpcodeID opcodeID = currentInstruction->opcodeID();

        std::optional<JITSizeStatistics::Marker> sizeMarker;
        if (UNLIKELY(Options::dumpBaselineJITSizeStatistics())) {
            String id = makeString("Baseline_slow_", opcodeNames[opcodeID]);
            sizeMarker = m_vm->jitSizeStatistics->markStart(id, *this);
        }

        if (UNLIKELY(Options::traceBaselineJITExecution())) {
            unsigned bytecodeOffset = m_bytecodeIndex.offset();
            CodeBlock* codeBlock = m_codeBlock;
            probeDebug([=] (Probe::Context& ctx) {
                dataLogLn("JIT [", bytecodeOffset, "] SLOW ", opcodeNames[opcodeID], " cfr ", RawPointer(ctx.fp()), " @ ", codeBlock);
            });
        }

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
        DEFINE_SLOWCASE_OP(op_in_by_val)
        DEFINE_SLOWCASE_OP(op_get_by_id)
        DEFINE_SLOWCASE_OP(op_get_by_id_with_this)
        DEFINE_SLOWCASE_OP(op_get_by_id_direct)
        DEFINE_SLOWCASE_OP(op_get_by_val)
        DEFINE_SLOWCASE_OP(op_get_private_name)
        DEFINE_SLOWCASE_OP(op_set_private_brand)
        DEFINE_SLOWCASE_OP(op_check_private_brand)
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
        DEFINE_SLOWCASE_OP(op_put_private_name)
        DEFINE_SLOWCASE_OP(op_del_by_val)
        DEFINE_SLOWCASE_OP(op_del_by_id)
        DEFINE_SLOWCASE_OP(op_sub)
        DEFINE_SLOWCASE_OP(op_has_enumerable_indexed_property)
#if !ENABLE(EXTRA_CTI_THUNKS)
        DEFINE_SLOWCASE_OP(op_get_from_scope)
#endif
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
        DEFINE_SLOWCASE_SLOW_OP(has_enumerable_structure_property)
        DEFINE_SLOWCASE_SLOW_OP(has_own_structure_property)
        DEFINE_SLOWCASE_SLOW_OP(in_structure_property)
#if !ENABLE(EXTRA_CTI_THUNKS)
        DEFINE_SLOWCASE_SLOW_OP(resolve_scope)
#endif
        DEFINE_SLOWCASE_SLOW_OP(check_tdz)
        DEFINE_SLOWCASE_SLOW_OP(to_property_key)
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (JITInternal::verbose)
            dataLog("At ", firstTo, " slow: ", iter - m_slowCases.begin(), "\n");

        RELEASE_ASSERT_WITH_MESSAGE(iter == m_slowCases.end() || firstTo.offset() != iter->to.offset(), "Not enough jumps linked in slow case codegen.");
        RELEASE_ASSERT_WITH_MESSAGE(firstTo.offset() == (iter - 1)->to.offset(), "Too many jumps linked in slow case codegen.");
        
        emitJumpSlowToHot(jump(), 0);
        ++bytecodeCountHavingSlowCase;

        if (UNLIKELY(sizeMarker))
            m_vm->jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this);
    }

    RELEASE_ASSERT(bytecodeCountHavingSlowCase == m_bytecodeCountHavingSlowCase);
    RELEASE_ASSERT(m_getByIdIndex == m_getByIds.size());
    RELEASE_ASSERT(m_getByIdWithThisIndex == m_getByIdsWithThis.size());
    RELEASE_ASSERT(m_putByIdIndex == m_putByIds.size());
    RELEASE_ASSERT(m_inByIdIndex == m_inByIds.size());
    RELEASE_ASSERT(m_instanceOfIndex == m_instanceOfs.size());
    RELEASE_ASSERT(m_privateBrandAccessIndex == m_privateBrandAccesses.size());
    RELEASE_ASSERT(m_callLinkInfoIndex == m_callCompilationInfo.size());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeIndex = BytecodeIndex();
#endif
}

static inline unsigned prologueGeneratorSelector(bool doesProfiling, bool isConstructor, bool hasHugeFrame)
{
    return doesProfiling << 2 | isConstructor << 1 | hasHugeFrame << 0;
}

#define FOR_EACH_NON_PROFILING_PROLOGUE_GENERATOR(v) \
    v(!doesProfiling, !isConstructor, !hasHugeFrame, prologueGenerator0, arityFixup_prologueGenerator0) \
    v(!doesProfiling, !isConstructor,  hasHugeFrame, prologueGenerator1, arityFixup_prologueGenerator1) \
    v(!doesProfiling,  isConstructor, !hasHugeFrame, prologueGenerator2, arityFixup_prologueGenerator2) \
    v(!doesProfiling,  isConstructor,  hasHugeFrame, prologueGenerator3, arityFixup_prologueGenerator3)

#if ENABLE(DFG_JIT)
#define FOR_EACH_PROFILING_PROLOGUE_GENERATOR(v) \
    v( doesProfiling, !isConstructor, !hasHugeFrame, prologueGenerator4, arityFixup_prologueGenerator4) \
    v( doesProfiling, !isConstructor,  hasHugeFrame, prologueGenerator5, arityFixup_prologueGenerator5) \
    v( doesProfiling,  isConstructor, !hasHugeFrame, prologueGenerator6, arityFixup_prologueGenerator6) \
    v( doesProfiling,  isConstructor,  hasHugeFrame, prologueGenerator7, arityFixup_prologueGenerator7)

#else // not ENABLE(DFG_JIT)
#define FOR_EACH_PROFILING_PROLOGUE_GENERATOR(v)
#endif // ENABLE(DFG_JIT)

#define FOR_EACH_PROLOGUE_GENERATOR(v) \
    FOR_EACH_NON_PROFILING_PROLOGUE_GENERATOR(v) \
    FOR_EACH_PROFILING_PROLOGUE_GENERATOR(v)

void JIT::compileAndLinkWithoutFinalizing(JITCompilationEffort effort)
{
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

    if (m_codeBlock->numberOfUnlinkedSwitchJumpTables() || m_codeBlock->numberOfUnlinkedStringSwitchJumpTables()) {
        ConcurrentJSLocker locker(m_codeBlock->m_lock);
        if (m_codeBlock->numberOfUnlinkedSwitchJumpTables())
            m_codeBlock->ensureJITData(locker).m_switchJumpTables = FixedVector<SimpleJumpTable>(m_codeBlock->numberOfUnlinkedSwitchJumpTables());
        if (m_codeBlock->numberOfUnlinkedStringSwitchJumpTables())
            m_codeBlock->ensureJITData(locker).m_stringSwitchJumpTables = FixedVector<StringJumpTable>(m_codeBlock->numberOfUnlinkedStringSwitchJumpTables());
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

    std::optional<JITSizeStatistics::Marker> sizeMarker;
    if (UNLIKELY(Options::dumpBaselineJITSizeStatistics())) {
        String id = makeString("Baseline_prologue");
        sizeMarker = m_vm->jitSizeStatistics->markStart(id, *this);
    }

    Label entryLabel(this);
    if (m_disassembler)
        m_disassembler->setStartOfCode(entryLabel);

    // Just add a little bit of randomness to the codegen
    if (random() & 1)
        nop();

    emitFunctionPrologue();

#if !ENABLE(EXTRA_CTI_THUNKS)
    emitPutToCallFrameHeader(m_codeBlock, CallFrameSlot::codeBlock);

    Label beginLabel(this);

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
            // If this is a constructor, then we want to put in a dummy profiling site (to
            // keep things consistent) but we don't actually want to record the dummy value.
            unsigned startArgument = m_codeBlock->isConstructor() ? 1 : 0;
            for (unsigned argument = startArgument; argument < m_codeBlock->numParameters(); ++argument) {
                int offset = CallFrame::argumentOffsetIncludingThis(argument) * static_cast<int>(sizeof(Register));
#if USE(JSVALUE64)
                JSValueRegs resultRegs = JSValueRegs(regT0);
                load64(Address(callFrameRegister, offset), resultRegs.payloadGPR());
#elif USE(JSVALUE32_64)
                JSValueRegs resultRegs = JSValueRegs(regT1, regT0);
                load32(Address(callFrameRegister, offset + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), resultRegs.payloadGPR());
                load32(Address(callFrameRegister, offset + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), resultRegs.tagGPR());
#endif
                emitValueProfilingSite(m_codeBlock->valueProfileForArgument(argument), resultRegs);
            }
        }
    }
#else // ENABLE(EXTRA_CTI_THUNKS)
    constexpr GPRReg codeBlockGPR = regT7;
    ASSERT(!m_bytecodeIndex);

    int frameTopOffset = stackPointerOffsetFor(m_codeBlock) * sizeof(Register);
    unsigned maxFrameSize = -frameTopOffset;

    bool doesProfiling = (m_codeBlock->codeType() == FunctionCode) && shouldEmitProfiling();
    bool isConstructor = m_codeBlock->isConstructor();
    bool hasHugeFrame = maxFrameSize > Options::reservedZoneSize();

    static constexpr ThunkGenerator generators[] = {
#define USE_PROLOGUE_GENERATOR(doesProfiling, isConstructor, hasHugeFrame, name, arityFixupName) name,
        FOR_EACH_PROLOGUE_GENERATOR(USE_PROLOGUE_GENERATOR)
#undef USE_PROLOGUE_GENERATOR
    };
    static constexpr unsigned numberOfGenerators = sizeof(generators) / sizeof(generators[0]);

    move(TrustedImmPtr(m_codeBlock), codeBlockGPR);

    unsigned generatorSelector = prologueGeneratorSelector(doesProfiling, isConstructor, hasHugeFrame);
    RELEASE_ASSERT(generatorSelector < numberOfGenerators);
    auto generator = generators[generatorSelector];
    emitNakedNearCall(vm().getCTIStub(generator).retaggedCode<NoPtrTag>());

    Label bodyLabel(this);
#endif // !ENABLE(EXTRA_CTI_THUNKS)

    RELEASE_ASSERT(!JITCode::isJIT(m_codeBlock->jitType()));

    if (UNLIKELY(sizeMarker))
        m_vm->jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this);

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();
    
    if (m_disassembler)
        m_disassembler->setEndOfSlowPath(label());
    m_pcToCodeOriginMapBuilder.appendItem(label(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

#if !ENABLE(EXTRA_CTI_THUNKS)
    stackOverflow.link(this);
    m_bytecodeIndex = BytecodeIndex(0);
    if (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);
    callOperationWithCallFrameRollbackOnException(operationThrowStackOverflowError, m_codeBlock);
#endif

    // If the number of parameters is 1, we never require arity fixup.
    bool requiresArityFixup = m_codeBlock->m_numParameters != 1;
    if (m_codeBlock->codeType() == FunctionCode && requiresArityFixup) {
        m_arityCheck = label();
#if !ENABLE(EXTRA_CTI_THUNKS)
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
        emitNakedNearCall(m_vm->getCTIStub(arityFixupGenerator).retaggedCode<NoPtrTag>());

        jump(beginLabel);

#else // ENABLE(EXTRA_CTI_THUNKS)
        emitFunctionPrologue();

        static_assert(codeBlockGPR == regT7);
        ASSERT(!m_bytecodeIndex);

        static constexpr ThunkGenerator generators[] = {
#define USE_PROLOGUE_GENERATOR(doesProfiling, isConstructor, hasHugeFrame, name, arityFixupName) arityFixupName,
            FOR_EACH_PROLOGUE_GENERATOR(USE_PROLOGUE_GENERATOR)
#undef USE_PROLOGUE_GENERATOR
        };
        static constexpr unsigned numberOfGenerators = sizeof(generators) / sizeof(generators[0]);

        move(TrustedImmPtr(m_codeBlock), codeBlockGPR);

        RELEASE_ASSERT(generatorSelector < numberOfGenerators);
        auto generator = generators[generatorSelector];
        RELEASE_ASSERT(generator);
        emitNakedNearCall(vm().getCTIStub(generator).retaggedCode<NoPtrTag>());

        jump(bodyLabel);
#endif // !ENABLE(EXTRA_CTI_THUNKS)

#if ASSERT_ENABLED
        m_bytecodeIndex = BytecodeIndex(); // Reset this, in order to guard its use with ASSERTs.
#endif
    } else
        m_arityCheck = entryLabel; // Never require arity fixup.

    ASSERT(m_jmpTable.isEmpty());
    
#if !ENABLE(EXTRA_CTI_THUNKS)
    privateCompileExceptionHandlers();
#endif
    
    if (m_disassembler)
        m_disassembler->setEndOfCode(label());
    m_pcToCodeOriginMapBuilder.appendItem(label(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

    m_linkBuffer = std::unique_ptr<LinkBuffer>(new LinkBuffer(*this, m_codeBlock, LinkBuffer::Profile::BaselineJIT, effort));
    link();
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::prologueGenerator(VM& vm, bool doesProfiling, bool isConstructor, bool hasHugeFrame, const char* thunkName)
{
    // This function generates the Baseline JIT's prologue code. It is not useable by other tiers.
    constexpr GPRReg codeBlockGPR = regT7; // incoming.

    constexpr int virtualRegisterSize = static_cast<int>(sizeof(Register));
    constexpr int virtualRegisterSizeShift = 3;
    static_assert((1 << virtualRegisterSizeShift) == virtualRegisterSize);

    tagReturnAddress();

    storePtr(codeBlockGPR, addressFor(CallFrameSlot::codeBlock));

    load32(Address(codeBlockGPR, CodeBlock::offsetOfNumCalleeLocals()), regT1);
    if constexpr (maxFrameExtentForSlowPathCallInRegisters)
        add32(TrustedImm32(maxFrameExtentForSlowPathCallInRegisters), regT1);
    lshift32(TrustedImm32(virtualRegisterSizeShift), regT1);
    neg64(regT1);
#if ASSERT_ENABLED
    Probe::Function probeFunction = [] (Probe::Context& context) {
        CodeBlock* codeBlock = context.fp<CallFrame*>()->codeBlock();
        int64_t frameTopOffset = stackPointerOffsetFor(codeBlock) * sizeof(Register);
        RELEASE_ASSERT(context.gpr<intptr_t>(regT1) == frameTopOffset);
    };
    probe(tagCFunctionPtr<JITProbePtrTag>(probeFunction), nullptr);
#endif

    addPtr(callFrameRegister, regT1);

    JumpList stackOverflow;
    if (hasHugeFrame)
        stackOverflow.append(branchPtr(Above, regT1, callFrameRegister));
    stackOverflow.append(branchPtr(Above, AbsoluteAddress(vm.addressOfSoftStackLimit()), regT1));

    // We'll be imminently returning with a `retab` (ARM64E's return with authentication
    // using the B key) in the normal path (see MacroAssemblerARM64E's implementation of
    // ret()), which will do validation. So, extra validation here is redundant and unnecessary.
    untagReturnAddressWithoutExtraValidation();
#if CPU(X86_64)
    pop(regT2); // Save the return address.
#endif
    move(regT1, stackPointerRegister);
    tagReturnAddress();
    checkStackPointerAlignment();
#if CPU(X86_64)
    push(regT2); // Restore the return address.
#endif

    emitSaveCalleeSavesForBaselineJIT();
    emitMaterializeTagCheckRegisters();

    if (doesProfiling) {
        constexpr GPRReg argumentValueProfileGPR = regT6;
        constexpr GPRReg numParametersGPR = regT5;
        constexpr GPRReg argumentGPR = regT4;

        load32(Address(codeBlockGPR, CodeBlock::offsetOfNumParameters()), numParametersGPR);
        loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfArgumentValueProfiles()), argumentValueProfileGPR);
        if (isConstructor)
            addPtr(TrustedImm32(sizeof(ValueProfile)), argumentValueProfileGPR);

        int startArgument = CallFrameSlot::thisArgument + (isConstructor ? 1 : 0);
        int startArgumentOffset = startArgument * virtualRegisterSize;
        move(TrustedImm64(startArgumentOffset), argumentGPR);

        add32(TrustedImm32(static_cast<int>(CallFrameSlot::thisArgument)), numParametersGPR);
        lshift32(TrustedImm32(virtualRegisterSizeShift), numParametersGPR);

        addPtr(callFrameRegister, argumentGPR);
        addPtr(callFrameRegister, numParametersGPR);

        Label loopStart(this);
        Jump done = branchPtr(AboveOrEqual, argumentGPR, numParametersGPR);
        {
            load64(Address(argumentGPR), regT0);
            store64(regT0, Address(argumentValueProfileGPR, OBJECT_OFFSETOF(ValueProfile, m_buckets)));

            // The argument ValueProfiles are stored in a FixedVector. Hence, the
            // address of the next profile can be trivially computed with an increment.
            addPtr(TrustedImm32(sizeof(ValueProfile)), argumentValueProfileGPR);
            addPtr(TrustedImm32(virtualRegisterSize), argumentGPR);
            jump().linkTo(loopStart, this);
        }
        done.link(this);
    }
    ret();

    stackOverflow.link(this);
#if CPU(X86_64)
    addPtr(TrustedImm32(1 * sizeof(CPURegister)), stackPointerRegister); // discard return address.
#endif

    uint32_t locationBits = CallSiteIndex(0).bits();
    store32(TrustedImm32(locationBits), tagFor(CallFrameSlot::argumentCountIncludingThis));

    if (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);

    setupArguments<decltype(operationThrowStackOverflowError)>(codeBlockGPR);
    prepareCallOperation(vm);
    MacroAssembler::Call operationCall = call(OperationPtrTag);
    Jump handleExceptionJump = jump();

    auto handler = vm.getCTIStub(handleExceptionWithCallFrameRollbackGenerator);

    LinkBuffer patchBuffer(*this, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(operationCall, FunctionPtr<OperationPtrTag>(operationThrowStackOverflowError));
    patchBuffer.link(handleExceptionJump, CodeLocationLabel(handler.retaggedCode<NoPtrTag>()));
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, thunkName);
}

static constexpr bool doesProfiling = true;
static constexpr bool isConstructor = true;
static constexpr bool hasHugeFrame = true;

#define DEFINE_PROGLOGUE_GENERATOR(doesProfiling, isConstructor, hasHugeFrame, name, arityFixupName) \
    MacroAssemblerCodeRef<JITThunkPtrTag> JIT::name(VM& vm) \
    { \
        JIT jit(vm); \
        return jit.prologueGenerator(vm, doesProfiling, isConstructor, hasHugeFrame, "Baseline: " #name); \
    }

FOR_EACH_PROLOGUE_GENERATOR(DEFINE_PROGLOGUE_GENERATOR)
#undef DEFINE_PROGLOGUE_GENERATOR

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::arityFixupPrologueGenerator(VM& vm, bool isConstructor, ThunkGenerator normalPrologueGenerator, const char* thunkName)
{
    // This function generates the Baseline JIT's prologue code. It is not useable by other tiers.
    constexpr GPRReg codeBlockGPR = regT7; // incoming.
    constexpr GPRReg numParametersGPR = regT6;

    tagReturnAddress();
#if CPU(X86_64)
    push(framePointerRegister);
#elif CPU(ARM64)
    pushPair(framePointerRegister, linkRegister);
#endif

    storePtr(codeBlockGPR, addressFor(CallFrameSlot::codeBlock));
    store8(TrustedImm32(0), Address(codeBlockGPR, CodeBlock::offsetOfShouldAlwaysBeInlined()));

    load32(payloadFor(CallFrameSlot::argumentCountIncludingThis), regT1);
    load32(Address(codeBlockGPR, CodeBlock::offsetOfNumParameters()), numParametersGPR);
    Jump noFixupNeeded = branch32(AboveOrEqual, regT1, numParametersGPR);

    if constexpr (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);

    loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfGlobalObject()), argumentGPR0);

    static_assert(std::is_same<decltype(operationConstructArityCheck), decltype(operationCallArityCheck)>::value);
    setupArguments<decltype(operationCallArityCheck)>(argumentGPR0);
    prepareCallOperation(vm);

    MacroAssembler::Call arityCheckCall = call(OperationPtrTag);
    Jump handleExceptionJump = emitNonPatchableExceptionCheck(vm);

    if constexpr (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(maxFrameExtentForSlowPathCall), stackPointerRegister);
    Jump needFixup = branchTest32(NonZero, returnValueGPR);
    noFixupNeeded.link(this);

    // The normal prologue expects incoming codeBlockGPR.
    load64(addressFor(CallFrameSlot::codeBlock), codeBlockGPR);

#if CPU(X86_64)
    pop(framePointerRegister);
#elif CPU(ARM64)
    popPair(framePointerRegister, linkRegister);
#endif
    untagReturnAddress();

    JumpList normalPrologueJump;
    normalPrologueJump.append(jump());

    needFixup.link(this);

    // Restore the stack for arity fixup, and preserve the return address.
    // arityFixupGenerator will be shifting the stack. So, we can't use the stack to
    // preserve the return address. We also can't use callee saved registers because
    // they haven't been saved yet.
    //
    // arityFixupGenerator is carefully crafted to only use a0, a1, a2, t3, t4 and t5.
    // So, the return address can be preserved in regT7.
#if CPU(X86_64)
    pop(argumentGPR2); // discard.
    pop(regT7); // save return address.
#elif CPU(ARM64)
    popPair(framePointerRegister, linkRegister);
    untagReturnAddress();
    move(linkRegister, regT7);
    auto randomReturnAddressTag = random();
    move(TrustedImm32(randomReturnAddressTag), regT1);
    tagPtr(regT1, regT7);
#endif
    move(returnValueGPR, GPRInfo::argumentGPR0);
    Call arityFixupCall = nearCall();

#if CPU(X86_64)
    push(regT7); // restore return address.
#elif CPU(ARM64)
    move(TrustedImm32(randomReturnAddressTag), regT1);
    untagPtr(regT1, regT7);
    move(regT7, linkRegister);
#endif

    load64(addressFor(CallFrameSlot::codeBlock), codeBlockGPR);
    normalPrologueJump.append(jump());

    auto arityCheckOperation = isConstructor ? operationConstructArityCheck : operationCallArityCheck;
    auto arityFixup = vm.getCTIStub(arityFixupGenerator);
    auto normalPrologue = vm.getCTIStub(normalPrologueGenerator);
    auto exceptionHandler = vm.getCTIStub(popThunkStackPreservesAndHandleExceptionGenerator);

    LinkBuffer patchBuffer(*this, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(arityCheckCall, FunctionPtr<OperationPtrTag>(arityCheckOperation));
    patchBuffer.link(arityFixupCall, FunctionPtr(arityFixup.retaggedCode<NoPtrTag>()));
    patchBuffer.link(normalPrologueJump, CodeLocationLabel(normalPrologue.retaggedCode<NoPtrTag>()));
    patchBuffer.link(handleExceptionJump, CodeLocationLabel(exceptionHandler.retaggedCode<NoPtrTag>()));
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, thunkName);
}

#define DEFINE_ARITY_PROGLOGUE_GENERATOR(doesProfiling, isConstructor, hasHugeFrame, name, arityFixupName) \
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::arityFixupName(VM& vm) \
    { \
        JIT jit(vm); \
        return jit.arityFixupPrologueGenerator(vm, isConstructor, name, "Baseline: " #arityFixupName); \
    }

FOR_EACH_PROLOGUE_GENERATOR(DEFINE_ARITY_PROGLOGUE_GENERATOR)
#undef DEFINE_ARITY_PROGLOGUE_GENERATOR

#endif // ENABLE(EXTRA_CTI_THUNKS)

void JIT::link()
{
    LinkBuffer& patchBuffer = *m_linkBuffer;
    
    if (patchBuffer.didFailToAllocate())
        return;

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (auto& record : m_switches) {
        unsigned bytecodeOffset = record.bytecodeIndex.offset();
        unsigned tableIndex = record.tableIndex;

        switch (record.type) {
        case SwitchRecord::Immediate:
        case SwitchRecord::Character: {
            const UnlinkedSimpleJumpTable& unlinkedTable = m_codeBlock->unlinkedSwitchJumpTable(tableIndex);
            SimpleJumpTable& linkedTable = m_codeBlock->switchJumpTable(tableIndex);
            linkedTable.m_ctiDefault = patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + record.defaultOffset]);
            for (unsigned j = 0; j < unlinkedTable.m_branchOffsets.size(); ++j) {
                unsigned offset = unlinkedTable.m_branchOffsets[j];
                linkedTable.m_ctiOffsets[j] = offset
                    ? patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + offset])
                    : linkedTable.m_ctiDefault;
            }
            break;
        }

        case SwitchRecord::String: {
            const UnlinkedStringJumpTable& unlinkedTable = m_codeBlock->unlinkedStringSwitchJumpTable(tableIndex);
            StringJumpTable& linkedTable = m_codeBlock->stringSwitchJumpTable(tableIndex);
            auto ctiDefault = patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + record.defaultOffset]);
            for (auto& location : unlinkedTable.m_offsetTable.values()) {
                unsigned offset = location.m_branchOffset;
                linkedTable.m_ctiOffsets[location.m_indexInTable] = offset
                    ? patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + offset])
                    : ctiDefault;
            }
            linkedTable.m_ctiOffsets[unlinkedTable.m_offsetTable.size()] = ctiDefault;
            break;
        }
        }
    }

#if ENABLE(EXTRA_CTI_THUNKS)
    if (!m_exceptionChecks.empty())
        patchBuffer.link(m_exceptionChecks, CodeLocationLabel(vm().getCTIStub(handleExceptionGenerator).retaggedCode<NoPtrTag>()));
    if (!m_exceptionChecksWithCallFrameRollback.empty())
        patchBuffer.link(m_exceptionChecksWithCallFrameRollback, CodeLocationLabel(vm().getCTIStub(handleExceptionWithCallFrameRollbackGenerator).retaggedCode<NoPtrTag>()));
#endif

    for (auto& record : m_nearJumps) {
        if (record.target)
            patchBuffer.link(record.from, record.target);
    }
    for (auto& record : m_nearCalls) {
        if (record.callee)
            patchBuffer.link(record.from, record.callee);
    }
    for (auto& record : m_farCalls) {
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
    finalizeInlineCaches(m_inByVals, patchBuffer);
    finalizeInlineCaches(m_instanceOfs, patchBuffer);
    finalizeInlineCaches(m_privateBrandAccesses, patchBuffer);

    if (m_byValCompilationInfo.size()) {
#if ENABLE(EXTRA_CTI_THUNKS)
        CodeLocationLabel exceptionHandler(vm().getCTIStub(handleExceptionGenerator).retaggedCode<ExceptionHandlerPtrTag>());
#else
        CodeLocationLabel<ExceptionHandlerPtrTag> exceptionHandler = patchBuffer.locationOf<ExceptionHandlerPtrTag>(m_exceptionHandler);
#endif

        for (const auto& byValCompilationInfo : m_byValCompilationInfo) {
            PatchableJump patchableNotIndexJump = byValCompilationInfo.notIndexJump;
            CodeLocationJump<JSInternalPtrTag> notIndexJump;
            if (Jump(patchableNotIndexJump).isSet())
                notIndexJump = CodeLocationJump<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(patchableNotIndexJump));

            PatchableJump patchableBadTypeJump = byValCompilationInfo.badTypeJump;
            CodeLocationJump<JSInternalPtrTag> badTypeJump;
            if (Jump(patchableBadTypeJump).isSet())
                badTypeJump = CodeLocationJump<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(byValCompilationInfo.badTypeJump));

            auto doneTarget = CodeLocationLabel<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(byValCompilationInfo.doneTarget));
            auto nextHotPathTarget = CodeLocationLabel<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(byValCompilationInfo.nextHotPathTarget));
            auto slowPathTarget = CodeLocationLabel<JSInternalPtrTag>(patchBuffer.locationOf<JSInternalPtrTag>(byValCompilationInfo.slowPathTarget));

            byValCompilationInfo.byValInfo->setUp(
                exceptionHandler,
                byValCompilationInfo.arrayMode,
                byValCompilationInfo.arrayProfile,
                doneTarget,
                nextHotPathTarget,
                slowPathTarget);
            if (JITCode::useDataIC(JITType::BaselineJIT)) {
                byValCompilationInfo.byValInfo->m_notIndexJumpTarget = slowPathTarget.retagged<JITStubRoutinePtrTag>();
                byValCompilationInfo.byValInfo->m_badTypeJumpTarget = slowPathTarget.retagged<JITStubRoutinePtrTag>();
            } else {
                byValCompilationInfo.byValInfo->m_notIndexJump = notIndexJump;
                byValCompilationInfo.byValInfo->m_badTypeJump = badTypeJump;
            }
        }
    }

    for (auto& compilationInfo : m_callCompilationInfo) {
        CallLinkInfo& info = *compilationInfo.callLinkInfo;
        info.setCodeLocations(
            patchBuffer.locationOf<JSInternalPtrTag>(compilationInfo.slowPathStart),
            patchBuffer.locationOf<JSInternalPtrTag>(compilationInfo.doneLocation));
    }

    {
        JITCodeMapBuilder jitCodeMapBuilder;
        for (unsigned bytecodeOffset = 0; bytecodeOffset < m_labels.size(); ++bytecodeOffset) {
            if (m_labels[bytecodeOffset].isSet())
                jitCodeMapBuilder.append(BytecodeIndex(bytecodeOffset), patchBuffer.locationOf<JSEntryPtrTag>(m_labels[bytecodeOffset]));
        }
        m_codeBlock->setJITCodeMap(jitCodeMapBuilder.finalize());
    }

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
        m_pcToCodeOriginMap = makeUnique<PCToCodeOriginMap>(WTFMove(m_pcToCodeOriginMapBuilder), patchBuffer);
    
    CodeRef<JSEntryPtrTag> result = FINALIZE_CODE(
        patchBuffer, JSEntryPtrTag,
        "Baseline JIT code for %s", toCString(CodeBlockWithJITType(m_codeBlock, JITType::BaselineJIT)).data());
    
    MacroAssemblerCodePtr<JSEntryPtrTag> withArityCheck = patchBuffer.locationOf<JSEntryPtrTag>(m_arityCheck);
    m_jitCode = adoptRef(*new DirectJITCode(result, withArityCheck, JITType::BaselineJIT));

    if (JITInternal::verbose)
        dataLogF("JIT generated code for %p at [%p, %p).\n", m_codeBlock, result.executableMemory()->start().untaggedPtr(), result.executableMemory()->end().untaggedPtr());
}

CompilationResult JIT::finalizeOnMainThread()
{
    RELEASE_ASSERT(!isCompilationThread());

    if (!m_jitCode)
        return CompilationFailed;

    m_linkBuffer->runMainThreadFinalizationTasks();

    {
        ConcurrentJSLocker locker(m_codeBlock->m_lock);
        m_codeBlock->shrinkToFit(locker, CodeBlock::ShrinkMode::LateShrink);
    }

    for (size_t i = 0; i < m_codeBlock->numberOfExceptionHandlers(); ++i) {
        HandlerInfo& handler = m_codeBlock->exceptionHandler(i);
        // FIXME: <rdar://problem/39433318>.
        handler.nativeCode = m_codeBlock->jitCodeMap().find(BytecodeIndex(handler.target)).retagged<ExceptionHandlerPtrTag>();
    }

    if (m_pcToCodeOriginMap)
        m_codeBlock->setPCToCodeOriginMap(WTFMove(m_pcToCodeOriginMap));

    m_vm->machineCodeBytesPerBytecodeWordForBaselineJIT->add(
        static_cast<double>(m_jitCode->size()) /
        static_cast<double>(m_codeBlock->instructionsSize()));

    m_codeBlock->setJITCode(m_jitCode.releaseNonNull());

    return CompilationSuccessful;
}

size_t JIT::codeSize() const
{
    if (!m_linkBuffer)
        return 0;
    return m_linkBuffer->size();
}

CompilationResult JIT::privateCompile(JITCompilationEffort effort)
{
    doMainThreadPreparationBeforeCompile();
    compileAndLinkWithoutFinalizing(effort);
    return finalizeOnMainThread();
}

#if !ENABLE(EXTRA_CTI_THUNKS)
void JIT::privateCompileExceptionHandlers()
{
    if (!m_exceptionChecksWithCallFrameRollback.empty()) {
        m_exceptionChecksWithCallFrameRollback.link(this);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

        // operationLookupExceptionHandlerFromCallerFrame is passed one argument, the VM*.
        move(TrustedImmPtr(&vm()), GPRInfo::argumentGPR0);
        prepareCallOperation(vm());
        m_farCalls.append(FarCallRecord(call(OperationPtrTag), FunctionPtr<OperationPtrTag>(operationLookupExceptionHandlerFromCallerFrame)));
        jumpToExceptionHandler(vm());
    }

    if (!m_exceptionChecks.empty() || m_byValCompilationInfo.size()) {
        m_exceptionHandler = label();
        m_exceptionChecks.link(this);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

        // operationLookupExceptionHandler is passed one argument, the VM*.
        move(TrustedImmPtr(&vm()), GPRInfo::argumentGPR0);
        prepareCallOperation(vm());
        m_farCalls.append(FarCallRecord(call(OperationPtrTag), FunctionPtr<OperationPtrTag>(operationLookupExceptionHandler)));
        jumpToExceptionHandler(vm());
    }
}
#endif // !ENABLE(EXTRA_CTI_THUNKS)

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

HashMap<CString, Seconds> JIT::compileTimeStats()
{
    HashMap<CString, Seconds> result;
    if (Options::reportTotalCompileTimes()) {
        result.add("Total Compile Time", totalCompileTime());
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
