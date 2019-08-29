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

#include "JSCJSValueInlines.h"
#include "WasmFunctionParser.h"
#include <wtf/CommaPrinter.h>

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
                out.print("If:       ");
                break;
            case BlockType::Block:
                out.print("Block:    ");
                break;
            case BlockType::Loop:
                out.print("Loop:     ");
                break;
            case BlockType::TopLevel:
                out.print("TopLevel: ");
                break;
            }
            out.print(makeString(signature()));
        }

        bool hasNonVoidSignature() const { return m_signature != Void; }

        BlockType type() const { return m_blockType; }
        Type signature() const { return m_signature; }
        Type branchTargetSignature() const { return type() == BlockType::Loop ? Void : signature(); }
    private:
        BlockType m_blockType;
        Type m_signature;
    };
    typedef String ErrorType;
    typedef Unexpected<ErrorType> UnexpectedResult;
    typedef Expected<void, ErrorType> Result;
    using ExpressionType = Type;
    using ExpressionList = Vector<ExpressionType, 1>;
    using Stack = ExpressionList;
    typedef ControlData ControlType;
    typedef FunctionParser<Validate>::ControlEntry ControlEntry;

    static constexpr ExpressionType emptyExpression() { return Void; }
    Stack createStack() { return Stack(); }

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module doesn't validate: "_s, makeString(args)...));
    }
#define WASM_VALIDATOR_FAIL_IF(condition, ...) do { \
        if (UNLIKELY(condition))                    \
        return fail(__VA_ARGS__);                   \
    } while (0)

    Result WARN_UNUSED_RETURN addArguments(const Signature&);
    Result WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type type, uint64_t) { return type; }

    // References
    Result WARN_UNUSED_RETURN addRefIsNull(ExpressionType& value, ExpressionType& result);
    Result WARN_UNUSED_RETURN addRefFunc(uint32_t index, ExpressionType& result);

    // Tables
    Result WARN_UNUSED_RETURN addTableGet(unsigned, ExpressionType& index, ExpressionType& result);
    Result WARN_UNUSED_RETURN addTableSet(unsigned, ExpressionType& index, ExpressionType& value);
    Result WARN_UNUSED_RETURN addTableSize(unsigned, ExpressionType& result);
    Result WARN_UNUSED_RETURN addTableGrow(unsigned, ExpressionType& fill, ExpressionType& delta, ExpressionType& result);
    Result WARN_UNUSED_RETURN addTableFill(unsigned, ExpressionType& offset, ExpressionType& fill, ExpressionType& count);
    // Locals
    Result WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType& result);
    Result WARN_UNUSED_RETURN setLocal(uint32_t index, ExpressionType value);

    // Globals
    Result WARN_UNUSED_RETURN getGlobal(uint32_t index, ExpressionType& result);
    Result WARN_UNUSED_RETURN setGlobal(uint32_t index, ExpressionType value);

    // Memory
    Result WARN_UNUSED_RETURN load(LoadOpType, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    Result WARN_UNUSED_RETURN store(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    // Basic operators
    template<OpType>
    Result WARN_UNUSED_RETURN addOp(ExpressionType arg, ExpressionType& result);
    template<OpType>
    Result WARN_UNUSED_RETURN addOp(ExpressionType left, ExpressionType right, ExpressionType& result);
    Result WARN_UNUSED_RETURN addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result);

    // Control flow
    ControlData WARN_UNUSED_RETURN addTopLevel(Type signature);
    ControlData WARN_UNUSED_RETURN addBlock(Type signature);
    ControlData WARN_UNUSED_RETURN addLoop(Type signature, const Stack&, uint32_t);
    Result WARN_UNUSED_RETURN addIf(ExpressionType condition, Type signature, ControlData& result);
    Result WARN_UNUSED_RETURN addElse(ControlData&, const Stack&);
    Result WARN_UNUSED_RETURN addElseToUnreachable(ControlData&);

    Result WARN_UNUSED_RETURN addReturn(ControlData& topLevel, const Stack& returnValues);
    Result WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const Stack& expressionStack);
    Result WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const Stack& expressionStack);
    Result WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    Result WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&);
    Result WARN_UNUSED_RETURN addGrowMemory(ExpressionType delta, ExpressionType& result);
    Result WARN_UNUSED_RETURN addCurrentMemory(ExpressionType& result);

    Result WARN_UNUSED_RETURN addUnreachable() { return { }; }

    // Calls
    Result WARN_UNUSED_RETURN addCall(unsigned calleeIndex, const Signature&, const Vector<ExpressionType>& args, ExpressionType& result);
    Result WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const Signature&, const Vector<ExpressionType>& args, ExpressionType& result);

    ALWAYS_INLINE void didKill(ExpressionType) { }

    bool hasMemory() const { return !!m_module.memory; }

    Validate(const ModuleInformation& module)
        : m_module(module)
    {
    }

    void dump(const Vector<ControlEntry>&, const Stack*);
    void setParser(FunctionParser<Validate>*) { }

private:
    Result WARN_UNUSED_RETURN unify(const Stack&, const ControlData&);

    Result WARN_UNUSED_RETURN checkBranchTarget(ControlData& target, const Stack& expressionStack);

    Vector<Type> m_locals;
    const ModuleInformation& m_module;
};

auto Validate::addArguments(const Signature& signature) -> Result
{
    for (size_t i = 0; i < signature.argumentCount(); ++i)
        WASM_FAIL_IF_HELPER_FAILS(addLocal(signature.argument(i), 1));
    return { };
}

auto Validate::addTableGet(unsigned tableIndex, ExpressionType& index, ExpressionType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(tableIndex >= m_module.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_module.tableCount());
    result = m_module.tables[tableIndex].wasmType();
    WASM_VALIDATOR_FAIL_IF(Type::I32 != index, "table.get index to type ", index, " expected ", Type::I32);

    return { };
}

auto Validate::addTableSet(unsigned tableIndex, ExpressionType& index, ExpressionType& value) -> Result
{
    WASM_VALIDATOR_FAIL_IF(tableIndex >= m_module.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_module.tableCount());
    auto type = m_module.tables[tableIndex].wasmType();
    WASM_VALIDATOR_FAIL_IF(Type::I32 != index, "table.set index to type ", index, " expected ", Type::I32);
    WASM_VALIDATOR_FAIL_IF(!isSubtype(value, type), "table.set value to type ", value, " expected ", type);
    RELEASE_ASSERT(m_module.tables[tableIndex].type() == TableElementType::Anyref || m_module.tables[tableIndex].type() == TableElementType::Funcref);

    return { };
}

auto Validate::addTableSize(unsigned tableIndex, ExpressionType& result) -> Result
{
    result = Type::I32;
    WASM_VALIDATOR_FAIL_IF(tableIndex >= m_module.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_module.tableCount());

    return { };
}

auto Validate::addTableGrow(unsigned tableIndex, ExpressionType& fill, ExpressionType& delta, ExpressionType& result) -> Result
{
    result = Type::I32;
    WASM_VALIDATOR_FAIL_IF(tableIndex >= m_module.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_module.tableCount());
    WASM_VALIDATOR_FAIL_IF(!isSubtype(fill, m_module.tables[tableIndex].wasmType()), "table.grow expects fill value of type ", m_module.tables[tableIndex].wasmType(), " got ", fill);
    WASM_VALIDATOR_FAIL_IF(Type::I32 != delta, "table.grow expects an i32 delta value, got ", delta);

    return { };
}

auto Validate::addTableFill(unsigned tableIndex, ExpressionType& offset, ExpressionType& fill, ExpressionType& count) -> Result
{
    WASM_VALIDATOR_FAIL_IF(tableIndex >= m_module.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_module.tableCount());
    WASM_VALIDATOR_FAIL_IF(!isSubtype(fill, m_module.tables[tableIndex].wasmType()), "table.fill expects fill value of type ", m_module.tables[tableIndex].wasmType(), " got ", fill);
    WASM_VALIDATOR_FAIL_IF(Type::I32 != offset, "table.fill expects an i32 offset value, got ", offset);
    WASM_VALIDATOR_FAIL_IF(Type::I32 != count, "table.fill expects an i32 count value, got ", count);

    return { };
}

auto Validate::addRefIsNull(ExpressionType& value, ExpressionType& result) -> Result
{
    result = Type::I32;
    WASM_VALIDATOR_FAIL_IF(!isSubtype(value, Type::Anyref), "ref.is_null to type ", value, " expected ", Type::Anyref);

    return { };
}

auto Validate::addRefFunc(uint32_t index, ExpressionType& result) -> Result
{
    result = Type::Funcref;
    WASM_VALIDATOR_FAIL_IF(index >= m_module.functionIndexSpaceSize(), "ref.func index ", index, " is too large, max is ", m_module.functionIndexSpaceSize());
    m_module.addReferencedFunction(index);

    return { };
}

auto Validate::addLocal(Type type, uint32_t count) -> Result
{
    size_t newSize = m_locals.size() + count;
    ASSERT(!(CheckedUint32(count) + m_locals.size()).hasOverflowed());
    ASSERT(newSize <= maxFunctionLocals);
    WASM_VALIDATOR_FAIL_IF(!m_locals.tryReserveCapacity(newSize), "can't allocate memory for ", newSize, " locals");

    for (uint32_t i = 0; i < count; ++i)
        m_locals.uncheckedAppend(type);
    return { };
}

auto Validate::getLocal(uint32_t index, ExpressionType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(index >= m_locals.size(), "attempt to use unknown local ", index, " last one is ", m_locals.size());
    result = m_locals[index];
    return { };
}

auto Validate::setLocal(uint32_t index, ExpressionType value) -> Result
{
    ExpressionType localType;
    WASM_FAIL_IF_HELPER_FAILS(getLocal(index, localType));
    WASM_VALIDATOR_FAIL_IF(!isSubtype(value, localType), "set_local to type ", value, " expected ", localType);
    return { };
}

auto Validate::getGlobal(uint32_t index, ExpressionType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(index >= m_module.globals.size(), "get_global ", index, " of unknown global, limit is ", m_module.globals.size());
    result = m_module.globals[index].type;
    ASSERT(isValueType(result));
    return { };
}

auto Validate::setGlobal(uint32_t index, ExpressionType value) -> Result
{
    WASM_VALIDATOR_FAIL_IF(index >= m_module.globals.size(), "set_global ", index, " of unknown global, limit is ", m_module.globals.size());
    WASM_VALIDATOR_FAIL_IF(m_module.globals[index].mutability == Global::Immutable, "set_global ", index, " is immutable");

    ExpressionType globalType = m_module.globals[index].type;
    ASSERT(isValueType(globalType));
    WASM_VALIDATOR_FAIL_IF(globalType != value, "set_global ", index, " with type ", globalType, " with a variable of type ", value);
    return { };
}

Validate::ControlType Validate::addTopLevel(Type signature)
{
    return ControlData(BlockType::TopLevel, signature);
}

Validate::ControlType Validate::addBlock(Type signature)
{
    return ControlData(BlockType::Block, signature);
}

Validate::ControlType Validate::addLoop(Type signature, const Stack&, uint32_t)
{
    return ControlData(BlockType::Loop, signature);
}

auto Validate::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(condition != I32, "select condition must be i32, got ", condition);
    WASM_VALIDATOR_FAIL_IF(nonZero != zero, "select result types must match, got ", nonZero, " and ", zero);
    result = zero;
    return { };
}

auto Validate::addIf(ExpressionType condition, Type signature, ControlType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(condition != I32, "if condition must be i32, got ", condition);
    result = ControlData(BlockType::If, signature);
    return { };
}

auto Validate::addElse(ControlType& current, const Stack& values) -> Result
{
    WASM_FAIL_IF_HELPER_FAILS(unify(values, current));
    return addElseToUnreachable(current);
}

auto Validate::addElseToUnreachable(ControlType& current) -> Result
{
    WASM_VALIDATOR_FAIL_IF(current.type() != BlockType::If, "else block isn't associated to an if");
    current = ControlData(BlockType::Block, current.signature());
    return { };
}

auto Validate::addReturn(ControlType& topLevel, const ExpressionList& returnValues) -> Result
{
    ASSERT(topLevel.type() == BlockType::TopLevel);
    if (topLevel.signature() == Void)
        return { };
    ASSERT(returnValues.size() == 1);
    WASM_VALIDATOR_FAIL_IF(topLevel.signature() != returnValues[0], "return type ", returnValues[0], " doesn't match function's return type ", topLevel.signature());
    return { };
}

auto Validate::checkBranchTarget(ControlType& target, const Stack& expressionStack) -> Result
{
    if (target.branchTargetSignature() == Void)
        return { };

    WASM_VALIDATOR_FAIL_IF(expressionStack.isEmpty(), target.type() == BlockType::TopLevel ? "branch out of function" : "branch to block", " on empty expression stack, but expected ", target.signature());
    WASM_VALIDATOR_FAIL_IF(target.branchTargetSignature() != expressionStack.last(), "branch's stack type doesn't match block's type");

    return { };
}

auto Validate::addBranch(ControlType& target, ExpressionType condition, const Stack& stack) -> Result
{
    // Void means this is an unconditional branch.
    WASM_VALIDATOR_FAIL_IF(condition != Void && condition != I32, "conditional branch with non-i32 condition ", condition);
    return checkBranchTarget(target, stack);
}

auto Validate::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const Stack& expressionStack) -> Result
{
    WASM_VALIDATOR_FAIL_IF(condition != I32, "br_table with non-i32 condition ", condition);

    for (auto target : targets)
        WASM_VALIDATOR_FAIL_IF(defaultTarget.branchTargetSignature() != target->branchTargetSignature(), "br_table target type mismatch");

    return checkBranchTarget(defaultTarget, expressionStack);
}

auto Validate::addGrowMemory(ExpressionType delta, ExpressionType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(delta != I32, "grow_memory with non-i32 delta");
    result = I32;
    return { };
}

auto Validate::addCurrentMemory(ExpressionType& result) -> Result
{
    result = I32;
    return { };
}

auto Validate::endBlock(ControlEntry& entry, Stack& stack) -> Result
{
    WASM_FAIL_IF_HELPER_FAILS(unify(stack, entry.controlData));
    return addEndToUnreachable(entry);
}

auto Validate::addEndToUnreachable(ControlEntry& entry) -> Result
{
    auto block = entry.controlData;
    if (block.signature() != Void) {
        WASM_VALIDATOR_FAIL_IF(block.type() == BlockType::If, "If-block had a non-void result type: ", block.signature(), " but had no else-block");
        entry.enclosedExpressionStack.append(block.signature());
    }
    return { };
}

auto Validate::addCall(unsigned, const Signature& signature, const Vector<ExpressionType>& args, ExpressionType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(signature.argumentCount() != args.size(), "arity mismatch in call, got ", args.size(), " arguments, expected ", signature.argumentCount());

    for (unsigned i = 0; i < args.size(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(args[i], signature.argument(i)), "argument type mismatch in call, got ", args[i], ", expected ", signature.argument(i));

    result = signature.returnType();
    return { };
}

auto Validate::addCallIndirect(unsigned tableIndex, const Signature& signature, const Vector<ExpressionType>& args, ExpressionType& result) -> Result
{
    RELEASE_ASSERT(tableIndex < m_module.tableCount());
    RELEASE_ASSERT(m_module.tables[tableIndex].type() == TableElementType::Funcref);
    const auto argumentCount = signature.argumentCount();
    WASM_VALIDATOR_FAIL_IF(argumentCount != args.size() - 1, "arity mismatch in call_indirect, got ", args.size() - 1, " arguments, expected ", argumentCount);

    for (unsigned i = 0; i < argumentCount; ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(args[i], signature.argument(i)), "argument type mismatch in call_indirect, got ", args[i], ", expected ", signature.argument(i));

    WASM_VALIDATOR_FAIL_IF(args.last() != I32, "non-i32 call_indirect index ", args.last());

    result = signature.returnType();
    return { };
}

auto Validate::unify(const Stack& values, const ControlType& block) -> Result
{
    if (block.signature() == Void) {
        WASM_VALIDATOR_FAIL_IF(!values.isEmpty(), "void block should end with an empty stack");
        return { };
    }

    WASM_VALIDATOR_FAIL_IF(values.size() != 1, "block with type: ", block.signature(), " ends with a stack containing more than one value");
    WASM_VALIDATOR_FAIL_IF(!isSubtype(values[0], block.signature()), "control flow returns with unexpected type");
    return { };
}

static void dumpExpressionStack(const CommaPrinter& comma, const Validate::Stack& expressionStack)
{
    dataLog(comma, " ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, makeString(expression));
}

void Validate::dump(const Vector<ControlEntry>& controlStack, const Stack* expressionStack)
{
    for (size_t i = controlStack.size(); i--;) {
        dataLog("  ", controlStack[i].controlData);
        CommaPrinter comma(", ", "");
        dumpExpressionStack(comma, *expressionStack);
        expressionStack = &controlStack[i].enclosedExpressionStack;
        dataLogLn();
    }
    dataLogLn();
}

Expected<void, String> validateFunction(const uint8_t* source, size_t length, const Signature& signature, const ModuleInformation& module)
{
    Validate context(module);
    FunctionParser<Validate> validator(context, source, length, signature, module);
    WASM_FAIL_IF_HELPER_FAILS(validator.parse());
    return { };
}

} } // namespace JSC::Wasm

#include "WasmValidateInlines.h"

#endif // ENABLE(WEBASSEMBLY)
