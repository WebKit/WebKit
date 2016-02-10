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
#include "JSArrayBuffer.h"
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

#if !USE(JSVALUE64)
static double JIT_OPERATION operationConvertUnsignedInt32ToDouble(uint32_t value)
{
    return static_cast<double>(value);
}
#endif

static size_t sizeOfMemoryType(WASMMemoryType memoryType)
{
    switch (memoryType) {
    case WASMMemoryType::I8:
        return 1;
    case WASMMemoryType::I16:
        return 2;
    case WASMMemoryType::I32:
    case WASMMemoryType::F32:
        return 4;
    case WASMMemoryType::F64:
        return 8;
    default:
        ASSERT_NOT_REACHED();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

class WASMFunctionCompiler : private CCallHelpers {
public:
    typedef int Expression;
    typedef int Statement;
    typedef int ExpressionList;
    struct MemoryAddress {
        MemoryAddress(void*) { }
        MemoryAddress(int, uint32_t offset)
            : offset(offset)
        {
        }
        uint32_t offset;
    };
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
        m_calleeSaveSpace = WTF::roundUpToMultipleOf(sizeof(StackSlot), RegisterSet::webAssemblyCalleeSaveRegisters().numberOfSetRegisters() * sizeof(void*));
        m_codeBlock->setCalleeSaveRegisters(RegisterSet::webAssemblyCalleeSaveRegisters());

        emitFunctionPrologue();
        emitPutToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);

        m_beginLabel = label();

        addPtr(TrustedImm32(-m_calleeSaveSpace - WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_stackHeight) * sizeof(StackSlot) - maxFrameExtentForSlowPathCall), GPRInfo::callFrameRegister, GPRInfo::regT1);
        m_stackOverflow = branchPtr(Above, AbsoluteAddress(m_vm->addressOfStackLimit()), GPRInfo::regT1);

        move(GPRInfo::regT1, stackPointerRegister);
        checkStackPointerAlignment();

        emitSaveCalleeSaves();
        emitMaterializeTagCheckRegisters();

        m_numberOfLocals = arguments.size() + numberOfI32LocalVariables + numberOfF32LocalVariables + numberOfF64LocalVariables;

        unsigned localIndex = 0;
        for (size_t i = 0; i < arguments.size(); ++i) {
            // FIXME: No need to do type conversion if the caller is a WebAssembly function.
            // https://bugs.webkit.org/show_bug.cgi?id=149310
            Address address(GPRInfo::callFrameRegister, CallFrame::argumentOffset(i) * sizeof(Register));
#if USE(JSVALUE64)
            JSValueRegs valueRegs(GPRInfo::regT0);
#else
            JSValueRegs valueRegs(GPRInfo::regT1, GPRInfo::regT0);
#endif
            loadValue(address, valueRegs);
            switch (arguments[i]) {
            case WASMType::I32:
                convertValueToInt32(valueRegs, GPRInfo::regT0);
                store32(GPRInfo::regT0, localAddress(localIndex++));
                break;
            case WASMType::F32:
            case WASMType::F64:
#if USE(JSVALUE64)
                convertValueToDouble(valueRegs, FPRInfo::fpRegT0, GPRInfo::regT1);
#else
                convertValueToDouble(valueRegs, FPRInfo::fpRegT0, GPRInfo::regT2, FPRInfo::fpRegT1);
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
        for (uint32_t i = 0; i < numberOfF64LocalVariables; ++i) {
#if USE(JSVALUE64)
            store64(TrustedImm64(0), localAddress(localIndex++));
#else
            store32(TrustedImm32(0), localAddress(localIndex));
            store32(TrustedImm32(0), localAddress(localIndex).withOffset(4));
            localIndex++;
#endif
        }

        m_codeBlock->setNumParameters(1 + arguments.size());
    }

    void endFunction()
    {
        ASSERT(!m_tempStackTop);

        // FIXME: Remove these if the last statement is a return statement.
#if USE(JSVALUE64)
        JSValueRegs returnValueRegs(GPRInfo::returnValueGPR);
#else
        JSValueRegs returnValueRegs(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
#endif
        moveTrustedValue(jsUndefined(), returnValueRegs);
        emitRestoreCalleeSaves();
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
        emitPutToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);
        jump(m_beginLabel);

        if (!m_divideErrorJumpList.empty()) {
            m_divideErrorJumpList.link(this);

            setupArgumentsExecState();
            appendCallWithExceptionCheck(operationThrowDivideError);
        }

        if (!m_outOfBoundsErrorJumpList.empty()) {
            m_outOfBoundsErrorJumpList.link(this);

            setupArgumentsExecState();
            appendCallWithExceptionCheck(operationThrowOutOfBoundsAccessError);
        }

        if (!m_exceptionChecks.empty()) {
            m_exceptionChecks.link(this);

            copyCalleeSavesToVMCalleeSavesBuffer();

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

    int buildSetLocal(WASMOpKind opKind, uint32_t localIndex, int, WASMType type)
    {
        switch (type) {
        case WASMType::I32:
        case WASMType::F32:
            load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT0);
            store32(GPRInfo::regT0, localAddress(localIndex));
            break;
        case WASMType::F64:
            loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT0);
            storeDouble(FPRInfo::fpRegT0, localAddress(localIndex));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        if (opKind == WASMOpKind::Statement)
            m_tempStackTop--;
        return UNUSED;
    }

    int buildSetGlobal(WASMOpKind opKind, uint32_t globalIndex, int, WASMType type)
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
        if (opKind == WASMOpKind::Statement)
            m_tempStackTop--;
        return UNUSED;
    }

    void buildReturn(int, WASMExpressionType returnType)
    {
#if USE(JSVALUE64)
        JSValueRegs returnValueRegs(GPRInfo::returnValueGPR);
#else
        JSValueRegs returnValueRegs(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
#endif
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
            convertDoubleToValue(FPRInfo::fpRegT0, returnValueRegs);
            m_tempStackTop--;
            break;
        case WASMExpressionType::Void:
            moveTrustedValue(jsUndefined(), returnValueRegs);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        emitRestoreCalleeSaves();
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

    int buildConvertType(int, WASMExpressionType fromType, WASMExpressionType toType, WASMTypeConversion conversion)
    {
        switch (fromType) {
        case WASMExpressionType::I32:
            load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT0);
            ASSERT(toType == WASMExpressionType::F32 || toType == WASMExpressionType::F64);
            if (conversion == WASMTypeConversion::ConvertSigned)
                convertInt32ToDouble(GPRInfo::regT0, FPRInfo::fpRegT0);
            else {
                ASSERT(conversion == WASMTypeConversion::ConvertUnsigned);
#if USE(JSVALUE64)
                convertInt64ToDouble(GPRInfo::regT0, FPRInfo::fpRegT0);
#else
                callOperation(operationConvertUnsignedInt32ToDouble, GPRInfo::regT0, FPRInfo::fpRegT0);
#endif
            }
            if (toType == WASMExpressionType::F32)
                convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
            storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop - 1));
            break;
        case WASMExpressionType::F32:
            loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT0);
            switch (toType) {
            case WASMExpressionType::I32:
                ASSERT(conversion == WASMTypeConversion::ConvertSigned);
                convertFloatToDouble(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
                truncateDoubleToInt32(FPRInfo::fpRegT0, GPRInfo::regT0);
                store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop - 1));
                break;
            case WASMExpressionType::F64:
                ASSERT(conversion == WASMTypeConversion::Promote);
                convertFloatToDouble(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
                storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop - 1));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        case WASMExpressionType::F64:
            loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT0);
            switch (toType) {
            case WASMExpressionType::I32:
                ASSERT(conversion == WASMTypeConversion::ConvertSigned);
                truncateDoubleToInt32(FPRInfo::fpRegT0, GPRInfo::regT0);
                store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop - 1));
                break;
            case WASMExpressionType::F32:
                ASSERT(conversion == WASMTypeConversion::Demote);
                convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
                storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop - 1));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        return UNUSED;
    }

    int buildLoad(const MemoryAddress& memoryAddress, WASMExpressionType expressionType, WASMMemoryType memoryType, MemoryAccessConversion conversion)
    {
        const ArrayBuffer* arrayBuffer = m_module->arrayBuffer()->impl();
        move(TrustedImmPtr(arrayBuffer->data()), GPRInfo::regT0);
        load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT1);
        if (memoryAddress.offset)
            add32(TrustedImm32(memoryAddress.offset), GPRInfo::regT1, GPRInfo::regT1);
        and32(TrustedImm32(~(sizeOfMemoryType(memoryType) - 1)), GPRInfo::regT1);

        ASSERT(arrayBuffer->byteLength() < (unsigned)(1 << 31));
        if (arrayBuffer->byteLength() >= sizeOfMemoryType(memoryType))
            m_outOfBoundsErrorJumpList.append(branch32(Above, GPRInfo::regT1, TrustedImm32(arrayBuffer->byteLength() - sizeOfMemoryType(memoryType))));
        else
            m_outOfBoundsErrorJumpList.append(jump());

        BaseIndex address = BaseIndex(GPRInfo::regT0, GPRInfo::regT1, TimesOne);

        switch (expressionType) {
        case WASMExpressionType::I32:
            switch (memoryType) {
            case WASMMemoryType::I8:
                if (conversion == MemoryAccessConversion::SignExtend)
                    load8SignedExtendTo32(address, GPRInfo::regT0);
                else {
                    ASSERT(conversion == MemoryAccessConversion::ZeroExtend);
                    load8(address, GPRInfo::regT0);
                }
                break;
            case WASMMemoryType::I16:
                if (conversion == MemoryAccessConversion::SignExtend)
                    load16SignedExtendTo32(address, GPRInfo::regT0);
                else {
                    ASSERT(conversion == MemoryAccessConversion::ZeroExtend);
                    load16(address, GPRInfo::regT0);
                }
                break;
            case WASMMemoryType::I32:
                load32(address, GPRInfo::regT0);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop - 1));
            break;
        case WASMExpressionType::F32:
            ASSERT(memoryType == WASMMemoryType::F32 && conversion == MemoryAccessConversion::NoConversion);
            load32(address, GPRInfo::regT0);
            store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop - 1));
            break;
        case WASMExpressionType::F64:
            ASSERT(memoryType == WASMMemoryType::F64 && conversion == MemoryAccessConversion::NoConversion);
            loadDouble(address, FPRInfo::fpRegT0);
            storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop - 1));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        return UNUSED;
    }

    int buildStore(WASMOpKind opKind, const MemoryAddress& memoryAddress, WASMExpressionType expressionType, WASMMemoryType memoryType, int)
    {
        const ArrayBuffer* arrayBuffer = m_module->arrayBuffer()->impl();
        move(TrustedImmPtr(arrayBuffer->data()), GPRInfo::regT0);
        load32(temporaryAddress(m_tempStackTop - 2), GPRInfo::regT1);
        if (memoryAddress.offset)
            add32(TrustedImm32(memoryAddress.offset), GPRInfo::regT1, GPRInfo::regT1);
        and32(TrustedImm32(~(sizeOfMemoryType(memoryType) - 1)), GPRInfo::regT1);

        ASSERT(arrayBuffer->byteLength() < (1u << 31));
        if (arrayBuffer->byteLength() >= sizeOfMemoryType(memoryType))
            m_outOfBoundsErrorJumpList.append(branch32(Above, GPRInfo::regT1, TrustedImm32(arrayBuffer->byteLength() - sizeOfMemoryType(memoryType))));
        else
            m_outOfBoundsErrorJumpList.append(jump());

        BaseIndex address = BaseIndex(GPRInfo::regT0, GPRInfo::regT1, TimesOne);

        switch (expressionType) {
        case WASMExpressionType::I32:
            load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT2);
            switch (memoryType) {
            case WASMMemoryType::I8:
                store8(GPRInfo::regT2, address);
                break;
            case WASMMemoryType::I16:
                store16(GPRInfo::regT2, address);
                break;
            case WASMMemoryType::I32:
                store32(GPRInfo::regT2, address);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        case WASMExpressionType::F32:
            ASSERT(memoryType == WASMMemoryType::F32);
            load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT2);
            store32(GPRInfo::regT2, address);
            break;
        case WASMExpressionType::F64:
            ASSERT(memoryType == WASMMemoryType::F64);
            loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT0);
            storeDouble(FPRInfo::fpRegT0, address);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        m_tempStackTop -= 2;

        if (opKind == WASMOpKind::Expression) {
            switch (expressionType) {
            case WASMExpressionType::I32:
            case WASMExpressionType::F32:
                store32(GPRInfo::regT2, temporaryAddress(m_tempStackTop++));
                break;
            case WASMExpressionType::F64:
                storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop++));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
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

    int buildUnaryF64(int, WASMOpExpressionF64 op)
    {
        loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT1);
        switch (op) {
        case WASMOpExpressionF64::Negate:
            negateDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF64::Abs:
            absDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF64::Sqrt:
            sqrtDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF64::Ceil:
        case WASMOpExpressionF64::Floor:
        case WASMOpExpressionF64::Cos:
        case WASMOpExpressionF64::Sin:
        case WASMOpExpressionF64::Tan:
        case WASMOpExpressionF64::ACos:
        case WASMOpExpressionF64::ASin:
        case WASMOpExpressionF64::ATan:
        case WASMOpExpressionF64::Exp:
        case WASMOpExpressionF64::Ln:
            D_JITOperation_D operation;
            switch (op) {
            case WASMOpExpressionF64::Ceil:
                operation = ceil;
                break;
            case WASMOpExpressionF64::Floor:
                operation = floor;
                break;
            case WASMOpExpressionF64::Cos:
                operation = cos;
                break;
            case WASMOpExpressionF64::Sin:
                operation = sin;
                break;
            case WASMOpExpressionF64::Tan:
                operation = tan;
                break;
            case WASMOpExpressionF64::ACos:
                operation = acos;
                break;
            case WASMOpExpressionF64::ASin:
                operation = asin;
                break;
            case WASMOpExpressionF64::ATan:
                operation = atan;
                break;
            case WASMOpExpressionF64::Exp:
                operation = exp;
                break;
            case WASMOpExpressionF64::Ln:
                operation = log;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            callOperation(operation, FPRInfo::fpRegT1, FPRInfo::fpRegT0);
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
                x86ConvertToDoubleWord32();
                x86Div32(X86Registers::ecx);
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

    int buildBinaryF64(int, int, WASMOpExpressionF64 op)
    {
        loadDouble(temporaryAddress(m_tempStackTop - 2), FPRInfo::fpRegT0);
        loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT1);
        switch (op) {
        case WASMOpExpressionF64::Add:
            addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF64::Sub:
            subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF64::Mul:
            mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF64::Div:
            divDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        case WASMOpExpressionF64::Mod:
        case WASMOpExpressionF64::ATan2:
        case WASMOpExpressionF64::Pow:
            D_JITOperation_DD operation;
            switch (op) {
            case WASMOpExpressionF64::Mod:
                operation = fmod;
                break;
            case WASMOpExpressionF64::ATan2:
                operation = atan2;
                break;
            case WASMOpExpressionF64::Pow:
                operation = pow;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            callOperation(operation, FPRInfo::fpRegT0, FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
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

    int buildMinOrMaxI32(int, int, WASMOpExpressionI32 op)
    {
        load32(temporaryAddress(m_tempStackTop - 2), GPRInfo::regT0);
        load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT1);
        RelationalCondition condition;
        switch (op) {
        case WASMOpExpressionI32::SMin:
            condition = LessThanOrEqual;
            break;
        case WASMOpExpressionI32::UMin:
            condition = BelowOrEqual;
            break;
        case WASMOpExpressionI32::SMax:
            condition = GreaterThanOrEqual;
            break;
        case WASMOpExpressionI32::UMax:
            condition = AboveOrEqual;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        Jump useLeft = branch32(condition, GPRInfo::regT0, GPRInfo::regT1);
        store32(GPRInfo::regT1, temporaryAddress(m_tempStackTop - 2));
        useLeft.link(this);
        m_tempStackTop--;
        return UNUSED;
    }

    int buildMinOrMaxF64(int, int, WASMOpExpressionF64 op)
    {
        loadDouble(temporaryAddress(m_tempStackTop - 2), FPRInfo::fpRegT0);
        loadDouble(temporaryAddress(m_tempStackTop - 1), FPRInfo::fpRegT1);
        DoubleCondition condition;
        switch (op) {
        case WASMOpExpressionF64::Min:
            condition = DoubleLessThanOrEqual;
            break;
        case WASMOpExpressionF64::Max:
            condition = DoubleGreaterThanOrEqual;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        Jump useLeft = branchDouble(condition, FPRInfo::fpRegT0, FPRInfo::fpRegT1);
        storeDouble(FPRInfo::fpRegT1, temporaryAddress(m_tempStackTop - 2));
        useLeft.link(this);
        m_tempStackTop--;
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

    int buildCallIndirect(uint32_t functionPointerTableIndex, int, int, const WASMSignature& signature, WASMExpressionType returnType)
    {
        boxArgumentsAndAdjustStackPointer(signature.arguments);

        const Vector<JSFunction*>& functions = m_module->functionPointerTables()[functionPointerTableIndex].functions;
        move(TrustedImmPtr(functions.data()), GPRInfo::regT0);
        load32(temporaryAddress(m_tempStackTop - 1), GPRInfo::regT1);
        m_tempStackTop--;
        and32(TrustedImm32(functions.size() - 1), GPRInfo::regT1);
        loadPtr(BaseIndex(GPRInfo::regT0, GPRInfo::regT1, timesPtr()), GPRInfo::regT0);

        callAndUnboxResult(returnType);
        return UNUSED;
    }

    int buildCallImport(uint32_t functionImportIndex, int, const WASMSignature& signature, WASMExpressionType returnType)
    {
        boxArgumentsAndAdjustStackPointer(signature.arguments);

        JSFunction* function = m_module->importedFunctions()[functionImportIndex].get();
        move(TrustedImmPtr(function), GPRInfo::regT0);

        callAndUnboxResult(returnType);
        return UNUSED;
    }

    void appendExpressionList(int&, int) { }

    void discard(int)
    {
        m_tempStackTop--;
    }

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

    enum class FloatingPointPrecision { Single, Double };

    Address localAddress(unsigned localIndex) const
    {
        ASSERT(localIndex < m_numberOfLocals);
        return Address(GPRInfo::callFrameRegister, -m_calleeSaveSpace - (localIndex + 1) * sizeof(StackSlot));
    }

    Address temporaryAddress(unsigned temporaryIndex) const
    {
        ASSERT(m_numberOfLocals + temporaryIndex < m_stackHeight);
        return Address(GPRInfo::callFrameRegister, -m_calleeSaveSpace - (m_numberOfLocals + temporaryIndex + 1) * sizeof(StackSlot));
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
    void appendCallSetResult(const FunctionPtr& function, FPRReg result, FloatingPointPrecision precision)
    {
        appendCall(function);
        switch (precision) {
        case FloatingPointPrecision::Single:
            m_assembler.fstps(0, stackPointerRegister);
            break;
        case FloatingPointPrecision::Double:
            m_assembler.fstpl(0, stackPointerRegister);
            break;
        }
        loadDouble(stackPointerRegister, result);
    }
#elif CPU(ARM) && !CPU(ARM_HARDFP)
    void appendCallSetResult(const FunctionPtr& function, FPRReg result, FloatingPointPrecision)
    {
        appendCall(function);
        m_assembler.vmov(result, GPRInfo::returnValueGPR, GPRInfo::returnValueGPR2);
    }
#else // CPU(X86_64) || (CPU(ARM) && CPU(ARM_HARDFP)) || CPU(ARM64) || CPU(MIPS) || CPU(SH4)
    void appendCallSetResult(const FunctionPtr& function, FPRReg result, FloatingPointPrecision)
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
        appendCallSetResult(operation, dst, FloatingPointPrecision::Double);
    }
#else
// EncodedJSValue in JSVALUE32_64 is a 64-bit integer. When being compiled in ARM EABI, it must be aligned on an even-numbered register (r0, r2 or [sp]).
// To prevent the assembler from using wrong registers, let's occupy r1 or r3 with a dummy argument when necessary.
#if (COMPILER_SUPPORTS(EABI) && CPU(ARM)) || CPU(MIPS)
#define EABI_32BIT_DUMMY_ARG      TrustedImm32(0),
#else
#define EABI_32BIT_DUMMY_ARG
#endif

    void callOperation(Z_JITOperation_EJ operation, GPRReg srcTag, GPRReg srcPayload, GPRReg dst)
    {
        setupArgumentsWithExecState(EABI_32BIT_DUMMY_ARG srcPayload, srcTag);
        appendCallSetResult(operation, dst);
    }

    void callOperation(D_JITOperation_EJ operation, GPRReg srcTag, GPRReg srcPayload, FPRReg dst)
    {
        setupArgumentsWithExecState(EABI_32BIT_DUMMY_ARG srcPayload, srcTag);
        appendCallSetResult(operation, dst, FloatingPointPrecision::Double);
    }
#endif

    void callOperation(float JIT_OPERATION (*operation)(float), FPRegisterID src, FPRegisterID dst)
    {
        setupArguments(src);
        appendCallSetResult(operation, dst, FloatingPointPrecision::Single);
    }

    void callOperation(D_JITOperation_D operation, FPRegisterID src, FPRegisterID dst)
    {
        setupArguments(src);
        appendCallSetResult(operation, dst, FloatingPointPrecision::Double);
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

    void callOperation(D_JITOperation_DD operation, FPRegisterID src1, FPRegisterID src2, FPRegisterID dst)
    {
        setupArguments(src1, src2);
        appendCallSetResult(operation, dst, FloatingPointPrecision::Double);
    }

    void callOperation(double JIT_OPERATION (*operation)(uint32_t), GPRReg src, FPRegisterID dst)
    {
        setupArguments(src);
        appendCallSetResult(operation, dst, FloatingPointPrecision::Double);
    }

    void boxArgumentsAndAdjustStackPointer(const Vector<WASMType>& arguments)
    {
        size_t argumentCount = arguments.size();
        int stackOffset = -m_calleeSaveSpace - WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_numberOfLocals + m_tempStackTop + argumentCount + 1 + JSStack::CallFrameHeaderSize);

        storeTrustedValue(jsUndefined(), Address(GPRInfo::callFrameRegister, (stackOffset + CallFrame::thisArgumentOffset()) * sizeof(Register)));

        for (size_t i = 0; i < argumentCount; ++i) {
            Address address(GPRInfo::callFrameRegister, (stackOffset + CallFrame::argumentOffset(i)) * sizeof(Register));
#if USE(JSVALUE64)
            JSValueRegs valueRegs(GPRInfo::regT0);
#else
            JSValueRegs valueRegs(GPRInfo::regT1, GPRInfo::regT0);
#endif
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
            case WASMType::F32:
            case WASMType::F64:
                loadDouble(temporaryAddress(m_tempStackTop - argumentCount + i), FPRInfo::fpRegT0);
                if (arguments[i] == WASMType::F32)
                    convertFloatToDouble(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
                convertDoubleToValue(FPRInfo::fpRegT0, valueRegs);
                storeValue(valueRegs, address);
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
        store32(GPRInfo::regT0, Address(stackPointerRegister, JSStack::Callee * static_cast<int>(sizeof(Register)) + PayloadOffset - sizeof(CallerFrameAndPC)));
        store32(TrustedImm32(JSValue::CellTag), Address(stackPointerRegister, JSStack::Callee * static_cast<int>(sizeof(Register)) + TagOffset - sizeof(CallerFrameAndPC)));
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
        addPtr(TrustedImm32(-m_calleeSaveSpace - WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_stackHeight) * sizeof(StackSlot) - maxFrameExtentForSlowPathCall), GPRInfo::callFrameRegister, stackPointerRegister);
        checkStackPointerAlignment();

        // FIXME: No need to do type conversion if the callee is a WebAssembly function.
        // https://bugs.webkit.org/show_bug.cgi?id=149310
#if USE(JSVALUE64)
        JSValueRegs valueRegs(GPRInfo::returnValueGPR);
#else
        JSValueRegs valueRegs(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
#endif
        switch (returnType) {
        case WASMExpressionType::I32:
            convertValueToInt32(valueRegs, GPRInfo::regT0);
            store32(GPRInfo::regT0, temporaryAddress(m_tempStackTop++));
            break;
        case WASMExpressionType::F32:
        case WASMExpressionType::F64:
#if USE(JSVALUE64)
            convertValueToDouble(valueRegs, FPRInfo::fpRegT0, GPRInfo::nonPreservedNonReturnGPR);
#else
            convertValueToDouble(valueRegs, FPRInfo::fpRegT0, GPRInfo::nonPreservedNonReturnGPR, FPRInfo::fpRegT1);
#endif
            if (returnType == WASMExpressionType::F32)
                convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
            storeDouble(FPRInfo::fpRegT0, temporaryAddress(m_tempStackTop++));
            break;
        case WASMExpressionType::Void:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

#if USE(JSVALUE64)
    void convertValueToInt32(JSValueRegs valueRegs, GPRReg dst)
    {
        Jump checkJSInt32 = branchIfInt32(valueRegs);

        callOperation(operationConvertJSValueToInt32, valueRegs.gpr(), valueRegs.gpr());

        checkJSInt32.link(this);
        move(valueRegs.gpr(), dst);
    }

    void convertValueToDouble(JSValueRegs valueRegs, FPRReg dst, GPRReg scratch)
    {
        Jump checkJSInt32 = branchIfInt32(valueRegs);
        Jump checkJSNumber = branchIfNumber(valueRegs, scratch);
        JumpList end;

        callOperation(operationConvertJSValueToDouble, valueRegs.gpr(), dst);
        end.append(jump());

        checkJSInt32.link(this);
        convertInt32ToDouble(valueRegs.gpr(), dst);
        end.append(jump());

        checkJSNumber.link(this);
        unboxDoubleWithoutAssertions(valueRegs.gpr(), dst);
        end.link(this);
    }
#else
    void convertValueToInt32(JSValueRegs valueRegs, GPRReg dst)
    {
        Jump checkJSInt32 = branchIfInt32(valueRegs);

        callOperation(operationConvertJSValueToInt32, valueRegs.tagGPR(), valueRegs.payloadGPR(), valueRegs.payloadGPR());

        checkJSInt32.link(this);
        move(valueRegs.payloadGPR(), dst);
    }

    void convertValueToDouble(JSValueRegs valueRegs, FPRReg dst, GPRReg scratch, FPRReg fpScratch)
    {
        Jump checkJSInt32 = branchIfInt32(valueRegs);
        Jump checkJSNumber = branchIfNumber(valueRegs, scratch);
        JumpList end;

        callOperation(operationConvertJSValueToDouble, valueRegs.tagGPR(), valueRegs.payloadGPR(), dst);
        end.append(jump());

        checkJSInt32.link(this);
        convertInt32ToDouble(valueRegs.payloadGPR(), dst);
        end.append(jump());

        checkJSNumber.link(this);
        unboxDouble(valueRegs.tagGPR(), valueRegs.payloadGPR(), dst, fpScratch);
        end.link(this);
    }
#endif

    void convertDoubleToValue(FPRReg fpr, JSValueRegs valueRegs)
    {
#if USE(JSVALUE64)
        boxDouble(fpr, valueRegs.gpr());
#else
        boxDouble(fpr, valueRegs.tagGPR(), valueRegs.payloadGPR());
#endif
    }

    JSWASMModule* m_module;
    unsigned m_stackHeight;
    unsigned m_numberOfLocals;
    unsigned m_tempStackTop { 0 };
    unsigned m_calleeSaveSpace;

    Vector<JumpTarget> m_breakTargets;
    Vector<JumpTarget> m_continueTargets;
    Vector<JumpTarget> m_breakLabelTargets;
    Vector<JumpTarget> m_continueLabelTargets;

    Label m_beginLabel;
    Jump m_stackOverflow;
    JumpList m_divideErrorJumpList;
    JumpList m_outOfBoundsErrorJumpList;
    JumpList m_exceptionChecks;

    Vector<std::pair<Call, void*>> m_calls;
    Vector<CallCompilationInfo> m_callCompilationInfo;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMFunctionCompiler_h
