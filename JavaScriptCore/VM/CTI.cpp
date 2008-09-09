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
#include "Machine.h"
#include "wrec/WREC.h"

using namespace std;

namespace JSC {

#if COMPILER(GCC) && PLATFORM(X86)
asm(
".globl _ctiTrampoline" "\n"
"_ctiTrampoline:" "\n"
    "pushl %esi" "\n"
    "pushl %edi" "\n"
    "subl $0x24, %esp" "\n"
    "movl $512, %esi" "\n"
    "call *0x30(%esp)" "\n" //Ox30 = 0x0C * 4, 0x0C = CTI_ARGS_code
    "addl $0x24, %esp" "\n"
    "popl %edi" "\n"
    "popl %esi" "\n"
    "ret" "\n"
);

asm(
".globl _ctiVMThrowTrampoline" "\n"
"_ctiVMThrowTrampoline:" "\n"
#ifndef NDEBUG
    "movl 0x34(%esp), %ecx" "\n" //Ox34 = 0x0D * 4, 0x0D = CTI_ARGS_exec
    "cmpl $0, 8(%ecx)" "\n"
    "jne 1f" "\n"
    "int3" "\n"
    "1:" "\n"
#endif
    "call __ZN3JSC7Machine12cti_vm_throwEPv" "\n"
    "addl $0x24, %esp" "\n"
    "popl %edi" "\n"
    "popl %esi" "\n"
    "ret" "\n"
);
    
#elif COMPILER(MSVC)
extern "C"
{
    
    __declspec(naked) JSValue* ctiTrampoline(void* code, ExecState* exec, RegisterFile* registerFile, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue** exception, Profiler**)
    {
        __asm {
            push esi;
            push edi;
            sub esp, 0x24;
            mov esi, 512;
            mov [esp], esp;
            call [esp + 0x30];
            add esp, 0x24;
            pop edi;
            pop esi;
            ret;
        }
    }
    
    __declspec(naked) void ctiVMThrowTrampoline()
    {
        __asm {
           mov [esp], esp;
            call JSC::Machine::cti_vm_throw;
            add esp, 0x24;
            pop edi;
            pop esi;
            ret;
        }
    }
    
}

#endif


// get arg puts an arg from the SF register array into a h/w register
ALWAYS_INLINE void CTI::emitGetArg(unsigned src, MacroAssembler::RegisterID dst)
{
    // TODO: we want to reuse values that are already in registers if we can - add a register allocator!
    if (src < m_codeBlock->constantRegisters.size()) {
        JSValue* js = m_codeBlock->constantRegisters[src].jsValue(m_exec);
        m_jit.emitMovl_i32r(reinterpret_cast<unsigned>(js), dst);
    } else
        m_jit.emitMovl_mr(src * sizeof(Register), MacroAssembler::edi, dst);
}

// get arg puts an arg from the SF register array onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void CTI::emitGetPutArg(unsigned src, unsigned offset, MacroAssembler::RegisterID scratch)
{
    if (src < m_codeBlock->constantRegisters.size()) {
        JSValue* js = m_codeBlock->constantRegisters[src].jsValue(m_exec);
        m_jit.emitMovl_i32m(reinterpret_cast<unsigned>(js), offset + sizeof(void*), MacroAssembler::esp);
    } else {
        m_jit.emitMovl_mr(src * sizeof(Register), MacroAssembler::edi, scratch);
        m_jit.emitMovl_rm(scratch, offset + sizeof(void*), MacroAssembler::esp);
    }
}

// puts an arg onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void CTI::emitPutArg(MacroAssembler::RegisterID src, unsigned offset)
{
    m_jit.emitMovl_rm(src, offset + sizeof(void*), MacroAssembler::esp);
}

ALWAYS_INLINE void CTI::emitPutArgConstant(unsigned value, unsigned offset)
{
    m_jit.emitMovl_i32m(value, offset + sizeof(void*), MacroAssembler::esp);
}

ALWAYS_INLINE JSValue* CTI::getConstantImmediateNumericArg(unsigned src)
{
    if (src < m_codeBlock->constantRegisters.size()) {
        JSValue* js = m_codeBlock->constantRegisters[src].jsValue(m_exec);
        return JSImmediate::isNumber(js) ? js : 0;
    }
    return 0;
}

ALWAYS_INLINE void CTI::emitPutCTIParam(MacroAssembler::RegisterID from, unsigned name)
{
    m_jit.emitMovl_rm(from, name * sizeof(void*), MacroAssembler::esp);
}

ALWAYS_INLINE void CTI::emitGetCTIParam(unsigned name, MacroAssembler::RegisterID to)
{
    m_jit.emitMovl_mr(name * sizeof(void*), MacroAssembler::esp, to);
}

ALWAYS_INLINE void CTI::emitPutToCallFrameHeader(MacroAssembler::RegisterID from, RegisterFile::CallFrameHeaderEntry entry)
{
    m_jit.emitMovl_rm(from, -((m_codeBlock->numLocals + RegisterFile::CallFrameHeaderSize) - entry) * sizeof(Register), MacroAssembler::edi);
}

ALWAYS_INLINE void CTI::emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, MacroAssembler::RegisterID to)
{
    m_jit.emitMovl_mr(-((m_codeBlock->numLocals + RegisterFile::CallFrameHeaderSize) - entry) * sizeof(Register), MacroAssembler::edi, to);
}

ALWAYS_INLINE void CTI::emitPutResult(unsigned dst, MacroAssembler::RegisterID from)
{
    m_jit.emitMovl_rm(from, dst * sizeof(Register), MacroAssembler::edi);
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

#if ENABLE(SAMPLING_TOOL)
unsigned incall = 0;
#endif

void ctiSetReturnAddress(void** where, void* what)
{
    *where = what;
}

void ctiRepatchCallByReturnAddress(void* where, void* what)
{
    (static_cast<void**>(where))[-1] = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(what) - reinterpret_cast<uintptr_t>(where));
}

#ifdef NDEBUG

ALWAYS_INLINE void CTI::emitDebugExceptionCheck()
{
}

#else

ALWAYS_INLINE void CTI::emitDebugExceptionCheck()
{
    emitGetCTIParam(CTI_ARGS_exec, MacroAssembler::ecx);
    m_jit.emitCmpl_i32m(0, OBJECT_OFFSET(ExecState, m_exception), MacroAssembler::ecx);
    MacroAssembler::JmpSrc noException = m_jit.emitUnlinkedJe();
    m_jit.emitInt3();
    m_jit.link(noException, m_jit.label());
}

void CTI::printOpcodeOperandTypes(unsigned src1, unsigned src2)
{
    char which1 = '*';
    if (src1 < m_codeBlock->constantRegisters.size()) {
        JSValue* js = m_codeBlock->constantRegisters[src1].jsValue(m_exec);
        which1 = 
            JSImmediate::isImmediate(js) ?
                (JSImmediate::isNumber(js) ? 'i' :
                JSImmediate::isBoolean(js) ? 'b' :
                js->isUndefined() ? 'u' :
                js->isNull() ? 'n' : '?')
                :
            (js->isString() ? 's' :
            js->isObject() ? 'o' :
            'k');
    }
    char which2 = '*';
    if (src2 < m_codeBlock->constantRegisters.size()) {
        JSValue* js = m_codeBlock->constantRegisters[src2].jsValue(m_exec);
        which2 = 
            JSImmediate::isImmediate(js) ?
                (JSImmediate::isNumber(js) ? 'i' :
                JSImmediate::isBoolean(js) ? 'b' :
                js->isUndefined() ? 'u' :
                js->isNull() ? 'n' : '?')
                :
            (js->isString() ? 's' :
            js->isObject() ? 'o' :
            'k');
    }
    if ((which1 != '*') | (which2 != '*'))
        fprintf(stderr, "Types %c %c\n", which1, which2);
}

#endif

ALWAYS_INLINE void CTI::emitCall(unsigned opcodeIndex, CTIHelper_j helper)
{
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(1, &incall);
#endif
    m_calls.append(CallRecord(m_jit.emitCall(), helper, opcodeIndex));
    emitDebugExceptionCheck();
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(0, &incall);
#endif
}

ALWAYS_INLINE void CTI::emitCall(unsigned opcodeIndex, CTIHelper_p helper)
{
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(1, &incall);
#endif
    m_calls.append(CallRecord(m_jit.emitCall(), helper, opcodeIndex));
    emitDebugExceptionCheck();
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(0, &incall);
#endif
}

ALWAYS_INLINE void CTI::emitCall(unsigned opcodeIndex, CTIHelper_b helper)
{
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(1, &incall);
#endif
    m_calls.append(CallRecord(m_jit.emitCall(), helper, opcodeIndex));
    emitDebugExceptionCheck();
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(0, &incall);
#endif
}

ALWAYS_INLINE void CTI::emitCall(unsigned opcodeIndex, CTIHelper_v helper)
{
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(1, &incall);
#endif
    m_calls.append(CallRecord(m_jit.emitCall(), helper, opcodeIndex));
    emitDebugExceptionCheck();
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(0, &incall);
#endif
}

ALWAYS_INLINE void CTI::emitCall(unsigned opcodeIndex, CTIHelper_s helper)
{
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(1, &incall);
#endif
    m_calls.append(CallRecord(m_jit.emitCall(), helper, opcodeIndex));
    emitDebugExceptionCheck();
#if ENABLE(SAMPLING_TOOL)
    m_jit.emitMovl_i32m(0, &incall);
#endif
}

ALWAYS_INLINE void CTI::emitJumpSlowCaseIfNotImm(MacroAssembler::RegisterID reg, unsigned opcodeIndex)
{
    m_jit.emitTestl_i32r(JSImmediate::TagBitTypeInteger, reg);
    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJe(), opcodeIndex));
}

ALWAYS_INLINE void CTI::emitJumpSlowCaseIfNotImms(MacroAssembler::RegisterID reg1, MacroAssembler::RegisterID reg2, unsigned opcodeIndex)
{
    m_jit.emitMovl_rr(reg1, MacroAssembler::ecx);
    m_jit.emitAndl_rr(reg2, MacroAssembler::ecx);
    emitJumpSlowCaseIfNotImm(MacroAssembler::ecx, opcodeIndex);
}

ALWAYS_INLINE unsigned CTI::getDeTaggedConstantImmediate(JSValue* imm)
{
    ASSERT(JSImmediate::isNumber(imm));
    return reinterpret_cast<unsigned>(imm) & ~JSImmediate::TagBitTypeInteger;
}

ALWAYS_INLINE void CTI::emitFastArithDeTagImmediate(MacroAssembler::RegisterID reg)
{
    // op_mod relies on this being a sub - setting zf if result is 0.
    m_jit.emitSubl_i8r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE void CTI::emitFastArithReTagImmediate(MacroAssembler::RegisterID reg)
{
    m_jit.emitAddl_i8r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE void CTI::emitFastArithPotentiallyReTagImmediate(MacroAssembler::RegisterID reg)
{
    m_jit.emitOrl_i8r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE void CTI::emitFastArithImmToInt(MacroAssembler::RegisterID reg)
{
    m_jit.emitSarl_i8r(1, reg);
}

ALWAYS_INLINE void CTI::emitFastArithIntToImmOrSlowCase(MacroAssembler::RegisterID reg, unsigned opcodeIndex)
{
    m_jit.emitAddl_rr(reg, reg);
    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), opcodeIndex));
    emitFastArithReTagImmediate(reg);
}

ALWAYS_INLINE void CTI::emitFastArithIntToImmNoCheck(MacroAssembler::RegisterID reg)
{
    m_jit.emitAddl_rr(reg, reg);
    emitFastArithReTagImmediate(reg);
}

CTI::CTI(Machine* machine, ExecState* exec, CodeBlock* codeBlock)
    : m_jit(machine->jitCodeBuffer())
    , m_machine(machine)
    , m_exec(exec)
    , m_codeBlock(codeBlock)
    , m_labels(codeBlock ? codeBlock->instructions.size() : 0)
{
}

#define CTI_COMPILE_BINARY_OP(name) \
    case name: { \
        emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx); \
        emitGetPutArg(instruction[i + 3].u.operand, 4, MacroAssembler::ecx); \
        emitCall(i, Machine::cti_##name); \
        emitPutResult(instruction[i + 1].u.operand); \
        i += 4; \
        break; \
    }

#if ENABLE(SAMPLING_TOOL)
OpcodeID what = (OpcodeID)-1;
#endif

void CTI::compileOpCall(Instruction* instruction, unsigned i, CompileOpCallType type)
{
    if (type == OpConstruct) {
        emitPutArgConstant(reinterpret_cast<unsigned>(instruction + i), 12);
        emitPutArgConstant(instruction[i + 4].u.operand, 8);
        emitPutArgConstant(instruction[i + 3].u.operand, 4);
    } else {
        emitPutArgConstant(reinterpret_cast<unsigned>(instruction + i), 16);
        emitPutArgConstant(instruction[i + 5].u.operand, 12);
        emitPutArgConstant(instruction[i + 4].u.operand, 8);
        // FIXME: should this be loaded dynamically off m_exec?
        int thisVal = instruction[i + 3].u.operand;
        if (thisVal == missingThisObjectMarker()) {
            emitPutArgConstant(reinterpret_cast<unsigned>(m_exec->globalThisValue()), 4);
        } else
            emitGetPutArg(thisVal, 4, MacroAssembler::ecx);
    }

    MacroAssembler::JmpSrc wasEval;
    if (type == OpCallEval) {
        emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
        emitCall(i, Machine::cti_op_call_eval);
        m_jit.emitRestoreArgumentReference();

       emitGetCTIParam(CTI_ARGS_r, MacroAssembler::edi); // edi := r

        m_jit.emitCmpl_i32r(reinterpret_cast<unsigned>(JSImmediate::impossibleValue()), MacroAssembler::eax);
        wasEval = m_jit.emitUnlinkedJne();
    
        // this reloads the first arg into ecx (checked just below).
        emitGetArg(instruction[i + 2].u.operand, MacroAssembler::ecx);
    } else {
        // this sets up the first arg, and explicitly leaves the value in ecx (checked just below).
        emitGetArg(instruction[i + 2].u.operand, MacroAssembler::ecx);
        emitPutArg(MacroAssembler::ecx, 0);
    }

    // Fast check for JS function.
    m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::ecx);
    MacroAssembler::JmpSrc isNotObject = m_jit.emitUnlinkedJne();
    m_jit.emitCmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsFunctionVptr), MacroAssembler::ecx);
    MacroAssembler::JmpSrc isJSFunction = m_jit.emitUnlinkedJe();
    m_jit.link(isNotObject, m_jit.label());

    // This handles host functions
    emitCall(i, ((type == OpConstruct) ? Machine::cti_op_construct_NotJSConstruct : Machine::cti_op_call_NotJSFunction));
    emitGetCTIParam(CTI_ARGS_r, MacroAssembler::edi); // edi := r
    
    MacroAssembler::JmpSrc wasNotJSFunction = m_jit.emitUnlinkedJmp();
    m_jit.link(isJSFunction, m_jit.label());

    // This handles JSFunctions
    emitCall(i, ((type == OpConstruct) ? Machine::cti_op_construct_JSConstruct : Machine::cti_op_call_JSFunction));
    m_jit.emitCallN_r(MacroAssembler::eax);
    emitGetCTIParam(CTI_ARGS_r, MacroAssembler::edi); // edi := r

    MacroAssembler::JmpDst end = m_jit.label();
    m_jit.link(wasNotJSFunction, end);
    if (type == OpCallEval)
        m_jit.link(wasEval, end);

    emitPutResult(instruction[i + 1].u.operand);
}

void CTI::privateCompileMainPass()
{
    Instruction* instruction = m_codeBlock->instructions.begin();
    unsigned instructionCount = m_codeBlock->instructions.size();

    for (unsigned i = 0; i < instructionCount; ) {
        m_labels[i] = m_jit.label();

#if ENABLE(SAMPLING_TOOL)
        m_jit.emitMovl_i32m(m_machine->getOpcodeID(instruction[i].u.opcode), &what);
#endif

        ASSERT_WITH_MESSAGE(m_machine->isOpcode(instruction[i].u.opcode), "privateCompileMainPass gone bad @ %d", i);
        m_jit.emitRestoreArgumentReference();
        switch (m_machine->getOpcodeID(instruction[i].u.opcode)) {
        case op_mov: {
            unsigned src = instruction[i + 2].u.operand;
            if (src < m_codeBlock->constantRegisters.size())
                m_jit.emitMovl_i32r(reinterpret_cast<unsigned>(m_codeBlock->constantRegisters[src].jsValue(m_exec)), MacroAssembler::edx);
            else
                emitGetArg(src, MacroAssembler::edx);
            emitPutResult(instruction[i + 1].u.operand, MacroAssembler::edx);
            i += 3;
            break;
        }
        case op_add: {
            unsigned dst = instruction[i + 1].u.operand;
            unsigned src1 = instruction[i + 2].u.operand;
            unsigned src2 = instruction[i + 3].u.operand;
            if (src2 < m_codeBlock->constantRegisters.size()) {
                JSValue* value = m_codeBlock->constantRegisters[src2].jsValue(m_exec);
                if (JSImmediate::isNumber(value)) {
                    emitGetArg(src1, MacroAssembler::eax);
                    emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
                    m_jit.emitAddl_i32r(getDeTaggedConstantImmediate(value), MacroAssembler::eax);
                    m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
                    emitPutResult(dst);
                    i += 4;
                    break;
                }
            } else if (!(src1 < m_codeBlock->constantRegisters.size())) {
                emitGetArg(src1, MacroAssembler::eax);
                emitGetArg(src2, MacroAssembler::edx);
                emitJumpSlowCaseIfNotImms(MacroAssembler::eax, MacroAssembler::edx, i);
                emitFastArithDeTagImmediate(MacroAssembler::eax);
                m_jit.emitAddl_rr(MacroAssembler::edx, MacroAssembler::eax);
                m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
                emitPutResult(dst);
                i += 4;
                break;
            }
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitGetPutArg(instruction[i + 3].u.operand, 4, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_add);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_end: {
            if (m_codeBlock->needsFullScopeChain)
                emitCall(i, Machine::cti_op_end);
            emitGetArg(instruction[i + 1].u.operand, MacroAssembler::eax);
#if ENABLE(SAMPLING_TOOL)
            m_jit.emitMovl_i32m(-1, &what);
#endif
            m_jit.emitPushl_m(-((m_codeBlock->numLocals + RegisterFile::CallFrameHeaderSize) - RegisterFile::CTIReturnEIP) * sizeof(Register), MacroAssembler::edi);
            m_jit.emitRet();
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
            emitGetArg(srcDst, MacroAssembler::eax);
            emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
            m_jit.emitAddl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitPutResult(srcDst, MacroAssembler::eax);
            i += 2;
            break;
        }
        case op_loop: {
            m_jit.emitSubl_i8r(1, MacroAssembler::esi);
            MacroAssembler::JmpSrc skipTimeout = m_jit.emitUnlinkedJne();
            emitCall(i, Machine::cti_timeout_check);

            emitGetCTIParam(CTI_ARGS_exec, MacroAssembler::ecx);
            m_jit.emitMovl_mr(OBJECT_OFFSET(ExecState, m_globalData) + OBJECT_OFFSET(JSGlobalData, machine), MacroAssembler::ecx, MacroAssembler::ecx);
            m_jit.emitMovl_mr(OBJECT_OFFSET(Machine, m_ticksUntilNextTimeoutCheck), MacroAssembler::ecx, MacroAssembler::esi);
            m_jit.link(skipTimeout, m_jit.label());

            unsigned target = instruction[i + 1].u.operand;
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 1 + target));
            i += 2;
            break;
        }
        case op_loop_if_less: {
            m_jit.emitSubl_i8r(1, MacroAssembler::esi);
            MacroAssembler::JmpSrc skipTimeout = m_jit.emitUnlinkedJne();
            emitCall(i, Machine::cti_timeout_check);

            emitGetCTIParam(CTI_ARGS_exec, MacroAssembler::ecx);
            m_jit.emitMovl_mr(OBJECT_OFFSET(ExecState, m_globalData) + OBJECT_OFFSET(JSGlobalData, machine), MacroAssembler::ecx, MacroAssembler::ecx);
            m_jit.emitMovl_mr(OBJECT_OFFSET(Machine, m_ticksUntilNextTimeoutCheck), MacroAssembler::ecx, MacroAssembler::esi);
            m_jit.link(skipTimeout, m_jit.label());

            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                emitGetArg(instruction[i + 1].u.operand, MacroAssembler::edx);
                emitJumpSlowCaseIfNotImm(MacroAssembler::edx, i);
                m_jit.emitCmpl_i32r(reinterpret_cast<unsigned>(src2imm), MacroAssembler::edx);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJl(), i + 3 + target));
            } else {
                emitGetArg(instruction[i + 1].u.operand, MacroAssembler::eax);
                emitGetArg(instruction[i + 2].u.operand, MacroAssembler::edx);
                emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
                emitJumpSlowCaseIfNotImm(MacroAssembler::edx, i);
                m_jit.emitCmpl_rr(MacroAssembler::edx, MacroAssembler::eax);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJl(), i + 3 + target));
            }
            i += 4;
            break;
        }
        case op_new_object: {
            emitCall(i, Machine::cti_op_new_object);
            emitPutResult(instruction[i + 1].u.operand);
            i += 2;
            break;
        }
        case op_put_by_id: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitGetArg(instruction[i + 1].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::edx);
            emitPutArg(MacroAssembler::eax, 0); // leave the base in eax
            emitPutArg(MacroAssembler::edx, 8); // leave the base in edx
            emitCall(i, Machine::cti_op_put_by_id);
            i += 6;
            break;
        }
        case op_get_by_id: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitPutArg(MacroAssembler::eax, 0); // leave the base in eax
            emitCall(i, Machine::cti_op_get_by_id);
            emitPutResult(instruction[i + 1].u.operand);
            i += 8;
            break;
        }
        case op_instanceof: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitGetPutArg(instruction[i + 3].u.operand, 4, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_instanceof);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_del_by_id: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitCall(i, Machine::cti_op_del_by_id);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_mul);
        case op_new_func: {
            FuncDeclNode* func = (m_codeBlock->functions[instruction[i + 2].u.operand]).get();
            emitPutArgConstant(reinterpret_cast<unsigned>(func), 0);
            emitCall(i, Machine::cti_op_new_func);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_call: {
            compileOpCall(instruction, i);
            i += 6;
            break;
        }
        case op_get_scoped_var: {
            int skip = instruction[i + 3].u.operand + m_codeBlock->needsFullScopeChain;

            emitGetCTIParam(CTI_ARGS_scopeChain, MacroAssembler::eax);
            while (skip--)
                m_jit.emitMovl_mr(OBJECT_OFFSET(ScopeChainNode, next), MacroAssembler::eax, MacroAssembler::eax);

            m_jit.emitMovl_mr(OBJECT_OFFSET(ScopeChainNode, object), MacroAssembler::eax, MacroAssembler::eax);
            m_jit.emitMovl_mr(JSVariableObject::offsetOf_d(), MacroAssembler::eax, MacroAssembler::eax);
            m_jit.emitMovl_mr(JSVariableObject::offsetOf_Data_registers(), MacroAssembler::eax, MacroAssembler::eax);
            m_jit.emitMovl_mr((instruction[i + 2].u.operand) * sizeof(Register), MacroAssembler::eax, MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_put_scoped_var: {
            int skip = instruction[i + 2].u.operand + m_codeBlock->needsFullScopeChain;

            emitGetCTIParam(CTI_ARGS_scopeChain, MacroAssembler::edx);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::eax);
            while (skip--)
                m_jit.emitMovl_mr(OBJECT_OFFSET(ScopeChainNode, next), MacroAssembler::edx, MacroAssembler::edx);

            m_jit.emitMovl_mr(OBJECT_OFFSET(ScopeChainNode, object), MacroAssembler::edx, MacroAssembler::edx);
            m_jit.emitMovl_mr(JSVariableObject::offsetOf_d(), MacroAssembler::edx, MacroAssembler::edx);
            m_jit.emitMovl_mr(JSVariableObject::offsetOf_Data_registers(), MacroAssembler::edx, MacroAssembler::edx);
            m_jit.emitMovl_rm(MacroAssembler::eax, (instruction[i + 1].u.operand) * sizeof(Register), MacroAssembler::edx);
            i += 4;
            break;
        }
        case op_ret: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_ret);

            m_jit.emitPushl_m(-((m_codeBlock->numLocals + RegisterFile::CallFrameHeaderSize) - RegisterFile::CTIReturnEIP) * sizeof(Register), MacroAssembler::edi);
            m_jit.emitRet();
            i += 2;
            break;
        }
        case op_new_array: {
            m_jit.emitLeal_mr(sizeof(Register) * instruction[i + 2].u.operand, MacroAssembler::edi, MacroAssembler::edx);
            emitPutArg(MacroAssembler::edx, 0);
            emitPutArgConstant(instruction[i + 3].u.operand, 4);
            emitCall(i, Machine::cti_op_new_array);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_resolve: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCall(i, Machine::cti_op_resolve);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_construct: {
            compileOpCall(instruction, i, OpConstruct);
            i += 5;
            break;
        }
        case op_get_by_val: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::edx);
            emitJumpSlowCaseIfNotImm(MacroAssembler::edx, i);
            emitFastArithImmToInt(MacroAssembler::edx);
            m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            m_jit.emitCmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsArrayVptr), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            m_jit.emitCmpl_rm(MacroAssembler::edx, OBJECT_OFFSET(JSArray, m_fastAccessCutoff), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJbe(), i));

            m_jit.emitMovl_mr(OBJECT_OFFSET(JSArray, m_storage), MacroAssembler::eax, MacroAssembler::eax);
            m_jit.emitMovl_mr(OBJECT_OFFSET(ArrayStorage, m_vector[0]), MacroAssembler::eax, MacroAssembler::edx, sizeof(JSValue*), MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_resolve_func: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCall(i, Machine::cti_op_resolve_func);
            emitPutResult(instruction[i + 1].u.operand);
            emitGetCTIParam(CTI_ARGS_2ndResult, MacroAssembler::eax);
            emitPutResult(instruction[i + 2].u.operand);
            i += 4;
            break;
        }
        case op_sub: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::edx);
            emitJumpSlowCaseIfNotImms(MacroAssembler::eax, MacroAssembler::edx, i);
            m_jit.emitSubl_rr(MacroAssembler::edx, MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitFastArithReTagImmediate(MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_put_by_val: {
            emitGetArg(instruction[i + 1].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::edx);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::ecx);
            emitJumpSlowCaseIfNotImm(MacroAssembler::edx, i);
            emitFastArithImmToInt(MacroAssembler::edx);
            m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            m_jit.emitCmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsArrayVptr), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            m_jit.emitCmpl_rm(MacroAssembler::edx, OBJECT_OFFSET(JSArray, m_fastAccessCutoff), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJbe(), i));

            m_jit.emitMovl_mr(OBJECT_OFFSET(JSArray, m_storage), MacroAssembler::eax, MacroAssembler::eax);
            m_jit.emitMovl_rm(MacroAssembler::ecx, OBJECT_OFFSET(ArrayStorage, m_vector[0]), MacroAssembler::eax, MacroAssembler::edx, sizeof(JSValue*));
            i += 4;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_lesseq)
        case op_loop_if_true: {
            m_jit.emitSubl_i8r(1, MacroAssembler::esi);
            MacroAssembler::JmpSrc skipTimeout = m_jit.emitUnlinkedJne();
            emitCall(i, Machine::cti_timeout_check);

            emitGetCTIParam(CTI_ARGS_exec, MacroAssembler::ecx);
            m_jit.emitMovl_mr(OBJECT_OFFSET(ExecState, m_globalData) + OBJECT_OFFSET(JSGlobalData, machine), MacroAssembler::ecx, MacroAssembler::ecx);
            m_jit.emitMovl_mr(OBJECT_OFFSET(Machine, m_ticksUntilNextTimeoutCheck), MacroAssembler::ecx, MacroAssembler::esi);
            m_jit.link(skipTimeout, m_jit.label());

            unsigned target = instruction[i + 2].u.operand;
            emitGetArg(instruction[i + 1].u.operand, MacroAssembler::eax);

            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::zeroImmediate()), MacroAssembler::eax);
            MacroAssembler::JmpSrc isZero = m_jit.emitUnlinkedJe();
            m_jit.emitTestl_i32r(JSImmediate::TagBitTypeInteger, MacroAssembler::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJne(), i + 2 + target));

            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::trueImmediate()), MacroAssembler::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJe(), i + 2 + target));
            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::falseImmediate()), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            m_jit.link(isZero, m_jit.label());
            i += 3;
            break;
        };
        case op_resolve_base: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCall(i, Machine::cti_op_resolve_base);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_negate: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_negate);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_resolve_skip: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitPutArgConstant(instruction[i + 3].u.operand + m_codeBlock->needsFullScopeChain, 4);
            emitCall(i, Machine::cti_op_resolve_skip);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_div)
        case op_pre_dec: {
            int srcDst = instruction[i + 1].u.operand;
            emitGetArg(srcDst, MacroAssembler::eax);
            emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
            m_jit.emitSubl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitPutResult(srcDst, MacroAssembler::eax);
            i += 2;
            break;
        }
        case op_jnless: {
            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                emitGetArg(instruction[i + 1].u.operand, MacroAssembler::edx);
                emitJumpSlowCaseIfNotImm(MacroAssembler::edx, i);
                m_jit.emitCmpl_i32r(reinterpret_cast<unsigned>(src2imm), MacroAssembler::edx);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJge(), i + 3 + target));
            } else {
                emitGetArg(instruction[i + 1].u.operand, MacroAssembler::eax);
                emitGetArg(instruction[i + 2].u.operand, MacroAssembler::edx);
                emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
                emitJumpSlowCaseIfNotImm(MacroAssembler::edx, i);
                m_jit.emitCmpl_rr(MacroAssembler::edx, MacroAssembler::eax);
                m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJge(), i + 3 + target));
            }
            i += 4;
            break;
        }
        case op_not: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            m_jit.emitXorl_i8r(JSImmediate::FullTagTypeBool, MacroAssembler::eax);
            m_jit.emitTestl_i32r(JSImmediate::FullTagTypeMask, MacroAssembler::eax); // i8?
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));
            m_jit.emitXorl_i8r((JSImmediate::FullTagTypeBool | JSImmediate::ExtendedPayloadBitBoolValue), MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_jfalse: {
            unsigned target = instruction[i + 2].u.operand;
            emitGetArg(instruction[i + 1].u.operand, MacroAssembler::eax);

            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::zeroImmediate()), MacroAssembler::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJe(), i + 2 + target));
            m_jit.emitTestl_i32r(JSImmediate::TagBitTypeInteger, MacroAssembler::eax);
            MacroAssembler::JmpSrc isNonZero = m_jit.emitUnlinkedJne();

            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::falseImmediate()), MacroAssembler::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJe(), i + 2 + target));
            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::trueImmediate()), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            m_jit.link(isNonZero, m_jit.label());
            i += 3;
            break;
        };
        case op_post_inc: {
            int srcDst = instruction[i + 2].u.operand;
            emitGetArg(srcDst, MacroAssembler::eax);
            m_jit.emitMovl_rr(MacroAssembler::eax, MacroAssembler::edx);
            emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
            m_jit.emitAddl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), MacroAssembler::edx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitPutResult(srcDst, MacroAssembler::edx);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_unexpected_load: {
            JSValue* v = m_codeBlock->unexpectedConstants[instruction[i + 2].u.operand];
            m_jit.emitMovl_i32r(reinterpret_cast<unsigned>(v), MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_jsr: {
            int retAddrDst = instruction[i + 1].u.operand;
            int target = instruction[i + 2].u.operand;
            m_jit.emitMovl_i32m(0, sizeof(Register) * retAddrDst, MacroAssembler::edi);
            MacroAssembler::JmpDst addrPosition = m_jit.label();
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 2 + target));
            MacroAssembler::JmpDst sretTarget = m_jit.label();
            m_jsrSites.append(JSRInfo(addrPosition, sretTarget));
            i += 3;
            break;
        }
        case op_sret: {
            m_jit.emitJmpN_m(sizeof(Register) * instruction[i + 1].u.operand, MacroAssembler::edi);
            i += 2;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_eq)
        case op_lshift: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::ecx);
            emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
            emitJumpSlowCaseIfNotImm(MacroAssembler::ecx, i);
            emitFastArithImmToInt(MacroAssembler::eax);
            emitFastArithImmToInt(MacroAssembler::ecx);
            m_jit.emitShll_CLr(MacroAssembler::eax);
            emitFastArithIntToImmOrSlowCase(MacroAssembler::eax, i);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_bitand: {
            unsigned src1 = instruction[i + 2].u.operand;
            unsigned src2 = instruction[i + 3].u.operand;
            unsigned dst = instruction[i + 1].u.operand;
            if (JSValue* value = getConstantImmediateNumericArg(src1)) {
                emitGetArg(src2, MacroAssembler::eax);
                emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
                m_jit.emitAndl_i32r(reinterpret_cast<unsigned>(value), MacroAssembler::eax); // FIXME: make it more obvious this is relying on the format of JSImmediate
                emitPutResult(dst);
            } else if (JSValue* value = getConstantImmediateNumericArg(src2)) {
                emitGetArg(src1, MacroAssembler::eax);
                emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
                m_jit.emitAndl_i32r(reinterpret_cast<unsigned>(value), MacroAssembler::eax);
                emitPutResult(dst);
            } else {
                emitGetArg(src1, MacroAssembler::eax);
                emitGetArg(src2, MacroAssembler::edx);
                m_jit.emitAndl_rr(MacroAssembler::edx, MacroAssembler::eax);
                emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
                emitPutResult(dst);
            }
            i += 4;
            break;
        }
        case op_rshift: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::ecx);
            emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
            emitJumpSlowCaseIfNotImm(MacroAssembler::ecx, i);
            emitFastArithImmToInt(MacroAssembler::ecx);
            m_jit.emitSarl_CLr(MacroAssembler::eax);
            emitFastArithPotentiallyReTagImmediate(MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_bitnot: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
            m_jit.emitXorl_i8r(~JSImmediate::TagBitTypeInteger, MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_resolve_with_base: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 3].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCall(i, Machine::cti_op_resolve_with_base);
            emitPutResult(instruction[i + 1].u.operand);
            emitGetCTIParam(CTI_ARGS_2ndResult, MacroAssembler::eax);
            emitPutResult(instruction[i + 2].u.operand);
            i += 4;
            break;
        }
        case op_new_func_exp: {
            FuncExprNode* func = (m_codeBlock->functionExpressions[instruction[i + 2].u.operand]).get();
            emitPutArgConstant(reinterpret_cast<unsigned>(func), 0);
            emitCall(i, Machine::cti_op_new_func_exp);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_mod: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::ecx);
            emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
            emitJumpSlowCaseIfNotImm(MacroAssembler::ecx, i);
            emitFastArithDeTagImmediate(MacroAssembler::eax);
            emitFastArithDeTagImmediate(MacroAssembler::ecx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJe(), i)); // This is checking if the last detag resulted in a value 0.
            m_jit.emitCdq();
            m_jit.emitIdivl_r(MacroAssembler::ecx);
            emitFastArithReTagImmediate(MacroAssembler::edx);
            m_jit.emitMovl_rr(MacroAssembler::edx, MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_jtrue: {
            unsigned target = instruction[i + 2].u.operand;
            emitGetArg(instruction[i + 1].u.operand, MacroAssembler::eax);

            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::zeroImmediate()), MacroAssembler::eax);
            MacroAssembler::JmpSrc isZero = m_jit.emitUnlinkedJe();
            m_jit.emitTestl_i32r(JSImmediate::TagBitTypeInteger, MacroAssembler::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJne(), i + 2 + target));

            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::trueImmediate()), MacroAssembler::eax);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJe(), i + 2 + target));
            m_jit.emitCmpl_i32r(reinterpret_cast<uint32_t>(JSImmediate::falseImmediate()), MacroAssembler::eax);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJne(), i));

            m_jit.link(isZero, m_jit.label());
            i += 3;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_less)
        CTI_COMPILE_BINARY_OP(op_neq)
        case op_post_dec: {
            int srcDst = instruction[i + 2].u.operand;
            emitGetArg(srcDst, MacroAssembler::eax);
            m_jit.emitMovl_rr(MacroAssembler::eax, MacroAssembler::edx);
            emitJumpSlowCaseIfNotImm(MacroAssembler::eax, i);
            m_jit.emitSubl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), MacroAssembler::edx);
            m_slowCases.append(SlowCaseEntry(m_jit.emitUnlinkedJo(), i));
            emitPutResult(srcDst, MacroAssembler::edx);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_urshift)
        case op_bitxor: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::edx);
            emitJumpSlowCaseIfNotImms(MacroAssembler::eax, MacroAssembler::edx, i);
            m_jit.emitXorl_rr(MacroAssembler::edx, MacroAssembler::eax);
            emitFastArithReTagImmediate(MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_new_regexp: {
            RegExp* regExp = m_codeBlock->regexps[instruction[i + 2].u.operand].get();
            emitPutArgConstant(reinterpret_cast<unsigned>(regExp), 0);
            emitCall(i, Machine::cti_op_new_regexp);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_bitor: {
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::edx);
            emitJumpSlowCaseIfNotImms(MacroAssembler::eax, MacroAssembler::edx, i);
            m_jit.emitOrl_rr(MacroAssembler::edx, MacroAssembler::eax);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_call_eval: {
            compileOpCall(instruction, i, OpCallEval);
            i += 6;
            break;
        }
        case op_throw: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_throw);
            m_jit.emitAddl_i8r(0x24, MacroAssembler::esp);
            m_jit.emitPopl_r(MacroAssembler::edi);
            m_jit.emitPopl_r(MacroAssembler::esi);
            m_jit.emitRet();
            i += 2;
            break;
        }
        case op_get_pnames: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_get_pnames);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_next_pname: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            unsigned target = instruction[i + 3].u.operand;
            emitCall(i, Machine::cti_op_next_pname);
            m_jit.emitTestl_rr(MacroAssembler::eax, MacroAssembler::eax);
            MacroAssembler::JmpSrc endOfIter = m_jit.emitUnlinkedJe();
            emitPutResult(instruction[i + 1].u.operand);
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 3 + target));
            m_jit.link(endOfIter, m_jit.label());
            i += 4;
            break;
        }
        case op_push_scope: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_push_scope);
            i += 2;
            break;
        }
        case op_pop_scope: {
            emitCall(i, Machine::cti_op_pop_scope);
            i += 1;
            break;
        }
        case op_typeof: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_typeof);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        CTI_COMPILE_BINARY_OP(op_stricteq)
        CTI_COMPILE_BINARY_OP(op_nstricteq)
        case op_to_jsnumber: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_to_jsnumber);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_in: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitGetPutArg(instruction[i + 3].u.operand, 4, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_in);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_push_new_scope: {
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitGetPutArg(instruction[i + 3].u.operand, 4, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_push_new_scope);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_catch: {
            emitGetCTIParam(CTI_ARGS_r, MacroAssembler::edi); // edi := r
            emitGetCTIParam(CTI_ARGS_exec, MacroAssembler::ecx);
            m_jit.emitMovl_mr(OBJECT_OFFSET(ExecState, m_exception), MacroAssembler::ecx, MacroAssembler::eax);
            m_jit.emitMovl_i32m(0, OBJECT_OFFSET(ExecState, m_exception), MacroAssembler::ecx);
            emitPutResult(instruction[i + 1].u.operand);
            i += 2;
            break;
        }
        case op_jmp_scopes: {
            unsigned count = instruction[i + 1].u.operand;
            emitPutArgConstant(count, 0);
            emitCall(i, Machine::cti_op_jmp_scopes);
            unsigned target = instruction[i + 2].u.operand;
            m_jmpTable.append(JmpTable(m_jit.emitUnlinkedJmp(), i + 2 + target));
            i += 3;
            break;
        }
        case op_put_by_index: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, MacroAssembler::ecx);
            emitPutArgConstant(instruction[i + 2].u.operand, 4);
            emitGetPutArg(instruction[i + 3].u.operand, 8, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_put_by_index);
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

            emitGetPutArg(scrutinee, 0, MacroAssembler::ecx);
            emitPutArgConstant(tableIndex, 4);
            emitCall(i, Machine::cti_op_switch_imm);
            m_jit.emitJmpN_r(MacroAssembler::eax);
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

            emitGetPutArg(scrutinee, 0, MacroAssembler::ecx);
            emitPutArgConstant(tableIndex, 4);
            emitCall(i, Machine::cti_op_switch_char);
            m_jit.emitJmpN_r(MacroAssembler::eax);
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

            emitGetPutArg(scrutinee, 0, MacroAssembler::ecx);
            emitPutArgConstant(tableIndex, 4);
            emitCall(i, Machine::cti_op_switch_string);
            m_jit.emitJmpN_r(MacroAssembler::eax);
            i += 4;
            break;
        }
        case op_del_by_val: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitGetPutArg(instruction[i + 3].u.operand, 4, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_del_by_val);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_put_getter: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, MacroAssembler::ecx);
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitGetPutArg(instruction[i + 3].u.operand, 8, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_put_getter);
            i += 4;
            break;
        }
        case op_put_setter: {
            emitGetPutArg(instruction[i + 1].u.operand, 0, MacroAssembler::ecx);
            Identifier* ident = &(m_codeBlock->identifiers[instruction[i + 2].u.operand]);
            emitPutArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitGetPutArg(instruction[i + 3].u.operand, 8, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_put_setter);
            i += 4;
            break;
        }
        case op_new_error: {
            JSValue* message = m_codeBlock->unexpectedConstants[instruction[i + 3].u.operand];
            emitPutArgConstant(instruction[i + 2].u.operand, 0);
            emitPutArgConstant(reinterpret_cast<unsigned>(message), 4);
            emitPutArgConstant(m_codeBlock->lineNumberForVPC(&instruction[i]), 8);
            emitCall(i, Machine::cti_op_new_error);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_debug: {
            emitPutArgConstant(instruction[i + 1].u.operand, 0);
            emitPutArgConstant(instruction[i + 2].u.operand, 4);
            emitPutArgConstant(instruction[i + 3].u.operand, 8);
            emitCall(i, Machine::cti_op_debug);
            i += 4;
            break;
        }
        case op_eq_null: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_eq_null);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_neq_null: {
            emitGetPutArg(instruction[i + 2].u.operand, 0, MacroAssembler::ecx);
            emitCall(i, Machine::cti_op_neq_null);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
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
            ASSERT_NOT_REACHED();
        }
    }
}


void CTI::privateCompileLinkPass()
{
    unsigned jmpTableCount = m_jmpTable.size();
    for (unsigned i = 0; i < jmpTableCount; ++i)
        m_jit.link(m_jmpTable[i].from, m_labels[m_jmpTable[i].to]);
    m_jmpTable.clear();
}

void CTI::privateCompileSlowCases()
{
    Instruction* instruction = m_codeBlock->instructions.begin();
    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end(); ++iter) {
        int i = iter->to;
       m_jit.emitRestoreArgumentReference();
        switch (m_machine->getOpcodeID(instruction[i].u.opcode)) {
        case op_add: {
            unsigned dst = instruction[i + 1].u.operand;
            unsigned src2 = instruction[i + 3].u.operand;
            if (src2 < m_codeBlock->constantRegisters.size()) {
                JSValue* value = m_codeBlock->constantRegisters[src2].jsValue(m_exec);
                if (JSImmediate::isNumber(value)) {
                    MacroAssembler::JmpSrc notImm = iter->from;
                    m_jit.link((++iter)->from, m_jit.label());
                    m_jit.emitSubl_i32r(getDeTaggedConstantImmediate(value), MacroAssembler::eax);
                    m_jit.link(notImm, m_jit.label());
                    emitPutArg(MacroAssembler::eax, 0);
                    emitGetPutArg(src2, 4, MacroAssembler::ecx);
                    emitCall(i, Machine::cti_op_add);
                    emitPutResult(dst);
                    i += 4;
                    break;
                }
            }

            ASSERT(!(static_cast<unsigned>(instruction[i + 2].u.operand) < m_codeBlock->constantRegisters.size()));

            MacroAssembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.emitSubl_rr(MacroAssembler::edx, MacroAssembler::eax);
            emitFastArithReTagImmediate(MacroAssembler::eax);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::edx, 4);
            emitCall(i, Machine::cti_op_add);
            emitPutResult(dst);
            i += 4;
            break;
        }
        case op_get_by_val: {
            MacroAssembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitFastArithIntToImmNoCheck(MacroAssembler::edx);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::edx, 4);
            emitCall(i, Machine::cti_op_get_by_val);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_sub: {
            MacroAssembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.emitAddl_rr(MacroAssembler::edx, MacroAssembler::eax);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::edx, 4);
            emitCall(i, Machine::cti_op_sub);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_rshift: {
            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::ecx, 4);
            emitCall(i, Machine::cti_op_rshift);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_lshift: {
            MacroAssembler::JmpSrc notImm1 = iter->from;
            MacroAssembler::JmpSrc notImm2 = (++iter)->from;
            m_jit.link((++iter)->from, m_jit.label());
            emitGetArg(instruction[i + 2].u.operand, MacroAssembler::eax);
            emitGetArg(instruction[i + 3].u.operand, MacroAssembler::ecx);
            m_jit.link(notImm1, m_jit.label());
            m_jit.link(notImm2, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::ecx, 4);
            emitCall(i, Machine::cti_op_lshift);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_loop_if_less: {
            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                m_jit.link(iter->from, m_jit.label());
                emitPutArg(MacroAssembler::edx, 0);
                emitGetPutArg(instruction[i + 2].u.operand, 4, MacroAssembler::ecx);
                emitCall(i, Machine::cti_op_loop_if_less);
                m_jit.emitTestl_rr(MacroAssembler::eax, MacroAssembler::eax);
                m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 3 + target]);
            } else {
                m_jit.link(iter->from, m_jit.label());
                m_jit.link((++iter)->from, m_jit.label());
                emitPutArg(MacroAssembler::eax, 0);
                emitPutArg(MacroAssembler::edx, 4);
                emitCall(i, Machine::cti_op_loop_if_less);
                m_jit.emitTestl_rr(MacroAssembler::eax, MacroAssembler::eax);
                m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 3 + target]);
            }
            i += 4;
            break;
        }
        case op_pre_inc: {
            unsigned srcDst = instruction[i + 1].u.operand;
            MacroAssembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.emitSubl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), MacroAssembler::eax);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_pre_inc);
            emitPutResult(srcDst);
            i += 2;
            break;
        }
        case op_put_by_val: {
            MacroAssembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitFastArithIntToImmNoCheck(MacroAssembler::edx);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::edx, 4);
            emitPutArg(MacroAssembler::ecx, 8);
            emitCall(i, Machine::cti_op_put_by_val);
            i += 4;
            break;
        }
        case op_loop_if_true: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_jtrue);
            m_jit.emitTestl_rr(MacroAssembler::eax, MacroAssembler::eax);
            unsigned target = instruction[i + 2].u.operand;
            m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 2 + target]);
            i += 3;
            break;
        }
        case op_pre_dec: {
            unsigned srcDst = instruction[i + 1].u.operand;
            MacroAssembler::JmpSrc notImm = iter->from;
            m_jit.link((++iter)->from, m_jit.label());
            m_jit.emitAddl_i8r(getDeTaggedConstantImmediate(JSImmediate::oneImmediate()), MacroAssembler::eax);
            m_jit.link(notImm, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_pre_dec);
            emitPutResult(srcDst);
            i += 2;
            break;
        }
        case op_jnless: {
            unsigned target = instruction[i + 3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(instruction[i + 2].u.operand);
            if (src2imm) {
                m_jit.link(iter->from, m_jit.label());
                emitPutArg(MacroAssembler::edx, 0);
                emitGetPutArg(instruction[i + 2].u.operand, 4, MacroAssembler::ecx);
                emitCall(i, Machine::cti_op_jless);
                m_jit.emitTestl_rr(MacroAssembler::eax, MacroAssembler::eax);
                m_jit.link(m_jit.emitUnlinkedJe(), m_labels[i + 3 + target]);
            } else {
                m_jit.link(iter->from, m_jit.label());
                m_jit.link((++iter)->from, m_jit.label());
                emitPutArg(MacroAssembler::eax, 0);
                emitPutArg(MacroAssembler::edx, 4);
                emitCall(i, Machine::cti_op_jless);
                m_jit.emitTestl_rr(MacroAssembler::eax, MacroAssembler::eax);
                m_jit.link(m_jit.emitUnlinkedJe(), m_labels[i + 3 + target]);
            }
            i += 4;
            break;
        }
        case op_not: {
            m_jit.link(iter->from, m_jit.label());
            m_jit.emitXorl_i8r(JSImmediate::FullTagTypeBool, MacroAssembler::eax);
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_not);
            emitPutResult(instruction[i + 1].u.operand);
            i += 3;
            break;
        }
        case op_jfalse: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_jtrue);
            m_jit.emitTestl_rr(MacroAssembler::eax, MacroAssembler::eax);
            unsigned target = instruction[i + 2].u.operand;
            m_jit.link(m_jit.emitUnlinkedJe(), m_labels[i + 2 + target]); // inverted!
            i += 3;
            break;
        }
        case op_post_inc: {
            unsigned srcDst = instruction[i + 2].u.operand;
            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_post_inc);
            emitPutResult(instruction[i + 1].u.operand);
            emitGetCTIParam(CTI_ARGS_2ndResult, MacroAssembler::eax);
            emitPutResult(srcDst);
            i += 3;
            break;
        }
        case op_bitnot: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_bitnot);
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
                emitGetPutArg(src1, 0, MacroAssembler::ecx);
                emitPutArg(MacroAssembler::eax, 4);
                emitCall(i, Machine::cti_op_bitand);
                emitPutResult(dst);
            } else if (getConstantImmediateNumericArg(src2)) {
                m_jit.link(iter->from, m_jit.label());
                emitPutArg(MacroAssembler::eax, 0);
                emitGetPutArg(src2, 4, MacroAssembler::ecx);
                emitCall(i, Machine::cti_op_bitand);
                emitPutResult(dst);
            } else {
                m_jit.link(iter->from, m_jit.label());
                emitGetPutArg(src1, 0, MacroAssembler::ecx);
                emitPutArg(MacroAssembler::edx, 4);
                emitCall(i, Machine::cti_op_bitand);
                emitPutResult(dst);
            }
            i += 4;
            break;
        }
        case op_jtrue: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_jtrue);
            m_jit.emitTestl_rr(MacroAssembler::eax, MacroAssembler::eax);
            unsigned target = instruction[i + 2].u.operand;
            m_jit.link(m_jit.emitUnlinkedJne(), m_labels[i + 2 + target]);
            i += 3;
            break;
        }
        case op_post_dec: {
            unsigned srcDst = instruction[i + 2].u.operand;
            m_jit.link(iter->from, m_jit.label());
            m_jit.link((++iter)->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitCall(i, Machine::cti_op_post_dec);
            emitPutResult(instruction[i + 1].u.operand);
            emitGetCTIParam(CTI_ARGS_2ndResult, MacroAssembler::eax);
            emitPutResult(srcDst);
            i += 3;
            break;
        }
        case op_bitxor: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::edx, 4);
            emitCall(i, Machine::cti_op_bitxor);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_bitor: {
            m_jit.link(iter->from, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::edx, 4);
            emitCall(i, Machine::cti_op_bitor);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        case op_mod: {
            MacroAssembler::JmpSrc notImm1 = iter->from;
            MacroAssembler::JmpSrc notImm2 = (++iter)->from;
            m_jit.link((++iter)->from, m_jit.label());
            emitFastArithReTagImmediate(MacroAssembler::eax);
            emitFastArithReTagImmediate(MacroAssembler::ecx);
            m_jit.link(notImm1, m_jit.label());
            m_jit.link(notImm2, m_jit.label());
            emitPutArg(MacroAssembler::eax, 0);
            emitPutArg(MacroAssembler::ecx, 4);
            emitCall(i, Machine::cti_op_mod);
            emitPutResult(instruction[i + 1].u.operand);
            i += 4;
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        m_jit.link(m_jit.emitUnlinkedJmp(), m_labels[i]);
    }
}

void CTI::privateCompile()
{
    // Could use a emitPopl_m, but would need to offset the following instruction if so.
    m_jit.emitPopl_r(MacroAssembler::ecx);
    emitGetCTIParam(CTI_ARGS_r, MacroAssembler::edi); // edi := r
    emitPutToCallFrameHeader(MacroAssembler::ecx, RegisterFile::CTIReturnEIP);

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();

    ASSERT(m_jmpTable.isEmpty());

    void* code = m_jit.copy();
    ASSERT(code);

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (unsigned i = 0; i < m_switches.size(); ++i) {
        SwitchRecord record = m_switches[i];
        unsigned opcodeIndex = record.m_opcodeIndex;

        if (record.m_type != SwitchRecord::String) {
            ASSERT(record.m_type == SwitchRecord::Immediate || record.m_type == SwitchRecord::Character); 
            ASSERT(record.m_jumpTable.m_simpleJumpTable->branchOffsets.size() == record.m_jumpTable.m_simpleJumpTable->ctiOffsets.size());

            record.m_jumpTable.m_simpleJumpTable->ctiDefault = m_jit.getRelocatedAddress(code, m_labels[opcodeIndex + 3 + record.m_defaultOffset]);

            for (unsigned j = 0; j < record.m_jumpTable.m_simpleJumpTable->branchOffsets.size(); ++j) {
                unsigned offset = record.m_jumpTable.m_simpleJumpTable->branchOffsets[j];
                record.m_jumpTable.m_simpleJumpTable->ctiOffsets[j] = offset ? m_jit.getRelocatedAddress(code, m_labels[opcodeIndex + 3 + offset]) : record.m_jumpTable.m_simpleJumpTable->ctiDefault;
            }
        } else {
            ASSERT(record.m_type == SwitchRecord::String);

            record.m_jumpTable.m_stringJumpTable->ctiDefault = m_jit.getRelocatedAddress(code, m_labels[opcodeIndex + 3 + record.m_defaultOffset]);

            StringJumpTable::StringOffsetTable::iterator end = record.m_jumpTable.m_stringJumpTable->offsetTable.end();            
            for (StringJumpTable::StringOffsetTable::iterator it = record.m_jumpTable.m_stringJumpTable->offsetTable.begin(); it != end; ++it) {
                unsigned offset = it->second.branchOffset;
                it->second.ctiOffset = offset ? m_jit.getRelocatedAddress(code, m_labels[opcodeIndex + 3 + offset]) : record.m_jumpTable.m_stringJumpTable->ctiDefault;
            }
        }
    }

    for (Vector<HandlerInfo>::iterator iter = m_codeBlock->exceptionHandlers.begin(); iter != m_codeBlock->exceptionHandlers.end(); ++iter)
         iter->nativeCode = m_jit.getRelocatedAddress(code, m_labels[iter->target]);

    // FIXME: There doesn't seem to be a way to hint to a hashmap that it should make a certain capacity available;
    // could be faster if we could do something like this:
    // m_codeBlock->ctiReturnAddressVPCMap.grow(m_calls.size());
    for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
        MacroAssembler::link(code, iter->from, iter->to);
        m_codeBlock->ctiReturnAddressVPCMap.add(m_jit.getRelocatedAddress(code, iter->from), iter->opcodeIndex);
    }

    // Link absolute addresses for jsr
    for (Vector<JSRInfo>::iterator iter = m_jsrSites.begin(); iter != m_jsrSites.end(); ++iter)
        MacroAssembler::linkAbsoluteAddress(code, iter->addrPosition, iter->target);

    m_codeBlock->ctiCode = code;
}

void* CTI::privateCompileGetByIdSelf(StructureID* structureID, size_t cachedOffset)
{
    // Check eax is an object of the right StructureID.
    m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.emitCmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // Checks out okay! - getDirectOffset
    m_jit.emitMovl_mr(OBJECT_OFFSET(JSObject, m_propertyMap) + OBJECT_OFFSET(PropertyMap, m_u.table), MacroAssembler::eax, MacroAssembler::eax);
    m_jit.emitMovl_mr(OBJECT_OFFSET(PropertyMapHashTable, entryIndices[0]) + (cachedOffset * sizeof(JSValue*)), MacroAssembler::eax, MacroAssembler::eax);
    m_jit.emitRet();

    void* code = m_jit.copy();
    ASSERT(code);

    MacroAssembler::link(code, failureCases1, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    MacroAssembler::link(code, failureCases2, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    
    m_codeBlock->structureIDAccessStubs.append(code);
    
    return code;
}

void* CTI::privateCompileGetByIdProto(StructureID* structureID, StructureID* prototypeStructureID, size_t cachedOffset)
{
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a StructureID that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = static_cast<JSObject*>(structureID->prototype());
    PropertyMapHashTable** protoTableAddress = &(protoObject->m_propertyMap.m_u.table);
    m_jit.emitMovl_mr(static_cast<void*>(protoTableAddress), MacroAssembler::edx);

    // check eax is an object of the right StructureID.
    m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.emitCmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // Check the prototype object's StructureID had not changed.
    StructureID** protoStructureIDAddress = &(protoObject->m_structureID);
    m_jit.emitCmpl_i32m(reinterpret_cast<uint32_t>(prototypeStructureID), static_cast<void*>(protoStructureIDAddress));
    MacroAssembler::JmpSrc failureCases3 = m_jit.emitUnlinkedJne();

    // Checks out okay! - getDirectOffset

    m_jit.emitMovl_mr(OBJECT_OFFSET(PropertyMapHashTable, entryIndices[0]) + (cachedOffset * sizeof(JSValue*)), MacroAssembler::edx, MacroAssembler::eax);

    m_jit.emitRet();

    void* code = m_jit.copy();
    ASSERT(code);

    MacroAssembler::link(code, failureCases1, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    MacroAssembler::link(code, failureCases2, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    MacroAssembler::link(code, failureCases3, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));

    m_codeBlock->structureIDAccessStubs.append(code);

    return code;
}

void* CTI::privateCompileGetByIdChain(StructureID* structureID, StructureIDChain* chain, size_t count, size_t cachedOffset)
{
    ASSERT(count);
    
    Vector<MacroAssembler::JmpSrc> bucketsOfFail;

    // Check eax is an object of the right StructureID.
    m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::eax);
    bucketsOfFail.append(m_jit.emitUnlinkedJne());
    m_jit.emitCmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), MacroAssembler::eax);
    bucketsOfFail.append(m_jit.emitUnlinkedJne());

    StructureID* currStructureID = structureID;
    RefPtr<StructureID>* chainEntries = chain->head();
    JSCell* protoObject = 0;
    for (unsigned i = 0; i<count; ++i) {
        protoObject = static_cast<JSCell*>(currStructureID->prototype());
        currStructureID = chainEntries[i].get();

        // Check the prototype object's StructureID had not changed.
        StructureID** protoStructureIDAddress = &(protoObject->m_structureID);
        m_jit.emitCmpl_i32m(reinterpret_cast<uint32_t>(currStructureID), static_cast<void*>(protoStructureIDAddress));
        bucketsOfFail.append(m_jit.emitUnlinkedJne());
    }
    ASSERT(protoObject);
 
    PropertyMapHashTable** protoTableAddress = &(reinterpret_cast<JSObject*>(protoObject)->m_propertyMap.m_u.table);
    m_jit.emitMovl_mr(static_cast<void*>(protoTableAddress), MacroAssembler::edx);
    m_jit.emitMovl_mr(OBJECT_OFFSET(PropertyMapHashTable, entryIndices[0]) + (cachedOffset * sizeof(JSValue*)), MacroAssembler::edx, MacroAssembler::eax);
    m_jit.emitRet();

    bucketsOfFail.append(m_jit.emitUnlinkedJmp());

    void* code = m_jit.copy();
    ASSERT(code);

    for (unsigned i = 0; i < bucketsOfFail.size(); ++i)
        MacroAssembler::link(code, bucketsOfFail[i], reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    m_codeBlock->structureIDAccessStubs.append(code);
    return code;
}

void* CTI::privateCompilePutByIdReplace(StructureID* structureID, size_t cachedOffset)
{
    // check eax is an object of the right StructureID.
    m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.emitCmpl_i32m(reinterpret_cast<uint32_t>(structureID), OBJECT_OFFSET(JSCell, m_structureID), MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // checks out okay! - putDirectOffset
    m_jit.emitMovl_mr(OBJECT_OFFSET(JSObject, m_propertyMap) + OBJECT_OFFSET(PropertyMap, m_u.table), MacroAssembler::eax, MacroAssembler::eax);
    m_jit.emitMovl_rm(MacroAssembler::edx, OBJECT_OFFSET(PropertyMapHashTable, entryIndices[0]) + (cachedOffset * sizeof(JSValue*)), MacroAssembler::eax);
    m_jit.emitRet();

    void* code = m_jit.copy();
    ASSERT(code);
    
    MacroAssembler::link(code, failureCases1, reinterpret_cast<void*>(Machine::cti_op_put_by_id_fail));
    MacroAssembler::link(code, failureCases2, reinterpret_cast<void*>(Machine::cti_op_put_by_id_fail));

    m_codeBlock->structureIDAccessStubs.append(code);
    
    return code;
}

void* CTI::privateArrayLengthTrampoline()
{
    // Check eax is an array
    m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.emitCmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsArrayVptr), MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // Checks out okay! - get the length from the storage
    m_jit.emitMovl_mr(OBJECT_OFFSET(JSArray, m_storage), MacroAssembler::eax, MacroAssembler::eax);
    m_jit.emitMovl_mr(OBJECT_OFFSET(ArrayStorage, m_length), MacroAssembler::eax, MacroAssembler::eax);

    m_jit.emitAddl_rr(MacroAssembler::eax, MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases3 = m_jit.emitUnlinkedJo();
    m_jit.emitAddl_i8r(1, MacroAssembler::eax);
    
    m_jit.emitRet();

    void* code = m_jit.copy();
    ASSERT(code);

    MacroAssembler::link(code, failureCases1, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    MacroAssembler::link(code, failureCases2, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    MacroAssembler::link(code, failureCases3, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    
    return code;
}

void* CTI::privateStringLengthTrampoline()
{
    // Check eax is a string
    m_jit.emitTestl_i32r(JSImmediate::TagMask, MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases1 = m_jit.emitUnlinkedJne();
    m_jit.emitCmpl_i32m(reinterpret_cast<unsigned>(m_machine->m_jsStringVptr), MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases2 = m_jit.emitUnlinkedJne();

    // Checks out okay! - get the length from the Ustring.
    m_jit.emitMovl_mr(OBJECT_OFFSET(JSString, m_value) + OBJECT_OFFSET(UString, m_rep), MacroAssembler::eax, MacroAssembler::eax);
    m_jit.emitMovl_mr(OBJECT_OFFSET(UString::Rep, len), MacroAssembler::eax, MacroAssembler::eax);

    m_jit.emitAddl_rr(MacroAssembler::eax, MacroAssembler::eax);
    MacroAssembler::JmpSrc failureCases3 = m_jit.emitUnlinkedJo();
    m_jit.emitAddl_i8r(1, MacroAssembler::eax);
    
    m_jit.emitRet();

    void* code = m_jit.copy();
    ASSERT(code);

    MacroAssembler::link(code, failureCases1, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    MacroAssembler::link(code, failureCases2, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));
    MacroAssembler::link(code, failureCases3, reinterpret_cast<void*>(Machine::cti_op_get_by_id_fail));

    return code;
}

#if ENABLE(WREC)

void* CTI::compileRegExp(ExecState* exec, const UString& pattern, unsigned* numSubpatterns_ptr, const char** error_ptr, bool ignoreCase, bool multiline)
{
    // TODO: better error messages
    if (pattern.size() > MaxPatternSize) {
        *error_ptr = "regular expression too large";
        return 0;
    }

    MacroAssembler jit(exec->machine()->jitCodeBuffer());
    WRECParser parser(pattern, ignoreCase, multiline, jit);
    
    jit.emitConvertToFastCall();
    // (0) Setup:
    //     Preserve regs & initialize OUTPUT_REG.
    jit.emitPushl_r(WRECGenerator::OUTPUT_REG);
    jit.emitPushl_r(WRECGenerator::CURR_VAL_REG);
    // push pos onto the stack, both to preserve and as a parameter available to parseDisjunction
    jit.emitPushl_r(WRECGenerator::CURR_POS_REG);
    // load output pointer
    jit.emitMovl_mr(16
#if COMPILER(MSVC)
                    + 3 * sizeof(void*)
#endif
                    , MacroAssembler::esp, WRECGenerator::OUTPUT_REG);
    
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

    jit.emitTestl_rr(WRECGenerator::OUTPUT_REG, WRECGenerator::OUTPUT_REG);
    WRECGenerator::JmpSrc noOutput = jit.emitUnlinkedJe();

    jit.emitMovl_rm(WRECGenerator::CURR_POS_REG, 4, WRECGenerator::OUTPUT_REG);
    jit.emitPopl_r(MacroAssembler::eax);
    jit.emitMovl_rm(MacroAssembler::eax, WRECGenerator::OUTPUT_REG);
    jit.emitPopl_r(WRECGenerator::CURR_VAL_REG);
    jit.emitPopl_r(WRECGenerator::OUTPUT_REG);
    jit.emitRet();
    
    jit.link(noOutput, jit.label());
    
    jit.emitPopl_r(MacroAssembler::eax);
    jit.emitMovl_rm(MacroAssembler::eax, WRECGenerator::OUTPUT_REG);
    jit.emitPopl_r(WRECGenerator::CURR_VAL_REG);
    jit.emitPopl_r(WRECGenerator::OUTPUT_REG);
    jit.emitRet();

    // (3) Failure:
    //     All fails link to here.  Progress the start point & if it is within scope, loop.
    //     Otherwise, return fail value.
    WRECGenerator::JmpDst here = jit.label();
    for (unsigned i = 0; i < failures.size(); ++i)
        jit.link(failures[i], here);
    failures.clear();

    jit.emitMovl_mr(MacroAssembler::esp, WRECGenerator::CURR_POS_REG);
    jit.emitAddl_i8r(1, WRECGenerator::CURR_POS_REG);
    jit.emitMovl_rm(WRECGenerator::CURR_POS_REG, MacroAssembler::esp);
    jit.emitCmpl_rr(WRECGenerator::LENGTH_REG, WRECGenerator::CURR_POS_REG);
    jit.link(jit.emitUnlinkedJle(), nextLabel);

    jit.emitAddl_i8r(4, MacroAssembler::esp);

    jit.emitMovl_i32r(-1, MacroAssembler::eax);
    jit.emitPopl_r(WRECGenerator::CURR_VAL_REG);
    jit.emitPopl_r(WRECGenerator::OUTPUT_REG);
    jit.emitRet();

    *numSubpatterns_ptr = parser.m_numSubpatterns;

    void* code = jit.copy();
    ASSERT(code);
    return code;
}

#endif // ENABLE(WREC)

} // namespace JSC

#endif // ENABLE(CTI)
