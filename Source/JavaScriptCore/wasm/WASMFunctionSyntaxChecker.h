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

#ifndef WASMFunctionSyntaxChecker_h
#define WASMFunctionSyntaxChecker_h

#if ENABLE(WEBASSEMBLY)

#define UNUSED 0

namespace JSC {

class WASMFunctionSyntaxChecker {
public:
    typedef int Expression;
    typedef int Statement;
    typedef int ExpressionList;
    struct MemoryAddress {
        MemoryAddress(void*) { }
        MemoryAddress(int, uint32_t) { }
    };
    typedef int JumpTarget;
    enum class JumpCondition { Zero, NonZero };

    void startFunction(const Vector<WASMType>& arguments, uint32_t numberOfI32LocalVariables, uint32_t numberOfF32LocalVariables, uint32_t numberOfF64LocalVariables)
    {
        m_numberOfLocals = arguments.size() + numberOfI32LocalVariables + numberOfF32LocalVariables + numberOfF64LocalVariables;
    }

    void endFunction()
    {
        ASSERT(!m_tempStackTop);
    }

    int buildSetLocal(WASMOpKind opKind, uint32_t, int, WASMType)
    {
        if (opKind == WASMOpKind::Statement)
            m_tempStackTop--;
        return UNUSED;
    }

    int buildSetGlobal(WASMOpKind opKind, uint32_t, int, WASMType)
    {
        if (opKind == WASMOpKind::Statement)
            m_tempStackTop--;
        return UNUSED;
    }

    void buildReturn(int, WASMExpressionType returnType)
    {
        if (returnType != WASMExpressionType::Void)
            m_tempStackTop--;
    }

    int buildImmediateI32(uint32_t)
    {
        m_tempStackTop++;
        updateTempStackHeight();
        return UNUSED;
    }

    int buildImmediateF32(float)
    {
        m_tempStackTop++;
        updateTempStackHeight();
        return UNUSED;
    }

    int buildImmediateF64(double)
    {
        m_tempStackTop++;
        updateTempStackHeight();
        return UNUSED;
    }

    int buildGetLocal(uint32_t, WASMType)
    {
        m_tempStackTop++;
        updateTempStackHeight();
        return UNUSED;
    }

    int buildGetGlobal(uint32_t, WASMType)
    {
        m_tempStackTop++;
        updateTempStackHeight();
        return UNUSED;
    }

    int buildConvertType(int, WASMExpressionType, WASMExpressionType, WASMTypeConversion)
    {
        return UNUSED;
    }

    int buildLoad(const MemoryAddress&, WASMExpressionType, WASMMemoryType, MemoryAccessConversion)
    {
        return UNUSED;
    }

    int buildStore(WASMOpKind opKind, const MemoryAddress&, WASMExpressionType, WASMMemoryType, int)
    {
        m_tempStackTop -= 2;
        if (opKind == WASMOpKind::Expression)
            m_tempStackTop++;
        return UNUSED;
    }

    int buildUnaryI32(int, WASMOpExpressionI32)
    {
        return UNUSED;
    }

    int buildUnaryF32(int, WASMOpExpressionF32)
    {
        return UNUSED;
    }

    int buildUnaryF64(int, WASMOpExpressionF64)
    {
        return UNUSED;
    }

    int buildBinaryI32(int, int, WASMOpExpressionI32)
    {
        m_tempStackTop--;
        return UNUSED;
    }

    int buildBinaryF32(int, int, WASMOpExpressionF32)
    {
        m_tempStackTop--;
        return UNUSED;
    }

    int buildBinaryF64(int, int, WASMOpExpressionF64)
    {
        m_tempStackTop--;
        return UNUSED;
    }

    int buildRelationalI32(int, int, WASMOpExpressionI32)
    {
        m_tempStackTop--;
        return UNUSED;
    }

    int buildRelationalF32(int, int, WASMOpExpressionI32)
    {
        m_tempStackTop--;
        return UNUSED;
    }

    int buildRelationalF64(int, int, WASMOpExpressionI32)
    {
        m_tempStackTop--;
        return UNUSED;
    }

    int buildMinOrMaxI32(int, int, WASMOpExpressionI32)
    {
        m_tempStackTop--;
        return UNUSED;
    }

    int buildMinOrMaxF64(int, int, WASMOpExpressionF64)
    {
        m_tempStackTop--;
        return UNUSED;
    }

    int buildCallInternal(uint32_t, int, const WASMSignature& signature, WASMExpressionType returnType)
    {
        size_t argumentCount = signature.arguments.size();
        updateTempStackHeightForCall(argumentCount);
        m_tempStackTop -= argumentCount;
        if (returnType != WASMExpressionType::Void) {
            m_tempStackTop++;
            updateTempStackHeight();
        }
        return UNUSED;
    }

    int buildCallImport(uint32_t, int, const WASMSignature& signature, WASMExpressionType returnType)
    {
        size_t argumentCount = signature.arguments.size();
        updateTempStackHeightForCall(argumentCount);
        m_tempStackTop -= argumentCount;
        if (returnType != WASMExpressionType::Void) {
            m_tempStackTop++;
            updateTempStackHeight();
        }
        return UNUSED;
    }

    int buildCallIndirect(uint32_t, int, int, const WASMSignature& signature, WASMExpressionType returnType)
    {
        size_t argumentCount = signature.arguments.size();
        updateTempStackHeightForCall(argumentCount);
        m_tempStackTop -= argumentCount + 1;
        if (returnType != WASMExpressionType::Void)
            m_tempStackTop++;
        return UNUSED;
    }

    void appendExpressionList(int&, int) { }

    void discard(int)
    {
        m_tempStackTop--;
    }

    void linkTarget(const int&) { }
    void jumpToTarget(const int&) { }
    void jumpToTargetIf(JumpCondition, int, const int&)
    {
        m_tempStackTop--;
    }

    void startLoop() { }
    void endLoop() { }
    void startSwitch() { }
    void endSwitch() { }
    void startLabel() { }
    void endLabel() { }

    int breakTarget() { return UNUSED; }
    int continueTarget() { return UNUSED; }
    int breakLabelTarget(uint32_t) { return UNUSED; }
    int continueLabelTarget(uint32_t) { return UNUSED; }

    void buildSwitch(int, const Vector<int64_t>&, const Vector<int>&, const int&)
    {
        m_tempStackTop--;
    }

    unsigned stackHeight()
    {
        return m_numberOfLocals + m_tempStackHeight;
    }

private:
    void updateTempStackHeight()
    {
        if (m_tempStackTop > m_tempStackHeight)
            m_tempStackHeight = m_tempStackTop;
    }

    void updateTempStackHeightForCall(size_t argumentCount)
    {
        // Boxed arguments + this argument + call frame header + maximum padding.
        m_tempStackTop += argumentCount + 1 + CallFrame::headerSizeInRegisters + 1;
        updateTempStackHeight();
        m_tempStackTop -= argumentCount + 1 + CallFrame::headerSizeInRegisters + 1;
    }

    unsigned m_numberOfLocals;
    unsigned m_tempStackTop { 0 };
    unsigned m_tempStackHeight { 0 };
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMFunctionSyntaxChecker_h
