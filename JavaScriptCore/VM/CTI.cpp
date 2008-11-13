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

#include "config.h"
#include "CTI.h"

#if ENABLE(CTI)

#include "CodeBlock.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Machine.h"
#include "wrec/WREC.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace JSC {

#if PLATFORM(MAC)

static inline bool isSSE2Present()
{
    return true; // All X86 Macs are guaranteed to support at least SSE2
}

#else

static bool isSSE2Present()
{
    static const int SSE2FeatureBit = 1 << 26;
    struct SSE2Check {
        SSE2Check()
        {
            int flags;
#if COMPILER(MSVC)
            _asm {
                mov eax, 1 // cpuid function 1 gives us the standard feature set
                cpuid;
                mov flags, edx;
            }
#else
            flags = 0;
            // FIXME: Add GCC code to do above asm
#endif
            present = (flags & SSE2FeatureBit) != 0;
        }
        bool present;
    };
    static SSE2Check check;
    return check.present;
}

#endif

COMPILE_ASSERT(CTI_ARGS_code == 0xC, CTI_ARGS_code_is_C);
COMPILE_ASSERT(CTI_ARGS_callFrame == 0xE, CTI_ARGS_callFrame_is_E);

#if COMPILER(GCC) && PLATFORM(X86)

#if PLATFORM(DARWIN)
#define SYMBOL_STRING(name) "_" #name
#else
#define SYMBOL_STRING(name) #name
#endif

asm(
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "pushl %esi" "\n"
    "pushl %edi" "\n"
    "pushl %ebx" "\n"
    "subl $0x20, %esp" "\n"
    "movl $512, %esi" "\n"
    "movl 0x38(%esp), %edi" "\n" // Ox38 = 0x0E * 4, 0x0E = CTI_ARGS_callFrame (see assertion above)
    "call *0x30(%esp)" "\n" // Ox30 = 0x0C * 4, 0x0C = CTI_ARGS_code (see assertion above)
    "addl $0x20, %esp" "\n"
    "popl %ebx" "\n"
    "popl %edi" "\n"
    "popl %esi" "\n"
    "ret" "\n"
);

asm(
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
#if USE(CTI_ARGUMENT)
#if USE(FAST_CALL_CTI_ARGUMENT)
    "movl %esp, %ecx" "\n"
#else
    "movl %esp, 0(%esp)" "\n"
#endif
    "call " SYMBOL_STRING(_ZN3JSC7Machine12cti_vm_throwEPPv) "\n"
#else
    "call " SYMBOL_STRING(_ZN3JSC7Machine12cti_vm_throwEPvz) "\n"
#endif
    "addl $0x20, %esp" "\n"
    "popl %ebx" "\n"
    "popl %edi" "\n"
    "popl %esi" "\n"
    "ret" "\n"
);
    
#elif COMPILER(MSVC)

extern "C" {
    
    __declspec(naked) JSValue* ctiTrampoline(void* code, RegisterFile*, CallFrame*, JSValue** exception, Profiler**, JSGlobalData*)
    {
        __asm {
            push esi;
            push edi;
            push ebx;
            sub esp, 0x20;
            mov esi, 512;
            mov ecx, esp;
            mov edi, [esp + 0x38];
            call [esp + 0x30]; // Ox30 = 0x0C * 4, 0x0C = CTI_ARGS_code (see assertion above)
            add esp, 0x20;
            pop ebx;
            pop edi;
            pop esi;
            ret;
        }
    }
    
    __declspec(naked) void ctiVMThrowTrampoline()
    {
        __asm {
            mov ecx, esp;
            call JSC::Machine::cti_vm_throw;
            add esp, 0x20;
            pop ebx;
            pop edi;
            pop esi;
            ret;
        }
    }
    
}

#endif

static ALWAYS_INLINE uintptr_t asInteger(JSValue* value)
{
    return reinterpret_cast<uintptr_t>(value);
}

// get arg puts an arg from the SF register array into a h/w register
ALWAYS_INLINE void CTI::emitGetArg(int src, X86Assembler::RegisterID dst)
{
    // TODO: we want to reuse values that are already in registers if we can - add a register allocator!
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue* value = m_codeBlock->getConstant(src);
        m_jit.movl_i32r(asInteger(value), dst);
    } else
        m_jit.movl_mr(src * sizeof(Register), X86::edi, dst);
}

// get arg puts an arg from the SF register array onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void CTI::emitGetPutArg(unsigned src, unsigned offset, X86Assembler::RegisterID scratch)
{
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue* value = m_codeBlock->getConstant(src);
        m_jit.movl_i32m(asInteger(value), offset + sizeof(void*), X86::esp);
    } else {
        m_jit.movl_mr(src * sizeof(Register), X86::edi, scratch);
        m_jit.movl_rm(scratch, offset + sizeof(void*), X86::esp);
    }
}

// puts an arg onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void CTI::emitPutArg(X86Assembler::RegisterID src, unsigned offset)
{
    m_jit.movl_rm(src, offset + sizeof(void*), X86::esp);
}

ALWAYS_INLINE void CTI::emitPutArgConstant(unsigned value, unsigned offset)
{
    m_jit.movl_i32m(value, offset + sizeof(void*), X86::esp);
}

ALWAYS_INLINE JSValue* CTI::getConstantImmediateNumericArg(unsigned src)
{
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue* value = m_codeBlock->getConstant(src);
        return JSImmediate::isNumber(value) ? value : noValue();
    }
    return noValue();
}

ALWAYS_INLINE void CTI::emitPutCTIParam(void* value, unsigned name)
{
    m_jit.movl_i32m(reinterpret_cast<intptr_t>(value), name * sizeof(void*), X86::esp);
}

ALWAYS_INLINE void CTI::emitPutCTIParam(X86Assembler::RegisterID from, unsigned name)
{
    m_jit.movl_rm(from, name * sizeof(void*), X86::esp);
}

ALWAYS_INLINE void CTI::emitGetCTIParam(unsigned name, X86Assembler::RegisterID to)
{
    m_jit.movl_mr(name * sizeof(void*), X86::esp, to);
}

ALWAYS_INLINE void CTI::emitPutToCallFrameHeader(X86Assembler::RegisterID from, RegisterFile::CallFrameHeaderEntry entry)
{
    m_jit.movl_rm(from, entry * sizeof(Register), X86::edi);
}

ALWAYS_INLINE void CTI::emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, X86Assembler::RegisterID to)
{
    m_jit.movl_mr(entry * sizeof(Register), X86::edi, to);
}

ALWAYS_INLINE void CTI::emitPutResult(unsigned dst, X86Assembler::RegisterID from)
{
    m_jit.movl_rm(from, dst * sizeof(Register), X86::edi);
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE void CTI::emitInitRegister(unsigned dst)
{
    m_jit.movl_i32m(asInteger(jsUndefined()), dst * sizeof(Register), X86::edi);
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

void ctiSetReturnAddress(void** where, void* what)
{
    *where = what;
}

void ctiRepatchCallByReturnAddress(void* where, void* what)
{
    (static_cast<void**>(where))[-1] = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(what) - reinterpret_cast<uintptr_t>(where));
}

#ifndef NDEBUG

void CTI::printOpcodeOperandTypes(unsigned src1, unsigned src2)
{
    char which1 = '*';
    if (m_codeBlock->isConstantRegisterIndex(src1)) {
        JSValue* value = m_codeBlock->getConstant(src1);
        which1 = 
            JSImmediate::isImmediate(value) ?
                (JSImmediate::isNumber(value) ? 'i' :
                JSImmediate::isBoolean(value) ? 'b' :
                value->isUndefined() ? 'u' :
                value->isNull() ? 'n' : '?')
                :
            (value->isString() ? 's' :
            value->isObject() ? 'o' :
            'k');
    }
    char which2 = '*';
    if (m_codeBlock->isConstantRegisterIndex(src2)) {
        JSValue* value = m_codeBlock->getConstant(src2);
        which2 = 
            JSImmediate::isImmediate(value) ?
                (JSImmediate::isNumber(value) ? 'i' :
                JSImmediate::isBoolean(value) ? 'b' :
                value->isUndefined() ? 'u' :
                value->isNull() ? 'n' : '?')
                :
            (value->isString() ? 's' :
            value->isObject() ? 'o' :
            'k');
    }
    if ((which1 != '*') | (which2 != '*'))
        fprintf(stderr, "Types %c %c\n", which1, which2);
}

#endif

extern "C" {
    static JSValue* FASTCALL allocateNumber(JSGlobalData* globalData) {
        JSValue* result = new (globalData) JSNumberCell(globalData);
        ASSERT(result);
        return result;
    }
}

ALWAYS_INLINE void CTI::emitAllocateNumber(JSGlobalData* globalData, unsigned opcodeIndex)
{
    m_jit.movl_i32r(reinterpret_cast<intptr_t>(globalData), X86::ecx);
    emitNakedFastCall(opcodeIndex, (void*)allocateNumber);
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitNakedCall(unsigned opcodeIndex, X86::RegisterID r)
{
    X86Assembler::JmpSrc call = m_jit.emitCall(r);
    m_calls.append(CallRecord(call, opcodeIndex));

    return call;
}

ALWAYS_INLINE  X86Assembler::JmpSrc CTI::emitNakedCall(unsigned opcodeIndex, void(*function)())
{
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, reinterpret_cast<CTIHelper_v>(function), opcodeIndex));
    return call;
}

ALWAYS_INLINE  X86Assembler::JmpSrc CTI::emitNakedFastCall(unsigned opcodeIndex, void* function)
{
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, reinterpret_cast<CTIHelper_v>(function), opcodeIndex));
    return call;
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitCTICall(Instruction* vPC, unsigned opcodeIndex, CTIHelper_j helper)
{
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, true), m_machine->sampler()->sampleSlot());
#else
    UNUSED_PARAM(vPC);
#endif
    m_jit.emitRestoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, helper, opcodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, false), m_machine->sampler()->sampleSlot());
#endif

    return call;
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitCTICall(Instruction* vPC, unsigned opcodeIndex, CTIHelper_o helper)
{
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, true), m_machine->sampler()->sampleSlot());
#else
    UNUSED_PARAM(vPC);
#endif
    m_jit.emitRestoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, helper, opcodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, false), m_machine->sampler()->sampleSlot());
#endif

    return call;
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitCTICall(Instruction* vPC, unsigned opcodeIndex, CTIHelper_p helper)
{
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, true), m_machine->sampler()->sampleSlot());
#else
    UNUSED_PARAM(vPC);
#endif
    m_jit.emitRestoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, helper, opcodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, false), m_machine->sampler()->sampleSlot());
#endif

    return call;
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitCTICall(Instruction* vPC, unsigned opcodeIndex, CTIHelper_b helper)
{
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, true), m_machine->sampler()->sampleSlot());
#else
    UNUSED_PARAM(vPC);
#endif
    m_jit.emitRestoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, helper, opcodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, false), m_machine->sampler()->sampleSlot());
#endif

    return call;
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitCTICall(Instruction* vPC, unsigned opcodeIndex, CTIHelper_v helper)
{
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, true), m_machine->sampler()->sampleSlot());
#else
    UNUSED_PARAM(vPC);
#endif
    m_jit.emitRestoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, helper, opcodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, false), m_machine->sampler()->sampleSlot());
#endif

    return call;
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitCTICall(Instruction* vPC, unsigned opcodeIndex, CTIHelper_s helper)
{
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, true), m_machine->sampler()->sampleSlot());
#else
    UNUSED_PARAM(vPC);
#endif
    m_jit.emitRestoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, helper, opcodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, false), m_machine->sampler()->sampleSlot());
#endif

    return call;
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitCTICall(Instruction* vPC, unsigned opcodeIndex, CTIHelper_2 helper)
{
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, true), m_machine->sampler()->sampleSlot());
#else
    UNUSED_PARAM(vPC);
#endif
    m_jit.emitRestoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    X86Assembler::JmpSrc call = m_jit.emitCall();
    m_calls.append(CallRecord(call, helper, opcodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    m_jit.movl_i32m(m_machine->sampler()->encodeSample(vPC, false), m_machine->sampler()->sampleSlot());
#endif

    return call;
}

ALWAYS_INLINE void CTI::emitJumpSlowCaseIfNotJSCell(X86Assembler::RegisterID reg, unsigned opcodeIndex)
{
    m_jit.testl_i32r(JSImmediate::TagMask, reg);
    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), opcodeIndex));
}

ALWAYS_INLINE void CTI::emitJumpSlowCaseIfNotImmNum(X86Assembler::RegisterID reg, unsigned opcodeIndex)
{
    m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, reg);
    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJe(), opcodeIndex));
}

ALWAYS_INLINE void CTI::emitJumpSlowCaseIfNotImmNums(X86Assembler::RegisterID reg1, X86Assembler::RegisterID reg2, unsigned opcodeIndex)
{
    m_jit.movl_rr(reg1, X86::ecx);
    m_jit.andl_rr(reg2, X86::ecx);
    emitJumpSlowCaseIfNotImmNum(X86::ecx, opcodeIndex);
}

ALWAYS_INLINE unsigned CTI::getDeTaggedConstantImmediate(JSValue* imm)
{
    ASSERT(JSImmediate::isNumber(imm));
    return asInteger(imm) & ~JSImmediate::TagBitTypeInteger;
}

ALWAYS_INLINE void CTI::emitFastArithDeTagImmediate(X86Assembler::RegisterID reg)
{
    m_jit.subl_i8r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitFastArithDeTagImmediateJumpIfZero(X86Assembler::RegisterID reg)
{
    m_jit.subl_i8r(JSImmediate::TagBitTypeInteger, reg);
    return m_jit.emitUnlinkedJe();
}

ALWAYS_INLINE void CTI::emitFastArithReTagImmediate(X86Assembler::RegisterID reg)
{
    m_jit.addl_i8r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE void CTI::emitFastArithPotentiallyReTagImmediate(X86Assembler::RegisterID reg)
{
    m_jit.orl_i32r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE void CTI::emitFastArithImmToInt(X86Assembler::RegisterID reg)
{
    m_jit.sarl_i8r(1, reg);
}

ALWAYS_INLINE void CTI::emitFastArithIntToImmOrSlowCase(X86Assembler::RegisterID reg, unsigned opcodeIndex)
{
    m_jit.addl_rr(reg, reg);
    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), opcodeIndex));
    emitFastArithReTagImmediate(reg);
}

ALWAYS_INLINE void CTI::emitFastArithIntToImmNoCheck(X86Assembler::RegisterID reg)
{
    m_jit.addl_rr(reg, reg);
    emitFastArithReTagImmediate(reg);
}

ALWAYS_INLINE X86Assembler::JmpSrc CTI::emitArithIntToImmWithJump(X86Assembler::RegisterID reg)
{
    m_jit.addl_rr(reg, reg);
    X86Assembler::JmpSrc jmp = m_jit.emitUnlinkedJo();
    emitFastArithReTagImmediate(reg);
    return jmp;
}

ALWAYS_INLINE void CTI::emitTagAsBoolImmediate(X86Assembler::RegisterID reg)
{
    m_jit.shl_i8r(JSImmediate::ExtendedPayloadShift, reg);
    m_jit.orl_i32r(JSImmediate::FullTagTypeBool, reg);
}

CTI::CTI(JSGlobalData* globalData, CodeBlock* codeBlock)
    : m_jit(globalData->machine->jitCodeBuffer())
    , m_machine(globalData->machine)
    , m_globalData(globalData)
    , m_codeBlock(codeBlock)
    , m_labels(codeBlock ? codeBlock->instructions.size() : 0)
    , m_propertyAccessCompilationInfo(codeBlock ? codeBlock->propertyAccessInstructions.size() : 0)
    , m_callStructureStubCompilationInfo(codeBlock ? codeBlock->callLinkInfos.size() : 0)
{
}

#define CTI_COMPILE_BINARY_OP(name) \
    case name: { \
        emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx); \
        emitGetPutArg(instruction[i + 3].u.operand, 4, X86::ecx); \
        emitCTICall(instruction + i, i, Machine::cti_##name); \
        emitPutResult(instruction[i + 1].u.operand); \
        i += 4; \
        break; \
    }

#define CTI_COMPILE_UNARY_OP(name) \
    case name: { \
        emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx); \
        emitCTICall(instruction + i, i, Machine::cti_##name); \
        emitPutResult(instruction[i + 1].u.operand); \
        i += 3; \
        break; \
    }

static void unreachable()
{
    ASSERT_NOT_REACHED();
    exit(1);
}

void CTI::compileOpCallInitializeCallFrame(unsigned, unsigned argCount)
{
    m_jit.movl_mr(OBJECT_OFFSET(JSFunction, m_scopeChain) + OBJECT_OFFSET(ScopeChain, m_node), X86::ecx, X86::ebx); // newScopeChain

    m_jit.movl_i32m(asInteger(noValue()), RegisterFile::OptionalCalleeArguments * static_cast<int>(sizeof(Register)), X86::edi);
    m_jit.movl_rm(X86::ecx, RegisterFile::Callee * static_cast<int>(sizeof(Register)), X86::edi);
    m_jit.movl_i32m(argCount, RegisterFile::ArgumentCount * static_cast<int>(sizeof(Register)), X86::edi);
    m_jit.movl_rm(X86::ebx, RegisterFile::ScopeChain * static_cast<int>(sizeof(Register)), X86::edi);
}

void CTI::compileOpCallSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // ecx holds func
    emitPutArg(X86::ecx, 0);
    emitPutArgConstant(registerOffset, 4);
    emitPutArgConstant(argCount, 8);
    emitPutArgConstant(reinterpret_cast<unsigned>(instruction), 12);
}

void CTI::compileOpCallEvalSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // ecx holds func
    emitPutArg(X86::ecx, 0);
    emitPutArgConstant(registerOffset, 4);
    emitPutArgConstant(argCount, 8);
    emitPutArgConstant(reinterpret_cast<unsigned>(instruction), 12);
}

void CTI::compileOpConstructSetupArgs(Instruction* instruction)
{
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;
    int proto = instruction[5].u.operand;
    int thisRegister = instruction[6].u.operand;

    // ecx holds func
    emitPutArg(X86::ecx, 0);
    emitPutArgConstant(registerOffset, 4);
    emitPutArgConstant(argCount, 8);
    emitGetPutArg(proto, 12, X86::eax);
    emitPutArgConstant(thisRegister, 16);
    emitPutArgConstant(reinterpret_cast<unsigned>(instruction), 20);
}

void CTI::compileOpCall(OpcodeID opcodeID, Instruction* instruction, unsigned i, unsigned callLinkInfoIndex)
{
    int dst = instruction[1].u.operand;
    int callee = instruction[2].u.operand;
    int argCount = instruction[3].u.operand;
    int registerOffset = instruction[4].u.operand;

    // Handle eval
    X86Assembler::JmpSrc wasEval;
    if (opcodeID == op_call_eval) {
        emitGetArg(callee, X86::ecx);
        compileOpCallEvalSetupArgs(instruction);

        emitCTICall(instruction, i, Machine::cti_op_call_eval);
        m_jit.cmpl_i32r(asInteger(JSImmediate::impossibleValue()), X86::eax);
        wasEval = m_jit.emitUnlinkedJne();
    }

    // This plants a check for a cached JSFunction value, so we can plant a fast link to the callee.
    // This deliberately leaves the callee in ecx, used when setting up the stack frame below
    emitGetArg(callee, X86::ecx);
    m_jit.cmpl_i32r(asInteger(JSImmediate::impossibleValue()), X86::ecx);
    X86Assembler::JmpDst addressOfLinkedFunctionCheck = m_jit.label();
    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
    ASSERT(X86Assembler::getDifferenceBetweenLabels(addressOfLinkedFunctionCheck, m_jit.label()) == repatchOffsetOpCallCall);
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathBegin = addressOfLinkedFunctionCheck;

    // The following is the fast case, only used whan a callee can be linked.

    // In the case of OpConstruct, call out to a cti_ function to create the new object.
    if (opcodeID == op_construct) {
        int proto = instruction[5].u.operand;
        int thisRegister = instruction[6].u.operand;

        emitPutArg(X86::ecx, 0);
        emitGetPutArg(proto, 12, X86::eax);
        emitCTICall(instruction, i, Machine::cti_op_construct_JSConstruct);
        emitPutResult(thisRegister);
        emitGetArg(callee, X86::ecx);
    }

    // Fast version of stack frame initialization, directly relative to edi.
    // Note that this omits to set up RegisterFile::CodeBlock, which is set in the callee
    m_jit.movl_i32m(asInteger(noValue()), (registerOffset + RegisterFile::OptionalCalleeArguments) * static_cast<int>(sizeof(Register)), X86::edi);
    m_jit.movl_rm(X86::ecx, (registerOffset + RegisterFile::Callee) * static_cast<int>(sizeof(Register)), X86::edi);
    m_jit.movl_mr(OBJECT_OFFSET(JSFunction, m_scopeChain) + OBJECT_OFFSET(ScopeChain, m_node), X86::ecx, X86::edx); // newScopeChain
    m_jit.movl_i32m(argCount, (registerOffset + RegisterFile::ArgumentCount) * static_cast<int>(sizeof(Register)), X86::edi);
    m_jit.movl_rm(X86::edi, (registerOffset + RegisterFile::CallerFrame) * static_cast<int>(sizeof(Register)), X86::edi);
    m_jit.movl_rm(X86::edx, (registerOffset + RegisterFile::ScopeChain) * static_cast<int>(sizeof(Register)), X86::edi);
    m_jit.addl_i32r(registerOffset * sizeof(Register), X86::edi);

    // Call to the callee
    m_callStructureStubCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall(i, unreachable);
    
    if (opcodeID == op_call_eval)
        m_jit.link(wasEval, m_jit.label());

    // Put the return value in dst. In the interpreter, op_ret does this.
    emitPutResult(dst);

#if ENABLE(CODEBLOCK_SAMPLING)
        m_jit.movl_i32m(reinterpret_cast<unsigned>(m_codeBlock), m_machine->sampler()->codeBlockSlot());
#endif
}

void CTI::compileOpStrictEq(Instruction* instruction, unsigned i, CompileOpStrictEqType type)
{
    bool negated = (type == OpNStrictEq);

    unsigned dst = instruction[1].u.operand;
    unsigned src1 = instruction[2].u.operand;
    unsigned src2 = instruction[3].u.operand;

    emitGetArg(src1, X86::eax);
    emitGetArg(src2, X86::edx);

    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    X86Assembler::JmpSrc firstNotImmediate = m_jit.emitUnlinkedJe();
    m_jit.testl_i32r(JSImmediate::TagMask, X86::edx);
    X86Assembler::JmpSrc secondNotImmediate = m_jit.emitUnlinkedJe();

    m_jit.cmpl_rr(X86::edx, X86::eax);
    if (negated)
        m_jit.setne_r(X86::eax);
    else
        m_jit.sete_r(X86::eax);
    m_jit.movzbl_rr(X86::eax, X86::eax);
    emitTagAsBoolImmediate(X86::eax);
            
    X86Assembler::JmpSrc bothWereImmediates = m_jit.emitUnlinkedJmp();

    m_jit.link(firstNotImmediate, m_jit.label());

    // check that edx is immediate but not the zero immediate
    m_jit.testl_i32r(JSImmediate::TagMask, X86::edx);
    m_jit.setz_r(X86::ecx);
    m_jit.movzbl_rr(X86::ecx, X86::ecx); // ecx is now 1 if edx was nonimmediate
    m_jit.cmpl_i32r(asInteger(JSImmediate::zeroImmediate()), X86::edx);
    m_jit.sete_r(X86::edx);
    m_jit.movzbl_rr(X86::edx, X86::edx); // edx is now 1 if edx was the 0 immediate
    m_jit.orl_rr(X86::ecx, X86::edx);

    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJnz(), i));

    m_jit.movl_i32r(asInteger(jsBoolean(negated)), X86::eax);

    X86Assembler::JmpSrc firstWasNotImmediate = m_jit.emitUnlinkedJmp();

    m_jit.link(secondNotImmediate, m_jit.label());
    // check that eax is not the zero immediate (we know it must be immediate)
    m_jit.cmpl_i32r(asInteger(JSImmediate::zeroImmediate()), X86::eax);
    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJe(), i));

    m_jit.movl_i32r(asInteger(jsBoolean(negated)), X86::eax);

    m_jit.link(bothWereImmediates, m_jit.label());
    m_jit.link(firstWasNotImmediate, m_jit.label());

    emitPutResult(dst);
}

void CTI::emitSlowScriptCheck(Instruction* vPC, unsigned opcodeIndex)
{
    m_jit.subl_i8r(1, X86::esi);
    X86Assembler::JmpSrc skipTimeout = m_jit.emitUnlinkedJne();
    emitCTICall(vPC, opcodeIndex, Machine::cti_timeout_check);

    emitGetCTIParam(CTI_ARGS_globalData, X86::ecx);
    m_jit.movl_mr(OBJECT_OFFSET(JSGlobalData, machine), X86::ecx, X86::ecx);
    m_jit.movl_mr(OBJECT_OFFSET(Machine, m_ticksUntilNextTimeoutCheck), X86::ecx, X86::esi);
    m_jit.link(skipTimeout, m_jit.label());
}

/*
  This is required since number representation is canonical - values representable as a JSImmediate should not be stored in a JSNumberCell.
  
  In the common case, the double value from 'xmmSource' is written to the reusable JSNumberCell pointed to by 'jsNumberCell', then 'jsNumberCell'
  is written to the output SF Register 'dst', and then a jump is planted (stored into *wroteJSNumberCell).
  
  However if the value from xmmSource is representable as a JSImmediate, then the JSImmediate value will be written to the output, and flow
  control will fall through from the code planted.
*/
void CTI::putDoubleResultToJSNumberCellOrJSImmediate(X86::XMMRegisterID xmmSource, X86::RegisterID jsNumberCell, unsigned dst, X86Assembler::JmpSrc* wroteJSNumberCell,  X86::XMMRegisterID tempXmm, X86::RegisterID tempReg1, X86::RegisterID tempReg2)
{
    // convert (double -> JSImmediate -> double), and check if the value is unchanged - in which case the value is representable as a JSImmediate.
    m_jit.cvttsd2si_rr(xmmSource, tempReg1);
    m_jit.addl_rr(tempReg1, tempReg1);
    m_jit.sarl_i8r(1, tempReg1);
    m_jit.cvtsi2sd_rr(tempReg1, tempXmm);
    // Compare & branch if immediate. 
    m_jit.ucomis_rr(tempXmm, xmmSource);
    X86Assembler::JmpSrc resultIsImm = m_jit.emitUnlinkedJe();
    X86Assembler::JmpDst resultLookedLikeImmButActuallyIsnt = m_jit.label();
    
    // Store the result to the JSNumberCell and jump.
    m_jit.movsd_rm(xmmSource, OBJECT_OFFSET(JSNumberCell, m_value), jsNumberCell);
    emitPutResult(dst, jsNumberCell);
    *wroteJSNumberCell = m_jit.emitUnlinkedJmp();

    m_jit.link(resultIsImm, m_jit.label());
    // value == (double)(JSImmediate)value... or at least, it looks that way...
    // ucomi will report that (0 == -0), and will report true if either input in NaN (result is unordered).
    m_jit.link(m_jit.emitUnlinkedJp(), resultLookedLikeImmButActuallyIsnt); // Actually was a NaN
    m_jit.pextrw_irr(3, xmmSource, tempReg2);
    m_jit.cmpl_i32r(0x8000, tempReg2);
    m_jit.link(m_jit.emitUnlinkedJe(), resultLookedLikeImmButActuallyIsnt); // Actually was -0
    // Yes it really really really is representable as a JSImmediate.
    emitFastArithIntToImmNoCheck(tempReg1);
    emitPutResult(dst, tempReg1);
}

void CTI::compileBinaryArithOp(OpcodeID opcodeID, unsigned dst, unsigned src1, unsigned src2, OperandTypes types, unsigned i)
{
    StructureID* numberStructureID = m_globalData->numberStructureID.get();
    X86Assembler::JmpSrc wasJSNumberCell1;
    X86Assembler::JmpSrc wasJSNumberCell1b;
    X86Assembler::JmpSrc wasJSNumberCell2;
    X86Assembler::JmpSrc wasJSNumberCell2b;

    emitGetArg(src1, X86::eax);
    emitGetArg(src2, X86::edx);

    if (types.second().isReusable() && isSSE2Present()) {
        ASSERT(types.second().mightBeNumber());

        // Check op2 is a number
        m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::edx);
        X86Assembler::JmpSrc op2imm = m_jit.emitUnlinkedJne();
        if (!types.second().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::edx, i);
            m_jit.cmpl_i32m(reinterpret_cast<unsigned>(numberStructureID), OBJECT_OFFSET(JSCell, m_structureID), X86::edx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
        }

        // (1) In this case src2 is a reusable number cell.
        //     Slow case if src1 is not a number type.
        m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::eax);
        X86Assembler::JmpSrc op1imm = m_jit.emitUnlinkedJne();
        if (!types.first().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::eax, i);
            m_jit.cmpl_i32m(reinterpret_cast<unsigned>(numberStructureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
        }

        // (1a) if we get here, src1 is also a number cell
        m_jit.movsd_mr(OBJECT_OFFSET(JSNumberCell, m_value), X86::eax, X86::xmm0);
        X86Assembler::JmpSrc loadedDouble = m_jit.emitUnlinkedJmp();
        // (1b) if we get here, src1 is an immediate
        m_jit.link(op1imm, m_jit.label());
        emitFastArithImmToInt(X86::eax);
        m_jit.cvtsi2sd_rr(X86::eax, X86::xmm0);
        // (1c) 
        m_jit.link(loadedDouble, m_jit.label());
        if (opcodeID == op_add)
            m_jit.addsd_mr(OBJECT_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        else if (opcodeID == op_sub)
            m_jit.subsd_mr(OBJECT_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        else {
            ASSERT(opcodeID == op_mul);
            m_jit.mulsd_mr(OBJECT_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm0);
        }

        putDoubleResultToJSNumberCellOrJSImmediate(X86::xmm0, X86::edx, dst, &wasJSNumberCell2, X86::xmm1, X86::ecx, X86::eax);
        wasJSNumberCell2b = m_jit.emitUnlinkedJmp();

        // (2) This handles cases where src2 is an immediate number.
        //     Two slow cases - either src1 isn't an immediate, or the subtract overflows.
        m_jit.link(op2imm, m_jit.label());
        emitJumpSlowCaseIfNotImmNum(X86::eax, i);
    } else if (types.first().isReusable() && isSSE2Present()) {
        ASSERT(types.first().mightBeNumber());

        // Check op1 is a number
        m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::eax);
        X86Assembler::JmpSrc op1imm = m_jit.emitUnlinkedJne();
        if (!types.first().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::eax, i);
            m_jit.cmpl_i32m(reinterpret_cast<unsigned>(numberStructureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
        }

        // (1) In this case src1 is a reusable number cell.
        //     Slow case if src2 is not a number type.
        m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::edx);
        X86Assembler::JmpSrc op2imm = m_jit.emitUnlinkedJne();
        if (!types.second().definitelyIsNumber()) {
            emitJumpSlowCaseIfNotJSCell(X86::edx, i);
            m_jit.cmpl_i32m(reinterpret_cast<unsigned>(numberStructureID), OBJECT_OFFSET(JSCell, m_structureID), X86::edx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
        }

        // (1a) if we get here, src2 is also a number cell
        m_jit.movsd_mr(OBJECT_OFFSET(JSNumberCell, m_value), X86::edx, X86::xmm1);
        X86Assembler::JmpSrc loadedDouble = m_jit.emitUnlinkedJmp();
        // (1b) if we get here, src2 is an immediate
        m_jit.link(op2imm, m_jit.label());
        emitFastArithImmToInt(X86::edx);
        m_jit.cvtsi2sd_rr(X86::edx, X86::xmm1);
        // (1c) 
        m_jit.link(loadedDouble, m_jit.label());
        m_jit.movsd_mr(OBJECT_OFFSET(JSNumberCell, m_value), X86::eax, X86::xmm0);
        if (opcodeID == op_add)
            m_jit.addsd_rr(X86::xmm1, X86::xmm0);
        else if (opcodeID == op_sub)
            m_jit.subsd_rr(X86::xmm1, X86::xmm0);
        else {
            ASSERT(opcodeID == op_mul);
            m_jit.mulsd_rr(X86::xmm1, X86::xmm0);
        }
        m_jit.movsd_rm(X86::xmm0, OBJECT_OFFSET(JSNumberCell, m_value), X86::eax);
        emitPutResult(dst);

        putDoubleResultToJSNumberCellOrJSImmediate(X86::xmm0, X86::eax, dst, &wasJSNumberCell1, X86::xmm1, X86::ecx, X86::edx);
        wasJSNumberCell1b = m_jit.emitUnlinkedJmp();

        // (2) This handles cases where src1 is an immediate number.
        //     Two slow cases - either src2 isn't an immediate, or the subtract overflows.
        m_jit.link(op1imm, m_jit.label());
        emitJumpSlowCaseIfNotImmNum(X86::edx, i);
    } else
        emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, i);

    if (opcodeID == op_add) {
        emitFastArithDeTagImmediate(X86::eax);
        m_jit.addl_rr(X86::edx, X86::eax);
        m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
    } else  if (opcodeID == op_sub) {
        m_jit.subl_rr(X86::edx, X86::eax);
        m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
        emitFastArithReTagImmediate(X86::eax);
    } else {
        ASSERT(opcodeID == op_mul);
        // convert eax & edx from JSImmediates to ints, and check if either are zero
        emitFastArithImmToInt(X86::edx);
        X86Assembler::JmpSrc op1Zero = emitFastArithDeTagImmediateJumpIfZero(X86::eax);
        m_jit.testl_rr(X86::edx, X86::edx);
        X86Assembler::JmpSrc op2NonZero = m_jit.emitUnlinkedJne();
        m_jit.link(op1Zero, m_jit.label());
        // if either input is zero, add the two together, and check if the result is < 0.
        // If it is, we have a problem (N < 0), (N * 0) == -0, not representatble as a JSImmediate. 
        m_jit.movl_rr(X86::eax, X86::ecx);
        m_jit.addl_rr(X86::edx, X86::ecx);
        m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJs(), i));
        // Skip the above check if neither input is zero
        m_jit.link(op2NonZero, m_jit.label());
        m_jit.imull_rr(X86::edx, X86::eax);
        m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
        emitFastArithReTagImmediate(X86::eax);
    }
    emitPutResult(dst);

    if (types.second().isReusable() && isSSE2Present()) {
        m_jit.link(wasJSNumberCell2, m_jit.label());
        m_jit.link(wasJSNumberCell2b, m_jit.label());
    }
    else if (types.first().isReusable() && isSSE2Present()) {
        m_jit.link(wasJSNumberCell1, m_jit.label());
        m_jit.link(wasJSNumberCell1b, m_jit.label());
    }
}

void CTI::compileBinaryArithOpSlowCase(Instruction* vPC, OpcodeID opcodeID, Vector<SlowCaseEntry>::iterator& iter, unsigned dst, unsigned src1, unsigned src2, OperandTypes types, unsigned i)
{
    X86Assembler::JmpDst here = m_jit.label();
    m_jit.link(iter->from, here);
    if (types.second().isReusable() && isSSE2Present()) {
        if (!types.first().definitelyIsNumber()) {
            m_jit.link((++iter)->from, here);
            m_jit.link((++iter)->from, here);
        }
        if (!types.second().definitelyIsNumber()) {
            m_jit.link((++iter)->from, here);
            m_jit.link((++iter)->from, here);
        }
        m_jit.link((++iter)->from, here);
    } else if (types.first().isReusable() && isSSE2Present()) {
        if (!types.first().definitelyIsNumber()) {
            m_jit.link((++iter)->from, here);
            m_jit.link((++iter)->from, here);
        }
        if (!types.second().definitelyIsNumber()) {
            m_jit.link((++iter)->from, here);
            m_jit.link((++iter)->from, here);
        }
        m_jit.link((++iter)->from, here);
    } else
        m_jit.link((++iter)->from, here);

    // additional entry point to handle -0 cases.
    if (opcodeID == op_mul)
        m_jit.link((++iter)->from, here);

    emitGetPutArg(src1, 0, X86::ecx);
    emitGetPutArg(src2, 4, X86::ecx);
    if (opcodeID == op_add)
        emitCTICall(vPC, i, Machine::cti_op_add);
    else if (opcodeID == op_sub)
        emitCTICall(vPC, i, Machine::cti_op_sub);
    else {
        ASSERT(opcodeID == op_mul);
        emitCTICall(vPC, i, Machine::cti_op_mul);
    }
    emitPutResult(dst);
}

void CTI::privateCompileMainPass()
{
    Instruction* instruction = m_codeBlock->instructions.begin();
    unsigned instructionCount = m_codeBlock->instructions.size();

    unsigned propertyAccessInstructionIndex = 0;
    unsigned callLinkInfoIndex = 0;

    for (unsigned i = 0; i < instructionCount; ) {
        ASSERT_WITH_MESSAGE(m_machine->isOpcode(instruction[i].u.opcode), "privateCompileMainPass gone bad @ %d", i);

#if ENABLE(OPCODE_SAMPLING)
        if (i > 0) // Avoid the overhead of sampling op_enter twice.
            m_jit.movl_i32m(m_machine->sampler()->encodeSample(instruction + i), m_machine->sampler()->sampleSlot());
#endif

        m_labels[i] = m_jit.label();
        OpcodeID opcodeID = m_machine->getOpcodeID(instruction[i].u.opcode);
        switch (opcodeID) {
        case op_mov: {
            unsigned src = instruction[i + 2].u.operand;
            if (m_codeBlock->isConstantRegisterIndex(src))
                m_jit.movl_i32r(asInteger(m_codeBlock->getConstant(src)), X86::eax);
            else
                emitGetArg(src, X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_add: {
            unsigned dst = instruction[i + 1].u.operand;
            unsigned src1 = instruction[i + 2].u.operand;
            unsigned src2 = instruction[i + 3].u.operand;

            if (JSValue* value = getConstantImmediateNumericArg(src1)) {
                emitGetArg(src2, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::edx, i);
                m_jit.addl_i32r(getDeTaggedConstantImmediate(value), X86::edx);
                m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
                emitPutResult(dst, X86::edx);
            } else if (JSValue* value = getConstantImmediateNumericArg(src2)) {
                emitGetArg(src1, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                m_jit.addl_i32r(getDeTaggedConstantImmediate(value), X86::eax);
                m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
                emitPutResult(dst);
            } else {
                OperandTypes types = OperandTypes::fromInt(instruction[i + 4].u.operand);
                if (types.first().mightBeNumber() && types.second().mightBeNumber())
                    compileBinaryArithOp(op_add, instruction[i + 1].u.operand, instruction[i + 2].u.operand, instruction[i + 3].u.operand, OperandTypes::fromInt(instruction[i + 4].u.operand), i);
                else {
                    emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
                    emitGetPutArg(instruction[i + 3].u.operand, 4, X86::ecx);
                    emitCTICall(instruction + i, i, Machine::cti_op_add);
                    emitPutResult(instruction[i + 1].u.operand);
                }
            }

            i += 5;
            break;
        }
        case op_end: {
            if (m_codeBlock->needsFullScopeChain)
                emitCTICall(instruction + i, i, Machine::cti_op_end);
            emitGetArg(instruction[i + 1].u.operand, X86::eax);
            m_jit.pushl_m(RegisterFile::ReturnPC * static_cast<int>(sizeof(Register)), X86::edi);
            m_jit.ret();
            i += 2;
            break;
        }
        case op_jmp: {
            unsigned target = instruction[i + 1].u.operand;
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 1 + target));
            i += 2;
            break;
        }
        case op_pre_inc: {
            int srcDst = instruction[i + 1].u.operand;
            emitGetArg(srcDst, X86::eax);
            emitJumpSlowCaseIfNotImmNum(X86::eax, i);
            m_jit.addl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitPutResult(srcDst);
            i += 2;
            break;
        }
        case op_loop: {
            emitSlowScriptCheck(instruction + i, i);

            unsigned target = instruction[i + 1].u.operand;
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 1 + target));
            i += 2;
            break;
        }
        case op_loop_if_less: {
            emitSlowScriptCheck(instruction + i, i);

            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                emitGetArg(instruction[i + 1].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::edx, i);
                m_jit.cmpl_i32r(asInteger(src2imm), X86::edx);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJl(), i + 3 + target));
            } else {
                emitGetArg(instruction[i + 1].u.operand, X86::eax);
                emitGetArg(instruction[i + 2].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                emitJumpSlowCaseIfNotImmNum(X86::edx, i);
                m_jit.cmpl_rr(X86::edx, X86::eax);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJl(), i + 3 + target));
            }
            i += 4;
            break;
        }
        case op_loop_if_lesseq: {
            emitSlowScriptCheck(instruction + i, i);

            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                emitGetArg(instruction[i + 1].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::edx, i);
                m_jit.cmpl_i32r(asInteger(src2imm), X86::edx);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJle(), i + 3 + target));
            } else {
                emitGetArg(instruction[i + 1].u.operand, X86::eax);
                emitGetArg(instruction[i + 2].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                emitJumpSlowCaseIfNotImmNum(X86::edx, i);
                m_jit.cmpl_rr(X86::edx, X86::eax);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJle(), i + 3 + target));
            }
            i += 4;
            break;
        }
        case op_new_object: {
            emitCTICall(instruction + i, i, Machine::cti_op_new_object);
            emitPutResult(instruction[i + 1].u.operand);
            i += 2;
            break;
        }
        case op_put_by_id: {
            // In order to be able to repatch both the StructureID, and the object offset, we store one pointer,
            // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
            // such that the StructureID & offset are always at the same distance from this.

            emitGetArg(instruction[i + 1].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::edx);

            ASSERT(m_codeBlock->propertyAccessInstructions[propertyAccessInstructionIndex].opcodeIndex == i);
            X86Assembler::JmpDst hotPathBegin = m_jit.label();
            m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;
            ++propertyAccessInstructionIndex;

            // Jump to a slow case if either the base object is an immediate, or if the StructureID does not match.
            emitJumpSlowCaseIfNotJSCell(X86::eax, i);
            // It is important that the following instruction plants a 32bit immediate, in order that it can be patched over.
            m_jit.cmpl_i32m(repatchGetByIdDefaultStructureID, OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
            ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, m_jit.label()) == repatchOffsetPutByIdStructureID);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            // Plant a load from a bogus ofset in the object's property map; we will patch this later, if it is to be used.
            m_jit.movl_mr(OBJECT_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
            m_jit.movl_rm(X86::edx, repatchGetByIdDefaultOffset, X86::eax);
            ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, m_jit.label()) == repatchOffsetPutByIdPropertyMapOffset);

            i += 8;
            break;
        }
        case op_get_by_id: {
            // As for put_by_id, get_by_id requires the offset of the StructureID and the offset of the access to be repatched.
            // Additionally, for get_by_id we need repatch the offset of the branch to the slow case (we repatch this to jump
            // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
            // to jump back to if one of these trampolies finds a match.

            emitGetArg(instruction[i + 2].u.operand, X86::eax);

            ASSERT(m_codeBlock->propertyAccessInstructions[propertyAccessInstructionIndex].opcodeIndex == i);

            X86Assembler::JmpDst hotPathBegin = m_jit.label();
            m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;
            ++propertyAccessInstructionIndex;

            emitJumpSlowCaseIfNotJSCell(X86::eax, i);
            m_jit.cmpl_i32m(repatchGetByIdDefaultStructureID, OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
            ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, m_jit.label()) == repatchOffsetGetByIdStructureID);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, m_jit.label()) == repatchOffsetGetByIdBranchToSlowCase);

            m_jit.movl_mr(OBJECT_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
            m_jit.movl_mr(repatchGetByIdDefaultOffset, X86::eax, X86::ecx);
            ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, m_jit.label()) == repatchOffsetGetByIdPropertyMapOffset);
            emitPutResult(instruction[i + 1].u.operand, X86::ecx);

            i += 8;
            break;
        }
        case op_instanceof: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax); // value
            emitGetArg(instruction[i + 3].u.operand, X86::ecx); // baseVal
            emitGetArg(instruction[i + 4].u.operand, X86::edx); // proto

            // check if any are immediates
            m_jit.orl_rr(X86::eax, X86::ecx);
            m_jit.orl_rr(X86::edx, X86::ecx);
            m_jit.testl_i32r(JSImmediate::TagMask, X86::ecx);

            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJnz(), i));

            // check that all are object type - this is a bit of a bithack to avoid excess branching;
            // we check that the sum of the three type codes from StructureIDs is exactly 3 * ObjectType,
            // this works because NumberType and StringType are smaller
            m_jit.movl_i32r(3 * ObjectType, X86::ecx);
            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::eax);
            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::edx, X86::edx);
            m_jit.subl_mr(OBJECT_OFFSET(StructureID, m_typeInfo.m_type), X86::eax, X86::ecx);
            m_jit.subl_mr(OBJECT_OFFSET(StructureID, m_typeInfo.m_type), X86::edx, X86::ecx);
            emitGetArg(instruction[i + 3].u.operand, X86::edx); // reload baseVal
            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::edx, X86::edx);
            m_jit.cmpl_rm(X86::ecx, OBJECT_OFFSET(StructureID, m_typeInfo.m_type), X86::edx);

            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            // check that baseVal's flags include ImplementsHasInstance but not OverridesHasInstance
            m_jit.movl_mr(OBJECT_OFFSET(StructureID, m_typeInfo.m_flags), X86::edx, X86::ecx);
            m_jit.andl_i32r(ImplementsHasInstance | OverridesHasInstance, X86::ecx);
            m_jit.cmpl_i32r(ImplementsHasInstance, X86::ecx);

            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            emitGetArg(instruction[i + 2].u.operand, X86::ecx); // reload value
            emitGetArg(instruction[i + 4].u.operand, X86::edx); // reload proto

            // optimistically load true result
            m_jit.movl_i32r(asInteger(jsBoolean(true)), X86::eax);

            X86Assembler::JmpDst loop = m_jit.label();

            // load value's prototype
            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::ecx, X86::ecx);
            m_jit.movl_mr(OBJECT_OFFSET(StructureID, m_prototype), X86::ecx, X86::ecx);
            
            m_jit.cmpl_rr(X86::ecx, X86::edx);
            X86Assembler::JmpSrc exit = m_jit.emitUnlinkedJe();

            m_jit.cmpl_i32r(asInteger(jsNull()), X86::ecx);
            X86Assembler::JmpSrc goToLoop = m_jit.emitUnlinkedJne();
            m_jit.link(goToLoop, loop);

            m_jit.movl_i32r(asInteger(jsBoolean(false)), X86::eax);

            m_jit.link(exit, m_jit.label());

            emitPutResult(instruction[i + 1].u.operand);

            i += 5;
            break;
        }
        case op_del_by_id: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitCTICall(instruction + i, i, Machine::cti_op_del_by_id);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_mul: {
            unsigned dst = instruction[i + 1].u.operand;
            unsigned src1 = instruction[i + 2].u.operand;
            unsigned src2 = instruction[i + 3].u.operand;

            // For now, only plant a fast int case if the constant operand is greater than zero.
            JSValue* src1Value = getConstantImmediateNumericArg(src1);
            JSValue* src2Value = getConstantImmediateNumericArg(src2);
            int32_t value;
            if (src1Value && ((value = JSImmediate::intValue(src1Value)) > 0)) {
                emitGetArg(src2, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                emitFastArithDeTagImmediate(X86::eax);
                m_jit.imull_i32r(X86::eax, value, X86::eax);
                m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
                emitFastArithReTagImmediate(X86::eax);
                emitPutResult(dst);
            } else if (src2Value && ((value = JSImmediate::intValue(src2Value)) > 0)) {
                emitGetArg(src1, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                emitFastArithDeTagImmediate(X86::eax);
                m_jit.imull_i32r(X86::eax, value, X86::eax);
                m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
                emitFastArithReTagImmediate(X86::eax);
                emitPutResult(dst);
            } else
                compileBinaryArithOp(op_mul, instruction[i + 1].u.operand, instruction[i + 2].u.operand, instruction[i + 3].u.operand, OperandTypes::fromInt(instruction[i + 4].u.operand), i);

            i += 5;
            break;
        }
        case op_new_func: {
            FuncDeclNode* func = (m_codeBlock->functions[instruction[i + 2].u.operand]).get();
            emitPutArgConstant(reinterpret_cast<unsigned>(func), 0);
            emitCTICall(instruction + i, i, Machine::cti_op_new_func);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_call:
        case op_call_eval:
        case op_construct: {
            compileOpCall(opcodeID, instruction + i, i, callLinkInfoIndex++);
            i += (opcodeID == op_construct ? 7 : 5);
            break;
        }
        case op_get_global_var: {
            JSVariableObject* globalObject = static_cast<JSVariableObject*>(instruction[i + 2].u.jsCell);
            m_jit.movl_i32r(asInteger(globalObject), X86::eax);
            emitGetVariableObjectRegister(X86::eax, instruction[i + 3].u.operand, X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_put_global_var: {
            JSVariableObject* globalObject = static_cast<JSVariableObject*>(instruction[i + 1].u.jsCell);
            m_jit.movl_i32r(asInteger(globalObject), X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::edx);
            emitPutVariableObjectRegister(X86::edx, X86::eax, instruction[i + 2].u.operand);
            i += 4;
            break;
        }
        case op_get_scoped_var: {
            int skip = instruction[i + 3].u.operand + m_codeBlock->needsFullScopeChain;

            emitGetArg(RegisterFile::ScopeChain, X86::eax);
            while (skip--)
                m_jit.movl_mr(OBJECT_OFFSET(ScopeChainNode, next), X86::eax, X86::eax);

            m_jit.movl_mr(OBJECT_OFFSET(ScopeChainNode, object), X86::eax, X86::eax);
            emitGetVariableObjectRegister(X86::eax, instruction[i + 2].u.operand, X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_put_scoped_var: {
            int skip = instruction[i + 2].u.operand + m_codeBlock->needsFullScopeChain;

            emitGetArg(RegisterFile::ScopeChain, X86::edx);
            emitGetArg(instruction[i + 3].u.operand, X86::eax);
            while (skip--)
                m_jit.movl_mr(OBJECT_OFFSET(ScopeChainNode, next), X86::edx, X86::edx);

            m_jit.movl_mr(OBJECT_OFFSET(ScopeChainNode, object), X86::edx, X86::edx);
            emitPutVariableObjectRegister(X86::eax, X86::edx, instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_tear_off_activation: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_tear_off_activation);
            i += 2;
            break;
        }
        case op_tear_off_arguments: {
            emitCTICall(instruction + i, i, Machine::cti_op_tear_off_arguments);
            i += 1;
            break;
        }
        case op_ret: {
            // We could JIT generate the deref, only calling out to C when the refcount hits zero.
            if (m_codeBlock->needsFullScopeChain)
                emitCTICall(instruction + i, i, Machine::cti_op_ret_scopeChain);

            // Return the result in %eax.
            emitGetArg(instruction[i + 1].u.operand, X86::eax);

            // Grab the return address.
            emitGetArg(RegisterFile::ReturnPC, X86::edx);

            // Restore our caller's "r".
            emitGetArg(RegisterFile::CallerFrame, X86::edi);

            // Return.
            m_jit.pushl_r(X86::edx);
            m_jit.ret();

            i += 2;
            break;
        }
        case op_new_array: {
            m_jit.leal_mr(sizeof(Register) * instruction[i + 2].u.operand, X86::edi, X86::edx);
            emitPutArg(X86::edx, 0);
            emitPutArgConstant(instruction[i + 3].u.operand, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_new_array);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_resolve: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCTICall(instruction + i, i, Machine::cti_op_resolve);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_construct_verify: {
            emitGetArg(instruction[i + 1].u.operand, X86::eax);
            
            m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
            X86Assembler::JmpSrc isImmediate = m_jit.emitUnlinkedJne();
            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::ecx);
            m_jit.cmpl_i32m(ObjectType, OBJECT_OFFSET(StructureID, m_typeInfo) + OBJECT_OFFSET(TypeInfo, m_type), X86::ecx);
            X86Assembler::JmpSrc isObject = m_jit.emitUnlinkedJe();

            m_jit.link(isImmediate, m_jit.label());
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            m_jit.link(isObject, m_jit.label());

            i += 3;
            break;
        }
        case op_get_by_val: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNum(X86::edx, i);
            emitFastArithImmToInt(X86::edx);
            m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            m_jit.cmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsArrayVptr), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            // This is an array; get the m_storage pointer into ecx, then check if the index is below the fast cutoff
            m_jit.movl_mr(OBJECT_OFFSET(JSArray, m_storage), X86::eax, X86::ecx);
            m_jit.cmpl_rm(X86::edx, OBJECT_OFFSET(JSArray, m_fastAccessCutoff), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJbe(), i));

            // Get the value from the vector
            m_jit.movl_mr(OBJECT_OFFSET(ArrayStorage, m_vector[0]), X86::ecx, X86::edx, sizeof(JSValue*), X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_resolve_func: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCTICall(instruction + i, i, Machine::cti_op_resolve_func);
            emitPutResult(instruction[i + 1].u.operand);
            emitPutResult(instruction[i + 2].u.operand, X86::edx);
            i += 4;
            break;
        }
        case op_sub: {
            compileBinaryArithOp(op_sub, instruction[i + 1].u.operand, instruction[i + 2].u.operand, instruction[i + 3].u.operand, OperandTypes::fromInt(instruction[i + 4].u.operand), i);
            i += 5;
            break;
        }
        case op_put_by_val: {
            emitGetArg(instruction[i + 1].u.operand, X86::eax);
            emitGetArg(instruction[i + 2].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNum(X86::edx, i);
            emitFastArithImmToInt(X86::edx);
            m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            m_jit.cmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsArrayVptr), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            // This is an array; get the m_storage pointer into ecx, then check if the index is below the fast cutoff
            m_jit.movl_mr(OBJECT_OFFSET(JSArray, m_storage), X86::eax, X86::ecx);
            m_jit.cmpl_rm(X86::edx, OBJECT_OFFSET(JSArray, m_fastAccessCutoff), X86::eax);
            X86Assembler::JmpSrc inFastVector = m_jit.emitUnlinkedJa();
            // No; oh well, check if the access if within the vector - if so, we may still be okay.
            m_jit.cmpl_rm(X86::edx, OBJECT_OFFSET(ArrayStorage, m_vectorLength), X86::ecx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJbe(), i));

            // This is a write to the slow part of the vector; first, we have to check if this would be the first write to this location.
            // FIXME: should be able to handle initial write to array; increment the the number of items in the array, and potentially update fast access cutoff. 
            m_jit.cmpl_i8m(0, OBJECT_OFFSET(ArrayStorage, m_vector[0]), X86::ecx, X86::edx, sizeof(JSValue*));
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJe(), i));

            // All good - put the value into the array.
            m_jit.link(inFastVector, m_jit.label());
            emitGetArg(instruction[i + 3].u.operand, X86::eax);
            m_jit.movl_rm(X86::eax, OBJECT_OFFSET(ArrayStorage, m_vector[0]), X86::ecx, X86::edx, sizeof(JSValue*));
            i += 4;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_lesseq)
        case op_loop_if_true: {
            emitSlowScriptCheck(instruction + i, i);

            unsigned target = instruction[i + 2].u.operand;
            emitGetArg(instruction[i + 1].u.operand, X86::eax);

            m_jit.cmpl_i32r(asInteger(JSImmediate::zeroImmediate()), X86::eax);
            X86Assembler::JmpSrc isZero = m_jit.emitUnlinkedJe();
            m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJne(), i + 2 + target));

            m_jit.cmpl_i32r(asInteger(JSImmediate::trueImmediate()), X86::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJe(), i + 2 + target));
            m_jit.cmpl_i32r(asInteger(JSImmediate::falseImmediate()), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            m_jit.link(isZero, m_jit.label());
            i += 3;
            break;
        };
        case op_resolve_base: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCTICall(instruction + i, i, Machine::cti_op_resolve_base);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_negate: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::eax);
            X86Assembler::JmpSrc notImmediate = m_jit.emitUnlinkedJe();

            m_jit.cmpl_i32r(JSImmediate::TagBitTypeInteger, X86::eax);
            X86Assembler::JmpSrc zeroImmediate = m_jit.emitUnlinkedJe();
            emitFastArithImmToInt(X86::eax);
            m_jit.negl_r(X86::eax); // This can't overflow as we only have a 31bit int at this point
            X86Assembler::JmpSrc overflow = emitArithIntToImmWithJump(X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            X86Assembler::JmpSrc immediateNegateSuccess = m_jit.emitUnlinkedJmp();

            if (!isSSE2Present()) {
                m_jit.link(zeroImmediate, m_jit.label());
                m_jit.link(overflow, m_jit.label());
                m_jit.link(notImmediate, m_jit.label());
                emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
                emitCTICall(instruction + i, i, Machine::cti_op_negate);
                emitPutResult(instruction[i + 1].u.operand);
            } else {
                // Slow case immediates
                m_slowCases.append(SlowCaseEntry(zeroImmediate, i));
                m_slowCases.append(SlowCaseEntry(overflow, i));
                m_jit.link(notImmediate, m_jit.label());
                ResultType resultType(instruction[i + 3].u.resultType);
                if (!resultType.definitelyIsNumber()) {
                    emitJumpSlowCaseIfNotJSCell(X86::eax, i);
                    StructureID* numberStructureID = m_globalData->numberStructureID.get();
                    m_jit.cmpl_i32m(reinterpret_cast<unsigned>(numberStructureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
                    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
                }
                m_jit.movsd_mr(OBJECT_OFFSET(JSNumberCell, m_value), X86::eax, X86::xmm0);
                // We need 3 copies of the sign bit mask so we can assure alignment and pad for the 128bit load
                static double doubleSignBit[] = { -0.0, -0.0, -0.0 };
                m_jit.xorpd_mr((void*)((((uintptr_t)doubleSignBit)+15)&~15), X86::xmm0);
                X86Assembler::JmpSrc wasCell;
                if (!resultType.isReusableNumber())
                    emitAllocateNumber(m_globalData, i);

                putDoubleResultToJSNumberCellOrJSImmediate(X86::xmm0, X86::eax, instruction[i + 1].u.operand, &wasCell,
                                                           X86::xmm1, X86::ecx, X86::edx);
                m_jit.link(wasCell, m_jit.label());
            }
            m_jit.link(immediateNegateSuccess, m_jit.label());
            i += 4;
            break;
        }
        case op_resolve_skip: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitPutArgConstant(instruction[i + 3].u.operand + m_codeBlock->needsFullScopeChain, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_resolve_skip);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_resolve_global: {
            // Fast case
            unsigned globalObject = asInteger(instruction[i + 2].u.jsCell);
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            void* structureIDAddr = reinterpret_cast<void*>(instruction + i + 4);
            void* offsetAddr = reinterpret_cast<void*>(instruction + i + 5);

            // Check StructureID of global object
            m_jit.movl_i32r(globalObject, X86::eax);
            m_jit.movl_mr(structureIDAddr, X86::edx);
            m_jit.cmpl_rm(X86::edx, OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
            X86Assembler::JmpSrc noMatch = m_jit.emitUnlinkedJne(); // StructureIDs don't match

            // Load cached property
            m_jit.movl_mr(OBJECT_OFFSET(JSGlobalObject, m_propertyStorage), X86::eax, X86::eax);
            m_jit.movl_mr(offsetAddr, X86::edx);
            m_jit.movl_mr(0, X86::eax, X86::edx, sizeof(JSValue*), X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            X86Assembler::JmpSrc end = m_jit.emitUnlinkedJmp();

            // Slow case
            m_jit.link(noMatch, m_jit.label());
            emitPutArgConstant(globalObject, 0);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitPutArgConstant(reinterpret_cast<unsigned>(instruction + i), 8);
            emitCTICall(instruction + i, i, Machine::cti_op_resolve_global);
            emitPutResult(instruction[i + 1].u.operand);
            m_jit.link(end, m_jit.label());
            i += 6;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_div)
        case op_pre_dec: {
            int srcDst = instruction[i + 1].u.operand;
            emitGetArg(srcDst, X86::eax);
            emitJumpSlowCaseIfNotImmNum(X86::eax, i);
            m_jit.subl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitPutResult(srcDst);
            i += 2;
            break;
        }
        case op_jnless: {
            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                emitGetArg(instruction[i + 1].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::edx, i);
                m_jit.cmpl_i32r(asInteger(src2imm), X86::edx);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJge(), i + 3 + target));
            } else {
                emitGetArg(instruction[i + 1].u.operand, X86::eax);
                emitGetArg(instruction[i + 2].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                emitJumpSlowCaseIfNotImmNum(X86::edx, i);
                m_jit.cmpl_rr(X86::edx, X86::eax);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJge(), i + 3 + target));
            }
            i += 4;
            break;
        }
        case op_not: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            m_jit.xorl_i8r(JSImmediate::FullTagTypeBool, X86::eax);
            m_jit.testl_i32r(JSImmediate::FullTagTypeMask, X86::eax); // i8?
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            m_jit.xorl_i8r((JSImmediate::FullTagTypeBool | JSImmediate::ExtendedPayloadBitBoolValue), X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_jfalse: {
            unsigned target = instruction[i + 2].u.operand;
            emitGetArg(instruction[i + 1].u.operand, X86::eax);

            m_jit.cmpl_i32r(asInteger(JSImmediate::zeroImmediate()), X86::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJe(), i + 2 + target));
            m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::eax);
            X86Assembler::JmpSrc isNonZero = m_jit.emitUnlinkedJne();

            m_jit.cmpl_i32r(asInteger(JSImmediate::falseImmediate()), X86::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJe(), i + 2 + target));
            m_jit.cmpl_i32r(asInteger(JSImmediate::trueImmediate()), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            m_jit.link(isNonZero, m_jit.label());
            i += 3;
            break;
        };
        case op_jeq_null: {
            unsigned src = instruction[i + 1].u.operand;
            unsigned target = instruction[i + 2].u.operand;

            emitGetArg(src, X86::eax);
            m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
            X86Assembler::JmpSrc isImmediate = m_jit.emitUnlinkedJnz();

            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::ecx);
            m_jit.testl_i32m(MasqueradesAsUndefined, OBJECT_OFFSET(StructureID, m_typeInfo.m_flags), X86::ecx);
            m_jit.setnz_r(X86::eax);

            X86Assembler::JmpSrc wasNotImmediate = m_jit.emitUnlinkedJmp();

            m_jit.link(isImmediate, m_jit.label());

            m_jit.movl_i32r(~JSImmediate::ExtendedTagBitUndefined, X86::ecx);
            m_jit.andl_rr(X86::eax, X86::ecx);
            m_jit.cmpl_i32r(JSImmediate::FullTagTypeNull, X86::ecx);
            m_jit.sete_r(X86::eax);

            m_jit.link(wasNotImmediate, m_jit.label());

            m_jit.movzbl_rr(X86::eax, X86::eax);
            m_jit.cmpl_i32r(0, X86::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJnz(), i + 2 + target));            

            i += 3;
            break;
        };
        case op_jneq_null: {
            unsigned src = instruction[i + 1].u.operand;
            unsigned target = instruction[i + 2].u.operand;

            emitGetArg(src, X86::eax);
            m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
            X86Assembler::JmpSrc isImmediate = m_jit.emitUnlinkedJnz();

            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::ecx);
            m_jit.testl_i32m(MasqueradesAsUndefined, OBJECT_OFFSET(StructureID, m_typeInfo.m_flags), X86::ecx);
            m_jit.setz_r(X86::eax);

            X86Assembler::JmpSrc wasNotImmediate = m_jit.emitUnlinkedJmp();

            m_jit.link(isImmediate, m_jit.label());

            m_jit.movl_i32r(~JSImmediate::ExtendedTagBitUndefined, X86::ecx);
            m_jit.andl_rr(X86::eax, X86::ecx);
            m_jit.cmpl_i32r(JSImmediate::FullTagTypeNull, X86::ecx);
            m_jit.setne_r(X86::eax);

            m_jit.link(wasNotImmediate, m_jit.label());

            m_jit.movzbl_rr(X86::eax, X86::eax);
            m_jit.cmpl_i32r(0, X86::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJnz(), i + 2 + target));            

            i += 3;
            break;
        }
        case op_post_inc: {
            int srcDst = instruction[i + 2].u.operand;
            emitGetArg(srcDst, X86::eax);
            m_jit.movl_rr(X86::eax, X86::edx);
            emitJumpSlowCaseIfNotImmNum(X86::eax, i);
            m_jit.addl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), X86::edx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitPutResult(srcDst, X86::edx);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_unexpected_load: {
            JSValue* v = m_codeBlock->unexpectedConstants[instruction[i + 2].u.operand];
            m_jit.movl_i32r(asInteger(v), X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_jsr: {
            int retAddrDst = instruction[i + 1].u.operand;
            int target = instruction[i + 2].u.operand;
            m_jit.movl_i32m(0, sizeof(Register) * retAddrDst, X86::edi);
            X86Assembler::JmpDst addrPosition = m_jit.label();
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 2 + target));
            X86Assembler::JmpDst sretTarget = m_jit.label();
            m_jsrSites.append(JSRInfo(addrPosition, sretTarget));
            i += 3;
            break;
        }
        case op_sret: {
            m_jit.jmp_m(sizeof(Register) * instruction[i + 1].u.operand, X86::edi);
            i += 2;
            break;
        }
        case op_eq: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, i);
            m_jit.cmpl_rr(X86::edx, X86::eax);
            m_jit.sete_r(X86::eax);
            m_jit.movzbl_rr(X86::eax, X86::eax);
            emitTagAsBoolImmediate(X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_lshift: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::ecx);
            emitJumpSlowCaseIfNotImmNum(X86::eax, i);
            emitJumpSlowCaseIfNotImmNum(X86::ecx, i);
            emitFastArithImmToInt(X86::eax);
            emitFastArithImmToInt(X86::ecx);
            m_jit.shll_CLr(X86::eax);
            emitFastArithIntToImmOrSlowCase(X86::eax, i);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_bitand: {
            unsigned src1 = instruction[i + 2].u.operand;
            unsigned src2 = instruction[i + 3].u.operand;
            unsigned dst = instruction[i + 1].u.operand;
            if (JSValue* value = getConstantImmediateNumericArg(src1)) {
                emitGetArg(src2, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                m_jit.andl_i32r(asInteger(value), X86::eax); // FIXME: make it more obvious this is relying on the format of JSImmediate
                emitPutResult(dst);
            } else if (JSValue* value = getConstantImmediateNumericArg(src2)) {
                emitGetArg(src1, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                m_jit.andl_i32r(asInteger(value), X86::eax);
                emitPutResult(dst);
            } else {
                emitGetArg(src1, X86::eax);
                emitGetArg(src2, X86::edx);
                m_jit.andl_rr(X86::edx, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax, i);
                emitPutResult(dst);
            }
            i += 5;
            break;
        }
        case op_rshift: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::ecx);
            emitJumpSlowCaseIfNotImmNum(X86::eax, i);
            emitJumpSlowCaseIfNotImmNum(X86::ecx, i);
            emitFastArithImmToInt(X86::ecx);
            m_jit.sarl_CLr(X86::eax);
            emitFastArithPotentiallyReTagImmediate(X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_bitnot: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitJumpSlowCaseIfNotImmNum(X86::eax, i);
            m_jit.xorl_i8r(~JSImmediate::TagBitTypeInteger, X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_resolve_with_base: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCTICall(instruction + i, i, Machine::cti_op_resolve_with_base);
            emitPutResult(instruction[i + 1].u.operand);
            emitPutResult(instruction[i + 2].u.operand, X86::edx);
            i += 4;
            break;
        }
        case op_new_func_exp: {
            FuncExprNode* func = (m_codeBlock->functionExpressions[instruction[i + 2].u.operand]).get();
            emitPutArgConstant(reinterpret_cast<unsigned>(func), 0);
            emitCTICall(instruction + i, i, Machine::cti_op_new_func_exp);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_mod: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::ecx);
            emitJumpSlowCaseIfNotImmNum(X86::eax, i);
            emitJumpSlowCaseIfNotImmNum(X86::ecx, i);
            emitFastArithDeTagImmediate(X86::eax);
            m_slowCases.append(SlowCaseEntry(emitFastArithDeTagImmediateJumpIfZero(X86::ecx), i));
            m_jit.cdq();
            m_jit.idivl_r(X86::ecx);
            emitFastArithReTagImmediate(X86::edx);
            m_jit.movl_rr(X86::edx, X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_jtrue: {
            unsigned target = instruction[i + 2].u.operand;
            emitGetArg(instruction[i + 1].u.operand, X86::eax);

            m_jit.cmpl_i32r(asInteger(JSImmediate::zeroImmediate()), X86::eax);
            X86Assembler::JmpSrc isZero = m_jit.emitUnlinkedJe();
            m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJne(), i + 2 + target));

            m_jit.cmpl_i32r(asInteger(JSImmediate::trueImmediate()), X86::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJe(), i + 2 + target));
            m_jit.cmpl_i32r(asInteger(JSImmediate::falseImmediate()), X86::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            m_jit.link(isZero, m_jit.label());
            i += 3;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_less)
        case op_neq: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, i);
            m_jit.cmpl_rr(X86::eax, X86::edx);

            m_jit.setne_r(X86::eax);
            m_jit.movzbl_rr(X86::eax, X86::eax);
            emitTagAsBoolImmediate(X86::eax);

            emitPutResult(instruction[i + 1].u.operand);

            i += 4;
            break;
        }
        case op_post_dec: {
            int srcDst = instruction[i + 2].u.operand;
            emitGetArg(srcDst, X86::eax);
            m_jit.movl_rr(X86::eax, X86::edx);
            emitJumpSlowCaseIfNotImmNum(X86::eax, i);
            m_jit.subl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), X86::edx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitPutResult(srcDst, X86::edx);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_urshift)
        case op_bitxor: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, i);
            m_jit.xorl_rr(X86::edx, X86::eax);
            emitFastArithReTagImmediate(X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 5;
            break;
        }
        case op_new_regexp: {
            RegExp* regExp = m_codeBlock->regexps[instruction[i + 2].u.operand].get();
            emitPutArgConstant(reinterpret_cast<unsigned>(regExp), 0);
            emitCTICall(instruction + i, i, Machine::cti_op_new_regexp);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_bitor: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, i);
            m_jit.orl_rr(X86::edx, X86::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 5;
            break;
        }
        case op_throw: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_throw);
            m_jit.addl_i8r(0x20, X86::esp);
            m_jit.popl_r(X86::ebx);
            m_jit.popl_r(X86::edi);
            m_jit.popl_r(X86::esi);
            m_jit.ret();
            i += 2;
            break;
        }
        case op_get_pnames: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_get_pnames);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_next_pname: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
            unsigned target = instruction[i + 3].u.operand;
            emitCTICall(instruction + i, i, Machine::cti_op_next_pname);
            m_jit.testl_rr(X86::eax, X86::eax);
            X86Assembler::JmpSrc endOfIter = m_jit.emitUnlinkedJe();
            emitPutResult(instruction[i + 1].u.operand);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 3 + target));
            m_jit.link(endOfIter, m_jit.label());
            i += 4;
            break;
        }
        case op_push_scope: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_push_scope);
            i += 2;
            break;
        }
        case op_pop_scope: {
            emitCTICall(instruction + i, i, Machine::cti_op_pop_scope);
            i += 1;
            break;
        }
        CTI_COMPILE_UNARY_OP(op_typeof)
        CTI_COMPILE_UNARY_OP(op_is_undefined)
        CTI_COMPILE_UNARY_OP(op_is_boolean)
        CTI_COMPILE_UNARY_OP(op_is_number)
        CTI_COMPILE_UNARY_OP(op_is_string)
        CTI_COMPILE_UNARY_OP(op_is_object)
        CTI_COMPILE_UNARY_OP(op_is_function)
        case op_stricteq: {
            compileOpStrictEq(instruction + i, i, OpStrictEq);
            i += 4;
            break;
        }
        case op_nstricteq: {
            compileOpStrictEq(instruction + i, i, OpNStrictEq);
            i += 4;
            break;
        }
        case op_to_jsnumber: {
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            
            m_jit.testl_i32r(JSImmediate::TagBitTypeInteger, X86::eax);
            X86Assembler::JmpSrc wasImmediate = m_jit.emitUnlinkedJnz();

            emitJumpSlowCaseIfNotJSCell(X86::eax, i);

            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::ecx);
            m_jit.cmpl_i32m(NumberType, OBJECT_OFFSET(StructureID, m_typeInfo.m_type), X86::ecx);
            
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            
            m_jit.link(wasImmediate, m_jit.label());

            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_in: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
            emitGetPutArg(instruction[i + 3].u.operand, 4, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_in);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_push_new_scope: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitGetPutArg(instruction[i + 3].u.operand, 4, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_push_new_scope);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_catch: {
            emitGetCTIParam(CTI_ARGS_callFrame, X86::edi); // edi := r
            emitPutResult(instruction[i + 1].u.operand);
            i += 2;
            break;
        }
        case op_jmp_scopes: {
            unsigned count = instruction[i + 1].u.operand;
            emitPutArgConstant(count, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_jmp_scopes);
            unsigned target = instruction[i + 2].u.operand;
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 2 + target));
            i += 3;
            break;
        }
        case op_put_by_index: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, X86::ecx);
            emitPutArgConstant(instruction[i + 2].u.operand, 4);
            emitGetPutArg(instruction[i + 3].u.operand, 8, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_put_by_index);
            i += 4;
            break;
        }
        case op_switch_imm: {
            unsigned tableIndex = instruction[i + 1].u.operand;
            unsigned defaultOffset = instruction[i + 2].u.operand;
            unsigned scrutinee = instruction[i + 3].u.operand;

            // create jump table for switch destinations, track this switch statement.
            SimpleJumpTable* jumpTable = &m_codeBlock->immediateSwitchJumpTables[tableIndex];
            m_switches.append(SwitchRecord(jumpTable, i, defaultOffset, SwitchRecord::Immediate));
            jumpTable->ctiOffsets.grow(jumpTable->branchOffsets.size());

            emitGetPutArg(scrutinee, 0, X86::ecx);
            emitPutArgConstant(tableIndex, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_switch_imm);
            m_jit.jmp_r(X86::eax);
            i += 4;
            break;
        }
        case op_switch_char: {
            unsigned tableIndex = instruction[i + 1].u.operand;
            unsigned defaultOffset = instruction[i + 2].u.operand;
            unsigned scrutinee = instruction[i + 3].u.operand;

            // create jump table for switch destinations, track this switch statement.
            SimpleJumpTable* jumpTable = &m_codeBlock->characterSwitchJumpTables[tableIndex];
            m_switches.append(SwitchRecord(jumpTable, i, defaultOffset, SwitchRecord::Character));
            jumpTable->ctiOffsets.grow(jumpTable->branchOffsets.size());

            emitGetPutArg(scrutinee, 0, X86::ecx);
            emitPutArgConstant(tableIndex, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_switch_char);
            m_jit.jmp_r(X86::eax);
            i += 4;
            break;
        }
        case op_switch_string: {
            unsigned tableIndex = instruction[i + 1].u.operand;
            unsigned defaultOffset = instruction[i + 2].u.operand;
            unsigned scrutinee = instruction[i + 3].u.operand;

            // create jump table for switch destinations, track this switch statement.
            StringJumpTable* jumpTable = &m_codeBlock->stringSwitchJumpTables[tableIndex];
            m_switches.append(SwitchRecord(jumpTable, i, defaultOffset));

            emitGetPutArg(scrutinee, 0, X86::ecx);
            emitPutArgConstant(tableIndex, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_switch_string);
            m_jit.jmp_r(X86::eax);
            i += 4;
            break;
        }
        case op_del_by_val: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
            emitGetPutArg(instruction[i + 3].u.operand, 4, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_del_by_val);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_put_getter: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, X86::ecx);
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitGetPutArg(instruction[i + 3].u.operand, 8, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_put_getter);
            i += 4;
            break;
        }
        case op_put_setter: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, X86::ecx);
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitGetPutArg(instruction[i + 3].u.operand, 8, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_put_setter);
            i += 4;
            break;
        }
        case op_new_error: {
            JSValue* message = m_codeBlock->unexpectedConstants[instruction[i + 3].u.operand];
            emitPutArgConstant(instruction[i + 2].u.operand, 0);
            emitPutArgConstant(asInteger(message), 4);
            emitPutArgConstant(m_codeBlock->lineNumberForVPC(&instruction[i]), 8);
            emitCTICall(instruction + i, i, Machine::cti_op_new_error);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_debug: {
            emitPutArgConstant(instruction[i + 1].u.operand, 0);
            emitPutArgConstant(instruction[i + 2].u.operand, 4);
            emitPutArgConstant(instruction[i + 3].u.operand, 8);
            emitCTICall(instruction + i, i, Machine::cti_op_debug);
            i += 4;
            break;
        }
        case op_eq_null: {
            unsigned dst = instruction[i + 1].u.operand;
            unsigned src1 = instruction[i + 2].u.operand;

            emitGetArg(src1, X86::eax);
            m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
            X86Assembler::JmpSrc isImmediate = m_jit.emitUnlinkedJnz();

            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::ecx);
            m_jit.testl_i32m(MasqueradesAsUndefined, OBJECT_OFFSET(StructureID, m_typeInfo.m_flags), X86::ecx);
            m_jit.setnz_r(X86::eax);

            X86Assembler::JmpSrc wasNotImmediate = m_jit.emitUnlinkedJmp();

            m_jit.link(isImmediate, m_jit.label());

            m_jit.movl_i32r(~JSImmediate::ExtendedTagBitUndefined, X86::ecx);
            m_jit.andl_rr(X86::eax, X86::ecx);
            m_jit.cmpl_i32r(JSImmediate::FullTagTypeNull, X86::ecx);
            m_jit.sete_r(X86::eax);

            m_jit.link(wasNotImmediate, m_jit.label());

            m_jit.movzbl_rr(X86::eax, X86::eax);
            emitTagAsBoolImmediate(X86::eax);
            emitPutResult(dst);

            i += 3;
            break;
        }
        case op_neq_null: {
            unsigned dst = instruction[i + 1].u.operand;
            unsigned src1 = instruction[i + 2].u.operand;

            emitGetArg(src1, X86::eax);
            m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
            X86Assembler::JmpSrc isImmediate = m_jit.emitUnlinkedJnz();

            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::ecx);
            m_jit.testl_i32m(MasqueradesAsUndefined, OBJECT_OFFSET(StructureID, m_typeInfo.m_flags), X86::ecx);
            m_jit.setz_r(X86::eax);

            X86Assembler::JmpSrc wasNotImmediate = m_jit.emitUnlinkedJmp();

            m_jit.link(isImmediate, m_jit.label());

            m_jit.movl_i32r(~JSImmediate::ExtendedTagBitUndefined, X86::ecx);
            m_jit.andl_rr(X86::eax, X86::ecx);
            m_jit.cmpl_i32r(JSImmediate::FullTagTypeNull, X86::ecx);
            m_jit.setne_r(X86::eax);

            m_jit.link(wasNotImmediate, m_jit.label());

            m_jit.movzbl_rr(X86::eax, X86::eax);
            emitTagAsBoolImmediate(X86::eax);
            emitPutResult(dst);

            i += 3;
            break;
        }
        case op_enter: {
            // Even though CTI doesn't use them, we initialize our constant
            // registers to zap stale pointers, to avoid unnecessarily prolonging
            // object lifetime and increasing GC pressure.
            size_t count = m_codeBlock->numVars + m_codeBlock->constantRegisters.size();
            for (size_t j = 0; j < count; ++j)
                emitInitRegister(j);

            i+= 1;
            break;
        }
        case op_enter_with_activation: {
            // Even though CTI doesn't use them, we initialize our constant
            // registers to zap stale pointers, to avoid unnecessarily prolonging
            // object lifetime and increasing GC pressure.
            size_t count = m_codeBlock->numVars + m_codeBlock->constantRegisters.size();
            for (size_t j = 0; j < count; ++j)
                emitInitRegister(j);

            emitCTICall(instruction + i, i, Machine::cti_op_push_activation);
            emitPutResult(instruction[i + 1].u.operand);

            i+= 2;
            break;
        }
        case op_create_arguments: {
            emitCTICall(instruction + i, i, (m_codeBlock->numParameters == 1) ? Machine::cti_op_create_arguments_no_params : Machine::cti_op_create_arguments);
            i += 1;
            break;
        }
        case op_convert_this: {
            emitGetArg(instruction[i + 1].u.operand, X86::eax);

            emitJumpSlowCaseIfNotJSCell(X86::eax, i);
            m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::edx);
            m_jit.testl_i32m(NeedsThisConversion, OBJECT_OFFSET(StructureID, m_typeInfo.m_flags), X86::edx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJnz(), i));

            i += 2;
            break;
        }
        case op_profile_will_call: {
            emitGetCTIParam(CTI_ARGS_profilerReference, X86::eax);
            m_jit.cmpl_i32m(0, X86::eax);
            X86Assembler::JmpSrc noProfiler = m_jit.emitUnlinkedJe();
            emitGetPutArg(instruction[i + 1].u.operand, 0, X86::eax);
            emitCTICall(instruction + i, i, Machine::cti_op_profile_will_call);
            m_jit.link(noProfiler, m_jit.label());

            i += 2;
            break;
        }
        case op_profile_did_call: {
            emitGetCTIParam(CTI_ARGS_profilerReference, X86::eax);
            m_jit.cmpl_i32m(0, X86::eax);
            X86Assembler::JmpSrc noProfiler = m_jit.emitUnlinkedJe();
            emitGetPutArg(instruction[i + 1].u.operand, 0, X86::eax);
            emitCTICall(instruction + i, i, Machine::cti_op_profile_did_call);
            m_jit.link(noProfiler, m_jit.label());

            i += 2;
            break;
        }
        case op_get_array_length:
        case op_get_by_id_chain:
        case op_get_by_id_generic:
        case op_get_by_id_proto:
        case op_get_by_id_self:
        case op_get_string_length:
        case op_put_by_id_generic:
        case op_put_by_id_replace:
        case op_put_by_id_transition:
            ASSERT_NOT_REACHED();
        }
    }

    ASSERT(propertyAccessInstructionIndex == m_codeBlock->propertyAccessInstructions.size());
    ASSERT(callLinkInfoIndex == m_codeBlock->callLinkInfos.size());
}


void CTI::privateCompileLinkPass()
{
    unsigned jmpTableCount = m_jmpTable.size();
    for (unsigned i = 0; i < jmpTableCount; ++i)
        m_jit.link(m_jmpTable[i].from, m_labels[m_jmpTable[i].to]);
    m_jmpTable.clear();
}

#define CTI_COMPILE_BINARY_OP_SLOW_CASE(name) \
    case name: { \
        m_jit.link(iter->from, m_jit.label()); \
        emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx); \
        emitGetPutArg(instruction[i + 3].u.operand, 4, X86::ecx); \
        emitCTICall(instruction + i, i, Machine::cti_##name); \
        emitPutResult(instruction[i + 1].u.operand); \
        i += 4; \
        break; \
    }
    
void CTI::privateCompileSlowCases()
{
    unsigned propertyAccessInstructionIndex = 0;
    unsigned callLinkInfoIndex = 0;

    Instruction* instruction = m_codeBlock->instructions.begin();
    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end(); ++iter) {
        unsigned i = iter->to;
        switch (OpcodeID opcodeID = m_machine->getOpcodeID(instruction[i].u.opcode)) {
        case op_convert_this: {
            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_convert_this);
            emitPutResult(instruction[i + 1].u.operand);
            i += 2;
            break;
        }
        case op_add: {
            unsigned dst = instruction[i + 1].u.operand;
            unsigned src1 = instruction[i + 2].u.operand;
            unsigned src2 = instruction[i + 3].u.operand;
            if (JSValue* value = getConstantImmediateNumericArg(src1)) {
                X86Assembler::JmpSrc notImm = iter->from;
                m_jit.link((++iter)->from, m_jit.label());
                m_jit.subl_i32r(getDeTaggedConstantImmediate(value), X86::edx);
                m_jit.link(notImm, m_jit.label());
                emitGetPutArg(src1, 0, X86::ecx);
                emitPutArg(X86::edx, 4);
                emitCTICall(instruction + i, i, Machine::cti_op_add);
                emitPutResult(dst);
            } else if (JSValue* value = getConstantImmediateNumericArg(src2)) {
                X86Assembler::JmpSrc notImm = iter->from;
                m_jit.link((++iter)->from, m_jit.label());
                m_jit.subl_i32r(getDeTaggedConstantImmediate(value), X86::eax);
                m_jit.link(notImm, m_jit.label());
                emitPutArg(X86::eax, 0);
                emitGetPutArg(src2, 4, X86::ecx);
                emitCTICall(instruction + i, i, Machine::cti_op_add);
                emitPutResult(dst);
            } else {
                OperandTypes types = OperandTypes::fromInt(instruction[i + 4].u.operand);
                if (types.first().mightBeNumber() && types.second().mightBeNumber())
                    compileBinaryArithOpSlowCase(instruction + i, op_add, iter, dst, src1, src2, types, i);
                else
                    ASSERT_NOT_REACHED();
            }

            i += 5;
            break;
        }
        case op_get_by_val: {
            // The slow case that handles accesses to arrays (below) may jump back up to here. 
            X86Assembler::JmpDst beginGetByValSlow = m_jit.label();

            X86Assembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitFastArithIntToImmNoCheck(X86::edx);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::edx, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_get_by_val);
            emitPutResult(instruction[i + 1].u.operand);
            m_jit.link(m_jit.emitUnlinkedJmp(), m_labels[i + 4]);

            // This is slow case that handles accesses to arrays above the fast cut-off.
            // First, check if this is an access to the vector
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.cmpl_rm(X86::edx, OBJECT_OFFSET(ArrayStorage, m_vectorLength), X86::ecx);
            m_jit.link(m_jit.emitUnlinkedJbe(), beginGetByValSlow);

            // okay, missed the fast region, but it is still in the vector.  Get the value.
            m_jit.movl_mr(OBJECT_OFFSET(ArrayStorage, m_vector[0]), X86::ecx, X86::edx, sizeof(JSValue*), X86::ecx);
            // Check whether the value loaded is zero; if so we need to return undefined.
            m_jit.testl_rr(X86::ecx, X86::ecx);
            m_jit.link(m_jit.emitUnlinkedJe(), beginGetByValSlow);
            emitPutResult(instruction[i + 1].u.operand, X86::ecx);
            
            i += 4;
            break;
        }
        case op_sub: {
            compileBinaryArithOpSlowCase(instruction + i, op_sub, iter, instruction[i + 1].u.operand, instruction[i + 2].u.operand, instruction[i + 3].u.operand, OperandTypes::fromInt(instruction[i + 4].u.operand), i);
            i += 5;
            break;
        }
        case op_negate: {
            m_jit.link(iter->from, m_jit.label());
            emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_negate);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_rshift: {
            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::ecx, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_rshift);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_lshift: {
            X86Assembler::JmpSrc notImm1 = iter->from;
            X86Assembler::JmpSrc notImm2 = (++iter)->from;
            m_jit.link((++iter)->from, m_jit.label());
            emitGetArg(instruction[i + 2].u.operand, X86::eax);
            emitGetArg(instruction[i + 3].u.operand, X86::ecx);
            m_jit.link(notImm1, m_jit.label());
            m_jit.link(notImm2, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::ecx, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_lshift);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_loop_if_less: {
            emitSlowScriptCheck(instruction + i, i);

            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                m_jit.link(iter->from, m_jit.label());
                emitPutArg(X86::edx, 0);
                emitGetPutArg(instruction[i + 2].u.operand, 4, X86::ecx);
                emitCTICall(instruction + i, i, Machine::cti_op_loop_if_less);
                m_jit.testl_rr(X86::eax, X86::eax);
                m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 3 + target]);
            } else {
                m_jit.link(iter->from, m_jit.label());
                m_jit.link((++iter)->from, m_jit.label());
                emitPutArg(X86::eax, 0);
                emitPutArg(X86::edx, 4);
                emitCTICall(instruction + i, i, Machine::cti_op_loop_if_less);
                m_jit.testl_rr(X86::eax, X86::eax);
                m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 3 + target]);
            }
            i += 4;
            break;
        }
        case op_put_by_id: {
            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());

            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::edx, 8);
            X86Assembler::JmpSrc call = emitCTICall(instruction + i, i, Machine::cti_op_put_by_id);

            // Track the location of the call; this will be used to recover repatch information.
            ASSERT(m_codeBlock->propertyAccessInstructions[propertyAccessInstructionIndex].opcodeIndex == i);
            m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].callReturnLocation = call;
            ++propertyAccessInstructionIndex;

            i += 8;
            break;
        }
        case op_get_by_id: {
            // As for the hot path of get_by_id, above, we ensure that we can use an architecture specific offset
            // so that we only need track one pointer into the slow case code - we track a pointer to the location
            // of the call (which we can use to look up the repatch information), but should a array-length or
            // prototype access trampoline fail we want to bail out back to here.  To do so we can subtract back
            // the distance from the call to the head of the slow case.

            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());

#ifndef NDEBUG
            X86Assembler::JmpDst coldPathBegin = m_jit.label();
#endif        
            emitPutArg(X86::eax, 0);
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            X86Assembler::JmpSrc call = emitCTICall(instruction + i, i, Machine::cti_op_get_by_id);
            ASSERT(X86Assembler::getDifferenceBetweenLabels(coldPathBegin, call) == repatchOffsetGetByIdSlowCaseCall);
            emitPutResult(instruction[i + 1].u.operand);

            // Track the location of the call; this will be used to recover repatch information.
            ASSERT(m_codeBlock->propertyAccessInstructions[propertyAccessInstructionIndex].opcodeIndex == i);
            m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].callReturnLocation = call;
            ++propertyAccessInstructionIndex;

            i += 8;
            break;
        }
        case op_loop_if_lesseq: {
            emitSlowScriptCheck(instruction + i, i);

            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                m_jit.link(iter->from, m_jit.label());
                emitPutArg(X86::edx, 0);
                emitGetPutArg(instruction[i + 2].u.operand, 4, X86::ecx);
                emitCTICall(instruction + i, i, Machine::cti_op_loop_if_lesseq);
                m_jit.testl_rr(X86::eax, X86::eax);
                m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 3 + target]);
            } else {
                m_jit.link(iter->from, m_jit.label());
                m_jit.link((++iter)->from, m_jit.label());
                emitPutArg(X86::eax, 0);
                emitPutArg(X86::edx, 4);
                emitCTICall(instruction + i, i, Machine::cti_op_loop_if_lesseq);
                m_jit.testl_rr(X86::eax, X86::eax);
                m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 3 + target]);
            }
            i += 4;
            break;
        }
        case op_pre_inc: {
            unsigned srcDst = instruction[i + 1].u.operand;
            X86Assembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.subl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), X86::eax);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_pre_inc);
            emitPutResult(srcDst);
            i += 2;
            break;
        }
        case op_put_by_val: {
            // Normal slow cases - either is not an immediate imm, or is an array.
            X86Assembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitFastArithIntToImmNoCheck(X86::edx);
            m_jit.link(notImm, m_jit.label());
            emitGetArg(instruction[i + 3].u.operand, X86::ecx);
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::edx, 4);
            emitPutArg(X86::ecx, 8);
            emitCTICall(instruction + i, i, Machine::cti_op_put_by_val);
            m_jit.link(m_jit.emitUnlinkedJmp(), m_labels[i + 4]);

            // slow cases for immediate int accesses to arrays
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitGetArg(instruction[i + 3].u.operand, X86::ecx);
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::edx, 4);
            emitPutArg(X86::ecx, 8);
            emitCTICall(instruction + i, i, Machine::cti_op_put_by_val_array);

            i += 4;
            break;
        }
        case op_loop_if_true: {
            emitSlowScriptCheck(instruction + i, i);

            m_jit.link(iter->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_jtrue);
            m_jit.testl_rr(X86::eax, X86::eax);
            unsigned target = instruction[i + 2].u.operand;
            m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 2 + target]);
            i += 3;
            break;
        }
        case op_pre_dec: {
            unsigned srcDst = instruction[i + 1].u.operand;
            X86Assembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.addl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), X86::eax);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_pre_dec);
            emitPutResult(srcDst);
            i += 2;
            break;
        }
        case op_jnless: {
            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                m_jit.link(iter->from, m_jit.label());
                emitPutArg(X86::edx, 0);
                emitGetPutArg(instruction[i + 2].u.operand, 4, X86::ecx);
                emitCTICall(instruction + i, i, Machine::cti_op_jless);
                m_jit.testl_rr(X86::eax, X86::eax);
                m_jit.link(m_jit.emitUnlinkedJe(), m_labels[i + 3 + target]);
            } else {
                m_jit.link(iter->from, m_jit.label());
                m_jit.link((++iter)->from, m_jit.label());
                emitPutArg(X86::eax, 0);
                emitPutArg(X86::edx, 4);
                emitCTICall(instruction + i, i, Machine::cti_op_jless);
                m_jit.testl_rr(X86::eax, X86::eax);
                m_jit.link(m_jit.emitUnlinkedJe(), m_labels[i + 3 + target]);
            }
            i += 4;
            break;
        }
        case op_not: {
            m_jit.link(iter->from, m_jit.label());
            m_jit.xorl_i8r(JSImmediate::FullTagTypeBool, X86::eax);
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_not);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_jfalse: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_jtrue);
            m_jit.testl_rr(X86::eax, X86::eax);
            unsigned target = instruction[i + 2].u.operand;
            m_jit.link(m_jit.emitUnlinkedJe(), m_labels[i + 2 + target]); // inverted!
            i += 3;
            break;
        }
        case op_post_inc: {
            unsigned srcDst = instruction[i + 2].u.operand;
            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_post_inc);
            emitPutResult(instruction[i + 1].u.operand);
            emitPutResult(srcDst, X86::edx);
            i += 3;
            break;
        }
        case op_bitnot: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_bitnot);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_bitand: {
            unsigned src1 = instruction[i + 2].u.operand;
            unsigned src2 = instruction[i + 3].u.operand;
            unsigned dst = instruction[i + 1].u.operand;
            if (getConstantImmediateNumericArg(src1)) {
                m_jit.link(iter->from, m_jit.label());
                emitGetPutArg(src1, 0, X86::ecx);
                emitPutArg(X86::eax, 4);
                emitCTICall(instruction + i, i, Machine::cti_op_bitand);
                emitPutResult(dst);
            } else if (getConstantImmediateNumericArg(src2)) {
                m_jit.link(iter->from, m_jit.label());
                emitPutArg(X86::eax, 0);
                emitGetPutArg(src2, 4, X86::ecx);
                emitCTICall(instruction + i, i, Machine::cti_op_bitand);
                emitPutResult(dst);
            } else {
                m_jit.link(iter->from, m_jit.label());
                emitGetPutArg(src1, 0, X86::ecx);
                emitPutArg(X86::edx, 4);
                emitCTICall(instruction + i, i, Machine::cti_op_bitand);
                emitPutResult(dst);
            }
            i += 5;
            break;
        }
        case op_jtrue: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_jtrue);
            m_jit.testl_rr(X86::eax, X86::eax);
            unsigned target = instruction[i + 2].u.operand;
            m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 2 + target]);
            i += 3;
            break;
        }
        case op_post_dec: {
            unsigned srcDst = instruction[i + 2].u.operand;
            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_post_dec);
            emitPutResult(instruction[i + 1].u.operand);
            emitPutResult(srcDst, X86::edx);
            i += 3;
            break;
        }
        case op_bitxor: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::edx, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_bitxor);
            emitPutResult(instruction[i + 1].u.operand);
            i += 5;
            break;
        }
        case op_bitor: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::edx, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_bitor);
            emitPutResult(instruction[i + 1].u.operand);
            i += 5;
            break;
        }
        case op_eq: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::edx, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_eq);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_neq: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::edx, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_neq);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        CTI_COMPILE_BINARY_OP_SLOW_CASE(op_stricteq);
        CTI_COMPILE_BINARY_OP_SLOW_CASE(op_nstricteq);
        case op_instanceof: {
            m_jit.link(iter->from, m_jit.label());
            emitGetPutArg(instruction[i + 2].u.operand, 0, X86::ecx);
            emitGetPutArg(instruction[i + 3].u.operand, 4, X86::ecx);
            emitGetPutArg(instruction[i + 4].u.operand, 8, X86::ecx);
            emitCTICall(instruction + i, i, Machine::cti_op_instanceof);
            emitPutResult(instruction[i + 1].u.operand);
            i += 5;
            break;
        }
        case op_mod: {
            X86Assembler::JmpSrc notImm1 = iter->from;
            X86Assembler::JmpSrc notImm2 = (++iter)->from;
            m_jit.link((++iter)->from, m_jit.label());
            emitFastArithReTagImmediate(X86::eax);
            emitFastArithReTagImmediate(X86::ecx);
            m_jit.link(notImm1, m_jit.label());
            m_jit.link(notImm2, m_jit.label());
            emitPutArg(X86::eax, 0);
            emitPutArg(X86::ecx, 4);
            emitCTICall(instruction + i, i, Machine::cti_op_mod);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_mul: {
            int dst = instruction[i + 1].u.operand;
            int src1 = instruction[i + 2].u.operand;
            int src2 = instruction[i + 3].u.operand;
            JSValue* src1Value = getConstantImmediateNumericArg(src1);
            JSValue* src2Value = getConstantImmediateNumericArg(src2);
            int32_t value;
            if (src1Value && ((value = JSImmediate::intValue(src1Value)) > 0)) {
                m_jit.link(iter->from, m_jit.label());
                // There is an extra slow case for (op1 * -N) or (-N * op2), to check for 0 since this should produce a result of -0.
                emitGetPutArg(src1, 0, X86::ecx);
                emitGetPutArg(src2, 4, X86::ecx);
                emitCTICall(instruction + i, i, Machine::cti_op_mul);
                emitPutResult(dst);
            } else if (src2Value && ((value = JSImmediate::intValue(src2Value)) > 0)) {
                m_jit.link(iter->from, m_jit.label());
                // There is an extra slow case for (op1 * -N) or (-N * op2), to check for 0 since this should produce a result of -0.
                emitGetPutArg(src1, 0, X86::ecx);
                emitGetPutArg(src2, 4, X86::ecx);
                emitCTICall(instruction + i, i, Machine::cti_op_mul);
                emitPutResult(dst);
            } else
                compileBinaryArithOpSlowCase(instruction + i, op_mul, iter, dst, src1, src2, OperandTypes::fromInt(instruction[i + 4].u.operand), i);
            i += 5;
            break;
        }

        case op_call:
        case op_call_eval:
        case op_construct: {
            int dst = instruction[i + 1].u.operand;
            int callee = instruction[i + 2].u.operand;
            int argCount = instruction[i + 3].u.operand;
            int registerOffset = instruction[i + 4].u.operand;

            m_jit.link(iter->from, m_jit.label());

            // The arguments have been set up on the hot path for op_call_eval
            if (opcodeID == op_call)
                compileOpCallSetupArgs(instruction + i);
            else if (opcodeID == op_construct)
                compileOpConstructSetupArgs(instruction + i);

            // Fast check for JS function.
            m_jit.testl_i32r(JSImmediate::TagMask, X86::ecx);
            X86Assembler::JmpSrc callLinkFailNotObject = m_jit.emitUnlinkedJne();
            m_jit.cmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsFunctionVptr), X86::ecx);
            X86Assembler::JmpSrc callLinkFailNotJSFunction = m_jit.emitUnlinkedJne();

            // First, in the case of a construct, allocate the new object.
            if (opcodeID == op_construct) {
                emitCTICall(instruction, i, Machine::cti_op_construct_JSConstruct);
                emitPutResult(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
                emitGetArg(callee, X86::ecx);
            }

            // Load the callee CodeBlock* into eax
            m_jit.movl_mr(OBJECT_OFFSET(JSFunction, m_body), X86::ecx, X86::eax);
            m_jit.movl_mr(OBJECT_OFFSET(FunctionBodyNode, m_code), X86::eax, X86::eax);
            m_jit.testl_rr(X86::eax, X86::eax);
            X86Assembler::JmpSrc hasCodeBlockForLink = m_jit.emitUnlinkedJne();
            emitCTICall(instruction + i, i, Machine::cti_op_call_JSFunction);
            emitGetArg(callee, X86::ecx);
            m_jit.link(hasCodeBlockForLink, m_jit.label());

            // Speculatively roll the callframe, assuming argCount will match the arity.
            m_jit.movl_rm(X86::edi, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register)), X86::edi);
            m_jit.addl_i32r(registerOffset * static_cast<int>(sizeof(Register)), X86::edi);

            // Check argCount matches callee arity.
            m_jit.cmpl_i32m(argCount, OBJECT_OFFSET(CodeBlock, numParameters), X86::eax);
            X86Assembler::JmpSrc arityCheckOkayForLink = m_jit.emitUnlinkedJe();
            emitPutArg(X86::eax, 12);
            emitCTICall(instruction + i, i, Machine::cti_op_call_arityCheck);
            emitGetArg(callee - registerOffset, X86::ecx);
            m_jit.movl_rr(X86::edx, X86::edi);
            m_jit.link(arityCheckOkayForLink, m_jit.label());

            // initialize the new call frame (pointed to by edx, after the last call).
            compileOpCallInitializeCallFrame(callee, argCount);

            // Try to link & repatch this call.
            CallLinkInfo* info = &(m_codeBlock->callLinkInfos[callLinkInfoIndex]);
            emitPutArgConstant(reinterpret_cast<unsigned>(info), 4);
            m_callStructureStubCompilationInfo[callLinkInfoIndex].callReturnLocation =
                emitCTICall(instruction + i, i, Machine::cti_vm_lazyLinkCall);
            emitNakedCall(i, X86::eax);
            X86Assembler::JmpSrc storeResultForFirstRun = m_jit.emitUnlinkedJmp();

            // This is the address for the cold path *after* the first run (which tries to link the call).
            m_callStructureStubCompilationInfo[callLinkInfoIndex].coldPathOther = m_jit.label();

            // The arguments have been set up on the hot path for op_call_eval
            if (opcodeID == op_call)
                compileOpCallSetupArgs(instruction + i);
            else if (opcodeID == op_construct)
                compileOpConstructSetupArgs(instruction + i);

            // Check for JSFunctions.
            m_jit.testl_i32r(JSImmediate::TagMask, X86::ecx);
            X86Assembler::JmpSrc isNotObject = m_jit.emitUnlinkedJne();
            m_jit.cmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsFunctionVptr), X86::ecx);
            X86Assembler::JmpSrc isJSFunction = m_jit.emitUnlinkedJe();

            // This handles host functions
            X86Assembler::JmpDst notJSFunctionlabel = m_jit.label();
            m_jit.link(isNotObject, notJSFunctionlabel);
            m_jit.link(callLinkFailNotObject, notJSFunctionlabel);
            m_jit.link(callLinkFailNotJSFunction, notJSFunctionlabel);
            emitCTICall(instruction + i, i, ((opcodeID == op_construct) ? Machine::cti_op_construct_NotJSConstruct : Machine::cti_op_call_NotJSFunction));
            X86Assembler::JmpSrc wasNotJSFunction = m_jit.emitUnlinkedJmp();

            // Next, handle JSFunctions...
            m_jit.link(isJSFunction, m_jit.label());

            // First, in the case of a construct, allocate the new object.
            if (opcodeID == op_construct) {
                emitCTICall(instruction, i, Machine::cti_op_construct_JSConstruct);
                emitPutResult(registerOffset - RegisterFile::CallFrameHeaderSize - argCount);
                emitGetArg(callee, X86::ecx);
            }

            // Load the callee CodeBlock* into eax
            m_jit.movl_mr(OBJECT_OFFSET(JSFunction, m_body), X86::ecx, X86::eax);
            m_jit.movl_mr(OBJECT_OFFSET(FunctionBodyNode, m_code), X86::eax, X86::eax);
            m_jit.testl_rr(X86::eax, X86::eax);
            X86Assembler::JmpSrc hasCodeBlock = m_jit.emitUnlinkedJne();
            emitCTICall(instruction + i, i, Machine::cti_op_call_JSFunction);
            emitGetArg(callee, X86::ecx);
            m_jit.link(hasCodeBlock, m_jit.label());

            // Speculatively roll the callframe, assuming argCount will match the arity.
            m_jit.movl_rm(X86::edi, (RegisterFile::CallerFrame + registerOffset) * static_cast<int>(sizeof(Register)), X86::edi);
            m_jit.addl_i32r(registerOffset * static_cast<int>(sizeof(Register)), X86::edi);

            // Check argCount matches callee arity.
            m_jit.cmpl_i32m(argCount, OBJECT_OFFSET(CodeBlock, numParameters), X86::eax);
            X86Assembler::JmpSrc arityCheckOkay = m_jit.emitUnlinkedJe();
            emitPutArg(X86::eax, 12);
            emitCTICall(instruction + i, i, Machine::cti_op_call_arityCheck);
            emitGetArg(callee - registerOffset, X86::ecx);
            m_jit.movl_rr(X86::edx, X86::edi);
            m_jit.link(arityCheckOkay, m_jit.label());

            // initialize the new call frame (pointed to by edx, after the last call).
            compileOpCallInitializeCallFrame(callee, argCount);

            // load ctiCode from the new codeBlock.
            m_jit.movl_mr(OBJECT_OFFSET(CodeBlock, ctiCode), X86::eax, X86::eax);

            emitNakedCall(i, X86::eax);

            // Put the return value in dst. In the interpreter, op_ret does this.
            X86Assembler::JmpDst storeResult = m_jit.label();
            m_jit.link(wasNotJSFunction, storeResult);
            m_jit.link(storeResultForFirstRun, storeResult);
            emitPutResult(dst);

#if ENABLE(CODEBLOCK_SAMPLING)
            m_jit.movl_i32m(reinterpret_cast<unsigned>(m_codeBlock), m_machine->sampler()->codeBlockSlot());
#endif
            ++callLinkInfoIndex;

            i += (opcodeID == op_construct ? 7 : 5);
            break;
        }
        case op_to_jsnumber: {
            m_jit.link(iter->from, m_jit.label());
            m_jit.link(iter->from, m_jit.label());

            emitPutArg(X86::eax, 0);
            emitCTICall(instruction + i, i, Machine::cti_op_to_jsnumber);

            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }

        default:
            ASSERT_NOT_REACHED();
            break;
        }

        m_jit.link(m_jit.emitUnlinkedJmp(), m_labels[i]);
    }

    ASSERT(propertyAccessInstructionIndex == m_codeBlock->propertyAccessInstructions.size());
    ASSERT(callLinkInfoIndex == m_codeBlock->callLinkInfos.size());
}

void CTI::privateCompile()
{
#if ENABLE(CODEBLOCK_SAMPLING)
        m_jit.movl_i32m(reinterpret_cast<unsigned>(m_codeBlock), m_machine->sampler()->codeBlockSlot());
#endif
#if ENABLE(OPCODE_SAMPLING)
        m_jit.movl_i32m(m_machine->sampler()->encodeSample(m_codeBlock->instructions.begin()), m_machine->sampler()->sampleSlot());
#endif

    // Could use a popl_m, but would need to offset the following instruction if so.
    m_jit.popl_r(X86::ecx);
    emitPutToCallFrameHeader(X86::ecx, RegisterFile::ReturnPC);

    X86Assembler::JmpSrc slowRegisterFileCheck;
    X86Assembler::JmpDst afterRegisterFileCheck;
    if (m_codeBlock->codeType == FunctionCode) {
        // In the case of a fast linked call, we do not set this up in the caller.
        m_jit.movl_i32m(reinterpret_cast<unsigned>(m_codeBlock), RegisterFile::CodeBlock * static_cast<int>(sizeof(Register)), X86::edi);

        emitGetCTIParam(CTI_ARGS_registerFile, X86::eax);
        m_jit.leal_mr(m_codeBlock->numCalleeRegisters * sizeof(Register), X86::edi, X86::edx);
        m_jit.cmpl_mr(OBJECT_OFFSET(RegisterFile, m_end), X86::eax, X86::edx);
        slowRegisterFileCheck = m_jit.emitUnlinkedJg();
        afterRegisterFileCheck = m_jit.label();
    }

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();

    if (m_codeBlock->codeType == FunctionCode) {
        m_jit.link(slowRegisterFileCheck, m_jit.label());
        emitCTICall(m_codeBlock->instructions.begin(), 0, Machine::cti_register_file_check);
        X86Assembler::JmpSrc backToBody = m_jit.emitUnlinkedJmp();
        m_jit.link(backToBody, afterRegisterFileCheck);
    }

    ASSERT(m_jmpTable.isEmpty());

    void* code = m_jit.copy();
    ASSERT(code);

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (unsigned i = 0; i < m_switches.size(); ++i) {
        SwitchRecord record = m_switches[i];
        unsigned opcodeIndex = record.opcodeIndex;

        if (record.type != SwitchRecord::String) {
            ASSERT(record.type == SwitchRecord::Immediate || record.type == SwitchRecord::Character); 
            ASSERT(record.jumpTable.simpleJumpTable->branchOffsets.size() == record.jumpTable.simpleJumpTable->ctiOffsets.size());

            record.jumpTable.simpleJumpTable->ctiDefault = m_jit.getRelocatedAddress(code, m_labels[opcodeIndex + 3 + record.defaultOffset]);

            for (unsigned j = 0; j < record.jumpTable.simpleJumpTable->branchOffsets.size(); ++j) {
                unsigned offset = record.jumpTable.simpleJumpTable->branchOffsets[j];
                record.jumpTable.simpleJumpTable->ctiOffsets[j] = offset ? m_jit.getRelocatedAddress(code, m_labels[opcodeIndex + 3 + offset]) : record.jumpTable.simpleJumpTable->ctiDefault;
            }
        } else {
            ASSERT(record.type == SwitchRecord::String);

            record.jumpTable.stringJumpTable->ctiDefault = m_jit.getRelocatedAddress(code, m_labels[opcodeIndex + 3 + record.defaultOffset]);

            StringJumpTable::StringOffsetTable::iterator end = record.jumpTable.stringJumpTable->offsetTable.end();            
            for (StringJumpTable::StringOffsetTable::iterator it = record.jumpTable.stringJumpTable->offsetTable.begin(); it != end; ++it) {
                unsigned offset = it->second.branchOffset;
                it->second.ctiOffset = offset ? m_jit.getRelocatedAddress(code, m_labels[opcodeIndex + 3 + offset]) : record.jumpTable.stringJumpTable->ctiDefault;
            }
        }
    }

    for (Vector<HandlerInfo>::iterator iter = m_codeBlock->exceptionHandlers.begin(); iter != m_codeBlock->exceptionHandlers.end(); ++iter)
         iter->nativeCode = m_jit.getRelocatedAddress(code, m_labels[iter->target]);

    for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
        if (iter->to)
            X86Assembler::link(code, iter->from, iter->to);
        m_codeBlock->ctiReturnAddressVPCMap.add(m_jit.getRelocatedAddress(code, iter->from), iter->opcodeIndex);
    }

    // Link absolute addresses for jsr
    for (Vector<JSRInfo>::iterator iter = m_jsrSites.begin(); iter != m_jsrSites.end(); ++iter)
        X86Assembler::linkAbsoluteAddress(code, iter->addrPosition, iter->target);

    for (unsigned i = 0; i < m_codeBlock->propertyAccessInstructions.size(); ++i) {
        StructureStubInfo& info = m_codeBlock->propertyAccessInstructions[i];
        info.callReturnLocation = X86Assembler::getRelocatedAddress(code, m_propertyAccessCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = X86Assembler::getRelocatedAddress(code, m_propertyAccessCompilationInfo[i].hotPathBegin);
    }
    for (unsigned i = 0; i < m_codeBlock->callLinkInfos.size(); ++i) {
        CallLinkInfo& info = m_codeBlock->callLinkInfos[i];
        info.callReturnLocation = X86Assembler::getRelocatedAddress(code, m_callStructureStubCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = X86Assembler::getRelocatedAddress(code, m_callStructureStubCompilationInfo[i].hotPathBegin);
        info.hotPathOther = X86Assembler::getRelocatedAddress(code, m_callStructureStubCompilationInfo[i].hotPathOther);
        info.coldPathOther = X86Assembler::getRelocatedAddress(code, m_callStructureStubCompilationInfo[i].coldPathOther);
    }

    m_codeBlock->ctiCode = code;
}

void CTI::privateCompileGetByIdSelf(StructureID* structureID, size_t cachedOffset, void* returnAddress)
{
    // Check eax is an object of the right StructureID.
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    X86Assembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
    X86Assembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // Checks out okay! - getDirectOffset
    m_jit.movl_mr(OBJECT_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    m_jit.movl_mr(cachedOffset * sizeof(JSValue*), X86::eax, X86::eax);
    m_jit.ret();

    void* code = m_jit.copy();
    ASSERT(code);

    X86Assembler::link(code, failureCases1, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    X86Assembler::link(code, failureCases2, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    
    m_codeBlock->getStubInfo(returnAddress).stubRoutine = code;
    
    ctiRepatchCallByReturnAddress(returnAddress, code);
}

void CTI::privateCompileGetByIdProto(StructureID* structureID, StructureID* prototypeStructureID, size_t cachedOffset, void* returnAddress, CallFrame* callFrame)
{
#if USE(CTI_REPATCH_PIC)
    StructureStubInfo& info = m_codeBlock->getStubInfo(returnAddress);

    // We don't want to repatch more than once - in future go to cti_op_put_by_id_generic.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));

    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a StructureID that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structureID->prototypeForLookup(callFrame));
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    m_jit.movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);

    // check eax is an object of the right StructureID.
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    X86Assembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
    X86Assembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // Check the prototype object's StructureID had not changed.
    StructureID** protoStructureIDAddress = &(protoObject->m_structureID);
    m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(prototypeStructureID), static_cast<void*>(protoStructureIDAddress));
    X86Assembler::JmpSrc failureCases3 = m_jit.emitUnlinkedJne();

    // Checks out okay! - getDirectOffset
    m_jit.movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::ecx);

    X86Assembler::JmpSrc success = m_jit.emitUnlinkedJmp();

    void* code = m_jit.copy();
    ASSERT(code);

    // Use the repatch information to link the failure cases back to the original slow case routine.
    void* slowCaseBegin = reinterpret_cast<char*>(info.callReturnLocation) - repatchOffsetGetByIdSlowCaseCall;
    X86Assembler::link(code, failureCases1, slowCaseBegin);
    X86Assembler::link(code, failureCases2, slowCaseBegin);
    X86Assembler::link(code, failureCases3, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    intptr_t successDest = (intptr_t)(info.hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset;
    X86Assembler::link(code, success, reinterpret_cast<void*>(successDest));

    // Track the stub we have created so that it will be deleted later.
    m_codeBlock->getStubInfo(returnAddress).stubRoutine = code;

    // Finally repatch the jump to sow case back in the hot path to jump here instead.
    // FIXME: should revert this repatching, on failure.
    intptr_t jmpLocation = reinterpret_cast<intptr_t>(info.hotPathBegin) + repatchOffsetGetByIdBranchToSlowCase;
    X86Assembler::repatchBranchOffset(jmpLocation, code);
#else
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a StructureID that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structureID->prototypeForLookup(callFrame));
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    m_jit.movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);

    // check eax is an object of the right StructureID.
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    X86Assembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
    X86Assembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // Check the prototype object's StructureID had not changed.
    StructureID** protoStructureIDAddress = &(protoObject->m_structureID);
    m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(prototypeStructureID), static_cast<void*>(protoStructureIDAddress));
    X86Assembler::JmpSrc failureCases3 = m_jit.emitUnlinkedJne();

    // Checks out okay! - getDirectOffset
    m_jit.movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::eax);

    m_jit.ret();

    void* code = m_jit.copy();
    ASSERT(code);

    X86Assembler::link(code, failureCases1, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    X86Assembler::link(code, failureCases2, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    X86Assembler::link(code, failureCases3, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));

    m_codeBlock->getStubInfo(returnAddress).stubRoutine = code;

    ctiRepatchCallByReturnAddress(returnAddress, code);
#endif
}

void CTI::privateCompileGetByIdChain(StructureID* structureID, StructureIDChain* chain, size_t count, size_t cachedOffset, void* returnAddress, CallFrame* callFrame)
{
    ASSERT(count);
    
    Vector<X86Assembler::JmpSrc> bucketsOfFail;

    // Check eax is an object of the right StructureID.
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    bucketsOfFail.append(m_jit.emitUnlinkedJne());
    m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
    bucketsOfFail.append(m_jit.emitUnlinkedJne());

    StructureID* currStructureID = structureID;
    RefPtr<StructureID>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i<count; ++i) {
        protoObject = asObject(currStructureID->prototypeForLookup(callFrame));
        currStructureID = chainEntries[i].get();

        // Check the prototype object's StructureID had not changed.
        StructureID** protoStructureIDAddress = &(protoObject->m_structureID);
        m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(currStructureID), static_cast<void*>(protoStructureIDAddress));
        bucketsOfFail.append(m_jit.emitUnlinkedJne());
    }
    ASSERT(protoObject);

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    m_jit.movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);
    m_jit.movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::eax);
    m_jit.ret();

    bucketsOfFail.append(m_jit.emitUnlinkedJmp());

    void* code = m_jit.copy();
    ASSERT(code);

    for (unsigned i = 0; i < bucketsOfFail.size(); ++i)
        X86Assembler::link(code, bucketsOfFail[i], reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));

    m_codeBlock->getStubInfo(returnAddress).stubRoutine = code;

    ctiRepatchCallByReturnAddress(returnAddress, code);
}

void CTI::privateCompilePutByIdReplace(StructureID* structureID, size_t cachedOffset, void* returnAddress)
{
    // check eax is an object of the right StructureID.
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    X86Assembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
    X86Assembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // checks out okay! - putDirectOffset
    m_jit.movl_mr(OBJECT_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    m_jit.movl_rm(X86::edx, cachedOffset * sizeof(JSValue*), X86::eax);
    m_jit.ret();

    void* code = m_jit.copy();
    ASSERT(code);
    
    X86Assembler::link(code, failureCases1, reinterpret_cast<void*>(Machine::cti_op_put_by_id_fail));
    X86Assembler::link(code, failureCases2, reinterpret_cast<void*>(Machine::cti_op_put_by_id_fail));

    m_codeBlock->getStubInfo(returnAddress).stubRoutine = code;
    
    ctiRepatchCallByReturnAddress(returnAddress, code);
}

extern "C" {

    static JSObject* resizePropertyStorage(JSObject* baseObject, size_t oldSize, size_t newSize)
    {
        baseObject->allocatePropertyStorageInline(oldSize, newSize);
        return baseObject;
    }

}

static inline bool transitionWillNeedStorageRealloc(StructureID* oldStructureID, StructureID* newStructureID)
{
    return oldStructureID->propertyStorageCapacity() != newStructureID->propertyStorageCapacity();
}

void CTI::privateCompilePutByIdTransition(StructureID* oldStructureID, StructureID* newStructureID, size_t cachedOffset, StructureIDChain* sIDC, void* returnAddress)
{
    Vector<X86Assembler::JmpSrc, 16> failureCases;
    // check eax is an object of the right StructureID.
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    failureCases.append(m_jit.emitUnlinkedJne());
    m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(oldStructureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);
    failureCases.append(m_jit.emitUnlinkedJne());
    Vector<X86Assembler::JmpSrc> successCases;

    //  ecx = baseObject
    m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::eax, X86::ecx);
    // proto(ecx) = baseObject->structureID()->prototype()
    m_jit.cmpl_i32m(ObjectType, OBJECT_OFFSET(StructureID, m_typeInfo) + OBJECT_OFFSET(TypeInfo, m_type), X86::ecx);
    failureCases.append(m_jit.emitUnlinkedJne());
    m_jit.movl_mr(OBJECT_OFFSET(StructureID, m_prototype), X86::ecx, X86::ecx);
    
    // ecx = baseObject->m_structureID
    for (RefPtr<StructureID>* it = sIDC->head(); *it; ++it) {
        // null check the prototype
        m_jit.cmpl_i32r(asInteger(jsNull()), X86::ecx);
        successCases.append(m_jit.emitUnlinkedJe());

        // Check the structure id
        m_jit.cmpl_i32m(reinterpret_cast<uint32_t>(it->get()), OBJECT_OFFSET(JSCell, m_structureID), X86::ecx);
        failureCases.append(m_jit.emitUnlinkedJne());
        
        m_jit.movl_mr(OBJECT_OFFSET(JSCell, m_structureID), X86::ecx, X86::ecx);
        m_jit.cmpl_i32m(ObjectType, OBJECT_OFFSET(StructureID, m_typeInfo) + OBJECT_OFFSET(TypeInfo, m_type), X86::ecx);
        failureCases.append(m_jit.emitUnlinkedJne());
        m_jit.movl_mr(OBJECT_OFFSET(StructureID, m_prototype), X86::ecx, X86::ecx);
    }

    failureCases.append(m_jit.emitUnlinkedJne());
    for (unsigned i = 0; i < successCases.size(); ++i)
        m_jit.link(successCases[i], m_jit.label());

    X86Assembler::JmpSrc callTarget;

    // emit a call only if storage realloc is needed
    if (transitionWillNeedStorageRealloc(oldStructureID, newStructureID)) {
        m_jit.pushl_r(X86::edx);
        m_jit.pushl_i32(newStructureID->propertyStorageCapacity());
        m_jit.pushl_i32(oldStructureID->propertyStorageCapacity());
        m_jit.pushl_r(X86::eax);
        callTarget = m_jit.emitCall();
        m_jit.addl_i32r(3 * sizeof(void*), X86::esp);
        m_jit.popl_r(X86::edx);
    }

    // Assumes m_refCount can be decremented easily, refcount decrement is safe as 
    // codeblock should ensure oldStructureID->m_refCount > 0
    m_jit.subl_i8m(1, reinterpret_cast<void*>(oldStructureID));
    m_jit.addl_i8m(1, reinterpret_cast<void*>(newStructureID));
    m_jit.movl_i32m(reinterpret_cast<uint32_t>(newStructureID), OBJECT_OFFSET(JSCell, m_structureID), X86::eax);

    // write the value
    m_jit.movl_mr(OBJECT_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    m_jit.movl_rm(X86::edx, cachedOffset * sizeof(JSValue*), X86::eax);

    m_jit.ret();
    
    X86Assembler::JmpSrc failureJump;
    if (failureCases.size()) {
        for (unsigned i = 0; i < failureCases.size(); ++i)
            m_jit.link(failureCases[i], m_jit.label());
        m_jit.emitRestoreArgumentReferenceForTrampoline();
        failureJump = m_jit.emitUnlinkedJmp();
    }

    void* code = m_jit.copy();
    ASSERT(code);

    if (failureCases.size())
        X86Assembler::link(code, failureJump, reinterpret_cast<void*>(Machine::cti_op_put_by_id_fail));

    if (transitionWillNeedStorageRealloc(oldStructureID, newStructureID))
        X86Assembler::link(code, callTarget, reinterpret_cast<void*>(resizePropertyStorage));
    
    m_codeBlock->getStubInfo(returnAddress).stubRoutine = code;
    
    ctiRepatchCallByReturnAddress(returnAddress, code);
}

void CTI::unlinkCall(CallLinkInfo* callLinkInfo)
{
    // When the JSFunction is deleted the pointer embedded in the instruction stream will no longer be valid
    // (and, if a new JSFunction happened to be constructed at the same location, we could get a false positive
    // match).  Reset the check so it no longer matches.
    reinterpret_cast<void**>(callLinkInfo->hotPathBegin)[-1] = asPointer(JSImmediate::impossibleValue());
}

void CTI::linkCall(JSFunction* callee, CodeBlock* calleeCodeBlock, void* ctiCode, CallLinkInfo* callLinkInfo, int callerArgCount)
{
    // Currently we only link calls with the exact number of arguments.
    if (callerArgCount == calleeCodeBlock->numParameters) {
        ASSERT(!callLinkInfo->isLinked());
    
        calleeCodeBlock->addCaller(callLinkInfo);
    
        reinterpret_cast<void**>(callLinkInfo->hotPathBegin)[-1] = callee;
        ctiRepatchCallByReturnAddress(callLinkInfo->hotPathOther, ctiCode);
    }

    // repatch the instruction that jumps out to the cold path, so that we only try to link once.
    void* repatchCheck = reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(callLinkInfo->hotPathBegin) + repatchOffsetOpCallCall);
    ctiRepatchCallByReturnAddress(repatchCheck, callLinkInfo->coldPathOther);
}

void CTI::privateCompileCTIMachineTrampolines()
{
    // (1) The first function provides fast property access for array length
    
    // Check eax is an array
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    X86Assembler::JmpSrc array_failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.cmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsArrayVptr), X86::eax);
    X86Assembler::JmpSrc array_failureCases2 = m_jit.emitUnlinkedJne();

    // Checks out okay! - get the length from the storage
    m_jit.movl_mr(OBJECT_OFFSET(JSArray, m_storage), X86::eax, X86::eax);
    m_jit.movl_mr(OBJECT_OFFSET(ArrayStorage, m_length), X86::eax, X86::eax);

    m_jit.addl_rr(X86::eax, X86::eax);
    X86Assembler::JmpSrc array_failureCases3 = m_jit.emitUnlinkedJo();
    m_jit.addl_i8r(1, X86::eax);
    
    m_jit.ret();

    // (2) The second function provides fast property access for string length
    
    X86Assembler::JmpDst stringLengthBegin = m_jit.align(16);

    // Check eax is a string
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    X86Assembler::JmpSrc string_failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.cmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsStringVptr), X86::eax);
    X86Assembler::JmpSrc string_failureCases2 = m_jit.emitUnlinkedJne();

    // Checks out okay! - get the length from the Ustring.
    m_jit.movl_mr(OBJECT_OFFSET(JSString, m_value) + OBJECT_OFFSET(UString, m_rep), X86::eax, X86::eax);
    m_jit.movl_mr(OBJECT_OFFSET(UString::Rep, len), X86::eax, X86::eax);

    m_jit.addl_rr(X86::eax, X86::eax);
    X86Assembler::JmpSrc string_failureCases3 = m_jit.emitUnlinkedJo();
    m_jit.addl_i8r(1, X86::eax);
    
    m_jit.ret();

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.

    void* code = m_jit.copy();
    ASSERT(code);

    X86Assembler::link(code, array_failureCases1, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    X86Assembler::link(code, array_failureCases2, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    X86Assembler::link(code, array_failureCases3, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    X86Assembler::link(code, string_failureCases1, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    X86Assembler::link(code, string_failureCases2, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    X86Assembler::link(code, string_failureCases3, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));

    m_machine->m_ctiArrayLengthTrampoline = code;
    m_machine->m_ctiStringLengthTrampoline = X86Assembler::getRelocatedAddress(code, stringLengthBegin);
}

void CTI::freeCTIMachineTrampolines(Machine* machine)
{
    WTF::fastFreeExecutable(machine->m_ctiArrayLengthTrampoline);
}

void CTI::patchGetByIdSelf(CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress)
{
    StructureStubInfo& info = codeBlock->getStubInfo(returnAddress);

    // We don't want to repatch more than once - in future go to cti_op_get_by_id_generic.
    // Should probably go to Machine::cti_op_get_by_id_fail, but that doesn't do anything interesting right now.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Machine::cti_op_get_by_id_generic));

    // Repatch the offset into the propoerty map to load from, then repatch the StructureID to look for.
    X86Assembler::repatchDisplacement(reinterpret_cast<intptr_t>(info.hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset, cachedOffset * sizeof(JSValue*));
    X86Assembler::repatchImmediate(reinterpret_cast<intptr_t>(info.hotPathBegin) + repatchOffsetGetByIdStructureID, reinterpret_cast<uint32_t>(structureID));
}

void CTI::patchPutByIdReplace(CodeBlock* codeBlock, StructureID* structureID, size_t cachedOffset, void* returnAddress)
{
    StructureStubInfo& info = codeBlock->getStubInfo(returnAddress);
    
    // We don't want to repatch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to Machine::cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Machine::cti_op_put_by_id_generic));

    // Repatch the offset into the propoerty map to load from, then repatch the StructureID to look for.
    X86Assembler::repatchDisplacement(reinterpret_cast<intptr_t>(info.hotPathBegin) + repatchOffsetPutByIdPropertyMapOffset, cachedOffset * sizeof(JSValue*));
    X86Assembler::repatchImmediate(reinterpret_cast<intptr_t>(info.hotPathBegin) + repatchOffsetPutByIdStructureID, reinterpret_cast<uint32_t>(structureID));
}

void CTI::privateCompilePatchGetArrayLength(void* returnAddress)
{
    StructureStubInfo& info = m_codeBlock->getStubInfo(returnAddress);

    // We don't want to repatch more than once - in future go to cti_op_put_by_id_generic.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));

    // Check eax is an array
    m_jit.testl_i32r(JSImmediate::TagMask, X86::eax);
    X86Assembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.cmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsArrayVptr), X86::eax);
    X86Assembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // Checks out okay! - get the length from the storage
    m_jit.movl_mr(OBJECT_OFFSET(JSArray, m_storage), X86::eax, X86::ecx);
    m_jit.movl_mr(OBJECT_OFFSET(ArrayStorage, m_length), X86::ecx, X86::ecx);

    m_jit.cmpl_i32r(JSImmediate::maxImmediateInt, X86::ecx);
    X86Assembler::JmpSrc failureCases3 = m_jit.emitUnlinkedJa();

    m_jit.addl_rr(X86::ecx, X86::ecx);
    m_jit.addl_i8r(1, X86::ecx);
    X86Assembler::JmpSrc success = m_jit.emitUnlinkedJmp();

    void* code = m_jit.copy();
    ASSERT(code);

    // Use the repatch information to link the failure cases back to the original slow case routine.
    void* slowCaseBegin = reinterpret_cast<char*>(info.callReturnLocation) - repatchOffsetGetByIdSlowCaseCall;
    X86Assembler::link(code, failureCases1, slowCaseBegin);
    X86Assembler::link(code, failureCases2, slowCaseBegin);
    X86Assembler::link(code, failureCases3, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    intptr_t successDest = (intptr_t)(info.hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset;
    X86Assembler::link(code, success, reinterpret_cast<void*>(successDest));

    // Track the stub we have created so that it will be deleted later.
    m_codeBlock->getStubInfo(returnAddress).stubRoutine = code;

    // Finally repatch the jump to sow case back in the hot path to jump here instead.
    // FIXME: should revert this repatching, on failure.
    intptr_t jmpLocation = reinterpret_cast<intptr_t>(info.hotPathBegin) + repatchOffsetGetByIdBranchToSlowCase;
    X86Assembler::repatchBranchOffset(jmpLocation, code);
}

void CTI::emitGetVariableObjectRegister(X86Assembler::RegisterID variableObject, int index, X86Assembler::RegisterID dst)
{
    m_jit.movl_mr(JSVariableObject::offsetOf_d(), variableObject, dst);
    m_jit.movl_mr(JSVariableObject::offsetOf_Data_registers(), dst, dst);
    m_jit.movl_mr(index * sizeof(Register), dst, dst);
}

void CTI::emitPutVariableObjectRegister(X86Assembler::RegisterID src, X86Assembler::RegisterID variableObject, int index)
{
    m_jit.movl_mr(JSVariableObject::offsetOf_d(), variableObject, variableObject);
    m_jit.movl_mr(JSVariableObject::offsetOf_Data_registers(), variableObject, variableObject);
    m_jit.movl_rm(src, index * sizeof(Register), variableObject);
}

#if ENABLE(WREC)

void* CTI::compileRegExp(Machine* machine, const UString& pattern, unsigned* numSubpatterns_ptr, const char** error_ptr, bool ignoreCase, bool multiline)
{
    // TODO: better error messages
    if (pattern.size() > MaxPatternSize) {
        *error_ptr = "regular expression too large";
        return 0;
    }

    X86Assembler jit(machine->jitCodeBuffer());
    WRECParser parser(pattern, ignoreCase, multiline, jit);
    
    jit.emitConvertToFastCall();
    // (0) Setup:
    //     Preserve regs & initialize outputRegister.
    jit.pushl_r(WRECGenerator::outputRegister);
    jit.pushl_r(WRECGenerator::currentValueRegister);
    // push pos onto the stack, both to preserve and as a parameter available to parseDisjunction
    jit.pushl_r(WRECGenerator::currentPositionRegister);
    // load output pointer
    jit.movl_mr(16
#if COMPILER(MSVC)
                    + 3 * sizeof(void*)
#endif
                    , X86::esp, WRECGenerator::outputRegister);
    
    // restart point on match fail.
    WRECGenerator::JmpDst nextLabel = jit.label();

    // (1) Parse Disjunction:
    
    //     Parsing the disjunction should fully consume the pattern.
    JmpSrcVector failures;
    parser.parseDisjunction(failures);
    if (parser.isEndOfPattern()) {
        parser.m_err = WRECParser::Error_malformedPattern;
    }
    if (parser.m_err) {
        // TODO: better error messages
        *error_ptr = "TODO: better error messages";
        return 0;
    }

    // (2) Success:
    //     Set return value & pop registers from the stack.

    jit.testl_rr(WRECGenerator::outputRegister, WRECGenerator::outputRegister);
    WRECGenerator::JmpSrc noOutput = jit.emitUnlinkedJe();

    jit.movl_rm(WRECGenerator::currentPositionRegister, 4, WRECGenerator::outputRegister);
    jit.popl_r(X86::eax);
    jit.movl_rm(X86::eax, WRECGenerator::outputRegister);
    jit.popl_r(WRECGenerator::currentValueRegister);
    jit.popl_r(WRECGenerator::outputRegister);
    jit.ret();
    
    jit.link(noOutput, jit.label());
    
    jit.popl_r(X86::eax);
    jit.movl_rm(X86::eax, WRECGenerator::outputRegister);
    jit.popl_r(WRECGenerator::currentValueRegister);
    jit.popl_r(WRECGenerator::outputRegister);
    jit.ret();

    // (3) Failure:
    //     All fails link to here.  Progress the start point & if it is within scope, loop.
    //     Otherwise, return fail value.
    WRECGenerator::JmpDst here = jit.label();
    for (unsigned i = 0; i < failures.size(); ++i)
        jit.link(failures[i], here);
    failures.clear();

    jit.movl_mr(X86::esp, WRECGenerator::currentPositionRegister);
    jit.addl_i8r(1, WRECGenerator::currentPositionRegister);
    jit.movl_rm(WRECGenerator::currentPositionRegister, X86::esp);
    jit.cmpl_rr(WRECGenerator::lengthRegister, WRECGenerator::currentPositionRegister);
    jit.link(jit.emitUnlinkedJle(), nextLabel);

    jit.addl_i8r(4, X86::esp);

    jit.movl_i32r(-1, X86::eax);
    jit.popl_r(WRECGenerator::currentValueRegister);
    jit.popl_r(WRECGenerator::outputRegister);
    jit.ret();

    *numSubpatterns_ptr = parser.m_numSubpatterns;

    void* code = jit.copy();
    ASSERT(code);
    return code;
}

#endif // ENABLE(WREC)

} // namespace JSC

#endif // ENABLE(CTI)
