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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "WasmParser.h"
#include "WasmSignatureInlines.h"
#include <wtf/DataLog.h>

namespace JSC { namespace Wasm {

enum class BlockType {
    If,
    Block,
    Loop,
    TopLevel
};

template<typename EnclosingStack, typename NewStack>
void splitStack(BlockSignature signature, EnclosingStack& enclosingStack, NewStack& newStack)
{
    newStack.reserveInitialCapacity(signature->argumentCount());
    ASSERT(enclosingStack.size() >= signature->argumentCount());
    unsigned offset = enclosingStack.size() - signature->argumentCount();
    for (unsigned i = 0; i < signature->argumentCount(); ++i)
        newStack.uncheckedAppend(enclosingStack.at(i + offset));
    enclosingStack.shrink(offset);
}

template<typename Context>
class FunctionParser : public Parser<void> {
public:
    struct ControlEntry;

    using ControlType = typename Context::ControlType;
    using ExpressionType = typename Context::ExpressionType;

    class TypedExpression {
    public:
        TypedExpression() = default;

        TypedExpression(Type type, ExpressionType value)
            : m_type(type)
            , m_value(value)
        {
        }

        Type type() const { return m_type; }

        ExpressionType value() const { return m_value; }
        operator ExpressionType() const { return m_value; }

        ExpressionType operator->() const { return m_value; }

    private:
        Type m_type;
        ExpressionType m_value;
    };

    using ControlStack = Vector<ControlEntry, 16>;
    using ResultList = Vector<ExpressionType, 8>;
    using Stack = Vector<TypedExpression, 16, UnsafeVectorOverflow>;

    struct ControlEntry {
        Stack enclosedExpressionStack;
        Stack elseBlockStack;
        ControlType controlData;
    };

    FunctionParser(Context&, const uint8_t* functionStart, size_t functionLength, const Signature&, const ModuleInformation&);

    Result WARN_UNUSED_RETURN parse();

    OpType currentOpcode() const { return m_currentOpcode; }
    size_t currentOpcodeStartingOffset() const { return m_currentOpcodeStartingOffset; }
    const Signature& signature() const { return m_signature; }

    ControlStack& controlStack() { return m_controlStack; }
    Stack& expressionStack() { return m_expressionStack; }

private:
    static constexpr bool verbose = false;

    PartialResult WARN_UNUSED_RETURN parseBody();
    PartialResult WARN_UNUSED_RETURN parseExpression();
    PartialResult WARN_UNUSED_RETURN parseUnreachableExpression();
    PartialResult WARN_UNUSED_RETURN unifyControl(Vector<ExpressionType>&, unsigned level);
    PartialResult WARN_UNUSED_RETURN checkBranchTarget(const ControlType&);
    PartialResult WARN_UNUSED_RETURN unify(const ControlType&);

#define WASM_TRY_POP_EXPRESSION_STACK_INTO(result, what) do {                               \
        WASM_PARSER_FAIL_IF(m_expressionStack.isEmpty(), "can't pop empty stack in " what); \
        result = m_expressionStack.takeLast();                                              \
        m_context.didPopValueFromStack();                                                   \
    } while (0)

    template<OpType>
    PartialResult WARN_UNUSED_RETURN unaryCase(Type returnType, Type operandType);

    template<OpType>
    PartialResult WARN_UNUSED_RETURN binaryCase(Type returnType, Type lhsType, Type rhsType);

    PartialResult WARN_UNUSED_RETURN store(Type memoryType);
    PartialResult WARN_UNUSED_RETURN load(Type memoryType);

#define WASM_TRY_ADD_TO_CONTEXT(add_expression) WASM_FAIL_IF_HELPER_FAILS(m_context.add_expression)

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN validationFail(const Args&... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        if (UNLIKELY(ASSERT_ENABLED && Options::crashOnFailedWebAssemblyValidate()))
            WTFBreakpointTrap();

        StringPrintStream out;
        out.print("WebAssembly.Module doesn't validate: "_s, args...);
        return UnexpectedResult(out.toString());
    }

#define WASM_VALIDATOR_FAIL_IF(condition, ...) do { \
        if (UNLIKELY(condition)) \
            return validationFail(__VA_ARGS__); \
    } while (0) \

    // FIXME add a macro as above for WASM_TRY_APPEND_TO_CONTROL_STACK https://bugs.webkit.org/show_bug.cgi?id=165862

    Context& m_context;
    Stack m_expressionStack;
    ControlStack m_controlStack;
    Vector<Type, 16> m_locals;
    const Signature& m_signature;
    const ModuleInformation& m_info;

    OpType m_currentOpcode;
    size_t m_currentOpcodeStartingOffset { 0 };

    unsigned m_unreachableBlocks { 0 };
    unsigned m_loopIndex { 0 };
};

template<typename Context>
FunctionParser<Context>::FunctionParser(Context& context, const uint8_t* functionStart, size_t functionLength, const Signature& signature, const ModuleInformation& info)
    : Parser(functionStart, functionLength)
    , m_context(context)
    , m_signature(signature)
    , m_info(info)
{
    if (verbose)
        dataLogLn("Parsing function starting at: ", (uintptr_t)functionStart, " of length: ", functionLength, " with signature: ", signature);
    m_context.setParser(this);
}

template<typename Context>
auto FunctionParser<Context>::parse() -> Result
{
    uint32_t localGroupsCount;

    WASM_PARSER_FAIL_IF(!m_context.addArguments(m_signature), "can't add ", m_signature.argumentCount(), " arguments to Function");
    WASM_PARSER_FAIL_IF(!parseVarUInt32(localGroupsCount), "can't get local groups count");

    WASM_PARSER_FAIL_IF(!m_locals.tryReserveCapacity(m_signature.argumentCount()), "can't allocate enough memory for function's ", m_signature.argumentCount(), " arguments");
    for (uint32_t i = 0; i < m_signature.argumentCount(); ++i)
        m_locals.uncheckedAppend(m_signature.argument(i));

    uint64_t totalNumberOfLocals = m_signature.argumentCount();
    for (uint32_t i = 0; i < localGroupsCount; ++i) {
        uint32_t numberOfLocals;
        Type typeOfLocal;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(numberOfLocals), "can't get Function's number of locals in group ", i);
        totalNumberOfLocals += numberOfLocals;
        WASM_PARSER_FAIL_IF(totalNumberOfLocals > maxFunctionLocals, "Function's number of locals is too big ", totalNumberOfLocals, " maximum ", maxFunctionLocals);
        WASM_PARSER_FAIL_IF(!parseValueType(typeOfLocal), "can't get Function local's type in group ", i);

        WASM_PARSER_FAIL_IF(!m_locals.tryReserveCapacity(totalNumberOfLocals), "can't allocate enough memory for function's ", totalNumberOfLocals, " locals");
        for (uint32_t i = 0; i < numberOfLocals; ++i)
            m_locals.uncheckedAppend(typeOfLocal);

        WASM_TRY_ADD_TO_CONTEXT(addLocal(typeOfLocal, numberOfLocals));
    }

    m_context.didFinishParsingLocals();

    WASM_FAIL_IF_HELPER_FAILS(parseBody());

    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseBody() -> PartialResult
{
    m_controlStack.append({ { }, { }, m_context.addTopLevel(&m_signature) });
    uint8_t op = 0;
    while (m_controlStack.size()) {
        m_currentOpcodeStartingOffset = m_offset;
        WASM_PARSER_FAIL_IF(!parseUInt8(op), "can't decode opcode");
        WASM_PARSER_FAIL_IF(!isValidOpType(op), "invalid opcode ", op);

        m_currentOpcode = static_cast<OpType>(op);

        if (verbose) {
            dataLogLn("processing op (", m_unreachableBlocks, "): ",  RawPointer(reinterpret_cast<void*>(op)), ", ", makeString(static_cast<OpType>(op)), " at offset: ", RawPointer(reinterpret_cast<void*>(m_offset)));
            m_context.dump(m_controlStack, &m_expressionStack);
        }

        if (m_unreachableBlocks)
            WASM_FAIL_IF_HELPER_FAILS(parseUnreachableExpression());
        else {
            WASM_FAIL_IF_HELPER_FAILS(parseExpression());
        }
    }
    WASM_FAIL_IF_HELPER_FAILS(m_context.endTopLevel(&m_signature, m_expressionStack));

    ASSERT(op == OpType::End);
    return { };
}

template<typename Context>
template<OpType op>
auto FunctionParser<Context>::binaryCase(Type returnType, Type lhsType, Type rhsType) -> PartialResult
{
    TypedExpression right;
    TypedExpression left;

    WASM_TRY_POP_EXPRESSION_STACK_INTO(right, "binary right");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(left, "binary left");

    WASM_VALIDATOR_FAIL_IF(left.type() != lhsType, op, " left value type mismatch");
    WASM_VALIDATOR_FAIL_IF(right.type() != rhsType, op, " right value type mismatch");

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(template addOp<op>(left, right, result));
    m_expressionStack.constructAndAppend(returnType, result);
    return { };
}

template<typename Context>
template<OpType op>
auto FunctionParser<Context>::unaryCase(Type returnType, Type operandType) -> PartialResult
{
    TypedExpression value;
    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "unary");

    WASM_VALIDATOR_FAIL_IF(value.type() != operandType, op, " value type mismatch");

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(template addOp<op>(value, result));
    m_expressionStack.constructAndAppend(returnType, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::load(Type memoryType) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(!m_info.memory, "load instruction without memory");

    uint32_t alignment;
    uint32_t offset;
    TypedExpression pointer;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get load alignment");
    WASM_PARSER_FAIL_IF(alignment > memoryLog2Alignment(m_currentOpcode), "byte alignment ", 1ull << alignment, " exceeds load's natural alignment ", 1ull << memoryLog2Alignment(m_currentOpcode));
    WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get load offset");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "load pointer");

    WASM_VALIDATOR_FAIL_IF(pointer.type() != I32, m_currentOpcode, " pointer type mismatch");

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(load(static_cast<LoadOpType>(m_currentOpcode), pointer, result, offset));
    m_expressionStack.constructAndAppend(memoryType, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::store(Type memoryType) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(!m_info.memory, "store instruction without memory");

    uint32_t alignment;
    uint32_t offset;
    TypedExpression value;
    TypedExpression pointer;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get store alignment");
    WASM_PARSER_FAIL_IF(alignment > memoryLog2Alignment(m_currentOpcode), "byte alignment ", 1ull << alignment, " exceeds store's natural alignment ", 1ull << memoryLog2Alignment(m_currentOpcode));
    WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get store offset");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "store value");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "store pointer");

    WASM_VALIDATOR_FAIL_IF(pointer.type() != I32, m_currentOpcode, " pointer type mismatch");
    WASM_VALIDATOR_FAIL_IF(value.type() != memoryType, m_currentOpcode, " value type mismatch");

    WASM_TRY_ADD_TO_CONTEXT(store(static_cast<StoreOpType>(m_currentOpcode), pointer, value, offset));
    return { };
}

template<typename Context>
auto FunctionParser<Context>::checkBranchTarget(const ControlType& target) -> PartialResult
{
    if (!target.branchTargetArity())
        return { };

    WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < target.branchTargetArity(), ControlType::isTopLevel(target) ? "branch out of function" : "branch to block", " on expression stack of size ", m_expressionStack.size(), ", but block, ", target.signature()->toString() , " expects ", target.branchTargetArity(), " values");


    unsigned offset = m_expressionStack.size() - target.branchTargetArity();
    for (unsigned i = 0; i < target.branchTargetArity(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(target.branchTargetType(i), m_expressionStack[offset + i].type()), "branch's stack type is not a subtype of block's type branch target type. Stack value has type", m_expressionStack[offset + i].type(), " but branch target expects a value with subtype of ", target.branchTargetType(i), " at index ", i);

    return { };
}

template<typename Context>
auto FunctionParser<Context>::unify(const ControlType& controlData) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(controlData.signature()->returnCount() != m_expressionStack.size(), " block with type: ", controlData.signature()->toString(), " returns: ", controlData.signature()->returnCount(), " but stack has: ", m_expressionStack.size(), " values");
    for (unsigned i = 0; i < controlData.signature()->returnCount(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(m_expressionStack[i].type(), controlData.signature()->returnType(i)), "control flow returns with unexpected type. ", m_expressionStack[i].type(), " is not a subtype of ", controlData.signature()->returnType(i));

    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseExpression() -> PartialResult
{
    switch (m_currentOpcode) {
#define CREATE_CASE(name, id, b3op, inc, lhsType, rhsType, returnType) case OpType::name: return binaryCase<OpType::name>(returnType, lhsType, rhsType);
    FOR_EACH_WASM_BINARY_OP(CREATE_CASE)
#undef CREATE_CASE

#define CREATE_CASE(name, id, b3op, inc, operandType, returnType) case OpType::name: return unaryCase<OpType::name>(returnType, operandType);
    FOR_EACH_WASM_UNARY_OP(CREATE_CASE)
#undef CREATE_CASE

    case Select: {
        TypedExpression condition;
        TypedExpression zero;
        TypedExpression nonZero;

        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "select condition");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(zero, "select zero");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(nonZero, "select non-zero");

        WASM_VALIDATOR_FAIL_IF(condition.type() != I32, "select condition must be i32, got ", condition.type());
        WASM_VALIDATOR_FAIL_IF(nonZero.type() != zero.type(), "select result types must match, got ", nonZero.type(), " and ", zero.type());

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addSelect(condition, nonZero, zero, result));

        m_expressionStack.constructAndAppend(zero.type(), result);
        return { };
    }

#define CREATE_CASE(name, id, b3op, inc, memoryType) case OpType::name: return load(memoryType);
FOR_EACH_WASM_MEMORY_LOAD_OP(CREATE_CASE)
#undef CREATE_CASE

#define CREATE_CASE(name, id, b3op, inc, memoryType) case OpType::name: return store(memoryType);
FOR_EACH_WASM_MEMORY_STORE_OP(CREATE_CASE)
#undef CREATE_CASE

    case F32Const: {
        uint32_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt32(constant), "can't parse 32-bit floating-point constant");
        m_expressionStack.constructAndAppend(F32, m_context.addConstant(F32, constant));
        return { };
    }

    case I32Const: {
        int32_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt32(constant), "can't parse 32-bit constant");
        m_expressionStack.constructAndAppend(I32, m_context.addConstant(I32, constant));
        return { };
    }

    case F64Const: {
        uint64_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt64(constant), "can't parse 64-bit floating-point constant");
        m_expressionStack.constructAndAppend(F64, m_context.addConstant(F64, constant));
        return { };
    }

    case I64Const: {
        int64_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt64(constant), "can't parse 64-bit constant");
        m_expressionStack.constructAndAppend(I64, m_context.addConstant(I64, constant));
        return { };
    }

    case TableGet: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyReferences(), "references are not enabled");
        unsigned tableIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't parse table index");
        WASM_VALIDATOR_FAIL_IF(tableIndex >= m_info.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_info.tableCount());

        TypedExpression index;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(index, "table.get");
        WASM_VALIDATOR_FAIL_IF(I32 != index.type(), "table.get index to type ", index.type(), " expected ", I32);

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addTableGet(tableIndex, index, result));
        Type resultType = m_info.tables[tableIndex].wasmType();
        m_expressionStack.constructAndAppend(resultType, result);
        return { };
    }

    case TableSet: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyReferences(), "references are not enabled");
        unsigned tableIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't parse table index");
        WASM_VALIDATOR_FAIL_IF(tableIndex >= m_info.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_info.tableCount());
        TypedExpression value, index;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "table.set");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(index, "table.set");
        WASM_VALIDATOR_FAIL_IF(I32 != index.type(), "table.set index to type ", index.type(), " expected ", I32);
        Type type = m_info.tables[tableIndex].wasmType();
        WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), type), "table.set value to type ", value.type(), " expected ", type);
        RELEASE_ASSERT(m_info.tables[tableIndex].type() == TableElementType::Anyref || m_info.tables[tableIndex].type() == TableElementType::Funcref);
        WASM_TRY_ADD_TO_CONTEXT(addTableSet(tableIndex, index, value));
        return { };
    }

    case ExtTable: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyReferences(), "references are not enabled");
        uint8_t extOp;
        WASM_PARSER_FAIL_IF(!parseUInt8(extOp), "can't parse table extended opcode");
        unsigned tableIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't parse table index");
        WASM_VALIDATOR_FAIL_IF(tableIndex >= m_info.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_info.tableCount());

        switch (static_cast<ExtTableOpType>(extOp)) {
        case ExtTableOpType::TableSize: {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addTableSize(tableIndex, result));
            m_expressionStack.constructAndAppend(I32, result);
            break;
        }
        case ExtTableOpType::TableGrow: {
            TypedExpression fill;
            TypedExpression delta;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(delta, "table.grow");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(fill, "table.grow");

            WASM_VALIDATOR_FAIL_IF(!isSubtype(fill.type(), m_info.tables[tableIndex].wasmType()), "table.grow expects fill value of type ", m_info.tables[tableIndex].wasmType(), " got ", fill.type());
            WASM_VALIDATOR_FAIL_IF(I32 != delta.type(), "table.grow expects an i32 delta value, got ", delta.type());

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addTableGrow(tableIndex, fill, delta, result));
            m_expressionStack.constructAndAppend(I32, result);
            break;
        }
        case ExtTableOpType::TableFill: {
            TypedExpression offset, fill, count;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(count, "table.fill");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(fill, "table.fill");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(offset, "table.fill");

            WASM_VALIDATOR_FAIL_IF(!isSubtype(fill.type(), m_info.tables[tableIndex].wasmType()), "table.fill expects fill value of type ", m_info.tables[tableIndex].wasmType(), " got ", fill.type());
            WASM_VALIDATOR_FAIL_IF(I32 != offset.type(), "table.fill expects an i32 offset value, got ", offset.type());
            WASM_VALIDATOR_FAIL_IF(I32 != count.type(), "table.fill expects an i32 count value, got ", count.type());

            WASM_TRY_ADD_TO_CONTEXT(addTableFill(tableIndex, offset, fill, count));
            break;
        }
        default:
            WASM_PARSER_FAIL_IF(true, "invalid extended table op ", extOp);
            break;
        }
        return { };
    }

    case RefNull: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyReferences(), "references are not enabled");
        m_expressionStack.constructAndAppend(Funcref, m_context.addConstant(Funcref, JSValue::encode(jsNull())));
        return { };
    }

    case RefIsNull: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyReferences(), "references are not enabled");
        TypedExpression value;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "ref.is_null");
        WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), Anyref), "ref.is_null to type ", value.type(), " expected ", Anyref);
        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addRefIsNull(value, result));
        m_expressionStack.constructAndAppend(I32, result);
        return { };
    }

    case RefFunc: {
        uint32_t index;
        ExpressionType result;
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyReferences(), "references are not enabled");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for ref.func");

        WASM_VALIDATOR_FAIL_IF(index >= m_info.functionIndexSpaceSize(), "ref.func index ", index, " is too large, max is ", m_info.functionIndexSpaceSize());
        m_info.addReferencedFunction(index);
        WASM_TRY_ADD_TO_CONTEXT(addRefFunc(index, result));
        m_expressionStack.constructAndAppend(Funcref, result);
        return { };
    }

    case GetLocal: {
        uint32_t index;
        ExpressionType result;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for get_local");
        WASM_VALIDATOR_FAIL_IF(index >= m_locals.size(), "attempt to use unknown local ", index, " last one is ", m_locals.size());
        WASM_TRY_ADD_TO_CONTEXT(getLocal(index, result));
        m_expressionStack.constructAndAppend(m_locals[index], result);
        return { };
    }

    case SetLocal: {
        uint32_t index;
        TypedExpression value;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for set_local");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "set_local");
        WASM_VALIDATOR_FAIL_IF(index >= m_locals.size(), "attempt to set unknown local ", index, " last one is ", m_locals.size());
        WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), m_locals[index]), "set_local to type ", value.type(), " expected ", m_locals[index]);
        WASM_TRY_ADD_TO_CONTEXT(setLocal(index, value));
        return { };
    }

    case TeeLocal: {
        uint32_t index;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for tee_local");
        WASM_PARSER_FAIL_IF(m_expressionStack.isEmpty(), "can't tee_local on empty expression stack");
        TypedExpression value = m_expressionStack.last();
        WASM_VALIDATOR_FAIL_IF(index >= m_locals.size(), "attempt to tee unknown local ", index, " last one is ", m_locals.size());
        WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), m_locals[index]), "set_local to type ", value.type(), " expected ", m_locals[index]);
        WASM_TRY_ADD_TO_CONTEXT(setLocal(index, value));
        return { };
    }

    case GetGlobal: {
        uint32_t index;
        ExpressionType result;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get get_global's index");
        WASM_VALIDATOR_FAIL_IF(index >= m_info.globals.size(), "get_global ", index, " of unknown global, limit is ", m_info.globals.size());
        Type resultType = m_info.globals[index].type;
        ASSERT(isValueType(resultType));
        WASM_TRY_ADD_TO_CONTEXT(getGlobal(index, result));
        m_expressionStack.constructAndAppend(resultType, result);
        return { };
    }

    case SetGlobal: {
        uint32_t index;
        TypedExpression value;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get set_global's index");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "set_global value");

        WASM_VALIDATOR_FAIL_IF(index >= m_info.globals.size(), "set_global ", index, " of unknown global, limit is ", m_info.globals.size());
        WASM_VALIDATOR_FAIL_IF(m_info.globals[index].mutability == GlobalInformation::Immutable, "set_global ", index, " is immutable");

        Type globalType = m_info.globals[index].type;
        ASSERT(isValueType(globalType));
        WASM_VALIDATOR_FAIL_IF(globalType != value.type(), "set_global ", index, " with type ", globalType, " with a variable of type ", value.type());

        WASM_TRY_ADD_TO_CONTEXT(setGlobal(index, value));
        return { };
    }

    case Call: {
        uint32_t functionIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't parse call's function index");
        WASM_PARSER_FAIL_IF(functionIndex >= m_info.functionIndexSpaceSize(), "call function index ", functionIndex, " exceeds function index space ", m_info.functionIndexSpaceSize());

        SignatureIndex calleeSignatureIndex = m_info.signatureIndexFromFunctionIndexSpace(functionIndex);
        const Signature& calleeSignature = SignatureInformation::get(calleeSignatureIndex);
        WASM_PARSER_FAIL_IF(calleeSignature.argumentCount() > m_expressionStack.size(), "call function index ", functionIndex, " has ", calleeSignature.argumentCount(), " arguments, but the expression stack currently holds ", m_expressionStack.size(), " values");

        size_t firstArgumentIndex = m_expressionStack.size() - calleeSignature.argumentCount();
        Vector<ExpressionType> args;
        WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(calleeSignature.argumentCount()), "can't allocate enough memory for call's ", calleeSignature.argumentCount(), " arguments");
        for (size_t i = firstArgumentIndex; i < m_expressionStack.size(); ++i) {
            TypedExpression arg = m_expressionStack.at(i);
            WASM_VALIDATOR_FAIL_IF(!isSubtype(arg.type(), calleeSignature.argument(i - firstArgumentIndex)), "argument type mismatch in call, got ", arg.type(), ", expected ", calleeSignature.argument(i - firstArgumentIndex));
            args.uncheckedAppend(arg);
            m_context.didPopValueFromStack();
        }
        m_expressionStack.shrink(firstArgumentIndex);

        RELEASE_ASSERT(calleeSignature.argumentCount() == args.size());

        ResultList results;
        WASM_TRY_ADD_TO_CONTEXT(addCall(functionIndex, calleeSignature, args, results));

        RELEASE_ASSERT(calleeSignature.returnCount() == results.size());

        for (unsigned i = 0; i < calleeSignature.returnCount(); ++i)
            m_expressionStack.constructAndAppend(calleeSignature.returnType(i), results[i]);

        return { };
    }

    case CallIndirect: {
        uint32_t signatureIndex;
        uint32_t tableIndex;
        WASM_PARSER_FAIL_IF(!m_info.tableCount(), "call_indirect is only valid when a table is defined or imported");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(signatureIndex), "can't get call_indirect's signature index");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't get call_indirect's table index");
        WASM_PARSER_FAIL_IF(tableIndex >= m_info.tableCount(), "call_indirect's table index ", tableIndex, " invalid, limit is ", m_info.tableCount());
        WASM_PARSER_FAIL_IF(m_info.usedSignatures.size() <= signatureIndex, "call_indirect's signature index ", signatureIndex, " exceeds known signatures ", m_info.usedSignatures.size());
        WASM_PARSER_FAIL_IF(m_info.tables[tableIndex].type() != TableElementType::Funcref, "call_indirect is only valid when a table has type funcref");

        const Signature& calleeSignature = m_info.usedSignatures[signatureIndex].get();
        size_t argumentCount = calleeSignature.argumentCount() + 1; // Add the callee's index.
        WASM_PARSER_FAIL_IF(argumentCount > m_expressionStack.size(), "call_indirect expects ", argumentCount, " arguments, but the expression stack currently holds ", m_expressionStack.size(), " values");

        WASM_VALIDATOR_FAIL_IF(m_expressionStack.last().type() != I32, "non-i32 call_indirect index ", m_expressionStack.last().type());

        Vector<ExpressionType> args;
        WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(argumentCount), "can't allocate enough memory for ", argumentCount, " call_indirect arguments");
        size_t firstArgumentIndex = m_expressionStack.size() - argumentCount;
        for (size_t i = firstArgumentIndex; i < m_expressionStack.size(); ++i) {
            TypedExpression arg = m_expressionStack.at(i);
            if (i < m_expressionStack.size() - 1)
                WASM_VALIDATOR_FAIL_IF(!isSubtype(arg.type(), calleeSignature.argument(i - firstArgumentIndex)), "argument type mismatch in call_indirect, got ", arg.type(), ", expected ", calleeSignature.argument(i - firstArgumentIndex));
            args.uncheckedAppend(arg);
            m_context.didPopValueFromStack();
        }
        m_expressionStack.shrink(firstArgumentIndex);

        ResultList results;
        WASM_TRY_ADD_TO_CONTEXT(addCallIndirect(tableIndex, calleeSignature, args, results));

        for (unsigned i = 0; i < calleeSignature.returnCount(); ++i)
            m_expressionStack.constructAndAppend(calleeSignature.returnType(i), results[i]);

        return { };
    }

    case Block: {
        BlockSignature inlineSignature;
        WASM_PARSER_FAIL_IF(!parseBlockSignature(m_info, inlineSignature), "can't get block's signature");

        WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < inlineSignature->argumentCount(), "Too few values on stack for block. Block expects ", inlineSignature->argumentCount(), ", but only ", m_expressionStack.size(), " were present. Block has inlineSignature: ", inlineSignature->toString());
        unsigned offset = m_expressionStack.size() - inlineSignature->argumentCount();
        for (unsigned i = 0; i < inlineSignature->argumentCount(); ++i) {
            Type type = m_expressionStack.at(offset + i).type();
            WASM_VALIDATOR_FAIL_IF(!isSubtype(type, inlineSignature->argument(i)), "Block expects the argument at index", i, " to be a subtype of ", inlineSignature->argument(i), " but argument has type ", type);
        }

        int64_t oldSize = m_expressionStack.size();
        Stack newStack;
        ControlType block;
        WASM_TRY_ADD_TO_CONTEXT(addBlock(inlineSignature, m_expressionStack, block, newStack));
        ASSERT_UNUSED(oldSize, oldSize - m_expressionStack.size() == inlineSignature->argumentCount());
        ASSERT(newStack.size() == inlineSignature->argumentCount());

        m_controlStack.append({ WTFMove(m_expressionStack), { },  WTFMove(block) });
        m_expressionStack = WTFMove(newStack);
        return { };
    }

    case Loop: {
        BlockSignature inlineSignature;
        WASM_PARSER_FAIL_IF(!parseBlockSignature(m_info, inlineSignature), "can't get loop's signature");

        WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < inlineSignature->argumentCount(), "Too few values on stack for loop block. Loop expects ", inlineSignature->argumentCount(), ", but only ", m_expressionStack.size(), " were present. Loop has inlineSignature: ", inlineSignature->toString());
        unsigned offset = m_expressionStack.size() - inlineSignature->argumentCount();
        for (unsigned i = 0; i < inlineSignature->argumentCount(); ++i) {
            Type type = m_expressionStack.at(offset + i).type();
            WASM_VALIDATOR_FAIL_IF(!isSubtype(type, inlineSignature->argument(i)), "Loop expects the argument at index", i, " to be a subtype of ", inlineSignature->argument(i), " but argument has type ", type);
        }

        int64_t oldSize = m_expressionStack.size();
        Stack newStack;
        ControlType loop;
        WASM_TRY_ADD_TO_CONTEXT(addLoop(inlineSignature, m_expressionStack, loop, newStack, m_loopIndex++));
        ASSERT_UNUSED(oldSize, oldSize - m_expressionStack.size() == inlineSignature->argumentCount());
        ASSERT(newStack.size() == inlineSignature->argumentCount());

        m_controlStack.append({ WTFMove(m_expressionStack), { }, WTFMove(loop) });
        m_expressionStack = WTFMove(newStack);
        return { };
    }

    case If: {
        BlockSignature inlineSignature;
        TypedExpression condition;
        WASM_PARSER_FAIL_IF(!parseBlockSignature(m_info, inlineSignature), "can't get if's signature");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "if condition");

        WASM_VALIDATOR_FAIL_IF(condition.type() != I32, "if condition must be i32, got ", condition.type());
        WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < inlineSignature->argumentCount(), "Too few arguments on stack for if block. If expects ", inlineSignature->argumentCount(), ", but only ", m_expressionStack.size(), " were present. If block has signature: ", inlineSignature->toString());
        unsigned offset = m_expressionStack.size() - inlineSignature->argumentCount();
        for (unsigned i = 0; i < inlineSignature->argumentCount(); ++i)
            WASM_VALIDATOR_FAIL_IF(!isSubtype(m_expressionStack[offset + i].type(), inlineSignature->argument(i)), "Loop expects the argument at index", i, " to be a subtype of ", inlineSignature->argument(i), " but argument has type ", m_expressionStack[i].type());

        int64_t oldSize = m_expressionStack.size();
        Stack newStack;
        ControlType control;
        WASM_TRY_ADD_TO_CONTEXT(addIf(condition, inlineSignature, m_expressionStack, control, newStack));
        ASSERT_UNUSED(oldSize, oldSize - m_expressionStack.size() == inlineSignature->argumentCount());
        ASSERT(newStack.size() == inlineSignature->argumentCount());

        m_controlStack.append({ WTFMove(m_expressionStack), newStack, WTFMove(control) });
        m_expressionStack = WTFMove(newStack);
        return { };
    }

    case Else: {
        WASM_PARSER_FAIL_IF(m_controlStack.size() == 1, "can't use else block at the top-level of a function");

        ControlEntry& controlEntry = m_controlStack.last();

        WASM_VALIDATOR_FAIL_IF(!ControlType::isIf(controlEntry.controlData), "else block isn't associated to an if");
        WASM_FAIL_IF_HELPER_FAILS(unify(controlEntry.controlData));
        WASM_TRY_ADD_TO_CONTEXT(addElse(controlEntry.controlData, m_expressionStack));
        m_expressionStack = WTFMove(controlEntry.elseBlockStack);
        return { };
    }

    case Br:
    case BrIf: {
        uint32_t target;
        TypedExpression condition;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(target), "can't get br / br_if's target");
        WASM_PARSER_FAIL_IF(target >= m_controlStack.size(), "br / br_if's target ", target, " exceeds control stack size ", m_controlStack.size());
        if (m_currentOpcode == BrIf) {
            WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "br / br_if condition");
            WASM_VALIDATOR_FAIL_IF(condition.type() != I32, "conditional branch with non-i32 condition ", condition.type());
        } else {
            m_unreachableBlocks = 1;
            condition = TypedExpression { Void, Context::emptyExpression() };
        }

        ControlType& data = m_controlStack[m_controlStack.size() - 1 - target].controlData;
        WASM_FAIL_IF_HELPER_FAILS(checkBranchTarget(data));
        WASM_TRY_ADD_TO_CONTEXT(addBranch(data, condition, m_expressionStack));
        return { };
    }

    case BrTable: {
        uint32_t numberOfTargets;
        uint32_t defaultTargetIndex;
        TypedExpression condition;
        Vector<ControlType*> targets;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(numberOfTargets), "can't get the number of targets for br_table");
        WASM_PARSER_FAIL_IF(numberOfTargets == std::numeric_limits<uint32_t>::max(), "br_table's number of targets is too big ", numberOfTargets);

        WASM_PARSER_FAIL_IF(!targets.tryReserveCapacity(numberOfTargets), "can't allocate memory for ", numberOfTargets, " br_table targets");
        for (uint32_t i = 0; i < numberOfTargets; ++i) {
            uint32_t target;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(target), "can't get ", i, "th target for br_table");
            WASM_PARSER_FAIL_IF(target >= m_controlStack.size(), "br_table's ", i, "th target ", target, " exceeds control stack size ", m_controlStack.size());
            targets.uncheckedAppend(&m_controlStack[m_controlStack.size() - 1 - target].controlData);
        }

        WASM_PARSER_FAIL_IF(!parseVarUInt32(defaultTargetIndex), "can't get default target for br_table");
        WASM_PARSER_FAIL_IF(defaultTargetIndex >= m_controlStack.size(), "br_table's default target ", defaultTargetIndex, " exceeds control stack size ", m_controlStack.size());
        ControlType& defaultTarget = m_controlStack[m_controlStack.size() - 1 - defaultTargetIndex].controlData;

        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "br_table condition");
        WASM_VALIDATOR_FAIL_IF(condition.type() != I32, "br_table with non-i32 condition ", condition.type());

        for (unsigned i = 0; i < targets.size(); ++i) {
            ControlType* target = targets[i];
            WASM_VALIDATOR_FAIL_IF(defaultTarget.branchTargetArity() != target->branchTargetArity(), "br_table target type size mismatch. Default has size: ", defaultTarget.branchTargetArity(), "but target: ", i, " has size: ", target->branchTargetArity());
            for (unsigned type = 0; type < defaultTarget.branchTargetArity(); ++type)
                WASM_VALIDATOR_FAIL_IF(!isSubtype(defaultTarget.branchTargetType(type), target->branchTargetType(type)), "br_table target type mismatch at offset ", type, " expected: ", defaultTarget.branchTargetType(type), " but saw: ", target->branchTargetType(type), " when targeting block: ", target->signature()->toString());
        }

        WASM_FAIL_IF_HELPER_FAILS(checkBranchTarget(defaultTarget));
        WASM_TRY_ADD_TO_CONTEXT(addSwitch(condition, targets, defaultTarget, m_expressionStack));

        m_unreachableBlocks = 1;
        return { };
    }

    case Return: {
        WASM_FAIL_IF_HELPER_FAILS(checkBranchTarget(m_controlStack[0].controlData));
        WASM_TRY_ADD_TO_CONTEXT(addReturn(m_controlStack[0].controlData, m_expressionStack));
        m_unreachableBlocks = 1;
        return { };
    }

    case End: {
        ControlEntry data = m_controlStack.takeLast();
        if (ControlType::isIf(data.controlData)) {
            WASM_FAIL_IF_HELPER_FAILS(unify(data.controlData));
            WASM_TRY_ADD_TO_CONTEXT(addElse(data.controlData, m_expressionStack));
            m_expressionStack = WTFMove(data.elseBlockStack);
        }
        // FIXME: This is a little weird in that it will modify the expressionStack for the result of the block.
        // That's a little too effectful for me but I don't have a better API right now.
        // see: https://bugs.webkit.org/show_bug.cgi?id=164353
        WASM_FAIL_IF_HELPER_FAILS(unify(data.controlData));
        WASM_TRY_ADD_TO_CONTEXT(endBlock(data, m_expressionStack));
        m_expressionStack.swap(data.enclosedExpressionStack);
        return { };
    }

    case Unreachable: {
        WASM_TRY_ADD_TO_CONTEXT(addUnreachable());
        m_unreachableBlocks = 1;
        return { };
    }

    case Drop: {
        WASM_PARSER_FAIL_IF(!m_expressionStack.size(), "can't drop on empty stack");
        m_expressionStack.takeLast();
        m_context.didPopValueFromStack();
        return { };
    }

    case Nop: {
        return { };
    }

    case GrowMemory: {
        WASM_PARSER_FAIL_IF(!m_info.memory, "grow_memory is only valid if a memory is defined or imported");

        uint8_t reserved;
        WASM_PARSER_FAIL_IF(!parseVarUInt1(reserved), "can't parse reserved varUint1 for grow_memory");
        WASM_PARSER_FAIL_IF(reserved != 0, "reserved varUint1 for grow_memory must be zero");

        TypedExpression delta;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(delta, "expect an i32 argument to grow_memory on the stack");
        WASM_VALIDATOR_FAIL_IF(delta.type() != I32, "grow_memory with non-i32 delta argument has type: ", delta.type());

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addGrowMemory(delta, result));
        m_expressionStack.constructAndAppend(I32, result);

        return { };
    }

    case CurrentMemory: {
        WASM_PARSER_FAIL_IF(!m_info.memory, "current_memory is only valid if a memory is defined or imported");

        uint8_t reserved;
        WASM_PARSER_FAIL_IF(!parseVarUInt1(reserved), "can't parse reserved varUint1 for current_memory");
        WASM_PARSER_FAIL_IF(reserved != 0, "reserved varUint1 for current_memory must be zero");

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addCurrentMemory(result));
        m_expressionStack.constructAndAppend(I32, result);

        return { };
    }
    }

    ASSERT_NOT_REACHED();
    return { };
}

// FIXME: We should try to use the same decoder function for both unreachable and reachable code. https://bugs.webkit.org/show_bug.cgi?id=165965
template<typename Context>
auto FunctionParser<Context>::parseUnreachableExpression() -> PartialResult
{
    ASSERT(m_unreachableBlocks);
#define CREATE_CASE(name, ...) case OpType::name:
    switch (m_currentOpcode) {
    case Else: {
        if (m_unreachableBlocks > 1)
            return { };

        ControlEntry& data = m_controlStack.last();
        m_unreachableBlocks = 0;
        WASM_VALIDATOR_FAIL_IF(!ControlType::isIf(data.controlData), "else block isn't associated to an if");
        WASM_TRY_ADD_TO_CONTEXT(addElseToUnreachable(data.controlData));
        m_expressionStack = WTFMove(data.elseBlockStack);
        return { };
    }

    case End: {
        if (m_unreachableBlocks == 1) {
            ControlEntry data = m_controlStack.takeLast();
            if (ControlType::isIf(data.controlData)) {
                WASM_TRY_ADD_TO_CONTEXT(addElseToUnreachable(data.controlData));
                m_expressionStack = WTFMove(data.elseBlockStack);
                WASM_FAIL_IF_HELPER_FAILS(unify(data.controlData));
                WASM_TRY_ADD_TO_CONTEXT(endBlock(data, m_expressionStack));
            } else
                WASM_TRY_ADD_TO_CONTEXT(addEndToUnreachable(data));

            m_expressionStack.swap(data.enclosedExpressionStack);
        }
        m_unreachableBlocks--;
        return { };
    }

    case Loop:
    case If:
    case Block: {
        m_unreachableBlocks++;
        BlockSignature unused;
        WASM_PARSER_FAIL_IF(!parseBlockSignature(m_info, unused), "can't get inline type for ", m_currentOpcode, " in unreachable context");
        return { };
    }

    case BrTable: {
        uint32_t numberOfTargets;
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(numberOfTargets), "can't get the number of targets for br_table in unreachable context");
        WASM_PARSER_FAIL_IF(numberOfTargets == std::numeric_limits<uint32_t>::max(), "br_table's number of targets is too big ", numberOfTargets);

        for (uint32_t i = 0; i < numberOfTargets; ++i)
            WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get ", i, "th target for br_table in unreachable context");

        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get default target for br_table in unreachable context");
        return { };
    }

    case CallIndirect: {
        uint32_t unused;
        uint32_t unused2;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get call_indirect's signature index in unreachable context");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused2), "can't get call_indirect's reserved byte in unreachable context");
        return { };
    }

    case F32Const: {
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseUInt32(unused), "can't parse 32-bit floating-point constant");
        return { };
    }

    case F64Const: {
        uint64_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt64(constant), "can't parse 64-bit floating-point constant");
        return { };
    }

    // two immediate cases
    FOR_EACH_WASM_MEMORY_LOAD_OP(CREATE_CASE)
    FOR_EACH_WASM_MEMORY_STORE_OP(CREATE_CASE) {
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get first immediate for ", m_currentOpcode, " in unreachable context");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get second immediate for ", m_currentOpcode, " in unreachable context");
        return { };
    }

    // one immediate cases
    case SetLocal:
    case GetLocal:
    case TeeLocal:
    case GetGlobal:
    case SetGlobal:
    case Br:
    case BrIf:
    case Call: {
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get immediate for ", m_currentOpcode, " in unreachable context");
        return { };
    }

    case I32Const: {
        int32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarInt32(unused), "can't get immediate for ", m_currentOpcode, " in unreachable context");
        return { };
    }

    case I64Const: {
        int64_t unused;
        WASM_PARSER_FAIL_IF(!parseVarInt64(unused), "can't get immediate for ", m_currentOpcode, " in unreachable context");
        return { };
    }

    case ExtTable:
    case TableGet:
    case TableSet: {
        unsigned tableIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't parse table index");
        FALLTHROUGH;
    }
    case RefIsNull:
    case RefNull: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyReferences(), "references are not enabled");
        return { };
    }

    case RefFunc: {
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get immediate for ", m_currentOpcode, " in unreachable context");
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyReferences(), "references are not enabled");
        return { };
    }

    case GrowMemory:
    case CurrentMemory: {
        uint8_t reserved;
        WASM_PARSER_FAIL_IF(!parseVarUInt1(reserved), "can't parse reserved varUint1 for grow_memory/current_memory");
        return { };
    }

    // no immediate cases
    FOR_EACH_WASM_BINARY_OP(CREATE_CASE)
    FOR_EACH_WASM_UNARY_OP(CREATE_CASE)
    case Unreachable:
    case Nop:
    case Return:
    case Select:
    case Drop: {
        return { };
    }
    }
#undef CREATE_CASE
    RELEASE_ASSERT_NOT_REACHED();
}

} } // namespace JSC::Wasm

#undef WASM_TRY_POP_EXPRESSION_STACK_INTO
#undef WASM_TRY_ADD_TO_CONTEXT
#undef WASM_VALIDATOR_FAIL_IF

#endif // ENABLE(WEBASSEMBLY)
