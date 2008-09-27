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

#include "Opcode.h"
#include "RegisterFile.h"
#include <masm/X86Assembler.h>
#include <profiler/Profiler.h>
#include <wtf/AlwaysInline.h>
#include <wtf/Vector.h>

#if ENABLE(SAMPLING_TOOL)
#include "SamplingTool.h"
#endif

#if COMPILER(MSVC)
#define CTI_ARGS void** args
#define ARGS (args)
#else
#define CTI_ARGS void* args
#define ARGS (&args)
#endif

#define CTI_ARGS_2ndResult 0x08

#define CTI_ARGS_code 0x0C
#define CTI_ARGS_exec 0x0D
#define CTI_ARGS_registerFile 0x0E
#define CTI_ARGS_r 0x0F
#define CTI_ARGS_scopeChain 0x10
#define CTI_ARGS_exception 0x11
#define CTI_ARGS_profilerReference 0x12
#define ARG_exec ((ExecState*)(ARGS)[CTI_ARGS_exec])
#define ARG_registerFile ((RegisterFile*)(ARGS)[CTI_ARGS_registerFile])
#define ARG_r ((Register*)(ARGS)[CTI_ARGS_r])
#define ARG_scopeChain ((ScopeChainNode*)(ARGS)[CTI_ARGS_scopeChain])
#define ARG_exception ((JSValue**)(ARGS)[CTI_ARGS_exception])
#define ARG_profilerReference ((Profiler**)(ARGS)[CTI_ARGS_profilerReference])

#define ARG_setScopeChain(newScopeChain) (*(volatile ScopeChainNode**)&(ARGS)[CTI_ARGS_scopeChain] = newScopeChain)
#define ARG_setR(newR) (*(volatile Register**)&(ARGS)[CTI_ARGS_r] = newR)
#define ARG_set2ndResult(new2ndResult) (*(volatile JSValue**)&(ARGS)[CTI_ARGS_2ndResult] = new2ndResult)

#define ARG_src1 ((JSValue*)((ARGS)[1]))
#define ARG_src2 ((JSValue*)((ARGS)[2]))
#define ARG_src3 ((JSValue*)((ARGS)[3]))
#define ARG_src4 ((JSValue*)((ARGS)[4]))
#define ARG_src5 ((JSValue*)((ARGS)[5]))
#define ARG_id1 ((Identifier*)((ARGS)[1]))
#define ARG_id2 ((Identifier*)((ARGS)[2]))
#define ARG_id3 ((Identifier*)((ARGS)[3]))
#define ARG_id4 ((Identifier*)((ARGS)[4]))
#define ARG_int1 ((int)((ARGS)[1]))
#define ARG_int2 ((int)((ARGS)[2]))
#define ARG_int3 ((int)((ARGS)[3]))
#define ARG_int4 ((int)((ARGS)[4]))
#define ARG_int5 ((int)((ARGS)[5]))
#define ARG_func1 ((FuncDeclNode*)((ARGS)[1]))
#define ARG_funcexp1 ((FuncExprNode*)((ARGS)[1]))
#define ARG_registers1 ((Register*)((ARGS)[1]))
#define ARG_regexp1 ((RegExp*)((ARGS)[1]))
#define ARG_pni1 ((JSPropertyNameIterator*)((ARGS)[1]))
#define ARG_instr1 ((Instruction*)((ARGS)[1]))
#define ARG_instr2 ((Instruction*)((ARGS)[2]))
#define ARG_instr3 ((Instruction*)((ARGS)[3]))
#define ARG_instr4 ((Instruction*)((ARGS)[4]))
#define ARG_instr5 ((Instruction*)((ARGS)[5]))
#define ARG_instr6 ((Instruction*)((ARGS)[6]))

#define CTI_RETURN_ADDRESS ((ARGS)[-1])

namespace JSC {

    class CodeBlock;
    class ExecState;
    class JSPropertyNameIterator;
    class JSValue;
    class Machine;
    class Register;
    class RegisterFile;
    class ScopeChainNode;
    class SimpleJumpTable;
    class StringJumpTable;
    class StructureIDChain;
    struct Instruction;
    struct OperandTypes;

    typedef JSValue* (*CTIHelper_j)(CTI_ARGS);
    typedef JSPropertyNameIterator* (*CTIHelper_p)(CTI_ARGS);
    typedef void (*CTIHelper_v)(CTI_ARGS);
    typedef void* (*CTIHelper_s)(CTI_ARGS);
    typedef int (*CTIHelper_b)(CTI_ARGS);

    struct CallRecord {
        X86Assembler::JmpSrc from;
        void* to;
        unsigned opcodeIndex;

        CallRecord()
        {
        }

        CallRecord(X86Assembler::JmpSrc f, CTIHelper_j t, unsigned i)
            : from(f)
            , to((void*)t)
            , opcodeIndex(i)
        {
        }

        CallRecord(X86Assembler::JmpSrc f, CTIHelper_p t, unsigned i)
            : from(f)
            , to((void*)t)
            , opcodeIndex(i)
        {
        }
        
        CallRecord(X86Assembler::JmpSrc f, CTIHelper_v t, unsigned i)
            : from(f)
            , to((void*)t)
            , opcodeIndex(i)
        {
        }
        
        CallRecord(X86Assembler::JmpSrc f, CTIHelper_s t, unsigned i)
            : from(f)
            , to((void*)t)
            , opcodeIndex(i)
        {
        }
        
        CallRecord(X86Assembler::JmpSrc f, CTIHelper_b t, unsigned i)
            : from(f)
            , to((void*)t)
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

        Type m_type;

        union {
            SimpleJumpTable* m_simpleJumpTable;
            StringJumpTable* m_stringJumpTable;
        } m_jumpTable;

        unsigned m_opcodeIndex;
        unsigned m_defaultOffset;

        SwitchRecord(SimpleJumpTable* jumpTable, unsigned opcodeIndex, unsigned defaultOffset, Type type)
            : m_type(type)
            , m_opcodeIndex(opcodeIndex)
            , m_defaultOffset(defaultOffset)
        {
            m_jumpTable.m_simpleJumpTable = jumpTable;
        }

        SwitchRecord(StringJumpTable* jumpTable, unsigned opcodeIndex, unsigned defaultOffset)
            : m_type(String)
            , m_opcodeIndex(opcodeIndex)
            , m_defaultOffset(defaultOffset)
        {
            m_jumpTable.m_stringJumpTable = jumpTable;
        }
    };

    struct StructureStubCompilationInfo {
        X86Assembler::JmpSrc callReturnLocation;
        X86Assembler::JmpDst hotPathBegin;
    };

    extern "C" {
        JSValue* ctiTrampoline(void* code, ExecState* exec, RegisterFile* registerFile, Register* r, ScopeChainNode* scopeChain, JSValue** exception, Profiler**);
        void ctiVMThrowTrampoline();
    };

    void ctiSetReturnAddress(void** where, void* what);
    void ctiRepatchCallByReturnAddress(void* where, void* what);

    class CTI {
        static const int repatchGetByIdDefaultStructureID = -1;
        // Magic number - initial offset cannot be representable as a signed 8bit value, or the X86Assembler
        // will compress the displacement, and we may not be able to fit a repatched offset.
        static const int repatchGetByIdDefaultOffset = 256;

        // These architecture specific value are used to enable repatching - see comment on op_put_by_id.
        static const int repatchOffsetPutByIdStructureID = 19;
        static const int repatchOffsetPutByIdPropertyMapOffset = 34;
        // These architecture specific value are used to enable repatching - see comment on op_get_by_id.
        static const int repatchOffsetGetByIdStructureID = 19;
        static const int repatchOffsetGetByIdBranchToSlowCase = 25;
        static const int repatchOffsetGetByIdPropertyMapOffset = 34;
        static const int repatchOffsetGetByIdSlowCaseCall = 17;

    public:
        static void compile(Machine* machine, ExecState* exec, CodeBlock* codeBlock)
        {
            CTI cti(machine, exec, codeBlock);
            cti.privateCompile();
        }

#if ENABLE(WREC)
        static void* compileRegExp(ExecState* exec, const UString& pattern, unsigned* numSubpatterns_ptr, const char** error_ptr, bool ignoreCase = false, bool multiline = false);
#endif

        static void compileGetByIdSelf(Machine* machine, ExecState* exec, CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress)
        {
            CTI cti(machine, exec, codeBlock);
            cti.privateCompileGetByIdSelf(structureID, cachedOffset, returnAddress);
        }

        static void compileGetByIdProto(Machine* machine, ExecState* exec, CodeBlock* codeBlock, StructureID* structureID, StructureID* prototypeStructureID, size_t cachedOffset, void* returnAddress)
        {
            CTI cti(machine, exec, codeBlock);
            cti.privateCompileGetByIdProto(structureID, prototypeStructureID, cachedOffset, returnAddress);
        }

        static void compileGetByIdChain(Machine* machine, ExecState* exec, CodeBlock* codeBlock, StructureID* structureID, StructureIDChain* chain, size_t count, size_t cachedOffset, void* returnAddress)
        {
            CTI cti(machine, exec, codeBlock);
            cti.privateCompileGetByIdChain(structureID, chain, count, cachedOffset, returnAddress);
        }

        static void compilePutByIdReplace(Machine* machine, ExecState* exec, CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress)
        {
            CTI cti(machine, exec, codeBlock);
            cti.privateCompilePutByIdReplace(structureID, cachedOffset, returnAddress);
        }
        
        static void compilePutByIdTransition(Machine* machine, ExecState* exec, CodeBlock* codeBlock, StructureID* oldStructureID, StructureID* newStructureID, size_t cachedOffset, StructureIDChain* sIDC, void* returnAddress)
        {
            CTI cti(machine, exec, codeBlock);
            cti.privateCompilePutByIdTransition(oldStructureID, newStructureID, cachedOffset, sIDC, returnAddress);
        }

        static void* compileArrayLengthTrampoline(Machine* machine, ExecState* exec, CodeBlock* codeBlock)
        {
            CTI cti(machine, exec, codeBlock);
            return cti.privateCompileArrayLengthTrampoline();
        }

        static void* compileStringLengthTrampoline(Machine* machine, ExecState* exec, CodeBlock* codeBlock)
        {
            CTI cti(machine, exec, codeBlock);
            return cti.privateCompileStringLengthTrampoline();
        }

        static void patchGetByIdSelf(CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress);
        static void patchPutByIdReplace(CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress);

        static void compilePatchGetArrayLength(Machine* machine, ExecState* exec, CodeBlock* codeBlock, void* returnAddress)
        {
            CTI cti(machine, exec, codeBlock);
            return cti.privateCompilePatchGetArrayLength(returnAddress);
        }

        inline static JSValue* execute(void* code, ExecState* exec, RegisterFile* registerFile, Register* r, ScopeChainNode* scopeChain, JSValue** exception)
        {
            JSValue* value = ctiTrampoline(code, exec, registerFile, r, scopeChain, exception, Profiler::enabledProfilerReference());
#if ENABLE(SAMPLING_TOOL)
            currentOpcodeID = static_cast<OpcodeID>(-1);
#endif
            return value;
        }

    private:
        CTI(Machine*, ExecState*, CodeBlock*);
        
        bool isConstant(int src);
        JSValue* getConstant(ExecState*, int src);

        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        void privateCompile();
        void privateCompileGetByIdSelf(StructureID*, size_t cachedOffset, void* returnAddress);
        void privateCompileGetByIdProto(StructureID*, StructureID* prototypeStructureID, size_t cachedOffset, void* returnAddress);
        void privateCompileGetByIdChain(StructureID*, StructureIDChain*, size_t count, size_t cachedOffset, void* returnAddress);
        void privateCompilePutByIdReplace(StructureID*, size_t cachedOffset, void* returnAddress);
        void privateCompilePutByIdTransition(StructureID*, StructureID*, size_t cachedOffset, StructureIDChain*, void* returnAddress);

        void* privateCompileArrayLengthTrampoline();
        void* privateCompileStringLengthTrampoline();
        void privateCompilePatchGetArrayLength(void* returnAddress);

        enum CompileOpCallType { OpCallNormal, OpCallEval, OpConstruct };
        void compileOpCall(Instruction* instruction, unsigned i, CompileOpCallType type = OpCallNormal);
        enum CompileOpStrictEqType { OpStrictEq, OpNStrictEq };
        void compileOpStrictEq(Instruction* instruction, unsigned i, CompileOpStrictEqType type);
        void putDoubleResultToJSNumberCellOrJSImmediate(X86::XMMRegisterID xmmSource, X86::RegisterID jsNumberCell, unsigned dst, X86Assembler::JmpSrc* wroteJSNumberCell,  X86::XMMRegisterID tempXmm, X86::RegisterID tempReg1, X86::RegisterID tempReg2);
        void compileBinaryArithOp(OpcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi, unsigned i);
        void compileBinaryArithOpSlowCase(OpcodeID, Vector<SlowCaseEntry>::iterator& iter, unsigned dst, unsigned src1, unsigned src2, OperandTypes opi, unsigned i);

        void emitGetArg(unsigned src, X86Assembler::RegisterID dst);
        void emitGetPutArg(unsigned src, unsigned offset, X86Assembler::RegisterID scratch);
        void emitPutArg(X86Assembler::RegisterID src, unsigned offset);
        void emitPutArgConstant(unsigned value, unsigned offset);
        void emitPutResult(unsigned dst, X86Assembler::RegisterID from = X86::eax);

        void emitInitRegister(unsigned dst);

        void emitPutCTIParam(void* value, unsigned name);
        void emitPutCTIParam(X86Assembler::RegisterID from, unsigned name);
        void emitGetCTIParam(unsigned name, X86Assembler::RegisterID to);

        void emitPutToCallFrameHeader(X86Assembler::RegisterID from, RegisterFile::CallFrameHeaderEntry entry);
        void emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, X86Assembler::RegisterID to);

        JSValue* getConstantImmediateNumericArg(unsigned src);
        unsigned getDeTaggedConstantImmediate(JSValue* imm);

        void emitJumpSlowCaseIfIsJSCell(X86Assembler::RegisterID reg, unsigned opcodeIndex);
        void emitJumpSlowCaseIfNotJSCell(X86Assembler::RegisterID reg, unsigned opcodeIndex);

        void emitJumpSlowCaseIfNotImmNum(X86Assembler::RegisterID, unsigned opcodeIndex);
        void emitJumpSlowCaseIfNotImmNums(X86Assembler::RegisterID, X86Assembler::RegisterID, unsigned opcodeIndex);

        void emitFastArithDeTagImmediate(X86Assembler::RegisterID);
        void emitFastArithReTagImmediate(X86Assembler::RegisterID);
        void emitFastArithPotentiallyReTagImmediate(X86Assembler::RegisterID);
        void emitFastArithImmToInt(X86Assembler::RegisterID);
        void emitFastArithIntToImmOrSlowCase(X86Assembler::RegisterID, unsigned opcodeIndex);
        void emitFastArithIntToImmNoCheck(X86Assembler::RegisterID);

        void emitTagAsBoolImmediate(X86Assembler::RegisterID reg);

        void emitDebugExceptionCheck();

        X86Assembler::JmpSrc emitCall(unsigned opcodeIndex, X86::RegisterID);
        X86Assembler::JmpSrc emitCall(unsigned opcodeIndex, CTIHelper_j);
        X86Assembler::JmpSrc emitCall(unsigned opcodeIndex, CTIHelper_p);
        X86Assembler::JmpSrc emitCall(unsigned opcodeIndex, CTIHelper_b);
        X86Assembler::JmpSrc emitCall(unsigned opcodeIndex, CTIHelper_v);
        X86Assembler::JmpSrc emitCall(unsigned opcodeIndex, CTIHelper_s);
        
        void emitGetVariableObjectRegister(X86Assembler::RegisterID variableObject, int index, X86Assembler::RegisterID dst);
        void emitPutVariableObjectRegister(X86Assembler::RegisterID src, X86Assembler::RegisterID variableObject, int index);
        
        void emitSlowScriptCheck(unsigned opcodeIndex);
#ifndef NDEBUG
        void printOpcodeOperandTypes(unsigned src1, unsigned src2);
#endif

        X86Assembler m_jit;
        Machine* m_machine;
        ExecState* m_exec;
        CodeBlock* m_codeBlock;

        Vector<CallRecord> m_calls;
        Vector<X86Assembler::JmpDst> m_labels;
        Vector<StructureStubCompilationInfo> m_structureStubCompilationInfo;
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

        // This limit comes from the limit set in PCRE
        static const int MaxPatternSize = (1 << 16);

    };
}

#endif // ENABLE(CTI)

#endif // CTI_h
