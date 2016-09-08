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

#include "config.h"
#include "WASMB3IRGenerator.h"

#if ENABLE(WEBASSEMBLY)

#include "B3BasicBlockInlines.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "WASMFunctionParser.h"
#include <wtf/Optional.h>

namespace JSC {

namespace WASM {

using namespace B3;

inline B3::Opcode toB3Op(BinaryOpType op)
{
    switch (op) {
#define CREATE_CASE(name, op, b3op) case BinaryOpType::name: return b3op;
    FOR_EACH_WASM_BINARY_OP(CREATE_CASE)
#undef CREATE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline B3::Opcode toB3Op(UnaryOpType op)
{
    switch (op) {
#define CREATE_CASE(name, op, b3op) case UnaryOpType::name: return b3op;
    FOR_EACH_WASM_UNARY_OP(CREATE_CASE)
#undef CREATE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

class B3IRGenerator {
public:
    typedef Value* ExpressionType;

    B3IRGenerator(Procedure&);

    void addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

    bool WARN_UNUSED_RETURN binaryOp(BinaryOpType, ExpressionType left, ExpressionType right, ExpressionType& result);
    bool WARN_UNUSED_RETURN unaryOp(UnaryOpType, ExpressionType arg, ExpressionType& result);

    bool WARN_UNUSED_RETURN addBlock();
    bool WARN_UNUSED_RETURN endBlock(Vector<ExpressionType>& expressionStack);
    bool WARN_UNUSED_RETURN addReturn(const Vector<ExpressionType, 1>& returnValues);

private:
    Optional<Vector<Variable*>>& stackForControlLevel(unsigned);
    BasicBlock* blockForControlLevel(unsigned);
    void unify(Variable* target, const ExpressionType source);
    Vector<Variable*> initializeIncommingTypes(BasicBlock*, const Vector<ExpressionType>&);
    void unifyValuesWithLevel(const Vector<ExpressionType>& resultStack, unsigned);

    Procedure& m_proc;
    BasicBlock* m_currentBlock;
    // This is a pair of the continuation and the types expected on the stack for that continuation.
    Vector<std::pair<BasicBlock*, Optional<Vector<Variable*>>>> m_controlStack;
};

B3IRGenerator::B3IRGenerator(Procedure& procedure)
    : m_proc(procedure)
{
    m_currentBlock = m_proc.addBlock();
}

void B3IRGenerator::addLocal(Type, uint32_t)
{
    // TODO: Add locals.
}

bool B3IRGenerator::unaryOp(UnaryOpType op, ExpressionType arg, ExpressionType& result)
{
    result = m_currentBlock->appendNew<Value>(m_proc, toB3Op(op), Origin(), arg);
    return true;
}

bool B3IRGenerator::binaryOp(BinaryOpType op, ExpressionType left, ExpressionType right, ExpressionType& result)
{
    result = m_currentBlock->appendNew<Value>(m_proc, toB3Op(op), Origin(), left, right);
    return true;
}

B3IRGenerator::ExpressionType B3IRGenerator::addConstant(Type type, uint64_t value)
{
    switch (type) {
    case Int32:
        return m_currentBlock->appendNew<Const32Value>(m_proc, Origin(), static_cast<int32_t>(value));
    case Int64:
        return m_currentBlock->appendNew<Const64Value>(m_proc, Origin(), value);
    case Float:
        return m_currentBlock->appendNew<ConstFloatValue>(m_proc, Origin(), bitwise_cast<float>(static_cast<int32_t>(value)));
    case Double:
        return m_currentBlock->appendNew<ConstDoubleValue>(m_proc, Origin(), bitwise_cast<double>(value));
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

bool B3IRGenerator::addBlock()
{
    m_controlStack.append(std::make_pair(m_proc.addBlock(), Nullopt));
    return true;
}

bool B3IRGenerator::endBlock(Vector<ExpressionType>& expressionStack)
{
    // This means that we are exiting the function.
    if (!m_controlStack.size()) {
        // FIXME: Should this require the stack is empty? It's not clear from the current spec.
        return !expressionStack.size();
    }

    unifyValuesWithLevel(expressionStack, 0);

    m_currentBlock = m_controlStack.takeLast().first;
    return true;
}

bool B3IRGenerator::addReturn(const Vector<ExpressionType, 1>& returnValues)
{
    ASSERT(returnValues.size() <= 1);
    if (returnValues.size())
        m_currentBlock->appendNewControlValue(m_proc, B3::Return, Origin(), returnValues[0]);
    else
        m_currentBlock->appendNewControlValue(m_proc, B3::Return, Origin());
    return true;
}

void B3IRGenerator::unify(Variable* variable, ExpressionType source)
{
    m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), variable, source);
}

Vector<Variable*> B3IRGenerator::initializeIncommingTypes(BasicBlock* block, const Vector<ExpressionType>& source)
{
    Vector<Variable*> result;
    result.reserveInitialCapacity(source.size());
    for (ExpressionType expr : source) {
        ASSERT(expr->type() != Void);
        Variable* var = m_proc.addVariable(expr->type());
        result.append(var);
        block->appendNew<VariableValue>(m_proc, B3::Get, Origin(), var);
    }

    return result;
}

void B3IRGenerator::unifyValuesWithLevel(const Vector<ExpressionType>& resultStack, unsigned level)
{
    ASSERT(level < m_controlStack.size());

    Optional<Vector<Variable*>>& expectedStack = stackForControlLevel(level);
    if (!expectedStack) {
        expectedStack = initializeIncommingTypes(blockForControlLevel(level), resultStack);
        return;
    }

    ASSERT(expectedStack.value().size() != resultStack.size());

    for (size_t i = 0; i < resultStack.size(); ++i)
        unify(expectedStack.value()[i], resultStack[i]);
}
    
Optional<Vector<Variable*>>& B3IRGenerator::stackForControlLevel(unsigned level)
{
    return m_controlStack[m_controlStack.size() - 1 - level].second;
}

BasicBlock* B3IRGenerator::blockForControlLevel(unsigned level)
{
    return m_controlStack[m_controlStack.size() - 1 - level].first;
}

std::unique_ptr<Compilation> parseAndCompile(VM& vm, Vector<uint8_t>& source, FunctionInformation info, unsigned optLevel)
{
    Procedure procedure;
    B3IRGenerator context(procedure);
    FunctionParser<B3IRGenerator> parser(context, source, info);
    if (!parser.parse())
        RELEASE_ASSERT_NOT_REACHED();

    return std::make_unique<Compilation>(vm, procedure, optLevel);
}

} // namespace WASM

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
