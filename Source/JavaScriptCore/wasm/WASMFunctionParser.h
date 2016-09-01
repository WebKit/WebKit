/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "WASMParser.h"
#include <wtf/DataLog.h>

namespace JSC {

namespace WASM {

template<typename Context>
class FunctionParser : public Parser {
public:
    typedef typename Context::ExpressionType ExpressionType;

    FunctionParser(Context&, const Vector<uint8_t>& sourceBuffer, const FunctionInformation&);

    bool WARN_UNUSED_RETURN parse();

private:
    static const bool verbose = false;

    bool WARN_UNUSED_RETURN parseBlock();
    bool WARN_UNUSED_RETURN parseExpression(OpType);
    bool WARN_UNUSED_RETURN unifyControl(Vector<ExpressionType>&, unsigned level);

    Optional<Vector<ExpressionType>>& stackForControlLevel(unsigned level);

    Context& m_context;
    Vector<ExpressionType> m_expressionStack;
};

template<typename Context>
FunctionParser<Context>::FunctionParser(Context& context, const Vector<uint8_t>& sourceBuffer, const FunctionInformation& info)
    : Parser(sourceBuffer, info.start, info.end)
    , m_context(context)
{
}

template<typename Context>
bool FunctionParser<Context>::parse()
{
    uint32_t localCount;
    if (!parseVarUInt32(localCount))
        return false;

    for (uint32_t i = 0; i < localCount; ++i) {
        uint32_t numberOfLocalsWithType;
        if (!parseUInt32(numberOfLocalsWithType))
            return false;

        Type typeOfLocal;
        if (!parseValueType(typeOfLocal))
            return false;

    }

    return parseBlock();
}

template<typename Context>
bool FunctionParser<Context>::parseBlock()
{
    while (true) {
        uint8_t op;
        if (!parseUInt7(op))
            return false;

        if (!parseExpression(static_cast<OpType>(op))) {
            if (verbose)
                dataLogLn("failed to process op:", op);
            return false;
        }

        if (op == OpType::End)
            break;
    }

    return true;
}

template<typename Context>
bool FunctionParser<Context>::parseExpression(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, b3op) case name:
    FOR_EACH_WASM_BINARY_OP(CREATE_CASE) {
        ExpressionType left = m_expressionStack.takeLast();
        ExpressionType right = m_expressionStack.takeLast();
        ExpressionType result;
        if (!m_context.binaryOp(static_cast<BinaryOpType>(op), left, right, result))
            return false;
        m_expressionStack.append(result);
        return true;
    }

    FOR_EACH_WASM_UNARY_OP(CREATE_CASE) {
        ExpressionType arg = m_expressionStack.takeLast();
        ExpressionType result;
        if (!m_context.unaryOp(static_cast<UnaryOpType>(op), arg, result))
            return false;
        m_expressionStack.append(result);
        return true;
    }
#undef CREATE_CASE

    case OpType::I32Const: {
        uint32_t constant;
        if (!parseVarUInt32(constant))
            return false;
        m_expressionStack.append(m_context.addConstant(Int32, constant));
        return true;
    }

    case OpType::Block: {
        if (!m_context.addBlock())
            return false;
        return parseBlock();
    }

    case OpType::Return: {
        uint8_t returnCount;
        if (!parseVarUInt1(returnCount))
            return false;
        Vector<ExpressionType, 1> returnValues;
        if (returnCount)
            returnValues.append(m_expressionStack.takeLast());

        return m_context.addReturn(returnValues);
    }

    case OpType::End:
        return m_context.endBlock(m_expressionStack);

    }

    // Unknown opcode.
    return false;
}

} // namespace WASM

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
