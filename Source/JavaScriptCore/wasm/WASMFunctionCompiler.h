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

#include "BinarySwitch.h"
#include "CCallHelpers.h"
#include "JIT.h"
#include "JITOperations.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"

#define UNUSED 0

namespace JSC {

static int32_t JIT_OPERATION operationConvertJSValueToInt32(ExecState* exec, EncodedJSValue value)
{
    return JSValue::decode(value).toInt32(exec);
}

static double JIT_OPERATION operationConvertJSValueToDouble(ExecState* exec, EncodedJSValue value)
{
    return JSValue::decode(value).toNumber(exec);
}

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
    typedef int ExpressionList;
    struct JumpTarget {
        Label label;
        JumpList jumpList;
    };
    enum class JumpCondition { Zero, NonZero };

    WASMFunctionCompiler(VM& vm, CodeBlock* codeBlock, JSWASMModule* module, unsigned stackHeight)
        : CCallHelpers(&vm, codeBlock)
        , m_module(module)
        , m_stackHeight(stackHeight)
    {
    }

    void startFunction(const Vector<WASMType>& arguments, uint32_t numberOfI32LocalVariables, uint32_t numberOfF32LocalVariables, uint32_t numberOfF64LocalVariables)
    {
        emitFunctionPrologue();
        emitPutImmediateToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);

        m_beginLabel = label();

        addPtr(TrustedImm32(-WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_stackHeight) * sizeof(StackSlot) - maxFrameExtentForSlowPathCall), GPRInfo::callFrameRegister, GPRInfo::regT1);
        m_stackOverflow = branchPtr(Above, AbsoluteAddress(m_vm->addressOfStackLimit()), GPRInfo::regT1);

        move(GPRInfo::regT1, stackPointerRegister);
        checkStackPointerAlignment();

        m_numberOfLocals = arguments.size() + numberOfI32LocalVariables + numberOfF32LocalVariables + numberOfF64LocalVariables;

        unsigned localIndex = 0;
        for (size_t i = 0; i < arguments.size(); ++i) {
            Address address(GPRInfo::callFrameRegister, CallFrame::argumentOffset(i) * sizeof(Register));
            switch (arguments[i]) {
            case WASMType::I32:
#if USE(JSVALUE64)
                loadValueAndConvertToInt32(address, GPRInfo::regT0);
#else
                loadValueAndConvertToInt32(address, GPRInfo::regT0, GPRInfo::regT1);
#endif
                store32(GPRInfo::regT0, localAddress(localIndex++));
                break;
            case WASMType::F32:
            case WASMType::F64:
#if USE(JSVALUE64)
                loadValueAndConvertToDouble(address, FPRInfo::fpRegT0, GPRInfo::regT0, GPRInfo::regT1);
#else
                loadValueAndConvertToDouble(address, FPRInfo::fpRegT0, GPRInfo::regT0, GPRInfo::regT1, GPRInfo::regT2, FPRInfo::fpRegT1);
#endif
                if (arguments[i] == WASMType::F32)
                    convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
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

        for (const auto& iterator : m_calls)
            patchBuffer.link(iterator.first, FunctionPtr(iterator.second));

        for (size_t i = 0; i < m_callCompilationInfo.size(); ++i) {
            CallCompilationInfo& compilationInfo = m_callCompilationInfo[i];
            CallLinkInfo& info = *compilationInfo.callLinkInfo;
            info.setCallLocations(patchBuffer.locationOfNearCall(compilationInfo.callReturnLocation),
                patchBuffer.locationOf(compilationInfo.hotPathBegin),
                patchBuffer.locationOfNearCall(compilationInfo.hotPathOther));
        }

        MacroAssemblerCodePtr withArityCheck = patchBuffer.locationOf(arityCheck);
        CodeRef result = FINALIZE_CODE(patchBuffer, ("Baseline JIT code for WebAssembly"));
        m_codeBlock->setJITCode(adoptRef(new DirectJITCode(result, withArityCheck, JITCode::BaselineJIT)));
        m_codeBlock->capabilityLevel();
    }

    void buildSetLocal(uint32_t localIndex, int, WASMType type)
    {
        switch (type) {
        case WASMType::I32:
        case WASMType::F32:
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

    void buildSetGlobal(uint32_t globalIndex, int, WASMType type)
    {
        move(TrustedImmPtr(&m_module->globalVariables()[globalIndex]), GPRInfo::regT0);
        switch (type) {
        case WASMType::I32:
        case WASMType::F32:
            load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT1);
            store32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        case WASMType::F64:
            loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT0);
            storeDouble(FPRInfo::fpRegT0, GPRInfo::regT0);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        m_tempStackTop--;
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
        case WASMExpressionType::F32:
        case WASMExpressionType::F64:
            loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT0);
            if (returnType == WASMExpressionType::F32)
                convertFloatToDouble(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
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

    int buildImmediateF32(float immediate)
    {
        store32(Imm32(bitwise_cast<int32_t>(immediate)), temporaryAddress(m_tempStackTop++));
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
        case WASMType::F32:
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

    int buildGetGlobal(uint32_t globalIndex, WASMType type)
    {
        move(TrustedImmPtr(&m_module->globalVariables()[globalIndex]), GPRInfo::regT0);
        switch (type) {
        case WASMType::I32:
        case WASMType::F32:
            load32(GPRInfo::regT0, GPRInfo::regT0);
            store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop++));
            break;
        case WASMType::F64:
            loadDouble(GPRInfo::regT0, FPRInfo::fpRegT0);
            storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop++));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        return UNUSED;
    }

    int buildUnaryI32(int, WASMOpExpressionI32 op)
    {
        load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT0);
        switch (op) {
        case WASMOpExpressionI32::Negate:
            neg32(GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::BitNot:
            xor32(TrustedImm32(-1), GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::CountLeadingZeros:
            countLeadingZeros32(GPRInfo::regT0, GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::LogicalNot: {
            // FIXME: Don't use branches.
            Jump zero = branchTest32(Zero, GPRInfo::regT0);
            move(TrustedImm32(0), GPRInfo::regT0);
            Jump end = jump();
            zero.link(this);
            move(TrustedImm32(1), GPRInfo::regT0);
            end.link(this);
            break;
        }
        case WASMOpExpressionI32::Abs: {
            // FIXME: Don't use branches.
            Jump end = branchTest32(PositiveOrZero, GPRInfo::regT0);
            neg32(GPRInfo::regT0);
            end.link(this);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
        store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop - 1));
        return UNUSED;
    }

    int buildUnaryF32(int, WASMOpExpressionF32 op)
    {
        loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT1);
        switch (op) {
        case WASMOpExpressionF32::Negate:
            convertFloatToDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT1);
            negateDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF32::Abs:
            convertFloatToDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT1);
            absDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF32::Ceil:
            callOperation(ceilf, FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF32::Floor:
            callOperation(floorf, FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF32::Sqrt:
            convertFloatToDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT1);
            sqrtDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop - 1));
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
        case WASMOpExpressionI32::Mul:
            mul32(GPRInfo::regT1, GPRInfo::regT0);
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
#endif
            break;
        }
        case WASMOpExpressionI32::BitOr:
            or32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::BitAnd:
            and32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::BitXor:
            xor32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::LeftShift:
            lshift32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::ArithmeticRightShift:
            rshift32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        case WASMOpExpressionI32::LogicalRightShift:
            urshift32(GPRInfo::regT1, GPRInfo::regT0);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        m_tempStackTop--;
        store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop - 1));
        return UNUSED;
    }

    int buildBinaryF32(int, int, WASMOpExpressionF32 op)
    {
        loadDouble(temporaryAddress(m_tempStackTop - 2), FPRInfo::fpRegT0);
        loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT1);
        convertFloatToDouble(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
        convertFloatToDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT1);
        switch (op) {
        case WASMOpExpressionF32::Add:
            addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF32::Sub:
            subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF32::Mul:
            mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF32::Div:
            divDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
        m_tempStackTop--;
        storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop - 1));
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

    int buildRelationalF32(int, int, WASMOpExpressionI32 op)
    {
        loadDouble(temporaryAddress(m_tempStackTop - 2), FPRInfo::fpRegT0);
        loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT1);
        convertFloatToDouble(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
        convertFloatToDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT1);
        DoubleCondition condition;
        switch (op) {
        case WASMOpExpressionI32::EqualF32:
            condition = DoubleEqual;
            break;
        case WASMOpExpressionI32::NotEqualF32:
            condition = DoubleNotEqual;
            break;
        case WASMOpExpressionI32::LessThanF32:
            condition = DoubleLessThan;
            break;
        case WASMOpExpressionI32::LessThanOrEqualF32:
            condition = DoubleLessThanOrEqual;
            break;
        case WASMOpExpressionI32::GreaterThanF32:
            condition = DoubleGreaterThan;
            break;
        case WASMOpExpressionI32::GreaterThanOrEqualF32:
            condition = DoubleGreaterThanOrEqual;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        m_tempStackTop--;
        Jump trueCase = branchDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1);
        store32(TrustedImm32(0), temporaryAddress(m_tempStackTop - 1));
        Jump end = jump();
        trueCase.link(this);
        store32(TrustedImm32(1), temporaryAddress(m_tempStackTop - 1));
        end.link(this);
        return UNUSED;
    }

    int buildRelationalF64(int, int, WASMOpExpressionI32 op)
    {
        loadDouble(temporaryAddress(m_tempStackTop - 2), FPRInfo::fpRegT0);
        loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT1);
        DoubleCondition condition;
        switch (op) {
        case WASMOpExpressionI32::EqualF64:
            condition = DoubleEqual;
            break;
        case WASMOpExpressionI32::NotEqualF64:
            condition = DoubleNotEqual;
            break;
        case WASMOpExpressionI32::LessThanF64:
            condition = DoubleLessThan;
            break;
        case WASMOpExpressionI32::LessThanOrEqualF64:
            condition = DoubleLessThanOrEqual;
            break;
        case WASMOpExpressionI32::GreaterThanF64:
            condition = DoubleGreaterThan;
            break;
        case WASMOpExpressionI32::GreaterThanOrEqualF64:
            condition = DoubleGreaterThanOrEqual;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        m_tempStackTop--;
        Jump trueCase = branchDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1);
        store32(TrustedImm32(0), temporaryAddress(m_tempStackTop - 1));
        Jump end = jump();
        trueCase.link(this);
        store32(TrustedImm32(1), temporaryAddress(m_tempStackTop - 1));
        end.link(this);
        return UNUSED;
    }

    int buildCallInternal(uint32_t functionIndex, int, const WASMSignature& signature, WASMExpressionType returnType)
    {
        boxArgumentsAndAdjustStackPointer(signature.arguments);

        JSFunction* function = m_module->functions()[functionIndex].get();
        move(TrustedImmPtr(function), GPRInfo::regT0);

        callAndUnboxResult(returnType);
        return UNUSED;
    }

    void appendExpressionList(int&, int) { }

    void linkTarget(JumpTarget& target)
    {
        target.label = label();
        target.jumpList.link(this);
    }

    void jumpToTarget(JumpTarget& target)
    {
        if (target.label.isSet())
            jump(target.label);
        else
            target.jumpList.append(jump());
    }

    void jumpToTargetIf(JumpCondition condition, int, JumpTarget& target)
    {
        load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT0);
        m_tempStackTop--;
        Jump taken = branchTest32((condition == JumpCondition::Zero) ? Zero : NonZero, GPRInfo::regT0);
        if (target.label.isSet())
            taken.linkTo(target.label, this);
        else
            target.jumpList.append(taken);
    }

    void startLoop()
    {
        m_breakTargets.append(JumpTarget());
        m_continueTargets.append(JumpTarget());
    }

    void endLoop()
    {
        m_breakTargets.removeLast();
        m_continueTargets.removeLast();
    }

    void startSwitch()
    {
        m_breakTargets.append(JumpTarget());
    }

    void endSwitch()
    {
        m_breakTargets.removeLast();
    }

    void startLabel()
    {
        m_breakLabelTargets.append(JumpTarget());
        m_continueLabelTargets.append(JumpTarget());

        linkTarget(m_continueLabelTargets.last());
    }

    void endLabel()
    {
        linkTarget(m_breakLabelTargets.last());

        m_breakLabelTargets.removeLast();
        m_continueLabelTargets.removeLast();
    }

    JumpTarget& breakTarget()
    {
        return m_breakTargets.last();
    }

    JumpTarget& continueTarget()
    {
        return m_continueTargets.last();
    }

    JumpTarget& breakLabelTarget(uint32_t labelIndex)
    {
        return m_breakLabelTargets[labelIndex];
    }

    JumpTarget& continueLabelTarget(uint32_t labelIndex)
    {
        return m_continueLabelTargets[labelIndex];
    }

    void buildSwitch(int, const Vector<int64_t>& cases, Vector<JumpTarget>& targets, JumpTarget defaultTarget)
    {
        load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT0);
        m_tempStackTop--;
        BinarySwitch binarySwitch(GPRInfo::regT0, cases, BinarySwitch::Int32);
        while (binarySwitch.advance(*this)) {
            unsigned index = binarySwitch.caseIndex();
            jump(targets[index].label);
        }
        binarySwitch.fallThrough().linkTo(defaultTarget.label, this);
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

    Call emitNakedCall(CodePtr function)
    {
        Call nakedCall = nearCall();
        m_calls.append(std::make_pair(nakedCall, function.executableAddress()));
        return nakedCall;
    }

    void appendCallSetResult(const FunctionPtr& function, GPRReg result)
    {
        appendCall(function);
        move(GPRInfo::returnValueGPR, result);
    }

#if CPU(X86)
    void appendCallSetResult(const FunctionPtr& function, FPRReg result)
    {
        appendCall(function);
        m_assembler.fstpl(0, stackPointerRegister);
        loadDouble(stackPointerRegister, result);
    }
#elif CPU(ARM) && !CPU(ARM_HARDFP)
    void appendCallSetResult(const FunctionPtr& function, FPRReg result)
    {
        appendCall(function);
        m_assembler.vmov(result, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
    }
#else // CPU(X86_64) || (CPU(ARM) && CPU(ARM_HARDFP)) || CPU(ARM64) || CPU(MIPS) || CPU(SH4)
    void appendCallSetResult(const FunctionPtr& function, FPRReg result)
    {
        appendCall(function);
        moveDouble(FPRInfo::returnValueFPR, result);
    }
#endif

#if USE(JSVALUE64)
    void callOperation(Z_JITOperation_EJ operation, GPRReg src, GPRReg dst)
    {
        setupArgumentsWithExecState(src);
        appendCallSetResult(operation, dst);
    }

    void callOperation(D_JITOperation_EJ operation, GPRReg src, FPRReg dst)
    {
        setupArgumentsWithExecState(src);
        appendCallSetResult(operation, dst);
    }
#else
    void callOperation(Z_JITOperation_EJ operation, GPRReg srcTag, GPRReg srcPayload, GPRReg dst)
    {
        setupArgumentsWithExecState(EABI_32BIT_DUMMY_ARG srcPayload, srcTag);
        appendCallSetResult(operation, dst);
    }

    void callOperation(D_JITOperation_EJ operation, GPRReg srcTag, GPRReg srcPayload, FPRReg dst)
    {
        setupArgumentsWithExecState(EABI_32BIT_DUMMY_ARG srcPayload, srcTag);
        appendCallSetResult(operation, dst);
    }
#endif

    void callOperation(float JIT_OPERATION (*operation)(float), FPRegisterID src, FPRegisterID dst)
    {
        setupArguments(src);
        appendCallSetResult(operation, dst);
    }

    void callOperation(int32_t JIT_OPERATION (*operation)(int32_t, int32_t), GPRReg src1, GPRReg src2, GPRReg dst)
    {
        setupArguments(src1, src2);
        appendCallSetResult(operation, dst);
    }

    void callOperation(uint32_t JIT_OPERATION (*operation)(uint32_t, uint32_t), GPRReg src1, GPRReg src2, GPRReg dst)
    {
        setupArguments(src1, src2);
        appendCallSetResult(operation, dst);
    }

    void boxArgumentsAndAdjustStackPointer(const Vector<WASMType>& arguments)
    {
        size_t argumentCount = arguments.size();
        int stackOffset = -WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_numberOfLocals + m_tempStackTop + argumentCount + 1 + JSStack::CallFrameHeaderSize);

        storeTrustedValue(jsUndefined(), Address(GPRInfo::callFrameRegister, (stackOffset + CallFrame::thisArgumentOffset()) * sizeof(Register)));

        for (size_t i = 0; i < argumentCount; ++i) {
            Address address(GPRInfo::callFrameRegister, (stackOffset + CallFrame::argumentOffset(i)) * sizeof(Register));
            switch (arguments[i]) {
            case WASMType::I32:
                load32(temporaryAddress(m_tempStackTop - argumentCount + i), GPRInfo::regT0);
#if USE(JSVALUE64)
                or64(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
                store64(GPRInfo::regT0, address);
#else
                store32(GPRInfo::regT0, address.withOffset(PayloadOffset));
                store32(TrustedImm32(JSValue::Int32Tag), address.withOffset(TagOffset));
#endif
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
        m_tempStackTop -= argumentCount;

        addPtr(TrustedImm32(stackOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), GPRInfo::callFrameRegister, stackPointerRegister);
        store32(TrustedImm32(argumentCount + 1), Address(stackPointerRegister, JSStack::ArgumentCount * static_cast<int>(sizeof(Register)) + PayloadOffset - sizeof(CallerFrameAndPC)));
    }

    void callAndUnboxResult(WASMExpressionType returnType)
    {
        // regT0 holds callee.
#if USE(JSVALUE64)
        store64(GPRInfo::regT0, Address(stackPointerRegister, JSStack::Callee * static_cast<int>(sizeof(Register)) - sizeof(CallerFrameAndPC)));
#else
        store32(regT0, Address(stackPointerRegister, JSStack::Callee * static_cast<int>(sizeof(Register)) + PayloadOffset - sizeof(CallerFrameAndPC)));
        store32(TrustedImm32(CellTag), Address(stackPointerRegister, JSStack::Callee * static_cast<int>(sizeof(Register)) + TagOffset - sizeof(CallerFrameAndPC)));
#endif

        DataLabelPtr addressOfLinkedFunctionCheck;
        Jump slowCase = branchPtrWithPatch(NotEqual, GPRInfo::regT0, addressOfLinkedFunctionCheck, TrustedImmPtr(0));

        CallLinkInfo* info = m_codeBlock->addCallLinkInfo();
        info->setUpCall(CallLinkInfo::Call, CodeOrigin(), GPRInfo::regT0);
        m_callCompilationInfo.append(CallCompilationInfo());
        m_callCompilationInfo.last().hotPathBegin = addressOfLinkedFunctionCheck;
        m_callCompilationInfo.last().callLinkInfo = info;
        m_callCompilationInfo.last().hotPathOther = nearCall();
        Jump end = jump();

        slowCase.link(this);
        move(TrustedImmPtr(info), GPRInfo::regT2);
        m_callCompilationInfo.last().callReturnLocation = emitNakedCall(m_vm->getCTIStub(linkCallThunkGenerator).code());

        end.link(this);
        addPtr(TrustedImm32(-WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_stackHeight) * sizeof(StackSlot) - maxFrameExtentForSlowPathCall), GPRInfo::callFrameRegister, stackPointerRegister);
        checkStackPointerAlignment();

        switch (returnType) {
        case WASMExpressionType::I32:
            store32(GPRInfo::returnValueGPR, temporaryAddress(m_tempStackTop++));
            break;
        case WASMExpressionType::Void:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

#if USE(JSVALUE64)
    void loadValueAndConvertToInt32(Address address, GPRReg dst)
    {
        JSValueRegs tempRegs(dst);
        loadValue(address, tempRegs);
        Jump checkJSInt32 = branchIfInt32(tempRegs);

        callOperation(operationConvertJSValueToInt32, dst, dst);

        checkJSInt32.link(this);
    }

    void loadValueAndConvertToDouble(Address address, FPRReg dst, GPRReg scratch1, GPRReg scratch2)
    {
        JSValueRegs tempRegs(scratch1);
        loadValue(address, tempRegs);
        Jump checkJSInt32 = branchIfInt32(tempRegs);
        Jump checkJSNumber = branchIfNumber(tempRegs, scratch2);
        JumpList end;

        callOperation(operationConvertJSValueToDouble, tempRegs.gpr(), dst);
        end.append(jump());

        checkJSInt32.link(this);
        convertInt32ToDouble(tempRegs.gpr(), dst);
        end.append(jump());

        checkJSNumber.link(this);
        unboxDoubleWithoutAssertions(tempRegs.gpr(), dst);
        end.link(this);
    }
#else
    void loadValueAndConvertToInt32(Address address, GPRReg dst, GPRReg scratch)
    {
        JSValueRegs tempRegs(scratch, dst);
        loadValue(address, tempRegs);
        Jump checkJSInt32 = branchIfInt32(tempRegs);

        callOperation(operationConvertJSValueToInt32, tempRegs.tagGPR(), tempRegs.payloadGPR(), dst);

        checkJSInt32.link(this);
    }

    void loadValueAndConvertToDouble(Address address, FPRReg dst, GPRReg scratch1, GPRReg scratch2, GPRReg scratch3, FPRReg fpScratch)
    {
        JSValueRegs tempRegs(scratch2, scratch1);
        loadValue(address, tempRegs);
        Jump checkJSInt32 = branchIfInt32(tempRegs);
        Jump checkJSNumber = branchIfNumber(tempRegs, scratch3);
        JumpList end;

        callOperation(operationConvertJSValueToDouble, tempRegs.tagGPR(), tempRegs.payloadGPR(), dst);
        end.append(jump());

        checkJSInt32.link(this);
        convertInt32ToDouble(tempRegs.payloadGPR(), dst);
        end.append(jump());

        checkJSNumber.link(this);
        unboxDouble(tempRegs.tagGPR(), tempRegs.payloadGPR(), dst, fpScratch)
        end.link(this);
    }
#endif

    JSWASMModule* m_module;
    unsigned m_stackHeight;
    unsigned m_numberOfLocals;
    unsigned m_tempStackTop { 0 };

    Vector<JumpTarget> m_breakTargets;
    Vector<JumpTarget> m_continueTargets;
    Vector<JumpTarget> m_breakLabelTargets;
    Vector<JumpTarget> m_continueLabelTargets;

    Label m_beginLabel;
    Jump m_stackOverflow;
    JumpList m_divideErrorJumpList;
    JumpList m_exceptionChecks;

    Vector<std::pair<Call, void*>> m_calls;
    Vector<CallCompilationInfo> m_callCompilationInfo;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMFunctionCompiler_h
