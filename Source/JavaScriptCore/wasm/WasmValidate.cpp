/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "WasmSignature.h"
#include <wtf/CommaPrinter.h>

namespace JSC { namespace Wasm {

class Validate {
public:
    class ControlData {
    public:
        ControlData() = default;
        ControlData(BlockType type, BlockSignature signature)
            : m_blockType(type)
            , m_signature(signature)
        {
        }

        void dump(PrintStream& out) const
        {
            switch (blockType()) {
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
            const Signature& sig = *signature();
            out.print(sig);
        }

        BlockType blockType() const { return m_blockType; }
        BlockSignature signature() const { return m_signature; }

        unsigned branchTargetArity() const { return blockType() == BlockType::Loop ? signature()->argumentCount() : signature()->returnCount(); }
        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            return blockType() == BlockType::Loop ? signature()->argument(i) : signature()->returnType(i);
        }

    private:
        BlockType m_blockType;
        BlockSignature m_signature;
    };

    typedef String ErrorType;
    typedef Unexpected<ErrorType> UnexpectedResult;
    typedef Expected<void, ErrorType> Result;
    using ExpressionType = Type;
    using ExpressionList = Vector<ExpressionType, 1>;
    using Stack = ExpressionList;
    typedef ControlData ControlType;
    typedef ExpressionList ResultList;
    typedef FunctionParser<Validate>::ControlEntry ControlEntry;

    static constexpr ExpressionType emptyExpression() { return Void; }
    Stack createStack() { return Stack(); }

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(const Args&... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        if (UNLIKELY(!ASSERT_DISABLED && Options::crashOnFailedWebAssemblyValidate()))
            WTFBreakpointTrap();

        StringPrintStream out;
        out.print("WebAssembly.Module doesn't validate: "_s, args...);
        return UnexpectedResult(out.toString());
    }
#define WASM_VALIDATOR_FAIL_IF(condition, ...) do { \
        if (UNLIKELY(condition))                    \
            return fail(__VA_ARGS__);               \
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
    ControlData WARN_UNUSED_RETURN addTopLevel(BlockSignature);
    Result WARN_UNUSED_RETURN addBlock(BlockSignature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack);
    Result WARN_UNUSED_RETURN addLoop(BlockSignature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex);
    Result WARN_UNUSED_RETURN addIf(ExpressionType condition, BlockSignature, Stack& enclosingStack, ControlType& result, Stack& newStack);
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
    Result WARN_UNUSED_RETURN addCall(unsigned calleeIndex, const Signature&, const Vector<ExpressionType>& args, ResultList&);
    Result WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const Signature&, const Vector<ExpressionType>& args, ResultList&);

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
    WASM_VALIDATOR_FAIL_IF(Type::I32 != index, "table.get index to type ", makeString(index), " expected ", makeString(Type::I32));

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
    WASM_VALIDATOR_FAIL_IF(Type::I32 != delta, "table.grow expects an i32 delta value, got ", makeString(delta));

    return { };
}

auto Validate::addTableFill(unsigned tableIndex, ExpressionType& offset, ExpressionType& fill, ExpressionType& count) -> Result
{
    WASM_VALIDATOR_FAIL_IF(tableIndex >= m_module.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_module.tableCount());
    WASM_VALIDATOR_FAIL_IF(!isSubtype(fill, m_module.tables[tableIndex].wasmType()), "table.fill expects fill value of type ", m_module.tables[tableIndex].wasmType(), " got ", fill);
    WASM_VALIDATOR_FAIL_IF(Type::I32 != offset, "table.fill expects an i32 offset value, got ", makeString(offset));
    WASM_VALIDATOR_FAIL_IF(Type::I32 != count, "table.fill expects an i32 count value, got ", makeString(count));

    return { };
}

auto Validate::addRefIsNull(ExpressionType& value, ExpressionType& result) -> Result
{
    result = Type::I32;
    WASM_VALIDATOR_FAIL_IF(!isSubtype(value, Type::Anyref), "ref.is_null to type ", makeString(value), " expected ", makeString(Type::Anyref));

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

Validate::ControlType Validate::addTopLevel(BlockSignature signature)
{
    return ControlData(BlockType::TopLevel, signature);
}

static Validate::Stack splitStack(BlockSignature signature, Validate::Stack& stack)
{
    Validate::Stack result;
    result.reserveInitialCapacity(signature->argumentCount());
    ASSERT(stack.size() >= signature->argumentCount());
    unsigned offset = stack.size() - signature->argumentCount();
    for (unsigned i = 0; i < signature->argumentCount(); ++i)
        result.uncheckedAppend(stack[i + offset]);
    stack.shrink(offset);
    return result;
}

auto Validate::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> Result
{
    WASM_VALIDATOR_FAIL_IF(enclosingStack.size() < signature->argumentCount(), "Too few values on stack for block. Block expects ", signature->argumentCount(), ", but only ", enclosingStack.size(), " were present. Block has signature: ", *signature);
    newStack = splitStack(signature, enclosingStack);
    newBlock = ControlData(BlockType::Block, signature);
    for (unsigned i = 0; i < signature->argumentCount(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(newStack[i], signature->argument(i)), "Loop expects the argument at index", i, " to be a subtype of ", signature->argument(i), " but argument has type ", newStack[i]);
    return { };
}

auto Validate::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& loop, Stack& newStack, uint32_t) -> Result
{
    WASM_VALIDATOR_FAIL_IF(enclosingStack.size() < signature->argumentCount(), "Too few values on stack for loop block. Loop expects ", signature->argumentCount(), ", but only ", enclosingStack.size(), " were present. Loop has signature: ", *signature);
    newStack = splitStack(signature, enclosingStack);
    loop = ControlData(BlockType::Loop, signature);
    for (unsigned i = 0; i < signature->argumentCount(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(newStack[i], signature->argument(i)), "Loop expects the argument at index", i, " to be a subtype of ", signature->argument(i), " but argument has type ", newStack[i]);
    return { };
}

auto Validate::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(condition != I32, "select condition must be i32, got ", condition);
    WASM_VALIDATOR_FAIL_IF(nonZero != zero, "select result types must match, got ", nonZero, " and ", zero);
    result = zero;
    return { };
}

auto Validate::addIf(ExpressionType condition, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> Result
{
    WASM_VALIDATOR_FAIL_IF(condition != I32, "if condition must be i32, got ", makeString(condition));
    WASM_VALIDATOR_FAIL_IF(enclosingStack.size() < signature->argumentCount(), "Too few arguments on stack for if block. If expects ", signature->argumentCount(), ", but only ", enclosingStack.size(), " were present. If block has signature: ", *signature);
    newStack = splitStack(signature, enclosingStack);
    result = ControlData(BlockType::If, signature);
    for (unsigned i = 0; i < signature->argumentCount(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(newStack[i], signature->argument(i)), "Loop expects the argument at index", i, " to be a subtype of ", signature->argument(i), " but argument has type ", newStack[i]);
    return { };
}

auto Validate::addElse(ControlType& current, const Stack& values) -> Result
{
    WASM_FAIL_IF_HELPER_FAILS(unify(values, current));
    return addElseToUnreachable(current);
}

auto Validate::addElseToUnreachable(ControlType& current) -> Result
{
    WASM_VALIDATOR_FAIL_IF(current.blockType() != BlockType::If, "else block isn't associated to an if");
    current = ControlData(BlockType::Block, current.signature());
    return { };
}

auto Validate::addReturn(ControlType& topLevel, const ExpressionList& returnValues) -> Result
{
    ASSERT(topLevel.blockType() == BlockType::TopLevel);
    return checkBranchTarget(topLevel, returnValues);
}

auto Validate::checkBranchTarget(ControlType& target, const Stack& expressionStack) -> Result
{
    if (!target.branchTargetArity())
        return { };

    WASM_VALIDATOR_FAIL_IF(expressionStack.size() < target.branchTargetArity(), target.blockType() == BlockType::TopLevel ? "branch out of function" : "branch to block", " on expression stack of size ", expressionStack.size(), ", but block, ", target , " expects ", target.branchTargetArity(), " values");


    unsigned expressionStackOffset = expressionStack.size() - target.branchTargetArity();
    for (unsigned i = 0; i < target.branchTargetArity(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(target.branchTargetType(i), expressionStack[expressionStackOffset + i]), "branch's stack type is not a subtype of block's type branch target type. Stack value has type", expressionStack[expressionStackOffset + i], " but branch target expects a value with subtype of ", target.branchTargetType(i), " at index ", i);

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

    for (unsigned i = 0; i < targets.size(); ++i) {
        auto* target = targets[i];
        WASM_VALIDATOR_FAIL_IF(defaultTarget.branchTargetArity() != target->branchTargetArity(), "br_table target type size mismatch. Default has size: ", defaultTarget.branchTargetArity(), "but target: ", i, " has size: ", target->branchTargetArity());
        for (unsigned type = 0; type < defaultTarget.branchTargetArity(); ++type)
            WASM_VALIDATOR_FAIL_IF(!isSubtype(defaultTarget.branchTargetType(type), target->branchTargetType(type)), "br_table target type mismatch at offset ", type, " expected: ", defaultTarget.branchTargetType(type), " but saw: ", target->branchTargetType(type), " when targeting block", *target);
    }

    return checkBranchTarget(defaultTarget, expressionStack);
}

auto Validate::addGrowMemory(ExpressionType delta, ExpressionType& result) -> Result
{
    WASM_VALIDATOR_FAIL_IF(delta != I32, "grow_memory with non-i32 delta argument has type: ", delta);
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
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(block.blockType() != BlockType::If);

    for (unsigned i = 0; i < block.signature()->returnCount(); ++i)
        entry.enclosedExpressionStack.append(block.signature()->returnType(i));
    return { };
}

auto Validate::addCall(unsigned, const Signature& signature, const Vector<ExpressionType>& args, ResultList& results) -> Result
{
    RELEASE_ASSERT(signature.argumentCount() == args.size());

    for (unsigned i = 0; i < args.size(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(args[i], signature.argument(i)), "argument type mismatch in call, got ", args[i], ", expected ", signature.argument(i));

    for (unsigned i = 0; i < signature.returnCount(); ++i)
        results.append(signature.returnType(i));
    return { };
}

auto Validate::addCallIndirect(unsigned tableIndex, const Signature& signature, const Vector<ExpressionType>& args, ResultList& results) -> Result
{
    RELEASE_ASSERT(tableIndex < m_module.tableCount());
    RELEASE_ASSERT(m_module.tables[tableIndex].type() == TableElementType::Funcref);
    const auto argumentCount = signature.argumentCount();
    RELEASE_ASSERT(argumentCount == args.size() - 1);

    for (unsigned i = 0; i < argumentCount; ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(args[i], signature.argument(i)), "argument type mismatch in call_indirect, got ", args[i], ", expected ", signature.argument(i));

    WASM_VALIDATOR_FAIL_IF(args.last() != I32, "non-i32 call_indirect index ", args.last());

    for (unsigned i = 0; i < signature.returnCount(); ++i)
        results.append(signature.returnType(i));
    return { };
}

auto Validate::unify(const Stack& values, const ControlType& block) -> Result
{
    WASM_VALIDATOR_FAIL_IF(block.signature()->returnCount() != values.size(), " block with type: ", *block.signature(), " returns: ", block.signature()->returnCount(), " but stack has: ", values.size(), " values");

    for (unsigned i = 0; i < block.signature()->returnCount(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(values[i], block.signature()->returnType(i)), "control flow returns with unexpected type. ", values[i], " is not a subtype of ", block.signature()->returnType(i));

    // WASM_VALIDATOR_FAIL_IF(values.size() != 1, "block with type: ", makeString(*block.signature()), " ends with a stack containing more than one value");
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

Expected<void, String> validateFunction(const FunctionData& function, const Signature& signature, const ModuleInformation& module)
{
    Validate context(module);
    FunctionParser<Validate> validator(context, function.data.data(), function.data.size(), signature, module);
    WASM_FAIL_IF_HELPER_FAILS(validator.parse());
    return { };
}

} } // namespace JSC::Wasm

#include "WasmValidateInlines.h"

#endif // ENABLE(WEBASSEMBLY)
