/*
 * Copyright (C) 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef JIT_h
#define JIT_h

#if ENABLE(JIT)

// We've run into some problems where changing the size of the class JIT leads to
// performance fluctuations.  Try forcing alignment in an attempt to stabalize this.
#if COMPILER(GCC)
#define JIT_CLASS_ALIGNMENT __attribute__ ((aligned (32)))
#else
#define JIT_CLASS_ALIGNMENT
#endif

#define ASSERT_JIT_OFFSET(actual, expected) ASSERT_WITH_MESSAGE(actual == expected, "JIT Offset \"%s\" should be %d, not %d.\n", #expected, static_cast<int>(expected), static_cast<int>(actual));

#include "CodeBlock.h"
#include "CompactJITCodeMap.h"
#include "Interpreter.h"
#include "JITDisassembler.h"
#include "JITInlineCacheGenerator.h"
#include "JSInterfaceJIT.h"
#include "LegacyProfiler.h"
#include "Opcode.h"
#include "ResultType.h"
#include "SamplingTool.h"
#include "UnusedPointer.h"

namespace JSC {

    class ArrayAllocationProfile;
    class CodeBlock;
    class FunctionExecutable;
    class JIT;
    class JSPropertyNameIterator;
    class Identifier;
    class Interpreter;
    class JSScope;
    class JSStack;
    class MarkedAllocator;
    class Register;
    class StructureChain;

    struct CallLinkInfo;
    struct Instruction;
    struct OperandTypes;
    struct PolymorphicAccessStructureList;
    struct SimpleJumpTable;
    struct StringJumpTable;
    struct StructureStubInfo;

    struct CallRecord {
        MacroAssembler::Call from;
        unsigned bytecodeOffset;
        void* to;

        CallRecord()
        {
        }

        CallRecord(MacroAssembler::Call from, unsigned bytecodeOffset, void* to = 0)
            : from(from)
            , bytecodeOffset(bytecodeOffset)
            , to(to)
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
        unsigned to;
        unsigned hint;
        
        SlowCaseEntry(MacroAssembler::Jump f, unsigned t, unsigned h = 0)
            : from(f)
            , to(t)
            , hint(h)
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

        unsigned bytecodeOffset;
        unsigned defaultOffset;

        SwitchRecord(SimpleJumpTable* jumpTable, unsigned bytecodeOffset, unsigned defaultOffset, Type type)
            : type(type)
            , bytecodeOffset(bytecodeOffset)
            , defaultOffset(defaultOffset)
        {
            this->jumpTable.simpleJumpTable = jumpTable;
        }

        SwitchRecord(StringJumpTable* jumpTable, unsigned bytecodeOffset, unsigned defaultOffset)
            : type(String)
            , bytecodeOffset(bytecodeOffset)
            , defaultOffset(defaultOffset)
        {
            this->jumpTable.stringJumpTable = jumpTable;
        }
    };

    struct ByValCompilationInfo {
        ByValCompilationInfo() { }
        
        ByValCompilationInfo(unsigned bytecodeIndex, MacroAssembler::PatchableJump badTypeJump, JITArrayMode arrayMode, MacroAssembler::Label doneTarget)
            : bytecodeIndex(bytecodeIndex)
            , badTypeJump(badTypeJump)
            , arrayMode(arrayMode)
            , doneTarget(doneTarget)
        {
        }
        
        unsigned bytecodeIndex;
        MacroAssembler::PatchableJump badTypeJump;
        JITArrayMode arrayMode;
        MacroAssembler::Label doneTarget;
        MacroAssembler::Label slowPathTarget;
        MacroAssembler::Call returnAddress;
    };

    struct StructureStubCompilationInfo {
        MacroAssembler::DataLabelPtr hotPathBegin;
        MacroAssembler::Call hotPathOther;
        MacroAssembler::Call callReturnLocation;
        CallLinkInfo::CallType callType;
        unsigned bytecodeIndex;
    };

    // Near calls can only be patched to other JIT code, regular calls can be patched to JIT code or relinked to stub functions.
    void ctiPatchNearCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, MacroAssemblerCodePtr newCalleeFunction);
    void ctiPatchCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, MacroAssemblerCodePtr newCalleeFunction);
    void ctiPatchCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, FunctionPtr newCalleeFunction);

    class JIT : private JSInterfaceJIT {
        friend class JITSlowPathCall;
        friend class JITStubCall;

        using MacroAssembler::Jump;
        using MacroAssembler::JumpList;
        using MacroAssembler::Label;

        static const uintptr_t patchGetByIdDefaultStructure = unusedPointer;
        static const int patchGetByIdDefaultOffset = 0;
        // Magic number - initial offset cannot be representable as a signed 8bit value, or the X86Assembler
        // will compress the displacement, and we may not be able to fit a patched offset.
        static const int patchPutByIdDefaultOffset = 256;

    public:
        static CompilationResult compile(VM* vm, CodeBlock* codeBlock, JITCompilationEffort effort)
        {
            return JIT(vm, codeBlock).privateCompile(effort);
        }
        
        static void compileClosureCall(VM* vm, CallLinkInfo* callLinkInfo, CodeBlock* callerCodeBlock, CodeBlock* calleeCodeBlock, Structure* expectedStructure, ExecutableBase* expectedExecutable, MacroAssemblerCodePtr codePtr)
        {
            JIT jit(vm, callerCodeBlock);
            jit.m_bytecodeOffset = callLinkInfo->codeOrigin.bytecodeIndex;
            jit.privateCompileClosureCall(callLinkInfo, calleeCodeBlock, expectedStructure, expectedExecutable, codePtr);
        }

        static void compileGetByVal(VM* vm, CodeBlock* codeBlock, ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
        {
            JIT jit(vm, codeBlock);
            jit.m_bytecodeOffset = byValInfo->bytecodeIndex;
            jit.privateCompileGetByVal(byValInfo, returnAddress, arrayMode);
        }

        static void compilePutByVal(VM* vm, CodeBlock* codeBlock, ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
        {
            JIT jit(vm, codeBlock);
            jit.m_bytecodeOffset = byValInfo->bytecodeIndex;
            jit.privateCompilePutByVal(byValInfo, returnAddress, arrayMode);
        }
        
        static void compileDirectPutByVal(VM* vm, CodeBlock* codeBlock, ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
        {
            JIT jit(vm, codeBlock);
            jit.m_bytecodeOffset = byValInfo->bytecodeIndex;
            jit.privateCompilePutByVal(byValInfo, returnAddress, arrayMode);
        }

        static CodeRef compileCTINativeCall(VM* vm, NativeFunction func)
        {
            if (!vm->canUseJIT()) {
#if ENABLE(LLINT)
                return CodeRef::createLLIntCodeRef(llint_native_call_trampoline);
#else
                return CodeRef();
#endif
            }
            JIT jit(vm, 0);
            return jit.privateCompileCTINativeCall(vm, func);
        }

        static unsigned frameRegisterCountFor(CodeBlock*);
        static int stackPointerOffsetFor(CodeBlock*);

    private:
        JIT(VM*, CodeBlock* = 0);

        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        CompilationResult privateCompile(JITCompilationEffort);
        
        void privateCompileClosureCall(CallLinkInfo*, CodeBlock* calleeCodeBlock, Structure*, ExecutableBase*, MacroAssemblerCodePtr);
        
        void privateCompileGetByVal(ByValInfo*, ReturnAddressPtr, JITArrayMode);
        void privateCompilePutByVal(ByValInfo*, ReturnAddressPtr, JITArrayMode);

        Label privateCompileCTINativeCall(VM*, bool isConstruct = false);
        CodeRef privateCompileCTINativeCall(VM*, NativeFunction);
        void privateCompilePatchGetArrayLength(ReturnAddressPtr returnAddress);

        // Add a call out from JIT code, without an exception check.
        Call appendCall(const FunctionPtr& function)
        {
            Call functionCall = call();
            m_calls.append(CallRecord(functionCall, m_bytecodeOffset, function.value()));
            return functionCall;
        }

        void exceptionCheck(Jump jumpToHandler)
        {
            m_exceptionChecks.append(jumpToHandler);
        }

        void exceptionCheck()
        {
            m_exceptionChecks.append(emitExceptionCheck());
        }

        void exceptionCheckWithCallFrameRollback()
        {
            m_exceptionChecksWithCallFrameRollback.append(emitExceptionCheck());
        }

        void privateCompileExceptionHandlers();

        void addSlowCase(Jump);
        void addSlowCase(JumpList);
        void addSlowCase();
        void addJump(Jump, int);
        void emitJumpSlowToHot(Jump, int);

        void compileOpCall(OpcodeID, Instruction*, unsigned callLinkInfoIndex);
        void compileOpCallSlowCase(OpcodeID, Instruction*, Vector<SlowCaseEntry>::iterator&, unsigned callLinkInfoIndex);
        void compileLoadVarargs(Instruction*);
        void compileCallEval(Instruction*);
        void compileCallEvalSlowCase(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitPutCallResult(Instruction*);

        enum CompileOpStrictEqType { OpStrictEq, OpNStrictEq };
        void compileOpStrictEq(Instruction* instruction, CompileOpStrictEqType type);
        bool isOperandConstantImmediateDouble(int src);
        
        void emitLoadDouble(int index, FPRegisterID value);
        void emitLoadInt32ToDouble(int index, FPRegisterID value);
        Jump emitJumpIfCellNotObject(RegisterID cellReg);

        Jump addStructureTransitionCheck(JSCell*, Structure*, StructureStubInfo*, RegisterID scratch);
        void addStructureTransitionCheck(JSCell*, Structure*, StructureStubInfo*, JumpList& failureCases, RegisterID scratch);
        void testPrototype(JSValue, JumpList& failureCases, StructureStubInfo*);

        enum WriteBarrierMode { UnconditionalWriteBarrier, ShouldFilterValue, ShouldFilterBaseAndValue };
        // value register in write barrier is used before any scratch registers
        // so may safely be the same as either of the scratch registers.
        Jump checkMarkWord(RegisterID owner);
        Jump checkMarkWord(JSCell* owner);
        void emitWriteBarrier(unsigned owner, unsigned value, WriteBarrierMode);
        void emitWriteBarrier(JSCell* owner, unsigned value, WriteBarrierMode);
        void emitWriteBarrier(JSCell* owner);

        template<typename StructureType> // StructureType can be RegisterID or ImmPtr.
        void emitAllocateJSObject(RegisterID allocator, StructureType, RegisterID result, RegisterID scratch);
        
        // This assumes that the value to profile is in regT0 and that regT3 is available for
        // scratch.
        void emitValueProfilingSite(ValueProfile*);
        void emitValueProfilingSite(unsigned bytecodeOffset);
        void emitValueProfilingSite();
        void emitArrayProfilingSiteWithCell(RegisterID cell, RegisterID indexingType, ArrayProfile*);
        void emitArrayProfilingSiteForBytecodeIndexWithCell(RegisterID cell, RegisterID indexingType, unsigned bytecodeIndex);
        void emitArrayProfileStoreToHoleSpecialCase(ArrayProfile*);
        void emitArrayProfileOutOfBoundsSpecialCase(ArrayProfile*);
        
        JITArrayMode chooseArrayMode(ArrayProfile*);
        
        // Property is in regT1, base is in regT0. regT2 contains indexing type.
        // Property is int-checked and zero extended. Base is cell checked.
        // Structure is already profiled. Returns the slow cases. Fall-through
        // case contains result in regT0, and it is not yet profiled.
        JumpList emitInt32GetByVal(Instruction* instruction, PatchableJump& badType) { return emitContiguousGetByVal(instruction, badType, Int32Shape); }
        JumpList emitDoubleGetByVal(Instruction*, PatchableJump& badType);
        JumpList emitContiguousGetByVal(Instruction*, PatchableJump& badType, IndexingType expectedShape = ContiguousShape);
        JumpList emitArrayStorageGetByVal(Instruction*, PatchableJump& badType);
        JumpList emitIntTypedArrayGetByVal(Instruction*, PatchableJump& badType, TypedArrayType);
        JumpList emitFloatTypedArrayGetByVal(Instruction*, PatchableJump& badType, TypedArrayType);
        
        // Property is in regT0, base is in regT0. regT2 contains indecing type.
        // The value to store is not yet loaded. Property is int-checked and
        // zero-extended. Base is cell checked. Structure is already profiled.
        // returns the slow cases.
        JumpList emitInt32PutByVal(Instruction* currentInstruction, PatchableJump& badType)
        {
            return emitGenericContiguousPutByVal(currentInstruction, badType, Int32Shape);
        }
        JumpList emitDoublePutByVal(Instruction* currentInstruction, PatchableJump& badType)
        {
            return emitGenericContiguousPutByVal(currentInstruction, badType, DoubleShape);
        }
        JumpList emitContiguousPutByVal(Instruction* currentInstruction, PatchableJump& badType)
        {
            return emitGenericContiguousPutByVal(currentInstruction, badType);
        }
        JumpList emitGenericContiguousPutByVal(Instruction*, PatchableJump& badType, IndexingType indexingShape = ContiguousShape);
        JumpList emitArrayStoragePutByVal(Instruction*, PatchableJump& badType);
        JumpList emitIntTypedArrayPutByVal(Instruction*, PatchableJump& badType, TypedArrayType);
        JumpList emitFloatTypedArrayPutByVal(Instruction*, PatchableJump& badType, TypedArrayType);
        
        enum FinalObjectMode { MayBeFinal, KnownNotFinal };

        template <typename T> Jump branchStructure(RelationalCondition, T leftHandSide, Structure*);

#if USE(JSVALUE32_64)
        bool getOperandConstantImmediateInt(int op1, int op2, int& op, int32_t& constant);

        void emitLoadTag(int index, RegisterID tag);
        void emitLoadPayload(int index, RegisterID payload);

        void emitLoad(const JSValue& v, RegisterID tag, RegisterID payload);
        void emitLoad(int index, RegisterID tag, RegisterID payload, RegisterID base = callFrameRegister);
        void emitLoad2(int index1, RegisterID tag1, RegisterID payload1, int index2, RegisterID tag2, RegisterID payload2);

        void emitStore(int index, RegisterID tag, RegisterID payload, RegisterID base = callFrameRegister);
        void emitStore(int index, const JSValue constant, RegisterID base = callFrameRegister);
        void emitStoreInt32(int index, RegisterID payload, bool indexIsInt32 = false);
        void emitStoreInt32(int index, TrustedImm32 payload, bool indexIsInt32 = false);
        void emitStoreCell(int index, RegisterID payload, bool indexIsCell = false);
        void emitStoreBool(int index, RegisterID payload, bool indexIsBool = false);
        void emitStoreDouble(int index, FPRegisterID value);

        void emitJumpSlowCaseIfNotJSCell(int virtualRegisterIndex);
        void emitJumpSlowCaseIfNotJSCell(int virtualRegisterIndex, RegisterID tag);

        void compileGetByIdHotPath(const Identifier*);
        void compileGetDirectOffset(RegisterID base, RegisterID resultTag, RegisterID resultPayload, PropertyOffset cachedOffset);
        void compileGetDirectOffset(JSObject* base, RegisterID resultTag, RegisterID resultPayload, PropertyOffset cachedOffset);
        void compileGetDirectOffset(RegisterID base, RegisterID resultTag, RegisterID resultPayload, RegisterID offset, FinalObjectMode = MayBeFinal);
        void compilePutDirectOffset(RegisterID base, RegisterID valueTag, RegisterID valuePayload, PropertyOffset cachedOffset);

        // Arithmetic opcode helpers
        void emitAdd32Constant(int dst, int op, int32_t constant, ResultType opType);
        void emitSub32Constant(int dst, int op, int32_t constant, ResultType opType);
        void emitBinaryDoubleOp(OpcodeID, int dst, int op1, int op2, OperandTypes, JumpList& notInt32Op1, JumpList& notInt32Op2, bool op1IsInRegisters = true, bool op2IsInRegisters = true);

#else // USE(JSVALUE32_64)
        void emitGetVirtualRegister(int src, RegisterID dst);
        void emitGetVirtualRegister(VirtualRegister src, RegisterID dst);
        void emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2);
        void emitGetVirtualRegisters(VirtualRegister src1, RegisterID dst1, VirtualRegister src2, RegisterID dst2);
        void emitPutVirtualRegister(int dst, RegisterID from = regT0);
        void emitPutVirtualRegister(VirtualRegister dst, RegisterID from = regT0);
        void emitStoreCell(int dst, RegisterID payload, bool /* only used in JSValue32_64 */ = false)
        {
            emitPutVirtualRegister(dst, payload);
        }
        void emitStoreCell(VirtualRegister dst, RegisterID payload)
        {
            emitPutVirtualRegister(dst, payload);
        }

        int32_t getConstantOperandImmediateInt(int src);

        Jump emitJumpIfJSCell(RegisterID);
        Jump emitJumpIfBothJSCells(RegisterID, RegisterID, RegisterID);
        void emitJumpSlowCaseIfJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID, int VReg);
        Jump emitJumpIfImmediateInteger(RegisterID);
        Jump emitJumpIfNotImmediateInteger(RegisterID);
        Jump emitJumpIfNotImmediateIntegers(RegisterID, RegisterID, RegisterID);
        void emitJumpSlowCaseIfNotImmediateInteger(RegisterID);
        void emitJumpSlowCaseIfNotImmediateNumber(RegisterID);
        void emitJumpSlowCaseIfNotImmediateIntegers(RegisterID, RegisterID, RegisterID);

        void emitFastArithReTagImmediate(RegisterID src, RegisterID dest);

        void emitTagAsBoolImmediate(RegisterID reg);
        void compileBinaryArithOp(OpcodeID, int dst, int src1, int src2, OperandTypes opi);
        void compileBinaryArithOpSlowCase(Instruction*, OpcodeID, Vector<SlowCaseEntry>::iterator&, int dst, int src1, int src2, OperandTypes, bool op1HasImmediateIntFastCase, bool op2HasImmediateIntFastCase);

        void compileGetByIdHotPath(int baseVReg, const Identifier*);
        void compileGetDirectOffset(RegisterID base, RegisterID result, PropertyOffset cachedOffset);
        void compileGetDirectOffset(JSObject* base, RegisterID result, PropertyOffset cachedOffset);
        void compileGetDirectOffset(RegisterID base, RegisterID result, RegisterID offset, RegisterID scratch, FinalObjectMode = MayBeFinal);
        void compilePutDirectOffset(RegisterID base, RegisterID value, PropertyOffset cachedOffset);

#endif // USE(JSVALUE32_64)

        void emit_compareAndJump(OpcodeID, int op1, int op2, unsigned target, RelationalCondition);
        void emit_compareAndJumpSlow(int op1, int op2, unsigned target, DoubleCondition, size_t (JIT_OPERATION *operation)(ExecState*, EncodedJSValue, EncodedJSValue), bool invert, Vector<SlowCaseEntry>::iterator&);

        void emit_op_touch_entry(Instruction*);
        void emit_op_add(Instruction*);
        void emit_op_bitand(Instruction*);
        void emit_op_bitor(Instruction*);
        void emit_op_bitxor(Instruction*);
        void emit_op_call(Instruction*);
        void emit_op_call_eval(Instruction*);
        void emit_op_call_varargs(Instruction*);
        void emit_op_captured_mov(Instruction*);
        void emit_op_catch(Instruction*);
        void emit_op_construct(Instruction*);
        void emit_op_get_callee(Instruction*);
        void emit_op_create_this(Instruction*);
        void emit_op_to_this(Instruction*);
        void emit_op_create_arguments(Instruction*);
        void emit_op_debug(Instruction*);
        void emit_op_del_by_id(Instruction*);
        void emit_op_div(Instruction*);
        void emit_op_end(Instruction*);
        void emit_op_enter(Instruction*);
        void emit_op_create_activation(Instruction*);
        void emit_op_eq(Instruction*);
        void emit_op_eq_null(Instruction*);
        void emit_op_get_by_id(Instruction*);
        void emit_op_get_arguments_length(Instruction*);
        void emit_op_get_by_val(Instruction*);
        void emit_op_get_argument_by_val(Instruction*);
        void emit_op_get_by_pname(Instruction*);
        void emit_op_init_lazy_reg(Instruction*);
        void emit_op_check_has_instance(Instruction*);
        void emit_op_instanceof(Instruction*);
        void emit_op_is_undefined(Instruction*);
        void emit_op_is_boolean(Instruction*);
        void emit_op_is_number(Instruction*);
        void emit_op_is_string(Instruction*);
        void emit_op_jeq_null(Instruction*);
        void emit_op_jfalse(Instruction*);
        void emit_op_jmp(Instruction*);
        void emit_op_jneq_null(Instruction*);
        void emit_op_jneq_ptr(Instruction*);
        void emit_op_jless(Instruction*);
        void emit_op_jlesseq(Instruction*);
        void emit_op_jgreater(Instruction*);
        void emit_op_jgreatereq(Instruction*);
        void emit_op_jnless(Instruction*);
        void emit_op_jnlesseq(Instruction*);
        void emit_op_jngreater(Instruction*);
        void emit_op_jngreatereq(Instruction*);
        void emit_op_jtrue(Instruction*);
        void emit_op_loop_hint(Instruction*);
        void emit_op_lshift(Instruction*);
        void emit_op_mod(Instruction*);
        void emit_op_mov(Instruction*);
        void emit_op_mul(Instruction*);
        void emit_op_negate(Instruction*);
        void emit_op_neq(Instruction*);
        void emit_op_neq_null(Instruction*);
        void emit_op_new_array(Instruction*);
        void emit_op_new_array_with_size(Instruction*);
        void emit_op_new_array_buffer(Instruction*);
        void emit_op_new_func(Instruction*);
        void emit_op_new_captured_func(Instruction*);
        void emit_op_new_func_exp(Instruction*);
        void emit_op_new_object(Instruction*);
        void emit_op_new_regexp(Instruction*);
        void emit_op_get_pnames(Instruction*);
        void emit_op_next_pname(Instruction*);
        void emit_op_not(Instruction*);
        void emit_op_nstricteq(Instruction*);
        void emit_op_pop_scope(Instruction*);
        void emit_op_dec(Instruction*);
        void emit_op_inc(Instruction*);
        void emit_op_profile_did_call(Instruction*);
        void emit_op_profile_will_call(Instruction*);
        void emit_op_push_name_scope(Instruction*);
        void emit_op_push_with_scope(Instruction*);
        void emit_op_put_by_id(Instruction*);
        void emit_op_put_by_index(Instruction*);
        void emit_op_put_by_val(Instruction*);
        void emit_op_put_getter_setter(Instruction*);
        void emit_op_init_global_const(Instruction*);
        void emit_op_ret(Instruction*);
        void emit_op_ret_object_or_this(Instruction*);
        void emit_op_rshift(Instruction*);
        void emit_op_strcat(Instruction*);
        void emit_op_stricteq(Instruction*);
        void emit_op_sub(Instruction*);
        void emit_op_switch_char(Instruction*);
        void emit_op_switch_imm(Instruction*);
        void emit_op_switch_string(Instruction*);
        void emit_op_tear_off_activation(Instruction*);
        void emit_op_tear_off_arguments(Instruction*);
        void emit_op_throw(Instruction*);
        void emit_op_throw_static_error(Instruction*);
        void emit_op_to_number(Instruction*);
        void emit_op_to_primitive(Instruction*);
        void emit_op_unexpected_load(Instruction*);
        void emit_op_unsigned(Instruction*);
        void emit_op_urshift(Instruction*);

        void emitSlow_op_add(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitand(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitor(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitxor(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_eval(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_varargs(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_captured_mov(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_construct(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_to_this(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_create_this(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_div(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_eq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_callee(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_arguments_length(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_argument_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_pname(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_check_has_instance(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_instanceof(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jfalse(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jless(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jlesseq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jgreater(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jgreatereq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnless(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnlesseq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jngreater(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jngreatereq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jtrue(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_hint(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_lshift(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mod(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mul(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_negate(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_neq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_new_object(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_not(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_nstricteq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_dec(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_inc(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_rshift(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_stricteq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_sub(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_to_number(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_to_primitive(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_unsigned(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_urshift(Instruction*, Vector<SlowCaseEntry>::iterator&);

        void emit_op_resolve_scope(Instruction*);
        void emit_op_get_from_scope(Instruction*);
        void emit_op_put_to_scope(Instruction*);
        void emitSlow_op_resolve_scope(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_from_scope(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_to_scope(Instruction*, Vector<SlowCaseEntry>::iterator&);

        void emitRightShift(Instruction*, bool isUnsigned);
        void emitRightShiftSlowCase(Instruction*, Vector<SlowCaseEntry>::iterator&, bool isUnsigned);

        void emitVarInjectionCheck(bool needsVarInjectionChecks);
        void emitResolveClosure(int dst, bool needsVarInjectionChecks, unsigned depth);
        void emitLoadWithStructureCheck(int scope, Structure** structureSlot);
        void emitGetGlobalProperty(uintptr_t* operandSlot);
        void emitGetGlobalVar(uintptr_t operand);
        void emitGetClosureVar(int scope, uintptr_t operand);
        void emitPutGlobalProperty(uintptr_t* operandSlot, int value);
#if USE(JSVALUE64)
        void emitNotifyWrite(RegisterID value, RegisterID scratch, VariableWatchpointSet*);
#else
        void emitNotifyWrite(RegisterID tag, RegisterID payload, RegisterID scratch, VariableWatchpointSet*);
#endif
        void emitPutGlobalVar(uintptr_t operand, int value, VariableWatchpointSet*);
        void emitPutClosureVar(int scope, uintptr_t operand, int value);

        void emitInitRegister(int dst);

        void emitPutIntToCallFrameHeader(RegisterID from, JSStack::CallFrameHeaderEntry);
        void emitGetFromCallFrameHeaderPtr(JSStack::CallFrameHeaderEntry, RegisterID to, RegisterID from = callFrameRegister);
        void emitGetFromCallFrameHeader32(JSStack::CallFrameHeaderEntry, RegisterID to, RegisterID from = callFrameRegister);
#if USE(JSVALUE64)
        void emitGetFromCallFrameHeader64(JSStack::CallFrameHeaderEntry, RegisterID to, RegisterID from = callFrameRegister);
#endif

        JSValue getConstantOperand(int src);
        bool isOperandConstantImmediateInt(int src);
        bool isOperandConstantImmediateChar(int src);

        Jump getSlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            return iter++->from;
        }
        void linkSlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            iter->from.link(this);
            ++iter;
        }
        void linkDummySlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            ASSERT(!iter->from.isSet());
            ++iter;
        }
        void linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator&, int virtualRegisterIndex);

        MacroAssembler::Call appendCallWithExceptionCheck(const FunctionPtr&);
        MacroAssembler::Call appendCallWithCallFrameRollbackOnException(const FunctionPtr&);
        MacroAssembler::Call appendCallWithExceptionCheckSetJSValueResult(const FunctionPtr&, int);
        MacroAssembler::Call appendCallWithExceptionCheckSetJSValueResultWithProfile(const FunctionPtr&, int);
        
        enum WithProfileTag { WithProfile };
        
        MacroAssembler::Call callOperation(C_JITOperation_E);
        MacroAssembler::Call callOperation(C_JITOperation_EO, GPRReg);
        MacroAssembler::Call callOperation(C_JITOperation_ESt, Structure*);
        MacroAssembler::Call callOperation(C_JITOperation_EZ, int32_t);
        MacroAssembler::Call callOperation(F_JITOperation_EJZZ, GPRReg, int32_t, int32_t);
        MacroAssembler::Call callOperation(J_JITOperation_E, int);
        MacroAssembler::Call callOperation(J_JITOperation_EAapJ, int, ArrayAllocationProfile*, GPRReg);
        MacroAssembler::Call callOperation(J_JITOperation_EAapJcpZ, int, ArrayAllocationProfile*, GPRReg, int32_t);
        MacroAssembler::Call callOperation(J_JITOperation_EAapJcpZ, int, ArrayAllocationProfile*, const JSValue*, int32_t);
        MacroAssembler::Call callOperation(J_JITOperation_EC, int, JSCell*);
        MacroAssembler::Call callOperation(V_JITOperation_EC, JSCell*);
        MacroAssembler::Call callOperation(J_JITOperation_EJ, int, GPRReg);
#if USE(JSVALUE64)
        MacroAssembler::Call callOperation(WithProfileTag, J_JITOperation_ESsiJI, int, StructureStubInfo*, GPRReg, StringImpl*);
#else
        MacroAssembler::Call callOperation(WithProfileTag, J_JITOperation_ESsiJI, int, StructureStubInfo*, GPRReg, GPRReg, StringImpl*);
#endif
        MacroAssembler::Call callOperation(J_JITOperation_EJIdc, int, GPRReg, const Identifier*);
        MacroAssembler::Call callOperation(J_JITOperation_EJJ, int, GPRReg, GPRReg);
#if USE(JSVALUE64)
        MacroAssembler::Call callOperation(WithProfileTag, J_JITOperation_EJJ, int, GPRReg, GPRReg);
#else
        MacroAssembler::Call callOperation(WithProfileTag, J_JITOperation_EJJ, int, GPRReg, GPRReg, GPRReg, GPRReg);
#endif
        MacroAssembler::Call callOperation(J_JITOperation_EP, int, void*);
        MacroAssembler::Call callOperation(WithProfileTag, J_JITOperation_EPc, int, Instruction*);
        MacroAssembler::Call callOperation(J_JITOperation_EZ, int, int32_t);
        MacroAssembler::Call callOperation(P_JITOperation_EJS, GPRReg, size_t);
        MacroAssembler::Call callOperation(S_JITOperation_ECC, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(S_JITOperation_EJ, RegisterID);
        MacroAssembler::Call callOperation(S_JITOperation_EJJ, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(S_JITOperation_EOJss, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(Sprt_JITOperation_EZ, int32_t);
        MacroAssembler::Call callOperation(V_JITOperation_E);
        MacroAssembler::Call callOperation(V_JITOperation_EC, RegisterID);
        MacroAssembler::Call callOperation(V_JITOperation_ECC, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(V_JITOperation_ECICC, RegisterID, const Identifier*, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(V_JITOperation_EIdJZ, const Identifier*, RegisterID, int32_t);
        MacroAssembler::Call callOperation(V_JITOperation_EJ, RegisterID);
#if USE(JSVALUE64)
        MacroAssembler::Call callOperationNoExceptionCheck(V_JITOperation_EJ, RegisterID);
#else
        MacroAssembler::Call callOperationNoExceptionCheck(V_JITOperation_EJ, RegisterID, RegisterID);
#endif
        MacroAssembler::Call callOperation(V_JITOperation_EJIdJJ, RegisterID, const Identifier*, RegisterID, RegisterID);
#if USE(JSVALUE64)
        MacroAssembler::Call callOperation(F_JITOperation_EFJJZ, RegisterID, RegisterID, RegisterID, int32_t);
        MacroAssembler::Call callOperation(V_JITOperation_ESsiJJI, StructureStubInfo*, RegisterID, RegisterID, StringImpl*);
#else
        MacroAssembler::Call callOperation(V_JITOperation_ESsiJJI, StructureStubInfo*, RegisterID, RegisterID, RegisterID, RegisterID, StringImpl*);
#endif
        MacroAssembler::Call callOperation(V_JITOperation_EJJJ, RegisterID, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(V_JITOperation_EJZJ, RegisterID, int32_t, RegisterID);
        MacroAssembler::Call callOperation(V_JITOperation_EJZ, RegisterID, int32_t);
        MacroAssembler::Call callOperation(V_JITOperation_EPc, Instruction*);
        MacroAssembler::Call callOperation(V_JITOperation_EZ, int32_t);
        MacroAssembler::Call callOperationWithCallFrameRollbackOnException(J_JITOperation_E);
        MacroAssembler::Call callOperationNoExceptionCheck(J_JITOperation_EE, RegisterID);
        MacroAssembler::Call callOperationWithCallFrameRollbackOnException(V_JITOperation_ECb, CodeBlock*);
        MacroAssembler::Call callOperationWithCallFrameRollbackOnException(Z_JITOperation_E);
#if USE(JSVALUE32_64)
        MacroAssembler::Call callOperation(F_JITOperation_EFJJZ, RegisterID, RegisterID, RegisterID, RegisterID, RegisterID, int32_t);
        MacroAssembler::Call callOperation(F_JITOperation_EJZZ, GPRReg, GPRReg, int32_t, int32_t);
        MacroAssembler::Call callOperation(J_JITOperation_EAapJ, int, ArrayAllocationProfile*, GPRReg, GPRReg);
        MacroAssembler::Call callOperation(J_JITOperation_EJ, int, GPRReg, GPRReg);
        MacroAssembler::Call callOperation(J_JITOperation_EJIdc, int, GPRReg, GPRReg, const Identifier*);
        MacroAssembler::Call callOperation(J_JITOperation_EJJ, int, GPRReg, GPRReg, GPRReg, GPRReg);
        MacroAssembler::Call callOperation(P_JITOperation_EJS, GPRReg, GPRReg, size_t);
        MacroAssembler::Call callOperation(S_JITOperation_EJ, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(S_JITOperation_EJJ, RegisterID, RegisterID, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(V_JITOperation_EIdJZ, const Identifier*, RegisterID, RegisterID, int32_t);
        MacroAssembler::Call callOperation(V_JITOperation_EJ, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(V_JITOperation_EJJJ, RegisterID, RegisterID, RegisterID, RegisterID, RegisterID, RegisterID);
        MacroAssembler::Call callOperation(V_JITOperation_EJZ, RegisterID, RegisterID, int32_t);
        MacroAssembler::Call callOperation(V_JITOperation_EJZJ, RegisterID, RegisterID, int32_t, RegisterID, RegisterID);
#endif

        Jump checkStructure(RegisterID reg, Structure* structure);

        void updateTopCallFrame();

        Call emitNakedCall(CodePtr function = CodePtr());

        // Loads the character value of a single character string into dst.
        void emitLoadCharacterString(RegisterID src, RegisterID dst, JumpList& failures);
        
#if ENABLE(DFG_JIT)
        void emitEnterOptimizationCheck();
#else
        void emitEnterOptimizationCheck() { }
#endif

#ifndef NDEBUG
        void printBytecodeOperandTypes(int src1, int src2);
#endif

#if ENABLE(SAMPLING_FLAGS)
        void setSamplingFlag(int32_t);
        void clearSamplingFlag(int32_t);
#endif

#if ENABLE(SAMPLING_COUNTERS)
        void emitCount(AbstractSamplingCounter&, int32_t = 1);
#endif

#if ENABLE(OPCODE_SAMPLING)
        void sampleInstruction(Instruction*, bool = false);
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

        Interpreter* m_interpreter;

        Vector<CallRecord> m_calls;
        Vector<Label> m_labels;
        Vector<JITGetByIdGenerator> m_getByIds;
        Vector<JITPutByIdGenerator> m_putByIds;
        Vector<ByValCompilationInfo> m_byValCompilationInfo;
        Vector<StructureStubCompilationInfo> m_callStructureStubCompilationInfo;
        Vector<JumpTable> m_jmpTable;

        unsigned m_bytecodeOffset;
        Vector<SlowCaseEntry> m_slowCases;
        Vector<SwitchRecord> m_switches;

        JumpList m_exceptionChecks;
        JumpList m_exceptionChecksWithCallFrameRollback;

        unsigned m_getByIdIndex;
        unsigned m_putByIdIndex;
        unsigned m_byValInstructionIndex;
        unsigned m_callLinkInfoIndex;

        OwnPtr<JITDisassembler> m_disassembler;
        RefPtr<Profiler::Compilation> m_compilation;
        WeakRandom m_randomGenerator;
        static CodeRef stringGetByValStubGenerator(VM*);

        bool m_canBeOptimized;
        bool m_canBeOptimizedOrInlined;
        bool m_shouldEmitProfiling;
    } JIT_CLASS_ALIGNMENT;

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JIT_h
