/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WASMFunctionCompiler_h
#define WASMFunctionCompiler_h

#if ENABLE(WEBASSEMBLY)

#include "CCallHelpers.h"
#include "JITOperations.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"

#define UNUSED 0

namespace JSC {

#if !CPU(X86) && !CPU(X86_64)
static int32_t JIT_OPERATION operationDiv(int32_t left, int32_t right)
{
    return left / right;
}

static int32_t JIT_OPERATION operationMod(int32_t left, int32_t right)
{
    return left % right;
}

static uint32_t JIT_OPERATION operationUnsignedDiv(uint32_t left, uint32_t right)
{
    return left / right;
}

static uint32_t JIT_OPERATION operationUnsignedMod(uint32_t left, uint32_t right)
{
    return left % right;
}
#endif

class WASMFunctionCompiler : private CCallHelpers {
public:
    typedef int Expression;
    typedef int Statement;

    WASMFunctionCompiler(VM& vm, CodeBlock* codeBlock, unsigned stackHeight)
        : CCallHelpers(&vm, codeBlock)
        , m_stackHeight(stackHeight)
    {
    }

    void startFunction(const Vector<WASMType>& arguments, uint32_t numberOfI32LocalVariables, uint32_t numberOfF32LocalVariables, uint32_t numberOfF64LocalVariables)
    {
        emitFunctionPrologue();
        emitPutImmediateToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);

        m_beginLabel = label();

        addPtr(TrustedImm32(-WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_stackHeight) * sizeof(StackSlot)), GPRInfo::callFrameRegister, GPRInfo::regT1);
        m_stackOverflow = branchPtr(Above, AbsoluteAddress(m_vm->addressOfStackLimit()), GPRInfo::regT1);

        move(GPRInfo::regT1, stackPointerRegister);
        checkStackPointerAlignment();

        m_numberOfLocals = arguments.size() + numberOfI32LocalVariables + numberOfF32LocalVariables + numberOfF64LocalVariables;

        unsigned localIndex = 0;
        for (size_t i = 0; i < arguments.size(); ++i) {
            Address address(GPRInfo::callFrameRegister, CallFrame::argumentOffset(i) * sizeof(Register));
            switch (arguments[i]) {
            case WASMType::I32:
                load32(address, GPRInfo::regT0);
                store32(GPRInfo::regT0, localAddress(localIndex++));
                break;
            case WASMType::F32:
                load64(address, GPRInfo::regT0);
                unboxDoubleWithoutAssertions(GPRInfo::regT0, FPRInfo::fpRegT0);
                convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
                storeDouble(FPRInfo::fpRegT0, localAddress(localIndex++));
                break;
            case WASMType::F64:
                load64(address, GPRInfo::regT0);
                unboxDoubleWithoutAssertions(GPRInfo::regT0, FPRInfo::fpRegT0);
                storeDouble(FPRInfo::fpRegT0, localAddress(localIndex++));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
        for (uint32_t i = 0; i < numberOfI32LocalVariables; ++i)
            store32(TrustedImm32(0), localAddress(localIndex++));
        for (uint32_t i = 0; i < numberOfF32LocalVariables; ++i)
            store32(TrustedImm32(0), localAddress(localIndex++));
        for (uint32_t i = 0; i < numberOfF64LocalVariables; ++i)
            store64(TrustedImm64(0), localAddress(localIndex++));

        m_codeBlock->setNumParameters(1 + arguments.size());
    }

    void endFunction()
    {
        ASSERT(!m_tempStackTop);

        // FIXME: Remove these if the last statement is a return statement.
        move(TrustedImm64(JSValue::encode(jsUndefined())), GPRInfo::returnValueGPR);
        emitFunctionEpilogue();
        ret();

        m_stackOverflow.link(this);
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(-maxFrameExtentForSlowPathCall), stackPointerRegister);
        setupArgumentsWithExecState(TrustedImmPtr(m_codeBlock));
        appendCallWithExceptionCheck(operationThrowStackOverflowError);

        // FIXME: Implement arity check.
        Label arityCheck = label();
        emitFunctionPrologue();
        emitPutImmediateToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);
        jump(m_beginLabel);

        if (!m_divideErrorJumpList.empty()) {
            m_divideErrorJumpList.link(this);

            if (maxFrameExtentForSlowPathCall)
                addPtr(TrustedImm32(-maxFrameExtentForSlowPathCall), stackPointerRegister);
            setupArgumentsExecState();
            appendCallWithExceptionCheck(operationThrowDivideError);
        }

        if (!m_exceptionChecks.empty()) {
            m_exceptionChecks.link(this);

            // lookupExceptionHandler is passed two arguments, the VM and the exec (the CallFrame*).
            move(TrustedImmPtr(vm()), GPRInfo::argumentGPR0);
            move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);

#if CPU(X86)
            // FIXME: should use the call abstraction, but this is currently in the SpeculativeJIT layer!
            poke(GPRInfo::argumentGPR0);
            poke(GPRInfo::argumentGPR1, 1);
#endif
            m_calls.append(std::make_pair(call(), FunctionPtr(lookupExceptionHandlerFromCallerFrame).value()));
            jumpToExceptionHandler();
        }

        LinkBuffer patchBuffer(*m_vm, *this, m_codeBlock, JITCompilationMustSucceed);

        for (auto iterator : m_calls)
            patchBuffer.link(iterator.first, FunctionPtr(iterator.second));

        MacroAssemblerCodePtr withArityCheck = patchBuffer.locationOf(arityCheck);
        CodeRef result = FINALIZE_CODE(patchBuffer, ("Baseline JIT code for WebAssembly"));
        m_codeBlock->setJITCode(adoptRef(new DirectJITCode(result, withArityCheck, JITCode::BaselineJIT)));
        m_codeBlock->capabilityLevel();
    }

    void buildSetLocal(uint32_t localIndex, int, WASMType type)
    {
        switch (type) {
        case WASMType::I32:
            load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT0);
            m_tempStackTop--;
            store32(GPRInfo::regT0, localAddress(localIndex));
            break;
        case WASMType::F64:
            loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT0);
            m_tempStackTop--;
            storeDouble(FPRInfo::fpRegT0, localAddress(localIndex));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    void buildReturn(int, WASMExpressionType returnType)
    {
        switch (returnType) {
        case WASMExpressionType::I32:
            load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::returnValueGPR);
#if USE(JSVALUE64)
            or64(GPRInfo::tagTypeNumberRegister, GPRInfo::returnValueGPR);
#else
            move(TrustedImm32(JSValue::Int32Tag), GPRInfo::returnValueGPR2);
#endif
            m_tempStackTop--;
            break;
        case WASMExpressionType::F64:
            loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT0);
#if USE(JSVALUE64)
            boxDouble(FPRInfo::fpRegT0, GPRInfo::returnValueGPR);
#else
            boxDouble(FPRInfo::fpRegT0, GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
#endif
            m_tempStackTop--;
            break;
        case WASMExpressionType::Void:
            move(TrustedImm64(JSValue::encode(jsUndefined())), GPRInfo::returnValueGPR);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        emitFunctionEpilogue();
        ret();
    }

    int buildImmediateI32(uint32_t immediate)
    {
        store32(Imm32(immediate), temporaryAddress(m_tempStackTop++));
        return UNUSED;
    }

    int buildImmediateF64(double immediate)
    {
#if USE(JSVALUE64)
        store64(Imm64(bitwise_cast<int64_t>(immediate)), temporaryAddress(m_tempStackTop++));
#else
        union {
            double doubleValue;
            int32_t int32Values[2];
        } u = { immediate };
        m_tempStackTop++;
        store32(Imm32(u.int32Values[0]), temporaryAddress(m_tempStackTop - 1));
        store32(Imm32(u.int32Values[1]), temporaryAddress(m_tempStackTop - 1).withOffset(4));
#endif
        return UNUSED;
    }

    int buildGetLocal(uint32_t localIndex, WASMType type)
    {
        switch (type) {
        case WASMType::I32:
            load32(localAddress(localIndex), GPRInfo::regT0);
            store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop++));
            break;
        case WASMType::F64:
            loadDouble(localAddress(localIndex), FPRInfo::fpRegT0);
            storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop++));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        return UNUSED;
    }

    int buildBinaryI32(int, int, WASMOpExpressionI32 op)
    {
        load32(temporaryAddress(m_tempStackTop - 2), GPRInfo::regT0);
        load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT1);
        switch (op) {
        case WASMOpExpressionI32::Add:
            add32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::Sub:
            sub32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::SDiv:
        case WASMOpExpressionI32::UDiv:
        case WASMOpExpressionI32::SMod:
        case WASMOpExpressionI32::UMod: {
            m_divideErrorJumpList.append(branchTest32(Zero, GPRInfo::regT1));
            if (op == WASMOpExpressionI32::SDiv || op == WASMOpExpressionI32::SMod) {
                Jump denominatorNotNeg1 = branch32(NotEqual, GPRInfo::regT1, TrustedImm32(-1));
                m_divideErrorJumpList.append(branch32(Equal, GPRInfo::regT0, TrustedImm32(-2147483647-1)));
                denominatorNotNeg1.link(this);
            }
#if CPU(X86) || CPU(X86_64)
            ASSERT(GPRInfo::regT0 == X86Registers::eax);
            move(GPRInfo::regT1, X86Registers::ecx);
            if (op == WASMOpExpressionI32::SDiv || op == WASMOpExpressionI32::SMod) {
                m_assembler.cdq();
                m_assembler.idivl_r(X86Registers::ecx);
            } else {
                ASSERT(op == WASMOpExpressionI32::UDiv || op == WASMOpExpressionI32::UMod);
                xor32(X86Registers::edx, X86Registers::edx);
                m_assembler.divl_r(X86Registers::ecx);
            }
            if (op == WASMOpExpressionI32::SMod || op == WASMOpExpressionI32::UMod)
                move(X86Registers::edx, GPRInfo::regT0);
#else
            // FIXME: We should be able to do an inline div on ARMv7 and ARM64.
            if (maxFrameExtentForSlowPathCall)
                addPtr(TrustedImm32(-maxFrameExtentForSlowPathCall), stackPointerRegister);
            switch (op) {
            case WASMOpExpressionI32::SDiv:
                callOperation(operationDiv, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT0);
                break;
            case WASMOpExpressionI32::UDiv:
                callOperation(operationUnsignedDiv, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT0);
                break;
            case WASMOpExpressionI32::SMod:
                callOperation(operationMod, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT0);
                break;
            case WASMOpExpressionI32::UMod:
                callOperation(operationUnsignedMod, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT0);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            if (maxFrameExtentForSlowPathCall)
                addPtr(TrustedImm32(maxFrameExtentForSlowPathCall), stackPointerRegister);
#endif
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
        m_tempStackTop--;
        store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop - 1));
        return UNUSED;
    }

    int buildRelationalI32(int, int, WASMOpExpressionI32 op)
    {
        load32(temporaryAddress(m_tempStackTop - 2), GPRInfo::regT0);
        load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT1);
        RelationalCondition condition;
        switch (op) {
        case WASMOpExpressionI32::EqualI32:
            condition = Equal;
            break;
        case WASMOpExpressionI32::NotEqualI32:
            condition = NotEqual;
            break;
        case WASMOpExpressionI32::SLessThanI32:
            condition = LessThan;
            break;
        case WASMOpExpressionI32::ULessThanI32:
            condition = Below;
            break;
        case WASMOpExpressionI32::SLessThanOrEqualI32:
            condition = LessThanOrEqual;
            break;
        case WASMOpExpressionI32::ULessThanOrEqualI32:
            condition = BelowOrEqual;
            break;
        case WASMOpExpressionI32::SGreaterThanI32:
            condition = GreaterThan;
            break;
        case WASMOpExpressionI32::UGreaterThanI32:
            condition = Above;
            break;
        case WASMOpExpressionI32::SGreaterThanOrEqualI32:
            condition = GreaterThanOrEqual;
            break;
        case WASMOpExpressionI32::UGreaterThanOrEqualI32:
            condition = AboveOrEqual;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        compare32(condition, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT0);
        m_tempStackTop--;
        store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop - 1));
        return UNUSED;
    }

private:
    union StackSlot {
        int32_t intValue;
        float floatValue;
        double doubleValue;
    };

    Address localAddress(unsigned localIndex) const
    {
        ASSERT(localIndex < m_numberOfLocals);
        return Address(GPRInfo::callFrameRegister, -(localIndex + 1) * sizeof(StackSlot));
    }

    Address temporaryAddress(unsigned temporaryIndex) const
    {
        ASSERT(m_numberOfLocals + temporaryIndex < m_stackHeight);
        return Address(GPRInfo::callFrameRegister, -(m_numberOfLocals + temporaryIndex + 1) * sizeof(StackSlot));
    }

    void appendCall(const FunctionPtr& function)
    {
        m_calls.append(std::make_pair(call(), function.value()));
    }

    void appendCallWithExceptionCheck(const FunctionPtr& function)
    {
        appendCall(function);
        m_exceptionChecks.append(emitExceptionCheck());
    }

    void callOperation(int32_t JIT_OPERATION (*operation)(int32_t, int32_t), GPRReg src1, GPRReg src2, GPRReg dst)
    {
        setupArguments(src1, src2);
        appendCall(operation);
        move(GPRInfo::returnValueGPR, dst);
    }

    void callOperation(uint32_t JIT_OPERATION (*operation)(uint32_t, uint32_t), GPRReg src1, GPRReg src2, GPRReg dst)
    {
        setupArguments(src1, src2);
        appendCall(operation);
        move(GPRInfo::returnValueGPR, dst);
    }

    unsigned m_stackHeight;
    unsigned m_numberOfLocals;
    unsigned m_tempStackTop { 0 };

    Label m_beginLabel;
    Jump m_stackOverflow;
    JumpList m_divideErrorJumpList;
    JumpList m_exceptionChecks;

    Vector<std::pair<Call, void*>> m_calls;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMFunctionCompiler_h
