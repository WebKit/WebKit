/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "LOLJIT.h"

#if ENABLE(JIT) && USE(JSVALUE64)

#include "BaselineJITPlan.h"
#include "BaselineJITRegisters.h"
#include "BytecodeGraph.h"
#include "BytecodeUseDef.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGCapabilities.h"
#include "JITBitAndGenerator.h"
#include "JITBitOrGenerator.h"
#include "JITBitXorGenerator.h"
#include "JITInlines.h"
#include "JITLeftShiftGenerator.h"
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
#include <wtf/SequesteredMalloc.h>
#include <wtf/SimpleStats.h>
#include <wtf/text/MakeString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::LOL {

namespace LOLJITInternal {
#ifdef NDEBUG
static constexpr bool verbose = false;
#else
static constexpr bool verbose = true;
#endif
}

LOLJIT::LOLJIT(VM& vm, BaselineJITPlan& plan, CodeBlock* codeBlock)
    : JIT(vm, plan, codeBlock)
    , m_fastAllocator(*this, codeBlock)
    , m_replayAllocator(*this, codeBlock)
{
}

RefPtr<BaselineJITCode> LOLJIT::compileAndLinkWithoutFinalizing(JITCompilationEffort effort)
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

    if (Options::dumpDisassembly() || Options::dumpBaselineDisassembly() || (m_vm->m_perBytecodeProfiler && Options::disassembleBaselineForProfiler())) [[unlikely]] {
        // FIXME: build a disassembler off of UnlinkedCodeBlock.
        m_disassembler = makeUnique<JITDisassembler>(m_profiledCodeBlock);
    }

    if (m_vm->m_perBytecodeProfiler) [[unlikely]] {
        // FIXME: build profiler disassembler off UnlinkedCodeBlock.
        m_compilation = adoptRef(
            new Profiler::Compilation(
                m_vm->m_perBytecodeProfiler->ensureBytecodesFor(m_profiledCodeBlock),
                Profiler::Baseline));
        Ref(*m_compilation)->addProfiledBytecodes(*m_vm->m_perBytecodeProfiler, m_profiledCodeBlock);
    }

    m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(BytecodeIndex(0)));

    std::optional<JITSizeStatistics::Marker> sizeMarker;
    if (Options::dumpBaselineJITSizeStatistics()) [[unlikely]]
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
    addPtr(TrustedImm32(frameTopOffset), callFrameRegister, regT1);
    JumpList stackOverflow;
#if !CPU(ADDRESS64)
    unsigned maxFrameSize = -frameTopOffset;
    if (maxFrameSize > Options::reservedZoneSize()) [[unlikely]]
        stackOverflow.append(branchPtr(Above, regT1, callFrameRegister));
#endif
    stackOverflow.append(branchPtr(GreaterThan, AbsoluteAddress(m_vm->addressOfSoftStackLimit()), regT1));

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
                // FIXME: We should consider poisoning `this`s profiling site so if anyone tries to consume it they would crash instead of whatever weirdness.
                if (m_unlinkedCodeBlock->isConstructor() && !argument)
                    continue;
                int offset = CallFrame::argumentOffsetIncludingThis(argument) * static_cast<int>(sizeof(Register));
                loadValue(Address(callFrameRegister, offset), jsRegT10);
                storeValue(jsRegT10, Address(regT2, FixedVector<ArgumentValueProfile>::Storage::offsetOfData() + argument * sizeof(ArgumentValueProfile) + ArgumentValueProfile::offsetOfFirstBucket()));
            }
        }
    }

    RELEASE_ASSERT(!JITCode::isJIT(m_profiledCodeBlock->jitType()));

    if (sizeMarker) [[unlikely]]
        m_vm->jitSizeStatistics->markEnd(WTF::move(*sizeMarker), *this, Ref(m_plan));

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
    m_bytecodeIndex = BytecodeIndex(0);
    stackOverflow.link(this);
    jumpThunk(CodeLocationLabel(vm().getCTIStub(CommonJITThunkID::ThrowStackOverflowAtPrologue).retaggedCode<NoPtrTag>()));

    ASSERT(m_jmpTable.isEmpty());

    if (m_disassembler)
        m_disassembler->setEndOfCode(label());
    m_pcToCodeOriginMapBuilder.appendItem(label(), PCToCodeOriginMapBuilder::defaultCodeOrigin());

    LinkBuffer linkBuffer(*this, m_profiledCodeBlock, LinkBuffer::Profile::Baseline, effort);
    return link(linkBuffer);
}

void LOLJIT::privateCompileMainPass()
{
#define DEFINE_SLOW_OP(name) \
    case op_##name: { \
        m_fastAllocator.flushAllRegisters(*this); \
        JITSlowPathCall slowPathCall(this, slow_path_##name); \
        slowPathCall.call(); \
        nextBytecodeIndexWithFlushForJumpTargetsIfNeeded(m_fastAllocator, true); \
        break; \
    }

#define DEFINE_OP(name) \
    case name: { \
        if (!isImplemented(name)) \
            m_fastAllocator.flushAllRegisters(*this); \
        emit_##name(m_currentInstruction); \
        nextBytecodeIndexWithFlushForJumpTargetsIfNeeded(m_fastAllocator, true); \
        break; \
    }

    dataLogIf(LOLJITInternal::verbose, "Compiling ", *m_profiledCodeBlock, "\n");

    jitAssertTagsInPlace();
    jitAssertArgumentCountSane();

    auto& instructions = m_unlinkedCodeBlock->instructions();
    unsigned instructionCount = m_unlinkedCodeBlock->instructions().size();

    m_bytecodeCountHavingSlowCase = 0;
    m_currentJumpTargetIndex = 0;
    for (m_bytecodeIndex = BytecodeIndex(0); m_bytecodeIndex.offset() < instructionCount; ) {
        unsigned previousSlowCasesSize = m_slowCases.size();
        auto* currentInstruction = instructions.at(m_bytecodeIndex).ptr();
        m_currentInstruction = currentInstruction;
        ASSERT(m_currentInstruction->size());

        if (m_disassembler)
            m_disassembler->setForBytecodeMainPath(m_bytecodeIndex.offset(), label(), toCString("Allocator State Before: ", m_fastAllocator));
        m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(m_bytecodeIndex));
        m_labels[m_bytecodeIndex.offset()] = label();

        if (LOLJITInternal::verbose) {
            dataLogLn("LOL JIT emitting code for ", m_bytecodeIndex, " at offset ", (long)debugOffset(), " allocator before: ", m_fastAllocator);
            m_profiledCodeBlock->dumpBytecode(WTF::dataFile(), m_bytecodeIndex.offset());
        }

        OpcodeID opcodeID = currentInstruction->opcodeID();

        std::optional<JITSizeStatistics::Marker> sizeMarker;
        if (Options::dumpBaselineJITSizeStatistics()) [[unlikely]] {
            String id = makeString("Baseline_fast_"_s, opcodeNames[opcodeID]);
            sizeMarker = m_vm->jitSizeStatistics->markStart(id, *this);
        }

        if (m_compilation) [[unlikely]] {
            add64(
                TrustedImm32(1),
                AbsoluteAddress(Ref(*m_compilation)->executionCounterFor(Profiler::OriginStack(Profiler::Origin(
                    m_compilation->bytecodes(), m_bytecodeIndex)))->address()));
        }

        if (Options::eagerlyUpdateTopCallFrame())
            updateTopCallFrame();

        unsigned bytecodeOffset = m_bytecodeIndex.offset();
        if (Options::traceBaselineJITExecution()) [[unlikely]] {
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
        DEFINE_OP(op_new_reg_exp)
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
        if (sizeMarker) [[unlikely]]
            m_vm->jitSizeStatistics->markEnd(WTF::move(*sizeMarker), *this, Ref(m_plan));

        if (LOLJITInternal::verbose)
            dataLogLn("At ", bytecodeOffset, ": added ", m_slowCases.size() - previousSlowCasesSize, "(", m_slowCases.size(), ") allocator: ", m_fastAllocator);
    }

#undef NEXT_OPCODE_IN_MAIN
#undef DEFINE_SLOW_OP
#undef DEFINE_OP
}

void LOLJIT::privateCompileSlowCases()
{
#define DEFINE_SLOWCASE_OP(name) \
    case name: { \
        if (!isImplemented(name)) \
            m_replayAllocator.flushAllRegisters(*this); \
        emitSlow_##name(currentInstruction, iter); \
        break; \
    }

#define DEFINE_SLOWCASE_SLOW_OP(name, Struct) \
    case op_##name: { \
        if constexpr (isImplemented(op_##name)) { \
            emitCommonSlowPathSlowCaseCall<Struct>(currentInstruction, iter, slow_path_##name); \
        } else { \
            m_replayAllocator.flushAllRegisters(*this); \
            emitSlowCaseCall(iter, slow_path_##name); \
        } \
        break; \
    }

#define REPLAY_ALLOCATION_FOR_OP(name, Struct) \
    case name: { \
        ASSERT(isImplemented(name)); \
        m_replayAllocator.allocate(*this, currentInstruction->as<Struct>(), m_bytecodeIndex); \
        break; \
    }

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
    m_currentJumpTargetIndex = 0;

    auto& instructions = m_unlinkedCodeBlock->instructions();
    unsigned instructionCount = m_unlinkedCodeBlock->instructions().size();

    Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin();
    for (m_bytecodeIndex = BytecodeIndex(0); m_bytecodeIndex.offset() < instructionCount; ) {
        if (iter == m_slowCases.end())
            break;

        auto* currentInstruction = instructions.at(m_bytecodeIndex).ptr();
        m_currentInstruction = currentInstruction;
        OpcodeID opcodeID = currentInstruction->opcodeID();

        if (LOLJITInternal::verbose) {
            dataLogLn("LOL JIT emitting slow code for ", m_bytecodeIndex, " at offset ", (long)debugOffset(), " allocator: ", m_replayAllocator);
            m_profiledCodeBlock->dumpBytecode(WTF::dataFile(), m_bytecodeIndex.offset());
        }

        ASSERT(currentInstruction->size());
        if (iter->to.offset() != m_bytecodeIndex.offset()) {
            if (!isImplemented(opcodeID)) {
                dataLogLnIf(LOLJITInternal::verbose, "LOL JIT no slow paths to link. Next slow path at ", iter->to);
                m_replayAllocator.flushAllRegisters(*this);
                nextBytecodeIndexWithFlushForJumpTargetsIfNeeded(m_replayAllocator, false);
                continue;
            }
        } else
            m_pcToCodeOriginMapBuilder.appendItem(label(), CodeOrigin(m_bytecodeIndex));

        Vector<SlowCaseEntry>::iterator iterStart = iter;
        BytecodeIndex firstTo = iter->to;

        if (m_disassembler)
            m_disassembler->setForBytecodeSlowPath(m_bytecodeIndex.offset(), label(), toCString("Allocator State Before: ", m_replayAllocator));

        std::optional<JITSizeStatistics::Marker> sizeMarker;
        if (Options::dumpBaselineJITSizeStatistics()) [[unlikely]] {
            String id = makeString("Baseline_slow_"_s, opcodeNames[opcodeID]);
            sizeMarker = m_vm->jitSizeStatistics->markStart(id, *this);
        }

        // FIXME: Does this do anything? we usually link in the emitSlow path.
        if (Options::traceBaselineJITExecution()) [[unlikely]] {
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

        DEFINE_SLOWCASE_SLOW_OP(unsigned, OpUnsigned)
        DEFINE_SLOWCASE_SLOW_OP(inc, OpInc)
        DEFINE_SLOWCASE_SLOW_OP(dec, OpDec)
        DEFINE_SLOWCASE_SLOW_OP(bitnot, OpBitnot)
        DEFINE_SLOWCASE_SLOW_OP(bitand, OpBitand)
        DEFINE_SLOWCASE_SLOW_OP(bitor, OpBitor)
        DEFINE_SLOWCASE_SLOW_OP(bitxor, OpBitxor)
        DEFINE_SLOWCASE_SLOW_OP(lshift, OpLshift)
        DEFINE_SLOWCASE_SLOW_OP(rshift, OpRshift)
        DEFINE_SLOWCASE_SLOW_OP(urshift, OpUrshift)
        DEFINE_SLOWCASE_SLOW_OP(div, OpDiv)
        DEFINE_SLOWCASE_SLOW_OP(create_this, OpCreateThis)
        DEFINE_SLOWCASE_SLOW_OP(create_promise, OpCreatePromise)
        DEFINE_SLOWCASE_SLOW_OP(create_generator, OpCreateGenerator)
        DEFINE_SLOWCASE_SLOW_OP(create_async_generator, OpCreateAsyncGenerator)
        DEFINE_SLOWCASE_SLOW_OP(to_this, OpToThis)
        DEFINE_SLOWCASE_SLOW_OP(to_primitive, OpToPrimitive)
        DEFINE_SLOWCASE_SLOW_OP(to_number, OpToNumber)
        DEFINE_SLOWCASE_SLOW_OP(to_numeric, OpToNumeric)
        DEFINE_SLOWCASE_SLOW_OP(to_string, OpToString)
        DEFINE_SLOWCASE_SLOW_OP(to_object, OpToObject)
        DEFINE_SLOWCASE_SLOW_OP(not, OpNot)
        DEFINE_SLOWCASE_SLOW_OP(stricteq, OpStricteq)
        DEFINE_SLOWCASE_SLOW_OP(nstricteq, OpNstricteq)
        DEFINE_SLOWCASE_SLOW_OP(get_prototype_of, OpGetPrototypeOf)
        DEFINE_SLOWCASE_SLOW_OP(check_tdz, OpCheckTdz)
        DEFINE_SLOWCASE_SLOW_OP(to_property_key, OpToPropertyKey)
        DEFINE_SLOWCASE_SLOW_OP(to_property_key_or_number, OpToPropertyKeyOrNumber)
        DEFINE_SLOWCASE_SLOW_OP(typeof_is_function, OpTypeofIsFunction)
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (LOLJITInternal::verbose)
            dataLogLn("At ", firstTo, " linked ", iter - iterStart, " slow cases");

        if (firstTo.offset() == m_bytecodeIndex.offset()) {
            RELEASE_ASSERT_WITH_MESSAGE(iter == m_slowCases.end() || firstTo.offset() != iter->to.offset(), "Not enough jumps linked in slow case codegen while handling %s.", toCString(currentInstruction->opcodeID()).data());
            RELEASE_ASSERT_WITH_MESSAGE(firstTo.offset() == (iter - 1)->to.offset(), "Too many jumps linked in slow case codegen while handling %s.", toCString(currentInstruction->opcodeID()).data());
        }

        jump().linkTo(fastPathResumePoint(), this);

        if (sizeMarker) [[unlikely]] {
            m_bytecodeIndex = BytecodeIndex(m_bytecodeIndex.offset() + currentInstruction->size());
            m_vm->jitSizeStatistics->markEnd(WTF::move(*sizeMarker), *this, Ref(m_plan));
        }

        nextBytecodeIndexWithFlushForJumpTargetsIfNeeded(m_replayAllocator, false);
    }

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

// Comparison bytecodes

template<typename Op>
    requires (LOLJIT::isImplemented(Op::opcodeID))
void LOLJIT::emitCommonSlowPathSlowCaseCall(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter, SlowPathFunction stub)
{
    auto bytecode = currentInstruction->as<Op>();
    m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);

    linkAllSlowCases(iter);

    // If a use is the same as a def we have to spill it before the call.
    silentSpill(m_replayAllocator);
    JITSlowPathCall slowPathCall(this, stub);
    slowPathCall.call();
    // The slow path will write the result to the stack, so we have silentFill fill it.
    silentFill(m_replayAllocator);
}

template<typename Op>
    requires (!LOLJIT::isImplemented(Op::opcodeID))
void LOLJIT::emitCommonSlowPathSlowCaseCall(const JSInstruction*, Vector<SlowCaseEntry>::iterator&, SlowPathFunction)
{
    UNREACHABLE_FOR_PLATFORM();
}

void LOLJIT::emit_op_eq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEq>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    emitJumpSlowCaseIfNotInt(leftRegs.gpr(), rightRegs.gpr(), s_scratch);
    compare32(Equal, leftRegs.gpr(), rightRegs.gpr(), destRegs.gpr());
    boxBoolean(destRegs.gpr(), destRegs);
}

void LOLJIT::emitSlow_op_eq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpEq>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    linkAllSlowCases(iter);

    silentSpill(m_replayAllocator, destRegs.payloadGPR());
    loadGlobalObject(s_scratch);
    callOperation(operationCompareEq, s_scratch, leftRegs, rightRegs);
    boxBoolean(returnValueGPR, destRegs);
    silentFill(m_replayAllocator, destRegs.payloadGPR());
}

void LOLJIT::emit_op_neq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNeq>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    emitJumpSlowCaseIfNotInt(leftRegs.payloadGPR(), rightRegs.payloadGPR(), s_scratch);
    compare32(NotEqual, leftRegs.payloadGPR(), rightRegs.payloadGPR(), destRegs.payloadGPR());
    boxBoolean(destRegs.payloadGPR(), destRegs);
}

void LOLJIT::emitSlow_op_neq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpNeq>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    silentSpill(m_replayAllocator, destRegs.payloadGPR());
    loadGlobalObject(s_scratch);
    callOperation(operationCompareEq, s_scratch, leftRegs, rightRegs);
    xor32(TrustedImm32(0x1), returnValueGPR);
    boxBoolean(returnValueGPR, destRegs);
    silentFill(m_replayAllocator, destRegs.payloadGPR());
}

template<typename Op>
void LOLJIT::emitCompare(const JSInstruction* instruction, RelationalCondition condition)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ op1Regs, op2Regs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;
    // FIXME: Old Clangs don't support captured structured bindings even though it's officially supported in C++20
    auto emitCompare = [&, dstRegs = dstRegs](RelationalCondition cond, JSValueRegs leftJSR, auto right) {
        GPRReg left = leftJSR.payloadGPR();
        compare32(cond, left, right, dstRegs.payloadGPR());
        boxBoolean(dstRegs.payloadGPR(), dstRegs);
    };
    emitCompareImpl(op1, op1Regs, op2, op2Regs, condition, emitCompare);
}

template <typename EmitCompareFunctor>
ALWAYS_INLINE void LOLJIT::emitCompareImpl(VirtualRegister op1, JSValueRegs op1Regs, VirtualRegister op2, JSValueRegs op2Regs, RelationalCondition condition, const EmitCompareFunctor& emitCompare)
{
    // We generate inline code for the following cases in the fast path:
    // - int immediate to constant int immediate
    // - constant int immediate to int immediate
    // - int immediate to int immediate

    constexpr bool disallowAllocation = false;
    auto handleConstantCharOperand = [&](VirtualRegister left, JSValueRegs rightRegs, RelationalCondition cond) {
        if (!isOperandConstantChar(left))
            return false;

        addSlowCase(branchIfNotCell(rightRegs));
        JumpList failures;
        emitLoadCharacterString(rightRegs.payloadGPR(), rightRegs.payloadGPR(), failures);
        addSlowCase(failures);
        emitCompare(commute(cond), rightRegs, Imm32(asString(getConstantOperand(left))->tryGetValue(disallowAllocation).data[0]));
        return true;
    };

    if (handleConstantCharOperand(op1, op2Regs, condition))
        return;
    if (handleConstantCharOperand(op2, op1Regs, commute(condition)))
        return;

    auto handleConstantIntOperand = [&](VirtualRegister left, JSValueRegs rightRegs, RelationalCondition cond) {
        if (!isOperandConstantInt(left))
            return false;

        emitJumpSlowCaseIfNotInt(rightRegs);
        emitCompare(commute(cond), rightRegs, Imm32(getOperandConstantInt(left)));
        return true;
    };

    if (handleConstantIntOperand(op1, op2Regs, condition))
        return;
    if (handleConstantIntOperand(op2, op1Regs, commute(condition)))
        return;

    // TODO: I think this can be a single branch with a emitJumpSlowCaseIfNotInt(JSValueRegs, JSValueRegs) helper.
    emitJumpSlowCaseIfNotInt(op1Regs);
    emitJumpSlowCaseIfNotInt(op2Regs);

    emitCompare(condition, op1Regs, op2Regs.payloadGPR());
}

template<typename Op, typename SlowOperation>
void LOLJIT::emitCompareSlow(const JSInstruction* instruction, DoubleCondition condition, SlowOperation operation, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = instruction->as<Op>();

    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ op1Regs, op2Regs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    // FIXME: Old Clangs don't support captured structured bindings even though it's officially supported in C++20
    auto emitDoubleCompare = [&, dstRegs = dstRegs](FPRReg left, FPRReg right) {
        compareDouble(condition, left, right, s_scratch);
        boxBoolean(s_scratch, dstRegs);
    };
    emitCompareSlowImpl(op1, op1Regs, op2, op2Regs, dstRegs, operation, iter, emitDoubleCompare);
}

template<typename SlowOperation, typename EmitDoubleCompareFunctor>
void LOLJIT::emitCompareSlowImpl(VirtualRegister op1, JSValueRegs op1Regs, VirtualRegister op2, JSValueRegs op2Regs, JSValueRegs dstRegs, SlowOperation operation, Vector<SlowCaseEntry>::iterator& iter, const EmitDoubleCompareFunctor& emitDoubleCompare)
{
    // We generate inline code for the following cases in the slow path:
    // - floating-point number to constant int immediate
    // - constant int immediate to floating-point number
    // - floating-point number to floating-point number.
    if (isOperandConstantChar(op1) || isOperandConstantChar(op2)) {
        linkAllSlowCases(iter);

        silentSpill(m_replayAllocator, dstRegs.payloadGPR());
        loadGlobalObject(s_scratch);
        callOperation(operation, s_scratch, op1Regs, op2Regs);
        if (dstRegs)
            boxBoolean(returnValueGPR, dstRegs);
        silentFill(m_replayAllocator, dstRegs.payloadGPR());
        return;
    }

    auto handleConstantIntOperandSlow = [&](VirtualRegister op, JSValueRegs op1Regs, FPRReg fpReg1, JSValueRegs op2Regs, FPRReg fpReg2) {
        if (!isOperandConstantInt(op))
            return false;
        linkAllSlowCases(iter);

        Jump fail1 = branchIfNotNumber(op2Regs, s_scratch);
        unboxDouble(op2Regs.payloadGPR(), s_scratch, fpReg2);

        convertInt32ToDouble(op1Regs.payloadGPR(), fpReg1);

        emitDoubleCompare(fpRegT0, fpRegT1);

        jump().linkTo(fastPathResumePoint(), this);

        fail1.link(this);

        silentSpill(m_replayAllocator, dstRegs.payloadGPR());
        loadGlobalObject(s_scratch);
        callOperation(operation, s_scratch, op1Regs, op2Regs);
        if (dstRegs)
            boxBoolean(returnValueGPR, dstRegs);
        silentFill(m_replayAllocator, dstRegs.payloadGPR());
        return true;
    };

    if (handleConstantIntOperandSlow(op1, op1Regs, fpRegT0, op2Regs, fpRegT1))
        return;
    if (handleConstantIntOperandSlow(op2, op2Regs, fpRegT1, op1Regs, fpRegT0))
        return;

    linkSlowCase(iter); // LHS is not Int.

    JumpList slows;
    JIT_COMMENT(*this, "checking for both doubles");
    slows.append(branchIfNotNumber(op1Regs, s_scratch));
    slows.append(branchIfNotNumber(op2Regs, s_scratch));
    // We only have to check if one side is an Int32 as we already must have failed the isInt32(op1) && isInt32(op2) from the fast path.
    slows.append(branchIfInt32(op2Regs));
    unboxDouble(op1Regs, s_scratch, fpRegT0);
    unboxDouble(op2Regs, s_scratch, fpRegT1);

    emitDoubleCompare(fpRegT0, fpRegT1);

    jump().linkTo(fastPathResumePoint(), this);

    slows.link(*this);

    linkSlowCase(iter); // RHS is not Int.
    silentSpill(m_replayAllocator, dstRegs.payloadGPR());
    loadGlobalObject(s_scratch);
    callOperation(operation, s_scratch, op1Regs, op2Regs);
    if (dstRegs)
        boxBoolean(returnValueGPR, dstRegs);
    silentFill(m_replayAllocator, dstRegs.payloadGPR());
}

void LOLJIT::emit_op_less(const JSInstruction* currentInstruction)
{
    emitCompare<OpLess>(currentInstruction, LessThan);
}

void LOLJIT::emit_op_lesseq(const JSInstruction* currentInstruction)
{
    emitCompare<OpLesseq>(currentInstruction, LessThanOrEqual);
}

void LOLJIT::emit_op_greater(const JSInstruction* currentInstruction)
{
    emitCompare<OpGreater>(currentInstruction, GreaterThan);
}

void LOLJIT::emit_op_greatereq(const JSInstruction* currentInstruction)
{
    emitCompare<OpGreatereq>(currentInstruction, GreaterThanOrEqual);
}

void LOLJIT::emitSlow_op_less(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareSlow<OpLess>(currentInstruction, DoubleLessThanAndOrdered, operationCompareLess, iter);
}

void LOLJIT::emitSlow_op_lesseq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareSlow<OpLesseq>(currentInstruction, DoubleLessThanOrEqualAndOrdered, operationCompareLessEq, iter);
}

void LOLJIT::emitSlow_op_greater(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareSlow<OpGreater>(currentInstruction, DoubleGreaterThanAndOrdered, operationCompareGreater, iter);
}

void LOLJIT::emitSlow_op_greatereq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareSlow<OpGreatereq>(currentInstruction, DoubleGreaterThanOrEqualAndOrdered, operationCompareGreaterEq, iter);
}

// Conversion

void LOLJIT::emit_op_to_number(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumber>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operand ] = allocations.uses;
    auto [ dst ] = allocations.defs;

    UnaryArithProfile* arithProfile = &m_unlinkedCodeBlock->unaryArithProfile(bytecode.m_profileIndex);

    auto isInt32 = branchIfInt32(operand);
    addSlowCase(branchIfNotNumber(operand, InvalidGPRReg));
    if (arithProfile && shouldEmitProfiling())
        arithProfile->emitUnconditionalSet(*this, UnaryArithProfile::observedNumberBits());
    isInt32.link(this);
    moveValueRegs(operand, dst);
}

void LOLJIT::emit_op_to_string(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToString>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    addSlowCase(branchIfNotCell(operandRegs));
    addSlowCase(branchIfNotString(operandRegs.payloadGPR()));

    moveValueRegs(operandRegs, dstRegs);
}

void LOLJIT::emit_op_to_numeric(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumeric>();
    UnaryArithProfile* arithProfile = &m_unlinkedCodeBlock->unaryArithProfile(bytecode.m_profileIndex);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    auto isInt32 = branchIfInt32(operandRegs);

    Jump isNotCell = branchIfNotCell(operandRegs);
    addSlowCase(branchIfNotHeapBigInt(operandRegs.payloadGPR()));
    if (arithProfile && shouldEmitProfiling())
        move(TrustedImm32(UnaryArithProfile::observedNonNumberBits()), s_scratch);
    Jump isBigInt = jump();

    isNotCell.link(this);
    addSlowCase(branchIfNotNumber(operandRegs, s_scratch));
    if (arithProfile && shouldEmitProfiling())
        move(TrustedImm32(UnaryArithProfile::observedNumberBits()), s_scratch);
    isBigInt.link(this);

    if (arithProfile && shouldEmitProfiling())
        arithProfile->emitUnconditionalSet(*this, s_scratch);

    isInt32.link(this);
    moveValueRegs(operandRegs, dstRegs);
}

void LOLJIT::emit_op_to_object(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToObject>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    addSlowCase(branchIfNotCell(operandRegs));
    addSlowCase(branchIfNotObject(operandRegs.payloadGPR()));

    emitValueProfilingSite(bytecode, operandRegs);
    moveValueRegs(operandRegs, dstRegs);
}

template<typename Op>
void LOLJIT::emitRightShiftFastPath(const JSInstruction* currentInstruction, JITRightShiftGenerator::ShiftType snippetShiftType)
{
    // FIXME: This allocates registers for constants but don't even use them if it's a constant.
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    JITRightShiftGenerator gen(leftOperand, rightOperand, destRegs, leftRegs, rightRegs, fpRegT0, s_scratch, snippetShiftType);

    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().link(this);

    addSlowCase(gen.slowPathJumpList());
}

void LOLJIT::emit_op_rshift(const JSInstruction* currentInstruction)
{
    emitRightShiftFastPath<OpRshift>(currentInstruction, JITRightShiftGenerator::SignedShift);
}

void LOLJIT::emit_op_urshift(const JSInstruction* currentInstruction)
{
    emitRightShiftFastPath<OpUrshift>(currentInstruction, JITRightShiftGenerator::UnsignedShift);
}

void LOLJIT::emit_op_lshift(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpLshift>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    JITLeftShiftGenerator gen(leftOperand, rightOperand, destRegs, leftRegs, rightRegs, s_scratch);

    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().link(this);

    addSlowCase(gen.slowPathJumpList());
}

template<typename Op, typename SnippetGenerator>
void LOLJIT::emitBitBinaryOpFastPath(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ resultRegs ] = allocations.defs;

    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    if constexpr (Op::opcodeID == op_bitand || Op::opcodeID == op_bitor || Op::opcodeID == op_bitxor) {
        leftOperand = SnippetOperand(bytecode.m_operandTypes.first());
        rightOperand = SnippetOperand(bytecode.m_operandTypes.second());
    }

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    SnippetGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, s_scratch);

    gen.generateFastPath(*this);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().link(this);

    addSlowCase(gen.slowPathJumpList());

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_bitand(const JSInstruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitand, JITBitAndGenerator>(currentInstruction);
}

void LOLJIT::emit_op_bitor(const JSInstruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitor, JITBitOrGenerator>(currentInstruction);
}

void LOLJIT::emit_op_bitxor(const JSInstruction* currentInstruction)
{
    emitBitBinaryOpFastPath<OpBitxor, JITBitXorGenerator>(currentInstruction);
}

template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
void LOLJIT::emitMathICFast(JITBinaryMathIC<Generator>* mathIC, const JSInstruction* currentInstruction, ProfiledFunction profiledFunction, NonProfiledFunction nonProfiledFunction)
{
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    SnippetOperand leftOperand(bytecode.m_operandTypes.first());
    SnippetOperand rightOperand(bytecode.m_operandTypes.second());

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    mathIC->m_generator = Generator(leftOperand, rightOperand, destRegs, leftRegs, rightRegs, fpRegT0, fpRegT1, s_scratch);

    ASSERT(!(Generator::isLeftOperandValidConstant(leftOperand) && Generator::isRightOperandValidConstant(rightOperand)));

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = label();
#endif

    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.add(currentInstruction, makeUniqueRef<MathICGenerationState>()).iterator->value.get();

    bool generatedInlineCode = mathIC->generateInline(*this, mathICGenerationState);
    if (!generatedInlineCode) {
        // FIXME: We should consider doing a handler IC for math bytecodes.
        BinaryArithProfile* arithProfile = mathIC->arithProfile();
        silentSpill(m_fastAllocator, destRegs.gpr());
        loadGlobalObject(s_scratch);
        if (arithProfile && shouldEmitProfiling())
            callOperationWithResult(profiledFunction, destRegs, s_scratch, leftRegs, rightRegs, TrustedImmPtr(arithProfile));
        else
            callOperationWithResult(nonProfiledFunction, destRegs, s_scratch, leftRegs, rightRegs);
        silentFill(m_fastAllocator, destRegs.gpr());
    } else
        addSlowCase(mathICGenerationState.slowPathJumps);

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(inlineEnd).taggedPtr<char*>() - linkBuffer.locationOf(inlineStart).taggedPtr<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif
}

template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
void LOLJIT::emitMathICSlow(JITBinaryMathIC<Generator>* mathIC, const JSInstruction* currentInstruction, ProfiledRepatchFunction profiledRepatchFunction, ProfiledFunction profiledFunction, RepatchFunction repatchFunction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ leftRegs, rightRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    if (!hasAnySlowCases(iter))
        return;

    linkAllSlowCases(iter);

    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
    mathICGenerationState.slowPathStart = label();

    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;

    SnippetOperand leftOperand(bytecode.m_operandTypes.first());
    SnippetOperand rightOperand(bytecode.m_operandTypes.second());

    if (isOperandConstantInt(op1))
        leftOperand.setConstInt32(getOperandConstantInt(op1));
    else if (isOperandConstantInt(op2))
        rightOperand.setConstInt32(getOperandConstantInt(op2));

    ASSERT(!(Generator::isLeftOperandValidConstant(leftOperand) && Generator::isRightOperandValidConstant(rightOperand)));

#if ENABLE(MATH_IC_STATS)
    auto slowPathStart = label();
#endif

    silentSpill(m_replayAllocator, destRegs.gpr());

    BinaryArithProfile* arithProfile = mathIC->arithProfile();
    loadGlobalObject(s_scratch);
    if (arithProfile && shouldEmitProfiling()) {
        if (mathICGenerationState.shouldSlowPathRepatch)
            mathICGenerationState.slowPathCall = callOperationWithResult(std::bit_cast<J_JITOperation_GJJMic>(profiledRepatchFunction), destRegs, s_scratch, leftRegs, rightRegs, TrustedImmPtr(mathIC));
        else
            mathICGenerationState.slowPathCall = callOperationWithResult(profiledFunction, destRegs, s_scratch, leftRegs, rightRegs, TrustedImmPtr(arithProfile));
    } else
        mathICGenerationState.slowPathCall = callOperationWithResult(std::bit_cast<J_JITOperation_GJJMic>(repatchFunction), destRegs, s_scratch, leftRegs, rightRegs, TrustedImmPtr(mathIC));

    silentFill(m_replayAllocator, destRegs.gpr());

#if ENABLE(MATH_IC_STATS)
    auto slowPathEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(slowPathEnd).taggedPtr<char*>() - linkBuffer.locationOf(slowPathStart).taggedPtr<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
        mathIC->finalizeInlineCode(mathICGenerationState, linkBuffer);
    });
}

template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
void LOLJIT::emitMathICFast(JITUnaryMathIC<Generator>* mathIC, const JSInstruction* currentInstruction, ProfiledFunction profiledFunction, NonProfiledFunction nonProfiledFunction)
{
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ srcRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = label();
#endif

    mathIC->m_generator = Generator(destRegs, srcRegs, s_scratch);

    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.add(currentInstruction, makeUniqueRef<MathICGenerationState>()).iterator->value.get();

    bool generatedInlineCode = mathIC->generateInline(*this, mathICGenerationState);
    if (!generatedInlineCode) {
        UnaryArithProfile* arithProfile = mathIC->arithProfile();
        // FIXME: We should consider doing a handler IC for math bytecodes.
        silentSpill(m_fastAllocator, destRegs.gpr());
        loadGlobalObject(s_scratch);
        if (arithProfile && shouldEmitProfiling())
            callOperationWithResult(profiledFunction, destRegs, s_scratch, srcRegs, TrustedImmPtr(arithProfile));
        else
            callOperationWithResult(nonProfiledFunction, destRegs, s_scratch, srcRegs);
        silentFill(m_fastAllocator, destRegs.gpr());
    } else
        addSlowCase(mathICGenerationState.slowPathJumps);

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(inlineEnd).taggedPtr<char*>() - linkBuffer.locationOf(inlineStart).taggedPtr<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif
}

template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
void LOLJIT::emitMathICSlow(JITUnaryMathIC<Generator>* mathIC, const JSInstruction* currentInstruction, ProfiledRepatchFunction profiledRepatchFunction, ProfiledFunction profiledFunction, RepatchFunction repatchFunction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ srcRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    if (!hasAnySlowCases(iter))
        return;

    linkAllSlowCases(iter);

    MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
    mathICGenerationState.slowPathStart = label();

#if ENABLE(MATH_IC_STATS)
    auto slowPathStart = label();
#endif

    silentSpill(m_replayAllocator, destRegs.gpr());

    UnaryArithProfile* arithProfile = mathIC->arithProfile();
    loadGlobalObject(s_scratch);
    if (arithProfile && shouldEmitProfiling()) {
        if (mathICGenerationState.shouldSlowPathRepatch)
            mathICGenerationState.slowPathCall = callOperationWithResult(reinterpret_cast<J_JITOperation_GJMic>(profiledRepatchFunction), destRegs, s_scratch, srcRegs, TrustedImmPtr(mathIC));
        else
            mathICGenerationState.slowPathCall = callOperationWithResult(profiledFunction, destRegs, s_scratch, srcRegs, TrustedImmPtr(arithProfile));
    } else
        mathICGenerationState.slowPathCall = callOperationWithResult(reinterpret_cast<J_JITOperation_GJMic>(repatchFunction), destRegs, s_scratch, srcRegs, TrustedImmPtr(mathIC));

    silentFill(m_replayAllocator, destRegs.gpr());

#if ENABLE(MATH_IC_STATS)
    auto slowPathEnd = label();
    addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = linkBuffer.locationOf(slowPathEnd).taggedPtr<char*>() - linkBuffer.locationOf(slowPathStart).taggedPtr<char*>();
        mathIC->m_generatedCodeSize += size;
    });
#endif

    addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        MathICGenerationState& mathICGenerationState = m_instructionToMathICGenerationState.find(currentInstruction)->value.get();
        mathIC->finalizeInlineCode(mathICGenerationState, linkBuffer);
    });
}

void LOLJIT::emit_op_add(const JSInstruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpAdd>().m_profileIndex);
    JITAddIC* addIC = m_mathICs.addJITAddIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, addIC);
    emitMathICFast<OpAdd>(addIC, currentInstruction, operationValueAddProfiled, operationValueAdd);
}

void LOLJIT::emitSlow_op_add(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    JITAddIC* addIC = std::bit_cast<JITAddIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpAdd>(addIC, currentInstruction, operationValueAddProfiledOptimize, operationValueAddProfiled, operationValueAddOptimize, iter);
}

void LOLJIT::emit_op_mul(const JSInstruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpMul>().m_profileIndex);
    JITMulIC* mulIC = m_mathICs.addJITMulIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, mulIC);
    emitMathICFast<OpMul>(mulIC, currentInstruction, operationValueMulProfiled, operationValueMul);
}

void LOLJIT::emitSlow_op_mul(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    JITMulIC* mulIC = std::bit_cast<JITMulIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpMul>(mulIC, currentInstruction, operationValueMulProfiledOptimize, operationValueMulProfiled, operationValueMulOptimize, iter);
}

void LOLJIT::emit_op_sub(const JSInstruction* currentInstruction)
{
    BinaryArithProfile* arithProfile = &m_unlinkedCodeBlock->binaryArithProfile(currentInstruction->as<OpSub>().m_profileIndex);
    JITSubIC* subIC = m_mathICs.addJITSubIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, subIC);
    emitMathICFast<OpSub>(subIC, currentInstruction, operationValueSubProfiled, operationValueSub);
}

void LOLJIT::emitSlow_op_sub(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    JITSubIC* subIC = std::bit_cast<JITSubIC*>(m_instructionToMathIC.get(currentInstruction));
    emitMathICSlow<OpSub>(subIC, currentInstruction, operationValueSubProfiledOptimize, operationValueSubProfiled, operationValueSubOptimize, iter);
}

void LOLJIT::emit_op_negate(const JSInstruction* currentInstruction)
{
    UnaryArithProfile* arithProfile = &m_unlinkedCodeBlock->unaryArithProfile(currentInstruction->as<OpNegate>().m_profileIndex);
    JITNegIC* negateIC = m_mathICs.addJITNegIC(arithProfile);
    m_instructionToMathIC.add(currentInstruction, negateIC);
    // FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
    emitMathICFast<OpNegate>(negateIC, currentInstruction, operationArithNegateProfiled, operationArithNegate);
}

void LOLJIT::emitSlow_op_negate(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    JITNegIC* negIC = std::bit_cast<JITNegIC*>(m_instructionToMathIC.get(currentInstruction));
    // FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
    emitMathICSlow<OpNegate>(negIC, currentInstruction, operationArithNegateProfiledOptimize, operationArithNegateProfiled, operationArithNegateOptimize, iter);
}

void LOLJIT::emit_op_bitnot(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpBitnot>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    addSlowCase(branchIfNotInt32(operandRegs));
    not32(operandRegs.payloadGPR(), dstRegs.payloadGPR());
#if USE(JSVALUE64)
    boxInt32(dstRegs.payloadGPR(), dstRegs);
#endif
    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_get_from_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();

    using Metadata = OpGetFromScope::Metadata;

    GPRReg thunkMetadataGPR = BaselineJITRegisters::GetFromScope::metadataGPR;
    GPRReg thunkScopeGPR = BaselineJITRegisters::GetFromScope::scopeGPR;
    GPRReg thunkBytecodeOffsetGPR = BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR;

    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;
    auto [ scratchRegs ] = allocations.scratches;

    // FIXME: In theory we don't need this scratch if it's a ClosureVar but that complicates the bookkeeping and may change later down the track.
    GPRReg metadataGPR = scratchRegs.gpr();
    GPRReg scopeGPR = scopeRegs.payloadGPR();

    if (profiledResolveType == ClosureVar) {
        loadPtrFromMetadata(bytecode, Metadata::offsetOfOperand(), s_scratch);
        loadValue(BaseIndex(scopeRegs.payloadGPR(), s_scratch, TimesEight, JSLexicalEnvironment::offsetOfVariables()), destRegs);
    } else {
        // Inlined fast path for common types.
        constexpr size_t metadataMinAlignment = alignof(Metadata);
        constexpr size_t metadataPointerAlignment = alignof(void*);
        static_assert(!(metadataPointerAlignment % metadataMinAlignment));
        static_assert(!(Metadata::offsetOfGetPutInfo() % metadataMinAlignment));
        static_assert(!(Metadata::offsetOfStructureID() % metadataMinAlignment));
        static_assert(!(Metadata::offsetOfOperand() % metadataPointerAlignment));
        auto metadataAddress = computeBaseAddressForMetadata<metadataMinAlignment>(bytecode, metadataGPR);

        auto getPutInfoAddress = metadataAddress.withOffset(Metadata::offsetOfGetPutInfo());
        auto structureIDAddress = metadataAddress.withOffset(Metadata::offsetOfStructureID());
        auto operandAddress = metadataAddress.withOffset(Metadata::offsetOfOperand());

        load32(getPutInfoAddress, s_scratch);
        and32(TrustedImm32(GetPutInfo::typeBits), s_scratch); // Load ResolveType into s_scratch

        switch (profiledResolveType) {
        case GlobalProperty: {
            addSlowCase(branch32(NotEqual, s_scratch, TrustedImm32(profiledResolveType)));
            load32(structureIDAddress, s_scratch);
            addSlowCase(branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), s_scratch));
            loadPtr(operandAddress, s_scratch);
            loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), destRegs.payloadGPR());
            negPtr(s_scratch);
            loadValue(BaseIndex(destRegs.payloadGPR(), s_scratch, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), destRegs);
            break;
        }
        case GlobalVar: {
            addSlowCase(branch32(NotEqual, s_scratch, TrustedImm32(profiledResolveType)));
            loadPtr(operandAddress, s_scratch);
            loadValue(Address(s_scratch), destRegs);
            break;
        }
        case GlobalLexicalVar: {
            addSlowCase(branch32(NotEqual, s_scratch, TrustedImm32(profiledResolveType)));
            loadPtr(operandAddress, s_scratch);
            loadValue(Address(s_scratch), destRegs);
            addSlowCase(branchIfEmpty(destRegs));
            break;
        }
        default: {

            MacroAssemblerCodeRef<JITThunkPtrTag> code;
            if (profiledResolveType == ClosureVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<ClosureVarWithVarInjectionChecks>);
            // FIXME: Aren't these three handled above and therefore unreachable?
            if (profiledResolveType == GlobalProperty)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalProperty>);
            if (profiledResolveType == GlobalVar)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);
            if (profiledResolveType == GlobalLexicalVar)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVar>);
            else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVarWithVarInjectionChecks>);
            else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
            else
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);

            // FIXME: This only needs to save the BaselineJITRegisters::GetFromScope registers.
            silentSpill(m_fastAllocator, destRegs.gpr());
            if (metadataAddress.base != thunkMetadataGPR) {
                // Materialize metadataGPR for the thunks if we didn't already.
                uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);
                addPtr(TrustedImm32(metadataOffset), GPRInfo::metadataTableRegister, thunkMetadataGPR);
            }
            // Thunks expect scopeGPR to have the scope
            move(scopeRegs.payloadGPR(), thunkScopeGPR);
            move(TrustedImm32(bytecodeOffset), thunkBytecodeOffsetGPR);
            nearCallThunk(CodeLocationLabel { code.retaggedCode<NoPtrTag>() });
            // Thunk returns result in returnValueJSR, move to the allocated register

            moveValueRegs(returnValueJSR, destRegs);
            silentFill(m_fastAllocator, destRegs.gpr());
            break;
        }
        }
    }

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, destRegs);
    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emitSlow_op_get_from_scope(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    if (!hasAnySlowCases(iter)) {
        m_replayAllocator.releaseScratches(allocations);
        return;
    }

    linkAllSlowCases(iter);

    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();

    GPRReg scopeGPR = scopeRegs.payloadGPR();

    GPRReg thunkMetadataGPR = BaselineJITRegisters::GetFromScope::metadataGPR;
    GPRReg thunkScopeGPR = BaselineJITRegisters::GetFromScope::scopeGPR;
    GPRReg thunkBytecodeOffsetGPR = BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR;

    MacroAssemblerCodeRef<JITThunkPtrTag> code;
    if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<ClosureVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalVar)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);
    else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalProperty)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalProperty>);
    else if (profiledResolveType == GlobalLexicalVar)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVar>);
    else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
    else
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);


    silentSpill(m_replayAllocator, destRegs.gpr());
    // Thunks expect scopeGPR to have the scope
    move(scopeGPR, thunkScopeGPR);
    // Materialize metadataGPR if we didn't already. Has to happen after thunkScopeGPR.
    uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);
    addPtr(TrustedImm32(metadataOffset), GPRInfo::metadataTableRegister, thunkMetadataGPR);
    move(TrustedImm32(bytecodeOffset), thunkBytecodeOffsetGPR);
    nearCallThunk(CodeLocationLabel { code.retaggedCode<NoPtrTag>() });
    // Thunk returns result in returnValueJSR, move to allocated register
    moveValueRegs(returnValueJSR, destRegs);
    silentFill(m_replayAllocator, destRegs.gpr());
    m_replayAllocator.releaseScratches(allocations);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(JIT) && USE(JSVALUE64)

