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

#ifndef WASMFunctionB3IRGenerator_h
#define WASMFunctionB3IRGenerator_h

#if ENABLE(WEBASSEMBLY) && ENABLE(FTL_JIT)

#include "FTLAbbreviatedTypes.h"

#define UNUSED 0

namespace JSC {

using FTL::LBasicBlock;
using FTL::LValue;

class WASMFunctionB3IRGenerator {
public:
    typedef LValue Expression;
    typedef int Statement;
    typedef Vector<LValue> ExpressionList;
    struct MemoryAddress {
        MemoryAddress(void*) { }
        MemoryAddress(LValue index, uint32_t offset)
            : index(index)
            , offset(offset)
        {
        }
        LValue index;
        uint32_t offset;
    };
    typedef LBasicBlock JumpTarget;
    enum class JumpCondition { Zero, NonZero };

    void startFunction(const Vector<WASMType>& arguments, uint32_t numberOfI32LocalVariables, uint32_t numberOfF32LocalVariables, uint32_t numberOfF64LocalVariables)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(arguments);
        UNUSED_PARAM(numberOfI32LocalVariables);
        UNUSED_PARAM(numberOfF32LocalVariables);
        UNUSED_PARAM(numberOfF64LocalVariables);
    }

    void endFunction()
    {
        // FIXME: Implement this method.
    }

    LValue buildSetLocal(WASMOpKind opKind, uint32_t localIndex, LValue value, WASMType type)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(opKind);
        UNUSED_PARAM(localIndex);
        UNUSED_PARAM(value);
        UNUSED_PARAM(type);
        return UNUSED;
    }

    LValue buildSetGlobal(WASMOpKind opKind, uint32_t globalIndex, LValue value, WASMType type)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(opKind);
        UNUSED_PARAM(globalIndex);
        UNUSED_PARAM(value);
        UNUSED_PARAM(type);
        return UNUSED;
    }

    void buildReturn(LValue value, WASMExpressionType returnType)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(value);
        UNUSED_PARAM(returnType);
    }

    LValue buildImmediateI32(uint32_t immediate)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(immediate);
        return UNUSED;
    }

    LValue buildImmediateF32(float immediate)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(immediate);
        return UNUSED;
    }

    LValue buildImmediateF64(double immediate)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(immediate);
        return UNUSED;
    }

    LValue buildGetLocal(uint32_t localIndex, WASMType type)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(localIndex);
        UNUSED_PARAM(type);
        return UNUSED;
    }

    LValue buildGetGlobal(uint32_t globalIndex, WASMType type)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(globalIndex);
        UNUSED_PARAM(type);
        return UNUSED;
    }

    LValue buildConvertType(LValue value, WASMExpressionType fromType, WASMExpressionType toType, WASMTypeConversion conversion)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(value);
        UNUSED_PARAM(fromType);
        UNUSED_PARAM(toType);
        UNUSED_PARAM(conversion);
        return UNUSED;
    }

    LValue buildLoad(const MemoryAddress& memoryAddress, WASMExpressionType expressionType, WASMMemoryType memoryType, MemoryAccessConversion conversion)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(memoryAddress);
        UNUSED_PARAM(expressionType);
        UNUSED_PARAM(memoryType);
        UNUSED_PARAM(conversion);
        return UNUSED;
    }

    LValue buildStore(WASMOpKind opKind, const MemoryAddress& memoryAddress, WASMExpressionType expressionType, WASMMemoryType memoryType, LValue value)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(opKind);
        UNUSED_PARAM(memoryAddress);
        UNUSED_PARAM(expressionType);
        UNUSED_PARAM(memoryType);
        UNUSED_PARAM(value);
        return UNUSED;
    }

    LValue buildUnaryI32(LValue value, WASMOpExpressionI32 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(value);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildUnaryF32(LValue value, WASMOpExpressionF32 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(value);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildUnaryF64(LValue value, WASMOpExpressionF64 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(value);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildBinaryI32(LValue left, LValue right, WASMOpExpressionI32 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildBinaryF32(LValue left, LValue right, WASMOpExpressionF32 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildBinaryF64(LValue left, LValue right, WASMOpExpressionF64 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildRelationalI32(LValue left, LValue right, WASMOpExpressionI32 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildRelationalF32(LValue left, LValue right, WASMOpExpressionI32 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildRelationalF64(LValue left, LValue right, WASMOpExpressionI32 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildMinOrMaxI32(LValue left, LValue right, WASMOpExpressionI32 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildMinOrMaxF64(LValue left, LValue right, WASMOpExpressionF64 op)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        UNUSED_PARAM(op);
        return UNUSED;
    }

    LValue buildCallInternal(uint32_t functionIndex, const Vector<LValue>& argumentList, const WASMSignature& signature, WASMExpressionType returnType)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(functionIndex);
        UNUSED_PARAM(argumentList);
        UNUSED_PARAM(signature);
        UNUSED_PARAM(returnType);
        return UNUSED;
    }

    LValue buildCallIndirect(uint32_t functionPointerTableIndex, LValue index, const Vector<LValue>& argumentList, const WASMSignature& signature, WASMExpressionType returnType)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(functionPointerTableIndex);
        UNUSED_PARAM(index);
        UNUSED_PARAM(argumentList);
        UNUSED_PARAM(signature);
        UNUSED_PARAM(returnType);
        return UNUSED;
    }

    LValue buildCallImport(uint32_t functionImportIndex, const Vector<LValue>& argumentList, const WASMSignature& signature, WASMExpressionType returnType)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(functionImportIndex);
        UNUSED_PARAM(argumentList);
        UNUSED_PARAM(signature);
        UNUSED_PARAM(returnType);
        return UNUSED;
    }

    void appendExpressionList(Vector<LValue>& expressionList, LValue value)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(expressionList);
        UNUSED_PARAM(value);
    }

    void discard(LValue)
    {
    }

    void linkTarget(LBasicBlock target)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(target);
    }

    void jumpToTarget(LBasicBlock target)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(target);
    }

    void jumpToTargetIf(JumpCondition, LValue value, LBasicBlock target)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(value);
        UNUSED_PARAM(target);
    }

    void startLoop()
    {
        // FIXME: Implement this method.
    }

    void endLoop()
    {
        // FIXME: Implement this method.
    }

    void startSwitch()
    {
        // FIXME: Implement this method.
    }

    void endSwitch()
    {
        // FIXME: Implement this method.
    }

    void startLabel()
    {
        // FIXME: Implement this method.
    }

    void endLabel()
    {
        // FIXME: Implement this method.
    }

    LBasicBlock breakTarget()
    {
        // FIXME: Implement this method.
        return UNUSED;
    }

    LBasicBlock continueTarget()
    {
        // FIXME: Implement this method.
        return UNUSED;
    }

    LBasicBlock breakLabelTarget(uint32_t labelIndex)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(labelIndex);
        return UNUSED;
    }

    LBasicBlock continueLabelTarget(uint32_t labelIndex)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(labelIndex);
        return UNUSED;
    }

    void buildSwitch(LValue value, const Vector<int64_t>& cases, const Vector<LBasicBlock>& targets, LBasicBlock defaultTarget)
    {
        // FIXME: Implement this method.
        UNUSED_PARAM(value);
        UNUSED_PARAM(cases);
        UNUSED_PARAM(targets);
        UNUSED_PARAM(defaultTarget);
    }
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMFunctionB3IRGenerator_h
