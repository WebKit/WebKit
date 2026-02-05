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

#pragma once

#if ENABLE(JIT) && USE(JSVALUE64)

#include "BaselineJITCode.h"
#include "BytecodeUseDef.h"
#include "CodeBlock.h"
#include "CommonSlowPaths.h"
#include "JIT.h"
#include "JITDisassembler.h"
#include "JITInlineCacheGenerator.h"
#include "JITMathIC.h"
#include "JITRightShiftGenerator.h"
#include "JSInterfaceJIT.h"
#include "LLIntData.h"
#include "LOLRegisterAllocator.h"
#include "PCToCodeOriginMap.h"
#include "SimpleRegisterAllocator.h"

#include <wtf/SequesteredMalloc.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>


namespace JSC::LOL {

#define FOR_EACH_IMPLEMENTED_OP(macro) \
    macro(op_add) \
    macro(op_mul) \
    macro(op_sub) \
    macro(op_negate) \
    macro(op_inc) \
    macro(op_dec) \
    macro(op_eq) \
    macro(op_neq) \
    macro(op_less) \
    macro(op_lesseq) \
    macro(op_greater) \
    macro(op_greatereq) \
    macro(op_resolve_scope) \
    macro(op_get_from_scope) \
    macro(op_put_to_scope) \
    macro(op_lshift) \
    macro(op_to_number) \
    macro(op_to_string) \
    macro(op_to_object) \
    macro(op_to_numeric) \
    macro(op_rshift) \
    macro(op_urshift) \
    macro(op_bitnot) \
    macro(op_bitand) \
    macro(op_bitor) \
    macro(op_bitxor) \
    macro(op_mov) \
    macro(op_is_empty) \
    macro(op_typeof_is_undefined) \
    macro(op_typeof_is_function) \
    macro(op_is_undefined_or_null) \
    macro(op_is_boolean) \
    macro(op_is_number) \
    macro(op_is_big_int) \
    macro(op_is_object) \
    macro(op_is_cell_with_type) \
    macro(op_has_structure_with_flags) \
    macro(op_jmp) \
    macro(op_jtrue) \
    macro(op_jfalse) \
    macro(op_jeq_null) \
    macro(op_jneq_null) \
    macro(op_jundefined_or_null) \
    macro(op_jnundefined_or_null) \
    macro(op_jeq_ptr) \
    macro(op_jneq_ptr) \
    macro(op_jeq) \
    macro(op_jneq) \
    macro(op_jless) \
    macro(op_jlesseq) \
    macro(op_jgreater) \
    macro(op_jgreatereq) \
    macro(op_jnless) \
    macro(op_jnlesseq) \
    macro(op_jngreater) \
    macro(op_jngreatereq) \
    macro(op_jstricteq) \
    macro(op_jnstricteq) \
    macro(op_jbelow) \
    macro(op_jbeloweq) \
    macro(op_create_lexical_environment) \
    macro(op_create_direct_arguments) \
    macro(op_create_scoped_arguments) \
    macro(op_create_cloned_arguments) \
    macro(op_new_array) \
    macro(op_new_array_with_size) \
    macro(op_new_func) \
    macro(op_new_func_exp) \
    macro(op_new_generator_func) \
    macro(op_new_generator_func_exp) \
    macro(op_new_async_func) \
    macro(op_new_async_func_exp) \
    macro(op_new_async_generator_func) \
    macro(op_new_async_generator_func_exp) \
    macro(op_new_object) \
    macro(op_new_reg_exp) \


#define FOR_EACH_OP_WITH_SLOW_CASE(macro) \
    macro(op_add) \
    macro(op_call_direct_eval) \
    macro(op_eq) \
    macro(op_try_get_by_id) \
    macro(op_in_by_id) \
    macro(op_in_by_val) \
    macro(op_has_private_name) \
    macro(op_has_private_brand) \
    macro(op_get_by_id) \
    macro(op_get_length) \
    macro(op_get_by_id_with_this) \
    macro(op_get_by_id_direct) \
    macro(op_get_by_val) \
    macro(op_get_by_val_with_this) \
    macro(op_enumerator_get_by_val) \
    macro(op_enumerator_put_by_val) \
    macro(op_get_private_name) \
    macro(op_set_private_brand) \
    macro(op_check_private_brand) \
    macro(op_instanceof) \
    macro(op_less) \
    macro(op_lesseq) \
    macro(op_greater) \
    macro(op_greatereq) \
    macro(op_jless) \
    macro(op_jlesseq) \
    macro(op_jgreater) \
    macro(op_jgreatereq) \
    macro(op_jnless) \
    macro(op_jnlesseq) \
    macro(op_jngreater) \
    macro(op_jngreatereq) \
    macro(op_jeq) \
    macro(op_jneq) \
    macro(op_jstricteq) \
    macro(op_jnstricteq) \
    macro(op_loop_hint) \
    macro(op_enter) \
    macro(op_check_traps) \
    macro(op_mod) \
    macro(op_pow) \
    macro(op_mul) \
    macro(op_negate) \
    macro(op_neq) \
    macro(op_new_object) \
    macro(op_put_by_id) \
    macro(op_put_by_val_direct) \
    macro(op_put_by_val) \
    macro(op_put_private_name) \
    macro(op_del_by_val) \
    macro(op_del_by_id) \
    macro(op_sub) \
    macro(op_resolve_scope) \
    macro(op_get_from_scope) \
    macro(op_put_to_scope) \
    macro(op_iterator_open) \
    macro(op_iterator_next) \

#define FOR_EACH_OP_WITH_OPERATION_SLOW_CASE(macro) \
    macro(op_unsigned) \
    macro(op_inc) \
    macro(op_dec) \
    macro(op_bitnot) \
    macro(op_bitand) \
    macro(op_bitor) \
    macro(op_bitxor) \
    macro(op_lshift) \
    macro(op_rshift) \
    macro(op_urshift) \
    macro(op_div) \
    macro(op_create_this) \
    macro(op_create_promise) \
    macro(op_create_generator) \
    macro(op_create_async_generator) \
    macro(op_to_this) \
    macro(op_to_primitive) \
    macro(op_to_number) \
    macro(op_to_numeric) \
    macro(op_to_string) \
    macro(op_to_object) \
    macro(op_not) \
    macro(op_stricteq) \
    macro(op_nstricteq) \
    macro(op_get_prototype_of) \
    macro(op_check_tdz) \
    macro(op_to_property_key) \
    macro(op_to_property_key_or_number) \
    macro(op_typeof_is_function) \


// We subclass ReplayBackend so we can just pass `this` to the ReplayAllocator as we can't use `{ }` for the allocator's internal Backend& in the constructor.
class LOLJIT : public JIT, public ReplayBackend {
    WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(LOLJIT);
public:

    LOLJIT(VM&, BaselineJITPlan&, CodeBlock*);

    RefPtr<BaselineJITCode> compileAndLinkWithoutFinalizing(JITCompilationEffort);

private:
    template<typename> friend class RegisterAllocator;
    void fill(VirtualRegister binding, GPRReg gpr)
    {
        JIT_COMMENT(*this, "Filling ", binding);
        emitGetVirtualRegister(binding, gpr);
    }

    void flush(const Location& location, GPRReg gpr, VirtualRegister binding)
    {
        JIT_COMMENT(*this, "Flushing ", binding);
        if (!location.isFlushed)
            emitPutVirtualRegister(binding, gpr);
#if ASSERT_ENABLED
        else {
            JIT_COMMENT(*this, " already flushed, validating");
            emitGetVirtualRegister(binding, scratchRegister());
            Jump ok = branch64(Equal, scratchRegister(), gpr);
            breakpoint();
            ok.link(this);
        }
#endif
    }

    ALWAYS_INLINE constexpr static bool hasSlowCase(OpcodeID op)
    {
        switch (op) {
#define HAS_SLOW_CASE(name) case name: return true;
        FOR_EACH_OP_WITH_OPERATION_SLOW_CASE(HAS_SLOW_CASE)
        FOR_EACH_OP_WITH_SLOW_CASE(HAS_SLOW_CASE)
        default: break;
#undef HAS_SLOW_CASE
        }
        return false;
    }

    ALWAYS_INLINE constexpr static bool isImplemented(OpcodeID op)
    {
        switch (op) {
#define HAS_SLOW_CASE(name) case name: return true;
        FOR_EACH_IMPLEMENTED_OP(HAS_SLOW_CASE)
        default: break;
#undef HAS_SLOW_CASE
        }
        return false;
    }

private:
    void privateCompileMainPass();
    void privateCompileSlowCases();

#define DECLARE_EMIT_METHODS(name) \
    void emit_##name(const JSInstruction*); \
    void emitSlow_##name(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
    FOR_EACH_IMPLEMENTED_OP(DECLARE_EMIT_METHODS)
#undef DECLARE_EMIT_METHODS

    void nextBytecodeIndexWithFlushForJumpTargetsIfNeeded(auto& allocator, bool shouldSetFastPathResumePoint)
    {
        auto next = BytecodeIndex(m_bytecodeIndex.offset() + m_currentInstruction->size());
        if (m_currentJumpTargetIndex < m_unlinkedCodeBlock->numberOfJumpTargets() && next.offset() == m_unlinkedCodeBlock->jumpTarget(m_currentJumpTargetIndex)) {
            if (shouldSetFastPathResumePoint) {
                // We need to set a resume point for slow paths to jump back to prior to flushing since the next instruction wouldn't have the flushes and we don't want to re-emit them in the slow path.
                // It's generally ok if a resume point is already set before here, it should still be correct w.r.t. flushing.
                //
                // For example:
                // [  X] op_add lhs: loc1, rhs: loc2, dst: loc3
                //       ... fast path code
                //   slowPathResume:
                //       ... flushing code
                // [  Y] op_loop_hint
                //   loopHintStart:
                //
                // If the slow path of op_add were to resume to loopHintStart rather than slowPathResume
                // it would have to flush (or be incorrect), which is mostly just worse for code gen/size.
                m_fastPathResumeLabels.add(m_bytecodeIndex, label());
            }

            JIT_COMMENT(*this, "Flush for jump target at bytecode ", m_bytecodeIndex);
            allocator.flushAllRegisters(*this);
            m_currentJumpTargetIndex++;
        }

        m_bytecodeIndex = next;
    }

    // Helpers
    template<typename Op>
    void emitRightShiftFastPath(const JSInstruction* currentInstruction, JITRightShiftGenerator::ShiftType snippetShiftType);

    void silentSpill(auto& allocator, const auto& allocations)
    {
        ScalarRegisterSet uses = RegisterSetBuilder::fromIterable(allocations.uses).buildScalarRegisterSet();
        ScalarRegisterSet defs = RegisterSetBuilder::fromIterable(allocations.defs).buildScalarRegisterSet();
        JIT_COMMENT(*this, "Silent spilling");
        for (Reg reg : allocator.allocatedRegisters()) {
            GPRReg gpr = reg.gpr();
            // We don't want to save defs unless they happen to alias a use. We need to save all uses
            // in case an exception is thrown by the operation and subsequently caught in the same frame.
            // FIXME: We could check if the exception would be caught in the same function here.
            if (defs.contains(gpr, IgnoreVectors) && !uses.contains(gpr, IgnoreVectors))
                continue;
            VirtualRegister binding = allocator.bindingFor(gpr);
            // This is scratch
            if (!binding.isValid())
                continue;
            Location location = allocator.locationOf(binding);
            ASSERT(location.gpr() == gpr);
            if (!location.isFlushed)
                emitPutVirtualRegister(binding, JSValueRegs(gpr));
        }
    }

    void silentFill(auto& allocator, auto... excludeGPRs)
    {
        JIT_COMMENT(*this, "Silent filling");
        for (Reg reg : allocator.allocatedRegisters()) {
            GPRReg gpr = reg.gpr();
            if (((gpr == excludeGPRs) || ... || false))
                continue;
            VirtualRegister binding = allocator.bindingFor(gpr);
            // This is scratch
            if (!binding.isValid())
                continue;
            ASSERT(allocator.locationOf(binding).gpr() == gpr);
            emitGetVirtualRegister(binding, JSValueRegs(gpr));
        }
    }

    void emitWriteBarrier(const auto& allocator, const auto& allocations, JSValueRegs owner, JSValueRegs value, GPRReg scratch, WriteBarrierMode mode)
    {
        ASSERT(noOverlap(owner, value.payloadGPR(), scratch));

        JumpList done;
        if (mode == ShouldFilterBase || mode == ShouldFilterBaseAndValue)
            done.append(branchIfNotCell(owner));
        else
            jitAssertIsCell(owner.payloadGPR());

        if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue)
            done.append(branchIfNotCell(value));

        // TODO: We should have a way to add out-of-line slow paths in a encapsulated way i.e. some addSlowPath(lambda)
        done.append(barrierBranch(vm(), owner.payloadGPR(), scratch));
        silentSpill(allocator, allocations);
        callOperationNoExceptionCheck(operationWriteBarrierSlowPath, TrustedImmPtr(&vm()), owner.payloadGPR());
        silentFill(allocator);

        done.link(this);
    }

    void emitLoadCharacterString(RegisterID src, RegisterID dst, JumpList& failures);

    template<typename Op>
        requires (LOLJIT::isImplemented(Op::opcodeID))
    void emitCommonSlowPathSlowCaseCall(const JSInstruction*, Vector<SlowCaseEntry>::iterator&, SlowPathFunction);

    template<typename Op>
        requires (!LOLJIT::isImplemented(Op::opcodeID))
    void emitCommonSlowPathSlowCaseCall(const JSInstruction*, Vector<SlowCaseEntry>::iterator&, SlowPathFunction);

    template<typename Op, typename SnippetGenerator>
    void emitBitBinaryOpFastPath(const JSInstruction* currentInstruction);

    template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
    void emitMathICFast(JITUnaryMathIC<Generator>*, const JSInstruction*, ProfiledFunction, NonProfiledFunction);
    template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
    void emitMathICFast(JITBinaryMathIC<Generator>*, const JSInstruction*, ProfiledFunction, NonProfiledFunction);

    template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
    void emitMathICSlow(JITBinaryMathIC<Generator>*, const JSInstruction*, ProfiledRepatchFunction, ProfiledFunction, RepatchFunction, Vector<SlowCaseEntry>::iterator&);
    template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
    void emitMathICSlow(JITUnaryMathIC<Generator>*, const JSInstruction*, ProfiledRepatchFunction, ProfiledFunction, RepatchFunction, Vector<SlowCaseEntry>::iterator&);

    template<typename Op>
    void emitCompare(const JSInstruction*, RelationalCondition);
    template <typename EmitCompareFunctor>
    void emitCompareImpl(VirtualRegister op1, JSValueRegs op1Regs, VirtualRegister op2, JSValueRegs op2Regs, RelationalCondition, const EmitCompareFunctor&);

    template<typename Op>
    void emitCompareAndJump(const JSInstruction*, RelationalCondition);

    template<typename Op, typename SlowOperation>
    void emitCompareSlow(const JSInstruction*, DoubleCondition, SlowOperation, Vector<SlowCaseEntry>::iterator&);
    template<typename SlowOperation>
    void emitCompareSlowImpl(const auto& allocations, VirtualRegister op1, JSValueRegs op1Regs, VirtualRegister op2, JSValueRegs op2Regs, JSValueRegs dstRegs, SlowOperation, Vector<SlowCaseEntry>::iterator&, const Invocable<void(FPRReg, FPRReg)> auto&);

    template<typename Op, typename SlowOperation>
    void emitCompareAndJumpSlow(const JSInstruction*, DoubleCondition, SlowOperation, bool invertOperationResult, Vector<SlowCaseEntry>::iterator&);

    template<typename Op>
    void emitCompareUnsignedAndJumpImpl(const JSInstruction*, RelationalCondition);

    template<typename Op>
    void emitStrictEqJumpImpl(const JSInstruction*, RelationalCondition);
    template<typename Op>
    void emitStrictEqJumpSlowImpl(const JSInstruction*, ResultCondition, Vector<SlowCaseEntry>::iterator&);

    template<typename Op>
    void emitNewFuncCommon(const JSInstruction*);
    template<typename Op>
    void emitNewFuncExprCommon(const JSInstruction*);

    static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_get_from_scopeGenerator(VM&);
    static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_resolve_scopeGenerator(VM&);
    static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_put_to_scopeGenerator(VM&);
    template <ResolveType>
    static MacroAssemblerCodeRef<JITThunkPtrTag> generateOpGetFromScopeThunk(VM&);
    template <ResolveType>
    static MacroAssemblerCodeRef<JITThunkPtrTag> generateOpResolveScopeThunk(VM&);

    Vector<RegisterSet> m_liveTempsForSlowPaths;
    Vector<JSValueRegs> m_slowPathOperandRegs;
    unsigned m_currentSlowPathOperandIndex;
    unsigned m_currentJumpTargetIndex;

    // This is laid out as [ locals, constants, headers, arguments ]
    // FixedVector<Location> m_locations;
    RegisterAllocator<LOLJIT> m_fastAllocator;
    static constexpr GPRReg s_scratch = RegisterAllocator<LOLJIT>::s_scratch;
    static constexpr JSValueRegs s_scratchRegs = JSValueRegs { s_scratch };
    ReplayRegisterAllocator m_replayAllocator;
    // SimpleRegisterAllocator<GPRBank> m_allocator;
    const JSInstruction* m_currentInstruction;
};

} // namespace JSC

#endif // ENABLE(JIT) && USE(JSVALUE64)

