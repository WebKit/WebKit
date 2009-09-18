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
    };

    struct MethodCallCompilationInfo {
        MethodCallCompilationInfo(unsigned propertyAccessIndex)
            : propertyAccessIndex(propertyAccessIndex)
        {
        }

        MacroAssembler::DataLabelPtr structureToCompare;
        unsigned propertyAccessIndex;
    };

    // Near calls can only be patched to other JIT code, regular calls can be patched to JIT code or relinked to stub functions.
    void ctiPatchNearCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, MacroAssemblerCodePtr newCalleeFunction);
    void ctiPatchCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, MacroAssemblerCodePtr newCalleeFunction);
    void ctiPatchCallByReturnAddress(CodeBlock* codeblock, ReturnAddressPtr returnAddress, FunctionPtr newCalleeFunction);

    class JIT : private MacroAssembler {
        friend class JITStubCall;

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
        // regT3 is required to be callee-preserved.
        //
        // tempRegister2 is has no such dependencies.  It is important that
        // on x86/x86-64 it is ecx for performance reasons, since the
        // MacroAssembler will need to plant register swaps if it is not -
        // however the code will still function correctly.
#if PLATFORM(X86_64)
        static const RegisterID returnValueRegister = X86Registers::eax;
        static const RegisterID cachedResultRegister = X86Registers::eax;
        static const RegisterID firstArgumentRegister = X86Registers::edi;

        static const RegisterID timeoutCheckRegister = X86Registers::r12;
        static const RegisterID callFrameRegister = X86Registers::r13;
        static const RegisterID tagTypeNumberRegister = X86Registers::r14;
        static const RegisterID tagMaskRegister = X86Registers::r15;

        static const RegisterID regT0 = X86Registers::eax;
        static const RegisterID regT1 = X86Registers::edx;
        static const RegisterID regT2 = X86Registers::ecx;
        static const RegisterID regT3 = X86Registers::ebx;

        static const FPRegisterID fpRegT0 = X86Registers::xmm0;
        static const FPRegisterID fpRegT1 = X86Registers::xmm1;
        static const FPRegisterID fpRegT2 = X86Registers::xmm2;
#elif PLATFORM(X86)
        static const RegisterID returnValueRegister = X86Registers::eax;
        static const RegisterID cachedResultRegister = X86Registers::eax;
        // On x86 we always use fastcall conventions = but on
        // OS X if might make more sense to just use regparm.
        static const RegisterID firstArgumentRegister = X86Registers::ecx;

        static const RegisterID timeoutCheckRegister = X86Registers::esi;
        static const RegisterID callFrameRegister = X86Registers::edi;

        static const RegisterID regT0 = X86Registers::eax;
        static const RegisterID regT1 = X86Registers::edx;
        static const RegisterID regT2 = X86Registers::ecx;
        static const RegisterID regT3 = X86Registers::ebx;

        static const FPRegisterID fpRegT0 = X86Registers::xmm0;
        static const FPRegisterID fpRegT1 = X86Registers::xmm1;
        static const FPRegisterID fpRegT2 = X86Registers::xmm2;
#elif PLATFORM(ARM_THUMB2)
        static const RegisterID returnValueRegister = ARMRegisters::r0;
        static const RegisterID cachedResultRegister = ARMRegisters::r0;
        static const RegisterID firstArgumentRegister = ARMRegisters::r0;

        static const RegisterID regT0 = ARMRegisters::r0;
        static const RegisterID regT1 = ARMRegisters::r1;
        static const RegisterID regT2 = ARMRegisters::r2;
        static const RegisterID regT3 = ARMRegisters::r4;

        static const RegisterID callFrameRegister = ARMRegisters::r5;
        static const RegisterID timeoutCheckRegister = ARMRegisters::r6;

        static const FPRegisterID fpRegT0 = ARMRegisters::d0;
        static const FPRegisterID fpRegT1 = ARMRegisters::d1;
        static const FPRegisterID fpRegT2 = ARMRegisters::d2;
#elif PLATFORM(ARM_TRADITIONAL)
        static const RegisterID returnValueRegister = ARMRegisters::r0;
        static const RegisterID cachedResultRegister = ARMRegisters::r0;
        static const RegisterID firstArgumentRegister = ARMRegisters::r0;

        static const RegisterID timeoutCheckRegister = ARMRegisters::r5;
        static const RegisterID callFrameRegister = ARMRegisters::r4;
        static const RegisterID ctiReturnRegister = ARMRegisters::r6;

        static const RegisterID regT0 = ARMRegisters::r0;
        static const RegisterID regT1 = ARMRegisters::r1;
        static const RegisterID regT2 = ARMRegisters::r2;
        // Callee preserved
        static const RegisterID regT3 = ARMRegisters::r7;

        static const RegisterID regS0 = ARMRegisters::S0;
        // Callee preserved
        static const RegisterID regS1 = ARMRegisters::S1;

        static const RegisterID regStackPtr = ARMRegisters::sp;
        static const RegisterID regLink = ARMRegisters::lr;

        static const FPRegisterID fpRegT0 = ARMRegisters::d0;
        static const FPRegisterID fpRegT1 = ARMRegisters::d1;
        static const FPRegisterID fpRegT2 = ARMRegisters::d2;
#else
    #error "JIT not supported on this platform."
#endif

        static const int patchGetByIdDefaultStructure = -1;
        // Magic number - initial offset cannot be representable as a signed 8bit value, or the X86Assembler
        // will compress the displacement, and we may not be able to fit a patched offset.
        static const int patchGetByIdDefaultOffset = 256;

    public:
        static JITCode compile(JSGlobalData* globalData, CodeBlock* codeBlock)
        {
            return JIT(globalData, codeBlock).privateCompile();
        }

        static void compileGetByIdProto(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, ReturnAddressPtr returnAddress)
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

        static void compileGetByIdChain(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, ReturnAddressPtr returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdChain(stubInfo, structure, chain, count, cachedOffset, returnAddress, callFrame);
        }
        
        static void compilePutByIdTransition(JSGlobalData* globalData, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, ReturnAddressPtr returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompilePutByIdTransition(stubInfo, oldStructure, newStructure, cachedOffset, chain, returnAddress);
        }

        static void compileCTIMachineTrampolines(JSGlobalData* globalData, RefPtr<ExecutablePool>* executablePool, CodePtr* ctiStringLengthTrampoline, CodePtr* ctiVirtualCallLink, CodePtr* ctiVirtualCall, CodePtr* ctiNativeCallThunk)
        {
            JIT jit(globalData);
            jit.privateCompileCTIMachineTrampolines(executablePool, globalData, ctiStringLengthTrampoline, ctiVirtualCallLink, ctiVirtualCall, ctiNativeCallThunk);
        }

        static void patchGetByIdSelf(CodeBlock* codeblock, StructureStubInfo*, Structure*, size_t cachedOffset, ReturnAddressPtr returnAddress);
        static void patchPutByIdReplace(CodeBlock* codeblock, StructureStubInfo*, Structure*, size_t cachedOffset, ReturnAddressPtr returnAddress);
        static void patchMethodCallProto(CodeBlock* codeblock, MethodCallLinkInfo&, JSFunction*, Structure*, JSObject*, ReturnAddressPtr);

        static void compilePatchGetArrayLength(JSGlobalData* globalData, CodeBlock* codeBlock, ReturnAddressPtr returnAddress)
        {
            JIT jit(globalData, codeBlock);
            return jit.privateCompilePatchGetArrayLength(returnAddress);
        }

        static void linkCall(JSFunction* callee, CodeBlock* callerCodeBlock, CodeBlock* calleeCodeBlock, JITCode&, CallLinkInfo*, int callerArgCount, JSGlobalData*);
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
        JITCode privateCompile();
        void privateCompileGetByIdProto(StructureStubInfo*, Structure*, Structure* prototypeStructure, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame);
        void privateCompileGetByIdSelfList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, size_t cachedOffset);
        void privateCompileGetByIdProtoList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, Structure* prototypeStructure, size_t cachedOffset, CallFrame* callFrame);
        void privateCompileGetByIdChainList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, StructureChain* chain, size_t count, size_t cachedOffset, CallFrame* callFrame);
        void privateCompileGetByIdChain(StructureStubInfo*, Structure*, StructureChain*, size_t count, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame);
        void privateCompilePutByIdTransition(StructureStubInfo*, Structure*, Structure*, size_t cachedOffset, StructureChain*, ReturnAddressPtr returnAddress);

        void privateCompileCTIMachineTrampolines(RefPtr<ExecutablePool>* executablePool, JSGlobalData* data, CodePtr* ctiStringLengthTrampoline, CodePtr* ctiVirtualCallLink, CodePtr* ctiVirtualCall, CodePtr* ctiNativeCallThunk);
        void privateCompilePatchGetArrayLength(ReturnAddressPtr returnAddress);

        void addSlowCase(Jump);
        void addSlowCase(JumpList);
        void addJump(Jump, int);
        void emitJumpSlowToHot(Jump, int);

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

#if USE(JSVALUE32_64)
        Address tagFor(unsigned index, RegisterID base = callFrameRegister);
        Address payloadFor(unsigned index, RegisterID base = callFrameRegister);
        Address addressFor(unsigned index, RegisterID base = callFrameRegister);

        bool getOperandConstantImmediateInt(unsigned op1, unsigned op2, unsigned& op, int32_t& constant);
        bool isOperandConstantImmediateDouble(unsigned src);

        void emitLoadTag(unsigned index, RegisterID tag);
        void emitLoadPayload(unsigned index, RegisterID payload);

        void emitLoad(const JSValue& v, RegisterID tag, RegisterID payload);
        void emitLoad(unsigned index, RegisterID tag, RegisterID payload, RegisterID base = callFrameRegister);
        void emitLoad2(unsigned index1, RegisterID tag1, RegisterID payload1, unsigned index2, RegisterID tag2, RegisterID payload2);
        void emitLoadDouble(unsigned index, FPRegisterID value);
        void emitLoadInt32ToDouble(unsigned index, FPRegisterID value);

        void emitStore(unsigned index, RegisterID tag, RegisterID payload, RegisterID base = callFrameRegister);
        void emitStore(unsigned index, const JSValue constant, RegisterID base = callFrameRegister);
        void emitStoreInt32(unsigned index, RegisterID payload, bool indexIsInt32 = false);
        void emitStoreInt32(unsigned index, Imm32 payload, bool indexIsInt32 = false);
        void emitStoreCell(unsigned index, RegisterID payload, bool indexIsCell = false);
        void emitStoreBool(unsigned index, RegisterID tag, bool indexIsBool = false);
        void emitStoreDouble(unsigned index, FPRegisterID value);

        bool isLabeled(unsigned bytecodeIndex);
        void map(unsigned bytecodeIndex, unsigned virtualRegisterIndex, RegisterID tag, RegisterID payload);
        void unmap(RegisterID);
        void unmap();
        bool isMapped(unsigned virtualRegisterIndex);
        bool getMappedPayload(unsigned virtualRegisterIndex, RegisterID& payload);
        bool getMappedTag(unsigned virtualRegisterIndex, RegisterID& tag);

        void emitJumpSlowCaseIfNotJSCell(unsigned virtualRegisterIndex);
        void emitJumpSlowCaseIfNotJSCell(unsigned virtualRegisterIndex, RegisterID tag);
        void linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator&, unsigned virtualRegisterIndex);

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
        void compileGetByIdHotPath();
        void compileGetByIdSlowCase(int resultVReg, int baseVReg, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, bool isMethodCheck = false);
#endif
        void compileGetDirectOffset(RegisterID base, RegisterID resultTag, RegisterID resultPayload, Structure* structure, size_t cachedOffset);
        void compileGetDirectOffset(JSObject* base, RegisterID temp, RegisterID resultTag, RegisterID resultPayload, size_t cachedOffset);
        void compilePutDirectOffset(RegisterID base, RegisterID valueTag, RegisterID valuePayload, Structure* structure, size_t cachedOffset);

        // Arithmetic opcode helpers
        void emitAdd32Constant(unsigned dst, unsigned op, int32_t constant, ResultType opType);
        void emitSub32Constant(unsigned dst, unsigned op, int32_t constant, ResultType opType);
        void emitBinaryDoubleOp(OpcodeID, unsigned dst, unsigned op1, unsigned op2, OperandTypes, JumpList& notInt32Op1, JumpList& notInt32Op2, bool op1IsInRegisters = true, bool op2IsInRegisters = true);

#if PLATFORM(X86)
        // These architecture specific value are used to enable patching - see comment on op_put_by_id.
        static const int patchOffsetPutByIdStructure = 7;
        static const int patchOffsetPutByIdExternalLoad = 13;
        static const int patchLengthPutByIdExternalLoad = 3;
        static const int patchOffsetPutByIdPropertyMapOffset1 = 22;
        static const int patchOffsetPutByIdPropertyMapOffset2 = 28;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 7;
        static const int patchOffsetGetByIdBranchToSlowCase = 13;
        static const int patchOffsetGetByIdExternalLoad = 13;
        static const int patchLengthGetByIdExternalLoad = 3;
        static const int patchOffsetGetByIdPropertyMapOffset1 = 22;
        static const int patchOffsetGetByIdPropertyMapOffset2 = 28;
        static const int patchOffsetGetByIdPutResult = 28;
#if ENABLE(OPCODE_SAMPLING) && USE(JIT_STUB_ARGUMENT_VA_LIST)
        static const int patchOffsetGetByIdSlowCaseCall = 35;
#elif ENABLE(OPCODE_SAMPLING)
        static const int patchOffsetGetByIdSlowCaseCall = 37;
#elif USE(JIT_STUB_ARGUMENT_VA_LIST)
        static const int patchOffsetGetByIdSlowCaseCall = 25;
#else
        static const int patchOffsetGetByIdSlowCaseCall = 27;
#endif
        static const int patchOffsetOpCallCompareToJump = 6;

        static const int patchOffsetMethodCheckProtoObj = 11;
        static const int patchOffsetMethodCheckProtoStruct = 18;
        static const int patchOffsetMethodCheckPutFunction = 29;
#else
#error "JSVALUE32_64 not supported on this platform."
#endif

#else // USE(JSVALUE32_64)
        void emitGetVirtualRegister(int src, RegisterID dst);
        void emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2);
        void emitPutVirtualRegister(unsigned dst, RegisterID from = regT0);

        int32_t getConstantOperandImmediateInt(unsigned src);

        void emitGetVariableObjectRegister(RegisterID variableObject, int index, RegisterID dst);
        void emitPutVariableObjectRegister(RegisterID src, RegisterID variableObject, int index);
        
        void killLastResultRegister();

        Jump emitJumpIfJSCell(RegisterID);
        Jump emitJumpIfBothJSCells(RegisterID, RegisterID, RegisterID);
        void emitJumpSlowCaseIfJSCell(RegisterID);
        Jump emitJumpIfNotJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID, int VReg);
#if USE(JSVALUE64)
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
        JIT::Jump emitJumpIfImmediateInteger(RegisterID);
        JIT::Jump emitJumpIfNotImmediateInteger(RegisterID);
        JIT::Jump emitJumpIfNotImmediateIntegers(RegisterID, RegisterID, RegisterID);
        void emitJumpSlowCaseIfNotImmediateInteger(RegisterID);
        void emitJumpSlowCaseIfNotImmediateIntegers(RegisterID, RegisterID, RegisterID);

#if !USE(JSVALUE64)
        void emitFastArithDeTagImmediate(RegisterID);
        Jump emitFastArithDeTagImmediateJumpIfZero(RegisterID);
#endif
        void emitFastArithReTagImmediate(RegisterID src, RegisterID dest);
        void emitFastArithImmToInt(RegisterID);
        void emitFastArithIntToImmNoCheck(RegisterID src, RegisterID dest);

        void emitTagAsBoolImmediate(RegisterID reg);
        void compileBinaryArithOp(OpcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi);
        void compileBinaryArithOpSlowCase(OpcodeID, Vector<SlowCaseEntry>::iterator&, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi);

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
        void compileGetByIdHotPath(int resultVReg, int baseVReg, Identifier* ident, unsigned propertyAccessInstructionIndex);
        void compileGetByIdSlowCase(int resultVReg, int baseVReg, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, bool isMethodCheck = false);
#endif
        void compileGetDirectOffset(RegisterID base, RegisterID result, Structure* structure, size_t cachedOffset);
        void compileGetDirectOffset(JSObject* base, RegisterID temp, RegisterID result, size_t cachedOffset);
        void compilePutDirectOffset(RegisterID base, RegisterID value, Structure* structure, size_t cachedOffset);

#if PLATFORM(X86_64)
        // These architecture specific value are used to enable patching - see comment on op_put_by_id.
        static const int patchOffsetPutByIdStructure = 10;
        static const int patchOffsetPutByIdExternalLoad = 20;
        static const int patchLengthPutByIdExternalLoad = 4;
        static const int patchOffsetPutByIdPropertyMapOffset = 31;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 10;
        static const int patchOffsetGetByIdBranchToSlowCase = 20;
        static const int patchOffsetGetByIdExternalLoad = 20;
        static const int patchLengthGetByIdExternalLoad = 4;
        static const int patchOffsetGetByIdPropertyMapOffset = 31;
        static const int patchOffsetGetByIdPutResult = 31;
#if ENABLE(OPCODE_SAMPLING)
        static const int patchOffsetGetByIdSlowCaseCall = 63;
#else
        static const int patchOffsetGetByIdSlowCaseCall = 41;
#endif
        static const int patchOffsetOpCallCompareToJump = 9;

        static const int patchOffsetMethodCheckProtoObj = 20;
        static const int patchOffsetMethodCheckProtoStruct = 30;
        static const int patchOffsetMethodCheckPutFunction = 50;
#elif PLATFORM(X86)
        // These architecture specific value are used to enable patching - see comment on op_put_by_id.
        static const int patchOffsetPutByIdStructure = 7;
        static const int patchOffsetPutByIdExternalLoad = 13;
        static const int patchLengthPutByIdExternalLoad = 3;
        static const int patchOffsetPutByIdPropertyMapOffset = 22;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 7;
        static const int patchOffsetGetByIdBranchToSlowCase = 13;
        static const int patchOffsetGetByIdExternalLoad = 13;
        static const int patchLengthGetByIdExternalLoad = 3;
        static const int patchOffsetGetByIdPropertyMapOffset = 22;
        static const int patchOffsetGetByIdPutResult = 22;
#if ENABLE(OPCODE_SAMPLING) && USE(JIT_STUB_ARGUMENT_VA_LIST)
        static const int patchOffsetGetByIdSlowCaseCall = 31;
#elif ENABLE(OPCODE_SAMPLING)
        static const int patchOffsetGetByIdSlowCaseCall = 33;
#elif USE(JIT_STUB_ARGUMENT_VA_LIST)
        static const int patchOffsetGetByIdSlowCaseCall = 21;
#else
        static const int patchOffsetGetByIdSlowCaseCall = 23;
#endif
        static const int patchOffsetOpCallCompareToJump = 6;

        static const int patchOffsetMethodCheckProtoObj = 11;
        static const int patchOffsetMethodCheckProtoStruct = 18;
        static const int patchOffsetMethodCheckPutFunction = 29;
#elif PLATFORM(ARM_THUMB2)
        // These architecture specific value are used to enable patching - see comment on op_put_by_id.
        static const int patchOffsetPutByIdStructure = 10;
        static const int patchOffsetPutByIdExternalLoad = 20;
        static const int patchLengthPutByIdExternalLoad = 12;
        static const int patchOffsetPutByIdPropertyMapOffset = 40;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 10;
        static const int patchOffsetGetByIdBranchToSlowCase = 20;
        static const int patchOffsetGetByIdExternalLoad = 20;
        static const int patchLengthGetByIdExternalLoad = 12;
        static const int patchOffsetGetByIdPropertyMapOffset = 40;
        static const int patchOffsetGetByIdPutResult = 44;
#if ENABLE(OPCODE_SAMPLING)
        static const int patchOffsetGetByIdSlowCaseCall = 0; // FIMXE
#else
        static const int patchOffsetGetByIdSlowCaseCall = 28;
#endif
        static const int patchOffsetOpCallCompareToJump = 10;

        static const int patchOffsetMethodCheckProtoObj = 18;
        static const int patchOffsetMethodCheckProtoStruct = 28;
        static const int patchOffsetMethodCheckPutFunction = 46;
#elif PLATFORM(ARM_TRADITIONAL)
        // These architecture specific value are used to enable patching - see comment on op_put_by_id.
        static const int patchOffsetPutByIdStructure = 4;
        static const int patchOffsetPutByIdExternalLoad = 16;
        static const int patchLengthPutByIdExternalLoad = 4;
        static const int patchOffsetPutByIdPropertyMapOffset = 20;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 4;
        static const int patchOffsetGetByIdBranchToSlowCase = 16;
        static const int patchOffsetGetByIdExternalLoad = 16;
        static const int patchLengthGetByIdExternalLoad = 4;
        static const int patchOffsetGetByIdPropertyMapOffset = 20;
        static const int patchOffsetGetByIdPutResult = 28;
#if ENABLE(OPCODE_SAMPLING)
        #error "OPCODE_SAMPLING is not yet supported"
#else
        static const int patchOffsetGetByIdSlowCaseCall = 36;
#endif
        static const int patchOffsetOpCallCompareToJump = 12;

        static const int patchOffsetMethodCheckProtoObj = 12;
        static const int patchOffsetMethodCheckProtoStruct = 20;
        static const int patchOffsetMethodCheckPutFunction = 32;
#endif
#endif // USE(JSVALUE32_64)

#if PLATFORM(ARM_TRADITIONAL)
        // sequenceOpCall
        static const int sequenceOpCallInstructionSpace = 12;
        static const int sequenceOpCallConstantSpace = 2;
        // sequenceMethodCheck
        static const int sequenceMethodCheckInstructionSpace = 40;
        static const int sequenceMethodCheckConstantSpace = 6;
        // sequenceGetByIdHotPath
        static const int sequenceGetByIdHotPathInstructionSpace = 28;
        static const int sequenceGetByIdHotPathConstantSpace = 3;
        // sequenceGetByIdSlowCase
        static const int sequenceGetByIdSlowCaseInstructionSpace = 40;
        static const int sequenceGetByIdSlowCaseConstantSpace = 2;
        // sequencePutById
        static const int sequencePutByIdInstructionSpace = 28;
        static const int sequencePutByIdConstantSpace = 3;
#endif

#if defined(ASSEMBLER_HAS_CONSTANT_POOL) && ASSEMBLER_HAS_CONSTANT_POOL
#define BEGIN_UNINTERRUPTED_SEQUENCE(name) beginUninterruptedSequence(name ## InstructionSpace, name ## ConstantSpace)
#define END_UNINTERRUPTED_SEQUENCE(name) endUninterruptedSequence(name ## InstructionSpace, name ## ConstantSpace)

        void beginUninterruptedSequence(int, int);
        void endUninterruptedSequence(int, int);

#else
#define BEGIN_UNINTERRUPTED_SEQUENCE(name)
#define END_UNINTERRUPTED_SEQUENCE(name)
#endif

        void emit_op_add(Instruction*);
        void emit_op_bitand(Instruction*);
        void emit_op_bitnot(Instruction*);
        void emit_op_bitor(Instruction*);
        void emit_op_bitxor(Instruction*);
        void emit_op_call(Instruction*);
        void emit_op_call_eval(Instruction*);
        void emit_op_call_varargs(Instruction*);
        void emit_op_catch(Instruction*);
        void emit_op_construct(Instruction*);
        void emit_op_construct_verify(Instruction*);
        void emit_op_convert_this(Instruction*);
        void emit_op_create_arguments(Instruction*);
        void emit_op_debug(Instruction*);
        void emit_op_del_by_id(Instruction*);
        void emit_op_div(Instruction*);
        void emit_op_end(Instruction*);
        void emit_op_enter(Instruction*);
        void emit_op_enter_with_activation(Instruction*);
        void emit_op_eq(Instruction*);
        void emit_op_eq_null(Instruction*);
        void emit_op_get_by_id(Instruction*);
        void emit_op_get_by_val(Instruction*);
        void emit_op_get_global_var(Instruction*);
        void emit_op_get_scoped_var(Instruction*);
        void emit_op_init_arguments(Instruction*);
        void emit_op_instanceof(Instruction*);
        void emit_op_jeq_null(Instruction*);
        void emit_op_jfalse(Instruction*);
        void emit_op_jmp(Instruction*);
        void emit_op_jmp_scopes(Instruction*);
        void emit_op_jneq_null(Instruction*);
        void emit_op_jneq_ptr(Instruction*);
        void emit_op_jnless(Instruction*);
        void emit_op_jnlesseq(Instruction*);
        void emit_op_jsr(Instruction*);
        void emit_op_jtrue(Instruction*);
        void emit_op_load_varargs(Instruction*);
        void emit_op_loop(Instruction*);
        void emit_op_loop_if_less(Instruction*);
        void emit_op_loop_if_lesseq(Instruction*);
        void emit_op_loop_if_true(Instruction*);
        void emit_op_lshift(Instruction*);
        void emit_op_method_check(Instruction*);
        void emit_op_mod(Instruction*);
        void emit_op_mov(Instruction*);
        void emit_op_mul(Instruction*);
        void emit_op_negate(Instruction*);
        void emit_op_neq(Instruction*);
        void emit_op_neq_null(Instruction*);
        void emit_op_new_array(Instruction*);
        void emit_op_new_error(Instruction*);
        void emit_op_new_func(Instruction*);
        void emit_op_new_func_exp(Instruction*);
        void emit_op_new_object(Instruction*);
        void emit_op_new_regexp(Instruction*);
        void emit_op_next_pname(Instruction*);
        void emit_op_not(Instruction*);
        void emit_op_nstricteq(Instruction*);
        void emit_op_pop_scope(Instruction*);
        void emit_op_post_dec(Instruction*);
        void emit_op_post_inc(Instruction*);
        void emit_op_pre_dec(Instruction*);
        void emit_op_pre_inc(Instruction*);
        void emit_op_profile_did_call(Instruction*);
        void emit_op_profile_will_call(Instruction*);
        void emit_op_push_new_scope(Instruction*);
        void emit_op_push_scope(Instruction*);
        void emit_op_put_by_id(Instruction*);
        void emit_op_put_by_index(Instruction*);
        void emit_op_put_by_val(Instruction*);
        void emit_op_put_getter(Instruction*);
        void emit_op_put_global_var(Instruction*);
        void emit_op_put_scoped_var(Instruction*);
        void emit_op_put_setter(Instruction*);
        void emit_op_resolve(Instruction*);
        void emit_op_resolve_base(Instruction*);
        void emit_op_resolve_global(Instruction*);
        void emit_op_resolve_skip(Instruction*);
        void emit_op_resolve_with_base(Instruction*);
        void emit_op_ret(Instruction*);
        void emit_op_rshift(Instruction*);
        void emit_op_sret(Instruction*);
        void emit_op_strcat(Instruction*);
        void emit_op_stricteq(Instruction*);
        void emit_op_sub(Instruction*);
        void emit_op_switch_char(Instruction*);
        void emit_op_switch_imm(Instruction*);
        void emit_op_switch_string(Instruction*);
        void emit_op_tear_off_activation(Instruction*);
        void emit_op_tear_off_arguments(Instruction*);
        void emit_op_throw(Instruction*);
        void emit_op_to_jsnumber(Instruction*);
        void emit_op_to_primitive(Instruction*);
        void emit_op_unexpected_load(Instruction*);

        void emitSlow_op_add(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitand(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitnot(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitor(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_bitxor(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_eval(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_varargs(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_construct(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_construct_verify(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_convert_this(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_div(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_eq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_instanceof(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jfalse(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnless(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnlesseq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jtrue(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_if_less(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_if_lesseq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_if_true(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_lshift(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_method_check(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mod(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mul(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_negate(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_neq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_not(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_nstricteq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_post_dec(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_post_inc(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_pre_dec(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_pre_inc(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_resolve_global(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_rshift(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_stricteq(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_sub(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_to_jsnumber(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_to_primitive(Instruction*, Vector<SlowCaseEntry>::iterator&);

        /* These functions are deprecated: Please use JITStubCall instead. */
        void emitPutJITStubArg(RegisterID src, unsigned argumentNumber);
#if USE(JSVALUE32_64)
        void emitPutJITStubArg(RegisterID tag, RegisterID payload, unsigned argumentNumber);
        void emitPutJITStubArgFromVirtualRegister(unsigned src, unsigned argumentNumber, RegisterID scratch1, RegisterID scratch2);
#else
        void emitPutJITStubArgFromVirtualRegister(unsigned src, unsigned argumentNumber, RegisterID scratch);
#endif
        void emitPutJITStubArgConstant(unsigned value, unsigned argumentNumber);
        void emitPutJITStubArgConstant(void* value, unsigned argumentNumber);
        void emitGetJITStubArg(unsigned argumentNumber, RegisterID dst);

        void emitInitRegister(unsigned dst);

        void emitPutToCallFrameHeader(RegisterID from, RegisterFile::CallFrameHeaderEntry entry);
        void emitPutImmediateToCallFrameHeader(void* value, RegisterFile::CallFrameHeaderEntry entry);
        void emitGetFromCallFrameHeaderPtr(RegisterFile::CallFrameHeaderEntry entry, RegisterID to, RegisterID from = callFrameRegister);
        void emitGetFromCallFrameHeader32(RegisterFile::CallFrameHeaderEntry entry, RegisterID to, RegisterID from = callFrameRegister);

        JSValue getConstantOperand(unsigned src);
        bool isOperandConstantImmediateInt(unsigned src);

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

        Jump checkStructure(RegisterID reg, Structure* structure);

        void restoreArgumentReference();
        void restoreArgumentReferenceForTrampoline();

        Call emitNakedCall(CodePtr function = CodePtr());

        void preserveReturnAddressAfterCall(RegisterID);
        void restoreReturnAddressBeforeReturn(RegisterID);
        void restoreReturnAddressBeforeReturn(Address);

        void emitTimeoutCheck();
#ifndef NDEBUG
        void printBytecodeOperandTypes(unsigned src1, unsigned src2);
#endif

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
        Vector<MethodCallCompilationInfo> m_methodCallCompilationInfo;
        Vector<JumpTable> m_jmpTable;

        unsigned m_bytecodeIndex;
        Vector<JSRInfo> m_jsrSites;
        Vector<SlowCaseEntry> m_slowCases;
        Vector<SwitchRecord> m_switches;

        unsigned m_propertyAccessInstructionIndex;
        unsigned m_globalResolveInfoIndex;
        unsigned m_callLinkInfoIndex;

#if USE(JSVALUE32_64)
        unsigned m_jumpTargetIndex;
        unsigned m_mappedBytecodeIndex;
        unsigned m_mappedVirtualRegisterIndex;
        RegisterID m_mappedTag;
        RegisterID m_mappedPayload;
#else
        int m_lastResultBytecodeRegister;
        unsigned m_jumpTargetsPosition;
#endif

#ifndef NDEBUG
#if defined(ASSEMBLER_HAS_CONSTANT_POOL) && ASSEMBLER_HAS_CONSTANT_POOL
        Label m_uninterruptedInstructionSequenceBegin;
        int m_uninterruptedConstantSequenceBegin;
#endif
#endif
    } JIT_CLASS_ALIGNMENT;
} // namespace JSC

#endif // ENABLE(JIT)

#endif // JIT_h
