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

#include "CodeBlock.h"
#include "CommonSlowPaths.h"
#include "JITDisassembler.h"
#include "JITInlineCacheGenerator.h"
#include "JITMathIC.h"
#include "JITRightShiftGenerator.h"
#include "JSInterfaceJIT.h"
#include "PCToCodeOriginMap.h"
#include "UnusedPointer.h"

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
    struct OpPutToScope;

    struct CallRecord {
        MacroAssembler::Call from;
        BytecodeIndex bytecodeIndex;
        FunctionPtr<OperationPtrTag> callee;

        CallRecord()
        {
        }

        CallRecord(MacroAssembler::Call from, BytecodeIndex bytecodeIndex, FunctionPtr<OperationPtrTag> callee)
            : from(from)
            , bytecodeIndex(bytecodeIndex)
            , callee(callee)
        {
        }
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

        union {
            SimpleJumpTable* simpleJumpTable;
            StringJumpTable* stringJumpTable;
        } jumpTable;

        BytecodeIndex bytecodeIndex;
        unsigned defaultOffset;

        SwitchRecord(SimpleJumpTable* jumpTable, BytecodeIndex bytecodeIndex, unsigned defaultOffset, Type type)
            : type(type)
            , bytecodeIndex(bytecodeIndex)
            , defaultOffset(defaultOffset)
        {
            this->jumpTable.simpleJumpTable = jumpTable;
        }

        SwitchRecord(StringJumpTable* jumpTable, BytecodeIndex bytecodeIndex, unsigned defaultOffset)
            : type(String)
            , bytecodeIndex(bytecodeIndex)
            , defaultOffset(defaultOffset)
        {
            this->jumpTable.stringJumpTable = jumpTable;
        }
    };

    struct ByValCompilationInfo {
        ByValCompilationInfo() { }
        
        ByValCompilationInfo(ByValInfo* byValInfo, BytecodeIndex bytecodeIndex, MacroAssembler::PatchableJump notIndexJump, MacroAssembler::PatchableJump badTypeJump, JITArrayMode arrayMode, ArrayProfile* arrayProfile, MacroAssembler::Label doneTarget, MacroAssembler::Label nextHotPathTarget)
            : byValInfo(byValInfo)
            , bytecodeIndex(bytecodeIndex)
            , notIndexJump(notIndexJump)
            , badTypeJump(badTypeJump)
            , arrayMode(arrayMode)
            , arrayProfile(arrayProfile)
            , doneTarget(doneTarget)
            , nextHotPathTarget(nextHotPathTarget)
        {
        }

        ByValInfo* byValInfo;
        BytecodeIndex bytecodeIndex;
        MacroAssembler::PatchableJump notIndexJump;
        MacroAssembler::PatchableJump badTypeJump;
        JITArrayMode arrayMode;
        ArrayProfile* arrayProfile;
        MacroAssembler::Label doneTarget;
        MacroAssembler::Label nextHotPathTarget;
        MacroAssembler::Label slowPathTarget;
        MacroAssembler::Call returnAddress;
    };

    struct CallCompilationInfo {
        MacroAssembler::DataLabelPtr hotPathBegin;
        MacroAssembler::Call hotPathOther;
        MacroAssembler::Call callReturnLocation;
        CallLinkInfo* callLinkInfo;
    };

    void ctiPatchCallByReturnAddress(ReturnAddressPtr, FunctionPtr<CFunctionPtrTag> newCalleeFunction);

    class JIT_CLASS_ALIGNMENT JIT : private JSInterfaceJIT {
        friend class JITSlowPathCall;
        friend class JITStubCall;

        using MacroAssembler::Jump;
        using MacroAssembler::JumpList;
        using MacroAssembler::Label;

        static constexpr uintptr_t patchGetByIdDefaultStructure = unusedPointer;
        static constexpr int patchGetByIdDefaultOffset = 0;
        // Magic number - initial offset cannot be representable as a signed 8bit value, or the X86Assembler
        // will compress the displacement, and we may not be able to fit a patched offset.
        static constexpr int patchPutByIdDefaultOffset = 256;

    public:
        JIT(VM&, CodeBlock* = nullptr, BytecodeIndex loopOSREntryBytecodeOffset = BytecodeIndex(0));
        ~JIT();

        VM& vm() { return *JSInterfaceJIT::vm(); }

        void compileWithoutLinking(JITCompilationEffort);
        CompilationResult link();

        void doMainThreadPreparationBeforeCompile();
        
        static CompilationResult compile(VM& vm, CodeBlock* codeBlock, JITCompilationEffort effort, BytecodeIndex bytecodeOffset = BytecodeIndex(0))
        {
            return JIT(vm, codeBlock, bytecodeOffset).privateCompile(effort);
        }
        
        static void compilePutByVal(const ConcurrentJSLocker& locker, VM& vm, CodeBlock* codeBlock, ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
        {
            JIT jit(vm, codeBlock);
            jit.m_bytecodeIndex = byValInfo->bytecodeIndex;
            jit.privateCompilePutByVal<OpPutByVal>(locker, byValInfo, returnAddress, arrayMode);
        }
        
        static void compileDirectPutByVal(const ConcurrentJSLocker& locker, VM& vm, CodeBlock* codeBlock, ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
        {
            JIT jit(vm, codeBlock);
            jit.m_bytecodeIndex = byValInfo->bytecodeIndex;
            jit.privateCompilePutByVal<OpPutByValDirect>(locker, byValInfo, returnAddress, arrayMode);
        }

        template<typename Op>
        static void compilePutByValWithCachedId(VM& vm, CodeBlock* codeBlock, ByValInfo* byValInfo, ReturnAddressPtr returnAddress, PutKind putKind, const Identifier& propertyName)
        {
            JIT jit(vm, codeBlock);
            jit.m_bytecodeIndex = byValInfo->bytecodeIndex;
            jit.privateCompilePutByValWithCachedId<Op>(byValInfo, returnAddress, putKind, propertyName);
        }

        static void compileHasIndexedProperty(VM& vm, CodeBlock* codeBlock, ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
        {
            JIT jit(vm, codeBlock);
            jit.m_bytecodeIndex = byValInfo->bytecodeIndex;
            jit.privateCompileHasIndexedProperty(byValInfo, returnAddress, arrayMode);
        }

        static unsigned frameRegisterCountFor(CodeBlock*);
        static int stackPointerOffsetFor(CodeBlock*);

        JS_EXPORT_PRIVATE static HashMap<CString, Seconds> compileTimeStats();
        JS_EXPORT_PRIVATE static Seconds totalCompileTime();

    private:
        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        CompilationResult privateCompile(JITCompilationEffort);
        
        void privateCompileGetByVal(const ConcurrentJSLocker&, ByValInfo*, ReturnAddressPtr, JITArrayMode);
        void privateCompileGetByValWithCachedId(ByValInfo*, ReturnAddressPtr, const Identifier&);
        template<typename Op>
        void privateCompilePutByVal(const ConcurrentJSLocker&, ByValInfo*, ReturnAddressPtr, JITArrayMode);
        template<typename Op>
        void privateCompilePutByValWithCachedId(ByValInfo*, ReturnAddressPtr, PutKind, const Identifier&);

        void privateCompileHasIndexedProperty(ByValInfo*, ReturnAddressPtr, JITArrayMode);

        void privateCompilePatchGetArrayLength(ReturnAddressPtr returnAddress);

        // Add a call out from JIT code, without an exception check.
        Call appendCall(const FunctionPtr<CFunctionPtrTag> function)
        {
            Call functionCall = call(OperationPtrTag);
            m_calls.append(CallRecord(functionCall, m_bytecodeIndex, function.retagged<OperationPtrTag>()));
            return functionCall;
        }

#if OS(WINDOWS) && CPU(X86_64)
        Call appendCallWithSlowPathReturnType(const FunctionPtr<CFunctionPtrTag> function)
        {
            Call functionCall = callWithSlowPathReturnType(OperationPtrTag);
            m_calls.append(CallRecord(functionCall, m_bytecodeIndex, function.retagged<OperationPtrTag>()));
            return functionCall;
        }
#endif

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
        , void> compileSetupFrame(const Op&, CallLinkInfo*);

        template<typename Op>
        std::enable_if_t<
            Op::opcodeID == op_call_varargs || Op::opcodeID == op_construct_varargs
            || Op::opcodeID == op_tail_call_varargs || Op::opcodeID == op_tail_call_forward_arguments
        , void> compileSetupFrame(const Op&, CallLinkInfo*);

        template<typename Op>
        bool compileTailCall(const Op&, CallLinkInfo*, unsigned callLinkInfoIndex);
        template<typename Op>
        bool compileCallEval(const Op&);
        void compileCallEvalSlowCase(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        template<typename Op>
        void emitPutCallResult(const Op&);

        enum class CompileOpStrictEqType { StrictEq, NStrictEq };
        template<typename Op>
        void compileOpStrictEq(const Instruction*, CompileOpStrictEqType);
        template<typename Op>
        void compileOpStrictEqJump(const Instruction*, CompileOpStrictEqType);
        enum class CompileOpEqType { Eq, NEq };
        void compileOpEqJumpSlow(Vector<SlowCaseEntry>::iterator&, CompileOpEqType, int jumpTarget);
        bool isOperandConstantDouble(VirtualRegister);
        
        enum WriteBarrierMode { UnconditionalWriteBarrier, ShouldFilterBase, ShouldFilterValue, ShouldFilterBaseAndValue };
        // value register in write barrier is used before any scratch registers
        // so may safely be the same as either of the scratch registers.
        void emitWriteBarrier(VirtualRegister owner, VirtualRegister value, WriteBarrierMode);
        void emitWriteBarrier(JSCell* owner, VirtualRegister value, WriteBarrierMode);
        void emitWriteBarrier(JSCell* owner);

        // This assumes that the value to profile is in regT0 and that regT3 is available for
        // scratch.
        void emitValueProfilingSite(ValueProfile&);
        template<typename Metadata> void emitValueProfilingSite(Metadata&);
        void emitValueProfilingSiteIfProfiledOpcode(...);
        template<typename Op>
        std::enable_if_t<std::is_same<decltype(Op::Metadata::m_profile), ValueProfile>::value, void>
        emitValueProfilingSiteIfProfiledOpcode(Op bytecode);

        void emitArrayProfilingSiteWithCell(RegisterID cell, RegisterID indexingType, ArrayProfile*);
        void emitArrayProfileStoreToHoleSpecialCase(ArrayProfile*);
        void emitArrayProfileOutOfBoundsSpecialCase(ArrayProfile*);
        
        JITArrayMode chooseArrayMode(ArrayProfile*);
        
        // Property is in regT1, base is in regT0. regT2 contains indexing type.
        // Property is int-checked and zero extended. Base is cell checked.
        // Structure is already profiled. Returns the slow cases. Fall-through
        // case contains result in regT0, and it is not yet profiled.
        JumpList emitInt32Load(const Instruction* instruction, PatchableJump& badType) { return emitContiguousLoad(instruction, badType, Int32Shape); }
        JumpList emitDoubleLoad(const Instruction*, PatchableJump& badType);
        JumpList emitContiguousLoad(const Instruction*, PatchableJump& badType, IndexingType expectedShape = ContiguousShape);
        JumpList emitArrayStorageLoad(const Instruction*, PatchableJump& badType);
        JumpList emitLoadForArrayMode(const Instruction*, JITArrayMode, PatchableJump& badType);

        // Property is in regT1, base is in regT0. regT2 contains indecing type.
        // The value to store is not yet loaded. Property is int-checked and
        // zero-extended. Base is cell checked. Structure is already profiled.
        // returns the slow cases.
        template<typename Op>
        JumpList emitInt32PutByVal(Op bytecode, PatchableJump& badType)
        {
            return emitGenericContiguousPutByVal(bytecode, badType, Int32Shape);
        }
        template<typename Op>
        JumpList emitDoublePutByVal(Op bytecode, PatchableJump& badType)
        {
            return emitGenericContiguousPutByVal(bytecode, badType, DoubleShape);
        }
        template<typename Op>
        JumpList emitContiguousPutByVal(Op bytecode, PatchableJump& badType)
        {
            return emitGenericContiguousPutByVal(bytecode, badType);
        }
        template<typename Op>
        JumpList emitGenericContiguousPutByVal(Op, PatchableJump& badType, IndexingType indexingShape = ContiguousShape);
        template<typename Op>
        JumpList emitArrayStoragePutByVal(Op, PatchableJump& badType);
        template<typename Op>
        JumpList emitIntTypedArrayPutByVal(Op, PatchableJump& badType, TypedArrayType);
        template<typename Op>
        JumpList emitFloatTypedArrayPutByVal(Op, PatchableJump& badType, TypedArrayType);

        // Identifier check helper for GetByVal and PutByVal.
        void emitByValIdentifierCheck(ByValInfo*, RegisterID cell, RegisterID scratch, const Identifier&, JumpList& slowCases);

        template<typename Op>
        JITPutByIdGenerator emitPutByValWithCachedId(ByValInfo*, Op, PutKind, const Identifier&, JumpList& doneCases, JumpList& slowCases);

        enum FinalObjectMode { MayBeFinal, KnownNotFinal };

        void emitGetVirtualRegister(VirtualRegister src, JSValueRegs dst);
        void emitPutVirtualRegister(VirtualRegister dst, JSValueRegs src);

        int32_t getOperandConstantInt(VirtualRegister src);
        double getOperandConstantDouble(VirtualRegister src);

#if USE(JSVALUE32_64)
        bool getOperandConstantInt(VirtualRegister op1, VirtualRegister op2, VirtualRegister& op, int32_t& constant);

        void emitLoadDouble(VirtualRegister, FPRegisterID value);
        void emitLoadTag(VirtualRegister, RegisterID tag);
        void emitLoadPayload(VirtualRegister, RegisterID payload);

        void emitLoad(const JSValue& v, RegisterID tag, RegisterID payload);
        void emitLoad(VirtualRegister, RegisterID tag, RegisterID payload, RegisterID base = callFrameRegister);
        void emitLoad2(VirtualRegister, RegisterID tag1, RegisterID payload1, VirtualRegister, RegisterID tag2, RegisterID payload2);

        void emitStore(VirtualRegister, RegisterID tag, RegisterID payload, RegisterID base = callFrameRegister);
        void emitStore(VirtualRegister, const JSValue constant, RegisterID base = callFrameRegister);
        void emitStoreInt32(VirtualRegister, RegisterID payload, bool indexIsInt32 = false);
        void emitStoreInt32(VirtualRegister, TrustedImm32 payload, bool indexIsInt32 = false);
        void emitStoreCell(VirtualRegister, RegisterID payload, bool indexIsCell = false);
        void emitStoreBool(VirtualRegister, RegisterID payload, bool indexIsBool = false);
        void emitStoreDouble(VirtualRegister, FPRegisterID value);

        void emitJumpSlowCaseIfNotJSCell(VirtualRegister);
        void emitJumpSlowCaseIfNotJSCell(VirtualRegister, RegisterID tag);

        void compileGetByIdHotPath(const Identifier*);

        // Arithmetic opcode helpers
        template <typename Op>
        void emitBinaryDoubleOp(const Instruction *, OperandTypes, JumpList& notInt32Op1, JumpList& notInt32Op2, bool op1IsInRegisters = true, bool op2IsInRegisters = true);

#else // USE(JSVALUE32_64)
        void emitGetVirtualRegister(VirtualRegister src, RegisterID dst);
        void emitGetVirtualRegisters(VirtualRegister src1, RegisterID dst1, VirtualRegister src2, RegisterID dst2);
        void emitPutVirtualRegister(VirtualRegister dst, RegisterID from = regT0);
        void emitStoreCell(VirtualRegister dst, RegisterID payload, bool /* only used in JSValue32_64 */ = false)
        {
            emitPutVirtualRegister(dst, payload);
        }

        Jump emitJumpIfBothJSCells(RegisterID, RegisterID, RegisterID);
        void emitJumpSlowCaseIfJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID, VirtualRegister);
        Jump emitJumpIfNotInt(RegisterID, RegisterID, RegisterID scratch);
        PatchableJump emitPatchableJumpIfNotInt(RegisterID);
        void emitJumpSlowCaseIfNotInt(RegisterID);
        void emitJumpSlowCaseIfNotNumber(RegisterID);
        void emitJumpSlowCaseIfNotInt(RegisterID, RegisterID, RegisterID scratch);

        void compileGetByIdHotPath(VirtualRegister baseReg, const Identifier*);

#endif // USE(JSVALUE32_64)

        template<typename Op>
        void emit_compareAndJump(const Instruction*, RelationalCondition);
        void emit_compareAndJumpImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, RelationalCondition);
        template<typename Op>
        void emit_compareUnsigned(const Instruction*, RelationalCondition);
        void emit_compareUnsignedImpl(VirtualRegister dst, VirtualRegister op1, VirtualRegister op2, RelationalCondition);
        template<typename Op>
        void emit_compareUnsignedAndJump(const Instruction*, RelationalCondition);
        void emit_compareUnsignedAndJumpImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, RelationalCondition);
        template<typename Op>
        void emit_compareAndJumpSlow(const Instruction*, DoubleCondition, size_t (JIT_OPERATION *operation)(JSGlobalObject*, EncodedJSValue, EncodedJSValue), bool invert, Vector<SlowCaseEntry>::iterator&);
        void emit_compareAndJumpSlowImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, size_t instructionSize, DoubleCondition, size_t (JIT_OPERATION *operation)(JSGlobalObject*, EncodedJSValue, EncodedJSValue), bool invert, Vector<SlowCaseEntry>::iterator&);
        
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
        void emit_op_del_by_val(const Instruction*);
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
        void emit_op_get_argument_by_val(const Instruction*);
        void emit_op_in_by_id(const Instruction*);
        void emit_op_init_lazy_reg(const Instruction*);
        void emit_op_overrides_has_instance(const Instruction*);
        void emit_op_instanceof(const Instruction*);
        void emit_op_instanceof_custom(const Instruction*);
        void emit_op_is_empty(const Instruction*);
        void emit_op_is_undefined(const Instruction*);
        void emit_op_is_undefined_or_null(const Instruction*);
        void emit_op_is_boolean(const Instruction*);
        void emit_op_is_number(const Instruction*);
        void emit_op_is_object(const Instruction*);
        void emit_op_is_cell_with_type(const Instruction*);
        void emit_op_jeq_null(const Instruction*);
        void emit_op_jfalse(const Instruction*);
        void emit_op_jmp(const Instruction*);
        void emit_op_jneq_null(const Instruction*);
        void emit_op_jundefined_or_null(const Instruction*);
        void emit_op_jnundefined_or_null(const Instruction*);
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
        void emit_op_has_structure_property(const Instruction*);
        void emit_op_has_indexed_property(const Instruction*);
        void emit_op_get_direct_pname(const Instruction*);
        void emit_op_enumerator_structure_pname(const Instruction*);
        void emit_op_enumerator_generic_pname(const Instruction*);
        void emit_op_get_internal_field(const Instruction*);
        void emit_op_put_internal_field(const Instruction*);
        void emit_op_log_shadow_chicken_prologue(const Instruction*);
        void emit_op_log_shadow_chicken_tail(const Instruction*);
        void emit_op_to_property_key(const Instruction*);

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
        void emitSlow_op_get_argument_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_in_by_id(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_instanceof(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_instanceof_custom(const Instruction*, Vector<SlowCaseEntry>::iterator&);
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
        void emitSlow_op_jtrue(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_hint(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_check_traps(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mod(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mul(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_negate(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_neq(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_new_object(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_id(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_val(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_sub(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_has_indexed_property(const Instruction*, Vector<SlowCaseEntry>::iterator&);

        void emit_op_resolve_scope(const Instruction*);
        void emit_op_get_from_scope(const Instruction*);
        void emit_op_put_to_scope(const Instruction*);
        void emit_op_get_from_arguments(const Instruction*);
        void emit_op_put_to_arguments(const Instruction*);
        void emitSlow_op_get_from_scope(const Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_to_scope(const Instruction*, Vector<SlowCaseEntry>::iterator&);

        void emitSlowCaseCall(const Instruction*, Vector<SlowCaseEntry>::iterator&, SlowPathFunction);

        void emitRightShift(const Instruction*, bool isUnsigned);
        void emitRightShiftSlowCase(const Instruction*, Vector<SlowCaseEntry>::iterator&, bool isUnsigned);

        template<typename Op>
        void emitNewFuncCommon(const Instruction*);
        template<typename Op>
        void emitNewFuncExprCommon(const Instruction*);
        void emitVarInjectionCheck(bool needsVarInjectionChecks);
        void emitResolveClosure(VirtualRegister dst, VirtualRegister scope, bool needsVarInjectionChecks, unsigned depth);
        void emitLoadWithStructureCheck(VirtualRegister scope, Structure** structureSlot);
#if USE(JSVALUE64)
        void emitGetVarFromPointer(JSValue* operand, GPRReg);
        void emitGetVarFromIndirectPointer(JSValue** operand, GPRReg);
#else
        void emitGetVarFromIndirectPointer(JSValue** operand, GPRReg tag, GPRReg payload);
        void emitGetVarFromPointer(JSValue* operand, GPRReg tag, GPRReg payload);
#endif
        void emitGetClosureVar(VirtualRegister scope, uintptr_t operand);
        void emitNotifyWrite(WatchpointSet*);
        void emitNotifyWrite(GPRReg pointerToSet);
        void emitPutGlobalVariable(JSValue* operand, VirtualRegister value, WatchpointSet*);
        void emitPutGlobalVariableIndirect(JSValue** addressOfOperand, VirtualRegister value, WatchpointSet**);
        void emitPutClosureVar(VirtualRegister scope, uintptr_t operand, VirtualRegister value, WatchpointSet*);

        void emitInitRegister(VirtualRegister);

        void emitPutIntToCallFrameHeader(RegisterID from, VirtualRegister);

        JSValue getConstantOperand(VirtualRegister);
        bool isOperandConstantInt(VirtualRegister);
        bool isOperandConstantChar(VirtualRegister);

        template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
        void emitMathICFast(JITUnaryMathIC<Generator>*, const Instruction*, ProfiledFunction, NonProfiledFunction);
        template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
        void emitMathICFast(JITBinaryMathIC<Generator>*, const Instruction*, ProfiledFunction, NonProfiledFunction);

        template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
        void emitMathICSlow(JITBinaryMathIC<Generator>*, const Instruction*, ProfiledRepatchFunction, ProfiledFunction, RepatchFunction);
        template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
        void emitMathICSlow(JITUnaryMathIC<Generator>*, const Instruction*, ProfiledRepatchFunction, ProfiledFunction, RepatchFunction);

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
        void linkDummySlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            ASSERT(!iter->from.isSet());
            ++iter;
        }
        void linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator&, VirtualRegister);
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
#if OS(WINDOWS) && CPU(X86_64)
        MacroAssembler::Call appendCallWithExceptionCheckAndSlowPathReturnType(const FunctionPtr<CFunctionPtrTag>);
#endif
        MacroAssembler::Call appendCallWithCallFrameRollbackOnException(const FunctionPtr<CFunctionPtrTag>);
        MacroAssembler::Call appendCallWithExceptionCheckSetJSValueResult(const FunctionPtr<CFunctionPtrTag>, VirtualRegister result);
        template<typename Metadata>
        MacroAssembler::Call appendCallWithExceptionCheckSetJSValueResultWithProfile(Metadata&, const FunctionPtr<CFunctionPtrTag>, VirtualRegister result);
        
        template<typename OperationType, typename... Args>
        std::enable_if_t<FunctionTraits<OperationType>::hasResult, MacroAssembler::Call>
        callOperation(OperationType operation, VirtualRegister result, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheckSetJSValueResult(operation, result);
        }

#if OS(WINDOWS) && CPU(X86_64)
        template<typename OperationType, typename... Args>
        std::enable_if_t<std::is_same<typename FunctionTraits<OperationType>::ResultType, SlowPathReturnType>::value, MacroAssembler::Call>
        callOperation(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheckAndSlowPathReturnType(operation);
        }

        template<typename Type>
        struct is64BitType {
            static constexpr bool value = sizeof(Type) <= 8;
        };

        template<>
        struct is64BitType<void> {
            static constexpr bool value = true;
        };

        template<typename OperationType, typename... Args>
        std::enable_if_t<!std::is_same<typename FunctionTraits<OperationType>::ResultType, SlowPathReturnType>::value, MacroAssembler::Call>
        callOperation(OperationType operation, Args... args)
        {
            static_assert(is64BitType<typename FunctionTraits<OperationType>::ResultType>::value, "Win64 cannot use standard call when return type is larger than 64 bits.");
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheck(operation);
        }
#else // OS(WINDOWS) && CPU(X86_64)
        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperation(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheck(operation);
        }
#endif // OS(WINDOWS) && CPU(X86_64)

        template<typename Metadata, typename OperationType, typename... Args>
        std::enable_if_t<FunctionTraits<OperationType>::hasResult, MacroAssembler::Call>
        callOperationWithProfile(Metadata& metadata, OperationType operation, VirtualRegister result, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheckSetJSValueResultWithProfile(metadata, operation, result);
        }

        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperationWithResult(OperationType operation, JSValueRegs resultRegs, Args... args)
        {
            setupArguments<OperationType>(args...);
            auto result = appendCallWithExceptionCheck(operation);
            setupResults(resultRegs);
            return result;
        }

        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperationNoExceptionCheck(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            updateTopCallFrame();
            return appendCall(operation);
        }

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

        Call emitNakedCall(CodePtr<NoPtrTag> function = CodePtr<NoPtrTag>());
        Call emitNakedTailCall(CodePtr<NoPtrTag> function = CodePtr<NoPtrTag>());

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

#if ENABLE(OPCODE_SAMPLING)
        void sampleInstruction(const Instruction*, bool = false);
#endif

#if ENABLE(CODEBLOCK_SAMPLING)
        void sampleCodeBlock(CodeBlock*);
#else
        void sampleCodeBlock(CodeBlock*) {}
#endif

#if ENABLE(DFG_JIT)
        bool canBeOptimized() { return m_canBeOptimized; }
        bool canBeOptimizedOrInlined() { return m_canBeOptimizedOrInlined; }
        bool shouldEmitProfiling() { return m_shouldEmitProfiling; }
#else
        bool canBeOptimized() { return false; }
        bool canBeOptimizedOrInlined() { return false; }
        // Enables use of value profiler with tiered compilation turned off,
        // in which case all code gets profiled.
        bool shouldEmitProfiling() { return false; }
#endif

        static bool reportCompileTimes();
        static bool computeCompileTimes();
        
        // If you need to check a value from the metadata table and you need it to
        // be consistent across the fast and slow path, then you want to use this.
        // It will give the slow path the same value read by the fast path.
        GetPutInfo copiedGetPutInfo(OpPutToScope);
        template<typename BinaryOp>
        BinaryArithProfile copiedArithProfile(BinaryOp);

        Interpreter* m_interpreter;

        Vector<CallRecord> m_calls;
        Vector<Label> m_labels;
        Vector<JITGetByIdGenerator> m_getByIds;
        Vector<JITGetByValGenerator> m_getByVals;
        Vector<JITGetByIdWithThisGenerator> m_getByIdsWithThis;
        Vector<JITPutByIdGenerator> m_putByIds;
        Vector<JITInByIdGenerator> m_inByIds;
        Vector<JITInstanceOfGenerator> m_instanceOfs;
        Vector<ByValCompilationInfo> m_byValCompilationInfo;
        Vector<CallCompilationInfo> m_callCompilationInfo;
        Vector<JumpTable> m_jmpTable;

        BytecodeIndex m_bytecodeIndex;
        Vector<SlowCaseEntry> m_slowCases;
        Vector<SwitchRecord> m_switches;

        HashMap<unsigned, unsigned> m_copiedGetPutInfos;
        HashMap<uint64_t, BinaryArithProfile> m_copiedArithProfiles;

        JumpList m_exceptionChecks;
        JumpList m_exceptionChecksWithCallFrameRollback;
        Label m_exceptionHandler;

        unsigned m_getByIdIndex { UINT_MAX };
        unsigned m_getByValIndex { UINT_MAX };
        unsigned m_getByIdWithThisIndex { UINT_MAX };
        unsigned m_putByIdIndex { UINT_MAX };
        unsigned m_inByIdIndex { UINT_MAX };
        unsigned m_instanceOfIndex { UINT_MAX };
        unsigned m_byValInstructionIndex { UINT_MAX };
        unsigned m_callLinkInfoIndex { UINT_MAX };
        unsigned m_bytecodeCountHavingSlowCase { 0 };
        
        Label m_arityCheck;
        std::unique_ptr<LinkBuffer> m_linkBuffer;

        std::unique_ptr<JITDisassembler> m_disassembler;
        RefPtr<Profiler::Compilation> m_compilation;

        PCToCodeOriginMapBuilder m_pcToCodeOriginMapBuilder;

        HashMap<const Instruction*, void*> m_instructionToMathIC;
        HashMap<const Instruction*, MathICGenerationState> m_instructionToMathICGenerationState;

        bool m_canBeOptimized;
        bool m_canBeOptimizedOrInlined;
        bool m_shouldEmitProfiling;
        BytecodeIndex m_loopOSREntryBytecodeIndex;
    };

} // namespace JSC


#endif // ENABLE(JIT)
