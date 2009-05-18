/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(JIT)

// We've run into some problems where changing the size of the class JIT leads to
// performance fluctuations.  Try forcing alignment in an attempt to stabalize this.
#if COMPILER(GCC)
#define JIT_CLASS_ALIGNMENT __attribute__ ((aligned (32)))
#else
#define JIT_CLASS_ALIGNMENT
#endif

#include "CodeBlock.h"
#include "Interpreter.h"
#include "JITCode.h"
#include "JITStubs.h"
#include "Opcode.h"
#include "RegisterFile.h"
#include "MacroAssembler.h"
#include "Profiler.h"
#include <bytecode/SamplingTool.h>
#include <wtf/AlwaysInline.h>
#include <wtf/Vector.h>

namespace JSC {

    class CodeBlock;
    class JIT;
    class JSPropertyNameIterator;
    class Interpreter;
    class Register;
    class RegisterFile;
    class ScopeChainNode;
    class SimpleJumpTable;
    class StringJumpTable;
    class StructureChain;

    struct CallLinkInfo;
    struct Instruction;
    struct OperandTypes;
    struct PolymorphicAccessStructureList;
    struct StructureStubInfo;

    struct CallRecord {
        MacroAssembler::Call from;
        unsigned bytecodeIndex;
        void* to;

        CallRecord()
        {
        }

        CallRecord(MacroAssembler::Call from, unsigned bytecodeIndex, void* to = 0)
            : from(from)
            , bytecodeIndex(bytecodeIndex)
            , to(to)
        {
        }
    };

    struct JumpTable {
        MacroAssembler::Jump from;
        unsigned toBytecodeIndex;

        JumpTable(MacroAssembler::Jump f, unsigned t)
            : from(f)
            , toBytecodeIndex(t)
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

        unsigned bytecodeIndex;
        unsigned defaultOffset;

        SwitchRecord(SimpleJumpTable* jumpTable, unsigned bytecodeIndex, unsigned defaultOffset, Type type)
            : type(type)
            , bytecodeIndex(bytecodeIndex)
            , defaultOffset(defaultOffset)
        {
            this->jumpTable.simpleJumpTable = jumpTable;
        }

        SwitchRecord(StringJumpTable* jumpTable, unsigned bytecodeIndex, unsigned defaultOffset)
            : type(String)
            , bytecodeIndex(bytecodeIndex)
            , defaultOffset(defaultOffset)
        {
            this->jumpTable.stringJumpTable = jumpTable;
        }
    };

    struct PropertyStubCompilationInfo {
        MacroAssembler::Call callReturnLocation;
        MacroAssembler::Label hotPathBegin;
    };

    struct StructureStubCompilationInfo {
        MacroAssembler::DataLabelPtr hotPathBegin;
        MacroAssembler::Call hotPathOther;
        MacroAssembler::Call callReturnLocation;
        MacroAssembler::Label coldPathOther;
    };

    void ctiPatchCallByReturnAddress(MacroAssembler::ProcessorReturnAddress returnAddress, void* newCalleeFunction);
    void ctiPatchNearCallByReturnAddress(MacroAssembler::ProcessorReturnAddress returnAddress, void* newCalleeFunction);

    class JIT : private MacroAssembler {
        friend class JITStubCall;
        friend class CallEvalJITStub;

        using MacroAssembler::Jump;
        using MacroAssembler::JumpList;
        using MacroAssembler::Label;

        // NOTES:
        //
        // regT0 has two special meanings.  The return value from a stub
        // call will always be in regT0, and by default (unless
        // a register is specified) emitPutVirtualRegister() will store
        // the value from regT0.
        //
        // tempRegister2 is has no such dependencies.  It is important that
        // on x86/x86-64 it is ecx for performance reasons, since the
        // MacroAssembler will need to plant register swaps if it is not -
        // however the code will still function correctly.
#if PLATFORM(X86_64)
        static const RegisterID returnValueRegister = X86::eax;
        static const RegisterID cachedResultRegister = X86::eax;
        static const RegisterID firstArgumentRegister = X86::edi;

        static const RegisterID timeoutCheckRegister = X86::r12;
        static const RegisterID callFrameRegister = X86::r13;
        static const RegisterID tagTypeNumberRegister = X86::r14;
        static const RegisterID tagMaskRegister = X86::r15;

        static const RegisterID regT0 = X86::eax;
        static const RegisterID regT1 = X86::edx;
        static const RegisterID regT2 = X86::ecx;
        // NOTE: privateCompileCTIMachineTrampolines() relies on this being callee preserved; this should be considered non-interface.
        static const RegisterID regT3 = X86::ebx;

        static const FPRegisterID fpRegT0 = X86::xmm0;
        static const FPRegisterID fpRegT1 = X86::xmm1;
        static const FPRegisterID fpRegT2 = X86::xmm2;
#elif PLATFORM(X86)
        static const RegisterID returnValueRegister = X86::eax;
        static const RegisterID cachedResultRegister = X86::eax;
        // On x86 we always use fastcall conventions = but on
        // OS X if might make more sense to just use regparm.
        static const RegisterID firstArgumentRegister = X86::ecx;

        static const RegisterID timeoutCheckRegister = X86::esi;
        static const RegisterID callFrameRegister = X86::edi;

        static const RegisterID regT0 = X86::eax;
        static const RegisterID regT1 = X86::edx;
        static const RegisterID regT2 = X86::ecx;
        // NOTE: privateCompileCTIMachineTrampolines() relies on this being callee preserved; this should be considered non-interface.
        static const RegisterID regT3 = X86::ebx;

        static const FPRegisterID fpRegT0 = X86::xmm0;
        static const FPRegisterID fpRegT1 = X86::xmm1;
        static const FPRegisterID fpRegT2 = X86::xmm2;
#else
    #error "JIT not supported on this platform."
#endif

        static const int patchGetByIdDefaultStructure = -1;
        // Magic number - initial offset cannot be representable as a signed 8bit value, or the X86Assembler
        // will compress the displacement, and we may not be able to fit a patched offset.
        static const int patchGetByIdDefaultOffset = 256;

#if USE(JIT_STUB_ARGUMENT_REGISTER)
#if PLATFORM(X86_64)
        static const int ctiArgumentInitSize = 6;
#else
        static const int ctiArgumentInitSize = 2;
#endif
#elif USE(JIT_STUB_ARGUMENT_STACK)
        static const int ctiArgumentInitSize = 4;
#else // JIT_STUB_ARGUMENT_VA_LIST
        static const int ctiArgumentInitSize = 0;
#endif

#if PLATFORM(X86_64)
        // These architecture specific value are used to enable patching - see comment on op_put_by_id.
        static const int patchOffsetPutByIdStructure = 10;
        static const int patchOffsetPutByIdExternalLoad = 20;
        static const int patchLengthPutByIdExternalLoad = 4;
        static const int patchLengthPutByIdExternalLoadPrefix = 1;
        static const int patchOffsetPutByIdPropertyMapOffset = 31;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 10;
        static const int patchOffsetGetByIdBranchToSlowCase = 20;
        static const int patchOffsetGetByIdExternalLoad = 20;
        static const int patchLengthGetByIdExternalLoad = 4;
        static const int patchLengthGetByIdExternalLoadPrefix = 1;
        static const int patchOffsetGetByIdPropertyMapOffset = 31;
        static const int patchOffsetGetByIdPutResult = 31;
#if ENABLE(OPCODE_SAMPLING)
        static const int patchOffsetGetByIdSlowCaseCall = 61 + ctiArgumentInitSize;
#else
        static const int patchOffsetGetByIdSlowCaseCall = 38 + ctiArgumentInitSize;
#endif
        static const int patchOffsetOpCallCompareToJump = 9;
#else
        // These architecture specific value are used to enable patching - see comment on op_put_by_id.
        static const int patchOffsetPutByIdStructure = 7;
        static const int patchOffsetPutByIdExternalLoad = 13;
        static const int patchLengthPutByIdExternalLoad = 3;
        static const int patchLengthPutByIdExternalLoadPrefix = 0;
        static const int patchOffsetPutByIdPropertyMapOffset = 22;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 7;
        static const int patchOffsetGetByIdBranchToSlowCase = 13;
        static const int patchOffsetGetByIdExternalLoad = 13;
        static const int patchLengthGetByIdExternalLoad = 3;
        static const int patchLengthGetByIdExternalLoadPrefix = 0;
        static const int patchOffsetGetByIdPropertyMapOffset = 22;
        static const int patchOffsetGetByIdPutResult = 22;
#if ENABLE(OPCODE_SAMPLING)
        static const int patchOffsetGetByIdSlowCaseCall = 31 + ctiArgumentInitSize;
#else
        static const int patchOffsetGetByIdSlowCaseCall = 21 + ctiArgumentInitSize;
#endif
        static const int patchOffsetOpCallCompareToJump = 6;
#endif

    public:
        static void compile(JSGlobalData* globalData, CodeBlock* codeBlock)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompile();
        }

        static void compileGetByIdProto(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdProto(stubInfo, structure, prototypeStructure, cachedOffset, returnAddress, callFrame);
        }

        static void compileGetByIdSelfList(JSGlobalData* globalData, CodeBlock* codeBlock, StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, size_t cachedOffset)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdSelfList(stubInfo, polymorphicStructures, currentIndex, structure, cachedOffset);
        }
        static void compileGetByIdProtoList(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructureList, int currentIndex, Structure* structure, Structure* prototypeStructure, size_t cachedOffset)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdProtoList(stubInfo, prototypeStructureList, currentIndex, structure, prototypeStructure, cachedOffset, callFrame);
        }
        static void compileGetByIdChainList(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructureList, int currentIndex, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdChainList(stubInfo, prototypeStructureList, currentIndex, structure, chain, count, cachedOffset, callFrame);
        }

        static void compileGetByIdChain(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, ProcessorReturnAddress returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdChain(stubInfo, structure, chain, count, cachedOffset, returnAddress, callFrame);
        }
        
        static void compilePutByIdTransition(JSGlobalData* globalData, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, ProcessorReturnAddress returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompilePutByIdTransition(stubInfo, oldStructure, newStructure, cachedOffset, chain, returnAddress);
        }

        static void compileCTIMachineTrampolines(JSGlobalData* globalData, RefPtr<ExecutablePool>* executablePool, void** ctiArrayLengthTrampoline, void** ctiStringLengthTrampoline, void** ctiVirtualCallPreLink, void** ctiVirtualCallLink, void** ctiVirtualCall, void** ctiNativeCallThunk)
        {
            JIT jit(globalData);
            jit.privateCompileCTIMachineTrampolines(executablePool, globalData, ctiArrayLengthTrampoline, ctiStringLengthTrampoline, ctiVirtualCallPreLink, ctiVirtualCallLink, ctiVirtualCall, ctiNativeCallThunk);
        }

        static void patchGetByIdSelf(StructureStubInfo*, Structure*, size_t cachedOffset, ProcessorReturnAddress returnAddress);
        static void patchPutByIdReplace(StructureStubInfo*, Structure*, size_t cachedOffset, ProcessorReturnAddress returnAddress);

        static void compilePatchGetArrayLength(JSGlobalData* globalData, CodeBlock* codeBlock, ProcessorReturnAddress returnAddress)
        {
            JIT jit(globalData, codeBlock);
            return jit.privateCompilePatchGetArrayLength(returnAddress);
        }

        static void linkCall(JSFunction* callee, CodeBlock* calleeCodeBlock, JITCode ctiCode, CallLinkInfo* callLinkInfo, int callerArgCount);
        static void unlinkCall(CallLinkInfo*);

    private:
        struct JSRInfo {
            DataLabelPtr storeLocation;
            Label target;

            JSRInfo(DataLabelPtr storeLocation, Label targetLocation)
                : storeLocation(storeLocation)
                , target(targetLocation)
            {
            }
        };

        JIT(JSGlobalData*, CodeBlock* = 0);

        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        void privateCompile();
        void privateCompileGetByIdProto(StructureStubInfo*, Structure*, Structure* prototypeStructure, size_t cachedOffset, ProcessorReturnAddress returnAddress, CallFrame* callFrame);
        void privateCompileGetByIdSelfList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, size_t cachedOffset);
        void privateCompileGetByIdProtoList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, Structure* prototypeStructure, size_t cachedOffset, CallFrame* callFrame);
        void privateCompileGetByIdChainList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, StructureChain* chain, size_t count, size_t cachedOffset, CallFrame* callFrame);
        void privateCompileGetByIdChain(StructureStubInfo*, Structure*, StructureChain*, size_t count, size_t cachedOffset, ProcessorReturnAddress returnAddress, CallFrame* callFrame);
        void privateCompilePutByIdTransition(StructureStubInfo*, Structure*, Structure*, size_t cachedOffset, StructureChain*, ProcessorReturnAddress returnAddress);

        void privateCompileCTIMachineTrampolines(RefPtr<ExecutablePool>* executablePool, JSGlobalData* data, void** ctiArrayLengthTrampoline, void** ctiStringLengthTrampoline, void** ctiVirtualCallPreLink, void** ctiVirtualCallLink, void** ctiVirtualCall, void** ctiNativeCallThunk);
        void privateCompilePatchGetArrayLength(ProcessorReturnAddress returnAddress);

        void addSlowCase(Jump);
        void addJump(Jump, int);
        void emitJumpSlowToHot(Jump, int);

        void compileGetByIdHotPath(int resultVReg, int baseVReg, Identifier* ident, unsigned propertyAccessInstructionIndex);
        void compileGetByIdSlowCase(int resultVReg, int baseVReg, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex);
        void compilePutByIdHotPath(int baseVReg, Identifier* ident, int valueVReg, unsigned propertyAccessInstructionIndex);
        void compilePutByIdSlowCase(int baseVReg, Identifier* ident, int valueVReg, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex);
        void compileOpCall(OpcodeID, Instruction* instruction, unsigned callLinkInfoIndex);
        void compileOpCallVarargs(Instruction* instruction);
        void compileOpCallInitializeCallFrame();
        void compileOpCallSetupArgs(Instruction*);
        void compileOpCallVarargsSetupArgs(Instruction*);
        void compileOpCallSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex, OpcodeID opcodeID);
        void compileOpCallVarargsSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter);
        void compileOpConstructSetupArgs(Instruction*);
        enum CompileOpStrictEqType { OpStrictEq, OpNStrictEq };
        void compileOpStrictEq(Instruction* instruction, CompileOpStrictEqType type);

        void compileGetDirectOffset(RegisterID base, RegisterID result, Structure* structure, size_t cachedOffset);
        void compileGetDirectOffset(JSObject* base, RegisterID temp, RegisterID result, size_t cachedOffset);
        void compilePutDirectOffset(RegisterID base, RegisterID value, Structure* structure, size_t cachedOffset);

        // Arithmetic Ops

        void emit_op_add(Instruction*);
        void emit_op_sub(Instruction*);
        void emit_op_mul(Instruction*);
        void emit_op_mod(Instruction*);
        void emit_op_bitand(Instruction*);
        void emit_op_lshift(Instruction*);
        void emit_op_rshift(Instruction*);
        void emit_op_jnless(Instruction*);
        void emit_op_jnlesseq(Instruction*);
        void emit_op_pre_inc(Instruction*);
        void emit_op_pre_dec(Instruction*);
        void emit_op_post_inc(Instruction*);
        void emit_op_post_dec(Instruction*);
        void emitSlow_op_add(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_sub(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mul(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mod(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitand(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_lshift(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_rshift(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnless(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnlesseq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_pre_inc(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_pre_dec(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_post_inc(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_post_dec(Instruction*, Vector<SlowCaseEntry>::iterator&);

        void emit_op_get_by_val(Instruction*);
        void emit_op_put_by_val(Instruction*);
        void emit_op_put_by_index(Instruction*);
        void emit_op_put_getter(Instruction*);
        void emit_op_put_setter(Instruction*);
        void emit_op_del_by_id(Instruction*);

        void emit_op_mov(Instruction*);
        void emit_op_end(Instruction*);
        void emit_op_jmp(Instruction*);
        void emit_op_loop(Instruction*);
        void emit_op_loop_if_less(Instruction*);
        void emit_op_loop_if_lesseq(Instruction*);
        void emit_op_new_object(Instruction*);
        void emit_op_put_by_id(Instruction*);
        void emit_op_get_by_id(Instruction*);
        void emit_op_instanceof(Instruction*);
        void emit_op_new_func(Instruction*);
        void emit_op_call(Instruction*);
        void emit_op_call_eval(Instruction*);
        void emit_op_load_varargs(Instruction*);
        void emit_op_call_varargs(Instruction*);
        void emit_op_construct(Instruction*);
        void emit_op_get_global_var(Instruction*);
        void emit_op_put_global_var(Instruction*);
        void emit_op_get_scoped_var(Instruction*);
        void emit_op_put_scoped_var(Instruction*);
        void emit_op_tear_off_activation(Instruction*);
        void emit_op_tear_off_arguments(Instruction*);
        void emit_op_ret(Instruction*);
        void emit_op_new_array(Instruction*);
        void emit_op_resolve(Instruction*);
        void emit_op_construct_verify(Instruction*);
        void emit_op_to_primitive(Instruction*);
        void emit_op_strcat(Instruction*);
        void emit_op_resolve_func(Instruction*);
        void emit_op_loop_if_true(Instruction*);
        void emit_op_resolve_base(Instruction*);
        void emit_op_resolve_skip(Instruction*);
        void emit_op_resolve_global(Instruction*);
        void emit_op_not(Instruction*);
        void emit_op_jfalse(Instruction*);
        void emit_op_jeq_null(Instruction*);
        void emit_op_jneq_null(Instruction*);
        void emit_op_jneq_ptr(Instruction*);
        void emit_op_unexpected_load(Instruction*);
        void emit_op_jsr(Instruction*);
        void emit_op_sret(Instruction*);
        void emit_op_eq(Instruction*);
        void emit_op_bitnot(Instruction*);
        void emit_op_resolve_with_base(Instruction*);
        void emit_op_new_func_exp(Instruction*);
        void emit_op_jtrue(Instruction*);
        void emit_op_neq(Instruction*);
        void emit_op_bitxor(Instruction*);
        void emit_op_new_regexp(Instruction*);
        void emit_op_bitor(Instruction*);
        void emit_op_throw(Instruction*);
        void emit_op_next_pname(Instruction*);
        void emit_op_push_scope(Instruction*);
        void emit_op_pop_scope(Instruction*);
        void emit_op_stricteq(Instruction*);
        void emit_op_nstricteq(Instruction*);
        void emit_op_to_jsnumber(Instruction*);
        void emit_op_push_new_scope(Instruction*);
        void emit_op_catch(Instruction*);
        void emit_op_jmp_scopes(Instruction*);
        void emit_op_switch_imm(Instruction*);
        void emit_op_switch_char(Instruction*);
        void emit_op_switch_string(Instruction*);
        void emit_op_new_error(Instruction*);
        void emit_op_debug(Instruction*);
        void emit_op_eq_null(Instruction*);
        void emit_op_neq_null(Instruction*);
        void emit_op_enter(Instruction*);
        void emit_op_enter_with_activation(Instruction*);
        void emit_op_init_arguments(Instruction*);
        void emit_op_create_arguments(Instruction*);
        void emit_op_convert_this(Instruction*);
        void emit_op_profile_will_call(Instruction*);
        void emit_op_profile_did_call(Instruction*);

        void emitSlow_op_convert_this(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_construct_verify(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_to_primitive(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_if_less(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_if_lesseq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_if_true(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_not(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jfalse(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitnot(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jtrue(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitxor(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitor(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_eq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_neq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_stricteq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_nstricteq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_instanceof(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_eval(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_varargs(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_construct(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_to_jsnumber(Instruction*, Vector<SlowCaseEntry>::iterator&);

#if ENABLE(JIT_OPTIMIZE_ARITHMETIC)
        void compileBinaryArithOp(OpcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi);
        void compileBinaryArithOpSlowCase(OpcodeID, Vector<SlowCaseEntry>::iterator&, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi);
#endif

        void emitGetVirtualRegister(int src, RegisterID dst);
        void emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2);
        void emitPutVirtualRegister(unsigned dst, RegisterID from = regT0);

        void emitPutJITStubArg(RegisterID src, unsigned argumentNumber);
        void emitPutJITStubArgFromVirtualRegister(unsigned src, unsigned argumentNumber, RegisterID scratch);
        void emitPutJITStubArgConstant(unsigned value, unsigned argumentNumber);
        void emitPutJITStubArgConstant(void* value, unsigned argumentNumber);
        void emitGetJITStubArg(unsigned argumentNumber, RegisterID dst);

        void emitInitRegister(unsigned dst);

        void emitPutToCallFrameHeader(RegisterID from, RegisterFile::CallFrameHeaderEntry entry);
        void emitPutImmediateToCallFrameHeader(void* value, RegisterFile::CallFrameHeaderEntry entry);
        void emitGetFromCallFrameHeaderPtr(RegisterFile::CallFrameHeaderEntry entry, RegisterID to, RegisterID from = callFrameRegister);
        void emitGetFromCallFrameHeader32(RegisterFile::CallFrameHeaderEntry entry, RegisterID to, RegisterID from = callFrameRegister);

        JSValue getConstantOperand(unsigned src);
        int32_t getConstantOperandImmediateInt(unsigned src);
        bool isOperandConstantImmediateInt(unsigned src);

        Jump emitJumpIfJSCell(RegisterID);
        Jump emitJumpIfBothJSCells(RegisterID, RegisterID, RegisterID);
        void emitJumpSlowCaseIfJSCell(RegisterID);
        Jump emitJumpIfNotJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID, int VReg);
#if USE(ALTERNATE_JSIMMEDIATE)
        JIT::Jump emitJumpIfImmediateNumber(RegisterID);
        JIT::Jump emitJumpIfNotImmediateNumber(RegisterID);
#else
        JIT::Jump emitJumpIfImmediateNumber(RegisterID reg)
        {
            return emitJumpIfImmediateInteger(reg);
        }
        
        JIT::Jump emitJumpIfNotImmediateNumber(RegisterID reg)
        {
            return emitJumpIfNotImmediateInteger(reg);
        }
#endif

        Jump getSlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            return iter++->from;
        }
        void linkSlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            iter->from.link(this);
            ++iter;
        }
        void linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator&, int vReg);

        JIT::Jump emitJumpIfImmediateInteger(RegisterID);
        JIT::Jump emitJumpIfNotImmediateInteger(RegisterID);
        JIT::Jump emitJumpIfNotImmediateIntegers(RegisterID, RegisterID, RegisterID);
        void emitJumpSlowCaseIfNotImmediateInteger(RegisterID);
        void emitJumpSlowCaseIfNotImmediateIntegers(RegisterID, RegisterID, RegisterID);

        Jump checkStructure(RegisterID reg, Structure* structure);

#if !USE(ALTERNATE_JSIMMEDIATE)
        void emitFastArithDeTagImmediate(RegisterID);
        Jump emitFastArithDeTagImmediateJumpIfZero(RegisterID);
#endif
        void emitFastArithReTagImmediate(RegisterID src, RegisterID dest);
        void emitFastArithImmToInt(RegisterID);
        void emitFastArithIntToImmNoCheck(RegisterID src, RegisterID dest);

        void emitTagAsBoolImmediate(RegisterID reg);

        void restoreArgumentReference();
        void restoreArgumentReferenceForTrampoline();

        Call emitNakedCall(void* function);

        void emitGetVariableObjectRegister(RegisterID variableObject, int index, RegisterID dst);
        void emitPutVariableObjectRegister(RegisterID src, RegisterID variableObject, int index);
        
        void emitTimeoutCheck();
#ifndef NDEBUG
        void printBytecodeOperandTypes(unsigned src1, unsigned src2);
#endif

        void killLastResultRegister();


#if ENABLE(SAMPLING_FLAGS)
        void setSamplingFlag(int32_t);
        void clearSamplingFlag(int32_t);
#endif

#if ENABLE(SAMPLING_COUNTERS)
        void emitCount(AbstractSamplingCounter&, uint32_t = 1);
#endif

#if ENABLE(OPCODE_SAMPLING)
        void sampleInstruction(Instruction*, bool = false);
#endif

#if ENABLE(CODEBLOCK_SAMPLING)
        void sampleCodeBlock(CodeBlock*);
#else
        void sampleCodeBlock(CodeBlock*) {}
#endif

        Interpreter* m_interpreter;
        JSGlobalData* m_globalData;
        CodeBlock* m_codeBlock;

        Vector<CallRecord> m_calls;
        Vector<Label> m_labels;
        Vector<PropertyStubCompilationInfo> m_propertyAccessCompilationInfo;
        Vector<StructureStubCompilationInfo> m_callStructureStubCompilationInfo;
        Vector<JumpTable> m_jmpTable;

        unsigned m_bytecodeIndex;
        Vector<JSRInfo> m_jsrSites;
        Vector<SlowCaseEntry> m_slowCases;
        Vector<SwitchRecord> m_switches;

        int m_lastResultBytecodeRegister;
        unsigned m_jumpTargetsPosition;

        unsigned m_propertyAccessInstructionIndex;
        unsigned m_globalResolveInfoIndex;
        unsigned m_callLinkInfoIndex;
    } JIT_CLASS_ALIGNMENT;

}

#endif // ENABLE(JIT)

#endif // JIT_h
