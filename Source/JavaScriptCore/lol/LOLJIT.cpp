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
#include "BytecodeGenerator.h"
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
#include "JITSizeStatistics.h"
#include "JITThunks.h"
#include "LLIntEntrypoint.h"
#include "LLIntThunks.h"
#include "LOLJITOperations.h"
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
        DEFINE_OP(op_check_tdz)
        DEFINE_OP(op_identity_with_profile)
        DEFINE_OP(op_debug)
        DEFINE_OP(op_del_by_id)
        DEFINE_OP(op_del_by_val)
        DEFINE_OP(op_div)
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

        REPLAY_ALLOCATION_FOR_OP(op_mov, OpMov)
        REPLAY_ALLOCATION_FOR_OP(op_is_empty, OpIsEmpty)
        REPLAY_ALLOCATION_FOR_OP(op_typeof_is_undefined, OpTypeofIsUndefined)
        REPLAY_ALLOCATION_FOR_OP(op_is_undefined_or_null, OpIsUndefinedOrNull)
        REPLAY_ALLOCATION_FOR_OP(op_is_boolean, OpIsBoolean)
        REPLAY_ALLOCATION_FOR_OP(op_is_number, OpIsNumber)
        REPLAY_ALLOCATION_FOR_OP(op_is_big_int, OpIsBigInt)
        REPLAY_ALLOCATION_FOR_OP(op_is_object, OpIsObject)
        REPLAY_ALLOCATION_FOR_OP(op_is_cell_with_type, OpIsCellWithType)
        REPLAY_ALLOCATION_FOR_OP(op_has_structure_with_flags, OpHasStructureWithFlags)
        REPLAY_ALLOCATION_FOR_OP(op_jmp, OpJmp)
        REPLAY_ALLOCATION_FOR_OP(op_jtrue, OpJtrue)
        REPLAY_ALLOCATION_FOR_OP(op_jfalse, OpJfalse)
        REPLAY_ALLOCATION_FOR_OP(op_jeq_null, OpJeqNull)
        REPLAY_ALLOCATION_FOR_OP(op_jneq_null, OpJneqNull)
        REPLAY_ALLOCATION_FOR_OP(op_jundefined_or_null, OpJundefinedOrNull)
        REPLAY_ALLOCATION_FOR_OP(op_jnundefined_or_null, OpJnundefinedOrNull)
        REPLAY_ALLOCATION_FOR_OP(op_jeq_ptr, OpJeqPtr)
        REPLAY_ALLOCATION_FOR_OP(op_jneq_ptr, OpJneqPtr)
        REPLAY_ALLOCATION_FOR_OP(op_jbelow, OpJbelow)
        REPLAY_ALLOCATION_FOR_OP(op_jbeloweq, OpJbeloweq)
        REPLAY_ALLOCATION_FOR_OP(op_create_lexical_environment, OpCreateLexicalEnvironment)
        REPLAY_ALLOCATION_FOR_OP(op_create_direct_arguments, OpCreateDirectArguments)
        REPLAY_ALLOCATION_FOR_OP(op_create_scoped_arguments, OpCreateScopedArguments)
        REPLAY_ALLOCATION_FOR_OP(op_create_cloned_arguments, OpCreateClonedArguments)
        REPLAY_ALLOCATION_FOR_OP(op_new_array, OpNewArray)
        REPLAY_ALLOCATION_FOR_OP(op_new_array_with_size, OpNewArrayWithSize)
        REPLAY_ALLOCATION_FOR_OP(op_new_func, OpNewFunc)
        REPLAY_ALLOCATION_FOR_OP(op_new_func_exp, OpNewFuncExp)
        REPLAY_ALLOCATION_FOR_OP(op_new_generator_func, OpNewGeneratorFunc)
        REPLAY_ALLOCATION_FOR_OP(op_new_generator_func_exp, OpNewGeneratorFuncExp)
        REPLAY_ALLOCATION_FOR_OP(op_new_async_func, OpNewAsyncFunc)
        REPLAY_ALLOCATION_FOR_OP(op_new_async_func_exp, OpNewAsyncFuncExp)
        REPLAY_ALLOCATION_FOR_OP(op_new_async_generator_func, OpNewAsyncGeneratorFunc)
        REPLAY_ALLOCATION_FOR_OP(op_new_async_generator_func_exp, OpNewAsyncGeneratorFuncExp)
        REPLAY_ALLOCATION_FOR_OP(op_new_reg_exp, OpNewRegExp)

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

template<typename Op>
    requires (LOLJIT::isImplemented(Op::opcodeID))
void LOLJIT::emitCommonSlowPathSlowCaseCall(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter, SlowPathFunction stub)
{
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);

    linkAllSlowCases(iter);

    silentSpill(m_replayAllocator, allocations);
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

void LOLJIT::emit_op_mov(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpMov>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ sourceRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    moveValueRegs(sourceRegs, destRegs);

    m_fastAllocator.releaseScratches(allocations);
}

// Comparison bytecodes


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

    silentSpill(m_replayAllocator, allocations);
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

    silentSpill(m_replayAllocator, allocations);
    loadGlobalObject(s_scratch);
    callOperation(operationCompareEq, s_scratch, leftRegs, rightRegs);
    xor32(TrustedImm32(0x1), returnValueGPR);
    boxBoolean(returnValueGPR, destRegs);
    silentFill(m_replayAllocator, destRegs.payloadGPR());
}

void LOLJIT::emitLoadCharacterString(RegisterID src, RegisterID dst, JumpList& failures)
{
    failures.append(branchIfNotString(src));
    loadPtr(MacroAssembler::Address(src, JSString::offsetOfValue()), dst);
    failures.append(branchIfRopeStringImpl(dst));
    failures.append(branch32(NotEqual, MacroAssembler::Address(dst, StringImpl::lengthMemoryOffset()), TrustedImm32(1)));

    // FIXME: We could deduplicate the String's data load if we had an extra scratch but we'd have find one for all our callers, which for emitCompareImpl likely entails teaching the allocator about constants.
    auto is16Bit = branchTest32(Zero, Address(dst, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));
    loadPtr(MacroAssembler::Address(dst, StringImpl::dataOffset()), dst);
    load8(MacroAssembler::Address(dst, 0), dst);
    auto done = jump();
    is16Bit.link(this);
    loadPtr(MacroAssembler::Address(dst, StringImpl::dataOffset()), dst);
    load16(MacroAssembler::Address(dst, 0), dst);
    done.link(this);
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
        // FIXME: We could deduplicate the String's data load in emitLoadCharacterString if we had an extra scratch but we'd have to teach the register allocator about constants to do that unless we wanted to have the scratch in all cases, which doesn't seem worth it.
        emitLoadCharacterString(rightRegs.payloadGPR(), s_scratch, failures);
        addSlowCase(failures);
        emitCompare(commute(cond), JSValueRegs { s_scratch }, Imm32(asString(getConstantOperand(left))->tryGetValue(disallowAllocation).data[0]));
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
    emitCompareSlowImpl(allocations, op1, op1Regs, op2, op2Regs, dstRegs, operation, iter, emitDoubleCompare);
}

// FIXME: Maybe this should take a shouldBox template parameter instead of relying on !dstRegs
template<typename SlowOperation>
void LOLJIT::emitCompareSlowImpl(const auto& allocations, VirtualRegister lhs, JSValueRegs lhsRegs, VirtualRegister rhs, JSValueRegs rhsRegs, JSValueRegs dstRegs, SlowOperation operation, Vector<SlowCaseEntry>::iterator& iter, const Invocable<void(FPRReg, FPRReg)> auto& emitDoubleCompare)
{
    // We generate inline code for the following cases in the slow path:
    // - floating-point number to constant int immediate
    // - constant int immediate to floating-point number
    // - floating-point number to floating-point number.
    if (isOperandConstantChar(lhs) || isOperandConstantChar(rhs)) {
        linkAllSlowCases(iter);

        silentSpill(m_replayAllocator, allocations);
        loadGlobalObject(s_scratch);
        callOperation(operation, s_scratch, lhsRegs, rhsRegs);
        if (dstRegs)
            boxBoolean(returnValueGPR, dstRegs);
        silentFill(m_replayAllocator, dstRegs.payloadGPR());
        return;
    }

    constexpr FPRReg lhsFPR = fpRegT0;
    constexpr FPRReg rhsFPR = fpRegT1;
    auto handleConstantIntOperandSlow = [&](VirtualRegister maybeConstantOperand, JSValueRegs constantRegs, FPRReg constantFPR, JSValueRegs nonConstantRegs, FPRReg nonConstantFPR) {
        if (!isOperandConstantInt(maybeConstantOperand))
            return false;
        linkAllSlowCases(iter);

        Jump fail1 = branchIfNotNumber(nonConstantRegs, s_scratch);
        unboxDouble(nonConstantRegs.payloadGPR(), s_scratch, nonConstantFPR);

        convertInt32ToDouble(constantRegs.payloadGPR(), constantFPR);

        // We compare these in their original order since we cannot invert double comparisons (due to NaNs)
        emitDoubleCompare(lhsFPR, rhsFPR);

        jump().linkTo(fastPathResumePoint(), this);

        fail1.link(this);

        silentSpill(m_replayAllocator, allocations);
        loadGlobalObject(s_scratch);
        callOperation(operation, s_scratch, lhsRegs, rhsRegs);
        if (dstRegs)
            boxBoolean(returnValueGPR, dstRegs);
        silentFill(m_replayAllocator, dstRegs.payloadGPR());
        return true;
    };

    if (handleConstantIntOperandSlow(lhs, lhsRegs, lhsFPR, rhsRegs, rhsFPR))
        return;
    if (handleConstantIntOperandSlow(rhs, rhsRegs, rhsFPR, lhsRegs, lhsFPR))
        return;

    linkSlowCase(iter); // LHS is not Int.

    JumpList slows;
    JIT_COMMENT(*this, "checking for both doubles");
    slows.append(branchIfNotNumber(lhsRegs, s_scratch));
    slows.append(branchIfNotNumber(rhsRegs, s_scratch));
    // We only have to check if rhs is an Int32 as we already must have failed the isInt32(lhs) from the fast path.
    slows.append(branchIfInt32(rhsRegs));
    unboxDouble(lhsRegs, s_scratch, lhsFPR);
    unboxDouble(rhsRegs, s_scratch, rhsFPR);

    emitDoubleCompare(lhsFPR, rhsFPR);

    jump().linkTo(fastPathResumePoint(), this);

    slows.link(*this);

    linkSlowCase(iter); // RHS is not Int.
    silentSpill(m_replayAllocator, allocations);
    loadGlobalObject(s_scratch);
    callOperation(operation, s_scratch, lhsRegs, rhsRegs);
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

// Jump compare bytecodes

template<typename Op>
void LOLJIT::emitCompareAndJump(const JSInstruction* instruction, RelationalCondition condition)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    unsigned target = jumpTarget(instruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ op1Regs, op2Regs ] = allocations.uses;

    auto emitCompareAndJump = [&](RelationalCondition cond, JSValueRegs leftJSR, auto right) {
        addJump(branch32(cond, leftJSR.payloadGPR(), right), target);
    };
    emitCompareImpl(op1, op1Regs, op2, op2Regs, condition, emitCompareAndJump);
    m_fastAllocator.releaseScratches(allocations);
}

template<typename Op, typename SlowOperation>
void LOLJIT::emitCompareAndJumpSlow(const JSInstruction* instruction, DoubleCondition condition, SlowOperation operation, bool invertOperationResult, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = instruction->as<Op>();
    unsigned target = jumpTarget(instruction, bytecode.m_targetLabel);

    VirtualRegister op1 = bytecode.m_lhs;
    VirtualRegister op2 = bytecode.m_rhs;
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ op1Regs, op2Regs ] = allocations.uses;

    auto emitDoubleCompare = [&](FPRReg left, FPRReg right) {
        emitJumpSlowToHot(branchDouble(condition, left, right), target);
    };
    // Pass empty dstRegs since we're doing a jump, not storing a result, result will be in returnValueGPR
    emitCompareSlowImpl(allocations, op1, op1Regs, op2, op2Regs, JSValueRegs(), operation, iter, emitDoubleCompare);

    emitJumpSlowToHot(branchTest32(invertOperationResult ? Zero : NonZero, returnValueGPR), target);

    m_replayAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jless(const JSInstruction* currentInstruction)
{
    emitCompareAndJump<OpJless>(currentInstruction, LessThan);
}

void LOLJIT::emit_op_jlesseq(const JSInstruction* currentInstruction)
{
    emitCompareAndJump<OpJlesseq>(currentInstruction, LessThanOrEqual);
}

void LOLJIT::emit_op_jgreater(const JSInstruction* currentInstruction)
{
    emitCompareAndJump<OpJgreater>(currentInstruction, GreaterThan);
}

void LOLJIT::emit_op_jgreatereq(const JSInstruction* currentInstruction)
{
    emitCompareAndJump<OpJgreatereq>(currentInstruction, GreaterThanOrEqual);
}

void LOLJIT::emit_op_jnless(const JSInstruction* currentInstruction)
{
    emitCompareAndJump<OpJnless>(currentInstruction, GreaterThanOrEqual);
}

void LOLJIT::emit_op_jnlesseq(const JSInstruction* currentInstruction)
{
    emitCompareAndJump<OpJnlesseq>(currentInstruction, GreaterThan);
}

void LOLJIT::emit_op_jngreater(const JSInstruction* currentInstruction)
{
    emitCompareAndJump<OpJngreater>(currentInstruction, LessThanOrEqual);
}

void LOLJIT::emit_op_jngreatereq(const JSInstruction* currentInstruction)
{
    emitCompareAndJump<OpJngreatereq>(currentInstruction, LessThan);
}

void LOLJIT::emitSlow_op_jless(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareAndJumpSlow<OpJless>(currentInstruction, DoubleLessThanAndOrdered, operationCompareLess, false, iter);
}

void LOLJIT::emitSlow_op_jlesseq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareAndJumpSlow<OpJlesseq>(currentInstruction, DoubleLessThanOrEqualAndOrdered, operationCompareLessEq, false, iter);
}

void LOLJIT::emitSlow_op_jgreater(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareAndJumpSlow<OpJgreater>(currentInstruction, DoubleGreaterThanAndOrdered, operationCompareGreater, false, iter);
}

void LOLJIT::emitSlow_op_jgreatereq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareAndJumpSlow<OpJgreatereq>(currentInstruction, DoubleGreaterThanOrEqualAndOrdered, operationCompareGreaterEq, false, iter);
}

void LOLJIT::emitSlow_op_jnless(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareAndJumpSlow<OpJnless>(currentInstruction, DoubleGreaterThanOrEqualOrUnordered, operationCompareLess, true, iter);
}

void LOLJIT::emitSlow_op_jnlesseq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareAndJumpSlow<OpJnlesseq>(currentInstruction, DoubleGreaterThanOrUnordered, operationCompareLessEq, true, iter);
}

void LOLJIT::emitSlow_op_jngreater(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareAndJumpSlow<OpJngreater>(currentInstruction, DoubleLessThanOrEqualOrUnordered, operationCompareGreater, true, iter);
}

void LOLJIT::emitSlow_op_jngreatereq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitCompareAndJumpSlow<OpJngreatereq>(currentInstruction, DoubleLessThanOrUnordered, operationCompareGreaterEq, true, iter);
}

// Strict equality jumps

template<typename Op>
void LOLJIT::emitStrictEqJumpImpl(const JSInstruction* currentInstruction, RelationalCondition condition)
{
    auto bytecode = currentInstruction->as<Op>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ lhsRegs, rhsRegs ] = allocations.uses;

    // Fast path: both are integers
    addSlowCase(branchIfNotInt32(lhsRegs));
    addSlowCase(branchIfNotInt32(rhsRegs));

    addJump(branch32(condition, lhsRegs.payloadGPR(), rhsRegs.payloadGPR()), target);

    m_fastAllocator.releaseScratches(allocations);
}

template<typename Op>
void LOLJIT::emitStrictEqJumpSlowImpl(const JSInstruction* currentInstruction, ResultCondition condition, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<Op>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ lhsRegs, rhsRegs ] = allocations.uses;

    linkAllSlowCases(iter);

    ASSERT(m_replayAllocator.allocatedRegisters().isEmpty());
    loadGlobalObject(s_scratch);
    callOperation(operationCompareStrictEq, s_scratch, lhsRegs, rhsRegs);

    emitJumpSlowToHot(branchTest32(condition, returnValueGPR), target);

    m_replayAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jstricteq(const JSInstruction* currentInstruction)
{
    emitStrictEqJumpImpl<OpJstricteq>(currentInstruction, Equal);
}

void LOLJIT::emitSlow_op_jstricteq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitStrictEqJumpSlowImpl<OpJstricteq>(currentInstruction, NonZero, iter);
}

void LOLJIT::emit_op_jnstricteq(const JSInstruction* currentInstruction)
{
    emitStrictEqJumpImpl<OpJnstricteq>(currentInstruction, NotEqual);
}

void LOLJIT::emitSlow_op_jnstricteq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitStrictEqJumpSlowImpl<OpJnstricteq>(currentInstruction, Zero, iter);
}

template<typename Op>
void LOLJIT::emitCompareUnsignedAndJumpImpl(const JSInstruction* currentInstruction, RelationalCondition condition)
{
    auto bytecode = currentInstruction->as<Op>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ lhsRegs, rhsRegs ] = allocations.uses;

    if (isOperandConstantInt(bytecode.m_rhs)) {
        jitAssertIsJSInt32(lhsRegs.payloadGPR());
        addJump(branch32(condition, lhsRegs.payloadGPR(), Imm32(getOperandConstantInt(bytecode.m_rhs))), target);
    } else if (isOperandConstantInt(bytecode.m_lhs)) {
        jitAssertIsJSInt32(rhsRegs.payloadGPR());
        addJump(branch32(commute(condition), rhsRegs.payloadGPR(), Imm32(getOperandConstantInt(bytecode.m_lhs))), target);
    } else {
        jitAssertIsJSInt32(lhsRegs.payloadGPR());
        jitAssertIsJSInt32(rhsRegs.payloadGPR());
        addJump(branch32(condition, lhsRegs.payloadGPR(), rhsRegs.payloadGPR()), target);
    }

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jbelow(const JSInstruction* currentInstruction)
{
    emitCompareUnsignedAndJumpImpl<OpJbelow>(currentInstruction, Below);
}

void LOLJIT::emit_op_jbeloweq(const JSInstruction* currentInstruction)
{
    emitCompareUnsignedAndJumpImpl<OpJbeloweq>(currentInstruction, BelowOrEqual);
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

void LOLJIT::emit_op_create_lexical_environment(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCreateLexicalEnvironment>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs, symbolTableRegs ] = allocations.uses;

    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister initialValue = bytecode.m_initialValue;

    ASSERT(initialValue.isConstant());
    ASSERT(m_profiledCodeBlock->isConstantOwnedByUnlinkedCodeBlock(initialValue));
    JSValue value = m_unlinkedCodeBlock->getConstant(initialValue);

    using Operation = decltype(operationCreateLexicalEnvironmentUndefined);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();
    constexpr GPRReg scopeGPR = preferredArgumentGPR<Operation, 1>();
    constexpr GPRReg symbolTableGPR = preferredArgumentGPR<Operation, 2>();

    shuffleRegisters<GPRReg, 2>({ scopeRegs.payloadGPR(), symbolTableRegs.payloadGPR() }, { scopeGPR, symbolTableGPR });
    loadGlobalObject(globalObjectGPR);
    callOperationNoExceptionCheck(value == jsUndefined() ? operationCreateLexicalEnvironmentUndefined : operationCreateLexicalEnvironmentTDZ, dst, globalObjectGPR, scopeGPR, symbolTableGPR);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_create_direct_arguments(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCreateDirectArguments>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);

    VirtualRegister dst = bytecode.m_dst;

    using Operation = decltype(operationCreateDirectArgumentsBaseline);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();

    loadGlobalObject(globalObjectGPR);
    callOperationNoExceptionCheck(operationCreateDirectArgumentsBaseline, dst, globalObjectGPR);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_create_scoped_arguments(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCreateScopedArguments>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs ] = allocations.uses;

    VirtualRegister dst = bytecode.m_dst;

    using Operation = decltype(operationCreateScopedArgumentsBaseline);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();
    constexpr GPRReg scopeGPR = preferredArgumentGPR<Operation, 1>();

    move(scopeRegs.payloadGPR(), scopeGPR);
    loadGlobalObject(globalObjectGPR);
    callOperationNoExceptionCheck(operationCreateScopedArgumentsBaseline, dst, globalObjectGPR, scopeGPR);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_create_cloned_arguments(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCreateClonedArguments>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);

    VirtualRegister dst = bytecode.m_dst;

    using Operation = decltype(operationCreateClonedArgumentsBaseline);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();

    loadGlobalObject(globalObjectGPR);
    callOperation(operationCreateClonedArgumentsBaseline, dst, globalObjectGPR);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_new_array(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewArray>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);

    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister valuesStart = bytecode.m_argv;
    int size = bytecode.m_argc;

    using Operation = decltype(operationNewArrayWithProfile);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();
    constexpr GPRReg profileGPR = preferredArgumentGPR<Operation, 1>();
    constexpr GPRReg valuesGPR = preferredArgumentGPR<Operation, 2>();

    addPtr(TrustedImm32(valuesStart.offset() * sizeof(Register)), callFrameRegister, valuesGPR);
    materializePointerIntoMetadata(bytecode, OpNewArray::Metadata::offsetOfArrayAllocationProfile(), profileGPR);
    loadGlobalObject(globalObjectGPR);
    callOperation(operationNewArrayWithProfile, dst, globalObjectGPR, profileGPR, valuesGPR, size);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_new_array_with_size(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewArrayWithSize>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ sizeRegs ] = allocations.uses;

    VirtualRegister dst = bytecode.m_dst;

    using Operation = decltype(operationNewArrayWithSizeAndProfile);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();
    constexpr GPRReg profileGPR = preferredArgumentGPR<Operation, 1>();
    constexpr JSValueRegs sizeJSR = preferredArgumentJSR<Operation, 2>();

    materializePointerIntoMetadata(bytecode, OpNewArrayWithSize::Metadata::offsetOfArrayAllocationProfile(), profileGPR);
    moveValueRegs(sizeRegs, sizeJSR);
    loadGlobalObject(globalObjectGPR);
    callOperation(operationNewArrayWithSizeAndProfile, dst, globalObjectGPR, profileGPR, sizeJSR);

    m_fastAllocator.releaseScratches(allocations);
}

template<typename Op>
void LOLJIT::emitNewFuncCommon(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs ] = allocations.uses;

    VirtualRegister dst = bytecode.m_dst;
    auto* unlinkedExecutable = m_unlinkedCodeBlock->functionDecl(bytecode.m_functionDecl);

    using Operation = decltype(operationNewFunction);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();
    constexpr GPRReg scopeGPR = preferredArgumentGPR<Operation, 1>();
    constexpr GPRReg functionDeclGPR = preferredArgumentGPR<Operation, 2>();

    // Move allocated register first before it can be clobbered
    move(scopeRegs.payloadGPR(), scopeGPR);
    loadGlobalObject(globalObjectGPR);
    auto constant = addToConstantPool(JITConstantPool::Type::FunctionDecl, std::bit_cast<void*>(static_cast<uintptr_t>(bytecode.m_functionDecl)));
    loadConstant(constant, functionDeclGPR);

    OpcodeID opcodeID = Op::opcodeID;
    auto function = operationNewFunction;
    if (opcodeID == op_new_func)
        function = selectNewFunctionOperation(unlinkedExecutable);
    else if (opcodeID == op_new_generator_func)
        function = operationNewGeneratorFunction;
    else if (opcodeID == op_new_async_func)
        function = operationNewAsyncFunction;
    else {
        ASSERT(opcodeID == op_new_async_generator_func);
        function = operationNewAsyncGeneratorFunction;
    }
    callOperationNoExceptionCheck(function, dst, globalObjectGPR, scopeGPR, functionDeclGPR);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_new_func(const JSInstruction* currentInstruction)
{
    emitNewFuncCommon<OpNewFunc>(currentInstruction);
}

void LOLJIT::emit_op_new_generator_func(const JSInstruction* currentInstruction)
{
    emitNewFuncCommon<OpNewGeneratorFunc>(currentInstruction);
}

void LOLJIT::emit_op_new_async_func(const JSInstruction* currentInstruction)
{
    emitNewFuncCommon<OpNewAsyncFunc>(currentInstruction);
}

void LOLJIT::emit_op_new_async_generator_func(const JSInstruction* currentInstruction)
{
    emitNewFuncCommon<OpNewAsyncGeneratorFunc>(currentInstruction);
}

template<typename Op>
void LOLJIT::emitNewFuncExprCommon(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs ] = allocations.uses;

    VirtualRegister dst = bytecode.m_dst;
    auto* unlinkedExecutable = m_unlinkedCodeBlock->functionExpr(bytecode.m_functionDecl);

    using Operation = decltype(operationNewFunction);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();
    constexpr GPRReg scopeGPR = preferredArgumentGPR<Operation, 1>();
    constexpr GPRReg functionDeclGPR = preferredArgumentGPR<Operation, 2>();

    // Move allocated register first before it can be clobbered
    move(scopeRegs.payloadGPR(), scopeGPR);
    loadGlobalObject(globalObjectGPR);
    auto constant = addToConstantPool(JITConstantPool::Type::FunctionExpr, std::bit_cast<void*>(static_cast<uintptr_t>(bytecode.m_functionDecl)));
    loadConstant(constant, functionDeclGPR);

    OpcodeID opcodeID = Op::opcodeID;
    auto function = operationNewFunction;
    if (opcodeID == op_new_func_exp)
        function = selectNewFunctionOperation(unlinkedExecutable);
    else if (opcodeID == op_new_generator_func_exp)
        function = operationNewGeneratorFunction;
    else if (opcodeID == op_new_async_func_exp)
        function = operationNewAsyncFunction;
    else {
        ASSERT(opcodeID == op_new_async_generator_func_exp);
        function = operationNewAsyncGeneratorFunction;
    }
    callOperationNoExceptionCheck(function, dst, globalObjectGPR, scopeGPR, functionDeclGPR);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_new_func_exp(const JSInstruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewFuncExp>(currentInstruction);
}

void LOLJIT::emit_op_new_generator_func_exp(const JSInstruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewGeneratorFuncExp>(currentInstruction);
}

void LOLJIT::emit_op_new_async_func_exp(const JSInstruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewAsyncFuncExp>(currentInstruction);
}

void LOLJIT::emit_op_new_async_generator_func_exp(const JSInstruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewAsyncGeneratorFuncExp>(currentInstruction);
}

void LOLJIT::emit_op_new_object(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewObject>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);

    RegisterID resultReg = regT0;
    RegisterID allocatorReg = regT1;
    RegisterID scratchReg = regT2;
    RegisterID structureReg = regT3;

    loadPtrFromMetadata(bytecode, OpNewObject::Metadata::offsetOfObjectAllocationProfile() + ObjectAllocationProfile::offsetOfAllocator(), allocatorReg);
    loadPtrFromMetadata(bytecode, OpNewObject::Metadata::offsetOfObjectAllocationProfile() + ObjectAllocationProfile::offsetOfStructure(), structureReg);

    JumpList slowCases;
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject(resultReg, JITAllocator::variable(), allocatorReg, structureReg, butterfly, scratchReg, slowCases, SlowAllocationResult::UndefinedBehavior);
    load8(Address(structureReg, Structure::inlineCapacityOffset()), scratchReg);
    emitInitializeInlineStorage(resultReg, scratchReg);
    mutatorFence(*m_vm);
    boxCell(resultReg, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);

    addSlowCase(slowCases);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emitSlow_op_new_object(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    RegisterID structureReg = regT3;

    auto bytecode = currentInstruction->as<OpNewObject>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);

    VirtualRegister dst = bytecode.m_dst;
    callOperationNoExceptionCheck(operationNewObject, TrustedImmPtr(&vm()), structureReg);
    boxCell(returnValueGPR, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);

    m_replayAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_new_reg_exp(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewRegExp>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);

    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister regexp = bytecode.m_regexp;

    using Operation = decltype(operationNewRegExp);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();

    loadGlobalObject(globalObjectGPR);
    callOperation(operationNewRegExp, globalObjectGPR, TrustedImmPtr(jsCast<RegExp*>(m_unlinkedCodeBlock->getConstant(regexp))));
    boxCell(returnValueGPR, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_is_empty(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsEmpty>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    isEmpty(operandRegs.gpr(), dstRegs.gpr());
    boxBoolean(dstRegs.gpr(), dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_typeof_is_undefined(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTypeofIsUndefined>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    Jump isCell = branchIfCell(operandRegs);

    isUndefined(operandRegs, s_scratch);
    Jump done = jump();

    isCell.link(this);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(operandRegs.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), s_scratch);
    Jump notMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    emitLoadStructure(vm(), operandRegs.payloadGPR(), s_scratch);
    // We don't need operandRegs anymore so it's ok to use dstRegs even if it is operandRegs.
    loadGlobalObject(dstRegs.gpr());
    loadPtr(Address(s_scratch, Structure::globalObjectOffset()), s_scratch);
    comparePtr(Equal, dstRegs.gpr(), s_scratch, s_scratch);

    notMasqueradesAsUndefined.link(this);
    done.link(this);
    boxBoolean(s_scratch, dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_typeof_is_function(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTypeofIsFunction>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    auto isNotCell = branchIfNotCell(operandRegs);
    addSlowCase(branchIfObject(operandRegs.payloadGPR()));
    isNotCell.link(this);
    moveTrustedValue(jsBoolean(false), dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_is_undefined_or_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsUndefinedOrNull>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    moveValueRegs(operandRegs, dstRegs);
    emitTurnUndefinedIntoNull(dstRegs);
    isNull(dstRegs, dstRegs.gpr());

    boxBoolean(dstRegs.gpr(), dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_is_boolean(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsBoolean>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

#if USE(JSVALUE64)
    move(operandRegs.gpr(), dstRegs.gpr());
    xor64(TrustedImm32(JSValue::ValueFalse), dstRegs.gpr());
    test64(Zero, dstRegs.gpr(), TrustedImm32(static_cast<int32_t>(~1)), dstRegs.gpr());
#elif USE(JSVALUE32_64)
    compare32(Equal, operandRegs.tagGPR(), TrustedImm32(JSValue::BooleanTag), dstRegs.gpr());
#endif

    boxBoolean(dstRegs.gpr(), dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_is_number(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsNumber>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

#if USE(JSVALUE64)
    test64(NonZero, operandRegs.gpr(), numberTagRegister, dstRegs.gpr());
#elif USE(JSVALUE32_64)
    move(operandRegs.tagGPR(), dstRegs.gpr());
    add32(TrustedImm32(1), dstRegs.gpr());
    compare32(Below, dstRegs.gpr(), TrustedImm32(JSValue::LowestTag + 1), dstRegs.gpr());
#endif

    boxBoolean(dstRegs.gpr(), dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

#if USE(BIGINT32)
void LOLJIT::emit_op_is_big_int(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsBigInt>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    Jump isCell = branchIfCell(operandRegs.gpr());

    move(TrustedImm64(JSValue::BigInt32Mask), s_scratch);
    and64(operandRegs.gpr(), s_scratch);
    compare64(Equal, s_scratch, TrustedImm32(JSValue::BigInt32Tag), dstRegs.gpr());
    boxBoolean(dstRegs.gpr(), dstRegs);
    Jump done = jump();

    isCell.link(this);
    compare8(Equal, Address(operandRegs.payloadGPR(), JSCell::typeInfoTypeOffset()), TrustedImm32(HeapBigIntType), dstRegs.gpr());
    boxBoolean(dstRegs.gpr(), dstRegs);

    done.link(this);

    m_fastAllocator.releaseScratches(allocations);
}
#else // if !USE(BIGINT32)
void LOLJIT::emit_op_is_big_int(const JSInstruction*)
{
    // If we only have HeapBigInts, then we emit isCellWithType instead of isBigInt.
    UNREACHABLE_FOR_PLATFORM();
}
#endif

void LOLJIT::emit_op_is_object(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsObject>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;

    move(TrustedImm32(0), s_scratch);
    Jump isNotCell = branchIfNotCell(operandRegs);
    compare8(AboveOrEqual, Address(operandRegs.payloadGPR(), JSCell::typeInfoTypeOffset()), TrustedImm32(ObjectType), s_scratch);
    isNotCell.link(this);

    boxBoolean(s_scratch, dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_is_cell_with_type(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsCellWithType>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;
    int type = bytecode.m_type;

    move(TrustedImm32(0), s_scratch);
    Jump isNotCell = branchIfNotCell(operandRegs);
    compare8(Equal, Address(operandRegs.payloadGPR(), JSCell::typeInfoTypeOffset()), TrustedImm32(type), s_scratch);
    isNotCell.link(this);

    boxBoolean(s_scratch, dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_has_structure_with_flags(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasStructureWithFlags>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ operandRegs ] = allocations.uses;
    auto [ dstRegs ] = allocations.defs;
    unsigned flags = bytecode.m_flags;

    emitLoadStructure(vm(), operandRegs.payloadGPR(), s_scratch);
    test32(NonZero, Address(s_scratch, Structure::bitFieldOffset()), TrustedImm32(flags), dstRegs.gpr());
    boxBoolean(dstRegs.gpr(), dstRegs);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jeq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ lhsRegs, rhsRegs ] = allocations.uses;

    addSlowCase(branchIfNotInt32(lhsRegs));
    addSlowCase(branchIfNotInt32(rhsRegs));

    addJump(branch32(Equal, lhsRegs.payloadGPR(), rhsRegs.payloadGPR()), target);

    m_fastAllocator.releaseScratches(allocations);
}


void LOLJIT::emitSlow_op_jeq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ lhsRegs, rhsRegs ] = allocations.uses;

    linkAllSlowCases(iter);

    // We don't need to spill here since the allocator flushed all registers already
    ASSERT(m_replayAllocator.allocatedRegisters().isEmpty());
    loadGlobalObject(s_scratch);
    callOperation(operationCompareEq, s_scratch, lhsRegs, rhsRegs);

    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);

    m_replayAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jneq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ lhsRegs, rhsRegs ] = allocations.uses;

    addSlowCase(branchIfNotInt32(lhsRegs));
    addSlowCase(branchIfNotInt32(rhsRegs));

    addJump(branch32(NotEqual, lhsRegs.payloadGPR(), rhsRegs.payloadGPR()), target);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emitSlow_op_jneq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ lhsRegs, rhsRegs ] = allocations.uses;

    linkAllSlowCases(iter);

    // We don't need to spill here since the allocator flushed all registers already
    ASSERT(m_replayAllocator.allocatedRegisters().isEmpty());
    loadGlobalObject(s_scratch);
    callOperation(operationCompareEq, s_scratch, lhsRegs, rhsRegs);

    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);

    m_replayAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jmp(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJmp>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    addJump(jump(), target);
}

void LOLJIT::emit_op_jtrue(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJtrue>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ valueRegs ] = allocations.uses;

    JumpList fallThrough;
#if USE(JSVALUE64)
    // Quick fast path.
    auto isNotBoolean = branchIfNotBoolean(valueRegs, s_scratch);
    addJump(branchTest64(NonZero, valueRegs.payloadGPR(), TrustedImm32(0x1)), target);
    fallThrough.append(jump());

    isNotBoolean.link(this);
    auto isNotInt32 = branchIfNotInt32(valueRegs);
    addJump(branchTest32(NonZero, valueRegs.payloadGPR()), target);
    fallThrough.append(jump());

    isNotInt32.link(this);
    fallThrough.append(branchIfOther(valueRegs, s_scratch));
#endif

    moveValueRegs(valueRegs, BaselineJITRegisters::JTrue::valueJSR);
    nearCallThunk(CodeLocationLabel { vm().getCTIStub(valueIsTruthyGenerator).retaggedCode<NoPtrTag>() });
    addJump(branchTest32(NonZero, regT0), target);
    fallThrough.link(this);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jfalse(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJfalse>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ valueRegs ] = allocations.uses;

    JumpList fallThrough;
#if USE(JSVALUE64)
    // Quick fast path.
    auto isNotBoolean = branchIfNotBoolean(valueRegs, s_scratch);
    addJump(branchTest64(Zero, valueRegs.payloadGPR(), TrustedImm32(0x1)), target);
    fallThrough.append(jump());

    isNotBoolean.link(this);
    auto isNotInt32 = branchIfNotInt32(valueRegs);
    addJump(branchTest32(Zero, valueRegs.payloadGPR()), target);
    fallThrough.append(jump());

    isNotInt32.link(this);
    addJump(branchIfOther(valueRegs, s_scratch), target);
#endif

    moveValueRegs(valueRegs, BaselineJITRegisters::JFalse::valueJSR);
    nearCallThunk(CodeLocationLabel { vm().getCTIStub(valueIsFalseyGenerator).retaggedCode<NoPtrTag>() });
    addJump(branchTest32(NonZero, regT0), target);
    fallThrough.link(this);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jeq_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeqNull>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ valueRegs ] = allocations.uses;

    Jump isImmediate = branchIfNotCell(valueRegs);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    Jump isNotMasqueradesAsUndefined = branchTest8(Zero, Address(valueRegs.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    emitLoadStructure(vm(), valueRegs.payloadGPR(), s_scratch);
    loadGlobalObject(regT0);
    addJump(branchPtr(Equal, Address(s_scratch, Structure::globalObjectOffset()), regT0), target);
    Jump masqueradesGlobalObjectIsForeign = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    emitTurnUndefinedIntoNull(valueRegs);
    addJump(branchIfNull(valueRegs), target);

    isNotMasqueradesAsUndefined.link(this);
    masqueradesGlobalObjectIsForeign.link(this);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jneq_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqNull>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ valueRegs ] = allocations.uses;

    Jump isImmediate = branchIfNotCell(valueRegs);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    addJump(branchTest8(Zero, Address(valueRegs.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);
    emitLoadStructure(vm(), valueRegs.payloadGPR(), s_scratch);
    loadGlobalObject(regT0);
    addJump(branchPtr(NotEqual, Address(s_scratch, Structure::globalObjectOffset()), regT0), target);
    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    emitTurnUndefinedIntoNull(valueRegs);
    addJump(branchIfNotNull(valueRegs), target);

    wasNotImmediate.link(this);

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jundefined_or_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJundefinedOrNull>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ valueRegs ] = allocations.uses;

#if USE(JSVALUE64)
    moveValueRegs(valueRegs, s_scratchRegs);
    emitTurnUndefinedIntoNull(s_scratchRegs);
    addJump(branchIfNull(s_scratchRegs), target);
#endif

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jnundefined_or_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJnundefinedOrNull>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ valueRegs ] = allocations.uses;

#if USE(JSVALUE64)
    moveValueRegs(valueRegs, s_scratchRegs);
    emitTurnUndefinedIntoNull(s_scratchRegs);
    addJump(branchIfNotNull(s_scratchRegs), target);
#endif

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jeq_ptr(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeqPtr>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ valueRegs ] = allocations.uses;

#if USE(JSVALUE64)
    loadCodeBlockConstantPayload(bytecode.m_specialPointer, s_scratch);
    addJump(branchPtr(Equal, valueRegs.payloadGPR(), s_scratch), target);
#endif

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_jneq_ptr(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqPtr>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ valueRegs ] = allocations.uses;

#if USE(JSVALUE64)
    loadCodeBlockConstantPayload(bytecode.m_specialPointer, s_scratch);
    CCallHelpers::Jump equal = branchPtr(Equal, valueRegs.payloadGPR(), s_scratch);
#endif
    store8ToMetadata(TrustedImm32(1), bytecode, OpJneqPtr::Metadata::offsetOfHasJumped());
    addJump(jump(), target);
#if USE(JSVALUE64)
    equal.link(this);
#endif

    m_fastAllocator.releaseScratches(allocations);
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
        silentSpill(m_fastAllocator, allocations);
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

    silentSpill(m_replayAllocator, allocations);

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
        silentSpill(m_fastAllocator, allocations);
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

    silentSpill(m_replayAllocator, allocations);

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
        // TODO: We should check if we're going to fall into the default case and do the right thing there.
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
            loadValue(Address(s_scratch), s_scratchRegs);
            addSlowCase(branchIfEmpty(s_scratchRegs));
            moveValueRegs(s_scratchRegs, destRegs);
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

            // TODO: This only needs to save the BaselineJITRegisters::GetFromScope registers.
            silentSpill(m_fastAllocator, allocations);

            move(scopeRegs.payloadGPR(), thunkScopeGPR);
            if (metadataAddress.base != thunkMetadataGPR) {
                // Materialize metadataGPR for the thunks if we didn't already.
                uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);
                addPtr(TrustedImm32(metadataOffset), GPRInfo::metadataTableRegister, thunkMetadataGPR);
            }
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


    silentSpill(m_replayAllocator, allocations);

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

template <ResolveType profiledResolveType>
MacroAssemblerCodeRef<JITThunkPtrTag> LOLJIT::generateOpGetFromScopeThunk(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    using Metadata = OpGetFromScope::Metadata;

    using BaselineJITRegisters::GetFromScope::metadataGPR; // Incoming
    using BaselineJITRegisters::GetFromScope::scopeGPR; // Incoming
    using BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR; // Incoming - pass through to slow path.
    using BaselineJITRegisters::GetFromScope::scratch1GPR;
    UNUSED_PARAM(bytecodeOffsetGPR);

    CCallHelpers jit;

    jit.tagReturnAddress();

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks) {
        if (!needsVarInjectionChecks)
            return;
        loadGlobalObject(jit, scratch1GPR);
        jit.loadPtr(Address(scratch1GPR, JSGlobalObject::offsetOfVarInjectionWatchpoint()), scratch1GPR);
        slowCase.append(jit.branch8(Equal, Address(scratch1GPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // Structure check covers var injection since we don't cache structures for anything but the GlobalObject. Additionally, resolve_scope handles checking for the var injection.
            jit.load32(Address(metadataGPR, OpGetFromScope::Metadata::offsetOfStructureID()), scratch1GPR);
            slowCase.append(jit.branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), scratch1GPR));

            jit.jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                loadGlobalObject(jit, scratch1GPR);
                return jit.branchPtr(Equal, scopeGPR, scratch1GPR);
            }));

            jit.loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratch1GPR);

            if (ASSERT_ENABLED) {
                Jump isOutOfLine = jit.branch32(GreaterThanOrEqual, scratch1GPR, TrustedImm32(firstOutOfLineOffset));
                jit.abortWithReason(JITOffsetIsNotOutOfLine);
                isOutOfLine.link(&jit);
            }

            jit.loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), scopeGPR);
            jit.negPtr(scratch1GPR);
            jit.loadValue(BaseIndex(scopeGPR, scratch1GPR, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), returnValueJSR);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            jit.loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratch1GPR);
            jit.loadValue(Address(scratch1GPR), returnValueJSR);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks) // TDZ check.
                slowCase.append(jit.branchIfEmpty(returnValueJSR));
            break;
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            jit.loadPtr(Address(metadataGPR,  Metadata::offsetOfOperand()), scratch1GPR);
            jit.loadValue(BaseIndex(scopeGPR, scratch1GPR, TimesEight, JSLexicalEnvironment::offsetOfVariables()), returnValueJSR);
            break;
        case Dynamic:
            slowCase.append(jit.jump());
            break;
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    if (profiledResolveType == ClosureVar || profiledResolveType == ClosureVarWithVarInjectionChecks)
        emitCode(profiledResolveType);
    else {
        JumpList skipToEnd;
        jit.load32(Address(metadataGPR, Metadata::offsetOfGetPutInfo()), scratch1GPR);
        jit.and32(TrustedImm32(GetPutInfo::typeBits), scratch1GPR); // Load ResolveType into scratch1GPR

        auto emitCaseWithoutCheck = [&] (ResolveType resolveType) {
            Jump notCase = jit.branch32(NotEqual, scratch1GPR, TrustedImm32(resolveType));
            emitCode(resolveType);
            skipToEnd.append(jit.jump());
            notCase.link(&jit);
        };

        auto emitCase = [&] (ResolveType resolveType) {
            if (profiledResolveType != resolveType)
                emitCaseWithoutCheck(resolveType);
        };

        switch (profiledResolveType) {
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            break;
        default:
            emitCaseWithoutCheck(profiledResolveType);
            break;
        }

        emitCase(GlobalVar);
        emitCase(GlobalProperty);
        emitCase(GlobalLexicalVar);
        emitCase(GlobalVarWithVarInjectionChecks);
        emitCase(GlobalPropertyWithVarInjectionChecks);
        emitCase(GlobalLexicalVarWithVarInjectionChecks);

        slowCase.append(jit.jump());
        skipToEnd.link(&jit);
    }

    jit.ret();

    slowCase.linkThunk(CodeLocationLabel { vm.getCTIStub(slow_op_get_from_scopeGenerator).retaggedCode<NoPtrTag>() }, &jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "get_from_scope"_s, "Baseline: get_from_scope");
}

MacroAssemblerCodeRef<JITThunkPtrTag> LOLJIT::slow_op_get_from_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using BaselineJITRegisters::GetFromScope::scopeGPR; // Incoming
    using BaselineJITRegisters::GetFromScope::metadataGPR; // Incoming
    using BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR; // Incoming
    constexpr GPRReg globalObjectGPR = argumentGPR0;
    static_assert(noOverlap(metadataGPR, bytecodeOffsetGPR, globalObjectGPR, scopeGPR));
    static_assert(noOverlap(metadataGPR, returnValueGPR));

    jit.emitCTIThunkPrologue(/* returnAddressAlreadyTagged: */ true); // Return address tagged in 'generateOpGetFromScopeThunk'

    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);

    // save metadataGPR (arguments to call below are in registers on all platforms, so ok to stack this).
    // Note: we will do a call, so can't use pushToSave, as it does not maintain ABI stack alignment.
    jit.subPtr(TrustedImmPtr(16), stackPointerRegister);
    jit.storePtr(metadataGPR, Address(stackPointerRegister));

    jit.setupArguments<decltype(operationGetFromScopeForLOL)>(bytecodeOffsetGPR, scopeGPR);
    jit.callOperation<OperationPtrTag>(operationGetFromScopeForLOL);
    Jump exceptionCheck = jit.emitNonPatchableExceptionCheck(vm);

    jit.loadPtr(Address(stackPointerRegister), metadataGPR); // Restore metadataGPR
    jit.addPtr(TrustedImmPtr(16), stackPointerRegister); // Restore stack pointer

    jit.emitCTIThunkEpilogue();
    jit.ret();

    exceptionCheck.link(&jit);
    jit.addPtr(TrustedImmPtr(16), stackPointerRegister); // Restore stack pointer

    jit.jumpThunk(CodeLocationLabel { vm.getCTIStub(popThunkStackPreservesAndHandleExceptionGenerator).retaggedCode<NoPtrTag>() });

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "slow_op_get_from_scope"_s, "Baseline: slow_op_get_from_scope");
}

void LOLJIT::emit_op_put_to_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToScope>();
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs, valueRegs ] = allocations.uses;
    auto [ metadataRegs ] = allocations.scratches;

    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();

    GPRReg scopeGPR = scopeRegs.payloadGPR();
    GPRReg metadataGPR = metadataRegs.payloadGPR();
    using Metadata = OpPutToScope::Metadata;

    constexpr size_t metadataPointerAlignment = alignof(void*);
    static_assert(!(Metadata::offsetOfGetPutInfo() % metadataPointerAlignment));
    static_assert(!(Metadata::offsetOfStructureID() % metadataPointerAlignment));
    static_assert(!(Metadata::offsetOfOperand() % metadataPointerAlignment));
    static_assert(!(Metadata::offsetOfWatchpointSet() % metadataPointerAlignment));
    auto metadataAddress = computeBaseAddressForMetadata<metadataPointerAlignment>(bytecode, metadataGPR);
    auto getPutInfoAddress = metadataAddress.withOffset(Metadata::offsetOfGetPutInfo());
    auto structureIDAddress = metadataAddress.withOffset(Metadata::offsetOfStructureID());
    auto operandAddress = metadataAddress.withOffset(Metadata::offsetOfOperand());
    auto watchpointSetAddress = metadataAddress.withOffset(Metadata::offsetOfWatchpointSet());

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // Structure check covers var injection since we don't cache structures for anything but the GlobalObject.
            // Additionally, resolve_scope handles checking for the var injection.
            load32(structureIDAddress, s_scratch);
            addSlowCase(branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), s_scratch));

            jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                loadGlobalObject(s_scratch);
                return branchPtr(Equal, scopeGPR, s_scratch);
            }));

            loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), s_scratch);
            loadPtr(operandAddress, metadataGPR);
            negPtr(metadataGPR);
            storeValue(valueRegs, BaseIndex(s_scratch, metadataGPR, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)));
            emitWriteBarrier(m_fastAllocator, allocations, scopeRegs, valueRegs, s_scratch, ShouldFilterValue);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), s_scratch);
            emitVarReadOnlyCheck(resolveType, s_scratch);

            loadPtr(operandAddress, s_scratch);

            // It would be a bit nicer to do this after the TDZ check below but that would mean the live range of metadataGPR requires a additional scratch.
            // That said, it shouldn't practically make a difference since we won't be watchpointing an empty value.
            loadPtr(watchpointSetAddress, metadataGPR);
            emitNotifyWriteWatchpoint(metadataGPR);

            if (!isInitialization(bytecode.m_getPutInfo.initializationMode()) && (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)) {
                // We need to do a TDZ check here because we can't always prove we need to emit TDZ checks statically.
                loadValue(Address(s_scratch), metadataRegs);
                addSlowCase(branchIfEmpty(metadataRegs));
            }

            storeValue(valueRegs, Address(s_scratch));

            emitWriteBarrier(m_fastAllocator, allocations, scopeRegs, valueRegs, s_scratch, ShouldFilterValue);
            break;
        }
        case ResolvedClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), s_scratch);

            loadPtr(watchpointSetAddress, s_scratch);
            loadPtr(operandAddress, metadataGPR);
            emitNotifyWriteWatchpoint(s_scratch);
            storeValue(valueRegs, BaseIndex(scopeRegs.payloadGPR(), metadataGPR, TimesEight, JSLexicalEnvironment::offsetOfVariables()));

            emitWriteBarrier(m_fastAllocator, allocations, scopeRegs, valueRegs, s_scratch, ShouldFilterValue);
            break;
        case ModuleVar:
        case Dynamic:
            addSlowCase(jump());
            break;
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    };

    // If any linked CodeBlock sees ClosureVar/ ClosureVarWithVarInjectionChecks, then we can compile things
    // that way for all CodeBlocks, since we've proven that is the type we will be. If we're a ClosureVar,
    // all CodeBlocks will be ClosureVar. If we're ClosureVarWithVarInjectionChecks, we're always ClosureVar
    // if the var injection watchpoint isn't fired. If it is fired, then we take the slow path, so it doesn't
    // matter what type we are dynamically.
    if (profiledResolveType == ClosureVar)
        emitCode(ClosureVar);
    else if (profiledResolveType == ResolvedClosureVar)
        emitCode(ResolvedClosureVar);
    else if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        emitCode(ClosureVarWithVarInjectionChecks);
    else {
        JumpList skipToEnd;
        load32(getPutInfoAddress, s_scratch);
        and32(TrustedImm32(GetPutInfo::typeBits), s_scratch); // Load ResolveType into scratch

        auto emitCaseWithoutCheck = [&] (ResolveType resolveType) {
            Jump notCase = branch32(NotEqual, s_scratch, TrustedImm32(resolveType));
            emitCode(resolveType);
            skipToEnd.append(jump());
            notCase.link(this);
        };

        auto emitCase = [&] (ResolveType resolveType) {
            if (profiledResolveType != resolveType)
                emitCaseWithoutCheck(resolveType);
        };

        switch (profiledResolveType) {
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            break;
        default:
            emitCaseWithoutCheck(profiledResolveType);
            break;
        }

        emitCase(GlobalVar);
        emitCase(GlobalProperty);
        emitCase(GlobalLexicalVar);
        emitCase(GlobalVarWithVarInjectionChecks);
        emitCase(GlobalPropertyWithVarInjectionChecks);
        emitCase(GlobalLexicalVarWithVarInjectionChecks);

        addSlowCase(jump());
        skipToEnd.link(this);
    }

    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emitSlow_op_put_to_scope(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutToScope>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs, valueRegs ] = allocations.uses;

    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();
    silentSpill(m_replayAllocator, allocations);
    if (profiledResolveType == ModuleVar) {
        // If any linked CodeBlock saw a ModuleVar, then all linked CodeBlocks are guaranteed
        // to also see ModuleVar.
        JITSlowPathCall slowPathCall(this, slow_path_throw_strict_mode_readonly_property_write_error);
        slowPathCall.call();
    } else {
        uint32_t bytecodeOffset = m_bytecodeIndex.offset();
        ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
        ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

        callOperation(operationPutToScopeForLOL, TrustedImm32(bytecodeOffset), scopeRegs.payloadGPR(), valueRegs.payloadGPR());
    }
    silentFill(m_replayAllocator);
    m_replayAllocator.releaseScratches(allocations);
}

void LOLJIT::emit_op_resolve_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpResolveScope>();
    // TODO: This should only allocate scopeRegs when profiledResolveType == ClosureVar as that's the only case that uses it and its static otherwise.
    // Perhaps we should have a ResolveClosureScope instruction instead as that would use less operands for every other case.
    auto allocations = m_fastAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;
    auto [ metadataRegs ] = allocations.scratches;

    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_resolveType;
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

    using Metadata = OpResolveScope::Metadata;
    GPRReg metadataGPR = metadataRegs.payloadGPR();

    // If we profile certain resolve types, we're guaranteed all linked code will have the same
    // resolve type.

    if (profiledResolveType == ModuleVar)
        loadPtrFromMetadata(bytecode, Metadata::offsetOfLexicalEnvironment(), destRegs.payloadGPR());
    else if (profiledResolveType == ClosureVar) {
        move(scopeRegs.payloadGPR(), destRegs.payloadGPR());
        unsigned localScopeDepth = bytecode.metadata(m_profiledCodeBlock).m_localScopeDepth;
        if (localScopeDepth < 8) {
            for (unsigned index = 0; index < localScopeDepth; ++index)
                loadPtr(Address(destRegs.payloadGPR(), JSScope::offsetOfNext()), destRegs.payloadGPR());
        } else {
            ASSERT(localScopeDepth >= 8);
            load32FromMetadata(bytecode, Metadata::offsetOfLocalScopeDepth(), s_scratch);
            auto loop = label();
            loadPtr(Address(destRegs.payloadGPR(), JSScope::offsetOfNext()), destRegs.payloadGPR());
            branchSub32(NonZero, s_scratch, TrustedImm32(1), s_scratch).linkTo(loop, this);
        }
    } else {
        // Inlined fast path for common types.
        constexpr size_t metadataMinAlignment = 4;
        static_assert(!(Metadata::offsetOfResolveType() % metadataMinAlignment));
        static_assert(!(Metadata::offsetOfGlobalLexicalBindingEpoch() % metadataMinAlignment));
        // TODO: We should check if we're going to fall into the default case and do the right thing there.
        auto metadataAddress = computeBaseAddressForMetadata<4>(bytecode, metadataGPR);

        auto resolveTypeAddress = metadataAddress.withOffset(Metadata::offsetOfResolveType());
        auto globalLexicalBindingEpochAddress = metadataAddress.withOffset(Metadata::offsetOfGlobalLexicalBindingEpoch());

        // FIXME: This code is weird when caching fails because it goes to a slow path that will check the exact same condition before falling into the C++ slow path.
        // It's unclear if that makes a meaningful difference for perf but we should consider doing something smarter.
        switch (profiledResolveType) {
        case GlobalProperty: {
            // This saves a move when scopeRegs != destRegs.
            // FIXME: This is probably not correct for 32-bit.
            GPRReg globalObjectGPR = scopeRegs == destRegs ? s_scratch : destRegs.payloadGPR();
            addSlowCase(branch32(NotEqual, resolveTypeAddress, TrustedImm32(profiledResolveType)));
            loadGlobalObject(globalObjectGPR);
            load32(globalLexicalBindingEpochAddress, metadataGPR);
            addSlowCase(branch32(NotEqual, Address(globalObjectGPR, JSGlobalObject::offsetOfGlobalLexicalBindingEpoch()), metadataGPR));
            move(globalObjectGPR, destRegs.payloadGPR());
            break;
        }
        case GlobalVar: {
            addSlowCase(branch32(NotEqual, resolveTypeAddress, TrustedImm32(profiledResolveType)));
            loadGlobalObject(destRegs.payloadGPR());
            break;
        }
        case GlobalLexicalVar: {
            addSlowCase(branch32(NotEqual, resolveTypeAddress, TrustedImm32(profiledResolveType)));
            loadGlobalObject(destRegs.payloadGPR());
            loadPtr(Address(destRegs.payloadGPR(), JSGlobalObject::offsetOfGlobalLexicalEnvironment()), destRegs.payloadGPR());
            break;
        }
        default: {
            MacroAssemblerCodeRef<JITThunkPtrTag> code;
            if (profiledResolveType == ClosureVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpResolveScopeThunk<ClosureVarWithVarInjectionChecks>);
            else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVarWithVarInjectionChecks>);
            else if (profiledResolveType == GlobalPropertyWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalPropertyWithVarInjectionChecks>);
            else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
            else
                code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVar>);


            // TODO: We should teach RegisterAllocator to always pick these registers when not one of the constant resolve types (e.g. ModuleVar).
            silentSpill(m_fastAllocator, allocations);

            if (metadataAddress.base != metadataGPR) {
                // Materialize metadataGPR for the thunks if we didn't already.
                // First move scope in case it conflicts with ResolveScope::metadataGPR
                move(scopeRegs.payloadGPR(), BaselineJITRegisters::ResolveScope::scopeGPR);
                uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);
                addPtr(TrustedImm32(metadataOffset), GPRInfo::metadataTableRegister, BaselineJITRegisters::ResolveScope::metadataGPR);
            } else
                shuffleRegisters<GPRReg, 2>({ scopeRegs.payloadGPR(), metadataGPR }, { BaselineJITRegisters::ResolveScope::scopeGPR, BaselineJITRegisters::ResolveScope::metadataGPR });

            move(TrustedImm32(bytecodeOffset), BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR);
            nearCallThunk(CodeLocationLabel { code.retaggedCode<NoPtrTag>() });
            move(returnValueGPR, destRegs.payloadGPR());
            silentFill(m_fastAllocator, destRegs.payloadGPR());
            break;
        }
        }
    }

    setFastPathResumePoint();
    boxCell(destRegs.payloadGPR(), destRegs);
    m_fastAllocator.releaseScratches(allocations);
}

void LOLJIT::emitSlow_op_resolve_scope(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{

    auto bytecode = currentInstruction->as<OpResolveScope>();
    auto allocations = m_replayAllocator.allocate(*this, bytecode, m_bytecodeIndex);
    auto [ scopeRegs ] = allocations.uses;
    auto [ destRegs ] = allocations.defs;

    if (!hasAnySlowCases(iter)) {
        m_replayAllocator.releaseScratches(allocations);
        return;
    }

    linkAllSlowCases(iter);

    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_resolveType;
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();

    MacroAssemblerCodeRef<JITThunkPtrTag> code;
    if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpResolveScopeThunk<ClosureVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalVar)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVar>);
    else if (profiledResolveType == GlobalProperty)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalProperty>);
    else if (profiledResolveType == GlobalLexicalVar)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalLexicalVar>);
    else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalPropertyWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalPropertyWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
    else
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVar>);


    silentSpill(m_replayAllocator, allocations);

    move(scopeRegs.payloadGPR(), BaselineJITRegisters::ResolveScope::scopeGPR);

    constexpr size_t metadataMinAlignment = 4;
    Address metadataAddress = computeBaseAddressForMetadata<metadataMinAlignment>(bytecode, BaselineJITRegisters::ResolveScope::metadataGPR);
    if (metadataAddress.base != BaselineJITRegisters::ResolveScope::metadataGPR)
        addPtr(TrustedImm32(m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode)), GPRInfo::metadataTableRegister, BaselineJITRegisters::ResolveScope::metadataGPR);

    move(TrustedImm32(bytecodeOffset), BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR);
    nearCallThunk(CodeLocationLabel { code.retaggedCode<NoPtrTag>() });
    move(returnValueGPR, destRegs.payloadGPR());
    silentFill(m_replayAllocator, destRegs.payloadGPR());
    m_replayAllocator.releaseScratches(allocations);
}

template <ResolveType profiledResolveType>
MacroAssemblerCodeRef<JITThunkPtrTag> LOLJIT::generateOpResolveScopeThunk(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().

    CCallHelpers jit;

    using Metadata = OpResolveScope::Metadata;
    using BaselineJITRegisters::ResolveScope::metadataGPR; // Incoming
    // TODO: This should probably not be the same as the returnValueGPR for just the emitResolveClosure case.
    using BaselineJITRegisters::ResolveScope::scopeGPR; // Incoming
    using BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR; // Incoming - pass through to slow path.
    using BaselineJITRegisters::ResolveScope::scratch1GPR;
    using BaselineJITRegisters::ResolveScope::scratch2GPR;
    UNUSED_PARAM(bytecodeOffsetGPR);
    // NOTE: This means we can't write to returnValueGPR until AFTER the last slowCase branch. Otherwise we could clobber the scope for native.
    static_assert(scopeGPR == returnValueGPR); // emitResolveClosure assumes this.

    jit.tagReturnAddress();

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks, GPRReg globalObjectGPR = InvalidGPRReg) {
        if (!needsVarInjectionChecks)
            return;
        if (globalObjectGPR == InvalidGPRReg) {
            globalObjectGPR = scratch1GPR;
            loadGlobalObject(jit, globalObjectGPR);
        }
        jit.loadPtr(Address(globalObjectGPR, JSGlobalObject::offsetOfVarInjectionWatchpoint()), scratch1GPR);
        slowCase.append(jit.branch8(Equal, Address(scratch1GPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };

    auto emitResolveClosure = [&] (bool needsVarInjectionChecks) {
        doVarInjectionCheck(needsVarInjectionChecks);
        jit.load32(Address(metadataGPR, Metadata::offsetOfLocalScopeDepth()), scratch1GPR);
        RELEASE_ASSERT(scopeGPR == returnValueGPR);

        Label loop = jit.label();
        Jump done = jit.branchTest32(Zero, scratch1GPR);
        jit.loadPtr(Address(returnValueGPR, JSScope::offsetOfNext()), returnValueGPR);
        jit.sub32(TrustedImm32(1), scratch1GPR);
        jit.jump().linkTo(loop, &jit);
        done.link(&jit);
    };

    auto emitCode = [&] (ResolveType resolveType) {
        JIT_COMMENT(jit, "Starting case for ", resolveType);
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // JSScope::constantScopeForCodeBlock() loads codeBlock->globalObject().
            loadGlobalObject(jit, scratch2GPR);
            doVarInjectionCheck(needsVarInjectionChecks(resolveType), scratch2GPR);
            jit.load32(Address(metadataGPR, Metadata::offsetOfGlobalLexicalBindingEpoch()), scratch1GPR);
            slowCase.append(jit.branch32(NotEqual, Address(scratch2GPR, JSGlobalObject::offsetOfGlobalLexicalBindingEpoch()), scratch1GPR));
            jit.move(scratch2GPR, returnValueGPR);
            break;
        }

        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            // JSScope::constantScopeForCodeBlock() loads codeBlock->globalObject() for GlobalVar*,
            // and codeBlock->globalObject()->globalLexicalEnvironment() for GlobalLexicalVar*.
            loadGlobalObject(jit, scratch2GPR);
            doVarInjectionCheck(needsVarInjectionChecks(resolveType), scratch2GPR);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                jit.loadPtr(Address(scratch2GPR, JSGlobalObject::offsetOfGlobalLexicalEnvironment()), returnValueGPR);
            else
                jit.move(scratch2GPR, returnValueGPR);
            break;
        }
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitResolveClosure(needsVarInjectionChecks(resolveType));
            break;
        case Dynamic:
            slowCase.append(jit.jump());
            break;
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    if (profiledResolveType == ClosureVar)
        emitCode(ClosureVar);
    else if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        emitCode(ClosureVarWithVarInjectionChecks);
    else {
        JumpList skipToEnd;
        jit.load32(Address(metadataGPR, Metadata::offsetOfResolveType()), regT1);

        auto emitCaseWithoutCheck = [&] (ResolveType resolveType) {
            Jump notCase = jit.branch32(NotEqual, regT1, TrustedImm32(resolveType));
            emitCode(resolveType);
            skipToEnd.append(jit.jump());
            notCase.link(&jit);
        };

        auto emitCase = [&] (ResolveType resolveType) {
            if (resolveType != profiledResolveType)
                emitCaseWithoutCheck(resolveType);
        };

        // Check that we're the profiled resolve type first.
        switch (profiledResolveType) {
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            break;
        default:
            emitCaseWithoutCheck(profiledResolveType);
            break;
        }

        emitCase(GlobalVar);
        emitCase(GlobalProperty);
        emitCase(GlobalLexicalVar);
        emitCase(GlobalVarWithVarInjectionChecks);
        emitCase(GlobalPropertyWithVarInjectionChecks);
        emitCase(GlobalLexicalVarWithVarInjectionChecks);
        slowCase.append(jit.jump());

        skipToEnd.link(&jit);
    }

    jit.ret();

    slowCase.linkThunk(CodeLocationLabel { vm.getCTIStub(slow_op_resolve_scopeGenerator).retaggedCode<NoPtrTag>() }, &jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "resolve_scope"_s, "Baseline: resolve_scope");
}

MacroAssemblerCodeRef<JITThunkPtrTag> LOLJIT::slow_op_resolve_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using BaselineJITRegisters::ResolveScope::scopeGPR; // Incoming
    using BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR; // Incoming

    jit.emitCTIThunkPrologue(/* returnAddressAlreadyTagged: */ true); // Return address tagged in 'generateOpResolveScopeThunk'

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    // FIXME: Maybe it's profitable to pick the order of arguments for this to match the incoming GPRs.
    jit.setupArguments<decltype(operationResolveScopeForLOL)>(bytecodeOffsetGPR, scopeGPR);
    jit.callOperation<OperationPtrTag>(operationResolveScopeForLOL);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    jit.jumpThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::CheckException).retaggedCode<NoPtrTag>()));

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "slow_op_resolve_scope"_s, "Baseline: slow_op_resolve_scope");
}


} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(JIT) && USE(JSVALUE64)

