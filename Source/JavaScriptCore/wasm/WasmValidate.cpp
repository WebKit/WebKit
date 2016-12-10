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
#include "WasmValidate.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmFunctionParser.h"
#include <wtf/CommaPrinter.h>
#include <wtf/text/StringBuilder.h>

namespace JSC { namespace Wasm {

class Validate {
public:
    class ControlData {
    public:
        ControlData(BlockType type, Type signature)
            : m_blockType(type)
            , m_signature(signature)
        {
        }

        ControlData()
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
        }

        bool hasNonVoidSignature() const { return m_signature != Void; }

        BlockType type() const { return m_blockType; }
        Type signature() const { return m_signature; }
    private:
        BlockType m_blockType;
        Type m_signature;
    };
    typedef Type ExpressionType;
    typedef ControlData ControlType;
    typedef Vector<ExpressionType, 1> ExpressionList;
    typedef FunctionParser<Validate>::ControlEntry ControlEntry;

    static const ExpressionType emptyExpression = Void;

    bool WARN_UNUSED_RETURN addArguments(const Vector<Type>&);
    bool WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type type, uint64_t) { return type; }

    // Locals
    bool WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType& result);
    bool WARN_UNUSED_RETURN setLocal(uint32_t index, ExpressionType value);

    // Memory
    bool WARN_UNUSED_RETURN load(LoadOpType, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    bool WARN_UNUSED_RETURN store(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    // Basic operators
    template<OpType>
    bool WARN_UNUSED_RETURN addOp(ExpressionType arg, ExpressionType& result);
    template<OpType>
    bool WARN_UNUSED_RETURN addOp(ExpressionType left, ExpressionType right, ExpressionType& result);
    bool WARN_UNUSED_RETURN addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result);

    // Control flow
    ControlData WARN_UNUSED_RETURN addBlock(Type signature);
    ControlData WARN_UNUSED_RETURN addLoop(Type signature);
    bool WARN_UNUSED_RETURN addIf(ExpressionType condition, Type signature, ControlData& result);
    bool WARN_UNUSED_RETURN addElse(ControlData&, const ExpressionList&);
    bool WARN_UNUSED_RETURN addElseToUnreachable(ControlData&);

    bool WARN_UNUSED_RETURN addReturn(const ExpressionList& returnValues);
    bool WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const ExpressionList& expressionStack);
    bool WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const ExpressionList& expressionStack);
    bool WARN_UNUSED_RETURN endBlock(ControlEntry&, ExpressionList& expressionStack);
    bool WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&);

    // Calls
    bool WARN_UNUSED_RETURN addCall(unsigned calleeIndex, const Signature*, const Vector<ExpressionType>& args, ExpressionType& result);
    bool WARN_UNUSED_RETURN addCallIndirect(const Signature*, const Vector<ExpressionType>& args, ExpressionType& result);

    void dump(const Vector<ControlEntry>& controlStack, const ExpressionList& expressionStack);

    bool hasMemory() const { return !!m_memory; }

    void setErrorMessage(String&& message) { ASSERT(m_errorMessage.isNull()); m_errorMessage = WTFMove(message); }
    String errorMessage() const { return m_errorMessage; }
    Validate(ExpressionType returnType, const MemoryInformation& memory)
        : m_returnType(returnType)
        , m_memory(memory)
    {
    }

private:
    bool unify(Type, Type);
    bool unify(const ExpressionList&, const ControlData&);

    bool checkBranchTarget(ControlData& target, const ExpressionList& expressionStack);

    ExpressionType m_returnType;
    Vector<Type> m_locals;
    String m_errorMessage;
    const MemoryInformation& m_memory;
};

bool Validate::addArguments(const Vector<Type>& args)
{
    for (Type arg : args) {
        if (!addLocal(arg, 1))
            return false;
    }
    return true;
}

bool Validate::addLocal(Type type, uint32_t count)
{
    if (!m_locals.tryReserveCapacity(m_locals.size() + count))
        return false;

    for (uint32_t i = 0; i < count; ++i)
        m_locals.uncheckedAppend(type);
    return true;
}

bool Validate::getLocal(uint32_t index, ExpressionType& result)
{
    if (index < m_locals.size()) {
        result = m_locals[index];
        return true;
    }
    m_errorMessage = ASCIILiteral("Attempt to use unknown local.");
    return false;
}

bool Validate::setLocal(uint32_t index, ExpressionType value)
{
    ExpressionType localType;
    if (!getLocal(index, localType))
        return false;

    if (localType == value)
        return true;

    m_errorMessage = makeString("Attempt to set local with type: ", toString(localType), " with a variable of type: ", toString(value));
    return false;
}

Validate::ControlType Validate::addBlock(Type signature)
{
    return ControlData(BlockType::Block, signature);
}

Validate::ControlType Validate::addLoop(Type signature)
{
    return ControlData(BlockType::Loop, signature);
}

bool Validate::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result)
{
    if (condition != I32) {
        m_errorMessage = makeString("Attempting to use ", toString(condition), " as the condition for select");
        return false;
    }

    if (nonZero != zero) {
        m_errorMessage = makeString("Result types of select don't match. Got: ", toString(nonZero), " and ", toString(zero));
        return false;
    }

    result = zero;
    return true;
}

bool Validate::addIf(ExpressionType condition, Type signature, ControlType& result)
{
    if (condition != I32) {
        m_errorMessage = makeString("Attempting to use ", toString(condition), " as the condition for an if block");
        return false;
    }
    result = ControlData(BlockType::If, signature);
    return true;
}

bool Validate::addElse(ControlType& current, const ExpressionList& values)
{
    if (!unify(values, current)) {
        ASSERT(errorMessage());
        return false;
    }

    return addElseToUnreachable(current);
}

bool Validate::addElseToUnreachable(ControlType& current)
{
    if (current.type() != BlockType::If) {
        m_errorMessage = makeString("Attempting to add else block to something other than an if");
        return false;
    }

    current = ControlData(BlockType::Block, current.signature());
    return true;
}

bool Validate::addReturn(const ExpressionList& returnValues)
{
    if (m_returnType == Void)
        return true;
    ASSERT(returnValues.size() == 1);

    if (m_returnType == returnValues[0])
        return true;

    m_errorMessage = makeString("Attempting to add return with type: ", toString(returnValues[0]), " but function expects return with type: ", toString(m_returnType));
    return false;
}

bool Validate::checkBranchTarget(ControlType& target, const ExpressionList& expressionStack)
    {
        if (target.type() == BlockType::Loop)
            return true;

        if (target.signature() == Void)
            return true;

        if (!expressionStack.size()) {
            m_errorMessage = makeString("Attempting to branch to block with expected type: ", toString(target.signature()), " but the stack was empty");
            return false;
        }

        if (target.signature() == expressionStack.last())
            return true;

        m_errorMessage = makeString("Attempting to branch to block with expected type: ", toString(target.signature()), " but stack has type: ", toString(target.signature()));
        return false;
    }

bool Validate::addBranch(ControlType& target, ExpressionType condition, const ExpressionList& stack)
{
    // Void means this is an unconditional branch.
    if (condition != Void && condition != I32) {
        m_errorMessage = makeString("Attempting to add a conditional branch with condition type: ", toString(condition), " but expected i32.");
        return false;
    }

    return checkBranchTarget(target, stack);
}

bool Validate::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const ExpressionList& expressionStack)
{
    if (condition != I32) {
        m_errorMessage = makeString("Attempting to add a br_table with condition type: ", toString(condition), " but expected i32.");
        return false;
    }

    for (auto target : targets) {
        if (defaultTarget.signature() != target->signature()) {
            m_errorMessage = makeString("Attempting to add a br_table with different expected types. Default target has type: ", toString(defaultTarget.signature()), " but case has type: ", toString(target->signature()));
            return false;
        }
    }

    return checkBranchTarget(defaultTarget, expressionStack);
}

bool Validate::endBlock(ControlEntry& entry, ExpressionList& stack)
{
    ControlData& block = entry.controlData;
    if (block.signature() == Void)
        return true;

    if (!stack.size()) {
        m_errorMessage = makeString("Block fallthough expected type: ", toString(block.signature()), " but the stack was empty");
        return false;
    }

    if (block.signature() == stack.last()) {
        entry.enclosedExpressionStack.append(block.signature());
        return true;
    }

    m_errorMessage = makeString("Block fallthrough has expected type: ", toString(block.signature()), " but produced type: ", toString(block.signature()));
    return false;
}

bool Validate::addEndToUnreachable(ControlEntry& entry)
{
    if (entry.controlData.signature() != Void)
        entry.enclosedExpressionStack.append(entry.controlData.signature());
    return true;
}

bool Validate::addCall(unsigned, const Signature* signature, const Vector<ExpressionType>& args, ExpressionType& result)
{
    if (signature->arguments.size() != args.size()) {
        StringBuilder builder;
        builder.append("Arity mismatch in call, expected: ");
        builder.appendNumber(signature->arguments.size());
        builder.append(" but got: ");
        builder.appendNumber(args.size());
        m_errorMessage = builder.toString();
        return false;
    }

    for (unsigned i = 0; i < args.size(); ++i) {
        if (args[i] != signature->arguments[i]) {
            m_errorMessage = makeString("Expected argument type: ", toString(signature->arguments[i]), " does not match passed argument type: ", toString(args[i]));
            return false;
        }
    }

    result = signature->returnType;
    return true;
}

bool Validate::addCallIndirect(const Signature* signature, const Vector<ExpressionType>& args, ExpressionType& result)
{
    const auto argumentCount = signature->arguments.size();
    if (argumentCount != args.size() - 1) {
        StringBuilder builder;
        builder.append("Arity mismatch in call_indirect, expected: ");
        builder.appendNumber(signature->arguments.size());
        builder.append(" but got: ");
        builder.appendNumber(args.size());
        m_errorMessage = builder.toString();
        return false;
    }

    for (unsigned i = 0; i < argumentCount; ++i) {
        if (args[i] != signature->arguments[i]) {
            m_errorMessage = makeString("Expected argument type: ", toString(signature->arguments[i]), " does not match passed argument type: ", toString(args[i]));
            return false;
        }
    }

    if (args.last() != I32) {
        m_errorMessage = makeString("Expected call_indirect target index to have type: i32 but got type: ", toString(args.last()));
        return false;
    }
    
    result = signature->returnType;
    return true;
}

bool Validate::unify(const ExpressionList& values, const ControlType& block)
{
    ASSERT(values.size() <= 1);
    if (block.signature() == Void)
        return true;

    if (!values.size()) {
        m_errorMessage = makeString("Block has non-void signature but has no stack entries on exit");
        return false;
    }

    if (values[0] == block.signature())
        return true;

    m_errorMessage = makeString("Expected control flow to return value with type: ", toString(block.signature()), " but got value with type: ", toString(values[0]));
    return false;
}

void Validate::dump(const Vector<ControlEntry>&, const ExpressionList&)
{
    // If you need this then you should fix the validator's error messages instead...
    // Think of this as penance for the sin of bad error messages.
}

String validateFunction(const uint8_t* source, size_t length, const Signature* signature, const ImmutableFunctionIndexSpace& functionIndexSpace, const ModuleInformation& info)
{
    Validate context(signature->returnType, info.memory);
    FunctionParser<Validate> validator(context, source, length, signature, functionIndexSpace, info);

    if (!validator.parse()) {
        // FIXME: add better location information here. see: https://bugs.webkit.org/show_bug.cgi?id=164288
        // FIXME: We should never not have an error message if we return false.
        // see: https://bugs.webkit.org/show_bug.cgi?id=164354
        if (context.errorMessage().isNull())
            return "Unknown error";
        return context.errorMessage();
    }

    return String();
}

} } // namespace JSC::Wasm

#include "WasmValidateInlines.h"

#endif // ENABLE(WEBASSEMBLY)
