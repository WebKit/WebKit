/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#include "BaselineJITPlan.h"
#include "BytecodeGraph.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGCapabilities.h"
#include "JITInlines.h"
#include "JITOperations.h"
#include "JITSizeStatistics.h"
#include "JITThunks.h"
#include "LLIntEntrypoint.h"
#include "LLIntThunks.h"
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
#include <wtf/BubbleSort.h>
#include <wtf/GraphNodeWorklist.h>
#include <wtf/SimpleStats.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace JITInternal {
static constexpr const bool verbose = false;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(JIT);

Seconds totalBaselineCompileTime;
Seconds totalDFGCompileTime;
Seconds totalFTLCompileTime;
Seconds totalFTLDFGCompileTime;
Seconds totalFTLB3CompileTime;

JIT::JIT(VM& vm, BaselineJITPlan& plan, CodeBlock* codeBlock)
    : JSInterfaceJIT(&vm, nullptr)
    , m_plan(plan)
    , m_labels(codeBlock ? codeBlock->instructions().size() : 0)
    , m_pcToCodeOriginMapBuilder(vm)
    , m_canBeOptimized(false)
    , m_shouldEmitProfiling(false)
    , m_profiledCodeBlock(codeBlock)
    , m_unlinkedCodeBlock(codeBlock->unlinkedCodeBlock())
{
}

JIT::~JIT() = default;

JITConstantPool::Constant JIT::addToConstantPool(JITConstantPool::Type type, void* payload)
{
    unsigned result = m_constantPool.size();
    m_constantPool.append(JITConstantPool::Value { payload, type });
    return result;
}

std::tuple<BaselineUnlinkedStructureStubInfo*, StructureStubInfoIndex> JIT::addUnlinkedStructureStubInfo()
{
    unsigned stubInfoIndex = m_unlinkedStubInfos.size();
    BaselineUnlinkedStructureStubInfo* stubInfo = &m_unlinkedStubInfos.alloc();
    return std::tuple { stubInfo, StructureStubInfoIndex { stubInfoIndex } };
}

BaselineUnlinkedCallLinkInfo* JIT::addUnlinkedCallLinkInfo()
{
    return &m_unlinkedCalls.alloc();
}

void JIT::emitNotifyWriteWatchpoint(GPRReg pointerToSet)
{
    auto ok = branchTestPtr(Zero, pointerToSet);
    addSlowCase(branch8(NotEqual, Address(pointerToSet, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    ok.link(this);
}

void JIT::emitVarReadOnlyCheck(ResolveType resolveType, GPRReg scratchGPR)
{
    if (resolveType == GlobalVar || resolveType == GlobalVarWithVarInjectionChecks) {
        loadGlobalObject(scratchGPR);
        loadPtr(Address(scratchGPR, JSGlobalObject::offsetOfVarReadOnlyWatchpoint()), scratchGPR);
        addSlowCase(branch8(Equal, Address(scratchGPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    }
}

void JIT::resetSP()
{
    addPtr(TrustedImm32(stackPointerOffsetFor(m_unlinkedCodeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();
}

#define NEXT_OPCODE_IN_MAIN(name) \
    if (previousSlowCasesSize != m_slowCases.size()) \
        ++m_bytecodeCountHavingSlowCase; \
    m_bytecodeIndex = BytecodeIndex(m_bytecodeIndex.offset() + currentInstruction->size()); \
    break;

#define DEFINE_SLOW_OP(name) \
    case op_##name: { \
        if (m_bytecodeIndex >= startBytecodeIndex) { \
            JITSlowPathCall slowPathCall(this, slow_path_##name); \
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
        break; \
    }

#define DEFINE_SLOWCASE_SLOW_OP(name) \
    case op_##name: { \
        emitSlowCaseCall(iter, slow_path_##name); \
        break; \
    }

void JIT::emitSlowCaseCall(Vector<SlowCaseEntry>::iterator& iter, SlowPathFunction stub)
{
    linkAllSlowCases(iter);

    JITSlowPathCall slowPathCall(this, stub);
    slowPathCall.call();
}

void JIT::privateCompileMainPass()
{
    if (JITInternal::verbose)
        dataLog("Compiling ", *m_profiledCodeBlock, "\n");
    
    jitAssertTagsInPlace();
    jitAssertArgumentCountSane();
    
    auto& instructions = m_unlinkedCodeBlock->instructions();
    unsigned instructionCount = m_unlinkedCodeBlock->instructions().size();

    BytecodeIndex startBytecodeIndex(0);

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
        const auto* currentInstruction = instructions.at(m_bytecodeIndex).ptr();
        ASSERT(currentInstruction->size());

        m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(m_bytecodeIndex));

        m_labels[m_bytecodeIndex.offset()] = label();

        if (JITInternal::verbose)
            dataLogLn("Baseline JIT emitting code for ", m_bytecodeIndex, " at offset ", (long)debugOffset());

        OpcodeID opcodeID = currentInstruction->opcodeID();

        std::optional<JITSizeStatistics::Marker> sizeMarker;
        if (UNLIKELY(m_bytecodeIndex >= startBytecodeIndex && Options::dumpBaselineJITSizeStatistics())) {
            String id = makeString("Baseline_fast_"_s, opcodeNames[opcodeID]);
            sizeMarker = m_vm->jitSizeStatistics->markStart(id, *this);
        }

#if ASSERT_ENABLED
        if (opcodeID != op_catch) {
            loadPtr(addressFor(CallFrameSlot::codeBlock), regT0);
            static_assert(static_cast<ptrdiff_t>(CodeBlock::offsetOfJITData() + sizeof(void*)) == CodeBlock::offsetOfMetadataTable());
            loadPairPtr(Address(regT0, CodeBlock::offsetOfJITData()), regT2, regT1);
            m_consistencyCheckCalls.append(nearCall());
        }
#endif

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
            VM* vm = m_vm;
            probeDebug([=] (Probe::Context& ctx) {
                CallFrame* callFrame = ctx.fp<CallFrame*>();
                if (opcodeID == op_catch) {
                    // The code generated by emit_op_catch() will update the callFrame to
                    // vm->callFrameForCatch later. Since that code doesn't execute until
                    // later, we should get the callFrame from vm->callFrameForCatch to get
                    // the real codeBlock that owns this op_catch bytecode.
                    callFrame = vm->callFrameForCatch;
                }
                CodeBlock* codeBlock = callFrame->codeBlock();
                dataLogLn("JIT [", bytecodeOffset, "] ", opcodeNames[opcodeID], " cfr ", RawPointer(ctx.fp()), " @ ", codeBlock);
            });
        }

        switch (opcodeID) {
        DEFINE_SLOW_OP(is_callable)
        DEFINE_SLOW_OP(is_constructor)
        DEFINE_SLOW_OP(typeof)
        DEFINE_SLOW_OP(typeof_is_object)
        DEFINE_SLOW_OP(strcat)
        DEFINE_SLOW_OP(push_with_scope)
        DEFINE_SLOW_OP(put_by_id_with_this)
        DEFINE_SLOW_OP(put_by_val_with_this)
        DEFINE_SLOW_OP(resolve_scope_for_hoisting_func_decl_in_eval)
        DEFINE_SLOW_OP(define_data_property)
        DEFINE_SLOW_OP(define_accessor_property)
        DEFINE_SLOW_OP(unreachable)
        DEFINE_SLOW_OP(throw_static_error)
        DEFINE_SLOW_OP(new_array_with_spread)
        DEFINE_SLOW_OP(new_array_with_species)
        DEFINE_SLOW_OP(new_array_buffer)
        DEFINE_SLOW_OP(spread)
        DEFINE_SLOW_OP(create_rest)
        DEFINE_SLOW_OP(create_promise)
        DEFINE_SLOW_OP(new_promise)
        DEFINE_SLOW_OP(create_generator)
        DEFINE_SLOW_OP(create_async_generator)
        DEFINE_SLOW_OP(new_generator)

        DEFINE_OP(op_add)
        DEFINE_OP(op_bitnot)
        DEFINE_OP(op_bitand)
        DEFINE_OP(op_bitor)
        DEFINE_OP(op_bitxor)
        DEFINE_OP(op_call)
        DEFINE_OP(op_call_ignore_result)
        DEFINE_OP(op_tail_call)
        DEFINE_OP(op_call_direct_eval)
        DEFINE_OP(op_call_varargs)
        DEFINE_OP(op_tail_call_varargs)
        DEFINE_OP(op_tail_call_forward_arguments)
        DEFINE_OP(op_construct_varargs)
        DEFINE_OP(op_super_construct_varargs)
        DEFINE_OP(op_catch)
        DEFINE_OP(op_construct)
        DEFINE_OP(op_super_construct)
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
        DEFINE_OP(op_has_private_name)
        DEFINE_OP(op_has_private_brand)
        DEFINE_OP(op_get_by_id)
        DEFINE_OP(op_get_length)
        DEFINE_OP(op_get_by_id_with_this)
        DEFINE_OP(op_get_by_id_direct)
        DEFINE_OP(op_get_by_val)
        DEFINE_OP(op_get_by_val_with_this)
        DEFINE_OP(op_get_property_enumerator)
        DEFINE_OP(op_enumerator_next)
        DEFINE_OP(op_enumerator_get_by_val)
        DEFINE_OP(op_enumerator_in_by_val)
        DEFINE_OP(op_enumerator_put_by_val)
        DEFINE_OP(op_enumerator_has_own_property)
        DEFINE_OP(op_get_private_name)
        DEFINE_OP(op_set_private_brand)
        DEFINE_OP(op_check_private_brand)
        DEFINE_OP(op_get_prototype_of)
        DEFINE_OP(op_overrides_has_instance)
        DEFINE_OP(op_instanceof)
        DEFINE_OP(op_is_empty)
        DEFINE_OP(op_typeof_is_undefined)
        DEFINE_OP(op_typeof_is_function)
        DEFINE_OP(op_is_undefined_or_null)
        DEFINE_OP(op_is_boolean)
        DEFINE_OP(op_is_number)
        DEFINE_OP(op_is_big_int)
        DEFINE_OP(op_is_object)
        DEFINE_OP(op_is_cell_with_type)
        DEFINE_OP(op_has_structure_with_flags)
        DEFINE_OP(op_jeq_null)
        DEFINE_OP(op_jfalse)
        DEFINE_OP(op_jmp)
        DEFINE_OP(op_jneq_null)
        DEFINE_OP(op_jundefined_or_null)
        DEFINE_OP(op_jnundefined_or_null)
        DEFINE_OP(op_jeq_ptr)
        DEFINE_OP(op_jneq_ptr)
        DEFINE_OP(op_less)
        DEFINE_OP(op_lesseq)
        DEFINE_OP(op_greater)
        DEFINE_OP(op_greatereq)
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
        DEFINE_OP(op_pow)
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
        DEFINE_OP(op_create_lexical_environment)
        DEFINE_OP(op_create_direct_arguments)
        DEFINE_OP(op_create_scoped_arguments)
        DEFINE_OP(op_create_cloned_arguments)
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
        DEFINE_OP(op_to_property_key_or_number)

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

        DEFINE_OP(op_log_shadow_chicken_prologue)
        DEFINE_OP(op_log_shadow_chicken_tail)

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (UNLIKELY(sizeMarker))
            m_vm->jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this, m_plan);

        if (JITInternal::verbose)
            dataLog("At ", bytecodeOffset, ": ", m_slowCases.size(), "\n");
    }

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
    m_getByValWithThisIndex = 0;
    m_putByIdIndex = 0;
    m_putByValIndex = 0;
    m_inByIdIndex = 0;
    m_inByValIndex = 0;
    m_delByIdIndex = 0;
    m_delByValIndex = 0;
    m_instanceOfIndex = 0;
    m_privateBrandAccessIndex = 0;

    unsigned bytecodeCountHavingSlowCase = 0;
    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end();) {
        m_bytecodeIndex = iter->to;

        m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(m_bytecodeIndex));

        BytecodeIndex firstTo = m_bytecodeIndex;

        const auto* currentInstruction = m_unlinkedCodeBlock->instructions().at(m_bytecodeIndex).ptr();

        if (JITInternal::verbose)
            dataLogLn("Baseline JIT emitting slow code for ", m_bytecodeIndex, " at offset ", (long)debugOffset());

        if (m_disassembler)
            m_disassembler->setForBytecodeSlowPath(m_bytecodeIndex.offset(), label());

        OpcodeID opcodeID = currentInstruction->opcodeID();

        std::optional<JITSizeStatistics::Marker> sizeMarker;
        if (UNLIKELY(Options::dumpBaselineJITSizeStatistics())) {
            String id = makeString("Baseline_slow_"_s, opcodeNames[opcodeID]);
            sizeMarker = m_vm->jitSizeStatistics->markStart(id, *this);
        }

        if (UNLIKELY(Options::traceBaselineJITExecution())) {
            unsigned bytecodeOffset = m_bytecodeIndex.offset();
            probeDebug([=] (Probe::Context& ctx) {
                CodeBlock* codeBlock = ctx.fp<CallFrame*>()->codeBlock();
                dataLogLn("JIT [", bytecodeOffset, "] SLOW ", opcodeNames[opcodeID], " cfr ", RawPointer(ctx.fp()), " @ ", codeBlock);
            });
        }

        switch (currentInstruction->opcodeID()) {
        DEFINE_SLOWCASE_OP(op_add)
        DEFINE_SLOWCASE_OP(op_call_direct_eval)
        DEFINE_SLOWCASE_OP(op_eq)
        DEFINE_SLOWCASE_OP(op_try_get_by_id)
        DEFINE_SLOWCASE_OP(op_in_by_id)
        DEFINE_SLOWCASE_OP(op_in_by_val)
        DEFINE_SLOWCASE_OP(op_has_private_name)
        DEFINE_SLOWCASE_OP(op_has_private_brand)
        DEFINE_SLOWCASE_OP(op_get_by_id)
        DEFINE_SLOWCASE_OP(op_get_length)
        DEFINE_SLOWCASE_OP(op_get_by_id_with_this)
        DEFINE_SLOWCASE_OP(op_get_by_id_direct)
        DEFINE_SLOWCASE_OP(op_get_by_val)
        DEFINE_SLOWCASE_OP(op_get_by_val_with_this)
        DEFINE_SLOWCASE_OP(op_enumerator_get_by_val)
        DEFINE_SLOWCASE_OP(op_enumerator_put_by_val)
        DEFINE_SLOWCASE_OP(op_get_private_name)
        DEFINE_SLOWCASE_OP(op_set_private_brand)
        DEFINE_SLOWCASE_OP(op_check_private_brand)
        DEFINE_SLOWCASE_OP(op_instanceof)
        DEFINE_SLOWCASE_OP(op_less)
        DEFINE_SLOWCASE_OP(op_lesseq)
        DEFINE_SLOWCASE_OP(op_greater)
        DEFINE_SLOWCASE_OP(op_greatereq)
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
        DEFINE_SLOWCASE_OP(op_enter)
        DEFINE_SLOWCASE_OP(op_check_traps)
        DEFINE_SLOWCASE_OP(op_mod)
        DEFINE_SLOWCASE_OP(op_pow)
        DEFINE_SLOWCASE_OP(op_mul)
        DEFINE_SLOWCASE_OP(op_negate)
        DEFINE_SLOWCASE_OP(op_neq)
        DEFINE_SLOWCASE_OP(op_new_object)
        DEFINE_SLOWCASE_OP(op_put_by_id)
        DEFINE_SLOWCASE_OP(op_put_by_val_direct)
        DEFINE_SLOWCASE_OP(op_put_by_val)
        DEFINE_SLOWCASE_OP(op_put_private_name)
        DEFINE_SLOWCASE_OP(op_del_by_val)
        DEFINE_SLOWCASE_OP(op_del_by_id)
        DEFINE_SLOWCASE_OP(op_sub)
        DEFINE_SLOWCASE_OP(op_resolve_scope)
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
        DEFINE_SLOWCASE_SLOW_OP(get_prototype_of)
        DEFINE_SLOWCASE_SLOW_OP(check_tdz)
        DEFINE_SLOWCASE_SLOW_OP(to_property_key)
        DEFINE_SLOWCASE_SLOW_OP(to_property_key_or_number)
        DEFINE_SLOWCASE_SLOW_OP(typeof_is_function)
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (JITInternal::verbose)
            dataLog("At ", firstTo, " slow: ", iter - m_slowCases.begin(), "\n");

        RELEASE_ASSERT_WITH_MESSAGE(iter == m_slowCases.end() || firstTo.offset() != iter->to.offset(), "Not enough jumps linked in slow case codegen.");
        RELEASE_ASSERT_WITH_MESSAGE(firstTo.offset() == (iter - 1)->to.offset(), "Too many jumps linked in slow case codegen.");

        jump().linkTo(fastPathResumePoint(), this);
        ++bytecodeCountHavingSlowCase;

        if (UNLIKELY(sizeMarker)) {
            m_bytecodeIndex = BytecodeIndex(m_bytecodeIndex.offset() + currentInstruction->size());
            m_vm->jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this, m_plan);
        }
    }

    RELEASE_ASSERT(bytecodeCountHavingSlowCase == m_bytecodeCountHavingSlowCase);
    RELEASE_ASSERT(m_getByIdIndex == m_getByIds.size());
    RELEASE_ASSERT(m_getByIdWithThisIndex == m_getByIdsWithThis.size());
    RELEASE_ASSERT(m_getByValWithThisIndex == m_getByValsWithThis.size());
    RELEASE_ASSERT(m_putByIdIndex == m_putByIds.size());
    RELEASE_ASSERT(m_putByValIndex == m_putByVals.size());
    RELEASE_ASSERT(m_inByIdIndex == m_inByIds.size());
    RELEASE_ASSERT(m_instanceOfIndex == m_instanceOfs.size());
    RELEASE_ASSERT(m_privateBrandAccessIndex == m_privateBrandAccesses.size());

#ifndef NDEBUG
    // Reset this, in order to guard its use with ASSERTs.
    m_bytecodeIndex = BytecodeIndex();
#endif
}

void JIT::emitMaterializeMetadataAndConstantPoolRegisters()
{
    emitMaterializeMetadataAndConstantPoolRegisters(*this);
}

void JIT::emitMaterializeMetadataAndConstantPoolRegisters(CCallHelpers& jit)
{
    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), GPRInfo::jitDataRegister);
    static_assert(static_cast<ptrdiff_t>(CodeBlock::offsetOfJITData() + sizeof(void*)) == CodeBlock::offsetOfMetadataTable());
    jit.loadPairPtr(Address(GPRInfo::jitDataRegister, CodeBlock::offsetOfJITData()), GPRInfo::jitDataRegister, GPRInfo::metadataTableRegister);
}

void JIT::emitSaveCalleeSaves()
{
    Base::emitSaveCalleeSavesFor(&RegisterAtOffsetList::llintBaselineCalleeSaveRegisters());
}

void JIT::emitRestoreCalleeSaves()
{
    Base::emitRestoreCalleeSavesFor(&RegisterAtOffsetList::llintBaselineCalleeSaveRegisters());
}

#if ASSERT_ENABLED
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::consistencyCheckGenerator(VM&)
{
    CCallHelpers jit;

    constexpr GPRReg stackOffsetGPR = regT0; // Incoming
    constexpr GPRReg expectedStackPointerGPR = regT1;
    constexpr GPRReg expectedMetadataGPR = regT2;
    constexpr GPRReg expectedConstantsGPR = regT3;

    jit.tagReturnAddress();

    jit.mul32(TrustedImm32(sizeof(Register)), stackOffsetGPR, stackOffsetGPR);
    jit.subPtr(callFrameRegister, stackOffsetGPR, expectedStackPointerGPR);
    // Fix up in case the call sequence (from the op) changed the stack pointer, e.g.: like on x86
    if (constexpr size_t delta = sizeof(CallerFrameAndPC) - prologueStackPointerDelta())
        jit.subPtr(TrustedImm32(delta), expectedStackPointerGPR);

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), expectedConstantsGPR);
    static_assert(static_cast<ptrdiff_t>(CodeBlock::offsetOfJITData() + sizeof(void*)) == CodeBlock::offsetOfMetadataTable());
    jit.loadPairPtr(Address(expectedConstantsGPR, CodeBlock::offsetOfJITData()), expectedConstantsGPR, expectedMetadataGPR);

    auto stackPointerOK = jit.branchPtr(Equal, expectedStackPointerGPR, stackPointerRegister);
    jit.breakpoint();
    stackPointerOK.link(&jit);

    auto metadataOK = jit.branchPtr(Equal, expectedMetadataGPR, GPRInfo::metadataTableRegister);
    jit.breakpoint();
    metadataOK.link(&jit);

    auto constantsOK = jit.branchPtr(Equal, expectedConstantsGPR, GPRInfo::jitDataRegister);
    jit.breakpoint();
    constantsOK.link(&jit);

    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "generateConsistencyCheck"_s, "generateConsistencyCheck");
}

void JIT::emitConsistencyCheck()
{
    ASSERT(!m_consistencyCheckLabel.isSet());
    m_consistencyCheckLabel = label();
    move(TrustedImm32(-stackPointerOffsetFor(m_unlinkedCodeBlock)), regT0);
    m_bytecodeIndex = BytecodeIndex(0);
    nearTailCallThunk(CodeLocationLabel { vm().getCTIStub(consistencyCheckGenerator).retaggedCode<NoPtrTag>() });
    m_bytecodeIndex = BytecodeIndex(); // Reset this, in order to guard its use with ASSERTs.
}
#endif

RefPtr<BaselineJITCode> JIT::compileAndLinkWithoutFinalizing(JITCompilationEffort effort)
{
    DFG::CapabilityLevel level = m_profiledCodeBlock->capabilityLevel();
    switch (level) {
    case DFG::CannotCompile:
        m_canBeOptimized = false;
        m_shouldEmitProfiling = false;
        break;
    case DFG::CanCompile:
    case DFG::CanCompileAndInline:
        m_canBeOptimized = true;
        m_shouldEmitProfiling = true;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (m_unlinkedCodeBlock->numberOfUnlinkedSwitchJumpTables() || m_unlinkedCodeBlock->numberOfUnlinkedStringSwitchJumpTables()) {
        if (m_unlinkedCodeBlock->numberOfUnlinkedSwitchJumpTables())
            m_switchJumpTables = FixedVector<SimpleJumpTable>(m_unlinkedCodeBlock->numberOfUnlinkedSwitchJumpTables());
        if (m_unlinkedCodeBlock->numberOfUnlinkedStringSwitchJumpTables())
            m_stringSwitchJumpTables = FixedVector<StringJumpTable>(m_unlinkedCodeBlock->numberOfUnlinkedStringSwitchJumpTables());
    }

    if (UNLIKELY(Options::dumpDisassembly() || Options::dumpBaselineDisassembly() || (m_vm->m_perBytecodeProfiler && Options::disassembleBaselineForProfiler()))) {
        // FIXME: build a disassembler off of UnlinkedCodeBlock.
        m_disassembler = makeUnique<JITDisassembler>(m_profiledCodeBlock);
    }
    if (UNLIKELY(m_vm->m_perBytecodeProfiler)) {
        // FIXME: build profiler disassembler off UnlinkedCodeBlock.
        m_compilation = adoptRef(
            new Profiler::Compilation(
                m_vm->m_perBytecodeProfiler->ensureBytecodesFor(m_profiledCodeBlock),
                Profiler::Baseline));
        m_compilation->addProfiledBytecodes(*m_vm->m_perBytecodeProfiler, m_profiledCodeBlock);
    }
    
    m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(BytecodeIndex(0)));

    std::optional<JITSizeStatistics::Marker> sizeMarker;
    if (UNLIKELY(Options::dumpBaselineJITSizeStatistics()))
        sizeMarker = m_vm->jitSizeStatistics->markStart("Baseline_prologue"_s, *this);

    Label entryLabel(this);
    if (m_disassembler)
        m_disassembler->setStartOfCode(entryLabel);

    // Just add a little bit of randomness to the codegen
    if (random() & 1)
        nop();

    emitFunctionPrologue();
    jitAssertCodeBlockOnCallFrameWithType(regT2, JITType::BaselineJIT);
    jitAssertCodeBlockMatchesCurrentCalleeCodeBlockOnCallFrame(regT1, regT2, *m_unlinkedCodeBlock);

    int frameTopOffset = stackPointerOffsetFor(m_unlinkedCodeBlock) * sizeof(Register);
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
    emitMaterializeMetadataAndConstantPoolRegisters();

    if (m_unlinkedCodeBlock->codeType() == FunctionCode) {
        ASSERT(!m_bytecodeIndex);
        if (shouldEmitProfiling() && (!m_unlinkedCodeBlock->isConstructor() || m_unlinkedCodeBlock->numParameters() > 1)) {
            emitGetFromCallFrameHeaderPtr(CallFrameSlot::codeBlock, regT2);
            loadPtr(Address(regT2, CodeBlock::offsetOfArgumentValueProfiles() + FixedVector<ArgumentValueProfile>::offsetOfStorage()), regT2);

            for (unsigned argument = 0; argument < m_unlinkedCodeBlock->numParameters(); ++argument) {
                // If this is a constructor, then we want to put in a dummy profiling site (to
                // keep things consistent) but we don't actually want to record the dummy value.
                if (m_unlinkedCodeBlock->isConstructor() && !argument)
                    continue;
                int offset = CallFrame::argumentOffsetIncludingThis(argument) * static_cast<int>(sizeof(Register));
                loadValue(Address(callFrameRegister, offset), jsRegT10);
                storeValue(jsRegT10, Address(regT2, FixedVector<ArgumentValueProfile>::Storage::offsetOfData() + argument * sizeof(ArgumentValueProfile) + ArgumentValueProfile::offsetOfFirstBucket()));
            }
        }
    }
    
    RELEASE_ASSERT(!JITCode::isJIT(m_profiledCodeBlock->jitType()));

    if (UNLIKELY(sizeMarker))
        m_vm->jitSizeStatistics->markEnd(WTFMove(*sizeMarker), *this, m_plan);

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();
    
    if (m_disassembler)
        m_disassembler->setEndOfSlowPath(label());
    m_pcToCodeOriginMapBuilder.appendItem(label(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

#if ASSERT_ENABLED
    emitConsistencyCheck();
#endif

    // If the number of parameters is 1, we never require arity fixup.
    JumpList stackOverflowWithEntry;
    bool requiresArityFixup = m_unlinkedCodeBlock->numParameters() != 1;
    if (m_unlinkedCodeBlock->codeType() == FunctionCode && requiresArityFixup) {
        m_arityCheck = label();
        RELEASE_ASSERT(m_unlinkedCodeBlock->codeType() == FunctionCode);

        unsigned numberOfParameters = m_unlinkedCodeBlock->numParameters();
        load32(CCallHelpers::calleeFramePayloadSlot(CallFrameSlot::argumentCountIncludingThis).withOffset(sizeof(CallerFrameAndPC) - prologueStackPointerDelta()), GPRInfo::argumentGPR2);
        branch32(AboveOrEqual, GPRInfo::argumentGPR2, TrustedImm32(numberOfParameters)).linkTo(entryLabel, this);
        m_bytecodeIndex = BytecodeIndex(0);
        getArityPadding(*m_vm, numberOfParameters, GPRInfo::argumentGPR2, GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR3, stackOverflowWithEntry);

#if CPU(X86_64)
        pop(GPRInfo::argumentGPR1);
#else
        tagPtr(NoPtrTag, linkRegister);
        move(linkRegister, GPRInfo::argumentGPR1);
#endif
        nearCallThunk(CodeLocationLabel { LLInt::arityFixup() });
#if CPU(X86_64)
        push(GPRInfo::argumentGPR1);
#else
        move(GPRInfo::argumentGPR1, linkRegister);
        untagPtr(NoPtrTag, linkRegister);
        validateUntaggedPtr(linkRegister, GPRInfo::argumentGPR0);
#endif
#if ASSERT_ENABLED
        m_bytecodeIndex = BytecodeIndex(); // Reset this, in order to guard its use with ASSERTs.
#endif
        jump(entryLabel);
    } else
        m_arityCheck = entryLabel; // Never require arity fixup.

    stackOverflowWithEntry.link(this);
    emitFunctionPrologue();
    stackOverflow.link(this);
    m_bytecodeIndex = BytecodeIndex(0);
    if (maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), stackPointerRegister);
    emitGetFromCallFrameHeaderPtr(CallFrameSlot::codeBlock, regT0);
    callThrowOperationWithCallFrameRollback(operationThrowStackOverflowError, regT0);

    ASSERT(m_jmpTable.isEmpty());

    if (m_disassembler)
        m_disassembler->setEndOfCode(label());
    m_pcToCodeOriginMapBuilder.appendItem(label(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

    LinkBuffer linkBuffer(*this, m_profiledCodeBlock, LinkBuffer::Profile::Baseline, effort);
    return link(linkBuffer);
}

RefPtr<BaselineJITCode> JIT::link(LinkBuffer& patchBuffer)
{
    if (patchBuffer.didFailToAllocate())
        return nullptr;

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (auto& record : m_switches) {
        unsigned bytecodeOffset = record.bytecodeIndex.offset();
        unsigned tableIndex = record.tableIndex;

        switch (record.type) {
        case SwitchRecord::Immediate:
        case SwitchRecord::Character: {
            const UnlinkedSimpleJumpTable& unlinkedTable = m_unlinkedCodeBlock->unlinkedSwitchJumpTable(tableIndex);
            SimpleJumpTable& linkedTable = m_switchJumpTables[tableIndex];
            linkedTable.m_ctiDefault = patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + record.defaultOffset]);
            for (unsigned j = 0; j < unlinkedTable.m_branchOffsets.size(); ++j) {
                int32_t offset = unlinkedTable.m_branchOffsets[j];
                linkedTable.m_ctiOffsets[j] = offset
                    ? patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + offset])
                    : linkedTable.m_ctiDefault;
            }
            break;
        }

        case SwitchRecord::String: {
            const UnlinkedStringJumpTable& unlinkedTable = m_unlinkedCodeBlock->unlinkedStringSwitchJumpTable(tableIndex);
            StringJumpTable& linkedTable = m_stringSwitchJumpTables[tableIndex];
            auto ctiDefault = patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + record.defaultOffset]);
            for (auto& location : unlinkedTable.m_offsetTable.values()) {
                int32_t offset = location.m_branchOffset;
                linkedTable.m_ctiOffsets[location.m_indexInTable] = offset
                    ? patchBuffer.locationOf<JSSwitchPtrTag>(m_labels[bytecodeOffset + offset])
                    : ctiDefault;
            }
            linkedTable.m_ctiOffsets[unlinkedTable.m_offsetTable.size()] = ctiDefault;
            break;
        }
        }
    }

    for (auto& record : m_farCalls) {
        if (record.callee)
            patchBuffer.link(record.from, record.callee);
    }

#if ASSERT_ENABLED
    const auto consistencyCheck = patchBuffer.locationOf<JSInternalPtrTag>(m_consistencyCheckLabel);
    for (auto& call : m_consistencyCheckCalls)
        patchBuffer.link<JSInternalPtrTag>(call, consistencyCheck);
#endif

    auto finalizeICs = [&] (auto& generators) {
        for (auto& gen : generators) {
            gen.m_unlinkedStubInfo->doneLocation = patchBuffer.locationOf<JSInternalPtrTag>(gen.m_done);
            gen.m_unlinkedStubInfo->slowPathStartLocation = patchBuffer.locationOf<JITStubRoutinePtrTag>(gen.m_slowPathBegin);
        }
    };

    finalizeICs(m_getByIds);
    finalizeICs(m_getByVals);
    finalizeICs(m_getByIdsWithThis);
    finalizeICs(m_getByValsWithThis);
    finalizeICs(m_putByIds);
    finalizeICs(m_putByVals);
    finalizeICs(m_delByIds);
    finalizeICs(m_delByVals);
    finalizeICs(m_inByIds);
    finalizeICs(m_inByVals);
    finalizeICs(m_instanceOfs);
    finalizeICs(m_privateBrandAccesses);

    for (auto& compilationInfo : m_callCompilationInfo) {
        auto& info = *compilationInfo.unlinkedCallLinkInfo;
        info.doneLocation = patchBuffer.locationOf<JSInternalPtrTag>(compilationInfo.doneLocation);
    }

    JITCodeMapBuilder jitCodeMapBuilder;
    for (unsigned bytecodeOffset = 0; bytecodeOffset < m_labels.size(); ++bytecodeOffset) {
        if (m_labels[bytecodeOffset].isSet())
            jitCodeMapBuilder.append(BytecodeIndex(bytecodeOffset), patchBuffer.locationOf<JSEntryPtrTag>(m_labels[bytecodeOffset]));
    }

    if (UNLIKELY(Options::dumpDisassembly() || Options::dumpBaselineDisassembly())) {
        m_disassembler->dump(patchBuffer);
        patchBuffer.didAlreadyDisassemble();
    }

    if (UNLIKELY(m_compilation)) {
        // FIXME: should we make the bytecode profiler know about UnlinkedCodeBlock?
        if (Options::disassembleBaselineForProfiler())
            m_disassembler->reportToProfiler(m_compilation.get(), patchBuffer);
        m_vm->m_perBytecodeProfiler->addCompilation(m_profiledCodeBlock, *m_compilation);
    }

    std::unique_ptr<PCToCodeOriginMap> pcToCodeOriginMap;
    if (m_pcToCodeOriginMapBuilder.didBuildMapping())
        pcToCodeOriginMap = makeUnique<PCToCodeOriginMap>(WTFMove(m_pcToCodeOriginMapBuilder), patchBuffer);
    
    // FIXME: Make a version of CodeBlockWithJITType that knows about UnlinkedCodeBlock.
    CodeRef<JSEntryPtrTag> result = FINALIZE_BASELINE_CODE(
        patchBuffer, JSEntryPtrTag,
        "Baseline JIT code for %s", toCString(CodeBlockWithJITType(m_profiledCodeBlock, JITType::BaselineJIT)).data());
    
    CodePtr<JSEntryPtrTag> withArityCheck = patchBuffer.locationOf<JSEntryPtrTag>(m_arityCheck);
    auto jitCode = adoptRef(*new BaselineJITCode(result, withArityCheck));

    jitCode->m_unlinkedCalls = FixedVector<BaselineUnlinkedCallLinkInfo>(m_unlinkedCalls.size());
    if (jitCode->m_unlinkedCalls.size()) {
        std::move(m_unlinkedCalls.begin(), m_unlinkedCalls.end(), jitCode->m_unlinkedCalls.begin());
        // It is almost always already sorted.
        WTF::bubbleSort(jitCode->m_unlinkedCalls.begin(), jitCode->m_unlinkedCalls.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.bytecodeIndex < rhs.bytecodeIndex;
            });
    }
    jitCode->m_unlinkedStubInfos = FixedVector<BaselineUnlinkedStructureStubInfo>(m_unlinkedStubInfos.size());
    if (jitCode->m_unlinkedStubInfos.size())
        std::move(m_unlinkedStubInfos.begin(), m_unlinkedStubInfos.end(), jitCode->m_unlinkedStubInfos.begin());
    jitCode->m_switchJumpTables = WTFMove(m_switchJumpTables);
    jitCode->m_stringSwitchJumpTables = WTFMove(m_stringSwitchJumpTables);
    jitCode->m_jitCodeMap = jitCodeMapBuilder.finalize();
    jitCode->adoptMathICs(m_mathICs);
    jitCode->m_constantPool = WTFMove(m_constantPool);
    jitCode->m_isShareable = m_isShareable;
    jitCode->m_pcToCodeOriginMap = WTFMove(pcToCodeOriginMap);

    if (JITInternal::verbose)
        dataLogF("JIT generated code for %p at [%p, %p).\n", m_unlinkedCodeBlock, result.executableMemory()->start().untaggedPtr(), result.executableMemory()->end().untaggedPtr());
    return jitCode;
}

CompilationResult JIT::finalizeOnMainThread(CodeBlock* codeBlock, BaselineJITPlan& plan, RefPtr<BaselineJITCode> jitCode)
{
    RELEASE_ASSERT(!isCompilationThread());

    if (!jitCode)
        return CompilationFailed;

    plan.runMainThreadFinalizationTasks();

    codeBlock->vm().machineCodeBytesPerBytecodeWordForBaselineJIT->add(
        static_cast<double>(jitCode->size()) /
        static_cast<double>(codeBlock->unlinkedCodeBlock()->instructionsSize()));

    codeBlock->setupWithUnlinkedBaselineCode(jitCode.releaseNonNull());

    return CompilationSuccessful;
}

CompilationResult JIT::compileSync(VM&, CodeBlock* codeBlock, JITCompilationEffort effort)
{
    auto plan = adoptRef(*new BaselineJITPlan(codeBlock));
    plan->compileSync(effort);
    return plan->finalize();
}

void JIT::doMainThreadPreparationBeforeCompile(VM& vm)
{
    // This ensures that we have the most up to date type information when performing typecheck optimizations for op_profile_type.
    if (vm.typeProfiler())
        vm.typeProfilerLog()->processLogEntries(vm, "Preparing for JIT compilation."_s);
}

unsigned JIT::frameRegisterCountFor(UnlinkedCodeBlock* codeBlock)
{
    ASSERT(static_cast<unsigned>(codeBlock->numCalleeLocals()) == WTF::roundUpToMultipleOf(stackAlignmentRegisters(), static_cast<unsigned>(codeBlock->numCalleeLocals())));

    return roundLocalRegisterCountForFramePointerOffset(codeBlock->numCalleeLocals() + maxFrameExtentForSlowPathCallInRegisters);
}

unsigned JIT::frameRegisterCountFor(CodeBlock* codeBlock)
{
    return frameRegisterCountFor(codeBlock->unlinkedCodeBlock());
}

int JIT::stackPointerOffsetFor(UnlinkedCodeBlock* codeBlock)
{
    return virtualRegisterForLocal(frameRegisterCountFor(codeBlock) - 1).offset();
}

int JIT::stackPointerOffsetFor(CodeBlock* codeBlock)
{
    return stackPointerOffsetFor(codeBlock->unlinkedCodeBlock());
}

UncheckedKeyHashMap<CString, Seconds> JIT::compileTimeStats()
{
    UncheckedKeyHashMap<CString, Seconds> result;
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

void JIT::exceptionCheck(Jump jumpToHandler)
{
    jumpToHandler.linkThunk(CodeLocationLabel(vm().getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), this);
}

void JIT::exceptionCheck()
{
    exceptionCheck(emitExceptionCheck(vm()));
}

void JIT::exceptionChecksWithCallFrameRollback(Jump jumpToHandler)
{
    jumpToHandler.linkThunk(CodeLocationLabel(vm().getCTIStub(CommonJITThunkID::HandleExceptionWithCallFrameRollback).retaggedCode<NoPtrTag>()), this);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(JIT)
