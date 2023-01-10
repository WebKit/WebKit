/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "AirArg.h"
#include "WasmOpcodeCounter.h"
#include "WasmParser.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/DataLog.h>
#include <wtf/ListDump.h>

namespace JSC { namespace Wasm {

enum class BlockType {
    If,
    Block,
    Loop,
    TopLevel,
    Try,
    Catch,
};

enum class CatchKind {
    Catch,
    CatchAll,
};

template<typename EnclosingStack, typename NewStack>
void splitStack(BlockSignature originalSignature, EnclosingStack& enclosingStack, NewStack& newStack)
{
    BlockSignature signature = &originalSignature->expand();
    newStack.reserveInitialCapacity(signature->as<FunctionSignature>()->argumentCount());
    ASSERT(enclosingStack.size() >= signature->as<FunctionSignature>()->argumentCount());
    unsigned offset = enclosingStack.size() - signature->as<FunctionSignature>()->argumentCount();
    for (unsigned i = 0; i < signature->as<FunctionSignature>()->argumentCount(); ++i)
        newStack.uncheckedAppend(enclosingStack.at(i + offset));
    enclosingStack.shrink(offset);
}

template<typename Control, typename Expression, typename Call>
struct FunctionParserTypes {
    using ControlType = Control;
    using ExpressionType = Expression;
    using CallType = Call;

    class TypedExpression {
    public:
        TypedExpression()
            : m_type(Types::Void)
        {
        }

        TypedExpression(Type type, ExpressionType value)
            : m_type(type)
            , m_value(value)
        {
        }

        Type type() const { return m_type; }

        ExpressionType value() const { return m_value; }
        operator ExpressionType() const { return m_value; }

        ExpressionType operator->() const { return m_value; }

        void dump(PrintStream& out) const
        {
            out.print(m_value, ": ", m_type);
        }

    private:
        Type m_type;
        ExpressionType m_value;
    };
    using Stack = Vector<TypedExpression, 16, UnsafeVectorOverflow>;

    struct ControlEntry {
        Stack enclosedExpressionStack;
        Stack elseBlockStack;
        ControlType controlData;
    };
    using ControlStack = Vector<ControlEntry, 16>;

    using ResultList = Vector<ExpressionType, 8>;
};

template<typename Context>
class FunctionParser : public Parser<void>, public FunctionParserTypes<typename Context::ControlType, typename Context::ExpressionType, typename Context::CallType> {
public:
    using CallType = typename FunctionParser::CallType;
    using ControlType = typename FunctionParser::ControlType;
    using ControlEntry = typename FunctionParser::ControlEntry;
    using ControlStack = typename FunctionParser::ControlStack;
    using ExpressionType = typename FunctionParser::ExpressionType;
    using TypedExpression = typename FunctionParser::TypedExpression;
    using Stack = typename FunctionParser::Stack;
    using ResultList = typename FunctionParser::ResultList;

    FunctionParser(Context&, const uint8_t* functionStart, size_t functionLength, const TypeDefinition&, const ModuleInformation&);

    Result WARN_UNUSED_RETURN parse();

    OpType currentOpcode() const { return m_currentOpcode; }
    size_t currentOpcodeStartingOffset() const { return m_currentOpcodeStartingOffset; }
    const TypeDefinition& signature() const { return m_signature; }

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
        WASM_PARSER_FAIL_IF(m_expressionStack.isEmpty(), "can't pop empty stack in ", what); \
        result = m_expressionStack.takeLast();                                              \
        m_context.didPopValueFromStack();                                                   \
    } while (0)

    using UnaryOperationHandler = PartialResult (Context::*)(ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN unaryCase(OpType, UnaryOperationHandler, Type returnType, Type operandType);
    using BinaryOperationHandler = PartialResult (Context::*)(ExpressionType, ExpressionType, ExpressionType&);
    PartialResult WARN_UNUSED_RETURN binaryCase(OpType, BinaryOperationHandler, Type returnType, Type lhsType, Type rhsType);

    PartialResult WARN_UNUSED_RETURN store(Type memoryType);
    PartialResult WARN_UNUSED_RETURN load(Type memoryType);

    PartialResult WARN_UNUSED_RETURN truncSaturated(Ext1OpType, Type returnType, Type operandType);

    PartialResult WARN_UNUSED_RETURN atomicLoad(ExtAtomicOpType, Type memoryType);
    PartialResult WARN_UNUSED_RETURN atomicStore(ExtAtomicOpType, Type memoryType);
    PartialResult WARN_UNUSED_RETURN atomicBinaryRMW(ExtAtomicOpType, Type memoryType);
    PartialResult WARN_UNUSED_RETURN atomicCompareExchange(ExtAtomicOpType, Type memoryType);
    PartialResult WARN_UNUSED_RETURN atomicWait(ExtAtomicOpType, Type memoryType);
    PartialResult WARN_UNUSED_RETURN atomicNotify(ExtAtomicOpType);
    PartialResult WARN_UNUSED_RETURN atomicFence(ExtAtomicOpType);

#if ENABLE(B3_JIT)
    template<bool isReachable, typename = void>
    PartialResult
    WARN_UNUSED_RETURN
    simd(SIMDLaneOperation, SIMDLane, SIMDSignMode, B3::Air::Arg optionalRelation = { });
#endif

    PartialResult WARN_UNUSED_RETURN parseTableIndex(unsigned&);
    PartialResult WARN_UNUSED_RETURN parseElementIndex(unsigned&);
    PartialResult WARN_UNUSED_RETURN parseDataSegmentIndex(unsigned&);

    PartialResult WARN_UNUSED_RETURN parseIndexForLocal(uint32_t&);
    PartialResult WARN_UNUSED_RETURN parseIndexForGlobal(uint32_t&);
    PartialResult WARN_UNUSED_RETURN parseFunctionIndex(uint32_t&);
    PartialResult WARN_UNUSED_RETURN parseExceptionIndex(uint32_t&);
    PartialResult WARN_UNUSED_RETURN parseBranchTarget(uint32_t&);
    PartialResult WARN_UNUSED_RETURN parseDelegateTarget(uint32_t&, uint32_t);

    struct TableInitImmediates {
        unsigned elementIndex;
        unsigned tableIndex;
    };
    PartialResult WARN_UNUSED_RETURN parseTableInitImmediates(TableInitImmediates&);

    struct TableCopyImmediates {
        unsigned srcTableIndex;
        unsigned dstTableIndex;
    };
    PartialResult WARN_UNUSED_RETURN parseTableCopyImmediates(TableCopyImmediates&);

    struct AnnotatedSelectImmediates {
        unsigned sizeOfAnnotationVector;
        Type targetType;
    };
    PartialResult WARN_UNUSED_RETURN parseAnnotatedSelectImmediates(AnnotatedSelectImmediates&);

    PartialResult WARN_UNUSED_RETURN parseMemoryFillImmediate();
    PartialResult WARN_UNUSED_RETURN parseMemoryCopyImmediates();

    struct MemoryInitImmediates {
        unsigned dataSegmentIndex;
        unsigned unused;
    };
    PartialResult WARN_UNUSED_RETURN parseMemoryInitImmediates(MemoryInitImmediates&);

    PartialResult WARN_UNUSED_RETURN parseStructTypeIndex(uint32_t& structTypeIndex, const char* operation);
    PartialResult WARN_UNUSED_RETURN parseStructFieldIndex(uint32_t& structFieldIndex, const StructType&, const char* operation);

    struct StructTypeIndexAndFieldIndex {
        uint32_t structTypeIndex;
        uint32_t fieldIndex;
    };
    PartialResult WARN_UNUSED_RETURN parseStructTypeIndexAndFieldIndex(StructTypeIndexAndFieldIndex& result, const char* operation);

    struct StructFieldManipulation {
        StructTypeIndexAndFieldIndex indices;
        TypedExpression structReference;
        FieldType field;
    };
    PartialResult WARN_UNUSED_RETURN parseStructFieldManipulation(StructFieldManipulation& result, const char* operation);

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
    const TypeDefinition& m_signature;
    const ModuleInformation& m_info;

    OpType m_currentOpcode;
    size_t m_currentOpcodeStartingOffset { 0 };

    unsigned m_unreachableBlocks { 0 };
    unsigned m_loopIndex { 0 };
};

template<typename ControlType>
static bool isTryOrCatch(ControlType& data)
{
    return ControlType::isTry(data) || ControlType::isCatch(data);
}

template<typename Context>
FunctionParser<Context>::FunctionParser(Context& context, const uint8_t* functionStart, size_t functionLength, const TypeDefinition& signature, const ModuleInformation& info)
    : Parser(functionStart, functionLength)
    , m_context(context)
    , m_signature(signature.expand())
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

    WASM_PARSER_FAIL_IF(!m_signature.is<FunctionSignature>(), "type signature was not a function signature");
    const auto& signature = *m_signature.as<FunctionSignature>();
    if (signature.numVectors() || signature.numReturnVectors()) {
        m_context.notifyFunctionUsesSIMD();
        if (!Context::tierSupportsSIMD)
            WASM_TRY_ADD_TO_CONTEXT(addCrash());
    }
    WASM_PARSER_FAIL_IF(!m_context.addArguments(m_signature), "can't add ", signature.argumentCount(), " arguments to Function");
    WASM_PARSER_FAIL_IF(!parseVarUInt32(localGroupsCount), "can't get local groups count");

    WASM_PARSER_FAIL_IF(!m_locals.tryReserveCapacity(signature.argumentCount()), "can't allocate enough memory for function's ", signature.argumentCount(), " arguments");
    for (uint32_t i = 0; i < signature.argumentCount(); ++i)
        m_locals.uncheckedAppend(signature.argumentType(i));

    uint64_t totalNumberOfLocals = signature.argumentCount();
    for (uint32_t i = 0; i < localGroupsCount; ++i) {
        uint32_t numberOfLocals;
        Type typeOfLocal;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(numberOfLocals), "can't get Function's number of locals in group ", i);
        totalNumberOfLocals += numberOfLocals;
        WASM_PARSER_FAIL_IF(totalNumberOfLocals > maxFunctionLocals, "Function's number of locals is too big ", totalNumberOfLocals, " maximum ", maxFunctionLocals);
        WASM_PARSER_FAIL_IF(!parseValueType(m_info, typeOfLocal), "can't get Function local's type in group ", i);
        WASM_PARSER_FAIL_IF(!isDefaultableType(typeOfLocal), "Function locals must have a defaultable type");

        if (typeOfLocal.isV128()) {
            m_context.notifyFunctionUsesSIMD();
            if (!Context::tierSupportsSIMD)
                WASM_TRY_ADD_TO_CONTEXT(addCrash());
        }

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

        if (UNLIKELY(Options::dumpWasmOpcodeStatistics()))
            WasmOpcodeCounter::singleton().increment(m_currentOpcode);

        if (verbose) {
            dataLogLn("processing op (", m_unreachableBlocks, "): ",  RawHex(op), ", ", makeString(static_cast<OpType>(op)), " at offset: ", RawHex(m_offset));
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
auto FunctionParser<Context>::binaryCase(OpType op, BinaryOperationHandler handler, Type returnType, Type lhsType, Type rhsType) -> PartialResult
{
    TypedExpression right;
    TypedExpression left;

    WASM_TRY_POP_EXPRESSION_STACK_INTO(right, "binary right");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(left, "binary left");

    WASM_VALIDATOR_FAIL_IF(left.type() != lhsType, op, " left value type mismatch");
    WASM_VALIDATOR_FAIL_IF(right.type() != rhsType, op, " right value type mismatch");

    ExpressionType result;
    WASM_FAIL_IF_HELPER_FAILS((m_context.*handler)(left, right, result));
    m_expressionStack.constructAndAppend(returnType, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::unaryCase(OpType op, UnaryOperationHandler handler, Type returnType, Type operandType) -> PartialResult
{
    TypedExpression value;
    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "unary");

    WASM_VALIDATOR_FAIL_IF(value.type() != operandType, op, " value type mismatch");

    ExpressionType result;
    WASM_FAIL_IF_HELPER_FAILS((m_context.*handler)(value, result));
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

    WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), m_currentOpcode, " pointer type mismatch");

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

    WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), m_currentOpcode, " pointer type mismatch");
    WASM_VALIDATOR_FAIL_IF(value.type() != memoryType, m_currentOpcode, " value type mismatch");

    WASM_TRY_ADD_TO_CONTEXT(store(static_cast<StoreOpType>(m_currentOpcode), pointer, value, offset));
    return { };
}

template<typename Context>
auto FunctionParser<Context>::truncSaturated(Ext1OpType op, Type returnType, Type operandType) -> PartialResult
{
    TypedExpression value;
    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "unary");

    WASM_VALIDATOR_FAIL_IF(value.type() != operandType, "trunc-saturated value type mismatch");

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(truncSaturated(op, value, result, returnType, operandType));
    m_expressionStack.constructAndAppend(returnType, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::atomicLoad(ExtAtomicOpType op, Type memoryType) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(!m_info.memory, "atomic instruction without memory");

    uint32_t alignment;
    uint32_t offset;
    TypedExpression pointer;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get load alignment");
    WASM_PARSER_FAIL_IF(alignment != memoryLog2Alignment(op), "byte alignment ", 1ull << alignment, " does not match against atomic op's natural alignment ", 1ull << memoryLog2Alignment(op));
    WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get load offset");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "load pointer");

    WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), static_cast<unsigned>(op), " pointer type mismatch");

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(atomicLoad(op, memoryType, pointer, result, offset));
    m_expressionStack.constructAndAppend(memoryType, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::atomicStore(ExtAtomicOpType op, Type memoryType) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(!m_info.memory, "atomic instruction without memory");

    uint32_t alignment;
    uint32_t offset;
    TypedExpression value;
    TypedExpression pointer;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get store alignment");
    WASM_PARSER_FAIL_IF(alignment != memoryLog2Alignment(op), "byte alignment ", 1ull << alignment, " does not match against atomic op's natural alignment ", 1ull << memoryLog2Alignment(op));
    WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get store offset");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "store value");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "store pointer");

    WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), m_currentOpcode, " pointer type mismatch");
    WASM_VALIDATOR_FAIL_IF(value.type() != memoryType, m_currentOpcode, " value type mismatch");

    WASM_TRY_ADD_TO_CONTEXT(atomicStore(op, memoryType, pointer, value, offset));
    return { };
}

template<typename Context>
auto FunctionParser<Context>::atomicBinaryRMW(ExtAtomicOpType op, Type memoryType) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(!m_info.memory, "atomic instruction without memory");

    uint32_t alignment;
    uint32_t offset;
    TypedExpression pointer;
    TypedExpression value;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get load alignment");
    WASM_PARSER_FAIL_IF(alignment != memoryLog2Alignment(op), "byte alignment ", 1ull << alignment, " does not match against atomic op's natural alignment ", 1ull << memoryLog2Alignment(op));
    WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get load offset");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "value");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "pointer");

    WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), static_cast<unsigned>(op), " pointer type mismatch");
    WASM_VALIDATOR_FAIL_IF(value.type() != memoryType, static_cast<unsigned>(op), " value type mismatch");

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(atomicBinaryRMW(op, memoryType, pointer, value, result, offset));
    m_expressionStack.constructAndAppend(memoryType, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::atomicCompareExchange(ExtAtomicOpType op, Type memoryType) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(!m_info.memory, "atomic instruction without memory");

    uint32_t alignment;
    uint32_t offset;
    TypedExpression pointer;
    TypedExpression expected;
    TypedExpression value;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get load alignment");
    WASM_PARSER_FAIL_IF(alignment !=  memoryLog2Alignment(op), "byte alignment ", 1ull << alignment, " does not match against atomic op's natural alignment ", 1ull << memoryLog2Alignment(op));
    WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get load offset");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "value");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(expected, "expected");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "pointer");

    WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), static_cast<unsigned>(op), " pointer type mismatch");
    WASM_VALIDATOR_FAIL_IF(expected.type() != memoryType, static_cast<unsigned>(op), " expected type mismatch");
    WASM_VALIDATOR_FAIL_IF(value.type() != memoryType, static_cast<unsigned>(op), " value type mismatch");

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(atomicCompareExchange(op, memoryType, pointer, expected, value, result, offset));
    m_expressionStack.constructAndAppend(memoryType, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::atomicWait(ExtAtomicOpType op, Type memoryType) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(!m_info.memory, "atomic instruction without memory");

    uint32_t alignment;
    uint32_t offset;
    TypedExpression pointer;
    TypedExpression value;
    TypedExpression timeout;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get load alignment");
    WASM_PARSER_FAIL_IF(alignment != memoryLog2Alignment(op), "byte alignment ", 1ull << alignment, " does not match against atomic op's natural alignment ", 1ull << memoryLog2Alignment(op));
    WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get load offset");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(timeout, "timeout");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "value");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "pointer");

    WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), static_cast<unsigned>(op), " pointer type mismatch");
    WASM_VALIDATOR_FAIL_IF(value.type() != memoryType, static_cast<unsigned>(op), " value type mismatch");
    WASM_VALIDATOR_FAIL_IF(!timeout.type().isI64(), static_cast<unsigned>(op), " timeout type mismatch");

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(atomicWait(op, pointer, value, timeout, result, offset));
    m_expressionStack.constructAndAppend(Types::I32, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::atomicNotify(ExtAtomicOpType op) -> PartialResult
{
    WASM_VALIDATOR_FAIL_IF(!m_info.memory, "atomic instruction without memory");

    uint32_t alignment;
    uint32_t offset;
    TypedExpression pointer;
    TypedExpression count;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get load alignment");
    WASM_PARSER_FAIL_IF(alignment != memoryLog2Alignment(op), "byte alignment ", 1ull << alignment, " does not match against atomic op's natural alignment ", 1ull << memoryLog2Alignment(op));
    WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get load offset");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(count, "count");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "pointer");

    WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), static_cast<unsigned>(op), " pointer type mismatch");
    WASM_VALIDATOR_FAIL_IF(!count.type().isI32(), static_cast<unsigned>(op), " count type mismatch"); // The spec's definition is saying i64, but all implementations (including tests) are using i32. So looks like the spec is wrong.

    ExpressionType result;
    WASM_TRY_ADD_TO_CONTEXT(atomicNotify(op, pointer, count, result, offset));
    m_expressionStack.constructAndAppend(Types::I32, result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::atomicFence(ExtAtomicOpType op) -> PartialResult
{
    uint8_t flags;
    WASM_PARSER_FAIL_IF(!parseUInt8(flags), "can't get flags");
    WASM_PARSER_FAIL_IF(flags != 0x0, "flags should be 0x0 but got ", flags);
    WASM_TRY_ADD_TO_CONTEXT(atomicFence(op, flags));
    return { };
}

#if ENABLE(B3_JIT)
template<typename Context>
template<bool isReachable, typename>
auto FunctionParser<Context>::simd(SIMDLaneOperation op, SIMDLane lane, SIMDSignMode signMode, B3::Air::Arg optionalRelation) -> PartialResult
{
    if (!Context::tierSupportsSIMD)
        WASM_TRY_ADD_TO_CONTEXT(addCrash());
    m_context.notifyFunctionUsesSIMD();

    auto pushUnreachable = [&](auto type) -> PartialResult {
        ASSERT(isReachable);
        // Appease generators without SIMD support.
        m_expressionStack.constructAndAppend(type, m_context.addConstant(Types::F64, 0));
        return { };
    };

    // only used in some specializations
    UNUSED_VARIABLE(pushUnreachable);
    UNUSED_PARAM(optionalRelation);

    auto parseMemOp = [&] (uint32_t& offset, TypedExpression& pointer) -> PartialResult {
        uint32_t maxAlignment;
        switch (op) {
        case SIMDLaneOperation::LoadLane8:
        case SIMDLaneOperation::StoreLane8:
        case SIMDLaneOperation::LoadSplat8:
            maxAlignment = 0;
            break;
        case SIMDLaneOperation::LoadLane16:
        case SIMDLaneOperation::StoreLane16:
        case SIMDLaneOperation::LoadSplat16:
            maxAlignment = 1;
            break;
        case SIMDLaneOperation::LoadLane32:
        case SIMDLaneOperation::StoreLane32:
        case SIMDLaneOperation::LoadSplat32:
        case SIMDLaneOperation::LoadPad32:
            maxAlignment = 2;
            break;
        case SIMDLaneOperation::LoadLane64:
        case SIMDLaneOperation::StoreLane64:
        case SIMDLaneOperation::LoadSplat64:
        case SIMDLaneOperation::LoadPad64:
        case SIMDLaneOperation::LoadExtend8U:
        case SIMDLaneOperation::LoadExtend8S:
        case SIMDLaneOperation::LoadExtend16U:
        case SIMDLaneOperation::LoadExtend16S:
        case SIMDLaneOperation::LoadExtend32U:
        case SIMDLaneOperation::LoadExtend32S:
            maxAlignment = 3;
            break;
        case SIMDLaneOperation::Load:
        case SIMDLaneOperation::Store:
            maxAlignment = 4;
            break;
        default: RELEASE_ASSERT_NOT_REACHED();
        }

        WASM_VALIDATOR_FAIL_IF(!m_info.memory, "simd memory instructions need a memory defined in the module");

        uint32_t alignment;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get simd memory op alignment");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get simd memory op offset");

        WASM_VALIDATOR_FAIL_IF(alignment > maxAlignment, "alignment: ", alignment, " can't be larger than max alignment for simd operation: ", maxAlignment);

        if constexpr (!isReachable)
            return { };

        WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "simd memory op pointer");
        WASM_VALIDATOR_FAIL_IF(!pointer.type().isI32(), "pointer must be i32");

        return { };
    };

    switch (op) {
    case SIMDLaneOperation::Const: {
        v128_t constant;
        ASSERT(lane == SIMDLane::v128);
        ASSERT(signMode == SIMDSignMode::None);
        WASM_PARSER_FAIL_IF(!parseImmByteArray16(constant), "can't parse 128-bit vector constant");

        if constexpr (!isReachable)
            return { };

        if constexpr (Context::tierSupportsSIMD) {
            m_expressionStack.constructAndAppend(Types::V128, m_context.addConstant(constant));
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::Splat: {
        ASSERT(signMode == SIMDSignMode::None);

        if constexpr (!isReachable)
            return { };

        TypedExpression scalar;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(scalar, "select condition");
        bool okType;
        switch (lane) {
        case SIMDLane::i8x16:
        case SIMDLane::i16x8:
        case SIMDLane::i32x4:
            okType = scalar.type().isI32();
            break;
        case SIMDLane::i64x2:
            okType = scalar.type().isI64();
            break;
        case SIMDLane::f32x4:
            okType = scalar.type().isF32();
            break;
        case SIMDLane::f64x2:
            okType = scalar.type().isF64();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        WASM_VALIDATOR_FAIL_IF(!okType, "Wrong type to SIMD splat");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDSplat(lane, scalar, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::Shr:
    case SIMDLaneOperation::Shl: {
        if constexpr (!isReachable)
            return { };

        TypedExpression vector;
        TypedExpression shift;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(shift, "shift i32");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(vector, "shift vector");
        WASM_VALIDATOR_FAIL_IF(!vector.type().isV128(), "Shift vector must be v128");
        WASM_VALIDATOR_FAIL_IF(!shift.type().isI32(), "Shift amount must be i32");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDShift(op, SIMDInfo { lane, signMode }, vector, shift, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);

    }
    case SIMDLaneOperation::ExtmulLow:
    case SIMDLaneOperation::ExtmulHigh: {
        if constexpr (!isReachable)
            return { };

        TypedExpression lhs;
        TypedExpression rhs;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(rhs, "rhs");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(lhs, "lhs");
        WASM_VALIDATOR_FAIL_IF(!lhs.type().isV128(), "extmul lhs vector must be v128");
        WASM_VALIDATOR_FAIL_IF(!rhs.type().isV128(), "extmul rhs vector must be v128");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDExtmul(op, SIMDInfo { lane, signMode }, lhs, rhs, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::LoadSplat8:
    case SIMDLaneOperation::LoadSplat16:
    case SIMDLaneOperation::LoadSplat32:
    case SIMDLaneOperation::LoadSplat64:
    case SIMDLaneOperation::Load: {
        uint32_t offset;
        TypedExpression pointer;
        WASM_FAIL_IF_HELPER_FAILS(parseMemOp(offset, pointer));

        if constexpr (!isReachable)
            return { };

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            if (op == SIMDLaneOperation::Load)
                WASM_TRY_ADD_TO_CONTEXT(addSIMDLoad(pointer, offset, result));
            else
                WASM_TRY_ADD_TO_CONTEXT(addSIMDLoadSplat(op, pointer, offset, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::Store: {
        TypedExpression val;

        if (isReachable) {
            WASM_TRY_POP_EXPRESSION_STACK_INTO(val, "val");
            WASM_VALIDATOR_FAIL_IF(!val.type().isV128(), "store vector must be v128");
        }

        uint32_t offset;
        TypedExpression pointer;
        WASM_FAIL_IF_HELPER_FAILS(parseMemOp(offset, pointer));

        if constexpr (!isReachable)
            return { };

        if constexpr (Context::tierSupportsSIMD) {
            WASM_TRY_ADD_TO_CONTEXT(addSIMDStore(val, pointer, offset));
            return { };
        } else
            return { };
    }
    case SIMDLaneOperation::LoadLane8:
    case SIMDLaneOperation::LoadLane16:
    case SIMDLaneOperation::LoadLane32:
    case SIMDLaneOperation::LoadLane64: {
        uint32_t offset;
        TypedExpression pointer;
        TypedExpression vector;
        uint8_t laneIndex;
        uint8_t laneCount = ([&] {
            switch (op) {
            case SIMDLaneOperation::LoadLane8:
                return 16;
            case SIMDLaneOperation::LoadLane16:
                return 8;
            case SIMDLaneOperation::LoadLane32:
                return 4;
            case SIMDLaneOperation::LoadLane64:
                return 2;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        })();

        if (isReachable) {
            WASM_TRY_POP_EXPRESSION_STACK_INTO(vector, "vector");
            WASM_VALIDATOR_FAIL_IF(!vector.type().isV128(), "load_lane input must be a vector");
        }

        WASM_FAIL_IF_HELPER_FAILS(parseMemOp(offset, pointer));
        WASM_FAIL_IF_HELPER_FAILS(parseImmLaneIdx(laneCount, laneIndex));

        if constexpr (!isReachable)
            return { };

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDLoadLane(op, pointer, vector, offset, laneIndex, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::StoreLane8:
    case SIMDLaneOperation::StoreLane16:
    case SIMDLaneOperation::StoreLane32:
    case SIMDLaneOperation::StoreLane64: {
        uint32_t offset;
        TypedExpression pointer;
        TypedExpression vector;
        uint8_t laneIndex;
        uint8_t laneCount = ([&] {
            switch (op) {
            case SIMDLaneOperation::StoreLane8:
                return 16;
            case SIMDLaneOperation::StoreLane16:
                return 8;
            case SIMDLaneOperation::StoreLane32:
                return 4;
            case SIMDLaneOperation::StoreLane64:
                return 2;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        })();

        if (isReachable) {
            WASM_TRY_POP_EXPRESSION_STACK_INTO(vector, "vector");
            WASM_VALIDATOR_FAIL_IF(!vector.type().isV128(), "store_lane input must be a vector");
        }

        WASM_FAIL_IF_HELPER_FAILS(parseMemOp(offset, pointer));
        WASM_FAIL_IF_HELPER_FAILS(parseImmLaneIdx(laneCount, laneIndex));

        if constexpr (!isReachable)
            return { };

        if constexpr (Context::tierSupportsSIMD) {
            WASM_TRY_ADD_TO_CONTEXT(addSIMDStoreLane(op, pointer, vector, offset, laneIndex));
            return { };
        } else
            return { };
    }
    case SIMDLaneOperation::LoadExtend8U:
    case SIMDLaneOperation::LoadExtend8S:
    case SIMDLaneOperation::LoadExtend16U:
    case SIMDLaneOperation::LoadExtend16S:
    case SIMDLaneOperation::LoadExtend32U:
    case SIMDLaneOperation::LoadExtend32S: {
        uint32_t offset;
        TypedExpression pointer;

        WASM_FAIL_IF_HELPER_FAILS(parseMemOp(offset, pointer));

        if constexpr (!isReachable)
            return { };

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDLoadExtend(op, pointer, offset, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::LoadPad32:
    case SIMDLaneOperation::LoadPad64: {
        uint32_t offset;
        TypedExpression pointer;

        WASM_FAIL_IF_HELPER_FAILS(parseMemOp(offset, pointer));

        if constexpr (!isReachable)
            return { };

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDLoadPad(op, pointer, offset, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::Shuffle: {
        v128_t imm;
        TypedExpression a;
        TypedExpression b;

        WASM_PARSER_FAIL_IF(!parseImmByteArray16(imm), "can't parse 128-bit shuffle immediate");
        for (auto i = 0; i < 16; ++i)
            WASM_PARSER_FAIL_IF(imm.u8x16[i] >= 2 * elementCount(lane));

        if constexpr (!isReachable)
            return { };

        WASM_TRY_POP_EXPRESSION_STACK_INTO(b, "vector argument");
        WASM_VALIDATOR_FAIL_IF(!b.type().isV128(), "shuffle input must be a vector");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(a, "vector argument");
        WASM_VALIDATOR_FAIL_IF(!a.type().isV128(), "shuffle input must be a vector");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDShuffle(imm, a, b, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::ExtractLane: {
        uint8_t laneIdx;
        TypedExpression v;
        WASM_FAIL_IF_HELPER_FAILS(parseImmLaneIdx(elementCount(lane), laneIdx));

        if constexpr (!isReachable)
            return { };

        WASM_TRY_POP_EXPRESSION_STACK_INTO(v, "vector argument");
        WASM_VALIDATOR_FAIL_IF(v.type() != Types::V128, "type mismatch for argument 0");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addExtractLane(SIMDInfo { lane, signMode }, laneIdx, v, result));
            m_expressionStack.constructAndAppend(simdScalarType(lane), result);
            return { };
        } else
            return pushUnreachable(simdScalarType(lane));
    }
    case SIMDLaneOperation::ReplaceLane: {
        uint8_t laneIdx;
        TypedExpression v;
        TypedExpression s;
        WASM_FAIL_IF_HELPER_FAILS(parseImmLaneIdx(elementCount(lane), laneIdx));

        if constexpr (!isReachable)
            return { };

        WASM_TRY_POP_EXPRESSION_STACK_INTO(s, "scalar argument");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(v, "vector argument");
        WASM_VALIDATOR_FAIL_IF(v.type() != Types::V128, "type mismatch for argument 1");
        WASM_VALIDATOR_FAIL_IF(s.type() != simdScalarType(lane), "type mismatch for argument 0");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addReplaceLane(SIMDInfo { lane, signMode }, laneIdx, v, s, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::Bitmask:
    case SIMDLaneOperation::AnyTrue:
    case SIMDLaneOperation::AllTrue: {
        if constexpr (!isReachable)
            return { };

        TypedExpression v;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(v, "vector argument");
        WASM_VALIDATOR_FAIL_IF(v.type() != Types::V128, "type mismatch for argument 0");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDI_V(op, SIMDInfo { lane, signMode }, v, result));
            m_expressionStack.constructAndAppend(Types::I32, result);
            return { };
        } else
            return pushUnreachable(Types::I32);
    }
    case SIMDLaneOperation::ExtaddPairwise:
    case SIMDLaneOperation::Convert:
    case SIMDLaneOperation::ConvertLow:
    case SIMDLaneOperation::ExtendHigh:
    case SIMDLaneOperation::ExtendLow:
    case SIMDLaneOperation::TruncSat:
    case SIMDLaneOperation::Not:
    case SIMDLaneOperation::Demote:
    case SIMDLaneOperation::Promote:
    case SIMDLaneOperation::Abs:
    case SIMDLaneOperation::Neg:
    case SIMDLaneOperation::Popcnt:
    case SIMDLaneOperation::Ceil:
    case SIMDLaneOperation::Floor:
    case SIMDLaneOperation::Trunc:
    case SIMDLaneOperation::Nearest:
    case SIMDLaneOperation::Sqrt: {
        if constexpr (!isReachable)
            return { };

        TypedExpression v;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(v, "vector argument");
        WASM_VALIDATOR_FAIL_IF(v.type() != Types::V128, "type mismatch for argument 0");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDV_V(op, SIMDInfo { lane, signMode }, v, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::BitwiseSelect: {
        if constexpr (!isReachable)
            return { };

        TypedExpression v1;
        TypedExpression v2;
        TypedExpression c;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(c, "vector argument");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(v2, "vector argument");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(v1, "vector argument");
        WASM_VALIDATOR_FAIL_IF(v1.type() != Types::V128, "type mismatch for argument 2");
        WASM_VALIDATOR_FAIL_IF(v2.type() != Types::V128, "type mismatch for argument 1");
        WASM_VALIDATOR_FAIL_IF(c.type() != Types::V128, "type mismatch for argument 0");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDBitwiseSelect(v1, v2, c, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::GreaterThan:
    case SIMDLaneOperation::GreaterThanOrEqual:
    case SIMDLaneOperation::LessThan:
    case SIMDLaneOperation::LessThanOrEqual: {
        if constexpr (!isReachable)
            return { };

        TypedExpression rhs;
        TypedExpression lhs;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(rhs, "vector argument");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(lhs, "vector argument");
        WASM_VALIDATOR_FAIL_IF(lhs.type() != Types::V128, "type mismatch for argument 1");
        WASM_VALIDATOR_FAIL_IF(rhs.type() != Types::V128, "type mismatch for argument 0");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDRelOp(op, SIMDInfo { lane, signMode }, lhs, rhs, optionalRelation, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::Equal:
    case SIMDLaneOperation::NotEqual: {
        if constexpr (!isReachable)
            return { };

        TypedExpression rhs;
        TypedExpression lhs;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(rhs, "vector argument");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(lhs, "vector argument");
        WASM_VALIDATOR_FAIL_IF(lhs.type() != Types::V128, "type mismatch for argument 1");
        WASM_VALIDATOR_FAIL_IF(rhs.type() != Types::V128, "type mismatch for argument 0");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDRelOp(op, SIMDInfo { lane, signMode }, lhs, rhs, optionalRelation, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    case SIMDLaneOperation::And:
    case SIMDLaneOperation::Andnot:
    case SIMDLaneOperation::AvgRound:
    case SIMDLaneOperation::DotProduct:
    case SIMDLaneOperation::Add:
    case SIMDLaneOperation::Mul:
    case SIMDLaneOperation::MulSat:
    case SIMDLaneOperation::Sub:
    case SIMDLaneOperation::Div:
    case SIMDLaneOperation::Pmax:
    case SIMDLaneOperation::Pmin:
    case SIMDLaneOperation::Or:
    case SIMDLaneOperation::Swizzle:
    case SIMDLaneOperation::Xor:
    case SIMDLaneOperation::Narrow:
    case SIMDLaneOperation::AddSat:
    case SIMDLaneOperation::SubSat:
    case SIMDLaneOperation::Max:
    case SIMDLaneOperation::Min: {
        if constexpr (!isReachable)
            return { };

        TypedExpression a;
        TypedExpression b;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(b, "vector argument");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(a, "vector argument");
        WASM_VALIDATOR_FAIL_IF(a.type() != Types::V128, "type mismatch for argument 1");
        WASM_VALIDATOR_FAIL_IF(b.type() != Types::V128, "type mismatch for argument 0");

        if constexpr (Context::tierSupportsSIMD) {
            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addSIMDV_VV(op, SIMDInfo { lane, signMode }, a, b, result));
            m_expressionStack.constructAndAppend(Types::V128, result);
            return { };
        } else
            return pushUnreachable(Types::V128);
    }
    default:
        ASSERT_NOT_REACHED();
        WASM_PARSER_FAIL_IF(true, "invalid simd op ", SIMDLaneOperationDump(op));
        break;
    }
    return { };
}
#endif

template<typename Context>
auto FunctionParser<Context>::parseTableIndex(unsigned& result) -> PartialResult
{
    unsigned tableIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't parse table index");
    WASM_VALIDATOR_FAIL_IF(tableIndex >= m_info.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_info.tableCount());
    result = tableIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseIndexForLocal(uint32_t& resultIndex) -> PartialResult
{
    uint32_t index;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for local");
    WASM_VALIDATOR_FAIL_IF(index >= m_locals.size(), "attempt to use unknown local ", index, ", the number of locals is ", m_locals.size());
    resultIndex = index;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseIndexForGlobal(uint32_t& resultIndex) -> PartialResult
{
    uint32_t index;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get global's index");
    WASM_VALIDATOR_FAIL_IF(index >= m_info.globals.size(), index, " of unknown global, limit is ", m_info.globals.size());
    resultIndex = index;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseFunctionIndex(uint32_t& resultIndex) -> PartialResult
{
    uint32_t functionIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't parse function index");
    WASM_PARSER_FAIL_IF(functionIndex >= m_info.functionIndexSpaceSize(), "function index ", functionIndex, " exceeds function index space ", m_info.functionIndexSpaceSize());
    resultIndex = functionIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseExceptionIndex(uint32_t& result) -> PartialResult
{
    uint32_t exceptionIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(exceptionIndex), "can't parse exception index");
    WASM_VALIDATOR_FAIL_IF(exceptionIndex >= m_info.exceptionIndexSpaceSize(), "exception index ", exceptionIndex, " is invalid, limit is ", m_info.exceptionIndexSpaceSize());
    result = exceptionIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseBranchTarget(uint32_t& resultTarget) -> PartialResult
{
    uint32_t target;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(target), "can't get br / br_if's target");
    WASM_PARSER_FAIL_IF(target >= m_controlStack.size(), "br / br_if's target ", target, " exceeds control stack size ", m_controlStack.size());
    resultTarget = target;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseDelegateTarget(uint32_t& resultTarget, uint32_t unreachableBlocks) -> PartialResult
{
    // Right now, control stack includes try-delegate block, and delegate needs to specify outer scope.
    uint32_t target;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(target), "can't get delegate target");
    Checked<uint32_t, RecordOverflow> controlStackSize { m_controlStack.size() };
    if (unreachableBlocks)
        controlStackSize += (unreachableBlocks - 1); // The first block is in the control stack already.
    controlStackSize -= 1; // delegate target does not include the current block.
    WASM_PARSER_FAIL_IF(controlStackSize.hasOverflowed(), "invalid control stack size");
    WASM_PARSER_FAIL_IF(target >= controlStackSize.value(), "delegate target ", target, " exceeds control stack size ", controlStackSize.value());
    resultTarget = target;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseElementIndex(unsigned& result) -> PartialResult
{
    unsigned elementIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(elementIndex), "can't parse element index");
    WASM_VALIDATOR_FAIL_IF(elementIndex >= m_info.elementCount(), "element index ", elementIndex, " is invalid, limit is ", m_info.elementCount());
    result = elementIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseDataSegmentIndex(unsigned& result) -> PartialResult
{
    unsigned dataSegmentIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(dataSegmentIndex), "can't parse data segment index");
    WASM_VALIDATOR_FAIL_IF(dataSegmentIndex >= m_info.dataSegmentsCount(), "data segment index ", dataSegmentIndex, " is invalid, limit is ", m_info.dataSegmentsCount());
    result = dataSegmentIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseTableInitImmediates(TableInitImmediates& result) -> PartialResult
{
    unsigned elementIndex;
    WASM_FAIL_IF_HELPER_FAILS(parseElementIndex(elementIndex));

    unsigned tableIndex;
    WASM_FAIL_IF_HELPER_FAILS(parseTableIndex(tableIndex));

    result.elementIndex = elementIndex;
    result.tableIndex = tableIndex;

    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseTableCopyImmediates(TableCopyImmediates& result) -> PartialResult
{
    unsigned dstTableIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(dstTableIndex), "can't parse destination table index");
    WASM_VALIDATOR_FAIL_IF(dstTableIndex >= m_info.tableCount(), "table index ", dstTableIndex, " is invalid, limit is ", m_info.tableCount());

    unsigned srcTableIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(srcTableIndex), "can't parse source table index");
    WASM_VALIDATOR_FAIL_IF(srcTableIndex >= m_info.tableCount(), "table index ", srcTableIndex, " is invalid, limit is ", m_info.tableCount());

    result.dstTableIndex = dstTableIndex;
    result.srcTableIndex = srcTableIndex;

    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseAnnotatedSelectImmediates(AnnotatedSelectImmediates& result) -> PartialResult
{
    uint32_t sizeOfAnnotationVector;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(sizeOfAnnotationVector), "select can't parse the size of annotation vector");
    WASM_PARSER_FAIL_IF(sizeOfAnnotationVector != 1, "select invalid result arity for");

    Type targetType;
    WASM_PARSER_FAIL_IF(!parseValueType(m_info, targetType), "select can't parse annotations");

    result.sizeOfAnnotationVector = sizeOfAnnotationVector;
    result.targetType = targetType;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseMemoryFillImmediate() -> PartialResult
{
    uint8_t auxiliaryByte;
    WASM_PARSER_FAIL_IF(!parseUInt8(auxiliaryByte), "can't parse auxiliary byte");
    WASM_PARSER_FAIL_IF(!!auxiliaryByte, "auxiliary byte for memory.fill should be zero, but got ", auxiliaryByte);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseMemoryCopyImmediates() -> PartialResult
{
    uint8_t firstAuxiliaryByte;
    WASM_PARSER_FAIL_IF(!parseUInt8(firstAuxiliaryByte), "can't parse auxiliary byte");
    WASM_PARSER_FAIL_IF(!!firstAuxiliaryByte, "auxiliary byte for memory.copy should be zero, but got ", firstAuxiliaryByte);

    uint8_t secondAuxiliaryByte;
    WASM_PARSER_FAIL_IF(!parseUInt8(secondAuxiliaryByte), "can't parse auxiliary byte");
    WASM_PARSER_FAIL_IF(!!secondAuxiliaryByte, "auxiliary byte for memory.copy should be zero, but got ", secondAuxiliaryByte);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseMemoryInitImmediates(MemoryInitImmediates& result) -> PartialResult
{
    unsigned dataSegmentIndex;
    WASM_FAIL_IF_HELPER_FAILS(parseDataSegmentIndex(dataSegmentIndex));

    unsigned unused;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't parse unused");
    WASM_PARSER_FAIL_IF(!!unused, "memory.init invalid unsued byte");

    result.unused = unused;
    result.dataSegmentIndex = dataSegmentIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseStructTypeIndex(uint32_t& structTypeIndex, const char* operation) -> PartialResult
{
    uint32_t typeIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(typeIndex), "can't get type index for", operation);
    WASM_VALIDATOR_FAIL_IF(typeIndex >= m_info.typeCount(), operation, " index ", typeIndex, " is out of bound");
    const TypeDefinition& type = m_info.typeSignatures[typeIndex].get();
    WASM_VALIDATOR_FAIL_IF(!type.is<StructType>(), operation, ": invalid type index", typeIndex);

    structTypeIndex = typeIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseStructFieldIndex(uint32_t& structFieldIndex, const StructType& structType, const char* operation) -> PartialResult
{
    uint32_t fieldIndex;
    WASM_PARSER_FAIL_IF(!parseVarUInt32(fieldIndex), "can't get type index for ", operation);
    WASM_PARSER_FAIL_IF(fieldIndex >= structType.fieldCount(), operation, " field immediate ", fieldIndex, " is out of bounds");

    structFieldIndex = fieldIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseStructTypeIndexAndFieldIndex(StructTypeIndexAndFieldIndex& result, const char* operation) -> PartialResult
{
    uint32_t structTypeIndex;
    WASM_FAIL_IF_HELPER_FAILS(parseStructTypeIndex(structTypeIndex, operation));

    const auto& typeDefinition = m_info.typeSignatures[structTypeIndex];
    uint32_t fieldIndex;
    WASM_FAIL_IF_HELPER_FAILS(parseStructFieldIndex(fieldIndex, *typeDefinition->template as<StructType>(), operation));

    result.fieldIndex = fieldIndex;
    result.structTypeIndex = structTypeIndex;
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseStructFieldManipulation(StructFieldManipulation& result, const char* operation) -> PartialResult
{
    StructTypeIndexAndFieldIndex typeIndexAndFieldIndex;
    WASM_PARSER_FAIL_IF(!parseStructTypeIndexAndFieldIndex(typeIndexAndFieldIndex, operation));

    TypedExpression structRef;
    WASM_TRY_POP_EXPRESSION_STACK_INTO(structRef, "struct reference");
    WASM_VALIDATOR_FAIL_IF(!isRefWithTypeIndex(structRef.type()), operation, " invalid index: ", structRef.type());
    const TypeIndex structTypeIndex = structRef.type().index;
    const TypeDefinition& structTypeDefinition = TypeInformation::get(structTypeIndex);
    WASM_VALIDATOR_FAIL_IF(!structTypeDefinition.is<StructType>(), operation, " type index points into a non struct type");
    const auto& structType = *structTypeDefinition.as<StructType>();
    WASM_VALIDATOR_FAIL_IF(structRef.type().index != m_info.typeSignatures[typeIndexAndFieldIndex.structTypeIndex]->index(), operation, ": popped struct has a different type index than specified in immeddiate");

    result.structReference = structRef;
    result.indices.fieldIndex = typeIndexAndFieldIndex.fieldIndex;
    result.indices.structTypeIndex = typeIndexAndFieldIndex.structTypeIndex;
    result.field = structType.field(result.indices.fieldIndex);
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
        WASM_VALIDATOR_FAIL_IF(!isSubtype(m_expressionStack[offset + i].type(), target.branchTargetType(i)), "branch's stack type is not a block's type branch target type. Stack value has type ", m_expressionStack[offset + i].type(), " but branch target expects a value of ", target.branchTargetType(i), " at index ", i);

    return { };
}

template<typename Context>
auto FunctionParser<Context>::unify(const ControlType& controlData) -> PartialResult
{
    const TypeDefinition* typeDefinition = controlData.signature(); // just to avoid a weird compiler error with templates at the next line.
    const FunctionSignature* signature = typeDefinition->as<FunctionSignature>();
    WASM_VALIDATOR_FAIL_IF(signature->returnCount() != m_expressionStack.size(), " block with type: ", signature->toString(), " returns: ", signature->returnCount(), " but stack has: ", m_expressionStack.size(), " values");
    for (unsigned i = 0; i < signature->returnCount(); ++i)
        WASM_VALIDATOR_FAIL_IF(!isSubtype(m_expressionStack[i].type(), signature->returnType(i)), "control flow returns with unexpected type. ", m_expressionStack[i].type(), " is not a ", signature->returnType(i));

    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseExpression() -> PartialResult
{
    switch (m_currentOpcode) {
#define CREATE_CASE(name, id, b3op, inc, lhsType, rhsType, returnType) case OpType::name: return binaryCase(OpType::name, &Context::add##name, Types::returnType, Types::lhsType, Types::rhsType);
        FOR_EACH_WASM_BINARY_OP(CREATE_CASE)
#undef CREATE_CASE

#define CREATE_CASE(name, id, b3op, inc, operandType, returnType) case OpType::name: return unaryCase(OpType::name, &Context::add##name, Types::returnType, Types::operandType);
        FOR_EACH_WASM_UNARY_OP(CREATE_CASE)
#undef CREATE_CASE

    case Select: {
        TypedExpression condition;
        TypedExpression zero;
        TypedExpression nonZero;

        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "select condition");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(zero, "select zero");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(nonZero, "select non-zero");

        WASM_PARSER_FAIL_IF(isRefType(nonZero.type()) || isRefType(nonZero.type()), "can't use ref-types with unannotated select");

        WASM_VALIDATOR_FAIL_IF(!condition.type().isI32(), "select condition must be i32, got ", condition.type());
        WASM_VALIDATOR_FAIL_IF(nonZero.type() != zero.type(), "select result types must match, got ", nonZero.type(), " and ", zero.type());

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addSelect(condition, nonZero, zero, result));

        m_expressionStack.constructAndAppend(zero.type(), result);
        return { };
    }

    case AnnotatedSelect: {
        AnnotatedSelectImmediates immediates;
        WASM_FAIL_IF_HELPER_FAILS(parseAnnotatedSelectImmediates(immediates));

        TypedExpression condition;
        TypedExpression zero;
        TypedExpression nonZero;

        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "select condition");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(zero, "select zero");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(nonZero, "select non-zero");

        WASM_VALIDATOR_FAIL_IF(!condition.type().isI32(), "select condition must be i32, got ", condition.type());
        WASM_VALIDATOR_FAIL_IF(nonZero.type() != immediates.targetType, "select result types must match, got ", nonZero.type(), " and ", immediates.targetType);
        WASM_VALIDATOR_FAIL_IF(zero.type() != immediates.targetType, "select result types must match, got ", zero.type(), " and ", immediates.targetType);

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addSelect(condition, nonZero, zero, result));

        m_expressionStack.constructAndAppend(immediates.targetType, result);
        return { };
    }

#define CREATE_CASE(name, id, b3op, inc, memoryType) case OpType::name: return load(Types::memoryType);
FOR_EACH_WASM_MEMORY_LOAD_OP(CREATE_CASE)
#undef CREATE_CASE

#define CREATE_CASE(name, id, b3op, inc, memoryType) case OpType::name: return store(Types::memoryType);
FOR_EACH_WASM_MEMORY_STORE_OP(CREATE_CASE)
#undef CREATE_CASE

    case F32Const: {
        uint32_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt32(constant), "can't parse 32-bit floating-point constant");
        m_expressionStack.constructAndAppend(Types::F32, m_context.addConstant(Types::F32, constant));
        return { };
    }

    case I32Const: {
        int32_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt32(constant), "can't parse 32-bit constant");
        m_expressionStack.constructAndAppend(Types::I32, m_context.addConstant(Types::I32, constant));
        return { };
    }

    case F64Const: {
        uint64_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt64(constant), "can't parse 64-bit floating-point constant");
        m_expressionStack.constructAndAppend(Types::F64, m_context.addConstant(Types::F64, constant));
        return { };
    }

    case I64Const: {
        int64_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt64(constant), "can't parse 64-bit constant");
        m_expressionStack.constructAndAppend(Types::I64, m_context.addConstant(Types::I64, constant));
        return { };
    }

    case TableGet: {
        unsigned tableIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't parse table index");
        WASM_VALIDATOR_FAIL_IF(tableIndex >= m_info.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_info.tableCount());

        TypedExpression index;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(index, "table.get");
        WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != index.type().kind, "table.get index to type ", index.type(), " expected ", TypeKind::I32);

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addTableGet(tableIndex, index, result));
        Type resultType = m_info.tables[tableIndex].wasmType();
        m_expressionStack.constructAndAppend(resultType, result);
        return { };
    }

    case TableSet: {
        unsigned tableIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't parse table index");
        WASM_VALIDATOR_FAIL_IF(tableIndex >= m_info.tableCount(), "table index ", tableIndex, " is invalid, limit is ", m_info.tableCount());
        TypedExpression value, index;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "table.set");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(index, "table.set");
        WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != index.type().kind, "table.set index to type ", index.type(), " expected ", TypeKind::I32);
        Type type = m_info.tables[tableIndex].wasmType();
        WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), type), "table.set value to type ", value.type(), " expected ", type);
        RELEASE_ASSERT(m_info.tables[tableIndex].type() == TableElementType::Externref || m_info.tables[tableIndex].type() == TableElementType::Funcref);
        WASM_TRY_ADD_TO_CONTEXT(addTableSet(tableIndex, index, value));
        return { };
    }

    case Ext1: {
        uint32_t extOp;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(extOp), "can't parse 0xfc extended opcode");

        Ext1OpType op = static_cast<Ext1OpType>(extOp);
        switch (op) {
        case Ext1OpType::TableInit: {
            TableInitImmediates immediates;
            WASM_FAIL_IF_HELPER_FAILS(parseTableInitImmediates(immediates));

            WASM_VALIDATOR_FAIL_IF(m_info.tables[immediates.tableIndex].type() != m_info.elements[immediates.elementIndex].elementType, "table.init requires table's type \"", m_info.tables[immediates.tableIndex].type(), "\" and element's type \"", m_info.elements[immediates.elementIndex].elementType, "\" are the same");

            TypedExpression dstOffset;
            TypedExpression srcOffset;
            TypedExpression length;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(length, "table.init");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(srcOffset, "table.init");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(dstOffset, "table.init");

            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != dstOffset.type().kind, "table.init dst_offset to type ", dstOffset.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != srcOffset.type().kind, "table.init src_offset to type ", srcOffset.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != length.type().kind, "table.init length to type ", length.type(), " expected ", TypeKind::I32);

            WASM_TRY_ADD_TO_CONTEXT(addTableInit(immediates.elementIndex, immediates.tableIndex, dstOffset, srcOffset, length));
            break;
        }
        case Ext1OpType::ElemDrop: {
            unsigned elementIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseElementIndex(elementIndex));

            WASM_TRY_ADD_TO_CONTEXT(addElemDrop(elementIndex));
            break;
        }
        case Ext1OpType::TableSize: {
            unsigned tableIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseTableIndex(tableIndex));

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addTableSize(tableIndex, result));
            m_expressionStack.constructAndAppend(Types::I32, result);
            break;
        }
        case Ext1OpType::TableGrow: {
            unsigned tableIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseTableIndex(tableIndex));

            TypedExpression fill;
            TypedExpression delta;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(delta, "table.grow");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(fill, "table.grow");

            Type tableType = m_info.tables[tableIndex].wasmType();
            WASM_VALIDATOR_FAIL_IF(!isSubtype(fill.type(), tableType), "table.grow expects fill value of type ", tableType, " got ", fill.type());
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != delta.type().kind, "table.grow expects an i32 delta value, got ", delta.type());

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addTableGrow(tableIndex, fill, delta, result));
            m_expressionStack.constructAndAppend(Types::I32, result);
            break;
        }
        case Ext1OpType::TableFill: {
            unsigned tableIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseTableIndex(tableIndex));

            TypedExpression offset, fill, count;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(count, "table.fill");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(fill, "table.fill");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(offset, "table.fill");

            Type tableType = m_info.tables[tableIndex].wasmType();
            WASM_VALIDATOR_FAIL_IF(!isSubtype(fill.type(), tableType), "table.fill expects fill value of type ", tableType, " got ", fill.type());
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != offset.type().kind, "table.fill expects an i32 offset value, got ", offset.type());
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != count.type().kind, "table.fill expects an i32 count value, got ", count.type());

            WASM_TRY_ADD_TO_CONTEXT(addTableFill(tableIndex, offset, fill, count));
            break;
        }
        case Ext1OpType::TableCopy: {
            TableCopyImmediates immediates;
            WASM_FAIL_IF_HELPER_FAILS(parseTableCopyImmediates(immediates));

            const auto srcType = m_info.table(immediates.srcTableIndex).wasmType();
            const auto dstType = m_info.table(immediates.dstTableIndex).wasmType();
            WASM_VALIDATOR_FAIL_IF(srcType != dstType, "type mismatch at table.copy. got ", srcType, " and ", dstType);

            TypedExpression dstOffset;
            TypedExpression srcOffset;
            TypedExpression length;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(length, "table.copy");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(srcOffset, "table.copy");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(dstOffset, "table.copy");

            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != dstOffset.type().kind, "table.copy dst_offset to type ", dstOffset.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != srcOffset.type().kind, "table.copy src_offset to type ", srcOffset.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != length.type().kind, "table.copy length to type ", length.type(), " expected ", TypeKind::I32);

            WASM_TRY_ADD_TO_CONTEXT(addTableCopy(immediates.dstTableIndex, immediates.srcTableIndex, dstOffset, srcOffset, length));
            break;
        }
        case Ext1OpType::MemoryFill: {
            WASM_FAIL_IF_HELPER_FAILS(parseMemoryFillImmediate());

            WASM_VALIDATOR_FAIL_IF(!m_info.memoryCount(), "memory must be present");

            TypedExpression dstAddress;
            TypedExpression targetValue;
            TypedExpression count;

            WASM_TRY_POP_EXPRESSION_STACK_INTO(count, "memory.fill");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(targetValue, "memory.fill");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(dstAddress, "memory.fill");

            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != dstAddress.type().kind, "memory.fill dstAddress to type ", dstAddress.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != targetValue.type().kind, "memory.fill targetValue to type ", targetValue.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != count.type().kind, "memory.fill size to type ", count.type(), " expected ", TypeKind::I32);

            WASM_TRY_ADD_TO_CONTEXT(addMemoryFill(dstAddress, targetValue, count));
            break;
        }
        case Ext1OpType::MemoryCopy: {
            WASM_FAIL_IF_HELPER_FAILS(parseMemoryCopyImmediates());

            WASM_VALIDATOR_FAIL_IF(!m_info.memoryCount(), "memory must be present");

            TypedExpression dstAddress;
            TypedExpression srcAddress;
            TypedExpression count;

            WASM_TRY_POP_EXPRESSION_STACK_INTO(count, "memory.copy");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(srcAddress, "memory.copy");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(dstAddress, "memory.copy");

            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != dstAddress.type().kind, "memory.copy dstAddress to type ", dstAddress.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != srcAddress.type().kind, "memory.copy targetValue to type ", srcAddress.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != count.type().kind, "memory.copy size to type ", count.type(), " expected ", TypeKind::I32);

            WASM_TRY_ADD_TO_CONTEXT(addMemoryCopy(dstAddress, srcAddress, count));
            break;
        }
        case Ext1OpType::MemoryInit: {
            MemoryInitImmediates immediates;
            WASM_FAIL_IF_HELPER_FAILS(parseMemoryInitImmediates(immediates));

            TypedExpression dstAddress;
            TypedExpression srcAddress;
            TypedExpression length;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(length, "memory.init");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(srcAddress, "memory.init");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(dstAddress, "memory.init");

            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != dstAddress.type().kind, "memory.init dst address to type ", dstAddress.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != srcAddress.type().kind, "memory.init src address to type ", srcAddress.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != length.type().kind, "memory.init length to type ", length.type(), " expected ", TypeKind::I32);

            WASM_TRY_ADD_TO_CONTEXT(addMemoryInit(immediates.dataSegmentIndex, dstAddress, srcAddress, length));
            break;
        }
        case Ext1OpType::DataDrop: {
            unsigned dataSegmentIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseDataSegmentIndex(dataSegmentIndex));

            WASM_TRY_ADD_TO_CONTEXT(addDataDrop(dataSegmentIndex));
            break;
        }

#define CREATE_CASE(name, id, b3op, inc, operandType, returnType) case Ext1OpType::name: return truncSaturated(op, Types::returnType, Types::operandType);
        FOR_EACH_WASM_TRUNC_SATURATED_OP(CREATE_CASE)
#undef CREATE_CASE

        default:
            WASM_PARSER_FAIL_IF(true, "invalid 0xfc extended op ", extOp);
            break;
        }
        return { };
    }

    case ExtGC: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");

        uint32_t extOp;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(extOp), "can't parse extended GC opcode");

        ExtGCOpType op = static_cast<ExtGCOpType>(extOp);
        switch (op) {
        case ExtGCOpType::I31New: {
            TypedExpression value;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "i31.new");
            WASM_VALIDATOR_FAIL_IF(!value.type().isI32(), "i31.new value to type ", value.type(), " expected ", TypeKind::I32);

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addI31New(value, result));

            m_expressionStack.constructAndAppend(Type { TypeKind::Ref, static_cast<TypeIndex>(TypeKind::I31ref) }, result);
            return { };
        }
        case ExtGCOpType::I31GetS: {
            TypedExpression ref;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(ref, "i31.get_s");
            WASM_VALIDATOR_FAIL_IF(!isI31ref(ref.type()), "i31.get_s ref to type ", ref.type(), " expected ", TypeKind::I31ref);

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addI31GetS(ref, result));

            m_expressionStack.constructAndAppend(Types::I32, result);
            return { };
        }
        case ExtGCOpType::I31GetU: {
            TypedExpression ref;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(ref, "i31.get_u");
            WASM_VALIDATOR_FAIL_IF(!isI31ref(ref.type()), "i31.get_u ref to type ", ref.type(), " expected ", TypeKind::I31ref);

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addI31GetU(ref, result));

            m_expressionStack.constructAndAppend(Types::I32, result);
            return { };
        }
        case ExtGCOpType::ArrayNew: {
            uint32_t typeIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(typeIndex), "can't get type index for array.new");
            WASM_VALIDATOR_FAIL_IF(typeIndex >= m_info.typeCount(), "array.new index ", typeIndex, " is out of bounds");

            const TypeDefinition& typeDefinition = m_info.typeSignatures[typeIndex].get();
            WASM_VALIDATOR_FAIL_IF(!typeDefinition.is<ArrayType>(), "array.new index ", typeIndex, " does not reference an array definition");
            // If this is a packed array, then the value has to have type i32
            const Type unpackedElementType = typeDefinition.as<ArrayType>()->elementType().type.unpacked();

            TypedExpression value, size;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(size, "array.new");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "array.new");
            WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), unpackedElementType), "array.new value to type ", value.type(), " expected ", unpackedElementType);
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != size.type().kind, "array.new index to type ", size.type(), " expected ", TypeKind::I32);

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addArrayNew(typeIndex, size, value, result));

            m_expressionStack.constructAndAppend(Type { TypeKind::Ref, TypeInformation::get(typeDefinition) }, result);
            return { };
        }
        case ExtGCOpType::ArrayNewDefault: {
            uint32_t typeIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(typeIndex), "can't get type index for array.new_default");
            WASM_VALIDATOR_FAIL_IF(typeIndex >= m_info.typeCount(), "array.new_default index ", typeIndex, " is out of bounds");

            const TypeDefinition& typeDefinition = m_info.typeSignatures[typeIndex].get();
            WASM_VALIDATOR_FAIL_IF(!typeDefinition.is<ArrayType>(), "array.new_default index ", typeIndex, " does not reference an array definition");
            const StorageType elementType = typeDefinition.as<ArrayType>()->elementType().type;
            WASM_VALIDATOR_FAIL_IF(!isDefaultableType(elementType), "array.new_default index ", typeIndex, " does not reference an array definition with a defaultable type");

            TypedExpression size;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(size, "array.new_default");
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != size.type().kind, "array.new_default index to type ", size.type(), " expected ", TypeKind::I32);

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addArrayNewDefault(typeIndex, size, result));

            m_expressionStack.constructAndAppend(Type { TypeKind::Ref, TypeInformation::get(typeDefinition) }, result);
            return { };
        }
        case ExtGCOpType::ArrayGet:
        case ExtGCOpType::ArrayGetS:
        case ExtGCOpType::ArrayGetU: {
            uint32_t typeIndex;
            const char* opName = op == ExtGCOpType::ArrayGet ? "array.get" : op == ExtGCOpType::ArrayGetS ? "array.get_s" : "array.get_u";
            WASM_PARSER_FAIL_IF(!parseVarUInt32(typeIndex), "can't get type index for ", opName);
            WASM_VALIDATOR_FAIL_IF(typeIndex >= m_info.typeCount(), opName, " index ", typeIndex, " is out of bounds");

            const TypeDefinition& typeDefinition = m_info.typeSignatures[typeIndex].get();
            WASM_VALIDATOR_FAIL_IF(!typeDefinition.is<ArrayType>(), opName, " index ", typeIndex, " does not reference an array definition");
            const StorageType elementType = typeDefinition.as<ArrayType>()->elementType().type;
            // The type of the result will be unpacked if the array is packed.
            const Type resultType = elementType.unpacked();

            // array.get_s and array.get_u are only valid for packed arrays
            if (op == ExtGCOpType::ArrayGetS || op == ExtGCOpType::ArrayGetU)
                WASM_PARSER_FAIL_IF(!elementType.is<PackedType>(), opName, " applied to wrong type of array -- expected: i8 or i16, found ", elementType.as<Type>().kind);

            // array.get is not valid for packed arrays
            if (op == ExtGCOpType::ArrayGet)
                WASM_PARSER_FAIL_IF(elementType.is<PackedType>(), opName, " applied to packed array of ", elementType.as<PackedType>(), " -- use array.get_s or array.get_u");

            TypedExpression arrayref, index;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(index, "array.get");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(arrayref, "array.get");
            WASM_VALIDATOR_FAIL_IF(!isSubtype(arrayref.type(), Type { TypeKind::RefNull, TypeInformation::get(typeDefinition) }), opName, " arrayref to type ", arrayref.type().kind, " expected arrayref");
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != index.type().kind, "array.get index to type ", index.type(), " expected ", TypeKind::I32);

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addArrayGet(op, typeIndex, arrayref, index, result));

            m_expressionStack.constructAndAppend(resultType, result);
            return { };
        }
        case ExtGCOpType::ArraySet: {
            uint32_t typeIndex;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(typeIndex), "can't get type index for array.set");
            WASM_VALIDATOR_FAIL_IF(typeIndex >= m_info.typeCount(), "array.set index ", typeIndex, " is out of bounds");

            const TypeDefinition& typeDefinition = m_info.typeSignatures[typeIndex].get();
            WASM_VALIDATOR_FAIL_IF(!typeDefinition.is<ArrayType>(), "array.set index ", typeIndex, " does not reference an array definition");
            WASM_VALIDATOR_FAIL_IF(!typeDefinition.is<ArrayType>(), "array.set index ", typeIndex, " does not reference an array definition");
            const FieldType elementType = typeDefinition.as<ArrayType>()->elementType();
            const Type unpackedElementType = elementType.type.unpacked();

            WASM_VALIDATOR_FAIL_IF(elementType.mutability != Mutability::Mutable, "array.set index ", typeIndex, " does not reference a mutable array definition");

            TypedExpression arrayref, index, value;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "array.set");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(index, "array.set");
            WASM_TRY_POP_EXPRESSION_STACK_INTO(arrayref, "array.set");
            WASM_VALIDATOR_FAIL_IF(!isSubtype(arrayref.type(), Type { TypeKind::RefNull, TypeInformation::get(typeDefinition) }), "array.set arrayref to type ", arrayref.type(), " expected arrayref");
            WASM_VALIDATOR_FAIL_IF(TypeKind::I32 != index.type().kind, "array.set index to type ", index.type(), " expected ", TypeKind::I32);
            WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), unpackedElementType), "array.set value to type ", value.type(), " expected ", unpackedElementType);

            WASM_TRY_ADD_TO_CONTEXT(addArraySet(typeIndex, arrayref, index, value));

            return { };
        }
        case ExtGCOpType::ArrayLen: {
            TypedExpression arrayref;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(arrayref, "array.len");
            WASM_VALIDATOR_FAIL_IF(!isSubtype(arrayref.type(), Type { TypeKind::RefNull, static_cast<TypeIndex>(TypeKind::Arrayref) }), "array.len value to type ", arrayref.type(), " expected arrayref");

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addArrayLen(arrayref, result));

            m_expressionStack.constructAndAppend(Types::I32, result);
            return { };
        }
        // The struct.new and struct.new_canon instructions are identical but with different opcodes for compatibility with both the spec & other implementations.
        // FIXME: Remove this redundancy when the GC proposal's opcode numbering is finalized.
        case ExtGCOpType::StructNew:
        case ExtGCOpType::StructNewCanon: {
            uint32_t typeIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseStructTypeIndex(typeIndex, "struct.new"));

            const auto& typeDefinition = m_info.typeSignatures[typeIndex];
            const auto* structType = typeDefinition->template as<StructType>();
            WASM_PARSER_FAIL_IF(structType->fieldCount() > m_expressionStack.size(), "struct.new ", typeIndex, " requires ", structType->fieldCount(), " values, but the expression stack currently holds ", m_expressionStack.size(), " values");

            Vector<ExpressionType> args;
            size_t firstArgumentIndex = m_expressionStack.size() - structType->fieldCount();
            WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(structType->fieldCount()), "can't allocate enough memory for struct.new ", structType->fieldCount(), " values");

            for (size_t i = firstArgumentIndex; i < m_expressionStack.size(); ++i) {
                TypedExpression arg = m_expressionStack.at(i);
                const auto& fieldType = structType->field(StructFieldCount(i - firstArgumentIndex)).type.unpacked();
                WASM_VALIDATOR_FAIL_IF(!isSubtype(arg.type(), fieldType), "argument type mismatch in struct.new, got ", arg.type(), ", expected ", fieldType);
                args.uncheckedAppend(arg);
                m_context.didPopValueFromStack();
            }
            m_expressionStack.shrink(firstArgumentIndex);
            RELEASE_ASSERT(structType->fieldCount() == args.size());

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addStructNew(typeIndex, args, result));
            m_expressionStack.constructAndAppend(Type { TypeKind::Ref, typeDefinition->index() }, result);
            return { };
        }
        case ExtGCOpType::StructNewDefault:
        case ExtGCOpType::StructNewCanonDefault: {
            uint32_t typeIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseStructTypeIndex(typeIndex, "struct.new_default"));

            const auto& typeDefinition = m_info.typeSignatures[typeIndex];
            const auto* structType = typeDefinition->template as<StructType>();

            for (StructFieldCount i = 0; i < structType->fieldCount(); i++)
                WASM_PARSER_FAIL_IF(!isDefaultableType(structType->field(i).type), "struct.new_default ", typeIndex, " requires all fields to be defaultable, but field ", i, " has type ", structType->field(i).type);

            ExpressionType result;
            WASM_TRY_ADD_TO_CONTEXT(addStructNewDefault(typeIndex, result));
            m_expressionStack.constructAndAppend(Type { TypeKind::Ref, typeDefinition->index() }, result);
            return { };
        }
        case ExtGCOpType::StructGet: {
            StructFieldManipulation structGetInput;
            WASM_PARSER_FAIL_IF(!parseStructFieldManipulation(structGetInput, "struct.get"));

            ExpressionType result;
            const auto& structType = *m_info.typeSignatures[structGetInput.indices.structTypeIndex]->template as<StructType>();
            WASM_TRY_ADD_TO_CONTEXT(addStructGet(structGetInput.structReference, structType, structGetInput.indices.fieldIndex, result));

            m_expressionStack.constructAndAppend(structGetInput.field.type.unpacked(), result);
            return { };
        }
        case ExtGCOpType::StructSet: {
            TypedExpression value;
            WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "struct.set value");

            StructFieldManipulation structSetInput;
            WASM_PARSER_FAIL_IF(!parseStructFieldManipulation(structSetInput, "struct.set"));

            const auto& field = structSetInput.field;
            WASM_PARSER_FAIL_IF(field.mutability != Mutability::Mutable, "the field ", structSetInput.indices.fieldIndex, " can't be set because it is immutable");
            WASM_PARSER_FAIL_IF(!isSubtype(value.type(), field.type.unpacked()), "type mismatch in struct.set");

            const auto& structType = *m_info.typeSignatures[structSetInput.indices.structTypeIndex]->template as<StructType>();
            WASM_TRY_ADD_TO_CONTEXT(addStructSet(structSetInput.structReference, structType, structSetInput.indices.fieldIndex, value));
            return { };
        }
        default:
            WASM_PARSER_FAIL_IF(true, "invalid extended GC op ", extOp);
            break;
        }
        return { };
    }

    case ExtAtomic: {
        uint32_t extOp;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(extOp), "can't parse atomic extended opcode");

        ExtAtomicOpType op = static_cast<ExtAtomicOpType>(extOp);

        if (UNLIKELY(Options::dumpWasmOpcodeStatistics()))
            WasmOpcodeCounter::singleton().increment(op);

        switch (op) {
#define CREATE_CASE(name, id, b3op, inc, memoryType) case ExtAtomicOpType::name: return atomicLoad(op, Types::memoryType);
        FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(CREATE_CASE)
#undef CREATE_CASE
#define CREATE_CASE(name, id, b3op, inc, memoryType) case ExtAtomicOpType::name: return atomicStore(op, Types::memoryType);
        FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(CREATE_CASE)
#undef CREATE_CASE
#define CREATE_CASE(name, id, b3op, inc, memoryType) case ExtAtomicOpType::name: return atomicBinaryRMW(op, Types::memoryType);
        FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(CREATE_CASE)
#undef CREATE_CASE
        case ExtAtomicOpType::MemoryAtomicWait64:
            return atomicWait(op, Types::I64);
        case ExtAtomicOpType::MemoryAtomicWait32:
            return atomicWait(op, Types::I32);
        case ExtAtomicOpType::MemoryAtomicNotify:
            return atomicNotify(op);
        case ExtAtomicOpType::AtomicFence:
            return atomicFence(op);
        case ExtAtomicOpType::I32AtomicRmw8CmpxchgU:
        case ExtAtomicOpType::I32AtomicRmw16CmpxchgU:
        case ExtAtomicOpType::I32AtomicRmwCmpxchg:
            return atomicCompareExchange(op, Types::I32);
        case ExtAtomicOpType::I64AtomicRmw8CmpxchgU:
        case ExtAtomicOpType::I64AtomicRmw16CmpxchgU:
        case ExtAtomicOpType::I64AtomicRmw32CmpxchgU:
        case ExtAtomicOpType::I64AtomicRmwCmpxchg:
            return atomicCompareExchange(op, Types::I64);
        default:
            WASM_PARSER_FAIL_IF(true, "invalid extended atomic op ", extOp);
            break;
        }
        return { };
    }

    case RefNull: {
        Type typeOfNull;
        if (Options::useWebAssemblyTypedFunctionReferences()) {
            int32_t heapType;
            WASM_PARSER_FAIL_IF(!parseHeapType(m_info, heapType), "ref.null heaptype must be funcref, externref or type_idx");
            if (isTypeIndexHeapType(heapType)) {
                TypeIndex typeIndex = TypeInformation::get(m_info.typeSignatures[heapType].get());
                typeOfNull = Type { TypeKind::RefNull, typeIndex };
            } else
                typeOfNull = Type { TypeKind::RefNull, static_cast<TypeIndex>(heapType) };
        } else
            WASM_PARSER_FAIL_IF(!parseRefType(m_info, typeOfNull), "ref.null type must be a reference type");
        m_expressionStack.constructAndAppend(typeOfNull, m_context.addConstant(typeOfNull, JSValue::encode(jsNull())));
        return { };
    }

    case RefIsNull: {
        TypedExpression value;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "ref.is_null");
        WASM_VALIDATOR_FAIL_IF(!isRefType(value.type()), "ref.is_null to type ", value.type(), " expected a reference type");
        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addRefIsNull(value, result));
        m_expressionStack.constructAndAppend(Types::I32, result);
        return { };
    }

    case RefFunc: {
        uint32_t index;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for ref.func");
        WASM_VALIDATOR_FAIL_IF(index >= m_info.functionIndexSpaceSize(), "ref.func index ", index, " is too large, max is ", m_info.functionIndexSpaceSize());
        WASM_VALIDATOR_FAIL_IF(!m_info.isDeclaredFunction(index), "ref.func index ", index, " isn't declared");
        m_info.addReferencedFunction(index);

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addRefFunc(index, result));

        if (Options::useWebAssemblyTypedFunctionReferences()) {
            TypeIndex typeIndex = m_info.typeIndexFromFunctionIndexSpace(index);
            m_expressionStack.constructAndAppend(Type { TypeKind::Ref, typeIndex }, result);
            return { };
        }

        m_expressionStack.constructAndAppend(Types::Funcref, result);
        return { };
    }

    case GetLocal: {
        uint32_t index;
        WASM_FAIL_IF_HELPER_FAILS(parseIndexForLocal(index));

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(getLocal(index, result));
        m_expressionStack.constructAndAppend(m_locals[index], result);
        return { };
    }

    case SetLocal: {
        uint32_t index;
        WASM_FAIL_IF_HELPER_FAILS(parseIndexForLocal(index));

        TypedExpression value;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "set_local");
        WASM_VALIDATOR_FAIL_IF(index >= m_locals.size(), "attempt to set unknown local ", index, ", the number of locals is ", m_locals.size());
        WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), m_locals[index]), "set_local to type ", value.type(), " expected ", m_locals[index]);
        WASM_TRY_ADD_TO_CONTEXT(setLocal(index, value));
        return { };
    }

    case TeeLocal: {
        uint32_t index;
        WASM_FAIL_IF_HELPER_FAILS(parseIndexForLocal(index));

        WASM_PARSER_FAIL_IF(m_expressionStack.isEmpty(), "can't tee_local on empty expression stack");
        TypedExpression value = m_expressionStack.last();
        WASM_VALIDATOR_FAIL_IF(index >= m_locals.size(), "attempt to tee unknown local ", index, ", the number of locals is ", m_locals.size());
        WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), m_locals[index]), "set_local to type ", value.type(), " expected ", m_locals[index]);
        WASM_TRY_ADD_TO_CONTEXT(setLocal(index, value));
        return { };
    }

    case GetGlobal: {
        uint32_t index;
        WASM_FAIL_IF_HELPER_FAILS(parseIndexForGlobal(index));
        Type resultType = m_info.globals[index].type;
        ASSERT(isValueType(resultType));

        if (resultType.isV128()) {
            m_context.notifyFunctionUsesSIMD();
            if (!Context::tierSupportsSIMD)
                WASM_TRY_ADD_TO_CONTEXT(addCrash());
        }

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(getGlobal(index, result));
        m_expressionStack.constructAndAppend(resultType, result);
        return { };
    }

    case SetGlobal: {
        uint32_t index;
        WASM_FAIL_IF_HELPER_FAILS(parseIndexForGlobal(index));
        WASM_VALIDATOR_FAIL_IF(index >= m_info.globals.size(), "set_global ", index, " of unknown global, limit is ", m_info.globals.size());
        WASM_VALIDATOR_FAIL_IF(m_info.globals[index].mutability == Mutability::Immutable, "set_global ", index, " is immutable");

        TypedExpression value;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "set_global value");

        Type globalType = m_info.globals[index].type;
        ASSERT(isValueType(globalType));

        WASM_VALIDATOR_FAIL_IF(!isSubtype(value.type(), globalType), "set_global ", index, " with type ", globalType.kind, " with a variable of type ", value.type().kind);

        if (globalType.isV128()) {
            m_context.notifyFunctionUsesSIMD();
            if (!Context::tierSupportsSIMD)
                WASM_TRY_ADD_TO_CONTEXT(addCrash());
        }

        WASM_TRY_ADD_TO_CONTEXT(setGlobal(index, value));
        return { };
    }

    case TailCall:
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyTailCalls(), "wasm tail calls are not enabled");
        FALLTHROUGH;
    case Call: {
        uint32_t functionIndex;
        WASM_FAIL_IF_HELPER_FAILS(parseFunctionIndex(functionIndex));

        TypeIndex calleeTypeIndex = m_info.typeIndexFromFunctionIndexSpace(functionIndex);
        const TypeDefinition& typeDefinition = TypeInformation::get(calleeTypeIndex).expand();
        const auto& calleeSignature = *typeDefinition.as<FunctionSignature>();
        WASM_PARSER_FAIL_IF(calleeSignature.argumentCount() > m_expressionStack.size(), "call function index ", functionIndex, " has ", calleeSignature.argumentCount(), " arguments, but the expression stack currently holds ", m_expressionStack.size(), " values");

        size_t firstArgumentIndex = m_expressionStack.size() - calleeSignature.argumentCount();
        Vector<ExpressionType> args;
        WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(calleeSignature.argumentCount()), "can't allocate enough memory for call's ", calleeSignature.argumentCount(), " arguments");
        for (size_t i = firstArgumentIndex; i < m_expressionStack.size(); ++i) {
            TypedExpression arg = m_expressionStack.at(i);
            WASM_VALIDATOR_FAIL_IF(!isSubtype(arg.type(), calleeSignature.argumentType(i - firstArgumentIndex)), "argument type mismatch in call, got ", arg.type(), ", expected ", calleeSignature.argumentType(i - firstArgumentIndex));
            args.uncheckedAppend(arg);
            m_context.didPopValueFromStack();
        }
        m_expressionStack.shrink(firstArgumentIndex);

        RELEASE_ASSERT(calleeSignature.argumentCount() == args.size());

        ResultList results;

        if (m_currentOpcode == TailCall) {

            const auto& callerSignature = *m_signature.as<FunctionSignature>();

            WASM_PARSER_FAIL_IF(calleeSignature.returnCount() != callerSignature.returnCount(), "tail call function index ", functionIndex, " with return count ", calleeSignature.returnCount(), ", but the caller's signature has ", callerSignature.returnCount(), " return values");

            for (unsigned i = 0; i < calleeSignature.returnCount(); ++i)
                WASM_VALIDATOR_FAIL_IF(calleeSignature.returnType(i) != callerSignature.returnType(i), "tail call function index ", functionIndex, " return type mismatch: " , "expected ", callerSignature.returnType(i), ", got ", calleeSignature.returnType(i));

            WASM_TRY_ADD_TO_CONTEXT(addCall(functionIndex, typeDefinition, args, results, CallType::TailCall));

            m_unreachableBlocks = 1;

            return { };
        }

        WASM_TRY_ADD_TO_CONTEXT(addCall(functionIndex, typeDefinition, args, results));
        RELEASE_ASSERT(calleeSignature.returnCount() == results.size());

        for (unsigned i = 0; i < calleeSignature.returnCount(); ++i)
            m_expressionStack.constructAndAppend(calleeSignature.returnType(i), results[i]);

        return { };
    }

    case TailCallIndirect:
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyTailCalls(), "wasm tail calls are not enabled");
        FALLTHROUGH;
    case CallIndirect: {
        uint32_t signatureIndex;
        uint32_t tableIndex;
        WASM_PARSER_FAIL_IF(!m_info.tableCount(), "call_indirect is only valid when a table is defined or imported");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(signatureIndex), "can't get call_indirect's signature index");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't get call_indirect's table index");
        WASM_PARSER_FAIL_IF(tableIndex >= m_info.tableCount(), "call_indirect's table index ", tableIndex, " invalid, limit is ", m_info.tableCount());
        WASM_PARSER_FAIL_IF(m_info.typeCount() <= signatureIndex, "call_indirect's signature index ", signatureIndex, " exceeds known signatures ", m_info.typeCount());
        WASM_PARSER_FAIL_IF(m_info.tables[tableIndex].type() != TableElementType::Funcref, "call_indirect is only valid when a table has type funcref");

        const TypeDefinition& typeDefinition = m_info.typeSignatures[signatureIndex].get();
        const auto& calleeSignature = *typeDefinition.expand().as<FunctionSignature>();
        size_t argumentCount = calleeSignature.argumentCount() + 1; // Add the callee's index.
        WASM_PARSER_FAIL_IF(argumentCount > m_expressionStack.size(), "call_indirect expects ", argumentCount, " arguments, but the expression stack currently holds ", m_expressionStack.size(), " values");

        WASM_VALIDATOR_FAIL_IF(!m_expressionStack.last().type().isI32(), "non-i32 call_indirect index ", m_expressionStack.last().type());

        Vector<ExpressionType> args;
        WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(argumentCount), "can't allocate enough memory for ", argumentCount, " call_indirect arguments");
        size_t firstArgumentIndex = m_expressionStack.size() - argumentCount;
        for (size_t i = firstArgumentIndex; i < m_expressionStack.size(); ++i) {
            TypedExpression arg = m_expressionStack.at(i);
            if (i < m_expressionStack.size() - 1)
                WASM_VALIDATOR_FAIL_IF(!isSubtype(arg.type(), calleeSignature.argumentType(i - firstArgumentIndex)), "argument type mismatch in call_indirect, got ", arg.type(), ", expected ", calleeSignature.argumentType(i - firstArgumentIndex));
            args.uncheckedAppend(arg);
            m_context.didPopValueFromStack();
        }
        m_expressionStack.shrink(firstArgumentIndex);

        ResultList results;

        if (m_currentOpcode == TailCallIndirect) {

            const auto& callerSignature = *m_signature.as<FunctionSignature>();

            WASM_PARSER_FAIL_IF(calleeSignature.returnCount() != callerSignature.returnCount(), "tail call indirect function with return count ", calleeSignature.returnCount(), ", but the caller's signature has ", callerSignature.returnCount(), " return values");

            for (unsigned i = 0; i < calleeSignature.returnCount(); ++i)
                WASM_VALIDATOR_FAIL_IF(calleeSignature.returnType(i) != callerSignature.returnType(i), "tail call indirect return type mismatch: " , "expected ", callerSignature.returnType(i), ", got ", calleeSignature.returnType(i));

            WASM_TRY_ADD_TO_CONTEXT(addCallIndirect(tableIndex, typeDefinition, args, results, CallType::TailCall));

            m_unreachableBlocks = 1;

            return { };
        }

        WASM_TRY_ADD_TO_CONTEXT(addCallIndirect(tableIndex, typeDefinition, args, results));

        for (unsigned i = 0; i < calleeSignature.returnCount(); ++i)
            m_expressionStack.constructAndAppend(calleeSignature.returnType(i), results[i]);

        return { };
    }

    case CallRef: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyTypedFunctionReferences(), "function references are not enabled");

        uint32_t typeIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(typeIndex), "can't get call_ref's signature index");
        WASM_VALIDATOR_FAIL_IF(typeIndex >= m_info.typeCount(), "call_ref index ", typeIndex, " is out of bounds");

        WASM_PARSER_FAIL_IF(m_expressionStack.isEmpty(), "can't call_ref on empty expression stack");
        WASM_VALIDATOR_FAIL_IF(!isRefWithTypeIndex(m_expressionStack.last().type()), "non-funcref call_ref value ", m_expressionStack.last().type().kind);
        WASM_VALIDATOR_FAIL_IF(m_expressionStack.last().type().index != m_info.typeSignatures[typeIndex]->index(), "invalid type index for call_ref value");

        const TypeIndex calleeTypeIndex = m_expressionStack.last().type().index;
        const TypeDefinition& typeDefinition = TypeInformation::get(calleeTypeIndex);
        const auto& calleeSignature = *typeDefinition.expand().as<FunctionSignature>();
        size_t argumentCount = calleeSignature.argumentCount() + 1; // Add the callee's value.
        WASM_PARSER_FAIL_IF(argumentCount > m_expressionStack.size(), "call_ref expects ", argumentCount, " arguments, but the expression stack currently holds ", m_expressionStack.size(), " values");

        Vector<ExpressionType> args;
        WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(argumentCount + 1), "can't allocate enough memory for ", argumentCount, " call_indirect arguments");
        size_t firstArgumentIndex = m_expressionStack.size() - argumentCount;
        for (size_t i = firstArgumentIndex; i < m_expressionStack.size(); ++i) {
            TypedExpression arg = m_expressionStack.at(i);
            if (i < m_expressionStack.size() - 1)
                WASM_VALIDATOR_FAIL_IF(arg.type() != calleeSignature.argumentType(i - firstArgumentIndex), "argument type mismatch in call_indirect, got ", arg.type(), ", expected ", calleeSignature.argumentType(i - firstArgumentIndex));
            args.uncheckedAppend(arg);
            m_context.didPopValueFromStack();
        }
        m_expressionStack.shrink(firstArgumentIndex);

        ResultList results;
        WASM_TRY_ADD_TO_CONTEXT(addCallRef(typeDefinition, args, results));

        for (unsigned i = 0; i < calleeSignature.returnCount(); ++i)
            m_expressionStack.constructAndAppend(calleeSignature.returnType(i), results[i]);

        return { };
    }

    case Block: {
        BlockSignature inlineSignatureAsType;
        WASM_PARSER_FAIL_IF(!parseBlockSignature(m_info, inlineSignatureAsType), "can't get block's signature");

        BlockSignature expandedSignature = &inlineSignatureAsType->expand();
        const FunctionSignature* inlineSignature = expandedSignature->as<FunctionSignature>();
        WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < inlineSignature->argumentCount(), "Too few values on stack for block. Block expects ", inlineSignature->argumentCount(), ", but only ", m_expressionStack.size(), " were present. Block has inlineSignature: ", inlineSignature->toString());
        unsigned offset = m_expressionStack.size() - inlineSignature->argumentCount();
        for (unsigned i = 0; i < inlineSignature->argumentCount(); ++i) {
            Type type = m_expressionStack.at(offset + i).type();
            WASM_VALIDATOR_FAIL_IF(type != inlineSignature->argumentType(i), "Block expects the argument at index", i, " to be ", inlineSignature->argumentType(i), " but argument has type ", type);
        }

        int64_t oldSize = m_expressionStack.size();
        Stack newStack;
        ControlType block;
        WASM_TRY_ADD_TO_CONTEXT(addBlock(expandedSignature, m_expressionStack, block, newStack));
        ASSERT_UNUSED(oldSize, oldSize - m_expressionStack.size() == inlineSignature->argumentCount());
        ASSERT(newStack.size() == inlineSignature->argumentCount());

        m_controlStack.append({ WTFMove(m_expressionStack), { },  WTFMove(block) });
        m_expressionStack = WTFMove(newStack);
        return { };
    }

    case Loop: {
        BlockSignature inlineSignatureAsType;
        WASM_PARSER_FAIL_IF(!parseBlockSignature(m_info, inlineSignatureAsType), "can't get loop's signature");

        BlockSignature expandedSignature = &inlineSignatureAsType->expand();
        const FunctionSignature* inlineSignature = expandedSignature->as<FunctionSignature>();
        WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < inlineSignature->argumentCount(), "Too few values on stack for loop block. Loop expects ", inlineSignature->argumentCount(), ", but only ", m_expressionStack.size(), " were present. Loop has inlineSignature: ", inlineSignature->toString());
        unsigned offset = m_expressionStack.size() - inlineSignature->argumentCount();
        for (unsigned i = 0; i < inlineSignature->argumentCount(); ++i) {
            Type type = m_expressionStack.at(offset + i).type();
            WASM_VALIDATOR_FAIL_IF(type != inlineSignature->argumentType(i), "Loop expects the argument at index", i, " to be ", inlineSignature->argumentType(i), " but argument has type ", type);
        }

        int64_t oldSize = m_expressionStack.size();
        Stack newStack;
        ControlType loop;
        WASM_TRY_ADD_TO_CONTEXT(addLoop(expandedSignature, m_expressionStack, loop, newStack, m_loopIndex++));
        ASSERT_UNUSED(oldSize, oldSize - m_expressionStack.size() == inlineSignature->argumentCount());
        ASSERT(newStack.size() == inlineSignature->argumentCount());

        m_controlStack.append({ WTFMove(m_expressionStack), { }, WTFMove(loop) });
        m_expressionStack = WTFMove(newStack);
        return { };
    }

    case If: {
        BlockSignature inlineSignatureAsType;
        TypedExpression condition;
        WASM_PARSER_FAIL_IF(!parseBlockSignature(m_info, inlineSignatureAsType), "can't get if's signature");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "if condition");

        BlockSignature expandedSignature = &inlineSignatureAsType->expand();
        const FunctionSignature* inlineSignature = expandedSignature->as<FunctionSignature>();
        WASM_VALIDATOR_FAIL_IF(!condition.type().isI32(), "if condition must be i32, got ", condition.type());
        WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < inlineSignature->argumentCount(), "Too few arguments on stack for if block. If expects ", inlineSignature->argumentCount(), ", but only ", m_expressionStack.size(), " were present. If block has signature: ", inlineSignature->toString());
        unsigned offset = m_expressionStack.size() - inlineSignature->argumentCount();
        for (unsigned i = 0; i < inlineSignature->argumentCount(); ++i)
            WASM_VALIDATOR_FAIL_IF(m_expressionStack[offset + i].type() != inlineSignature->argumentType(i), "Loop expects the argument at index", i, " to be ", inlineSignature->argumentType(i), " but argument has type ", m_expressionStack[i].type());

        int64_t oldSize = m_expressionStack.size();
        Stack newStack;
        ControlType control;
        WASM_TRY_ADD_TO_CONTEXT(addIf(condition, expandedSignature, m_expressionStack, control, newStack));
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

    case Try: {
        BlockSignature inlineSignatureAsType;
        WASM_PARSER_FAIL_IF(!parseBlockSignature(m_info, inlineSignatureAsType), "can't get try's signature");

        BlockSignature expandedSignature = &inlineSignatureAsType->expand();
        const FunctionSignature* inlineSignature = expandedSignature->as<FunctionSignature>();
        WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < inlineSignature->argumentCount(), "Too few arguments on stack for try block. Trye expects ", inlineSignature->argumentCount(), ", but only ", m_expressionStack.size(), " were present. Try block has signature: ", inlineSignature->toString());
        unsigned offset = m_expressionStack.size() - inlineSignature->argumentCount();
        for (unsigned i = 0; i < inlineSignature->argumentCount(); ++i)
            WASM_VALIDATOR_FAIL_IF(m_expressionStack[offset + i].type() != inlineSignature->argumentType(i), "Try expects the argument at index", i, " to be ", inlineSignature->argumentType(i), " but argument has type ", m_expressionStack[i].type());

        int64_t oldSize = m_expressionStack.size();
        Stack newStack;
        ControlType control;
        WASM_TRY_ADD_TO_CONTEXT(addTry(expandedSignature, m_expressionStack, control, newStack));
        ASSERT_UNUSED(oldSize, oldSize - m_expressionStack.size() == inlineSignature->argumentCount());
        ASSERT(newStack.size() == inlineSignature->argumentCount());

        m_controlStack.append({ WTFMove(m_expressionStack), { }, WTFMove(control) });
        m_expressionStack = WTFMove(newStack);
        return { };
    }

    case Catch: {
        WASM_PARSER_FAIL_IF(m_controlStack.size() == 1, "can't use catch block at the top-level of a function");

        uint32_t exceptionIndex;
        WASM_FAIL_IF_HELPER_FAILS(parseExceptionIndex(exceptionIndex));
        TypeIndex typeIndex = m_info.typeIndexFromExceptionIndexSpace(exceptionIndex);
        const TypeDefinition& exceptionSignature = TypeInformation::get(typeIndex).expand();

        ControlEntry& controlEntry = m_controlStack.last();
        WASM_VALIDATOR_FAIL_IF(!isTryOrCatch(controlEntry.controlData), "catch block isn't associated to a try");
        WASM_FAIL_IF_HELPER_FAILS(unify(controlEntry.controlData));

        ResultList results;
        Stack preCatchStack;
        m_expressionStack.swap(preCatchStack);
        WASM_TRY_ADD_TO_CONTEXT(addCatch(exceptionIndex, exceptionSignature, preCatchStack, controlEntry.controlData, results));

        RELEASE_ASSERT(exceptionSignature.as<FunctionSignature>()->argumentCount() == results.size());
        for (unsigned i = 0; i < exceptionSignature.as<FunctionSignature>()->argumentCount(); ++i)
            m_expressionStack.constructAndAppend(exceptionSignature.as<FunctionSignature>()->argumentType(i), results[i]);
        return { };
    }

    case CatchAll: {
        WASM_PARSER_FAIL_IF(m_controlStack.size() == 1, "can't use catch block at the top-level of a function");

        ControlEntry& controlEntry = m_controlStack.last();

        WASM_VALIDATOR_FAIL_IF(!isTryOrCatch(controlEntry.controlData), "catch block isn't associated to a try");
        WASM_FAIL_IF_HELPER_FAILS(unify(controlEntry.controlData));

        ResultList results;
        Stack preCatchStack;
        m_expressionStack.swap(preCatchStack);
        WASM_TRY_ADD_TO_CONTEXT(addCatchAll(preCatchStack, controlEntry.controlData));
        return { };
    }

    case Delegate: {
        WASM_PARSER_FAIL_IF(m_controlStack.size() == 1, "can't use delegate at the top-level of a function");

        uint32_t target;
        WASM_FAIL_IF_HELPER_FAILS(parseDelegateTarget(target, /* unreachableBlocks */ 0));

        ControlEntry controlEntry = m_controlStack.takeLast();
        WASM_VALIDATOR_FAIL_IF(!ControlType::isTry(controlEntry.controlData), "delegate isn't associated to a try");

        ControlType& targetData = m_controlStack[m_controlStack.size() - 1 - target].controlData;
        WASM_VALIDATOR_FAIL_IF(!ControlType::isTry(targetData) && !ControlType::isTopLevel(targetData), "delegate target isn't a try or the top level block");

        WASM_TRY_ADD_TO_CONTEXT(addDelegate(targetData, controlEntry.controlData));
        WASM_FAIL_IF_HELPER_FAILS(unify(controlEntry.controlData));
        WASM_TRY_ADD_TO_CONTEXT(endBlock(controlEntry, m_expressionStack));
        m_expressionStack.swap(controlEntry.enclosedExpressionStack);
        return { };
    }

    case Throw: {
        uint32_t exceptionIndex;
        WASM_FAIL_IF_HELPER_FAILS(parseExceptionIndex(exceptionIndex));
        TypeIndex typeIndex = m_info.typeIndexFromExceptionIndexSpace(exceptionIndex);
        const auto& exceptionSignature = TypeInformation::getFunctionSignature(typeIndex);

        WASM_VALIDATOR_FAIL_IF(m_expressionStack.size() < exceptionSignature.argumentCount(), "Too few arguments on stack for the exception being thrown. The exception expects ", exceptionSignature.argumentCount(), ", but only ", m_expressionStack.size(), " were present. Exception has signature: ", exceptionSignature.toString());
        unsigned offset = m_expressionStack.size() - exceptionSignature.argumentCount();
        Vector<ExpressionType> args;
        WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(exceptionSignature.argumentCount()), "can't allocate enough memory for throw's ", exceptionSignature.argumentCount(), " arguments");
        for (unsigned i = 0; i < exceptionSignature.argumentCount(); ++i) {
            TypedExpression arg = m_expressionStack.at(offset + i);
            WASM_VALIDATOR_FAIL_IF(arg.type() != exceptionSignature.argumentType(i), "The exception being thrown expects the argument at index ", i, " to be ", exceptionSignature.argumentType(i), " but argument has type ", arg.type());
            args.uncheckedAppend(arg);
            m_context.didPopValueFromStack();
        }
        m_expressionStack.shrink(offset);

        WASM_TRY_ADD_TO_CONTEXT(addThrow(exceptionIndex, args, m_expressionStack));
        m_unreachableBlocks = 1;
        return { };
    }

    case Rethrow: {
        uint32_t target;
        WASM_FAIL_IF_HELPER_FAILS(parseBranchTarget(target));

        ControlType& data = m_controlStack[m_controlStack.size() - 1 - target].controlData;
        WASM_VALIDATOR_FAIL_IF(!ControlType::isAnyCatch(data), "rethrow doesn't refer to a catch block");

        WASM_TRY_ADD_TO_CONTEXT(addRethrow(target, data));
        m_unreachableBlocks = 1;
        return { };
    }

    case Br:
    case BrIf: {
        uint32_t target;
        WASM_FAIL_IF_HELPER_FAILS(parseBranchTarget(target));

        TypedExpression condition;
        if (m_currentOpcode == BrIf) {
            WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "br / br_if condition");
            WASM_VALIDATOR_FAIL_IF(!condition.type().isI32(), "conditional branch with non-i32 condition ", condition.type());
        } else {
            m_unreachableBlocks = 1;
            condition = TypedExpression { Types::Void, Context::emptyExpression() };
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
        WASM_VALIDATOR_FAIL_IF(!condition.type().isI32(), "br_table with non-i32 condition ", condition.type());

        for (unsigned i = 0; i < targets.size(); ++i) {
            ControlType* target = targets[i];
            WASM_VALIDATOR_FAIL_IF(defaultTarget.branchTargetArity() != target->branchTargetArity(), "br_table target type size mismatch. Default has size: ", defaultTarget.branchTargetArity(), "but target: ", i, " has size: ", target->branchTargetArity());
            for (unsigned type = 0; type < defaultTarget.branchTargetArity(); ++type)
                WASM_VALIDATOR_FAIL_IF(defaultTarget.branchTargetType(type) != target->branchTargetType(type), "br_table target type mismatch at offset ", type, " expected: ", defaultTarget.branchTargetType(type), " but saw: ", target->branchTargetType(type), " when targeting block: ", target->signature()->toString());
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
        WASM_PARSER_FAIL_IF(!parseUInt8(reserved), "can't parse reserved byte for grow_memory");
        WASM_PARSER_FAIL_IF(reserved != 0, "reserved byte for grow_memory must be zero");

        TypedExpression delta;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(delta, "expect an i32 argument to grow_memory on the stack");
        WASM_VALIDATOR_FAIL_IF(!delta.type().isI32(), "grow_memory with non-i32 delta argument has type: ", delta.type());

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addGrowMemory(delta, result));
        m_expressionStack.constructAndAppend(Types::I32, result);

        return { };
    }

    case CurrentMemory: {
        WASM_PARSER_FAIL_IF(!m_info.memory, "current_memory is only valid if a memory is defined or imported");

        uint8_t reserved;
        WASM_PARSER_FAIL_IF(!parseUInt8(reserved), "can't parse reserved byte for current_memory");
        WASM_PARSER_FAIL_IF(reserved != 0, "reserved byte for current_memory must be zero");

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addCurrentMemory(result));
        m_expressionStack.constructAndAppend(Types::I32, result);

        return { };
    }
#if ENABLE(B3_JIT)
    case ExtSIMD: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblySIMD(), "wasm-simd is not enabled");
        m_context.notifyFunctionUsesSIMD();
        uint32_t extOp;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(extOp), "can't parse wasm extended opcode");

        constexpr bool isReachable = true;

        ExtSIMDOpType op = static_cast<ExtSIMDOpType>(extOp);
        if (UNLIKELY(Options::dumpWasmOpcodeStatistics()))
            WasmOpcodeCounter::singleton().increment(op);

        switch (op) {
        #define CREATE_SIMD_CASE(name, _, laneOp, lane, signMode) case ExtSIMDOpType::name: return simd<isReachable>(SIMDLaneOperation::laneOp, lane, signMode);
        FOR_EACH_WASM_EXT_SIMD_GENERAL_OP(CREATE_SIMD_CASE)
        #undef CREATE_SIMD_CASE
        #define CREATE_SIMD_CASE(name, _, laneOp, lane, signMode, relArg) case ExtSIMDOpType::name: return simd<isReachable>(SIMDLaneOperation::laneOp, lane, signMode, relArg);
        FOR_EACH_WASM_EXT_SIMD_REL_OP(CREATE_SIMD_CASE)
        #undef CREATE_SIMD_CASE
        default:
            WASM_PARSER_FAIL_IF(true, "invalid extended simd op ", extOp);
            break;
        }
        return { };
    }
#else
    case ExtSIMD:
        WASM_PARSER_FAIL_IF(true, "wasm-simd is not supported");
        return { };
#endif
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

    case Catch: {
        uint32_t exceptionIndex;
        WASM_FAIL_IF_HELPER_FAILS(parseExceptionIndex(exceptionIndex));
        TypeIndex typeIndex = m_info.typeIndexFromExceptionIndexSpace(exceptionIndex);
        const TypeDefinition& exceptionSignature = TypeInformation::get(typeIndex).expand();

        if (m_unreachableBlocks > 1)
            return { };

        ControlEntry& data = m_controlStack.last();
        WASM_VALIDATOR_FAIL_IF(!isTryOrCatch(data.controlData), "catch block isn't associated to a try");

        m_unreachableBlocks = 0;
        m_expressionStack = { };
        ResultList results;
        WASM_TRY_ADD_TO_CONTEXT(addCatchToUnreachable(exceptionIndex, exceptionSignature, data.controlData, results));

        RELEASE_ASSERT(exceptionSignature.as<FunctionSignature>()->argumentCount() == results.size());
        for (unsigned i = 0; i < exceptionSignature.as<FunctionSignature>()->argumentCount(); ++i)
            m_expressionStack.constructAndAppend(exceptionSignature.as<FunctionSignature>()->argumentType(i), results[i]);
        return { };
    }

    case CatchAll: {
        if (m_unreachableBlocks > 1)
            return { };

        ControlEntry& data = m_controlStack.last();
        m_unreachableBlocks = 0;
        m_expressionStack = { };
        WASM_VALIDATOR_FAIL_IF(!isTryOrCatch(data.controlData), "catch block isn't associated to a try");
        WASM_TRY_ADD_TO_CONTEXT(addCatchAllToUnreachable(data.controlData));
        return { };
    }

    case Delegate: {
        WASM_PARSER_FAIL_IF(m_controlStack.size() == 1, "can't use delegate at the top-level of a function");

        uint32_t target;
        WASM_FAIL_IF_HELPER_FAILS(parseDelegateTarget(target, m_unreachableBlocks));

        if (m_unreachableBlocks == 1) {
            ControlEntry controlEntry = m_controlStack.takeLast();
            WASM_VALIDATOR_FAIL_IF(!ControlType::isTry(controlEntry.controlData), "delegate isn't associated to a try");

            ControlType& data = m_controlStack[m_controlStack.size() - 1 - target].controlData;
            WASM_VALIDATOR_FAIL_IF(!ControlType::isTry(data) && !ControlType::isTopLevel(data), "delegate target isn't a try block");

            WASM_TRY_ADD_TO_CONTEXT(addDelegateToUnreachable(data, controlEntry.controlData));
            Stack emptyStack;
            WASM_TRY_ADD_TO_CONTEXT(addEndToUnreachable(controlEntry, emptyStack));
            m_expressionStack.swap(controlEntry.enclosedExpressionStack);
        }
        m_unreachableBlocks--;
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
            } else {
                Stack emptyStack;
                WASM_TRY_ADD_TO_CONTEXT(addEndToUnreachable(data, emptyStack));
            }

            m_expressionStack.swap(data.enclosedExpressionStack);
        }
        m_unreachableBlocks--;
        return { };
    }

    case Try:
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

    case TailCallIndirect:
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyTailCalls(), "wasm tail calls are not enabled");
        FALLTHROUGH;
    case CallIndirect: {
        uint32_t unused;
        uint32_t unused2;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get call_indirect's signature index in unreachable context");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused2), "can't get call_indirect's reserved byte in unreachable context");
        return { };
    }

    case CallRef: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyTypedFunctionReferences(), "function references are not enabled");
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't call_ref's signature index in unreachable context");
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

    case GetLocal:
    case SetLocal:
    case TeeLocal: {
        uint32_t index;
        WASM_FAIL_IF_HELPER_FAILS(parseIndexForLocal(index));
        return { };
    }

    case GetGlobal:
    case SetGlobal: {
        uint32_t index;
        WASM_FAIL_IF_HELPER_FAILS(parseIndexForGlobal(index));
        return { };
    }

    case TailCall:
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyTailCalls(), "wasm tail calls are not enabled");
        FALLTHROUGH;
    case Call: {
        uint32_t functionIndex;
        WASM_FAIL_IF_HELPER_FAILS(parseFunctionIndex(functionIndex));
        return { };
    }

    case Rethrow: {
        uint32_t target;
        WASM_FAIL_IF_HELPER_FAILS(parseBranchTarget(target));

        ControlType& data = m_controlStack[m_controlStack.size() - 1 - target].controlData;
        WASM_VALIDATOR_FAIL_IF(!ControlType::isAnyCatch(data), "rethrow doesn't refer to a catch block");
        return { };
    }

    case Br:
    case BrIf: {
        uint32_t target;
        WASM_FAIL_IF_HELPER_FAILS(parseBranchTarget(target));
        return { };
    }

    case Throw: {
        uint32_t exceptionIndex;
        WASM_FAIL_IF_HELPER_FAILS(parseExceptionIndex(exceptionIndex));

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

    case Ext1: {
        uint32_t extOp;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(extOp), "can't parse extended 0xfc opcode");

        switch (static_cast<Ext1OpType>(extOp)) {
        case Ext1OpType::TableInit: {
            TableInitImmediates immediates;
            WASM_FAIL_IF_HELPER_FAILS(parseTableInitImmediates(immediates));
            return { };
        }
        case Ext1OpType::ElemDrop: {
            unsigned elementIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseElementIndex(elementIndex));
            return { };
        }
        case Ext1OpType::TableSize:
        case Ext1OpType::TableGrow:
        case Ext1OpType::TableFill: {
            unsigned tableIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseTableIndex(tableIndex));
            return { };
        }
        case Ext1OpType::TableCopy: {
            TableCopyImmediates immediates;
            WASM_FAIL_IF_HELPER_FAILS(parseTableCopyImmediates(immediates));
            return { };
        }
        case Ext1OpType::MemoryFill: {
            WASM_FAIL_IF_HELPER_FAILS(parseMemoryFillImmediate());
            return { };
        }
        case Ext1OpType::MemoryCopy: {
            WASM_FAIL_IF_HELPER_FAILS(parseMemoryCopyImmediates());
            return { };
        }
        case Ext1OpType::MemoryInit: {
            MemoryInitImmediates immediates;
            WASM_FAIL_IF_HELPER_FAILS(parseMemoryInitImmediates(immediates));
            return { };
        }
        case Ext1OpType::DataDrop: {
            unsigned dataSegmentIndex;
            WASM_FAIL_IF_HELPER_FAILS(parseDataSegmentIndex(dataSegmentIndex));
            return { };
        }
#define CREATE_EXT1_CASE(name, ...) case Ext1OpType::name:
        FOR_EACH_WASM_TRUNC_SATURATED_OP(CREATE_EXT1_CASE)
            return { };
#undef CREATE_EXT1_CASE
        default:
            WASM_PARSER_FAIL_IF(true, "invalid extended 0xfc op ", extOp);
            break;
        }

        return { };
    }

    case AnnotatedSelect: {
        AnnotatedSelectImmediates immediates;
        WASM_FAIL_IF_HELPER_FAILS(parseAnnotatedSelectImmediates(immediates));
        return { };
    }

    case TableGet:
    case TableSet: {
        unsigned tableIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(tableIndex), "can't parse table index");
        FALLTHROUGH;
    }
    case RefIsNull:
    case RefNull: {
        return { };
    }

    case RefFunc: {
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get immediate for ", m_currentOpcode, " in unreachable context");
        return { };
    }

    case ExtGC: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblyGC(), "Wasm GC is not enabled");

        uint32_t extOp;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(extOp), "can't parse extended GC opcode");

        ExtGCOpType op = static_cast<ExtGCOpType>(extOp);

        if (UNLIKELY(Options::dumpWasmOpcodeStatistics()))
            WasmOpcodeCounter::singleton().increment(op);

        switch (op) {
        case ExtGCOpType::I31New:
        case ExtGCOpType::I31GetS:
        case ExtGCOpType::I31GetU:
            return { };
        case ExtGCOpType::ArrayNew: {
            uint32_t unused;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get type index immediate for array.new in unreachable context");
            return { };
        }
        case ExtGCOpType::ArrayNewDefault: {
            uint32_t unused;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get type index immediate for array.new_default in unreachable context");
            return { };
        }
        case ExtGCOpType::ArrayGet: {
            uint32_t unused;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get type index immediate for array.get in unreachable context");
            return { };
        }
        case ExtGCOpType::ArrayGetS: {
            uint32_t unused;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get type index immediate for array.get_s in unreachable context");
            return { };
        }
        case ExtGCOpType::ArrayGetU: {
            uint32_t unused;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get type index immediate for array.get_u in unreachable context");
            return { };
        }
        case ExtGCOpType::ArraySet: {
            uint32_t unused;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get type index immediate for array.set in unreachable context");
            return { };
        }
        case ExtGCOpType::ArrayLen:
            return { };
        case ExtGCOpType::StructNew:
        case ExtGCOpType::StructNewCanon: {
            uint32_t unused;
            WASM_FAIL_IF_HELPER_FAILS(parseStructTypeIndex(unused, "struct.new"));
            return { };
        }
        case ExtGCOpType::StructNewDefault:
        case ExtGCOpType::StructNewCanonDefault: {
            uint32_t unused;
            WASM_FAIL_IF_HELPER_FAILS(parseStructTypeIndex(unused, "struct.new_default"));
            return { };
        }
        case ExtGCOpType::StructGet: {
            StructTypeIndexAndFieldIndex unused;
            WASM_FAIL_IF_HELPER_FAILS(parseStructTypeIndexAndFieldIndex(unused, "struct.get"));
            return { };
        }
        case ExtGCOpType::StructSet: {
            StructTypeIndexAndFieldIndex unused;
            WASM_FAIL_IF_HELPER_FAILS(parseStructTypeIndexAndFieldIndex(unused, "struct.set"));
            return { };
        }
        default:
            WASM_PARSER_FAIL_IF(true, "invalid extended GC op ", extOp);
            break;
        }

        return { };
    }

    case GrowMemory:
    case CurrentMemory: {
        uint8_t reserved;
        WASM_PARSER_FAIL_IF(!parseUInt8(reserved), "can't parse reserved byte for grow_memory/current_memory");
        WASM_PARSER_FAIL_IF(reserved != 0, "reserved byte for grow_memory/current_memory must be zero");
        return { };
    }

#define CREATE_ATOMIC_CASE(name, ...) case ExtAtomicOpType::name:
    case ExtAtomic: {
        uint32_t extOp;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(extOp), "can't parse atomic extended opcode");
        ExtAtomicOpType op = static_cast<ExtAtomicOpType>(extOp);
        switch (op) {
        FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(CREATE_ATOMIC_CASE)
        FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(CREATE_ATOMIC_CASE)
        FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(CREATE_ATOMIC_CASE)
        case ExtAtomicOpType::MemoryAtomicWait64:
        case ExtAtomicOpType::MemoryAtomicWait32:
        case ExtAtomicOpType::MemoryAtomicNotify:
        case ExtAtomicOpType::I32AtomicRmw8CmpxchgU:
        case ExtAtomicOpType::I32AtomicRmw16CmpxchgU:
        case ExtAtomicOpType::I32AtomicRmwCmpxchg:
        case ExtAtomicOpType::I64AtomicRmw8CmpxchgU:
        case ExtAtomicOpType::I64AtomicRmw16CmpxchgU:
        case ExtAtomicOpType::I64AtomicRmw32CmpxchgU:
        case ExtAtomicOpType::I64AtomicRmwCmpxchg:
        {
            WASM_VALIDATOR_FAIL_IF(!m_info.memory, "atomic instruction without memory");
            uint32_t alignment;
            uint32_t unused;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get load alignment");
            WASM_PARSER_FAIL_IF(alignment != memoryLog2Alignment(op), "byte alignment ", 1ull << alignment, " does not match against atomic op's natural alignment ", 1ull << memoryLog2Alignment(op));
            WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get first immediate for atomic ", static_cast<unsigned>(op), " in unreachable context");
            break;
        }
        case ExtAtomicOpType::AtomicFence: {
            uint8_t flags;
            WASM_PARSER_FAIL_IF(!parseUInt8(flags), "can't get flags");
            WASM_PARSER_FAIL_IF(flags != 0x0, "flags should be 0x0 but got ", flags);
            break;
        }
        default:
            WASM_PARSER_FAIL_IF(true, "invalid extended atomic op ", extOp);
            break;
        }

        return { };
    }
#undef CREATE_ATOMIC_CASE

#if ENABLE(B3_JIT)
    case ExtSIMD: {
        WASM_PARSER_FAIL_IF(!Options::useWebAssemblySIMD(), "wasm-simd is not enabled");
        m_context.notifyFunctionUsesSIMD();
        uint32_t extOp;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(extOp), "can't parse wasm extended opcode");

        constexpr bool isReachable = false;

        ExtSIMDOpType op = static_cast<ExtSIMDOpType>(extOp);
        switch (op) {
        #define CREATE_SIMD_CASE(name, _, laneOp, lane, signMode) case ExtSIMDOpType::name: return simd<isReachable>(SIMDLaneOperation::laneOp, lane, signMode);
        FOR_EACH_WASM_EXT_SIMD_GENERAL_OP(CREATE_SIMD_CASE)
        #undef CREATE_SIMD_CASE
        #define CREATE_SIMD_CASE(name, _, laneOp, lane, signMode, relArg) case ExtSIMDOpType::name: return simd<isReachable>(SIMDLaneOperation::laneOp, lane, signMode, relArg);
        FOR_EACH_WASM_EXT_SIMD_REL_OP(CREATE_SIMD_CASE)
        #undef CREATE_SIMD_CASE
        default:
            WASM_PARSER_FAIL_IF(true, "invalid extended simd op ", extOp);
            break;
        }
        return { };
    }
#else
    case ExtSIMD:
        WASM_PARSER_FAIL_IF(true, "wasm-simd is not supported");
        return { };
#endif

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
