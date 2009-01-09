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

#include "Interpreter.h"
#include "Opcode.h"
#include "RegisterFile.h"
#include "MacroAssembler.h"
#include "Profiler.h"
#include <wtf/AlwaysInline.h>
#include <wtf/Vector.h>

#define STUB_ARGS_offset 0x0C
#define STUB_ARGS_code (STUB_ARGS_offset)
#define STUB_ARGS_registerFile (STUB_ARGS_offset + 1)
#define STUB_ARGS_callFrame (STUB_ARGS_offset + 2)
#define STUB_ARGS_exception (STUB_ARGS_offset + 3)
#define STUB_ARGS_profilerReference (STUB_ARGS_offset + 4)
#define STUB_ARGS_globalData (STUB_ARGS_offset + 5)

#define ARG_callFrame static_cast<CallFrame*>(ARGS[STUB_ARGS_callFrame])
#define ARG_registerFile static_cast<RegisterFile*>(ARGS[STUB_ARGS_registerFile])
#define ARG_exception static_cast<JSValuePtr*>(ARGS[STUB_ARGS_exception])
#define ARG_profilerReference static_cast<Profiler**>(ARGS[STUB_ARGS_profilerReference])
#define ARG_globalData static_cast<JSGlobalData*>(ARGS[STUB_ARGS_globalData])

#define ARG_setCallFrame(newCallFrame) (ARGS[STUB_ARGS_callFrame] = (newCallFrame))

#define ARG_src1 JSValuePtr::decode(static_cast<JSValueEncodedAsPointer*>(ARGS[1]))
#define ARG_src2 JSValuePtr::decode(static_cast<JSValueEncodedAsPointer*>(ARGS[2]))
#define ARG_src3 JSValuePtr::decode(static_cast<JSValueEncodedAsPointer*>(ARGS[3]))
#define ARG_src4 JSValuePtr::decode(static_cast<JSValueEncodedAsPointer*>(ARGS[4]))
#define ARG_src5 JSValuePtr::decode(static_cast<JSValueEncodedAsPointer*>(ARGS[5]))
#define ARG_id1 static_cast<Identifier*>(ARGS[1])
#define ARG_id2 static_cast<Identifier*>(ARGS[2])
#define ARG_id3 static_cast<Identifier*>(ARGS[3])
#define ARG_id4 static_cast<Identifier*>(ARGS[4])
#define ARG_int1 static_cast<int32_t>(reinterpret_cast<intptr_t>(ARGS[1]))
#define ARG_int2 static_cast<int32_t>(reinterpret_cast<intptr_t>(ARGS[2]))
#define ARG_int3 static_cast<int32_t>(reinterpret_cast<intptr_t>(ARGS[3]))
#define ARG_int4 static_cast<int32_t>(reinterpret_cast<intptr_t>(ARGS[4]))
#define ARG_int5 static_cast<int32_t>(reinterpret_cast<intptr_t>(ARGS[5]))
#define ARG_int6 static_cast<int32_t>(reinterpret_cast<intptr_t>(ARGS[6]))
#define ARG_func1 static_cast<FuncDeclNode*>(ARGS[1])
#define ARG_funcexp1 static_cast<FuncExprNode*>(ARGS[1])
#define ARG_regexp1 static_cast<RegExp*>(ARGS[1])
#define ARG_pni1 static_cast<JSPropertyNameIterator*>(ARGS[1])
#define ARG_returnAddress2 static_cast<void*>(ARGS[2])
#define ARG_codeBlock4 static_cast<CodeBlock*>(ARGS[4])

#define STUB_RETURN_ADDRESS_SLOT (ARGS[-1])

namespace JSC {

    class CodeBlock;
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

    typedef JSValueEncodedAsPointer* (JIT_STUB *CTIHelper_j)(STUB_ARGS);
    typedef JSObject* (JIT_STUB *CTIHelper_o)(STUB_ARGS);
    typedef JSPropertyNameIterator* (JIT_STUB *CTIHelper_p)(STUB_ARGS);
    typedef void (JIT_STUB *CTIHelper_v)(STUB_ARGS);
    typedef void* (JIT_STUB *CTIHelper_s)(STUB_ARGS);
    typedef int (JIT_STUB *CTIHelper_b)(STUB_ARGS);
    typedef VoidPtrPair (JIT_STUB *CTIHelper_2)(STUB_ARGS);

    struct CallRecord {
        MacroAssembler::Jump from;
        unsigned bytecodeIndex;
        void* to;

        CallRecord()
        {
        }

        CallRecord(MacroAssembler::Jump from, unsigned bytecodeIndex, void* to = 0)
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
        MacroAssembler::Jump callReturnLocation;
        MacroAssembler::Label hotPathBegin;
    };

    struct StructureStubCompilationInfo {
        MacroAssembler::DataLabelPtr hotPathBegin;
        MacroAssembler::Jump hotPathOther;
        MacroAssembler::Jump callReturnLocation;
        MacroAssembler::Label coldPathOther;
    };

    extern "C" {
        JSValueEncodedAsPointer* ctiTrampoline(
#if PLATFORM(X86_64)
            // FIXME: (bug #22910) this will force all arguments onto the stack (regparm(0) does not appear to have any effect).
            // We can allow register passing here, and move the writes of these values into the trampoline.
            void*, void*, void*, void*, void*, void*,
#endif
            void* code, RegisterFile*, CallFrame*, JSValuePtr* exception, Profiler**, JSGlobalData*);
        void ctiVMThrowTrampoline();
    };

    void ctiSetReturnAddress(void** where, void* what);
    void ctiPatchCallByReturnAddress(void* where, void* what);

    class JIT : private MacroAssembler {
        using MacroAssembler::Jump;
        using MacroAssembler::JumpList;
        using MacroAssembler::Label;

#if PLATFORM(X86_64)
        static const RegisterID timeoutCheckRegister = X86::r12;
        static const RegisterID callFrameRegister = X86::r13;
#else
        static const RegisterID timeoutCheckRegister = X86::esi;
        static const RegisterID callFrameRegister = X86::edi;
#endif

        static const int patchGetByIdDefaultStructure = -1;
        // Magic number - initial offset cannot be representable as a signed 8bit value, or the X86Assembler
        // will compress the displacement, and we may not be able to fit a patched offset.
        static const int patchGetByIdDefaultOffset = 256;

#if USE(JIT_STUB_ARGUMENT_REGISTER)
#if PLATFORM(X86_64)
        static const int ctiArgumentInitSize = 3;
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
        static const int patchOffsetPutByIdPropertyMapOffset = 31;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 10;
        static const int patchOffsetGetByIdBranchToSlowCase = 20;
        static const int patchOffsetGetByIdPropertyMapOffset = 31;
        static const int patchOffsetGetByIdPutResult = 31;
#if ENABLE(OPCODE_SAMPLING)
        static const int patchOffsetGetByIdSlowCaseCall = 40 + ctiArgumentInitSize;
#else
        static const int patchOffsetGetByIdSlowCaseCall = 30 + ctiArgumentInitSize;
#endif
        static const int patchOffsetOpCallCompareToJump = 9;
#else
        // These architecture specific value are used to enable patching - see comment on op_put_by_id.
        static const int patchOffsetPutByIdStructure = 7;
        static const int patchOffsetPutByIdPropertyMapOffset = 22;
        // These architecture specific value are used to enable patching - see comment on op_get_by_id.
        static const int patchOffsetGetByIdStructure = 7;
        static const int patchOffsetGetByIdBranchToSlowCase = 13;
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

        static void compileGetByIdSelf(JSGlobalData* globalData, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdSelf(stubInfo, structure, cachedOffset, returnAddress);
        }

        static void compileGetByIdProto(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdProto(stubInfo, structure, prototypeStructure, cachedOffset, returnAddress, callFrame);
        }

#if USE(CTI_REPATCH_PIC)
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
#endif

        static void compileGetByIdChain(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdChain(stubInfo, structure, chain, count, cachedOffset, returnAddress, callFrame);
        }

        static void compilePutByIdReplace(JSGlobalData* globalData, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompilePutByIdReplace(stubInfo, structure, cachedOffset, returnAddress);
        }
        
        static void compilePutByIdTransition(JSGlobalData* globalData, CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompilePutByIdTransition(stubInfo, oldStructure, newStructure, cachedOffset, chain, returnAddress);
        }

        static void compileCTIMachineTrampolines(JSGlobalData* globalData)
        {
            JIT jit(globalData);
            jit.privateCompileCTIMachineTrampolines();
        }

        static void patchGetByIdSelf(StructureStubInfo*, Structure*, size_t cachedOffset, void* returnAddress);
        static void patchPutByIdReplace(StructureStubInfo*, Structure*, size_t cachedOffset, void* returnAddress);

        static void compilePatchGetArrayLength(JSGlobalData* globalData, CodeBlock* codeBlock, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            return jit.privateCompilePatchGetArrayLength(returnAddress);
        }

        static void linkCall(JSFunction* callee, CodeBlock* calleeCodeBlock, void* ctiCode, CallLinkInfo* callLinkInfo, int callerArgCount);
        static void unlinkCall(CallLinkInfo*);

        inline static JSValuePtr execute(void* code, RegisterFile* registerFile, CallFrame* callFrame, JSGlobalData* globalData, JSValuePtr* exception)
        {
            return JSValuePtr::decode(ctiTrampoline(
#if PLATFORM(X86_64)
                0, 0, 0, 0, 0, 0,
#endif
                code, registerFile, callFrame, exception, Profiler::enabledProfilerReference(), globalData));
        }

    private:
        JIT(JSGlobalData*, CodeBlock* = 0);

        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        void privateCompile();
        void privateCompileGetByIdSelf(StructureStubInfo*, Structure*, size_t cachedOffset, void* returnAddress);
        void privateCompileGetByIdProto(StructureStubInfo*, Structure*, Structure* prototypeStructure, size_t cachedOffset, void* returnAddress, CallFrame* callFrame);
#if USE(CTI_REPATCH_PIC)
        void privateCompileGetByIdSelfList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, size_t cachedOffset);
        void privateCompileGetByIdProtoList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, Structure* prototypeStructure, size_t cachedOffset, CallFrame* callFrame);
        void privateCompileGetByIdChainList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, StructureChain* chain, size_t count, size_t cachedOffset, CallFrame* callFrame);
#endif
        void privateCompileGetByIdChain(StructureStubInfo*, Structure*, StructureChain*, size_t count, size_t cachedOffset, void* returnAddress, CallFrame* callFrame);
        void privateCompilePutByIdReplace(StructureStubInfo*, Structure*, size_t cachedOffset, void* returnAddress);
        void privateCompilePutByIdTransition(StructureStubInfo*, Structure*, Structure*, size_t cachedOffset, StructureChain*, void* returnAddress);

        void privateCompileCTIMachineTrampolines();
        void privateCompilePatchGetArrayLength(void* returnAddress);

        void addSlowCase(Jump);
        void addJump(Jump, int);
        void emitJumpSlowToHot(Jump, int);

        void compileGetByIdHotPath(int resultVReg, int baseVReg, Identifier* ident, unsigned propertyAccessInstructionIndex);
        void compileGetByIdSlowCase(int resultVReg, int baseVReg, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex);
        void compilePutByIdHotPath(int baseVReg, Identifier* ident, int valueVReg, unsigned propertyAccessInstructionIndex);
        void compilePutByIdSlowCase(int baseVReg, Identifier* ident, int valueVReg, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex);
        void compileOpCall(OpcodeID, Instruction* instruction, unsigned callLinkInfoIndex);
        void compileOpCallInitializeCallFrame();
        void compileOpCallSetupArgs(Instruction*);
        void compileOpCallEvalSetupArgs(Instruction*);
        void compileOpCallSlowCase(Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex, OpcodeID opcodeID);
        void compileOpConstructSetupArgs(Instruction*);
        enum CompileOpStrictEqType { OpStrictEq, OpNStrictEq };
        void compileOpStrictEq(Instruction* instruction, CompileOpStrictEqType type);
        void putDoubleResultToJSNumberCellOrJSImmediate(X86Assembler::XMMRegisterID xmmSource, RegisterID jsNumberCell, unsigned dst, X86Assembler::JmpSrc* wroteJSNumberCell, X86Assembler::XMMRegisterID tempXmm, RegisterID tempReg1, RegisterID tempReg2);

        void compileFastArith_op_add(Instruction*);
        void compileFastArith_op_mul(Instruction*);
        void compileFastArith_op_mod(unsigned result, unsigned op1, unsigned op2);
        void compileFastArith_op_bitand(unsigned result, unsigned op1, unsigned op2);
        void compileFastArith_op_lshift(unsigned result, unsigned op1, unsigned op2);
        void compileFastArith_op_rshift(unsigned result, unsigned op1, unsigned op2);
        void compileFastArith_op_pre_inc(unsigned srcDst);
        void compileFastArith_op_pre_dec(unsigned srcDst);
        void compileFastArith_op_post_inc(unsigned result, unsigned srcDst);
        void compileFastArith_op_post_dec(unsigned result, unsigned srcDst);
        void compileFastArithSlow_op_add(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_mul(Instruction*, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_mod(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_bitand(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_lshift(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_rshift(unsigned result, unsigned op1, unsigned op2, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_pre_inc(unsigned srcDst, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_pre_dec(unsigned srcDst, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_post_inc(unsigned result, unsigned srcDst, Vector<SlowCaseEntry>::iterator&);
        void compileFastArithSlow_op_post_dec(unsigned result, unsigned srcDst, Vector<SlowCaseEntry>::iterator&);
        void compileBinaryArithOp(OpcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi);
        void compileBinaryArithOpSlowCase(OpcodeID, Vector<SlowCaseEntry>::iterator&, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi);

        void emitGetVirtualRegister(int src, RegisterID dst);
        void emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2);
        void emitPutVirtualRegister(unsigned dst, RegisterID from = X86::eax);

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
        void emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, RegisterID to);

        JSValuePtr getConstantOperand(unsigned src);
        int32_t getConstantOperandImmediateInt(unsigned src);
        bool isOperandConstantImmediateInt(unsigned src);

        Jump emitJumpIfJSCell(RegisterID);
        void emitJumpSlowCaseIfJSCell(RegisterID);
        Jump emitJumpIfNotJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID);
        void emitJumpSlowCaseIfNotJSCell(RegisterID, int VReg);

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

        JIT::Jump emitJumpIfImmNum(RegisterID);
        void emitJumpSlowCaseIfNotImmNum(RegisterID);
        void emitJumpSlowCaseIfNotImmNums(RegisterID, RegisterID, RegisterID);

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

        Jump emitNakedCall(RegisterID);
        Jump emitNakedCall(void* function);
        Jump emitCTICall_internal(void*);
        Jump emitCTICall(CTIHelper_j helper) { return emitCTICall_internal(reinterpret_cast<void*>(helper)); }
        Jump emitCTICall(CTIHelper_o helper) { return emitCTICall_internal(reinterpret_cast<void*>(helper)); }
        Jump emitCTICall(CTIHelper_p helper) { return emitCTICall_internal(reinterpret_cast<void*>(helper)); }
        Jump emitCTICall(CTIHelper_v helper) { return emitCTICall_internal(reinterpret_cast<void*>(helper)); }
        Jump emitCTICall(CTIHelper_s helper) { return emitCTICall_internal(reinterpret_cast<void*>(helper)); }
        Jump emitCTICall(CTIHelper_b helper) { return emitCTICall_internal(reinterpret_cast<void*>(helper)); }
        Jump emitCTICall(CTIHelper_2 helper) { return emitCTICall_internal(reinterpret_cast<void*>(helper)); }

        void emitGetVariableObjectRegister(RegisterID variableObject, int index, RegisterID dst);
        void emitPutVariableObjectRegister(RegisterID src, RegisterID variableObject, int index);
        
        void emitSlowScriptCheck();
#ifndef NDEBUG
        void printBytecodeOperandTypes(unsigned src1, unsigned src2);
#endif

        void killLastResultRegister();

        Interpreter* m_interpreter;
        JSGlobalData* m_globalData;
        CodeBlock* m_codeBlock;

        Vector<CallRecord> m_calls;
        Vector<Label> m_labels;
        Vector<PropertyStubCompilationInfo> m_propertyAccessCompilationInfo;
        Vector<StructureStubCompilationInfo> m_callStructureStubCompilationInfo;
        Vector<JumpTable> m_jmpTable;

        struct JSRInfo {
            DataLabelPtr storeLocation;
            Label target;

            JSRInfo(DataLabelPtr storeLocation, Label targetLocation)
                : storeLocation(storeLocation)
                , target(targetLocation)
            {
            }
        };

        unsigned m_bytecodeIndex;
        Vector<JSRInfo> m_jsrSites;
        Vector<SlowCaseEntry> m_slowCases;
        Vector<SwitchRecord> m_switches;

        int m_lastResultBytecodeRegister;
        unsigned m_jumpTargetsPosition;
    };
}

#endif // ENABLE(JIT)

#endif // JIT_h
