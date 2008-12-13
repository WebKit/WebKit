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
#include "JIT.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "JITInlineMethods.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace JSC {

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
    "call " SYMBOL_STRING(_ZN3JSC11Interpreter12cti_vm_throwEPvz) "\n"
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
            call JSC::Interpreter::cti_vm_throw;
            add esp, 0x20;
            pop ebx;
            pop edi;
            pop esi;
            ret;
        }
    }
    
}

#endif

void ctiSetReturnAddress(void** where, void* what)
{
    *where = what;
}

void ctiRepatchCallByReturnAddress(void* where, void* what)
{
    (static_cast<void**>(where))[-1] = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(what) - reinterpret_cast<uintptr_t>(where));
}

JIT::JIT(JSGlobalData* globalData, CodeBlock* codeBlock)
    : m_interpreter(globalData->interpreter)
    , m_globalData(globalData)
    , m_codeBlock(codeBlock)
    , m_labels(codeBlock ? codeBlock->instructions().size() : 0)
    , m_propertyAccessCompilationInfo(codeBlock ? codeBlock->numberOfStructureStubInfos() : 0)
    , m_callStructureStubCompilationInfo(codeBlock ? codeBlock->numberOfCallLinkInfos() : 0)
    , m_lastResultBytecodeRegister(std::numeric_limits<int>::max())
    , m_jumpTargetsPosition(0)
{
}

void JIT::compileOpStrictEq(Instruction* currentInstruction, CompileOpStrictEqType type)
{
    bool negated = (type == OpNStrictEq);

    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitGetVirtualRegisters(src1, X86::eax, src2, X86::edx);

    // Check that bot are immediates, if so check if they're equal
    Jump firstNotImmediate = emitJumpIfJSCell(X86::eax);
    Jump secondNotImmediate = emitJumpIfJSCell(X86::edx);
    Jump bothWereImmediatesButNotEqual = jne32(X86::edx, X86::eax);

    // They are equal - set the result to true. (Or false, if negated).
    move(Imm32(asInteger(jsBoolean(!negated))), X86::eax);
    Jump bothWereImmediatesAndEqual = jump();

    // eax was not an immediate, we haven't yet checked edx.
    // If edx is also a JSCell, or is 0, then jump to a slow case,
    // otherwise these values are not equal.
    firstNotImmediate.link(this);
    emitJumpSlowCaseIfJSCell(X86::edx);
    addSlowCase(je32(X86::edx, Imm32(asInteger(JSImmediate::zeroImmediate()))));
    Jump firstWasNotImmediate = jump();

    // eax was an immediate, but edx wasn't.
    // If eax is 0 jump to a slow case, otherwise these values are not equal.
    secondNotImmediate.link(this);
    addSlowCase(je32(X86::eax, Imm32(asInteger(JSImmediate::zeroImmediate()))));

    // We get here if the two values are different immediates, or one is 0 and the other is a JSCell.
    // Vaelues are not equal, set the result to false.
    bothWereImmediatesButNotEqual.link(this);
    firstWasNotImmediate.link(this);
    move(Imm32(asInteger(jsBoolean(negated))), X86::eax);
    
    bothWereImmediatesAndEqual.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emitSlowScriptCheck()
{
    Jump skipTimeout = jnzSub32(Imm32(1), X86::esi);
    emitCTICall(Interpreter::cti_timeout_check);
    move(X86::eax, X86::esi);
    skipTimeout.link(this);

    killLastResultRegister();
}


#define NEXT_OPCODE(name) \
    m_bytecodeIndex += OPCODE_LENGTH(name); \
    break;

#define CTI_COMPILE_BINARY_OP(name) \
    case name: { \
        emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx); \
        emitPutCTIArgFromVirtualRegister(currentInstruction[3].u.operand, 4, X86::ecx); \
        emitCTICall(Interpreter::cti_##name); \
        emitPutVirtualRegister(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

#define CTI_COMPILE_UNARY_OP(name) \
    case name: { \
        emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx); \
        emitCTICall(Interpreter::cti_##name); \
        emitPutVirtualRegister(currentInstruction[1].u.operand); \
        NEXT_OPCODE(name); \
    }

void JIT::privateCompileMainPass()
{
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    unsigned instructionCount = m_codeBlock->instructions().size();
    unsigned propertyAccessInstructionIndex = 0;
    unsigned globalResolveInfoIndex = 0;
    unsigned callLinkInfoIndex = 0;

    for (m_bytecodeIndex = 0; m_bytecodeIndex < instructionCount; ) {
        Instruction* currentInstruction = instructionsBegin + m_bytecodeIndex;
        ASSERT_WITH_MESSAGE(m_interpreter->isOpcode(currentInstruction->u.opcode), "privateCompileMainPass gone bad @ %d", m_bytecodeIndex);

#if ENABLE(OPCODE_SAMPLING)
        if (m_bytecodeIndex > 0) // Avoid the overhead of sampling op_enter twice.
            store32(m_interpreter->sampler()->encodeSample(currentInstruction), m_interpreter->sampler()->sampleSlot());
#endif

        m_labels[m_bytecodeIndex] = __ label();
        OpcodeID opcodeID = m_interpreter->getOpcodeID(currentInstruction->u.opcode);

        switch (opcodeID) {
        case op_mov: {
            emitGetVirtualRegister(currentInstruction[2].u.operand, X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_mov);
        }
        case op_add: {
            unsigned dst = currentInstruction[1].u.operand;
            unsigned src1 = currentInstruction[2].u.operand;
            unsigned src2 = currentInstruction[3].u.operand;

            if (JSValue* value = getConstantImmediateNumericArg(src1)) {
                emitGetVirtualRegister(src2, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                addSlowCase(joAdd32(Imm32(getDeTaggedConstantImmediate(value)), X86::eax));
                emitPutVirtualRegister(dst);
            } else if (JSValue* value = getConstantImmediateNumericArg(src2)) {
                emitGetVirtualRegister(src1, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                addSlowCase(joAdd32(Imm32(getDeTaggedConstantImmediate(value)), X86::eax));
                emitPutVirtualRegister(dst);
            } else {
                OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
                if (types.first().mightBeNumber() && types.second().mightBeNumber())
                    compileBinaryArithOp(op_add, currentInstruction[1].u.operand, currentInstruction[2].u.operand, currentInstruction[3].u.operand, OperandTypes::fromInt(currentInstruction[4].u.operand));
                else {
                    emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx);
                    emitPutCTIArgFromVirtualRegister(currentInstruction[3].u.operand, 4, X86::ecx);
                    emitCTICall(Interpreter::cti_op_add);
                    emitPutVirtualRegister(currentInstruction[1].u.operand);
                }
            }
            NEXT_OPCODE(op_add);
        }
        case op_end: {
            if (m_codeBlock->needsFullScopeChain())
                emitCTICall(Interpreter::cti_op_end);
            emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);
            __ pushl_m(RegisterFile::ReturnPC * static_cast<int>(sizeof(Register)), X86::edi);
            __ ret();
            NEXT_OPCODE(op_end);
        }
        case op_jmp: {
            unsigned target = currentInstruction[1].u.operand;
            addJump(jump(), target + 1);
            NEXT_OPCODE(op_jmp);
        }
        case op_pre_inc: {
            int srcDst = currentInstruction[1].u.operand;
            emitGetVirtualRegister(srcDst, X86::eax);
            emitJumpSlowCaseIfNotImmNum(X86::eax);
            addSlowCase(joAdd32(Imm32(getDeTaggedConstantImmediate(JSImmediate::oneImmediate())), X86::eax));
            emitPutVirtualRegister(srcDst);
            NEXT_OPCODE(op_pre_inc);
        }
        case op_loop: {
            emitSlowScriptCheck();

            unsigned target = currentInstruction[1].u.operand;
            addJump(jump(), target + 1);
            NEXT_OPCODE(op_end);
        }
        case op_loop_if_less: {
            emitSlowScriptCheck();

            unsigned target = currentInstruction[3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(currentInstruction[2].u.operand);
            if (src2imm) {
                emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                addJump(jl32(X86::eax, Imm32(asInteger(src2imm))), target + 3);
            } else {
                emitGetVirtualRegisters(currentInstruction[1].u.operand, X86::eax, currentInstruction[2].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::edx);
                addJump(jl32(X86::eax, X86::edx), target + 3);
            }
            NEXT_OPCODE(op_loop_if_less);
        }
        case op_loop_if_lesseq: {
            emitSlowScriptCheck();

            unsigned target = currentInstruction[3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(currentInstruction[2].u.operand);
            if (src2imm) {
                emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                addJump(jle32(X86::eax, Imm32(asInteger(src2imm))), target + 3);
            } else {
                emitGetVirtualRegisters(currentInstruction[1].u.operand, X86::eax, currentInstruction[2].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::edx);
                addJump(jle32(X86::eax, X86::edx), target + 3);
            }
            NEXT_OPCODE(op_loop_if_lesseq);
        }
        case op_new_object: {
            emitCTICall(Interpreter::cti_op_new_object);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_new_object);
        }
        case op_put_by_id: {
            compilePutByIdHotPath(currentInstruction[1].u.operand, &(m_codeBlock->identifier(currentInstruction[2].u.operand)), currentInstruction[3].u.operand, propertyAccessInstructionIndex++);
            NEXT_OPCODE(op_put_by_id);
        }
        case op_get_by_id: {
            compileGetByIdHotPath(currentInstruction[1].u.operand, currentInstruction[2].u.operand, &(m_codeBlock->identifier(currentInstruction[3].u.operand)), propertyAccessInstructionIndex++);
            NEXT_OPCODE(op_get_by_id);
        }
        case op_instanceof: {
            emitGetVirtualRegister(currentInstruction[2].u.operand, X86::eax); // value
            emitGetVirtualRegister(currentInstruction[3].u.operand, X86::ecx); // baseVal
            emitGetVirtualRegister(currentInstruction[4].u.operand, X86::edx); // proto

            // check if any are immediates
            move(X86::eax, X86::ebx);
            or32(X86::ecx, X86::ebx);
            or32(X86::edx, X86::ebx);
            emitJumpSlowCaseIfNotJSCell(X86::ebx);

            // check that all are object type - this is a bit of a bithack to avoid excess branching;
            // we check that the sum of the three type codes from Structures is exactly 3 * ObjectType,
            // this works because NumberType and StringType are smaller
            move(Imm32(3 * ObjectType), X86::ebx);
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::eax);
            loadPtr(Address(X86::ecx, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
            loadPtr(Address(X86::edx, FIELD_OFFSET(JSCell, m_structure)), X86::edx);
            sub32(Address(X86::eax, FIELD_OFFSET(Structure, m_typeInfo.m_type)), X86::ebx);
            sub32(Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo.m_type)), X86::ebx);
            addSlowCase(jne32(Address(X86::edx, FIELD_OFFSET(Structure, m_typeInfo.m_type)), X86::ebx));

            // check that baseVal's flags include ImplementsHasInstance but not OverridesHasInstance
            load32(Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo.m_flags)), X86::ecx);
            and32(Imm32(ImplementsHasInstance | OverridesHasInstance), X86::ecx);
            addSlowCase(jne32(X86::ecx, Imm32(ImplementsHasInstance)));

            emitGetVirtualRegister(currentInstruction[2].u.operand, X86::ecx); // reload value
            emitGetVirtualRegister(currentInstruction[4].u.operand, X86::edx); // reload proto

            // optimistically load true result
            move(Imm32(asInteger(jsBoolean(true))), X86::eax);

            Label loop(this);

            // load value's prototype
            loadPtr(Address(X86::ecx, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
            loadPtr(Address(X86::ecx, FIELD_OFFSET(Structure, m_prototype)), X86::ecx);

            Jump exit = jePtr(X86::ecx, X86::edx);

            jne32(X86::ecx, Imm32(asInteger(jsNull())), loop);

            move(Imm32(asInteger(jsBoolean(false))), X86::eax);

            exit.link(this);

            emitPutVirtualRegister(currentInstruction[1].u.operand);

            NEXT_OPCODE(op_instanceof);
        }
        case op_del_by_id: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx);
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitCTICall(Interpreter::cti_op_del_by_id);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_del_by_id);
        }
        case op_mul: {
            unsigned dst = currentInstruction[1].u.operand;
            unsigned src1 = currentInstruction[2].u.operand;
            unsigned src2 = currentInstruction[3].u.operand;

            // For now, only plant a fast int case if the constant operand is greater than zero.
            JSValue* src1Value = getConstantImmediateNumericArg(src1);
            JSValue* src2Value = getConstantImmediateNumericArg(src2);
            int32_t value;
            if (src1Value && ((value = JSImmediate::intValue(src1Value)) > 0)) {
                emitGetVirtualRegister(src2, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                emitFastArithDeTagImmediate(X86::eax);
                addSlowCase(joMul32(Imm32(value), X86::eax, X86::eax));
                emitFastArithReTagImmediate(X86::eax);
                emitPutVirtualRegister(dst);
            } else if (src2Value && ((value = JSImmediate::intValue(src2Value)) > 0)) {
                emitGetVirtualRegister(src1, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                emitFastArithDeTagImmediate(X86::eax);
                addSlowCase(joMul32(Imm32(value), X86::eax, X86::eax));
                emitFastArithReTagImmediate(X86::eax);
                emitPutVirtualRegister(dst);
            } else
                compileBinaryArithOp(op_mul, currentInstruction[1].u.operand, currentInstruction[2].u.operand, currentInstruction[3].u.operand, OperandTypes::fromInt(currentInstruction[4].u.operand));

            NEXT_OPCODE(op_mul);
        }
        case op_new_func: {
            FuncDeclNode* func = m_codeBlock->function(currentInstruction[2].u.operand);
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(func), 0);
            emitCTICall(Interpreter::cti_op_new_func);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_new_func);
        }
        case op_call: {
            compileOpCall(opcodeID, currentInstruction, callLinkInfoIndex++);
            NEXT_OPCODE(op_call);
        }
        case op_call_eval: {
            compileOpCall(opcodeID, currentInstruction, callLinkInfoIndex++);
            NEXT_OPCODE(op_call_eval);
        }
        case op_construct: {
            compileOpCall(opcodeID, currentInstruction, callLinkInfoIndex++);
            NEXT_OPCODE(op_construct);
        }
        case op_get_global_var: {
            JSVariableObject* globalObject = static_cast<JSVariableObject*>(currentInstruction[2].u.jsCell);
            move(ImmPtr(globalObject), X86::eax);
            emitGetVariableObjectRegister(X86::eax, currentInstruction[3].u.operand, X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_get_global_var);
        }
        case op_put_global_var: {
            emitGetVirtualRegister(currentInstruction[3].u.operand, X86::edx);
            JSVariableObject* globalObject = static_cast<JSVariableObject*>(currentInstruction[1].u.jsCell);
            move(ImmPtr(globalObject), X86::eax);
            emitPutVariableObjectRegister(X86::edx, X86::eax, currentInstruction[2].u.operand);
            NEXT_OPCODE(op_put_global_var);
        }
        case op_get_scoped_var: {
            int skip = currentInstruction[3].u.operand + m_codeBlock->needsFullScopeChain();

            emitGetFromCallFrameHeader(RegisterFile::ScopeChain, X86::eax);
            while (skip--)
                loadPtr(Address(X86::eax, FIELD_OFFSET(ScopeChainNode, next)), X86::eax);

            loadPtr(Address(X86::eax, FIELD_OFFSET(ScopeChainNode, object)), X86::eax);
            emitGetVariableObjectRegister(X86::eax, currentInstruction[2].u.operand, X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_get_scoped_var);
        }
        case op_put_scoped_var: {
            int skip = currentInstruction[2].u.operand + m_codeBlock->needsFullScopeChain();

            emitGetFromCallFrameHeader(RegisterFile::ScopeChain, X86::edx);
            emitGetVirtualRegister(currentInstruction[3].u.operand, X86::eax);
            while (skip--)
                loadPtr(Address(X86::edx, FIELD_OFFSET(ScopeChainNode, next)), X86::edx);

            loadPtr(Address(X86::edx, FIELD_OFFSET(ScopeChainNode, object)), X86::edx);
            emitPutVariableObjectRegister(X86::eax, X86::edx, currentInstruction[1].u.operand);
            NEXT_OPCODE(op_put_scoped_var);
        }
        case op_tear_off_activation: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[1].u.operand, 0, X86::ecx);
            emitCTICall(Interpreter::cti_op_tear_off_activation);
            NEXT_OPCODE(op_tear_off_activation);
        }
        case op_tear_off_arguments: {
            emitCTICall(Interpreter::cti_op_tear_off_arguments);
            NEXT_OPCODE(op_tear_off_arguments);
        }
        case op_ret: {
            // We could JIT generate the deref, only calling out to C when the refcount hits zero.
            if (m_codeBlock->needsFullScopeChain())
                emitCTICall(Interpreter::cti_op_ret_scopeChain);

            // Return the result in %eax.
            emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);

            // Grab the return address.
            emitGetFromCallFrameHeader(RegisterFile::ReturnPC, X86::edx);

            // Restore our caller's "r".
            emitGetFromCallFrameHeader(RegisterFile::CallerFrame, X86::edi);

            // Return.
            __ pushl_r(X86::edx);
            __ ret();

            NEXT_OPCODE(op_ret);
        }
        case op_new_array: {
            emitPutCTIArgConstant(currentInstruction[2].u.operand, 0);
            emitPutCTIArgConstant(currentInstruction[3].u.operand, 4);
            emitCTICall(Interpreter::cti_op_new_array);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_new_array);
        }
        case op_resolve: {
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCTICall(Interpreter::cti_op_resolve);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_resolve);
        }
        case op_construct_verify: {
            emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);

            emitJumpSlowCaseIfNotJSCell(X86::eax);
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
            addSlowCase(jne32(Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo) + FIELD_OFFSET(TypeInfo, m_type)), Imm32(ObjectType)));

            NEXT_OPCODE(op_construct_verify);
        }
        case op_get_by_val: {
            emitGetVirtualRegisters(currentInstruction[2].u.operand, X86::eax, currentInstruction[3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNum(X86::edx);
            emitFastArithImmToInt(X86::edx);
            emitJumpSlowCaseIfNotJSCell(X86::eax);
            addSlowCase(jnePtr(Address(X86::eax), ImmPtr(m_interpreter->m_jsArrayVptr)));

            // This is an array; get the m_storage pointer into ecx, then check if the index is below the fast cutoff
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSArray, m_storage)), X86::ecx);
            addSlowCase(jae32(X86::edx, Address(X86::eax, FIELD_OFFSET(JSArray, m_fastAccessCutoff))));

            // Get the value from the vector
            loadPtr(BaseIndex(X86::ecx, X86::edx, ScalePtr, FIELD_OFFSET(ArrayStorage, m_vector[0])), X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_get_by_val);
        }
        case op_resolve_func: {
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCTICall(Interpreter::cti_op_resolve_func);
            emitPutVirtualRegister(currentInstruction[2].u.operand, X86::edx);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_resolve_func);
        }
        case op_sub: {
            compileBinaryArithOp(op_sub, currentInstruction[1].u.operand, currentInstruction[2].u.operand, currentInstruction[3].u.operand, OperandTypes::fromInt(currentInstruction[4].u.operand));
            NEXT_OPCODE(op_sub);
        }
        case op_put_by_val: {
            emitGetVirtualRegisters(currentInstruction[1].u.operand, X86::eax, currentInstruction[2].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNum(X86::edx);
            emitFastArithImmToInt(X86::edx);
            emitJumpSlowCaseIfNotJSCell(X86::eax);
            addSlowCase(jnePtr(Address(X86::eax), ImmPtr(m_interpreter->m_jsArrayVptr)));

            // This is an array; get the m_storage pointer into ecx, then check if the index is below the fast cutoff
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSArray, m_storage)), X86::ecx);
            Jump inFastVector = jb32(X86::edx, Address(X86::eax, FIELD_OFFSET(JSArray, m_fastAccessCutoff)));
            // No; oh well, check if the access if within the vector - if so, we may still be okay.
            addSlowCase(jae32(X86::edx, Address(X86::ecx, FIELD_OFFSET(ArrayStorage, m_vectorLength))));

            // This is a write to the slow part of the vector; first, we have to check if this would be the first write to this location.
            // FIXME: should be able to handle initial write to array; increment the the number of items in the array, and potentially update fast access cutoff. 
            addSlowCase(jzPtr(BaseIndex(X86::ecx, X86::edx, ScalePtr, FIELD_OFFSET(ArrayStorage, m_vector[0]))));

            // All good - put the value into the array.
            inFastVector.link(this);
            emitGetVirtualRegister(currentInstruction[3].u.operand, X86::eax);
            storePtr(X86::eax, BaseIndex(X86::ecx, X86::edx, ScalePtr, FIELD_OFFSET(ArrayStorage, m_vector[0])));
            NEXT_OPCODE(op_put_by_val);
        }
        CTI_COMPILE_BINARY_OP(op_lesseq)
        case op_loop_if_true: {
            emitSlowScriptCheck();

            unsigned target = currentInstruction[2].u.operand;
            emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);

            Jump isZero = je32(X86::eax, Imm32(asInteger(JSImmediate::zeroImmediate())));
            addJump(jnz32(X86::eax, Imm32(JSImmediate::TagBitTypeInteger)), target + 2);

            addJump(je32(X86::eax, Imm32(asInteger(JSImmediate::trueImmediate()))), target + 2);
            addSlowCase(jne32(X86::eax, Imm32(asInteger(JSImmediate::falseImmediate()))));

            isZero.link(this);
            NEXT_OPCODE(op_loop_if_true);
        };
        case op_resolve_base: {
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCTICall(Interpreter::cti_op_resolve_base);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_resolve_base);
        }
        case op_negate: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx);
            emitCTICall(Interpreter::cti_op_negate);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_negate);
        }
        case op_resolve_skip: {
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitPutCTIArgConstant(currentInstruction[3].u.operand + m_codeBlock->needsFullScopeChain(), 4);
            emitCTICall(Interpreter::cti_op_resolve_skip);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_resolve_skip);
        }
        case op_resolve_global: {
            // Fast case
            void* globalObject = currentInstruction[2].u.jsCell;
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));
            
            unsigned currentIndex = globalResolveInfoIndex++;
            void* structureAddress = &(m_codeBlock->globalResolveInfo(currentIndex).structure);
            void* offsetAddr = &(m_codeBlock->globalResolveInfo(currentIndex).offset);

            // Check Structure of global object
            move(ImmPtr(globalObject), X86::eax);
            loadPtr(structureAddress, X86::edx);
            Jump noMatch = jnePtr(X86::edx, Address(X86::eax, FIELD_OFFSET(JSCell, m_structure))); // Structures don't match

            // Load cached property
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSGlobalObject, m_propertyStorage)), X86::eax);
            load32(offsetAddr, X86::edx);
            loadPtr(BaseIndex(X86::eax, X86::edx, ScalePtr), X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            Jump end = jump();

            // Slow case
            noMatch.link(this);
            emitPutCTIArgConstant(globalObject, 0);
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitPutCTIArgConstant(currentIndex, 8);
            emitCTICall(Interpreter::cti_op_resolve_global);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            end.link(this);
            NEXT_OPCODE(op_resolve_global);
        }
        CTI_COMPILE_BINARY_OP(op_div)
        case op_pre_dec: {
            int srcDst = currentInstruction[1].u.operand;
            emitGetVirtualRegister(srcDst, X86::eax);
            emitJumpSlowCaseIfNotImmNum(X86::eax);
            addSlowCase(joSub32(Imm32(getDeTaggedConstantImmediate(JSImmediate::oneImmediate())), X86::eax));
            emitPutVirtualRegister(srcDst);
            NEXT_OPCODE(op_pre_dec);
        }
        case op_jnless: {
            unsigned target = currentInstruction[3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(currentInstruction[2].u.operand);
            if (src2imm) {
                emitGetVirtualRegister(currentInstruction[1].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::edx);
                addJump(jge32(X86::edx, Imm32(asInteger(src2imm))), target + 3);
            } else {
                emitGetVirtualRegisters(currentInstruction[1].u.operand, X86::eax, currentInstruction[2].u.operand, X86::edx);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::edx);
                addJump(jge32(X86::eax, X86::edx), target + 3);
            }
            NEXT_OPCODE(op_jnless);
        }
        case op_not: {
            emitGetVirtualRegister(currentInstruction[2].u.operand, X86::eax);
            xor32(Imm32(JSImmediate::FullTagTypeBool), X86::eax);
            addSlowCase(jnz32(X86::eax, Imm32(JSImmediate::FullTagTypeMask)));
            xor32(Imm32(JSImmediate::FullTagTypeBool | JSImmediate::ExtendedPayloadBitBoolValue), X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_not);
        }
        case op_jfalse: {
            unsigned target = currentInstruction[2].u.operand;
            emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);

            addJump(je32(X86::eax, Imm32(asInteger(JSImmediate::zeroImmediate()))), target + 2);
            Jump isNonZero = jnz32(X86::eax, Imm32(JSImmediate::TagBitTypeInteger));

            addJump(je32(X86::eax, Imm32(asInteger(JSImmediate::falseImmediate()))), target + 2);
            addSlowCase(jne32(X86::eax, Imm32(asInteger(JSImmediate::trueImmediate()))));

            isNonZero.link(this);
            NEXT_OPCODE(op_jfalse);
        };
        case op_jeq_null: {
            unsigned src = currentInstruction[1].u.operand;
            unsigned target = currentInstruction[2].u.operand;

            emitGetVirtualRegister(src, X86::eax);
            Jump isImmediate = emitJumpIfNotJSCell(X86::eax);

            // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
            addJump(jnz32(Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined)), target + 2);
            Jump wasNotImmediate = jump();

            // Now handle the immediate cases - undefined & null
            isImmediate.link(this);
            and32(Imm32(~JSImmediate::ExtendedTagBitUndefined), X86::eax);
            addJump(je32(X86::eax, Imm32(asInteger(jsNull()))), target + 2);            

            wasNotImmediate.link(this);
            NEXT_OPCODE(op_jeq_null);
        };
        case op_jneq_null: {
            unsigned src = currentInstruction[1].u.operand;
            unsigned target = currentInstruction[2].u.operand;

            emitGetVirtualRegister(src, X86::eax);
            Jump isImmediate = emitJumpIfNotJSCell(X86::eax);

            // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
            addJump(jz32(Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined)), target + 2);
            Jump wasNotImmediate = jump();

            // Now handle the immediate cases - undefined & null
            isImmediate.link(this);
            and32(Imm32(~JSImmediate::ExtendedTagBitUndefined), X86::eax);
            addJump(jne32(X86::eax, Imm32(asInteger(jsNull()))), target + 2);            

            wasNotImmediate.link(this);
            NEXT_OPCODE(op_jneq_null);
        }
        case op_post_inc: {
            int srcDst = currentInstruction[2].u.operand;
            emitGetVirtualRegister(srcDst, X86::eax);
            move(X86::eax, X86::edx);
            emitJumpSlowCaseIfNotImmNum(X86::eax);
            addSlowCase(joAdd32(Imm32(getDeTaggedConstantImmediate(JSImmediate::oneImmediate())), X86::edx));
            emitPutVirtualRegister(srcDst, X86::edx);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_post_inc);
        }
        case op_unexpected_load: {
            JSValue* v = m_codeBlock->unexpectedConstant(currentInstruction[2].u.operand);
            move(ImmPtr(v), X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_unexpected_load);
        }
        case op_jsr: {
            int retAddrDst = currentInstruction[1].u.operand;
            int target = currentInstruction[2].u.operand;
            __ movl_i32m(0, sizeof(Register) * retAddrDst, X86::edi);
            JmpDst addrPosition = __ label();
            addJump(__ jmp(), target + 2);
            JmpDst sretTarget = __ label();
            m_jsrSites.append(JSRInfo(addrPosition, sretTarget));
            NEXT_OPCODE(op_jsr);
        }
        case op_sret: {
            __ jmp_m(sizeof(Register) * currentInstruction[1].u.operand, X86::edi);
            NEXT_OPCODE(op_sret);
        }
        case op_eq: {
            emitGetVirtualRegisters(currentInstruction[2].u.operand, X86::eax, currentInstruction[3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, X86::ecx);
            sete32(X86::edx, X86::eax);
            emitTagAsBoolImmediate(X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_eq);
        }
        case op_lshift: {
            emitGetVirtualRegisters(currentInstruction[2].u.operand, X86::eax, currentInstruction[3].u.operand, X86::ecx);
            emitJumpSlowCaseIfNotImmNum(X86::eax);
            emitJumpSlowCaseIfNotImmNum(X86::ecx);
            emitFastArithImmToInt(X86::eax);
            emitFastArithImmToInt(X86::ecx);
            __ shll_CLr(X86::eax);
            emitFastArithIntToImmOrSlowCase(X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_lshift);
        }
        case op_bitand: {
            unsigned src1 = currentInstruction[2].u.operand;
            unsigned src2 = currentInstruction[3].u.operand;
            unsigned dst = currentInstruction[1].u.operand;
            if (JSValue* value = getConstantImmediateNumericArg(src1)) {
                emitGetVirtualRegister(src2, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                and32(Imm32(asInteger(value)), X86::eax); // FIXME: make it more obvious this is relying on the format of JSImmediate
                emitPutVirtualRegister(dst);
            } else if (JSValue* value = getConstantImmediateNumericArg(src2)) {
                emitGetVirtualRegister(src1, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                and32(Imm32(asInteger(value)), X86::eax);
                emitPutVirtualRegister(dst);
            } else {
                emitGetVirtualRegisters(src1, X86::eax, src2, X86::edx);
                and32(X86::edx, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                emitPutVirtualRegister(dst);
            }
            NEXT_OPCODE(op_bitand);
        }
        case op_rshift: {
            unsigned src1 = currentInstruction[2].u.operand;
            unsigned src2 = currentInstruction[3].u.operand;
            if (JSValue* value = getConstantImmediateNumericArg(src2)) {
                emitGetVirtualRegister(src1, X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                // Mask with 0x1f as per ecma-262 11.7.2 step 7.
                rshift32(Imm32(JSImmediate::getTruncatedUInt32(value) & 0x1f), X86::eax);
            } else {
                emitGetVirtualRegisters(src1, X86::eax, src2, X86::ecx);
                emitJumpSlowCaseIfNotImmNum(X86::eax);
                emitJumpSlowCaseIfNotImmNum(X86::ecx);
                emitFastArithImmToInt(X86::ecx);
                __ sarl_CLr(X86::eax);
            }
            emitFastArithPotentiallyReTagImmediate(X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_rshift);
        }
        case op_bitnot: {
            emitGetVirtualRegister(currentInstruction[2].u.operand, X86::eax);
            emitJumpSlowCaseIfNotImmNum(X86::eax);
            xor32(Imm32(~JSImmediate::TagBitTypeInteger), X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitnot);
        }
        case op_resolve_with_base: {
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitCTICall(Interpreter::cti_op_resolve_with_base);
            emitPutVirtualRegister(currentInstruction[2].u.operand, X86::edx);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_resolve_with_base);
        }
        case op_new_func_exp: {
            FuncExprNode* func = m_codeBlock->functionExpression(currentInstruction[2].u.operand);
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(func), 0);
            emitCTICall(Interpreter::cti_op_new_func_exp);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_new_func_exp);
        }
        case op_mod: {
            emitGetVirtualRegisters(currentInstruction[2].u.operand, X86::eax, currentInstruction[3].u.operand, X86::ecx);
            emitJumpSlowCaseIfNotImmNum(X86::eax);
            emitJumpSlowCaseIfNotImmNum(X86::ecx);
            emitFastArithDeTagImmediate(X86::eax);
            addSlowCase(emitFastArithDeTagImmediateJumpIfZero(X86::ecx));
            __ cdq();
            __ idivl_r(X86::ecx);
            emitFastArithReTagImmediate(X86::edx);
            move(X86::edx, X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_mod);
        }
        case op_jtrue: {
            unsigned target = currentInstruction[2].u.operand;
            emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);

            Jump isZero = je32(X86::eax, Imm32(asInteger(JSImmediate::zeroImmediate())));
            addJump(jnz32(X86::eax, Imm32(JSImmediate::TagBitTypeInteger)), target + 2);

            addJump(je32(X86::eax, Imm32(asInteger(JSImmediate::trueImmediate()))), target + 2);
            addSlowCase(jne32(X86::eax, Imm32(asInteger(JSImmediate::falseImmediate()))));

            isZero.link(this);
            NEXT_OPCODE(op_jtrue);
        }
        CTI_COMPILE_BINARY_OP(op_less)
        case op_neq: {
            emitGetVirtualRegisters(currentInstruction[2].u.operand, X86::eax, currentInstruction[3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, X86::ecx);
            setne32(X86::edx, X86::eax);
            emitTagAsBoolImmediate(X86::eax);

            emitPutVirtualRegister(currentInstruction[1].u.operand);

            NEXT_OPCODE(op_neq);
        }
        case op_post_dec: {
            int srcDst = currentInstruction[2].u.operand;
            emitGetVirtualRegister(srcDst, X86::eax);
            move(X86::eax, X86::edx);
            emitJumpSlowCaseIfNotImmNum(X86::eax);
            addSlowCase(joSub32(Imm32(getDeTaggedConstantImmediate(JSImmediate::oneImmediate())), X86::edx));
            emitPutVirtualRegister(srcDst, X86::edx);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_post_dec);
        }
        CTI_COMPILE_BINARY_OP(op_urshift)
        case op_bitxor: {
            emitGetVirtualRegisters(currentInstruction[2].u.operand, X86::eax, currentInstruction[3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, X86::ecx);
            xor32(X86::edx, X86::eax);
            emitFastArithReTagImmediate(X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitxor);
        }
        case op_new_regexp: {
            RegExp* regExp = m_codeBlock->regexp(currentInstruction[2].u.operand);
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(regExp), 0);
            emitCTICall(Interpreter::cti_op_new_regexp);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_new_regexp);
        }
        case op_bitor: {
            emitGetVirtualRegisters(currentInstruction[2].u.operand, X86::eax, currentInstruction[3].u.operand, X86::edx);
            emitJumpSlowCaseIfNotImmNums(X86::eax, X86::edx, X86::ecx);
            or32(X86::edx, X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitor);
        }
        case op_throw: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[1].u.operand, 0, X86::ecx);
            emitCTICall(Interpreter::cti_op_throw);
            __ addl_i8r(0x20, X86::esp);
            __ popl_r(X86::ebx);
            __ popl_r(X86::edi);
            __ popl_r(X86::esi);
            __ ret();
            NEXT_OPCODE(op_throw);
        }
        case op_get_pnames: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx);
            emitCTICall(Interpreter::cti_op_get_pnames);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_get_pnames);
        }
        case op_next_pname: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx);
            unsigned target = currentInstruction[3].u.operand;
            emitCTICall(Interpreter::cti_op_next_pname);
            Jump endOfIter = jzPtr(X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            addJump(jump(), target + 3);
            endOfIter.link(this);
            NEXT_OPCODE(op_next_pname);
        }
        case op_push_scope: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[1].u.operand, 0, X86::ecx);
            emitCTICall(Interpreter::cti_op_push_scope);
            NEXT_OPCODE(op_push_scope);
        }
        case op_pop_scope: {
            emitCTICall(Interpreter::cti_op_pop_scope);
            NEXT_OPCODE(op_pop_scope);
        }
        CTI_COMPILE_UNARY_OP(op_typeof)
        CTI_COMPILE_UNARY_OP(op_is_undefined)
        CTI_COMPILE_UNARY_OP(op_is_boolean)
        CTI_COMPILE_UNARY_OP(op_is_number)
        CTI_COMPILE_UNARY_OP(op_is_string)
        CTI_COMPILE_UNARY_OP(op_is_object)
        CTI_COMPILE_UNARY_OP(op_is_function)
        case op_stricteq: {
            compileOpStrictEq(currentInstruction, OpStrictEq);
            NEXT_OPCODE(op_stricteq);
        }
        case op_nstricteq: {
            compileOpStrictEq(currentInstruction, OpNStrictEq);
            NEXT_OPCODE(op_nstricteq);
        }
        case op_to_jsnumber: {
            int srcVReg = currentInstruction[2].u.operand;
            emitGetVirtualRegister(srcVReg, X86::eax);
            
            Jump wasImmediate = jnz32(X86::eax, Imm32(JSImmediate::TagBitTypeInteger));

            emitJumpSlowCaseIfNotJSCell(X86::eax, srcVReg);
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
            addSlowCase(jne32(Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo.m_type)), Imm32(NumberType)));
            
            wasImmediate.link(this);

            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_to_jsnumber);
        }
        CTI_COMPILE_BINARY_OP(op_in)
        case op_push_new_scope: {
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 0);
            emitPutCTIArgFromVirtualRegister(currentInstruction[3].u.operand, 4, X86::ecx);
            emitCTICall(Interpreter::cti_op_push_new_scope);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_push_new_scope);
        }
        case op_catch: {
            emitGetCTIParam(CTI_ARGS_callFrame, X86::edi); // edi := r
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_catch);
        }
        case op_jmp_scopes: {
            unsigned count = currentInstruction[1].u.operand;
            emitPutCTIArgConstant(count, 0);
            emitCTICall(Interpreter::cti_op_jmp_scopes);
            unsigned target = currentInstruction[2].u.operand;
            addJump(jump(), target + 2);
            NEXT_OPCODE(op_jmp_scopes);
        }
        case op_put_by_index: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[1].u.operand, 0, X86::ecx);
            emitPutCTIArgConstant(currentInstruction[2].u.operand, 4);
            emitPutCTIArgFromVirtualRegister(currentInstruction[3].u.operand, 8, X86::ecx);
            emitCTICall(Interpreter::cti_op_put_by_index);
            NEXT_OPCODE(op_put_by_index);
        }
        case op_switch_imm: {
            unsigned tableIndex = currentInstruction[1].u.operand;
            unsigned defaultOffset = currentInstruction[2].u.operand;
            unsigned scrutinee = currentInstruction[3].u.operand;

            // create jump table for switch destinations, track this switch statement.
            SimpleJumpTable* jumpTable = &m_codeBlock->immediateSwitchJumpTable(tableIndex);
            m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset, SwitchRecord::Immediate));
            jumpTable->ctiOffsets.grow(jumpTable->branchOffsets.size());

            emitPutCTIArgFromVirtualRegister(scrutinee, 0, X86::ecx);
            emitPutCTIArgConstant(tableIndex, 4);
            emitCTICall(Interpreter::cti_op_switch_imm);
            jump(X86::eax);
            NEXT_OPCODE(op_switch_imm);
        }
        case op_switch_char: {
            unsigned tableIndex = currentInstruction[1].u.operand;
            unsigned defaultOffset = currentInstruction[2].u.operand;
            unsigned scrutinee = currentInstruction[3].u.operand;

            // create jump table for switch destinations, track this switch statement.
            SimpleJumpTable* jumpTable = &m_codeBlock->characterSwitchJumpTable(tableIndex);
            m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset, SwitchRecord::Character));
            jumpTable->ctiOffsets.grow(jumpTable->branchOffsets.size());

            emitPutCTIArgFromVirtualRegister(scrutinee, 0, X86::ecx);
            emitPutCTIArgConstant(tableIndex, 4);
            emitCTICall(Interpreter::cti_op_switch_char);
            jump(X86::eax);
            NEXT_OPCODE(op_switch_char);
        }
        case op_switch_string: {
            unsigned tableIndex = currentInstruction[1].u.operand;
            unsigned defaultOffset = currentInstruction[2].u.operand;
            unsigned scrutinee = currentInstruction[3].u.operand;

            // create jump table for switch destinations, track this switch statement.
            StringJumpTable* jumpTable = &m_codeBlock->stringSwitchJumpTable(tableIndex);
            m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset));

            emitPutCTIArgFromVirtualRegister(scrutinee, 0, X86::ecx);
            emitPutCTIArgConstant(tableIndex, 4);
            emitCTICall(Interpreter::cti_op_switch_string);
            jump(X86::eax);
            NEXT_OPCODE(op_switch_string);
        }
        case op_del_by_val: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx);
            emitPutCTIArgFromVirtualRegister(currentInstruction[3].u.operand, 4, X86::ecx);
            emitCTICall(Interpreter::cti_op_del_by_val);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_del_by_val);
        }
        case op_put_getter: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[1].u.operand, 0, X86::ecx);
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitPutCTIArgFromVirtualRegister(currentInstruction[3].u.operand, 8, X86::ecx);
            emitCTICall(Interpreter::cti_op_put_getter);
            NEXT_OPCODE(op_put_getter);
        }
        case op_put_setter: {
            emitPutCTIArgFromVirtualRegister(currentInstruction[1].u.operand, 0, X86::ecx);
            Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));
            emitPutCTIArgConstant(reinterpret_cast<unsigned>(ident), 4);
            emitPutCTIArgFromVirtualRegister(currentInstruction[3].u.operand, 8, X86::ecx);
            emitCTICall(Interpreter::cti_op_put_setter);
            NEXT_OPCODE(op_put_setter);
        }
        case op_new_error: {
            JSValue* message = m_codeBlock->unexpectedConstant(currentInstruction[3].u.operand);
            emitPutCTIArgConstant(currentInstruction[2].u.operand, 0);
            emitPutCTIArgConstant(asInteger(message), 4);
            emitPutCTIArgConstant(m_codeBlock->lineNumberForBytecodeOffset(m_bytecodeIndex), 8);
            emitCTICall(Interpreter::cti_op_new_error);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_new_error);
        }
        case op_debug: {
            emitPutCTIArgConstant(currentInstruction[1].u.operand, 0);
            emitPutCTIArgConstant(currentInstruction[2].u.operand, 4);
            emitPutCTIArgConstant(currentInstruction[3].u.operand, 8);
            emitCTICall(Interpreter::cti_op_debug);
            NEXT_OPCODE(op_debug);
        }
        case op_eq_null: {
            unsigned dst = currentInstruction[1].u.operand;
            unsigned src1 = currentInstruction[2].u.operand;

            emitGetVirtualRegister(src1, X86::eax);
            Jump isImmediate = emitJumpIfNotJSCell(X86::eax);

            loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
            setnz32(Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined), X86::eax);

            Jump wasNotImmediate = jump();

            isImmediate.link(this);

            and32(Imm32(~JSImmediate::ExtendedTagBitUndefined), X86::eax);
            sete32(Imm32(JSImmediate::FullTagTypeNull), X86::eax);

            wasNotImmediate.link(this);

            emitTagAsBoolImmediate(X86::eax);
            emitPutVirtualRegister(dst);

            NEXT_OPCODE(op_eq_null);
        }
        case op_neq_null: {
            unsigned dst = currentInstruction[1].u.operand;
            unsigned src1 = currentInstruction[2].u.operand;

            emitGetVirtualRegister(src1, X86::eax);
            Jump isImmediate = emitJumpIfNotJSCell(X86::eax);

            loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
            setz32(Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined), X86::eax);

            Jump wasNotImmediate = jump();

            isImmediate.link(this);

            and32(Imm32(~JSImmediate::ExtendedTagBitUndefined), X86::eax);
            setne32(Imm32(JSImmediate::FullTagTypeNull), X86::eax);

            wasNotImmediate.link(this);

            emitTagAsBoolImmediate(X86::eax);
            emitPutVirtualRegister(dst);

            NEXT_OPCODE(op_neq_null);
        }
        case op_enter: {
            // Even though CTI doesn't use them, we initialize our constant
            // registers to zap stale pointers, to avoid unnecessarily prolonging
            // object lifetime and increasing GC pressure.
            size_t count = m_codeBlock->m_numVars + m_codeBlock->numberOfConstantRegisters();
            for (size_t j = 0; j < count; ++j)
                emitInitRegister(j);

            NEXT_OPCODE(op_enter);
        }
        case op_enter_with_activation: {
            // Even though CTI doesn't use them, we initialize our constant
            // registers to zap stale pointers, to avoid unnecessarily prolonging
            // object lifetime and increasing GC pressure.
            size_t count = m_codeBlock->m_numVars + m_codeBlock->numberOfConstantRegisters();
            for (size_t j = 0; j < count; ++j)
                emitInitRegister(j);

            emitCTICall(Interpreter::cti_op_push_activation);
            emitPutVirtualRegister(currentInstruction[1].u.operand);

            NEXT_OPCODE(op_enter_with_activation);
        }
        case op_create_arguments: {
            if (m_codeBlock->m_numParameters == 1)
                emitCTICall(Interpreter::cti_op_create_arguments_no_params);
            else
                emitCTICall(Interpreter::cti_op_create_arguments);
            NEXT_OPCODE(op_create_arguments);
        }
        case op_convert_this: {
            emitGetVirtualRegister(currentInstruction[1].u.operand, X86::eax);

            emitJumpSlowCaseIfNotJSCell(X86::eax);
            loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::edx);
            addSlowCase(jnz32(Address(X86::edx, FIELD_OFFSET(Structure, m_typeInfo.m_flags)), Imm32(NeedsThisConversion)));

            NEXT_OPCODE(op_convert_this);
        }
        case op_profile_will_call: {
            emitGetCTIParam(CTI_ARGS_profilerReference, X86::eax);
            __ cmpl_i32m(0, X86::eax);
            JmpSrc noProfiler = __ je();
            emitPutCTIArgFromVirtualRegister(currentInstruction[1].u.operand, 0, X86::eax);
            emitCTICall(Interpreter::cti_op_profile_will_call);
            __ link(noProfiler, __ label());

            NEXT_OPCODE(op_profile_will_call);
        }
        case op_profile_did_call: {
            emitGetCTIParam(CTI_ARGS_profilerReference, X86::eax);
            __ cmpl_i32m(0, X86::eax);
            JmpSrc noProfiler = __ je();
            emitPutCTIArgFromVirtualRegister(currentInstruction[1].u.operand, 0, X86::eax);
            emitCTICall(Interpreter::cti_op_profile_did_call);
            __ link(noProfiler, __ label());

            NEXT_OPCODE(op_profile_did_call);
        }
        case op_get_array_length:
        case op_get_by_id_chain:
        case op_get_by_id_generic:
        case op_get_by_id_proto:
        case op_get_by_id_proto_list:
        case op_get_by_id_self:
        case op_get_by_id_self_list:
        case op_get_string_length:
        case op_put_by_id_generic:
        case op_put_by_id_replace:
        case op_put_by_id_transition:
            ASSERT_NOT_REACHED();
        }
    }

    ASSERT(propertyAccessInstructionIndex == m_codeBlock->numberOfStructureStubInfos());
    ASSERT(callLinkInfoIndex == m_codeBlock->numberOfCallLinkInfos());

#ifndef NDEBUG
    // reset this, in order to guard it's use with asserts
    m_bytecodeIndex = (unsigned)-1;
#endif
}


void JIT::privateCompileLinkPass()
{
    unsigned jmpTableCount = m_jmpTable.size();
    for (unsigned i = 0; i < jmpTableCount; ++i)
        __ link(m_jmpTable[i].from, m_labels[m_jmpTable[i].to]);
    m_jmpTable.clear();
}

void JIT::privateCompileSlowCases()
{
    Instruction* instructionsBegin = m_codeBlock->instructions().begin();
    unsigned propertyAccessInstructionIndex = 0;
    unsigned callLinkInfoIndex = 0;

    for (Vector<SlowCaseEntry>::iterator iter = m_slowCases.begin(); iter != m_slowCases.end();) {
        // FIXME: enable peephole optimizations for slow cases when applicable
        killLastResultRegister();

        m_bytecodeIndex = iter->to;
#ifndef NDEBUG
        unsigned firstTo = m_bytecodeIndex;
#endif
        Instruction* currentInstruction = instructionsBegin + m_bytecodeIndex;

        switch (OpcodeID opcodeID = m_interpreter->getOpcodeID(currentInstruction->u.opcode)) {
        case op_convert_this: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_convert_this);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_convert_this);
        }
        case op_add: {
            unsigned dst = currentInstruction[1].u.operand;
            unsigned src1 = currentInstruction[2].u.operand;
            unsigned src2 = currentInstruction[3].u.operand;
            if (JSValue* value = getConstantImmediateNumericArg(src1)) {
                Jump notImm = getSlowCase(iter);
                linkSlowCase(iter);
                sub32(Imm32(getDeTaggedConstantImmediate(value)), X86::eax);
                notImm.link(this);
                emitPutCTIArgFromVirtualRegister(src1, 0, X86::ecx);
                emitPutCTIArg(X86::eax, 4);
                emitCTICall(Interpreter::cti_op_add);
                emitPutVirtualRegister(dst);
            } else if (JSValue* value = getConstantImmediateNumericArg(src2)) {
                Jump notImm = getSlowCase(iter);
                linkSlowCase(iter);
                sub32(Imm32(getDeTaggedConstantImmediate(value)), X86::eax);
                notImm.link(this);
                emitPutCTIArg(X86::eax, 0);
                emitPutCTIArgFromVirtualRegister(src2, 4, X86::ecx);
                emitCTICall(Interpreter::cti_op_add);
                emitPutVirtualRegister(dst);
            } else {
                OperandTypes types = OperandTypes::fromInt(currentInstruction[4].u.operand);
                ASSERT(types.first().mightBeNumber() && types.second().mightBeNumber());
                compileBinaryArithOpSlowCase(op_add, iter, dst, src1, src2, types);
            }

            NEXT_OPCODE(op_add);
        }
        case op_construct_verify: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitGetVirtualRegister(currentInstruction[2].u.operand, X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand);

            NEXT_OPCODE(op_construct_verify);
        }
        case op_get_by_val: {
            // The slow case that handles accesses to arrays (below) may jump back up to here. 
            Label beginGetByValSlow(this);

            Jump notImm = getSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitFastArithIntToImmNoCheck(X86::edx);
            notImm.link(this);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitCTICall(Interpreter::cti_op_get_by_val);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_get_by_val));

            // This is slow case that handles accesses to arrays above the fast cut-off.
            // First, check if this is an access to the vector
            linkSlowCase(iter);
            jae32(X86::edx, Address(X86::ecx, FIELD_OFFSET(ArrayStorage, m_vectorLength)), beginGetByValSlow);

            // okay, missed the fast region, but it is still in the vector.  Get the value.
            loadPtr(BaseIndex(X86::ecx, X86::edx, ScalePtr, FIELD_OFFSET(ArrayStorage, m_vector[0])), X86::ecx);
            // Check whether the value loaded is zero; if so we need to return undefined.
            jzPtr(X86::ecx, beginGetByValSlow);
            move(X86::ecx, X86::eax);
            emitPutVirtualRegister(currentInstruction[1].u.operand, X86::eax);

            NEXT_OPCODE(op_get_by_val);
        }
        case op_sub: {
            compileBinaryArithOpSlowCase(op_sub, iter, currentInstruction[1].u.operand, currentInstruction[2].u.operand, currentInstruction[3].u.operand, OperandTypes::fromInt(currentInstruction[4].u.operand));
            NEXT_OPCODE(op_sub);
        }
        case op_rshift: {
            unsigned src2 = currentInstruction[3].u.operand;
            linkSlowCase(iter);
            if (getConstantImmediateNumericArg(src2))
                emitPutCTIArgFromVirtualRegister(src2, 4, X86::ecx);
            else {
                linkSlowCase(iter);
                emitPutCTIArg(X86::ecx, 4);
            }

            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_rshift);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_rshift);
        }
        case op_lshift: {
            Jump notImm1 = getSlowCase(iter);
            Jump notImm2 = getSlowCase(iter);
            linkSlowCase(iter);
            emitGetVirtualRegisters(currentInstruction[2].u.operand, X86::eax, currentInstruction[3].u.operand, X86::ecx);
            notImm1.link(this);
            notImm2.link(this);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::ecx, 4);
            emitCTICall(Interpreter::cti_op_lshift);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_lshift);
        }
        case op_loop_if_less: {
            unsigned target = currentInstruction[3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(currentInstruction[2].u.operand);
            if (src2imm) {
                linkSlowCase(iter);
                emitPutCTIArg(X86::eax, 0);
                emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 4, X86::ecx);
                emitCTICall(Interpreter::cti_op_loop_if_less);
                emitJumpSlowToHot(jnz32(X86::eax), target + 3);
            } else {
                linkSlowCase(iter);
                linkSlowCase(iter);
                emitPutCTIArg(X86::eax, 0);
                emitPutCTIArg(X86::edx, 4);
                emitCTICall(Interpreter::cti_op_loop_if_less);
                emitJumpSlowToHot(jnz32(X86::eax), target + 3);
            }
            NEXT_OPCODE(op_loop_if_less);
        }
        case op_put_by_id: {
            compilePutByIdSlowCase(currentInstruction[1].u.operand, &(m_codeBlock->identifier(currentInstruction[2].u.operand)), currentInstruction[3].u.operand, iter, propertyAccessInstructionIndex++);
            NEXT_OPCODE(op_put_by_id);
        }
        case op_get_by_id: {
            compileGetByIdSlowCase(currentInstruction[1].u.operand, currentInstruction[2].u.operand, &(m_codeBlock->identifier(currentInstruction[3].u.operand)), iter, propertyAccessInstructionIndex++);
            NEXT_OPCODE(op_get_by_id);
        }
        case op_loop_if_lesseq: {
            unsigned target = currentInstruction[3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(currentInstruction[2].u.operand);
            if (src2imm) {
                linkSlowCase(iter);
                emitPutCTIArg(X86::eax, 0);
                emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 4, X86::ecx);
                emitCTICall(Interpreter::cti_op_loop_if_lesseq);
                emitJumpSlowToHot(jnz32(X86::eax), target + 3);
            } else {
                linkSlowCase(iter);
                linkSlowCase(iter);
                emitPutCTIArg(X86::eax, 0);
                emitPutCTIArg(X86::edx, 4);
                emitCTICall(Interpreter::cti_op_loop_if_lesseq);
                emitJumpSlowToHot(jnz32(X86::eax), target + 3);
            }
            NEXT_OPCODE(op_loop_if_lesseq);
        }
        case op_pre_inc: {
            unsigned srcDst = currentInstruction[1].u.operand;
            Jump notImm = getSlowCase(iter);
            linkSlowCase(iter);
            sub32(Imm32(getDeTaggedConstantImmediate(JSImmediate::oneImmediate())), X86::eax);
            notImm.link(this);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_pre_inc);
            emitPutVirtualRegister(srcDst);
            NEXT_OPCODE(op_pre_inc);
        }
        case op_put_by_val: {
            // Normal slow cases - either is not an immediate imm, or is an array.
            Jump notImm = getSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitFastArithIntToImmNoCheck(X86::edx);
            notImm.link(this);
            emitGetVirtualRegister(currentInstruction[3].u.operand, X86::ecx);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitPutCTIArg(X86::ecx, 8);
            emitCTICall(Interpreter::cti_op_put_by_val);
            emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_put_by_val));

            // slow cases for immediate int accesses to arrays
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitGetVirtualRegister(currentInstruction[3].u.operand, X86::ecx);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitPutCTIArg(X86::ecx, 8);
            emitCTICall(Interpreter::cti_op_put_by_val_array);

            NEXT_OPCODE(op_put_by_val);
        }
        case op_loop_if_true: {
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_jtrue);
            unsigned target = currentInstruction[2].u.operand;
            emitJumpSlowToHot(jnz32(X86::eax), target + 2);
            NEXT_OPCODE(op_loop_if_true);
        }
        case op_pre_dec: {
            unsigned srcDst = currentInstruction[1].u.operand;
            Jump notImm = getSlowCase(iter);
            linkSlowCase(iter);
            add32(Imm32(getDeTaggedConstantImmediate(JSImmediate::oneImmediate())), X86::eax);
            notImm.link(this);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_pre_dec);
            emitPutVirtualRegister(srcDst);
            NEXT_OPCODE(op_pre_dec);
        }
        case op_jnless: {
            unsigned target = currentInstruction[3].u.operand;
            JSValue* src2imm = getConstantImmediateNumericArg(currentInstruction[2].u.operand);
            if (src2imm) {
                linkSlowCase(iter);
                emitPutCTIArg(X86::edx, 0);
                emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 4, X86::ecx);
                emitCTICall(Interpreter::cti_op_jless);
                emitJumpSlowToHot(jz32(X86::eax), target + 3);
            } else {
                linkSlowCase(iter);
                linkSlowCase(iter);
                emitPutCTIArg(X86::eax, 0);
                emitPutCTIArg(X86::edx, 4);
                emitCTICall(Interpreter::cti_op_jless);
                emitJumpSlowToHot(jz32(X86::eax), target + 3);
            }
            NEXT_OPCODE(op_jnless);
        }
        case op_not: {
            linkSlowCase(iter);
            xor32(Imm32(JSImmediate::FullTagTypeBool), X86::eax);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_not);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_not);
        }
        case op_jfalse: {
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_jtrue);
            unsigned target = currentInstruction[2].u.operand;
            emitJumpSlowToHot(jz32(X86::eax), target + 2); // inverted!
            NEXT_OPCODE(op_jfalse);
        }
        case op_post_inc: {
            unsigned srcDst = currentInstruction[2].u.operand;
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_post_inc);
            emitPutVirtualRegister(srcDst, X86::edx);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_post_inc);
        }
        case op_bitnot: {
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_bitnot);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitnot);
        }
        case op_bitand: {
            linkSlowCase(iter);
            unsigned src1 = currentInstruction[2].u.operand;
            unsigned src2 = currentInstruction[3].u.operand;
            unsigned dst = currentInstruction[1].u.operand;
            if (getConstantImmediateNumericArg(src1)) {
                emitPutCTIArgFromVirtualRegister(src1, 0, X86::ecx);
                emitPutCTIArg(X86::eax, 4);
                emitCTICall(Interpreter::cti_op_bitand);
                emitPutVirtualRegister(dst);
            } else if (getConstantImmediateNumericArg(src2)) {
                emitPutCTIArg(X86::eax, 0);
                emitPutCTIArgFromVirtualRegister(src2, 4, X86::ecx);
                emitCTICall(Interpreter::cti_op_bitand);
                emitPutVirtualRegister(dst);
            } else {
                emitPutCTIArgFromVirtualRegister(src1, 0, X86::ecx);
                emitPutCTIArg(X86::edx, 4);
                emitCTICall(Interpreter::cti_op_bitand);
                emitPutVirtualRegister(dst);
            }
            NEXT_OPCODE(op_bitand);
        }
        case op_jtrue: {
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_jtrue);
            unsigned target = currentInstruction[2].u.operand;
            emitJumpSlowToHot(jnz32(X86::eax), target + 2);
            NEXT_OPCODE(op_jtrue);
        }
        case op_post_dec: {
            unsigned srcDst = currentInstruction[2].u.operand;
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_post_dec);
            emitPutVirtualRegister(srcDst, X86::edx);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_post_dec);
        }
        case op_bitxor: {
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitCTICall(Interpreter::cti_op_bitxor);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitxor);
        }
        case op_bitor: {
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitCTICall(Interpreter::cti_op_bitor);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_bitor);
        }
        case op_eq: {
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitCTICall(Interpreter::cti_op_eq);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_eq);
        }
        case op_neq: {
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitCTICall(Interpreter::cti_op_neq);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_neq);
        }
        case op_stricteq: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitCTICall(Interpreter::cti_op_stricteq);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_stricteq);
        }
        case op_nstricteq: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::edx, 4);
            emitCTICall(Interpreter::cti_op_nstricteq);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_nstricteq);
        }
        case op_instanceof: {
            linkSlowCase(iter);
            linkSlowCase(iter);
            linkSlowCase(iter);
            emitPutCTIArgFromVirtualRegister(currentInstruction[2].u.operand, 0, X86::ecx);
            emitPutCTIArgFromVirtualRegister(currentInstruction[3].u.operand, 4, X86::ecx);
            emitPutCTIArgFromVirtualRegister(currentInstruction[4].u.operand, 8, X86::ecx);
            emitCTICall(Interpreter::cti_op_instanceof);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_instanceof);
        }
        case op_mod: {
            Jump notImm1 = getSlowCase(iter);
            Jump notImm2 = getSlowCase(iter);
            linkSlowCase(iter);
            emitFastArithReTagImmediate(X86::eax);
            emitFastArithReTagImmediate(X86::ecx);
            notImm1.link(this);
            notImm2.link(this);
            emitPutCTIArg(X86::eax, 0);
            emitPutCTIArg(X86::ecx, 4);
            emitCTICall(Interpreter::cti_op_mod);
            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_mod);
        }
        case op_mul: {
            int dst = currentInstruction[1].u.operand;
            int src1 = currentInstruction[2].u.operand;
            int src2 = currentInstruction[3].u.operand;
            JSValue* src1Value = getConstantImmediateNumericArg(src1);
            JSValue* src2Value = getConstantImmediateNumericArg(src2);
            int32_t value;
            if (src1Value && ((value = JSImmediate::intValue(src1Value)) > 0)) {
                linkSlowCase(iter);
                linkSlowCase(iter);
                // There is an extra slow case for (op1 * -N) or (-N * op2), to check for 0 since this should produce a result of -0.
                emitPutCTIArgFromVirtualRegister(src1, 0, X86::ecx);
                emitPutCTIArgFromVirtualRegister(src2, 4, X86::ecx);
                emitCTICall(Interpreter::cti_op_mul);
                emitPutVirtualRegister(dst);
            } else if (src2Value && ((value = JSImmediate::intValue(src2Value)) > 0)) {
                linkSlowCase(iter);
                linkSlowCase(iter);
                // There is an extra slow case for (op1 * -N) or (-N * op2), to check for 0 since this should produce a result of -0.
                emitPutCTIArgFromVirtualRegister(src1, 0, X86::ecx);
                emitPutCTIArgFromVirtualRegister(src2, 4, X86::ecx);
                emitCTICall(Interpreter::cti_op_mul);
                emitPutVirtualRegister(dst);
            } else
                compileBinaryArithOpSlowCase(op_mul, iter, dst, src1, src2, OperandTypes::fromInt(currentInstruction[4].u.operand));
            NEXT_OPCODE(op_mul);
        }

        case op_call: {
            compileOpCallSlowCase(currentInstruction, iter, callLinkInfoIndex++, opcodeID);
            NEXT_OPCODE(op_call);
        }
        case op_call_eval: {
            compileOpCallSlowCase(currentInstruction, iter, callLinkInfoIndex++, opcodeID);
            NEXT_OPCODE(op_call_eval);
        }
        case op_construct: {
            compileOpCallSlowCase(currentInstruction, iter, callLinkInfoIndex++, opcodeID);
            NEXT_OPCODE(op_construct);
        }
        case op_to_jsnumber: {
            linkSlowCaseIfNotJSCell(iter, currentInstruction[2].u.operand);
            linkSlowCase(iter);

            emitPutCTIArg(X86::eax, 0);
            emitCTICall(Interpreter::cti_op_to_jsnumber);

            emitPutVirtualRegister(currentInstruction[1].u.operand);
            NEXT_OPCODE(op_to_jsnumber);
        }

        default:
            ASSERT_NOT_REACHED();
        }

        ASSERT_WITH_MESSAGE(iter == m_slowCases.end() || firstTo != iter->to,"Not enough jumps linked in slow case codegen.");
        ASSERT_WITH_MESSAGE(firstTo == (iter - 1)->to, "Too many jumps linked in slow case codegen.");

        emitJumpSlowToHot(jump(), 0);
    }

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    ASSERT(propertyAccessInstructionIndex == m_codeBlock->numberOfStructureStubInfos());
#endif
    ASSERT(callLinkInfoIndex == m_codeBlock->numberOfCallLinkInfos());

#ifndef NDEBUG
    // reset this, in order to guard it's use with asserts
    m_bytecodeIndex = (unsigned)-1;
#endif
}

void JIT::privateCompile()
{
#if ENABLE(CODEBLOCK_SAMPLING)
        storePtr(ImmPtr(m_codeBlock), m_interpreter->sampler()->codeBlockSlot());
#endif
#if ENABLE(OPCODE_SAMPLING)
        store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin())), m_interpreter->sampler()->sampleSlot());
#endif

    // Could use a popl_m, but would need to offset the following instruction if so.
    __ popl_r(X86::ecx);
    emitPutToCallFrameHeader(X86::ecx, RegisterFile::ReturnPC);

    Jump slowRegisterFileCheck;
    Label afterRegisterFileCheck;
    if (m_codeBlock->codeType() == FunctionCode) {
        // In the case of a fast linked call, we do not set this up in the caller.
        emitPutImmediateToCallFrameHeader(m_codeBlock, RegisterFile::CodeBlock);

        emitGetCTIParam(CTI_ARGS_registerFile, X86::eax);
        __ leal_mr(m_codeBlock->m_numCalleeRegisters * sizeof(Register), X86::edi, X86::edx);
        slowRegisterFileCheck = jg32(X86::edx, Address(X86::eax, FIELD_OFFSET(RegisterFile, m_end)));
        afterRegisterFileCheck = MacroAssembler::Label(this);
    }

    privateCompileMainPass();
    privateCompileLinkPass();
    privateCompileSlowCases();

    if (m_codeBlock->codeType() == FunctionCode) {
        slowRegisterFileCheck.link(this);
        m_bytecodeIndex = 0; // emitCTICall will add to the map, but doesn't actually need this...
        emitCTICall(Interpreter::cti_register_file_check);
#ifndef NDEBUG
        // reset this, in order to guard it's use with asserts
        m_bytecodeIndex = (unsigned)-1;
#endif
        jump(afterRegisterFileCheck);
    }

    ASSERT(m_jmpTable.isEmpty());

    RefPtr<ExecutablePool> allocator = m_globalData->poolForSize(__ size());
    m_codeBlock->setExecutablePool(allocator.get());
    void* code = __ executableCopy(allocator.get());

    // Translate vPC offsets into addresses in JIT generated code, for switch tables.
    for (unsigned i = 0; i < m_switches.size(); ++i) {
        SwitchRecord record = m_switches[i];
        unsigned bytecodeIndex = record.bytecodeIndex;

        if (record.type != SwitchRecord::String) {
            ASSERT(record.type == SwitchRecord::Immediate || record.type == SwitchRecord::Character); 
            ASSERT(record.jumpTable.simpleJumpTable->branchOffsets.size() == record.jumpTable.simpleJumpTable->ctiOffsets.size());

            record.jumpTable.simpleJumpTable->ctiDefault = __ getRelocatedAddress(code, m_labels[bytecodeIndex + 3 + record.defaultOffset]);

            for (unsigned j = 0; j < record.jumpTable.simpleJumpTable->branchOffsets.size(); ++j) {
                unsigned offset = record.jumpTable.simpleJumpTable->branchOffsets[j];
                record.jumpTable.simpleJumpTable->ctiOffsets[j] = offset ? __ getRelocatedAddress(code, m_labels[bytecodeIndex + 3 + offset]) : record.jumpTable.simpleJumpTable->ctiDefault;
            }
        } else {
            ASSERT(record.type == SwitchRecord::String);

            record.jumpTable.stringJumpTable->ctiDefault = __ getRelocatedAddress(code, m_labels[bytecodeIndex + 3 + record.defaultOffset]);

            StringJumpTable::StringOffsetTable::iterator end = record.jumpTable.stringJumpTable->offsetTable.end();            
            for (StringJumpTable::StringOffsetTable::iterator it = record.jumpTable.stringJumpTable->offsetTable.begin(); it != end; ++it) {
                unsigned offset = it->second.branchOffset;
                it->second.ctiOffset = offset ? __ getRelocatedAddress(code, m_labels[bytecodeIndex + 3 + offset]) : record.jumpTable.stringJumpTable->ctiDefault;
            }
        }
    }

    for (size_t i = 0; i < m_codeBlock->numberOfExceptionHandlers(); ++i) {
        HandlerInfo& handler = m_codeBlock->exceptionHandler(i);
        handler.nativeCode = __ getRelocatedAddress(code, m_labels[handler.target]);
    }

    m_codeBlock->pcVector().reserveCapacity(m_calls.size());
    for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
        if (iter->to)
            X86Assembler::link(code, iter->from, iter->to);
        m_codeBlock->pcVector().append(PC(__ getRelocatedAddress(code, iter->from), iter->bytecodeIndex));
    }

    // Link absolute addresses for jsr
    for (Vector<JSRInfo>::iterator iter = m_jsrSites.begin(); iter != m_jsrSites.end(); ++iter)
        X86Assembler::linkAbsoluteAddress(code, iter->addrPosition, iter->target);

    for (unsigned i = 0; i < m_codeBlock->numberOfStructureStubInfos(); ++i) {
        StructureStubInfo& info = m_codeBlock->structureStubInfo(i);
        info.callReturnLocation = X86Assembler::getRelocatedAddress(code, m_propertyAccessCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = X86Assembler::getRelocatedAddress(code, m_propertyAccessCompilationInfo[i].hotPathBegin);
    }
    for (unsigned i = 0; i < m_codeBlock->numberOfCallLinkInfos(); ++i) {
        CallLinkInfo& info = m_codeBlock->callLinkInfo(i);
        info.callReturnLocation = X86Assembler::getRelocatedAddress(code, m_callStructureStubCompilationInfo[i].callReturnLocation);
        info.hotPathBegin = X86Assembler::getRelocatedAddress(code, m_callStructureStubCompilationInfo[i].hotPathBegin);
        info.hotPathOther = X86Assembler::getRelocatedAddress(code, m_callStructureStubCompilationInfo[i].hotPathOther);
        info.coldPathOther = X86Assembler::getRelocatedAddress(code, m_callStructureStubCompilationInfo[i].coldPathOther);
    }

    m_codeBlock->setJITCode(code);
}

void JIT::privateCompileCTIMachineTrampolines()
{
    // (1) The first function provides fast property access for array length
    
    // Check eax is an array
    JmpSrc array_failureCases1 = emitJumpIfNotJSCell(X86::eax);
    __ cmpl_i32m(reinterpret_cast<unsigned>(m_interpreter->m_jsArrayVptr), X86::eax);
    JmpSrc array_failureCases2 = __ jne();

    // Checks out okay! - get the length from the storage
    __ movl_mr(FIELD_OFFSET(JSArray, m_storage), X86::eax, X86::eax);
    __ movl_mr(FIELD_OFFSET(ArrayStorage, m_length), X86::eax, X86::eax);

    __ cmpl_i32r(JSImmediate::maxImmediateInt, X86::eax);
    JmpSrc array_failureCases3 = __ ja();

    __ addl_rr(X86::eax, X86::eax);
    __ addl_i8r(1, X86::eax);
    
    __ ret();

    // (2) The second function provides fast property access for string length
    
    JmpDst stringLengthBegin = __ align(16);

    // Check eax is a string
    JmpSrc string_failureCases1 = emitJumpIfNotJSCell(X86::eax);
    __ cmpl_i32m(reinterpret_cast<unsigned>(m_interpreter->m_jsStringVptr), X86::eax);
    JmpSrc string_failureCases2 = __ jne();

    // Checks out okay! - get the length from the Ustring.
    __ movl_mr(FIELD_OFFSET(JSString, m_value) + FIELD_OFFSET(UString, m_rep), X86::eax, X86::eax);
    __ movl_mr(FIELD_OFFSET(UString::Rep, len), X86::eax, X86::eax);

    __ cmpl_i32r(JSImmediate::maxImmediateInt, X86::eax);
    JmpSrc string_failureCases3 = __ ja();

    __ addl_rr(X86::eax, X86::eax);
    __ addl_i8r(1, X86::eax);
    
    __ ret();

    // (3) Trampolines for the slow cases of op_call / op_call_eval / op_construct.
    
    JmpDst virtualCallPreLinkBegin = __ align(16);

    // Load the callee CodeBlock* into eax
    __ movl_mr(FIELD_OFFSET(JSFunction, m_body), X86::ecx, X86::eax);
    __ movl_mr(FIELD_OFFSET(FunctionBodyNode, m_code), X86::eax, X86::eax);
    __ testl_rr(X86::eax, X86::eax);
    JmpSrc hasCodeBlock1 = __ jne();
    __ popl_r(X86::ebx);
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc callJSFunction1 = __ call();
    emitGetCTIArg(0, X86::ecx);
    emitGetCTIArg(8, X86::edx);
    __ pushl_r(X86::ebx);
    __ link(hasCodeBlock1, __ label());

    // Check argCount matches callee arity.
    __ cmpl_rm(X86::edx, FIELD_OFFSET(CodeBlock, m_numParameters), X86::eax);
    JmpSrc arityCheckOkay1 = __ je();
    __ popl_r(X86::ebx);
    emitPutCTIArg(X86::ebx, 4);
    emitPutCTIArg(X86::eax, 12);
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc callArityCheck1 = __ call();
    __ movl_rr(X86::edx, X86::edi);
    emitGetCTIArg(0, X86::ecx);
    emitGetCTIArg(8, X86::edx);
    __ pushl_r(X86::ebx);
    __ link(arityCheckOkay1, __ label());

    compileOpCallInitializeCallFrame();

    __ popl_r(X86::ebx);
    emitPutCTIArg(X86::ebx, 4);
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc callDontLazyLinkCall = __ call();
    __ pushl_r(X86::ebx);

    __ jmp_r(X86::eax);

    JmpDst virtualCallLinkBegin = __ align(16);

    // Load the callee CodeBlock* into eax
    __ movl_mr(FIELD_OFFSET(JSFunction, m_body), X86::ecx, X86::eax);
    __ movl_mr(FIELD_OFFSET(FunctionBodyNode, m_code), X86::eax, X86::eax);
    __ testl_rr(X86::eax, X86::eax);
    JmpSrc hasCodeBlock2 = __ jne();
    __ popl_r(X86::ebx);
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc callJSFunction2 = __ call();
    emitGetCTIArg(0, X86::ecx);
    emitGetCTIArg(8, X86::edx);
    __ pushl_r(X86::ebx);
    __ link(hasCodeBlock2, __ label());

    // Check argCount matches callee arity.
    __ cmpl_rm(X86::edx, FIELD_OFFSET(CodeBlock, m_numParameters), X86::eax);
    JmpSrc arityCheckOkay2 = __ je();
    __ popl_r(X86::ebx);
    emitPutCTIArg(X86::ebx, 4);
    emitPutCTIArg(X86::eax, 12);
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc callArityCheck2 = __ call();
    __ movl_rr(X86::edx, X86::edi);
    emitGetCTIArg(0, X86::ecx);
    emitGetCTIArg(8, X86::edx);
    __ pushl_r(X86::ebx);
    __ link(arityCheckOkay2, __ label());

    compileOpCallInitializeCallFrame();

    __ popl_r(X86::ebx);
    emitPutCTIArg(X86::ebx, 4);
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc callLazyLinkCall = __ call();
    __ pushl_r(X86::ebx);

    __ jmp_r(X86::eax);

    JmpDst virtualCallBegin = __ align(16);

    // Load the callee CodeBlock* into eax
    __ movl_mr(FIELD_OFFSET(JSFunction, m_body), X86::ecx, X86::eax);
    __ movl_mr(FIELD_OFFSET(FunctionBodyNode, m_code), X86::eax, X86::eax);
    __ testl_rr(X86::eax, X86::eax);
    JmpSrc hasCodeBlock3 = __ jne();
    __ popl_r(X86::ebx);
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc callJSFunction3 = __ call();
    emitGetCTIArg(0, X86::ecx);
    emitGetCTIArg(8, X86::edx);
    __ pushl_r(X86::ebx);
    __ link(hasCodeBlock3, __ label());

    // Check argCount matches callee arity.
    __ cmpl_rm(X86::edx, FIELD_OFFSET(CodeBlock, m_numParameters), X86::eax);
    JmpSrc arityCheckOkay3 = __ je();
    __ popl_r(X86::ebx);
    emitPutCTIArg(X86::ebx, 4);
    emitPutCTIArg(X86::eax, 12);
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc callArityCheck3 = __ call();
    __ movl_rr(X86::edx, X86::edi);
    emitGetCTIArg(0, X86::ecx);
    emitGetCTIArg(8, X86::edx);
    __ pushl_r(X86::ebx);
    __ link(arityCheckOkay3, __ label());

    compileOpCallInitializeCallFrame();

    // load ctiCode from the new codeBlock.
    __ movl_mr(FIELD_OFFSET(CodeBlock, m_jitCode), X86::eax, X86::eax);

    __ jmp_r(X86::eax);

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.
    m_interpreter->m_executablePool = m_globalData->poolForSize(__ size());
    void* code = __ executableCopy(m_interpreter->m_executablePool.get());

    X86Assembler::link(code, array_failureCases1, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_array_fail));
    X86Assembler::link(code, array_failureCases2, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_array_fail));
    X86Assembler::link(code, array_failureCases3, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_array_fail));
    X86Assembler::link(code, string_failureCases1, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_string_fail));
    X86Assembler::link(code, string_failureCases2, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_string_fail));
    X86Assembler::link(code, string_failureCases3, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_string_fail));
    X86Assembler::link(code, callArityCheck1, reinterpret_cast<void*>(Interpreter::cti_op_call_arityCheck));
    X86Assembler::link(code, callArityCheck2, reinterpret_cast<void*>(Interpreter::cti_op_call_arityCheck));
    X86Assembler::link(code, callArityCheck3, reinterpret_cast<void*>(Interpreter::cti_op_call_arityCheck));
    X86Assembler::link(code, callJSFunction1, reinterpret_cast<void*>(Interpreter::cti_op_call_JSFunction));
    X86Assembler::link(code, callJSFunction2, reinterpret_cast<void*>(Interpreter::cti_op_call_JSFunction));
    X86Assembler::link(code, callJSFunction3, reinterpret_cast<void*>(Interpreter::cti_op_call_JSFunction));
    X86Assembler::link(code, callDontLazyLinkCall, reinterpret_cast<void*>(Interpreter::cti_vm_dontLazyLinkCall));
    X86Assembler::link(code, callLazyLinkCall, reinterpret_cast<void*>(Interpreter::cti_vm_lazyLinkCall));

    m_interpreter->m_ctiArrayLengthTrampoline = code;
    m_interpreter->m_ctiStringLengthTrampoline = X86Assembler::getRelocatedAddress(code, stringLengthBegin);
    m_interpreter->m_ctiVirtualCallPreLink = X86Assembler::getRelocatedAddress(code, virtualCallPreLinkBegin);
    m_interpreter->m_ctiVirtualCallLink = X86Assembler::getRelocatedAddress(code, virtualCallLinkBegin);
    m_interpreter->m_ctiVirtualCall = X86Assembler::getRelocatedAddress(code, virtualCallBegin);
}

void JIT::emitGetVariableObjectRegister(RegisterID variableObject, int index, RegisterID dst)
{
    loadPtr(Address(variableObject, FIELD_OFFSET(JSVariableObject, d)), dst);
    loadPtr(Address(dst, FIELD_OFFSET(JSVariableObject::JSVariableObjectData, registers)), dst);
    loadPtr(Address(dst, index * sizeof(Register)), dst);
}

void JIT::emitPutVariableObjectRegister(RegisterID src, RegisterID variableObject, int index)
{
    loadPtr(Address(variableObject, FIELD_OFFSET(JSVariableObject, d)), variableObject);
    loadPtr(Address(variableObject, FIELD_OFFSET(JSVariableObject::JSVariableObjectData, registers)), variableObject);
    storePtr(src, Address(variableObject, index * sizeof(Register)));
}

} // namespace JSC

#endif // ENABLE(JIT)
