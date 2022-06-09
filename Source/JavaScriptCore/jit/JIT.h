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

#pragma once

#if ENABLE(JIT)

// We've run into some problems where changing the size of the class JIT leads to
// performance fluctuations. Try forcing alignment in an attempt to stabilize this.
#if COMPILER(GCC_COMPATIBLE)
#define JIT_CLASS_ALIGNMENT alignas(32)
#else
#define JIT_CLASS_ALIGNMENT
#endif

#define ASSERT_JIT_OFFSET(actual, expected) ASSERT_WITH_MESSAGE(actual == expected, "JIT Offset \"%s\" should be %d, not %d.\n", #expected, static_cast<int>(expected), static_cast<int>(actual));

#include "BaselineJITCode.h"
#include "CodeBlock.h"
#include "CommonSlowPaths.h"
#include "JITDisassembler.h"
#include "JITInlineCacheGenerator.h"
#include "JITMathIC.h"
#include "JITRightShiftGenerator.h"
#include "JSInterfaceJIT.h"
#include "LLIntData.h"
#include "PCToCodeOriginMap.h"
#include "UnusedPointer.h"
#include <wtf/UniqueRef.h>

namespace JSC {

    enum OpcodeID : unsigned;

    class ArrayAllocationProfile;
    class CallLinkInfo;
    class CodeBlock;
    class FunctionExecutable;
    class JIT;
    class Identifier;
    class Interpreter;
    class BlockDirectory;
    class Register;
    class StructureChain;
    class StructureStubInfo;

    struct Instruction;
    struct OperandTypes;
    struct SimpleJumpTable;
    struct StringJumpTable;

    struct OpPutByVal;
    struct OpPutByValDirect;
    struct OpPutPrivateName;
    struct OpPutToScope;

    template<PtrTag tag>
    struct CallRecord {
        MacroAssembler::Call from;
        FunctionPtr<tag> callee;

        CallRecord()
        {
        }

        CallRecord(MacroAssembler::Call from, FunctionPtr<tag> callee)
            : from(from)
            , callee(callee)
        {
        }
    };

    using FarCallRecord = CallRecord<OperationPtrTag>;
    using NearCallRecord = CallRecord<JSInternalPtrTag>;

    struct NearJumpRecord {
        MacroAssembler::Jump from;
        CodeLocationLabel<JITThunkPtrTag> target;

        NearJumpRecord() = default;
        NearJumpRecord(MacroAssembler::Jump from, CodeLocationLabel<JITThunkPtrTag> target)
            : from(from)
            , target(target)
        { }
    };

    struct JumpTable {
        MacroAssembler::Jump from;
        unsigned toBytecodeOffset;

        JumpTable(MacroAssembler::Jump f, unsigned t)
            : from(f)
            , toBytecodeOffset(t)
        {
        }
    };

    struct SlowCaseEntry {
        MacroAssembler::Jump from;
        BytecodeIndex to;
        
        SlowCaseEntry(MacroAssembler::Jump f, BytecodeIndex t)
            : from(f)
            , to(t)
        {
        }
    };

    struct SwitchRecord {
        enum Type {
            Immediate,
            Character,
            String
        };

        Type type;

        BytecodeIndex bytecodeIndex;
        unsigned defaultOffset;
        unsigned tableIndex;

        SwitchRecord(unsigned tableIndex, BytecodeIndex bytecodeIndex, unsigned defaultOffset, Type type)
            : type(type)
            , bytecodeIndex(bytecodeIndex)
            , defaultOffset(defaultOffset)
            , tableIndex(tableIndex)
        {
        }
    };

    struct CallCompilationInfo {
        MacroAssembler::Label doneLocation;
        UnlinkedCallLinkInfo* unlinkedCallLinkInfo;
    };

    void ctiPatchCallByReturnAddress(ReturnAddressPtr, FunctionPtr<CFunctionPtrTag> newCalleeFunction);

    class JIT_CLASS_ALIGNMENT JIT : public JSInterfaceJIT {
        friend class JITSlowPathCall;
        friend class JITStubCall;
        friend class JITThunks;

        using MacroAssembler::Jump;
        using MacroAssembler::JumpList;
        using MacroAssembler::Label;

        static constexpr uintptr_t patchGetByIdDefaultStructure = unusedPointer;
        static constexpr int patchGetByIdDefaultOffset = 0;
        // Magic number - initial offset cannot be representable as a signed 8bit value, or the X86Assembler
        // will compress the displacement, and we may not be able to fit a patched offset.
        static constexpr int patchPutByIdDefaultOffset = 256;

        using Base = JSInterfaceJIT;

    public:
        JIT(VM&, CodeBlock* = nullptr, BytecodeIndex loopOSREntryBytecodeOffset = BytecodeIndex(0));
        ~JIT();

        VM& vm() { return *JSInterfaceJIT::vm(); }

        void compileAndLinkWithoutFinalizing(JITCompilationEffort);
        CompilationResult finalizeOnMainThread(CodeBlock*);
        size_t codeSize() const;

        void doMainThreadPreparationBeforeCompile();
        
        static CompilationResult compile(VM& vm, CodeBlock* codeBlock, JITCompilationEffort effort, BytecodeIndex bytecodeOffset = BytecodeIndex(0))
        {
            return JIT(vm, codeBlock, bytecodeOffset).privateCompile(codeBlock, effort);
        }

        static unsigned frameRegisterCountFor(UnlinkedCodeBlock*);
        static unsigned frameRegisterCountFor(CodeBlock*);
        static int stackPointerOffsetFor(UnlinkedCodeBlock*);
        static int stackPointerOffsetFor(CodeBlock*);

        JS_EXPORT_PRIVATE static HashMap<CString, Seconds> compileTimeStats();
        JS_EXPORT_PRIVATE static Seconds totalCompileTime();

        static constexpr GPRReg s_metadataGPR = LLInt::Registers::metadataTableGPR;
        static constexpr GPRReg s_constantsGPR = LLInt::Registers::pbGPR;

    private:
        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        void link();
        CompilationResult privateCompile(CodeBlock*, JITCompilationEffort);

        // Add a call out from JIT code, without an exception check.
        Call appendCall(const FunctionPtr<CFunctionPtrTag> function)
        {
            Call functionCall = call(OperationPtrTag);
            m_farCalls.append(FarCallRecord(functionCall, function.retagged<OperationPtrTag>()));
            return functionCall;
        }

        void appendCall(Address function)
        {
            call(function, OperationPtrTag);
        }

#if OS(WINDOWS) && CPU(X86_64)
        Call appendCallWithSlowPathReturnType(const FunctionPtr<CFunctionPtrTag> function)
        {
            Call functionCall = callWithSlowPathReturnType(OperationPtrTag);
            m_farCalls.append(FarCallRecord(functionCall, function.retagged<OperationPtrTag>()));
            return functionCall;
        }
#endif

        template <typename Bytecode>
        void loadPtrFromMetadata(const Bytecode&, size_t offset, GPRReg);

        template <typename Bytecode>
        void load32FromMetadata(const Bytecode&, size_t offset, GPRReg);

        template <typename Bytecode>
        void load8FromMetadata(const Bytecode&, size_t offset, GPRReg);

        template <typename ValueType, typename Bytecode>
        void store8ToMetadata(ValueType, const Bytecode&, size_t offset);

        template <typename Bytecode>
        void store32ToMetadata(GPRReg, const Bytecode&, size_t offset);

        template <typename Bytecode>
        void materializePointerIntoMetadata(const Bytecode&, size_t offset, GPRReg);

    public:
        void loadConstant(unsigned constantIndex, GPRReg);
    private:
        void loadGlobalObject(GPRReg);
        void loadCodeBlockConstant(VirtualRegister, JSValueRegs);
        void loadCodeBlockConstantPayload(VirtualRegister, RegisterID);
#if USE(JSVALUE32_64)
        void loadCodeBlockConstantTag(VirtualRegister, RegisterID);
#endif

        void emitPutCodeBlockToFrameInPrologue(GPRReg result = regT0);

        void exceptionCheck(Jump jumpToHandler)
        {
            m_exceptionChecks.append(jumpToHandler);
        }

        void exceptionCheck()
        {
            m_exceptionChecks.append(emitExceptionCheck(vm()));
        }

        void exceptionCheckWithCallFrameRollback()
        {
            m_exceptionChecksWithCallFrameRollback.append(emitExceptionCheck(vm()));
        }

        void privateCompileExceptionHandlers();

        void advanceToNextCheckpoint();
        void emitJumpSlowToHotForCheckpoint(Jump);
        void setFastPathResumePoint();
        Label fastPathResumePoint() const;

        void addSlowCase(Jump);
        void addSlowCase(const JumpList&);
        void addSlowCase();
        void addJump(Jump, int);
        void addJump(const JumpList&, int);
        void emitJumpSlowToHot(Jump, int);

        template<typename Op>
        void compileOpCall(const Instruction*, unsigned callLinkInfoIndex);
        template<typename Op>
        void compileOpCallSlowCase(const Instruction*, Vector<SlowCaseEntry>::iterator&, unsigned callLinkInfoIndex);

        template<typename Op>
        std::enable_if_t<
            Op::opcodeID != op_call_varargs && Op::opcodeID != op_construct_varargs
            && Op::opcodeID != op_tail_call_varargs && Op::opcodeID != op_tail_call_forward_arguments
        , void> compileSetupFrame(const Op&);

        template<typename Op>
        std::enable_if_t<
            Op::opcodeID == op_call_varargs || Op::opcodeID == op_construct_varargs
            || Op::opcodeID == op_tail_call_varargs || Op::opcodeID == op_tail_call_forward_arguments
        , void> compileSetupFrame(const Op&);

        template<typename Op>
        bool compileTailCall(const Op&, UnlinkedCallLinkInfo*, unsigned callLinkInfoIndex);
        template<typename Op>
        bool compileCallEval(const Op&);
        void compileCallEvalSlowCase(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        template<typename Op>
        void emitPutCallResult(const Op&);

#if USE(JSVALUE64)
        template<typename Op> void compileOpStrictEq(const Instruction*);
        template<typename Op> void compileOpStrictEqJump(const Instruction*);
#elif USE(JSVALUE32_64)
        void compileOpEqCommon(VirtualRegister src1, VirtualRegister src2);
        void compileOpEqSlowCommon(Vector<SlowCaseEntry>::iterator&);
        void compileOpStrictEqCommon(VirtualRegister src1,  VirtualRegister src2);
#endif

        enum WriteBarrierMode { UnconditionalWriteBarrier, ShouldFilterBase, ShouldFilterValue, ShouldFilterBaseAndValue };
        // value register in write barrier is used before any scratch registers
        // so may safely be the same as either of the scratch registers.
        void emitWriteBarrier(VirtualRegister owner, WriteBarrierMode);
        void emitWriteBarrier(VirtualRegister owner, VirtualRegister value, WriteBarrierMode);
        void emitWriteBarrier(JSCell* owner);
        void emitWriteBarrier(GPRReg owner);

        template<typename Bytecode> void emitValueProfilingSite(const Bytecode&, JSValueRegs);

        template<typename Op>
        static inline constexpr bool isProfiledOp = std::is_same_v<decltype(Op::Metadata::m_profile), ValueProfile>;
        template<typename Op>
        std::enable_if_t<isProfiledOp<Op>, void>
        emitValueProfilingSiteIfProfiledOpcode(Op bytecode)
        { // This assumes that the value to profile is in jsRegT10.
            emitValueProfilingSite(bytecode, jsRegT10);
        }
        template<typename Op>
        std::enable_if_t<!isProfiledOp<Op>, void>
        emitValueProfilingSiteIfProfiledOpcode(Op)
        { }

        template <typename Bytecode>
        void emitArrayProfilingSiteWithCell(const Bytecode&, RegisterID cellGPR, RegisterID scratchGPR);
        template <typename Bytecode>
        void emitArrayProfilingSiteWithCell(const Bytecode&, ptrdiff_t, RegisterID cellGPR, RegisterID scratchGPR);

        template<typename Op>
        ECMAMode ecmaMode(Op);

        void emitGetVirtualRegister(VirtualRegister src, JSValueRegs dst);
        void emitGetVirtualRegisterPayload(VirtualRegister src, RegisterID dst);
        void emitPutVirtualRegister(VirtualRegister dst, JSValueRegs src);

#if USE(JSVALUE32_64)
        void emitGetVirtualRegisterTag(VirtualRegister src, RegisterID dst);
#elif USE(JSVALUE64)
        // Machine register variants purely for convenience
        void emitGetVirtualRegister(VirtualRegister src, RegisterID dst);
        void emitPutVirtualRegister(VirtualRegister dst, RegisterID from);

        Jump emitJumpIfNotInt(RegisterID, RegisterID, RegisterID scratch);
        void emitJumpSlowCaseIfNotInt(RegisterID, RegisterID, RegisterID scratch);
        void emitJumpSlowCaseIfNotInt(RegisterID);
#endif

        void emitJumpSlowCaseIfNotInt(JSValueRegs);

        void emitJumpSlowCaseIfNotJSCell(JSValueRegs);
        void emitJumpSlowCaseIfNotJSCell(JSValueRegs, VirtualRegister);

        template<typename Op>
        void emit_compareAndJump(const Instruction*, RelationalCondition);
        void emit_compareAndJumpImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, RelationalCondition);
        template<typename Op>
        void emit_compareUnsigned(const Instruction*, RelationalCondition);
        void emit_compareUnsignedImpl(VirtualRegister dst, VirtualRegister op1, VirtualRegister op2, RelationalCondition);
        template<typename Op>
        void emit_compareUnsignedAndJump(const Instruction*, RelationalCondition);
        void emit_compareUnsignedAndJumpImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, RelationalCondition);
        template<typename Op, typename SlowOperation>
        void emit_compareAndJumpSlow(const Instruction*, DoubleCondition, SlowOperation, bool invert, Vector<SlowCaseEntry>::iterator&);
        template<typename SlowOperation>
        void emit_compareAndJumpSlowImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, size_t instructionSize, DoubleCondition, SlowOperation, bool invert, Vector<SlowCaseEntry>::iterator&);
        
        void assertStackPointerOffset();

        void emit_op_add(const Instruction*);
        void emit_op_bitand(const Instruction*);
        void emit_op_bitor(const Instruction*);
        void emit_op_bitxor(const Instruction*);
        void emit_op_bitnot(const Instruction*);
        void emit_op_call(const Instruction*);
        void emit_op_tail_call(const Instruction*);
        void emit_op_call_eval(const Instruction*);
        void emit_op_call_varargs(const Instruction*);
        void emit_op_tail_call_varargs(const Instruction*);
        void emit_op_tail_call_forward_arguments(const Instruction*);
        void emit_op_construct_varargs(const Instruction*);
        void emit_op_catch(const Instruction*);
        void emit_op_construct(const Instruction*);
        void emit_op_create_this(const Instruction*);
        void emit_op_to_this(const Instruction*);
        void emit_op_get_argument(const Instruction*);
        void emit_op_argument_count(const Instruction*);
        void emit_op_get_rest_length(const Instruction*);
        void emit_op_check_tdz(const Instruction*);
        void emit_op_identity_with_profile(const Instruction*);
        void emit_op_debug(const Instruction*);
        void emit_op_del_by_id(const Instruction*);
        void emitSlow_op_del_by_id(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emit_op_del_by_val(const Instruction*);
        void emitSlow_op_del_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emit_op_div(const Instruction*);
        void emit_op_end(const Instruction*);
        void emit_op_enter(const Instruction*);
        void emit_op_get_scope(const Instruction*);
        void emit_op_eq(const Instruction*);
        void emit_op_eq_null(const Instruction*);
        void emit_op_below(const Instruction*);
        void emit_op_beloweq(const Instruction*);
        void emit_op_try_get_by_id(const Instruction*);
        void emit_op_get_by_id(const Instruction*);
        void emit_op_get_by_id_with_this(const Instruction*);
        void emit_op_get_by_id_direct(const Instruction*);
        void emit_op_get_by_val(const Instruction*);
        void emit_op_get_private_name(const Instruction*);
        void emit_op_set_private_brand(const Instruction*);
        void emit_op_check_private_brand(const Instruction*);
        void emit_op_get_argument_by_val(const Instruction*);
        void emit_op_get_prototype_of(const Instruction*);
        void emit_op_in_by_id(const Instruction*);
        void emit_op_in_by_val(const Instruction*);
        void emit_op_has_private_name(const Instruction*);
        void emit_op_has_private_brand(const Instruction*);
        void emit_op_init_lazy_reg(const Instruction*);
        void emit_op_overrides_has_instance(const Instruction*);
        void emit_op_instanceof(const Instruction*);
        void emit_op_is_empty(const Instruction*);
        void emit_op_typeof_is_undefined(const Instruction*);
        void emit_op_is_undefined_or_null(const Instruction*);
        void emit_op_is_boolean(const Instruction*);
        void emit_op_is_number(const Instruction*);
#if USE(BIGINT32)
        void emit_op_is_big_int(const Instruction*);
#else
        NO_RETURN void emit_op_is_big_int(const Instruction*);
#endif
        void emit_op_is_object(const Instruction*);
        void emit_op_is_cell_with_type(const Instruction*);
        void emit_op_jeq_null(const Instruction*);
        void emit_op_jfalse(const Instruction*);
        void emit_op_jmp(const Instruction*);
        void emit_op_jneq_null(const Instruction*);
        void emit_op_jundefined_or_null(const Instruction*);
        void emit_op_jnundefined_or_null(const Instruction*);
        void emit_op_jeq_ptr(const Instruction*);
        void emit_op_jneq_ptr(const Instruction*);
        void emit_op_jless(const Instruction*);
        void emit_op_jlesseq(const Instruction*);
        void emit_op_jgreater(const Instruction*);
        void emit_op_jgreatereq(const Instruction*);
        void emit_op_jnless(const Instruction*);
        void emit_op_jnlesseq(const Instruction*);
        void emit_op_jngreater(const Instruction*);
        void emit_op_jngreatereq(const Instruction*);
        void emit_op_jeq(const Instruction*);
        void emit_op_jneq(const Instruction*);
        void emit_op_jstricteq(const Instruction*);
        void emit_op_jnstricteq(const Instruction*);
        void emit_op_jbelow(const Instruction*);
        void emit_op_jbeloweq(const Instruction*);
        void emit_op_jtrue(const Instruction*);
        void emit_op_loop_hint(const Instruction*);
        void emit_op_check_traps(const Instruction*);
        void emit_op_nop(const Instruction*);
        void emit_op_super_sampler_begin(const Instruction*);
        void emit_op_super_sampler_end(const Instruction*);
        void emit_op_lshift(const Instruction*);
        void emit_op_mod(const Instruction*);
        void emit_op_pow(const Instruction*);
        void emit_op_mov(const Instruction*);
        void emit_op_mul(const Instruction*);
        void emit_op_negate(const Instruction*);
        void emit_op_neq(const Instruction*);
        void emit_op_neq_null(const Instruction*);
        void emit_op_new_array(const Instruction*);
        void emit_op_new_array_with_size(const Instruction*);
        void emit_op_new_func(const Instruction*);
        void emit_op_new_func_exp(const Instruction*);
        void emit_op_new_generator_func(const Instruction*);
        void emit_op_new_generator_func_exp(const Instruction*);
        void emit_op_new_async_func(const Instruction*);
        void emit_op_new_async_func_exp(const Instruction*);
        void emit_op_new_async_generator_func(const Instruction*);
        void emit_op_new_async_generator_func_exp(const Instruction*);
        void emit_op_new_object(const Instruction*);
        void emit_op_new_regexp(const Instruction*);
        void emit_op_not(const Instruction*);
        void emit_op_nstricteq(const Instruction*);
        void emit_op_dec(const Instruction*);
        void emit_op_inc(const Instruction*);
        void emit_op_profile_type(const Instruction*);
        void emit_op_profile_control_flow(const Instruction*);
        void emit_op_get_parent_scope(const Instruction*);
        void emit_op_put_by_id(const Instruction*);
        template<typename Op = OpPutByVal>
        void emit_op_put_by_val(const Instruction*);
        void emit_op_put_by_val_direct(const Instruction*);
        void emit_op_put_private_name(const Instruction*);
        void emit_op_put_getter_by_id(const Instruction*);
        void emit_op_put_setter_by_id(const Instruction*);
        void emit_op_put_getter_setter_by_id(const Instruction*);
        void emit_op_put_getter_by_val(const Instruction*);
        void emit_op_put_setter_by_val(const Instruction*);
        void emit_op_ret(const Instruction*);
        void emit_op_rshift(const Instruction*);
        void emit_op_set_function_name(const Instruction*);
        void emit_op_stricteq(const Instruction*);
        void emit_op_sub(const Instruction*);
        void emit_op_switch_char(const Instruction*);
        void emit_op_switch_imm(const Instruction*);
        void emit_op_switch_string(const Instruction*);
        void emit_op_tear_off_arguments(const Instruction*);
        void emit_op_throw(const Instruction*);
        void emit_op_to_number(const Instruction*);
        void emit_op_to_numeric(const Instruction*);
        void emit_op_to_string(const Instruction*);
        void emit_op_to_object(const Instruction*);
        void emit_op_to_primitive(const Instruction*);
        void emit_op_unexpected_load(const Instruction*);
        void emit_op_unsigned(const Instruction*);
        void emit_op_urshift(const Instruction*);
        void emit_op_get_internal_field(const Instruction*);
        void emit_op_put_internal_field(const Instruction*);
        void emit_op_log_shadow_chicken_prologue(const Instruction*);
        void emit_op_log_shadow_chicken_tail(const Instruction*);
        void emit_op_to_property_key(const Instruction*);

        template<typename OpcodeType>
        void generateGetByValSlowCase(const OpcodeType&, Vector<SlowCaseEntry>::iterator&);

        void emit_op_get_property_enumerator(const Instruction*);
        void emit_op_enumerator_next(const Instruction*);
        void emit_op_enumerator_get_by_val(const Instruction*);
        void emitSlow_op_enumerator_get_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&);

        template<typename OpcodeType, typename SlowPathFunctionType>
        void emit_enumerator_has_propertyImpl(const Instruction*, const OpcodeType&, SlowPathFunctionType);
        void emit_op_enumerator_in_by_val(const Instruction*);
        void emit_op_enumerator_has_own_property(const Instruction*);

        void emitSlow_op_add(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_tail_call(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_eval(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_varargs(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_tail_call_varargs(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_tail_call_forward_arguments(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_construct_varargs(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_construct(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_eq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_callee(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_try_get_by_id(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id_with_this(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id_direct(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_private_name(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_set_private_brand(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_check_private_brand(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_argument_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_in_by_id(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_in_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_has_private_name(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_has_private_brand(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_instanceof(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jless(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jlesseq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jgreater(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jgreatereq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnless(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnlesseq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jngreater(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jngreatereq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jeq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jneq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jstricteq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnstricteq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_hint(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_check_traps(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mod(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_pow(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mul(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_negate(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_neq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_new_object(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_id(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_private_name(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_sub(const Instruction*, Vector<SlowCaseEntry>::iterator&);

        void emit_op_resolve_scope(const Instruction*);
        void emit_op_get_from_scope(const Instruction*);
        void emit_op_put_to_scope(const Instruction*);
        void emit_op_get_from_arguments(const Instruction*);
        void emit_op_put_to_arguments(const Instruction*);
        void emitSlow_op_put_to_scope(const Instruction*, Vector<SlowCaseEntry>::iterator&);

        void emitSlowCaseCall(const Instruction*, Vector<SlowCaseEntry>::iterator&, SlowPathFunction);

        void emit_op_iterator_open(const Instruction*);
        void emitSlow_op_iterator_open(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emit_op_iterator_next(const Instruction*);
        void emitSlow_op_iterator_next(const Instruction*, Vector<SlowCaseEntry>::iterator&);

        void emitHasPrivate(VirtualRegister dst, VirtualRegister base, VirtualRegister propertyOrBrand, AccessType);
        void emitHasPrivateSlow(VirtualRegister base, VirtualRegister property, AccessType);

        template<typename Op>
        void emitNewFuncCommon(const Instruction*);
        template<typename Op>
        void emitNewFuncExprCommon(const Instruction*);
        void emitVarInjectionCheck(bool needsVarInjectionChecks, GPRReg);
        void emitVarReadOnlyCheck(ResolveType, GPRReg scratchGPR);
        void emitNotifyWriteWatchpoint(GPRReg pointerToSet);

        bool isKnownCell(VirtualRegister);

        JSValue getConstantOperand(VirtualRegister);

#if USE(JSVALUE64)
        bool isOperandConstantDouble(VirtualRegister);
        double getOperandConstantDouble(VirtualRegister src);
#endif
        bool isOperandConstantInt(VirtualRegister);
        int32_t getOperandConstantInt(VirtualRegister src);
        bool isOperandConstantChar(VirtualRegister);

        template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
        void emitMathICFast(JITUnaryMathIC<Generator>*, const Instruction*, ProfiledFunction, NonProfiledFunction);
        template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
        void emitMathICFast(JITBinaryMathIC<Generator>*, const Instruction*, ProfiledFunction, NonProfiledFunction);

        template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
        void emitMathICSlow(JITBinaryMathIC<Generator>*, const Instruction*, ProfiledRepatchFunction, ProfiledFunction, RepatchFunction);
        template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
        void emitMathICSlow(JITUnaryMathIC<Generator>*, const Instruction*, ProfiledRepatchFunction, ProfiledFunction, RepatchFunction);

    public:
        static MacroAssemblerCodeRef<JITThunkPtrTag> returnFromBaselineGenerator(VM&);

    private:
#if ENABLE(EXTRA_CTI_THUNKS)
        // Thunk generators.
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_del_by_id_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_del_by_val_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_get_by_id_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_get_by_id_with_this_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_get_by_val_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_get_private_name_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_put_by_id_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_put_by_val_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_put_private_name_prepareCallGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_put_to_scopeGenerator(VM&);

        static MacroAssemblerCodeRef<JITThunkPtrTag> op_check_traps_handlerGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> op_enter_handlerGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> op_throw_handlerGenerator(VM&);

        static constexpr bool thunkIsUsedForOpGetFromScope(ResolveType resolveType)
        {
            // GlobalVar because it is more efficient to emit inline than use a thunk.
            // ResolvedClosureVar and ModuleVar because we don't use these types with op_get_from_scope.
            return !(resolveType == GlobalVar || resolveType == ResolvedClosureVar || resolveType == ModuleVar);
        }

        static MacroAssemblerCodeRef<JITThunkPtrTag> valueIsFalseyGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> valueIsTruthyGenerator(VM&);
#endif // ENABLE(EXTRA_CTI_THUNKS)
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_get_from_scopeGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_resolve_scopeGenerator(VM&);
        template <ResolveType>
        static MacroAssemblerCodeRef<JITThunkPtrTag> generateOpGetFromScopeThunk(VM&);
        template <ResolveType>
        static MacroAssemblerCodeRef<JITThunkPtrTag> generateOpResolveScopeThunk(VM&);

        Jump getSlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            return iter++->from;
        }
        void linkSlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            if (iter->from.isSet())
                iter->from.link(this);
            ++iter;
        }
        void linkAllSlowCasesForBytecodeIndex(Vector<SlowCaseEntry>& slowCases,
            Vector<SlowCaseEntry>::iterator&, BytecodeIndex bytecodeOffset);

        void linkAllSlowCases(Vector<SlowCaseEntry>::iterator& iter)
        {
            linkAllSlowCasesForBytecodeIndex(m_slowCases, iter, m_bytecodeIndex);
        }

        bool hasAnySlowCases(Vector<SlowCaseEntry>& slowCases, Vector<SlowCaseEntry>::iterator&, BytecodeIndex bytecodeOffset);
        bool hasAnySlowCases(Vector<SlowCaseEntry>::iterator& iter)
        {
            return hasAnySlowCases(m_slowCases, iter, m_bytecodeIndex);
        }

        MacroAssembler::Call appendCallWithExceptionCheck(const FunctionPtr<CFunctionPtrTag>);
        void appendCallWithExceptionCheck(Address);
#if OS(WINDOWS) && CPU(X86_64)
        MacroAssembler::Call appendCallWithExceptionCheckAndSlowPathReturnType(const FunctionPtr<CFunctionPtrTag>);
#endif
        MacroAssembler::Call appendCallWithCallFrameRollbackOnException(const FunctionPtr<CFunctionPtrTag>);
        MacroAssembler::Call appendCallWithExceptionCheckSetJSValueResult(const FunctionPtr<CFunctionPtrTag>, VirtualRegister result);
        void appendCallWithExceptionCheckSetJSValueResult(Address, VirtualRegister result);
        template<typename Bytecode>
        MacroAssembler::Call appendCallWithExceptionCheckSetJSValueResultWithProfile(const Bytecode&, const FunctionPtr<CFunctionPtrTag>, VirtualRegister result);
        template<typename Bytecode>
        void appendCallWithExceptionCheckSetJSValueResultWithProfile(const Bytecode&, Address, VirtualRegister result);
        
        template<typename OperationType, typename... Args>
        std::enable_if_t<FunctionTraits<OperationType>::hasResult, MacroAssembler::Call>
        callOperation(OperationType operation, VirtualRegister result, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheckSetJSValueResult(operation, result);
        }

        template<typename OperationType, typename... Args>
        std::enable_if_t<FunctionTraits<OperationType>::hasResult, void>
        callOperation(Address target, VirtualRegister result, Args... args)
        {
            setupArgumentsForIndirectCall<OperationType>(target, args...);
            return appendCallWithExceptionCheckSetJSValueResult(Address(GPRInfo::nonArgGPR0, target.offset), result);
        }

#if OS(WINDOWS) && CPU(X86_64)
        template<typename Type>
        struct is64BitType {
            static constexpr bool value = sizeof(Type) <= 8;
        };

        template<>
        struct is64BitType<void> {
            static constexpr bool value = true;
        };

        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperation(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            // x64 Windows cannot use standard call when the return type is larger than 64 bits.
            if constexpr (is64BitType<typename FunctionTraits<OperationType>::ResultType>::value)
                return appendCallWithExceptionCheck(operation);
            return appendCallWithExceptionCheckAndSlowPathReturnType(operation);
        }
#else // OS(WINDOWS) && CPU(X86_64)
        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperation(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheck(operation);
        }
#endif // OS(WINDOWS) && CPU(X86_64)

        template<typename OperationType, typename... Args>
        void callOperation(Address target, Args... args)
        {
#if OS(WINDOWS) && CPU(X86_64)
            // x64 Windows cannot use standard call when the return type is larger than 64 bits.
            static_assert(is64BitType<typename FunctionTraits<OperationType>::ResultType>::value);
#endif
            setupArgumentsForIndirectCall<OperationType>(target, args...);
            appendCallWithExceptionCheck(Address(GPRInfo::nonArgGPR0, target.offset));
        }

        template<typename Bytecode, typename OperationType, typename... Args>
        std::enable_if_t<FunctionTraits<OperationType>::hasResult, MacroAssembler::Call>
        callOperationWithProfile(const Bytecode& bytecode, OperationType operation, VirtualRegister result, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheckSetJSValueResultWithProfile(bytecode, operation, result);
        }

        template<typename OperationType, typename Bytecode, typename... Args>
        std::enable_if_t<FunctionTraits<OperationType>::hasResult, void>
        callOperationWithProfile(const Bytecode& bytecode, Address target, VirtualRegister result, Args... args)
        {
            setupArgumentsForIndirectCall<OperationType>(target, args...);
            return appendCallWithExceptionCheckSetJSValueResultWithProfile(bytecode, Address(GPRInfo::nonArgGPR0, target.offset), result);
        }

        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperationWithResult(OperationType operation, JSValueRegs resultRegs, Args... args)
        {
            setupArguments<OperationType>(args...);
            auto result = appendCallWithExceptionCheck(operation);
            setupResults(resultRegs);
            return result;
        }

#if OS(WINDOWS) && CPU(X86_64)
        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperationNoExceptionCheck(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            updateTopCallFrame();
            // x64 Windows cannot use standard call when the return type is larger than 64 bits.
            if constexpr (is64BitType<typename FunctionTraits<OperationType>::ResultType>::value)
                return appendCall(operation);
            return appendCallWithSlowPathReturnType(operation);
        }
#else // OS(WINDOWS) && CPU(X86_64)
        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperationNoExceptionCheck(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            updateTopCallFrame();
            return appendCall(operation);
        }
#endif // OS(WINDOWS) && CPU(X86_64)

        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperationWithCallFrameRollbackOnException(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithCallFrameRollbackOnException(operation);
        }

        enum class ProfilingPolicy {
            ShouldEmitProfiling,
            NoProfiling
        };

        template<typename Op, typename SnippetGenerator>
        void emitBitBinaryOpFastPath(const Instruction* currentInstruction, ProfilingPolicy shouldEmitProfiling = ProfilingPolicy::NoProfiling);

        void emitRightShiftFastPath(const Instruction* currentInstruction, OpcodeID);

        template<typename Op>
        void emitRightShiftFastPath(const Instruction* currentInstruction, JITRightShiftGenerator::ShiftType);

        void updateTopCallFrame();

        Call emitNakedNearCall(CodePtr<NoPtrTag> function = { });
        Call emitNakedNearTailCall(CodePtr<NoPtrTag> function = { });
        Jump emitNakedNearJump(CodePtr<JITThunkPtrTag> function = { });

        // Loads the character value of a single character string into dst.
        void emitLoadCharacterString(RegisterID src, RegisterID dst, JumpList& failures);

        int jumpTarget(const Instruction*, int target);
        
#if ENABLE(DFG_JIT)
        void emitEnterOptimizationCheck();
#else
        void emitEnterOptimizationCheck() { }
#endif

#ifndef NDEBUG
        void printBytecodeOperandTypes(VirtualRegister src1, VirtualRegister src2);
#endif

#if ENABLE(SAMPLING_FLAGS)
        void setSamplingFlag(int32_t);
        void clearSamplingFlag(int32_t);
#endif

#if ENABLE(SAMPLING_COUNTERS)
        void emitCount(AbstractSamplingCounter&, int32_t = 1);
#endif

#if ENABLE(DFG_JIT)
        bool canBeOptimized() { return m_canBeOptimized; }
        bool shouldEmitProfiling() { return m_shouldEmitProfiling; }
#else
        bool canBeOptimized() { return false; }
        // Enables use of value profiler with tiered compilation turned off,
        // in which case all code gets profiled.
        bool shouldEmitProfiling() { return false; }
#endif

        void emitMaterializeMetadataAndConstantPoolRegisters();

        void emitSaveCalleeSaves();
        void emitRestoreCalleeSaves();

        static bool reportCompileTimes();
        static bool computeCompileTimes();

        void resetSP();

        JITConstantPool::Constant addToConstantPool(JITConstantPool::Type, void* payload = nullptr);
        std::tuple<UnlinkedStructureStubInfo*, JITConstantPool::Constant> addUnlinkedStructureStubInfo();
        UnlinkedCallLinkInfo* addUnlinkedCallLinkInfo();

        Interpreter* m_interpreter;

        Vector<FarCallRecord> m_farCalls;
        Vector<NearCallRecord> m_nearCalls;
        Vector<NearJumpRecord> m_nearJumps;
        Vector<Label> m_labels;
        HashMap<BytecodeIndex, Label> m_checkpointLabels;
        HashMap<BytecodeIndex, Label> m_fastPathResumeLabels;
        Vector<JITGetByIdGenerator> m_getByIds;
        Vector<JITGetByValGenerator> m_getByVals;
        Vector<JITGetByIdWithThisGenerator> m_getByIdsWithThis;
        Vector<JITPutByIdGenerator> m_putByIds;
        Vector<JITPutByValGenerator> m_putByVals;
        Vector<JITInByIdGenerator> m_inByIds;
        Vector<JITInByValGenerator> m_inByVals;
        Vector<JITDelByIdGenerator> m_delByIds;
        Vector<JITDelByValGenerator> m_delByVals;
        Vector<JITInstanceOfGenerator> m_instanceOfs;
        Vector<JITPrivateBrandAccessGenerator> m_privateBrandAccesses;
        Vector<CallCompilationInfo> m_callCompilationInfo;
        Vector<JumpTable> m_jmpTable;

        BytecodeIndex m_bytecodeIndex;
        Vector<SlowCaseEntry> m_slowCases;
        Vector<SwitchRecord> m_switches;

        JumpList m_exceptionChecks;
        JumpList m_exceptionChecksWithCallFrameRollback;

        unsigned m_getByIdIndex { UINT_MAX };
        unsigned m_getByValIndex { UINT_MAX };
        unsigned m_getByIdWithThisIndex { UINT_MAX };
        unsigned m_putByIdIndex { UINT_MAX };
        unsigned m_putByValIndex { UINT_MAX };
        unsigned m_inByIdIndex { UINT_MAX };
        unsigned m_inByValIndex { UINT_MAX };
        unsigned m_delByValIndex { UINT_MAX };
        unsigned m_delByIdIndex { UINT_MAX };
        unsigned m_instanceOfIndex { UINT_MAX };
        unsigned m_privateBrandAccessIndex { UINT_MAX };
        unsigned m_callLinkInfoIndex { UINT_MAX };
        unsigned m_bytecodeCountHavingSlowCase { 0 };
        
        Label m_arityCheck;
        std::unique_ptr<LinkBuffer> m_linkBuffer;

        std::unique_ptr<JITDisassembler> m_disassembler;
        RefPtr<Profiler::Compilation> m_compilation;

        PCToCodeOriginMapBuilder m_pcToCodeOriginMapBuilder;
        std::unique_ptr<PCToCodeOriginMap> m_pcToCodeOriginMap;

        HashMap<const Instruction*, void*> m_instructionToMathIC;
        HashMap<const Instruction*, UniqueRef<MathICGenerationState>> m_instructionToMathICGenerationState;

        bool m_canBeOptimized;
        bool m_shouldEmitProfiling;
        BytecodeIndex m_loopOSREntryBytecodeIndex;

        CodeBlock* m_profiledCodeBlock { nullptr };
        UnlinkedCodeBlock* m_unlinkedCodeBlock { nullptr };

        MathICHolder m_mathICs;
        RefPtr<BaselineJITCode> m_jitCode;

        Vector<JITConstantPool::Value> m_constantPool;
        JITConstantPool::Constant m_globalObjectConstant { std::numeric_limits<unsigned>::max() };
        SegmentedVector<UnlinkedCallLinkInfo> m_unlinkedCalls;
        SegmentedVector<UnlinkedStructureStubInfo> m_unlinkedStubInfos;
        FixedVector<SimpleJumpTable> m_switchJumpTables;
        FixedVector<StringJumpTable> m_stringSwitchJumpTables;

        struct NotACodeBlock { } m_codeBlock;

        bool m_isShareable { true };
    };

} // namespace JSC


#endif // ENABLE(JIT)
