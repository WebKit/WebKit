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

#ifndef CTI_h
#define CTI_h

#if ENABLE(CTI)

#define WTF_USE_CTI_REPATCH_PIC 1

#include "Machine.h"
#include "Opcode.h"
#include "RegisterFile.h"
#include <masm/X86Assembler.h>
#include <profiler/Profiler.h>
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

#if COMPILER(MSVC)
#define FASTCALL __fastcall
#elif COMPILER(GCC)
#define FASTCALL  __attribute__ ((fastcall))
#else
#error Need to support fastcall calling convention in this compiler
#endif

namespace JSC {

    class CodeBlock;
    class JSPropertyNameIterator;
    class BytecodeInterpreter;
    class Register;
    class RegisterFile;
    class ScopeChainNode;
    class SimpleJumpTable;
    class StringJumpTable;
    class StructureIDChain;

    struct CallLinkInfo;
    struct Instruction;
    struct OperandTypes;

    typedef JSValue* (SFX_CALL *CTIHelper_j)(CTI_ARGS);
    typedef JSObject* (SFX_CALL *CTIHelper_o)(CTI_ARGS);
    typedef JSPropertyNameIterator* (SFX_CALL *CTIHelper_p)(CTI_ARGS);
    typedef void (SFX_CALL *CTIHelper_v)(CTI_ARGS);
    typedef void* (SFX_CALL *CTIHelper_s)(CTI_ARGS);
    typedef int (SFX_CALL *CTIHelper_b)(CTI_ARGS);
    typedef VoidPtrPair (SFX_CALL *CTIHelper_2)(CTI_ARGS);

    struct CallRecord {
        X86Assembler::JmpSrc from;
        void* to;
        unsigned opcodeIndex;

        CallRecord()
        {
        }

        CallRecord(X86Assembler::JmpSrc f, CTIHelper_j t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , opcodeIndex(i)
        {
        }

        CallRecord(X86Assembler::JmpSrc f, CTIHelper_o t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , opcodeIndex(i)
        {
        }

        CallRecord(X86Assembler::JmpSrc f, CTIHelper_p t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , opcodeIndex(i)
        {
        }
        
        CallRecord(X86Assembler::JmpSrc f, CTIHelper_v t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , opcodeIndex(i)
        {
        }
        
        CallRecord(X86Assembler::JmpSrc f, CTIHelper_s t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , opcodeIndex(i)
        {
        }
        
        CallRecord(X86Assembler::JmpSrc f, CTIHelper_b t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , opcodeIndex(i)
        {
        }

        CallRecord(X86Assembler::JmpSrc f, CTIHelper_2 t, unsigned i)
            : from(f)
            , to(reinterpret_cast<void*>(t))
            , opcodeIndex(i)
        {
        }

        CallRecord(X86Assembler::JmpSrc f, unsigned i)
            : from(f)
            , to(0)
            , opcodeIndex(i)
        {
        }
    };

    struct JmpTable {
        X86Assembler::JmpSrc from;
        unsigned to;
        
        JmpTable(X86Assembler::JmpSrc f, unsigned t)
            : from(f)
            , to(t)
        {
        }
    };

    struct SlowCaseEntry {
        X86Assembler::JmpSrc from;
        unsigned to;
        unsigned hint;
        
        SlowCaseEntry(X86Assembler::JmpSrc f, unsigned t, unsigned h = 0)
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

        unsigned opcodeIndex;
        unsigned defaultOffset;

        SwitchRecord(SimpleJumpTable* jumpTable, unsigned opcodeIndex, unsigned defaultOffset, Type type)
            : type(type)
            , opcodeIndex(opcodeIndex)
            , defaultOffset(defaultOffset)
        {
            this->jumpTable.simpleJumpTable = jumpTable;
        }

        SwitchRecord(StringJumpTable* jumpTable, unsigned opcodeIndex, unsigned defaultOffset)
            : type(String)
            , opcodeIndex(opcodeIndex)
            , defaultOffset(defaultOffset)
        {
            this->jumpTable.stringJumpTable = jumpTable;
        }
    };

    struct StructureStubCompilationInfo {
        X86Assembler::JmpSrc callReturnLocation;
        X86Assembler::JmpDst hotPathBegin;
        X86Assembler::JmpSrc hotPathOther;
        X86Assembler::JmpDst coldPathOther;
    };

    extern "C" {
        JSValue* ctiTrampoline(void* code, RegisterFile*, CallFrame*, JSValue** exception, Profiler**, JSGlobalData*);
        void ctiVMThrowTrampoline();
    };

    void ctiSetReturnAddress(void** where, void* what);
    void ctiRepatchCallByReturnAddress(void* where, void* what);

    class CTI {
        static const int repatchGetByIdDefaultStructureID = -1;
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
        static const int repatchOffsetPutByIdStructureID = 7;
        static const int repatchOffsetPutByIdPropertyMapOffset = 22;
        // These architecture specific value are used to enable repatching - see comment on op_get_by_id.
        static const int repatchOffsetGetByIdStructureID = 7;
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
            CTI cti(globalData, codeBlock);
            cti.privateCompile();
        }

#if ENABLE(WREC)
        static void* compileRegExp(BytecodeInterpreter*, const UString& pattern, unsigned* numSubpatterns_ptr, const char** error_ptr, bool ignoreCase = false, bool multiline = false);
#endif

        static void compileGetByIdSelf(JSGlobalData* globalData, CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress)
        {
            CTI cti(globalData, codeBlock);
            cti.privateCompileGetByIdSelf(structureID, cachedOffset, returnAddress);
        }

        static void compileGetByIdProto(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureID* structureID, StructureID* prototypeStructureID, size_t cachedOffset, void* returnAddress)
        {
            CTI cti(globalData, codeBlock);
            cti.privateCompileGetByIdProto(structureID, prototypeStructureID, cachedOffset, returnAddress, callFrame);
        }

        static void compileGetByIdChain(JSGlobalData* globalData, CallFrame* callFrame, CodeBlock* codeBlock, StructureID* structureID, StructureIDChain* chain, size_t count, size_t cachedOffset, void* returnAddress)
        {
            CTI cti(globalData, codeBlock);
            cti.privateCompileGetByIdChain(structureID, chain, count, cachedOffset, returnAddress, callFrame);
        }

        static void compilePutByIdReplace(JSGlobalData* globalData, CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress)
        {
            CTI cti(globalData, codeBlock);
            cti.privateCompilePutByIdReplace(structureID, cachedOffset, returnAddress);
        }
        
        static void compilePutByIdTransition(JSGlobalData* globalData, CodeBlock* codeBlock, StructureID* oldStructureID, StructureID* newStructureID, size_t cachedOffset, StructureIDChain* sIDC, void* returnAddress)
        {
            CTI cti(globalData, codeBlock);
            cti.privateCompilePutByIdTransition(oldStructureID, newStructureID, cachedOffset, sIDC, returnAddress);
        }

        static void compileCTIMachineTrampolines(JSGlobalData* globalData)
        {
            CTI cti(globalData);
            cti.privateCompileCTIMachineTrampolines();
        }
        static void freeCTIMachineTrampolines(BytecodeInterpreter*);

        static void patchGetByIdSelf(CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress);
        static void patchPutByIdReplace(CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress);

        static void compilePatchGetArrayLength(JSGlobalData* globalData, CodeBlock* codeBlock, void* returnAddress)
        {
            CTI cti(globalData, codeBlock);
            return cti.privateCompilePatchGetArrayLength(returnAddress);
        }

        static void linkCall(JSFunction* callee, CodeBlock* calleeCodeBlock, void* ctiCode, CallLinkInfo* callLinkInfo, int callerArgCount);
        static void unlinkCall(CallLinkInfo*);

        inline static JSValue* execute(void* code, RegisterFile* registerFile, CallFrame* callFrame, JSGlobalData* globalData, JSValue** exception)
        {
            return ctiTrampoline(code, registerFile, callFrame, exception, Profiler::enabledProfilerReference(), globalData);
        }

    private:
        CTI(JSGlobalData*, CodeBlock* = 0);

        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        void privateCompile();
        void privateCompileGetByIdSelf(StructureID*, size_t cachedOffset, void* returnAddress);
        void privateCompileGetByIdProto(StructureID*, StructureID* prototypeStructureID, size_t cachedOffset, void* returnAddress, CallFrame* callFrame);
        void privateCompileGetByIdChain(StructureID*, StructureIDChain*, size_t count, size_t cachedOffset, void* returnAddress, CallFrame* callFrame);
        void privateCompilePutByIdReplace(StructureID*, size_t cachedOffset, void* returnAddress);
        void privateCompilePutByIdTransition(StructureID*, StructureID*, size_t cachedOffset, StructureIDChain*, void* returnAddress);

        void privateCompileCTIMachineTrampolines();
        void privateCompilePatchGetArrayLength(void* returnAddress);

        void compileOpCall(OpcodeID, Instruction* instruction, unsigned i, unsigned callLinkInfoIndex);
        void compileOpCallInitializeCallFrame();
        void compileOpCallSetupArgs(Instruction*);
        void compileOpCallEvalSetupArgs(Instruction*);
        void compileOpConstructSetupArgs(Instruction*);
        enum CompileOpStrictEqType { OpStrictEq, OpNStrictEq };
        void compileOpStrictEq(Instruction* instruction, unsigned i, CompileOpStrictEqType type);
        void putDoubleResultToJSNumberCellOrJSImmediate(X86::XMMRegisterID xmmSource, X86::RegisterID jsNumberCell, unsigned dst, X86Assembler::JmpSrc* wroteJSNumberCell,  X86::XMMRegisterID tempXmm, X86::RegisterID tempReg1, X86::RegisterID tempReg2);
        void compileBinaryArithOp(OpcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi, unsigned i);
        void compileBinaryArithOpSlowCase(Instruction*, OpcodeID, Vector<SlowCaseEntry>::iterator& iter, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi, unsigned i);

        void emitGetVirtualRegister(int src, X86Assembler::RegisterID dst, unsigned i);
        void emitGetVirtualRegisters(int src1, X86Assembler::RegisterID dst1, int src2, X86Assembler::RegisterID dst2, unsigned i);
        void emitPutVirtualRegister(unsigned dst, X86Assembler::RegisterID from = X86::eax);

        void emitPutCTIArg(X86Assembler::RegisterID src, unsigned offset);
        void emitPutCTIArgFromVirtualRegister(unsigned src, unsigned offset, X86Assembler::RegisterID scratch);
        void emitPutCTIArgConstant(unsigned value, unsigned offset);
        void emitGetCTIArg(unsigned offset, X86Assembler::RegisterID dst);

        void emitInitRegister(unsigned dst);

        void emitPutCTIParam(void* value, unsigned name);
        void emitPutCTIParam(X86Assembler::RegisterID from, unsigned name);
        void emitGetCTIParam(unsigned name, X86Assembler::RegisterID to);

        void emitPutToCallFrameHeader(X86Assembler::RegisterID from, RegisterFile::CallFrameHeaderEntry entry);
        void emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, X86Assembler::RegisterID to);

        JSValue* getConstantImmediateNumericArg(unsigned src);
        unsigned getDeTaggedConstantImmediate(JSValue* imm);

        bool linkSlowCaseIfNotJSCell(const Vector<SlowCaseEntry>::iterator&, int vReg);
        void emitJumpSlowCaseIfNotJSCell(X86Assembler::RegisterID, unsigned opcodeIndex);
        void emitJumpSlowCaseIfNotJSCell(X86Assembler::RegisterID, unsigned opcodeIndex, int VReg);

        void emitJumpSlowCaseIfNotImmNum(X86Assembler::RegisterID, unsigned opcodeIndex);
        void emitJumpSlowCaseIfNotImmNums(X86Assembler::RegisterID, X86Assembler::RegisterID, unsigned opcodeIndex);

        void emitFastArithDeTagImmediate(X86Assembler::RegisterID);
        X86Assembler::JmpSrc emitFastArithDeTagImmediateJumpIfZero(X86Assembler::RegisterID);
        void emitFastArithReTagImmediate(X86Assembler::RegisterID);
        void emitFastArithPotentiallyReTagImmediate(X86Assembler::RegisterID);
        void emitFastArithImmToInt(X86Assembler::RegisterID);
        void emitFastArithIntToImmOrSlowCase(X86Assembler::RegisterID, unsigned opcodeIndex);
        void emitFastArithIntToImmNoCheck(X86Assembler::RegisterID);
        X86Assembler::JmpSrc emitArithIntToImmWithJump(X86Assembler::RegisterID reg);

        void emitTagAsBoolImmediate(X86Assembler::RegisterID reg);

        void emitAllocateNumber(JSGlobalData*, unsigned);

        X86Assembler::JmpSrc emitNakedCall(unsigned opcodeIndex, X86::RegisterID);
        X86Assembler::JmpSrc emitNakedCall(unsigned opcodeIndex, void* function);
        X86Assembler::JmpSrc emitNakedFastCall(unsigned opcodeIndex, void*);
        X86Assembler::JmpSrc emitCTICall(Instruction*, unsigned opcodeIndex, CTIHelper_j);
        X86Assembler::JmpSrc emitCTICall(Instruction*, unsigned opcodeIndex, CTIHelper_o);
        X86Assembler::JmpSrc emitCTICall(Instruction*, unsigned opcodeIndex, CTIHelper_p);
        X86Assembler::JmpSrc emitCTICall(Instruction*, unsigned opcodeIndex, CTIHelper_v);
        X86Assembler::JmpSrc emitCTICall(Instruction*, unsigned opcodeIndex, CTIHelper_s);
        X86Assembler::JmpSrc emitCTICall(Instruction*, unsigned opcodeIndex, CTIHelper_b);
        X86Assembler::JmpSrc emitCTICall(Instruction*, unsigned opcodeIndex, CTIHelper_2);

        void emitGetVariableObjectRegister(X86Assembler::RegisterID variableObject, int index, X86Assembler::RegisterID dst);
        void emitPutVariableObjectRegister(X86Assembler::RegisterID src, X86Assembler::RegisterID variableObject, int index);
        
        void emitSlowScriptCheck(Instruction*, unsigned opcodeIndex);
#ifndef NDEBUG
        void printOpcodeOperandTypes(unsigned src1, unsigned src2);
#endif

        void killLastResultRegister();

        X86Assembler m_jit;
        BytecodeInterpreter* m_interpreter;
        JSGlobalData* m_globalData;
        CodeBlock* m_codeBlock;

        Vector<CallRecord> m_calls;
        Vector<X86Assembler::JmpDst> m_labels;
        Vector<StructureStubCompilationInfo> m_propertyAccessCompilationInfo;
        Vector<StructureStubCompilationInfo> m_callStructureStubCompilationInfo;
        Vector<JmpTable> m_jmpTable;

        struct JSRInfo {
            X86Assembler::JmpDst addrPosition;
            X86Assembler::JmpDst target;

            JSRInfo(const X86Assembler::JmpDst& storeLocation, const X86Assembler::JmpDst& targetLocation)
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

        // This limit comes from the limit set in PCRE
        static const int MaxPatternSize = (1 << 16);
    };
}

#endif // ENABLE(CTI)

#endif // CTI_h
