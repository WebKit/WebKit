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
#include "X86Assembler.h"
#include "Profiler.h"
#include <wtf/AlwaysInline.h>
#include <wtf/Vector.h>

#define CTI_ARGS_code 0x0C
#define CTI_ARGS_registerFile 0x0D
#define CTI_ARGS_callFrame 0x0E
#define CTI_ARGS_exception 0x0F
#define CTI_ARGS_profilerReference 0x10
#define CTI_ARGS_globalData 0x11

#define ARG_callFrame static_cast<CallFrame*>(ARGS[CTI_ARGS_callFrame])
#define ARG_registerFile static_cast<RegisterFile*>(ARGS[CTI_ARGS_registerFile])
#define ARG_exception static_cast<JSValue**>(ARGS[CTI_ARGS_exception])
#define ARG_profilerReference static_cast<Profiler**>(ARGS[CTI_ARGS_profilerReference])
#define ARG_globalData static_cast<JSGlobalData*>(ARGS[CTI_ARGS_globalData])

#define ARG_setCallFrame(newCallFrame) (ARGS[CTI_ARGS_callFrame] = (newCallFrame))

#define ARG_src1 static_cast<JSValue*>(ARGS[1])
#define ARG_src2 static_cast<JSValue*>(ARGS[2])
#define ARG_src3 static_cast<JSValue*>(ARGS[3])
#define ARG_src4 static_cast<JSValue*>(ARGS[4])
#define ARG_src5 static_cast<JSValue*>(ARGS[5])
#define ARG_id1 static_cast<Identifier*>(ARGS[1])
#define ARG_id2 static_cast<Identifier*>(ARGS[2])
#define ARG_id3 static_cast<Identifier*>(ARGS[3])
#define ARG_id4 static_cast<Identifier*>(ARGS[4])
#define ARG_int1 reinterpret_cast<intptr_t>(ARGS[1])
#define ARG_int2 reinterpret_cast<intptr_t>(ARGS[2])
#define ARG_int3 reinterpret_cast<intptr_t>(ARGS[3])
#define ARG_int4 reinterpret_cast<intptr_t>(ARGS[4])
#define ARG_int5 reinterpret_cast<intptr_t>(ARGS[5])
#define ARG_int6 reinterpret_cast<intptr_t>(ARGS[6])
#define ARG_func1 static_cast<FuncDeclNode*>(ARGS[1])
#define ARG_funcexp1 static_cast<FuncExprNode*>(ARGS[1])
#define ARG_registers1 static_cast<Register*>(ARGS[1])
#define ARG_regexp1 static_cast<RegExp*>(ARGS[1])
#define ARG_pni1 static_cast<JSPropertyNameIterator*>(ARGS[1])
#define ARG_instr1 static_cast<Instruction*>(ARGS[1])
#define ARG_instr2 static_cast<Instruction*>(ARGS[2])
#define ARG_instr3 static_cast<Instruction*>(ARGS[3])
#define ARG_instr4 static_cast<Instruction*>(ARGS[4])
#define ARG_instr5 static_cast<Instruction*>(ARGS[5])
#define ARG_instr6 static_cast<Instruction*>(ARGS[6])
#define ARG_returnAddress2 static_cast<void*>(ARGS[2])
#define ARG_codeBlock4 static_cast<CodeBlock*>(ARGS[4])

#define CTI_RETURN_ADDRESS_SLOT (ARGS[-1])

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

    typedef JSValue* (SFX_CALL *CTIHelper_j)(CTI_ARGS);
    typedef JSObject* (SFX_CALL *CTIHelper_o)(CTI_ARGS);
    typedef JSPropertyNameIterator* (SFX_CALL *CTIHelper_p)(CTI_ARGS);
    typedef void (SFX_CALL *CTIHelper_v)(CTI_ARGS);
    typedef void* (SFX_CALL *CTIHelper_s)(CTI_ARGS);
    typedef int (SFX_CALL *CTIHelper_b)(CTI_ARGS);
    typedef VoidPtrPair (SFX_CALL *CTIHelper_2)(CTI_ARGS);

    struct CallRecord {
        typedef X86Assembler::JmpSrc JmpSrc;

        JmpSrc from;
        void* to;
        unsigned bytecodeIndex;

        CallRecord()
        {
        }

        CallRecord(JmpSrc f, CTIHelper_j t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , bytecodeIndex(i)
        {
        }

        CallRecord(JmpSrc f, CTIHelper_o t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , bytecodeIndex(i)
        {
        }

        CallRecord(JmpSrc f, CTIHelper_p t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , bytecodeIndex(i)
        {
        }
        
        CallRecord(JmpSrc f, CTIHelper_v t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , bytecodeIndex(i)
        {
        }
        
        CallRecord(JmpSrc f, CTIHelper_s t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , bytecodeIndex(i)
        {
        }
        
        CallRecord(JmpSrc f, CTIHelper_b t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , bytecodeIndex(i)
        {
        }

        CallRecord(JmpSrc f, CTIHelper_2 t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , bytecodeIndex(i)
        {
        }

        CallRecord(JmpSrc f, unsigned i)
            : from(f)
            , to(0)
            , bytecodeIndex(i)
        {
        }
    };

    struct JmpTable {
        typedef X86Assembler::JmpSrc JmpSrc;

        JmpSrc from;
        unsigned to;
        
        JmpTable(JmpSrc f, unsigned t)
            : from(f)
            , to(t)
        {
        }
    };

    struct SlowCaseEntry {
        typedef X86Assembler::JmpSrc JmpSrc;

        JmpSrc from;
        unsigned to;
        unsigned hint;
        
        SlowCaseEntry(JmpSrc f, unsigned t, unsigned h = 0)
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

    struct StructureStubCompilationInfo {
        typedef X86Assembler::JmpSrc JmpSrc;
        typedef X86Assembler::JmpDst JmpDst;

        JmpSrc callReturnLocation;
        JmpDst hotPathBegin;
        JmpSrc hotPathOther;
        JmpDst coldPathOther;
    };

    extern "C" {
        JSValue* ctiTrampoline(void* code, RegisterFile*, CallFrame*, JSValue** exception, Profiler**, JSGlobalData*);
        void ctiVMThrowTrampoline();
    };

    void ctiSetReturnAddress(void** where, void* what);
    void ctiRepatchCallByReturnAddress(void* where, void* what);

    class JIT {
        typedef X86Assembler::RegisterID RegisterID;
        typedef X86Assembler::XMMRegisterID XMMRegisterID;
        typedef X86Assembler::JmpSrc JmpSrc;
        typedef X86Assembler::JmpDst JmpDst;

        static const int repatchGetByIdDefaultStructure = -1;
        // Magic number - initial offset cannot be representable as a signed 8bit value, or the X86Assembler
        // will compress the displacement, and we may not be able to fit a repatched offset.
        static const int repatchGetByIdDefaultOffset = 256;

#if USE(FAST_CALL_CTI_ARGUMENT)
        static const int ctiArgumentInitSize = 2;
#elif USE(CTI_ARGUMENT)
        static const int ctiArgumentInitSize = 4;
#else
        static const int ctiArgumentInitSize = 0;
#endif
        // These architecture specific value are used to enable repatching - see comment on op_put_by_id.
        static const int repatchOffsetPutByIdStructure = 7;
        static const int repatchOffsetPutByIdPropertyMapOffset = 22;
        // These architecture specific value are used to enable repatching - see comment on op_get_by_id.
        static const int repatchOffsetGetByIdStructure = 7;
        static const int repatchOffsetGetByIdBranchToSlowCase = 13;
        static const int repatchOffsetGetByIdPropertyMapOffset = 22;
#if ENABLE(OPCODE_SAMPLING)
        static const int repatchOffsetGetByIdSlowCaseCall = 27 + 4 + ctiArgumentInitSize;
#else
        static const int repatchOffsetGetByIdSlowCaseCall = 17 + 4 + ctiArgumentInitSize;
#endif
        static const int repatchOffsetOpCallCall = 6;

    public:
        static void compile(JSGlobalData* globalData, CodeBlock* codeBlock)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompile();
        }

        static void compileGetByIdSelf(JSGlobalData* globalData, CodeBlock* codeBlock, Structure* structure, size_t cachedOffset, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdSelf(structure, cachedOffset, returnAddress);
        }

        static void compileGetByIdProto(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdProto(structure, prototypeStructure, cachedOffset, returnAddress, callFrame);
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

        static void compileGetByIdChain(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompileGetByIdChain(structure, chain, count, cachedOffset, returnAddress, callFrame);
        }

        static void compilePutByIdReplace(JSGlobalData* globalData, CodeBlock* codeBlock, Structure* structure, size_t cachedOffset, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompilePutByIdReplace(structure, cachedOffset, returnAddress);
        }
        
        static void compilePutByIdTransition(JSGlobalData* globalData, CodeBlock* codeBlock, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            jit.privateCompilePutByIdTransition(oldStructure, newStructure, cachedOffset, chain, returnAddress);
        }

        static void compileCTIMachineTrampolines(JSGlobalData* globalData)
        {
            JIT jit(globalData);
            jit.privateCompileCTIMachineTrampolines();
        }
        static void freeCTIMachineTrampolines(Interpreter*);

        static void patchGetByIdSelf(CodeBlock* codeBlock, Structure* structure, size_t cachedOffset, void* returnAddress);
        static void patchPutByIdReplace(CodeBlock* codeBlock, Structure* structure, size_t cachedOffset, void* returnAddress);

        static void compilePatchGetArrayLength(JSGlobalData* globalData, CodeBlock* codeBlock, void* returnAddress)
        {
            JIT jit(globalData, codeBlock);
            return jit.privateCompilePatchGetArrayLength(returnAddress);
        }

        static void linkCall(JSFunction* callee, CodeBlock* calleeCodeBlock, void* ctiCode, CallLinkInfo* callLinkInfo, int callerArgCount);
        static void unlinkCall(CallLinkInfo*);

        inline static JSValue* execute(void* code, RegisterFile* registerFile, CallFrame* callFrame, JSGlobalData* globalData, JSValue** exception)
        {
            return ctiTrampoline(code, registerFile, callFrame, exception, Profiler::enabledProfilerReference(), globalData);
        }

    private:
        JIT(JSGlobalData*, CodeBlock* = 0);

        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        void privateCompile();
        void privateCompileGetByIdSelf(Structure*, size_t cachedOffset, void* returnAddress);
        void privateCompileGetByIdProto(Structure*, Structure* prototypeStructure, size_t cachedOffset, void* returnAddress, CallFrame* callFrame);
#if USE(CTI_REPATCH_PIC)
        void privateCompileGetByIdSelfList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, size_t cachedOffset);
        void privateCompileGetByIdProtoList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, Structure* prototypeStructure, size_t cachedOffset, CallFrame* callFrame);
        void privateCompileGetByIdChainList(StructureStubInfo*, PolymorphicAccessStructureList*, int, Structure*, StructureChain* chain, size_t count, size_t cachedOffset, CallFrame* callFrame);
#endif
        void privateCompileGetByIdChain(Structure*, StructureChain*, size_t count, size_t cachedOffset, void* returnAddress, CallFrame* callFrame);
        void privateCompilePutByIdReplace(Structure*, size_t cachedOffset, void* returnAddress);
        void privateCompilePutByIdTransition(Structure*, Structure*, size_t cachedOffset, StructureChain*, void* returnAddress);

        void privateCompileCTIMachineTrampolines();
        void privateCompilePatchGetArrayLength(void* returnAddress);

        void compileOpCall(OpcodeID, Instruction* instruction, unsigned i, unsigned callLinkInfoIndex);
        void compileOpCallInitializeCallFrame();
        void compileOpCallSetupArgs(Instruction*);
        void compileOpCallEvalSetupArgs(Instruction*);
        void compileOpConstructSetupArgs(Instruction*);
        enum CompileOpStrictEqType { OpStrictEq, OpNStrictEq };
        void compileOpStrictEq(Instruction* instruction, unsigned i, CompileOpStrictEqType type);
        void putDoubleResultToJSNumberCellOrJSImmediate(XMMRegisterID xmmSource, RegisterID jsNumberCell, unsigned dst, JmpSrc* wroteJSNumberCell,  XMMRegisterID tempXmm, RegisterID tempReg1, RegisterID tempReg2);
        void compileBinaryArithOp(OpcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi, unsigned i);
        void compileBinaryArithOpSlowCase(OpcodeID, Vector<SlowCaseEntry>::iterator& iter, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi, unsigned i);

        void emitGetVirtualRegister(int src, RegisterID dst, unsigned i);
        void emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2, unsigned i);
        void emitPutVirtualRegister(unsigned dst, RegisterID from = X86::eax);

        void emitPutCTIArg(RegisterID src, unsigned offset);
        void emitPutCTIArgFromVirtualRegister(unsigned src, unsigned offset, RegisterID scratch);
        void emitPutCTIArgConstant(unsigned value, unsigned offset);
        void emitGetCTIArg(unsigned offset, RegisterID dst);

        void emitInitRegister(unsigned dst);

        void emitPutCTIParam(void* value, unsigned name);
        void emitPutCTIParam(RegisterID from, unsigned name);
        void emitGetCTIParam(unsigned name, RegisterID to);

        void emitPutToCallFrameHeader(RegisterID from, RegisterFile::CallFrameHeaderEntry entry);
        void emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, RegisterID to);

        JSValue* getConstantImmediateNumericArg(unsigned src);
        unsigned getDeTaggedConstantImmediate(JSValue* imm);

        bool linkSlowCaseIfNotJSCell(const Vector<SlowCaseEntry>::iterator&, int vReg);
        void emitJumpSlowCaseIfNotJSCell(RegisterID, unsigned bytecodeIndex);
        void emitJumpSlowCaseIfNotJSCell(RegisterID, unsigned bytecodeIndex, int VReg);

        void emitJumpSlowCaseIfNotImmNum(RegisterID, unsigned bytecodeIndex);
        void emitJumpSlowCaseIfNotImmNums(RegisterID, RegisterID, unsigned bytecodeIndex);

        JmpSrc checkStructure(RegisterID reg, Structure* structure);

        void emitFastArithDeTagImmediate(RegisterID);
        JmpSrc emitFastArithDeTagImmediateJumpIfZero(RegisterID);
        void emitFastArithReTagImmediate(RegisterID);
        void emitFastArithPotentiallyReTagImmediate(RegisterID);
        void emitFastArithImmToInt(RegisterID);
        void emitFastArithIntToImmOrSlowCase(RegisterID, unsigned bytecodeIndex);
        void emitFastArithIntToImmNoCheck(RegisterID);

        void emitTagAsBoolImmediate(RegisterID reg);

        JmpSrc emitNakedCall(unsigned bytecodeIndex, RegisterID);
        JmpSrc emitNakedCall(unsigned bytecodeIndex, void* function);
        JmpSrc emitCTICall(unsigned bytecodeIndex, CTIHelper_j);
        JmpSrc emitCTICall(unsigned bytecodeIndex, CTIHelper_o);
        JmpSrc emitCTICall(unsigned bytecodeIndex, CTIHelper_p);
        JmpSrc emitCTICall(unsigned bytecodeIndex, CTIHelper_v);
        JmpSrc emitCTICall(unsigned bytecodeIndex, CTIHelper_s);
        JmpSrc emitCTICall(unsigned bytecodeIndex, CTIHelper_b);
        JmpSrc emitCTICall(unsigned bytecodeIndex, CTIHelper_2);

        void emitGetVariableObjectRegister(RegisterID variableObject, int index, RegisterID dst);
        void emitPutVariableObjectRegister(RegisterID src, RegisterID variableObject, int index);
        
        void emitSlowScriptCheck(unsigned bytecodeIndex);
#ifndef NDEBUG
        void printBytecodeOperandTypes(unsigned src1, unsigned src2);
#endif

        void killLastResultRegister();

        X86Assembler m_assembler;
        Interpreter* m_interpreter;
        JSGlobalData* m_globalData;
        CodeBlock* m_codeBlock;

        Vector<CallRecord> m_calls;
        Vector<JmpDst> m_labels;
        Vector<StructureStubCompilationInfo> m_propertyAccessCompilationInfo;
        Vector<StructureStubCompilationInfo> m_callStructureStubCompilationInfo;
        Vector<JmpTable> m_jmpTable;

        struct JSRInfo {
            JmpDst addrPosition;
            JmpDst target;

            JSRInfo(const JmpDst& storeLocation, const JmpDst& targetLocation)
                : addrPosition(storeLocation)
                , target(targetLocation)
            {
            }
        };

        Vector<JSRInfo> m_jsrSites;
        Vector<SlowCaseEntry> m_slowCases;
        Vector<SwitchRecord> m_switches;

        int m_lastResultBytecodeRegister;
        unsigned m_jumpTargetsPosition;
    };
}

#endif // ENABLE(JIT)

#endif // JIT_h
