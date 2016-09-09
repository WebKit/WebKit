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
#include "B3FixSSA.h"
#include "B3Validate.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "VirtualRegister.h"
#include "WASMCallingConvention.h"
#include "WASMFunctionParser.h"
#include <wtf/Optional.h>

void dumpProcedure(void* ptr)
{
    JSC::B3::Procedure* proc = static_cast<JSC::B3::Procedure*>(ptr);
    proc->dump(WTF::dataFile());
}

namespace JSC { namespace WASM {

namespace {

using namespace B3;

const bool verbose = false;

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
private:
    class LazyBlock {
    public:
        LazyBlock(BasicBlock* block)
            : m_block(block)
        {
        }

        explicit operator bool() const { return !!m_block; }

        BasicBlock* get(Procedure& proc)
        {
            if (!m_block)
                m_block = proc.addBlock();
            return m_block;
        }

        void dump(PrintStream& out) const
        {
            if (m_block)
                out.print(*m_block);
            else
                out.print("Uninitialized");
        }

    private:
        BasicBlock* m_block { nullptr };
    };

public:
    struct ControlData {
        ControlData(Optional<Vector<Variable*>>&& stack, BasicBlock* special = nullptr, BasicBlock* continuation = nullptr)
            : continuation(continuation)
            , special(special)
            , stack(stack)
        {
        }

        void dump(PrintStream& out) const
        {
            switch (type()) {
            case BlockType::If:
                out.print("If:    ");
                break;
            case BlockType::Block:
                out.print("Block: ");
                break;
            case BlockType::Loop:
                out.print("Loop:  ");
                break;
            }
            out.print("Continuation: ", continuation, ", Special: ");
            if (special)
                out.print(*special);
            else
                out.print("None");
        }

        BlockType type() const
        {
            if (!special)
                return BlockType::Block;
            if (continuation)
                return BlockType::If;
            return BlockType::Loop;
        }

        BasicBlock* targetBlockForBranch(Procedure& proc)
        {
            if (special)
                return special;
            return continuation.get(proc);
        }

    private:
        friend class B3IRGenerator;
        // We use a LazyBlock for the continuation since B3::validate does not like orphaned blocks. Note,
        // it's possible to create an orphaned block by doing something like (block (return (...))). In
        // that example, if we eagerly allocate a BasicBlock for the continuation it will never be reachable.
        LazyBlock continuation;
        BasicBlock* special;
        Optional<Vector<Variable*>> stack;
    };

    typedef Value* ExpressionType;
    typedef ControlData ControlType;
    static constexpr ExpressionType emptyExpression = nullptr;

    B3IRGenerator(Procedure&);

    void addArguments(const Vector<Type>&);
    void addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

    bool WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType& result);
    bool WARN_UNUSED_RETURN setLocal(uint32_t index, ExpressionType value);

    bool WARN_UNUSED_RETURN binaryOp(BinaryOpType, ExpressionType left, ExpressionType right, ExpressionType& result);
    bool WARN_UNUSED_RETURN unaryOp(UnaryOpType, ExpressionType arg, ExpressionType& result);

    ControlData WARN_UNUSED_RETURN addBlock();
    ControlData WARN_UNUSED_RETURN addLoop();
    ControlData WARN_UNUSED_RETURN addIf(ExpressionType condition);
    bool WARN_UNUSED_RETURN addElse(ControlData&);

    bool WARN_UNUSED_RETURN addReturn(const Vector<ExpressionType, 1>& returnValues);
    bool WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const Vector<ExpressionType, 1>& returnValues);
    bool WARN_UNUSED_RETURN endBlock(ControlData&, Vector<ExpressionType, 1>& expressionStack);

    bool isContinuationReachable(ControlData&);

    void dump(const Vector<ControlType>& controlStack, const Vector<ExpressionType, 1>& expressionStack);

private:
    void unify(Variable* target, const ExpressionType source);
    Vector<Variable*> initializeIncommingTypes(BasicBlock*, const Vector<ExpressionType, 1>&);
    void unifyValuesWithBlock(const Vector<ExpressionType, 1>& resultStack, Optional<Vector<Variable*>>& stack, BasicBlock* target);

    Procedure& m_proc;
    BasicBlock* m_currentBlock;
    Vector<Variable*> m_locals;
};

B3IRGenerator::B3IRGenerator(Procedure& procedure)
    : m_proc(procedure)
{
    m_currentBlock = m_proc.addBlock();
}

void B3IRGenerator::addLocal(Type type, uint32_t count)
{
    m_locals.reserveCapacity(m_locals.size() + count);
    for (uint32_t i = 0; i < count; ++i)
        m_locals.append(m_proc.addVariable(toB3Type(type)));
}

void B3IRGenerator::addArguments(const Vector<Type>& types)
{
    ASSERT(!m_locals.size());
    m_locals.grow(types.size());
    jscCallingConvention().iterate(types, m_proc, m_currentBlock, Origin(),
        [&] (ExpressionType argument, unsigned i) {
            Variable* argumentVariable = m_proc.addVariable(argument->type());
            m_locals[i] = argumentVariable;
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), argumentVariable, argument);
        });
}

bool WARN_UNUSED_RETURN B3IRGenerator::getLocal(uint32_t index, ExpressionType& result)
{
    ASSERT(m_locals[index]);
    result = m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, Origin(), m_locals[index]);
    return true;
}

bool WARN_UNUSED_RETURN B3IRGenerator::setLocal(uint32_t index, ExpressionType value)
{
    ASSERT(m_locals[index]);
    m_currentBlock->appendNew<VariableValue>(m_proc, B3::Set, Origin(), m_locals[index], value);
    return true;
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

B3IRGenerator::ControlData B3IRGenerator::addBlock()
{
    return ControlData(Nullopt);
}

B3IRGenerator::ControlData B3IRGenerator::addLoop()
{
    BasicBlock* body = m_proc.addBlock();
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), body);
    body->addPredecessor(m_currentBlock);
    m_currentBlock = body;
    return ControlData(Vector<Variable*>(), body);
}

B3IRGenerator::ControlData B3IRGenerator::addIf(ExpressionType condition)
{
    // FIXME: This needs to do some kind of stack passing.

    BasicBlock* taken = m_proc.addBlock();
    BasicBlock* notTaken = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    m_currentBlock->appendNew<Value>(m_proc, B3::Branch, Origin(), condition);
    m_currentBlock->setSuccessors(FrequentedBlock(taken), FrequentedBlock(notTaken));
    taken->addPredecessor(m_currentBlock);
    notTaken->addPredecessor(m_currentBlock);

    m_currentBlock = taken;
    return ControlData(Nullopt, notTaken, continuation);
}

bool B3IRGenerator::addElse(ControlData& data)
{
    ASSERT(data.continuation);
    m_currentBlock = data.special;
    // Clear the special pointer so that when we parse the end we don't think that this block is an if block.
    data.special = nullptr;
    ASSERT(data.type() == BlockType::Block);
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

bool B3IRGenerator::addBranch(ControlData& data, ExpressionType condition, const Vector<ExpressionType, 1>& returnValues)
{
    BasicBlock* target = data.targetBlockForBranch(m_proc);
    unifyValuesWithBlock(returnValues, data.stack, target);
    if (condition) {
        BasicBlock* continuation = m_proc.addBlock();
        m_currentBlock->appendNew<Value>(m_proc, B3::Branch, Origin(), condition);
        m_currentBlock->setSuccessors(FrequentedBlock(target), FrequentedBlock(continuation));
        target->addPredecessor(m_currentBlock);
        continuation->addPredecessor(m_currentBlock);
        m_currentBlock = continuation;
    } else {
        m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), FrequentedBlock(target));
        target->addPredecessor(m_currentBlock);
    }

    return true;
}

bool B3IRGenerator::endBlock(ControlData& data, Vector<ExpressionType, 1>& expressionStack)
{
    if (!data.continuation)
        return true;

    BasicBlock* continuation = data.continuation.get(m_proc);
    if (data.type() == BlockType::If) {
        ASSERT(!data.special->size() && !data.special->successors().size());
        // Since we don't have any else block we need to point the notTaken branch to the continuation.
        data.special->appendNewControlValue(m_proc, Jump, Origin());
        data.special->setSuccessors(FrequentedBlock(continuation));
        continuation->addPredecessor(data.special);
    }

    unifyValuesWithBlock(expressionStack, data.stack, continuation);
    m_currentBlock->appendNewControlValue(m_proc, Jump, Origin(), continuation);
    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = continuation;
    return true;
}

bool B3IRGenerator::isContinuationReachable(ControlData& data)
{
    // If nothing targets the continuation of the current block then we don't want to create
    // an orphaned BasicBlock since it can't be reached by fallthrough.
    if (!data.continuation)
        return false;

    m_currentBlock = data.continuation.get(m_proc);
    if (data.type() == BlockType::If) {
        data.special->appendNewControlValue(m_proc, Jump, Origin(), m_currentBlock);
        m_currentBlock->addPredecessor(data.special);
    }

    return true;
}

void B3IRGenerator::unify(Variable* variable, ExpressionType source)
{
    m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), variable, source);
}

Vector<Variable*> B3IRGenerator::initializeIncommingTypes(BasicBlock* block, const Vector<ExpressionType, 1>& source)
{
    Vector<Variable*> result;
    result.reserveInitialCapacity(source.size());
    for (ExpressionType expr : source) {
        ASSERT(expr->type() != B3::Void);
        Variable* var = m_proc.addVariable(expr->type());
        result.append(var);
        block->appendNew<VariableValue>(m_proc, B3::Get, Origin(), var);
    }

    return result;
}

void B3IRGenerator::unifyValuesWithBlock(const Vector<ExpressionType, 1>& resultStack, Optional<Vector<Variable*>>& stack, BasicBlock* target)
{
    if (!stack) {
        stack = initializeIncommingTypes(target, resultStack);
        return;
    }

    ASSERT(stack.value().size() == resultStack.size());

    for (size_t i = 0; i < resultStack.size(); ++i)
        unify(stack.value()[i], resultStack[i]);
}

void B3IRGenerator::dump(const Vector<ControlType>& controlStack, const Vector<ExpressionType, 1>& expressionStack)
{
    dataLogLn("Processing Graph:");
    dataLog(m_proc);
    dataLogLn("With current block:", *m_currentBlock);
    dataLogLn("Control stack:");
    for (const ControlType& data : controlStack)
        dataLogLn("  ", data);
    dataLogLn("ExpressionStack:");
    for (const ExpressionType& expression : expressionStack)
        dataLogLn("  ", *expression);
    dataLogLn("\n");
}

} // anonymous namespace

std::unique_ptr<Compilation> parseAndCompile(VM& vm, Vector<uint8_t>& source, FunctionInformation info, unsigned optLevel)
{
    Procedure procedure;
    B3IRGenerator context(procedure);
    FunctionParser<B3IRGenerator> parser(context, source, info);
    if (!parser.parse())
        RELEASE_ASSERT_NOT_REACHED();

    procedure.resetReachability();
    validate(procedure, "After parsing:\n");

    fixSSA(procedure);
    if (verbose)
        dataLog("Post SSA: ", procedure);
    return std::make_unique<Compilation>(vm, procedure, optLevel);
}

} } // namespace JSC::WASM

#endif // ENABLE(WEBASSEMBLY)
