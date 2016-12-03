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

#include "WasmParser.h"
#include <wtf/DataLog.h>

namespace JSC { namespace Wasm {

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
    typedef typename Context::ExpressionList ExpressionList;

    FunctionParser(Context&, const uint8_t* functionStart, size_t functionLength, const Signature*, const Vector<FunctionInformation>& functions);

    bool WARN_UNUSED_RETURN parse();

    struct ControlEntry {
        ExpressionList enclosedExpressionStack;
        ControlType controlData;
    };

private:
    static const bool verbose = false;

    bool WARN_UNUSED_RETURN parseBody();
    bool WARN_UNUSED_RETURN parseExpression(OpType);
    bool WARN_UNUSED_RETURN parseUnreachableExpression(OpType);
    bool WARN_UNUSED_RETURN addReturn();
    bool WARN_UNUSED_RETURN unifyControl(Vector<ExpressionType>&, unsigned level);

    bool WARN_UNUSED_RETURN popExpressionStack(ExpressionType& result);

    template<OpType>
    bool WARN_UNUSED_RETURN unaryCase();

    template<OpType>
    bool WARN_UNUSED_RETURN binaryCase();

    void setErrorMessage(String&& message) { m_context.setErrorMessage(WTFMove(message)); }

    Context& m_context;
    ExpressionList m_expressionStack;
    Vector<ControlEntry> m_controlStack;
    const Signature* m_signature;
    const Vector<FunctionInformation>& m_functions;
    unsigned m_unreachableBlocks { 0 };
};

template<typename Context>
FunctionParser<Context>::FunctionParser(Context& context, const uint8_t* functionStart, size_t functionLength, const Signature* signature, const Vector<FunctionInformation>& functions)
    : Parser(functionStart, functionLength)
    , m_context(context)
    , m_signature(signature)
    , m_functions(functions)
{
    if (verbose)
        dataLogLn("Parsing function starting at: ", (uintptr_t)functionStart, " of length: ", functionLength);
}

template<typename Context>
bool FunctionParser<Context>::parse()
{
    if (!m_context.addArguments(m_signature->arguments))
        return false;

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

        if (!m_context.addLocal(typeOfLocal, numberOfLocals))
            return false;
    }

    return parseBody();
}

template<typename Context>
bool FunctionParser<Context>::parseBody()
{
    while (true) {
        uint8_t op;
        if (!parseUInt8(op) || !isValidOpType(op)) {
            if (verbose)
                WTF::dataLogLn("attempted to decode invalid op: ", RawPointer(reinterpret_cast<void*>(op)), " at offset: ", RawPointer(reinterpret_cast<void*>(m_offset)));
            return false;
        }

        if (verbose) {
            dataLogLn("processing op (", m_unreachableBlocks, "): ",  RawPointer(reinterpret_cast<void*>(op)), " at offset: ", RawPointer(reinterpret_cast<void*>(m_offset)));
            m_context.dump(m_controlStack, m_expressionStack);
        }

        if (op == OpType::End && !m_controlStack.size())
            return m_unreachableBlocks ? true : addReturn();

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

    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Context>
bool FunctionParser<Context>::addReturn()
{
    ExpressionList returnValues;
    if (m_signature->returnType != Void) {
        ExpressionType returnValue;
        if (!popExpressionStack(returnValue))
            return false;
        returnValues.append(returnValue);
    }

    m_unreachableBlocks = 1;
    return m_context.addReturn(returnValues);
}

template<typename Context>
template<OpType op>
bool FunctionParser<Context>::binaryCase()
{
    ExpressionType right;
    if (!popExpressionStack(right))
        return false;

    ExpressionType left;
    if (!popExpressionStack(left))
        return false;

    ExpressionType result;
    if (!m_context.template addOp<op>(left, right, result))
        return false;
    m_expressionStack.append(result);
    return true;
}

template<typename Context>
template<OpType op>
bool FunctionParser<Context>::unaryCase()
{
    ExpressionType value;
    if (!popExpressionStack(value))
        return false;

    ExpressionType result;
    if (!m_context.template addOp<op>(value, result))
        return false;
    m_expressionStack.append(result);
    return true;
}

template<typename Context>
bool FunctionParser<Context>::parseExpression(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, b3op) case OpType::name: return binaryCase<OpType::name>();
    FOR_EACH_WASM_SIMPLE_BINARY_OP(CREATE_CASE)
#undef CREATE_CASE

    case OpType::F32ConvertUI64: return unaryCase<OpType::F32ConvertUI64>();
    case OpType::F64ConvertUI64: return unaryCase<OpType::F64ConvertUI64>();
#define CREATE_CASE(name, id, b3op) case OpType::name: return unaryCase<OpType::name>();
    FOR_EACH_WASM_SIMPLE_UNARY_OP(CREATE_CASE)
#undef CREATE_CASE

    case OpType::Select: {
        ExpressionType condition;
        if (!popExpressionStack(condition))
            return false;

        ExpressionType zero;
        if (!popExpressionStack(zero))
            return false;

        ExpressionType nonZero;
        if (!popExpressionStack(nonZero))
            return false;

        ExpressionType result;
        if (!m_context.addSelect(condition, nonZero, zero, result))
            return false;

        m_expressionStack.append(result);
        return true;
    }

#define CREATE_CASE(name, id, b3op) case OpType::name:
    FOR_EACH_WASM_MEMORY_LOAD_OP(CREATE_CASE) {
        uint32_t alignment;
        if (!parseVarUInt32(alignment))
            return false;

        uint32_t offset;
        if (!parseVarUInt32(offset))
            return false;

        ExpressionType pointer;
        if (!popExpressionStack(pointer))
            return false;

        ExpressionType result;
        if (!m_context.load(static_cast<LoadOpType>(op), pointer, result, offset))
            return false;
        m_expressionStack.append(result);
        return true;
    }

    FOR_EACH_WASM_MEMORY_STORE_OP(CREATE_CASE) {
        uint32_t alignment;
        if (!parseVarUInt32(alignment))
            return false;

        uint32_t offset;
        if (!parseVarUInt32(offset))
            return false;

        ExpressionType value;
        if (!popExpressionStack(value))
            return false;

        ExpressionType pointer;
        if (!popExpressionStack(pointer))
            return false;

        return m_context.store(static_cast<StoreOpType>(op), pointer, value, offset);
    }
#undef CREATE_CASE

    case OpType::F32Const:
    case OpType::I32Const: {
        uint32_t constant;
        if (!parseVarUInt32(constant))
            return false;
        m_expressionStack.append(m_context.addConstant(I32, constant));
        return true;
    }

    case OpType::F64Const:
    case OpType::I64Const: {
        uint64_t constant;
        if (!parseVarUInt64(constant))
            return false;
        m_expressionStack.append(m_context.addConstant(I64, constant));
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
        ExpressionType value;
        if (!popExpressionStack(value))
            return false;
        return m_context.setLocal(index, value);
    }

    case OpType::Call: {
        uint32_t functionIndex;
        if (!parseVarUInt32(functionIndex))
            return false;

        if (functionIndex >= m_functions.size())
            return false;

        const FunctionInformation& info = m_functions[functionIndex];

        if (info.signature->arguments.size() > m_expressionStack.size())
            return false;

        size_t firstArgumentIndex = m_expressionStack.size() - info.signature->arguments.size();
        Vector<ExpressionType> args;
        args.reserveInitialCapacity(info.signature->arguments.size());
        for (unsigned i = firstArgumentIndex; i < m_expressionStack.size(); ++i)
            args.append(m_expressionStack[i]);
        m_expressionStack.shrink(firstArgumentIndex);

        ExpressionType result = Context::emptyExpression;
        if (!m_context.addCall(functionIndex, info, args, result))
            return false;

        if (result != Context::emptyExpression)
            m_expressionStack.append(result);

        return true;
    }

    case OpType::Block: {
        Type inlineSignature;
        if (!parseResultType(inlineSignature))
            return false;

        m_controlStack.append({ WTFMove(m_expressionStack), m_context.addBlock(inlineSignature) });
        m_expressionStack = ExpressionList();
        return true;
    }

    case OpType::Loop: {
        Type inlineSignature;
        if (!parseResultType(inlineSignature))
            return false;

        m_controlStack.append({ WTFMove(m_expressionStack), m_context.addLoop(inlineSignature) });
        m_expressionStack = ExpressionList();
        return true;
    }

    case OpType::If: {
        Type inlineSignature;
        if (!parseResultType(inlineSignature))
            return false;

        ExpressionType condition;
        if (!popExpressionStack(condition))
            return false;

        ControlType control;
        if (!m_context.addIf(condition, inlineSignature, control))
            return false;

        m_controlStack.append({ WTFMove(m_expressionStack), control });
        m_expressionStack = ExpressionList();
        return true;
    }

    case OpType::Else: {
        if (!m_controlStack.size()) {
            setErrorMessage("Attempted to use else block at the top-level of a function");
            return false;
        }

        if (!m_context.addElse(m_controlStack.last().controlData, m_expressionStack))
            return false;
        m_expressionStack.shrink(0);
        return true;
    }

    case OpType::Br:
    case OpType::BrIf: {
        uint32_t target;
        if (!parseVarUInt32(target) || target >= m_controlStack.size())
            return false;

        ExpressionType condition = Context::emptyExpression;
        if (op == OpType::BrIf) {
            if (!popExpressionStack(condition))
                return false;
        } else
            m_unreachableBlocks = 1;

        ControlType& data = m_controlStack[m_controlStack.size() - 1 - target].controlData;

        return m_context.addBranch(data, condition, m_expressionStack);
    }

    case OpType::BrTable: {
        uint32_t numberOfTargets;
        if (!parseVarUInt32(numberOfTargets))
            return false;

        Vector<ControlType*> targets;
        if (!targets.tryReserveCapacity(numberOfTargets))
            return false;

        for (uint32_t i = 0; i < numberOfTargets; ++i) {
            uint32_t target;
            if (!parseVarUInt32(target))
                return false;

            if (target >= m_controlStack.size())
                return false;

            targets.uncheckedAppend(&m_controlStack[m_controlStack.size() - 1 - target].controlData);
        }

        uint32_t defaultTarget;
        if (!parseVarUInt32(defaultTarget))
            return false;

        if (defaultTarget >= m_controlStack.size())
            return false;

        ExpressionType condition;
        if (!popExpressionStack(condition))
            return false;
        
        if (!m_context.addSwitch(condition, targets, m_controlStack[m_controlStack.size() - 1 - defaultTarget].controlData, m_expressionStack))
            return false;

        m_unreachableBlocks = 1;
        return true;
    }

    case OpType::Return: {
        return addReturn();
    }

    case OpType::End: {
        ControlEntry data = m_controlStack.takeLast();
        // FIXME: This is a little weird in that it will modify the expressionStack for the result of the block.
        // That's a little too effectful for me but I don't have a better API right now.
        // see: https://bugs.webkit.org/show_bug.cgi?id=164353
        if (!m_context.endBlock(data, m_expressionStack))
            return false;
        m_expressionStack.swap(data.enclosedExpressionStack);
        return true;
    }

    case OpType::Unreachable: {
        m_unreachableBlocks = 1;
        return true;
    }

    default: {
        // FIXME: Not yet implemented.
        return false;
    }
    }

    ASSERT_NOT_REACHED();
}

template<typename Context>
bool FunctionParser<Context>::parseUnreachableExpression(OpType op)
{
    ASSERT(m_unreachableBlocks);
    switch (op) {
    case OpType::Else: {
        if (m_unreachableBlocks > 1)
            return true;

        ControlEntry& data = m_controlStack.last();
        m_unreachableBlocks = 0;
        if (!m_context.addElseToUnreachable(data.controlData))
            return false;
        m_expressionStack.shrink(0);
        return true;
    }

    case OpType::End: {
        if (m_unreachableBlocks == 1) {
            ControlEntry data = m_controlStack.takeLast();
            if (!m_context.addEndToUnreachable(data))
                return false;
            m_expressionStack.swap(data.enclosedExpressionStack);
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
    case OpType::Br:
    case OpType::BrIf: {
        uint32_t unused;
        if (!parseVarUInt32(unused))
            return false;
        return parseVarUInt32(unused);
    }

    // one immediate cases
    case OpType::F32Const:
    case OpType::I32Const:
    case OpType::F64Const:
    case OpType::I64Const:
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

template<typename Context>
bool FunctionParser<Context>::popExpressionStack(ExpressionType& result)
{
    if (m_expressionStack.size()) {
        result = m_expressionStack.takeLast();
        return true;
    }

    setErrorMessage("Attempted to use a stack value when none existed");
    return false;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
