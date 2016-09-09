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

namespace JSC { namespace WASM {

enum class BlockType {
    If,
    Block,
    Loop
};

template<typename Context>
class FunctionParser : public Parser {
public:
    typedef typename Context::ExpressionType ExpressionType;
    typedef typename Context::ControlType ControlType;

    FunctionParser(Context&, const Vector<uint8_t>& sourceBuffer, const FunctionInformation&);

    bool WARN_UNUSED_RETURN parse();

private:
    static const bool verbose = false;

    bool WARN_UNUSED_RETURN parseBlock();
    bool WARN_UNUSED_RETURN parseExpression(OpType);
    bool WARN_UNUSED_RETURN parseUnreachableExpression(OpType);
    bool WARN_UNUSED_RETURN unifyControl(Vector<ExpressionType>&, unsigned level);

    Context& m_context;
    Vector<ExpressionType, 1> m_expressionStack;
    Vector<ControlType> m_controlStack;
    unsigned m_unreachableBlocks { 0 };
};

template<typename Context>
FunctionParser<Context>::FunctionParser(Context& context, const Vector<uint8_t>& sourceBuffer, const FunctionInformation& info)
    : Parser(sourceBuffer, info.start, info.end)
    , m_context(context)
{
    m_context.addArguments(info.signature->arguments);
}

template<typename Context>
bool FunctionParser<Context>::parse()
{
    uint32_t localCount;
    if (!parseVarUInt32(localCount))
        return false;

    for (uint32_t i = 0; i < localCount; ++i) {
        uint32_t numberOfLocals;
        if (!parseVarUInt32(numberOfLocals))
            return false;

        Type typeOfLocal;
        if (!parseValueType(typeOfLocal))
            return false;

        m_context.addLocal(typeOfLocal, numberOfLocals);
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

        if (verbose) {
            dataLogLn("processing op (", m_unreachableBlocks, "): ",  RawPointer(reinterpret_cast<void*>(op)));
            m_context.dump(m_controlStack, m_expressionStack);
        }

        if (op == OpType::End && !m_controlStack.size())
            break;

        if (m_unreachableBlocks) {
            if (!parseUnreachableExpression(static_cast<OpType>(op))) {
                if (verbose)
                    dataLogLn("failed to process unreachable op:", op);
                return false;
            }
        } else if (!parseExpression(static_cast<OpType>(op))) {
            if (verbose)
                dataLogLn("failed to process op:", op);
            return false;
        }

    }

    // I'm not sure if we should check the expression stack here...
    return true;
}
#define CREATE_CASE(name, id, b3op) case name:

template<typename Context>
bool FunctionParser<Context>::parseExpression(OpType op)
{
    switch (op) {
    FOR_EACH_WASM_BINARY_OP(CREATE_CASE) {
        ExpressionType right = m_expressionStack.takeLast();
        ExpressionType left = m_expressionStack.takeLast();
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

    case OpType::I32Const: {
        uint32_t constant;
        if (!parseVarUInt32(constant))
            return false;
        m_expressionStack.append(m_context.addConstant(I32, constant));
        return true;
    }

    case OpType::GetLocal: {
        uint32_t index;
        if (!parseVarUInt32(index))
            return false;
        ExpressionType result;
        if (!m_context.getLocal(index, result))
            return false;

        m_expressionStack.append(result);
        return true;
    }

    case OpType::SetLocal: {
        uint32_t index;
        if (!parseVarUInt32(index))
            return false;
        ExpressionType value = m_expressionStack.takeLast();
        return m_context.setLocal(index, value);
    }

    case OpType::Block: {
        m_controlStack.append(m_context.addBlock());
        return true;
    }

    case OpType::Loop: {
        m_controlStack.append(m_context.addLoop());
        return true;
    }

    case OpType::If: {
        ExpressionType condition = m_expressionStack.takeLast();
        m_controlStack.append(m_context.addIf(condition));
        return true;
    }

    case OpType::Else: {
        return m_context.addElse(m_controlStack.last());
    }

    case OpType::Branch:
    case OpType::BranchIf: {
        uint32_t arity;
        if (!parseVarUInt32(arity) || arity > m_expressionStack.size())
            return false;

        uint32_t target;
        if (!parseVarUInt32(target))
            return false;

        ExpressionType condition = Context::emptyExpression;
        if (op == OpType::BranchIf)
            condition = m_expressionStack.takeLast();
        else
            m_unreachableBlocks = 1;

        Vector<ExpressionType, 1> values(arity);
        for (unsigned i = arity; i; i--)
            values[i-1] = m_expressionStack.takeLast();

        if (target >= m_controlStack.size())
            return false;
        ControlType& data = m_controlStack[m_controlStack.size() - 1 - target];

        return m_context.addBranch(data, condition, values);
    }

    case OpType::Return: {
        uint8_t returnCount;
        if (!parseVarUInt1(returnCount))
            return false;
        Vector<ExpressionType, 1> returnValues;
        if (returnCount)
            returnValues.append(m_expressionStack.takeLast());

        m_unreachableBlocks = 1;
        return m_context.addReturn(returnValues);
    }

    case OpType::End: {
        ControlType data = m_controlStack.takeLast();
        return m_context.endBlock(data, m_expressionStack);
    }

    }

    // Unknown opcode.
    return false;
}

template<typename Context>
bool FunctionParser<Context>::parseUnreachableExpression(OpType op)
{
    ASSERT(m_unreachableBlocks);
    switch (op) {
    case OpType::Else: {
        if (m_unreachableBlocks > 1)
            return true;

        ControlType& data = m_controlStack.last();
        ASSERT(data.type() == BlockType::If);
        m_unreachableBlocks = 0;
        return m_context.addElse(data);
    }

    case OpType::End: {
        if (m_unreachableBlocks == 1) {
            ControlType data = m_controlStack.takeLast();
            if (!m_context.isContinuationReachable(data))
                return true;
        }
        m_unreachableBlocks--;
        return true;
    }

    case OpType::Loop:
    case OpType::If:
    case OpType::Block: {
        m_unreachableBlocks++;
        return true;
    }

    // two immediate cases
    case OpType::Branch:
    case OpType::BranchIf: {
        uint32_t unused;
        if (!parseVarUInt32(unused))
            return false;
        return parseVarUInt32(unused);
    }

    // one immediate cases
    case OpType::Return:
    case OpType::I32Const:
    case OpType::SetLocal:
    case OpType::GetLocal: {
        uint32_t unused;
        return parseVarUInt32(unused);
    }

    default:
        break;
    }
    return true;
}

#undef CREATE_CASE

} } // namespace JSC::WASM

#endif // ENABLE(WEBASSEMBLY)
