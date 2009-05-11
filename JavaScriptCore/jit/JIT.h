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

#define WTF_USE_CTI_REPATCH_PIC 1

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

        void compileFastArith_op_add(Instruction*);
        void compileFastArith_op_sub(Instruction*);
        void compileFastArith_op_mul(Instruction*);
        void compileFastArith_op_mod(unsigned result, unsigned op1, unsigned op2);
        void compileFastArith_op_bitand(unsigned result, unsigned op1, unsigned op2);
        void compileFastArith_op_lshift(unsigned result, unsigned op1, unsigned op2);
        void compileFastArith_op_rshift(unsigned result, unsigned op1, unsigned op2);
        void compileFastArith_op_jnless(unsigned op1, unsigned op2, unsigned target);
        void compileFastArith_op_jnlesseq(unsigned op1, unsigned op2, unsigned target);
        void compileFastArith_op_pre_inc(unsigned srcDst);
        void compileFastArith_op_pre_dec(unsigned srcDst);
        void compileFastArith_op_post_inc(unsigned result, unsigned srcDst);
        void compileFastArith_op_post_dec(unsigned result, unsigned srcDst);
        void compileFastArithSlow_op_add(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_sub(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_mul(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_mod(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_bitand(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_lshift(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_rshift(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_jnless(unsigned op1, unsigned op2, unsigned target, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_jnlesseq(unsigned op1, unsigned op2, unsigned target, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_pre_inc(unsigned srcDst, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_pre_dec(unsigned srcDst, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_post_inc(unsigned result, unsigned srcDst, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_post_dec(unsigned result, unsigned srcDst, Vector<SlowCaseEntry>::iterator&);
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

        void emitPutCTIParam(void* value, unsigned name);
        void emitPutCTIParam(RegisterID from, unsigned name);
        void emitGetCTIParam(unsigned name, RegisterID to);

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

#if ENABLE(CODEBLOCK_SAMPLING)
        void sampleCodeBlock(CodeBlock* codeBlock)
        {
#if PLATFORM(X86_64)
            move(ImmPtr(m_interpreter->sampler()->codeBlockSlot()), X86::ecx);
            storePtr(ImmPtr(codeBlock), X86::ecx);
#else
            storePtr(ImmPtr(codeBlock), m_interpreter->sampler()->codeBlockSlot());
#endif
        }
#else
        void sampleCodeBlock(CodeBlock*) {}
#endif

#if ENABLE(OPCODE_SAMPLING)
        void sampleInstruction(Instruction* instruction, bool inHostFunction=false)
        {
#if PLATFORM(X86_64)
            move(ImmPtr(m_interpreter->sampler()->sampleSlot()), X86::ecx);
            storePtr(ImmPtr(m_interpreter->sampler()->encodeSample(instruction, inHostFunction)), X86::ecx);
#else
            storePtr(ImmPtr(m_interpreter->sampler()->encodeSample(instruction, inHostFunction)), m_interpreter->sampler()->sampleSlot());
#endif
        }
#else
        void sampleInstruction(Instruction*, bool) {}
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
    };

    class JITStubCall {
    public:
        JITStubCall(JIT* jit, JSObject* (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Default)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, JSPropertyNameIterator* (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Default)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, void* (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Default)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, int (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Default)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, void (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Void)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, VoidPtrPair (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Pair)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        // Arguments are added first to last.

        template <typename T> void addArgument(T argument)
        {
            m_jit->poke(argument, m_argumentIndex);
            ++m_argumentIndex;
        }

        void addArgument(unsigned src, JIT::RegisterID scratchRegister) // src is a virtual register.
        {
            if (m_jit->m_codeBlock->isConstantRegisterIndex(src))
                addArgument(JIT::ImmPtr(JSValue::encode(m_jit->m_codeBlock->getConstant(src))));
            else {
                m_jit->loadPtr(JIT::Address(JIT::callFrameRegister, src * sizeof(Register)), scratchRegister);
                addArgument(scratchRegister);
            }
            m_jit->killLastResultRegister();
        }

        JIT::Call call()
        {
            ASSERT(m_jit->m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

#if ENABLE(OPCODE_SAMPLING)
            m_jit->sampleInstruction(m_jit->m_codeBlock->instructions().begin() + m_jit->m_bytecodeIndex, true);
#endif

            m_jit->restoreArgumentReference();
            JIT::Call call = m_jit->call();
            m_jit->m_calls.append(CallRecord(call, m_jit->m_bytecodeIndex, m_stub));

#if ENABLE(OPCODE_SAMPLING)
            m_jit->sampleInstruction(m_jit->m_codeBlock->instructions().begin() + m_jit->m_bytecodeIndex, false);
#endif

            m_jit->killLastResultRegister();
            return call;
        }

        JIT::Call call(unsigned dst) // dst is a virtual register.
        {
            JIT::Call call = this->call();
            m_jit->emitPutVirtualRegister(dst);
            return call;
        }

        // Used for stubs that return two values encoded as a VoidPtrPair.
        JIT::Call call(unsigned dst1, unsigned dst2) // dst1 and dst1 are virtual registers.
        {
            JIT::Call call = this->call();
            m_jit->emitPutVirtualRegister(dst1);
            m_jit->emitPutVirtualRegister(dst2, JIT::regT1);
            return call;
        }

        JIT::Call call(JIT::RegisterID dst)
        {
            JIT::Call call = this->call();
            if (dst != JIT::returnValueRegister)
                m_jit->move(JIT::returnValueRegister, dst);
            return call;
        }

    private:
        JIT* m_jit;
        void* m_stub;
        enum { Default, Void, Pair } m_returnType;
        size_t m_argumentIndex;
    };

    class CallEvalJITStub : public JITStubCall {
    public:
        CallEvalJITStub(JIT* jit, Instruction* instruction)
            : JITStubCall(jit, JITStubs::cti_op_call_eval)
        {
            int callee = instruction[2].u.operand;
            int argCount = instruction[3].u.operand;
            int registerOffset = instruction[4].u.operand;

            addArgument(callee, JIT::regT2);
            addArgument(JIT::Imm32(registerOffset));
            addArgument(JIT::Imm32(argCount));
        }
    };
}

#endif // ENABLE(JIT)

#endif // JIT_h
