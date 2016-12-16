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
class FunctionParser : public Parser<void> {
public:
    typedef typename Context::ExpressionType ExpressionType;
    typedef typename Context::ControlType ControlType;
    typedef typename Context::ExpressionList ExpressionList;

    FunctionParser(Context&, const uint8_t* functionStart, size_t functionLength, const Signature*, const ImmutableFunctionIndexSpace&, const ModuleInformation&);

    Result WARN_UNUSED_RETURN parse();

    struct ControlEntry {
        ExpressionList enclosedExpressionStack;
        ControlType controlData;
    };

private:
    static const bool verbose = false;

    PartialResult WARN_UNUSED_RETURN parseBody();
    PartialResult WARN_UNUSED_RETURN parseExpression(OpType);
    PartialResult WARN_UNUSED_RETURN parseUnreachableExpression(OpType);
    PartialResult WARN_UNUSED_RETURN addReturn();
    PartialResult WARN_UNUSED_RETURN unifyControl(Vector<ExpressionType>&, unsigned level);

#define WASM_TRY_POP_EXPRESSION_STACK_INTO(result, what) do {                               \
        WASM_PARSER_FAIL_IF(m_expressionStack.isEmpty(), "can't pop empty stack in " what); \
        result = m_expressionStack.takeLast();                                              \
    } while (0)

    template<OpType>
    PartialResult WARN_UNUSED_RETURN unaryCase();

    template<OpType>
    PartialResult WARN_UNUSED_RETURN binaryCase();

#define WASM_TRY_ADD_TO_CONTEXT(add_expression) WASM_FAIL_IF_HELPER_FAILS(m_context.add_expression)

    // FIXME add a macro as above for WASM_TRY_APPEND_TO_CONTROL_STACK https://bugs.webkit.org/show_bug.cgi?id=165862

    Context& m_context;
    ExpressionList m_expressionStack;
    Vector<ControlEntry> m_controlStack;
    const Signature* m_signature;
    const ImmutableFunctionIndexSpace& m_functionIndexSpace;
    const ModuleInformation& m_info;
    unsigned m_unreachableBlocks { 0 };
};

template<typename Context>
FunctionParser<Context>::FunctionParser(Context& context, const uint8_t* functionStart, size_t functionLength, const Signature* signature, const ImmutableFunctionIndexSpace& functionIndexSpace, const ModuleInformation& info)
    : Parser(functionStart, functionLength)
    , m_context(context)
    , m_signature(signature)
    , m_functionIndexSpace(functionIndexSpace)
    , m_info(info)
{
    if (verbose)
        dataLogLn("Parsing function starting at: ", (uintptr_t)functionStart, " of length: ", functionLength);
}

template<typename Context>
auto FunctionParser<Context>::parse() -> Result
{
    uint32_t localCount;

    WASM_PARSER_FAIL_IF(!m_context.addArguments(m_signature->arguments), "can't add ", m_signature->arguments.size(), " arguments to Function");
    WASM_PARSER_FAIL_IF(!parseVarUInt32(localCount), "can't get local count");
    WASM_PARSER_FAIL_IF(localCount == std::numeric_limits<uint32_t>::max(), "Function section's local count is too big ", localCount);

    for (uint32_t i = 0; i < localCount; ++i) {
        uint32_t numberOfLocals;
        Type typeOfLocal;

        WASM_PARSER_FAIL_IF(!parseVarUInt32(numberOfLocals), "can't get Function's number of locals in group ", i);
        WASM_PARSER_FAIL_IF(numberOfLocals == std::numeric_limits<uint32_t>::max(), "Function section's ", i, "th local group count is too big ", numberOfLocals);
        WASM_PARSER_FAIL_IF(!parseValueType(typeOfLocal), "can't get Function local's type in group ", i);
        WASM_PARSER_FAIL_IF(!m_context.addLocal(typeOfLocal, numberOfLocals), "can't add ", numberOfLocals, " Function locals from group ", i);
    }

    WASM_FAIL_IF_HELPER_FAILS(parseBody());

    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseBody() -> PartialResult
{
    while (true) {
        uint8_t op;
        WASM_PARSER_FAIL_IF(!parseUInt8(op), "can't decode opcode");
        WASM_PARSER_FAIL_IF(!isValidOpType(op), "invalid opcode ", op);

        if (verbose) {
            dataLogLn("processing op (", m_unreachableBlocks, "): ",  RawPointer(reinterpret_cast<void*>(op)), ", ", makeString(static_cast<OpType>(op)), " at offset: ", RawPointer(reinterpret_cast<void*>(m_offset)));
            m_context.dump(m_controlStack, m_expressionStack);
        }

        if (op == OpType::End && !m_controlStack.size()) {
            if (m_unreachableBlocks)
                return { };
            return addReturn();
        }

        if (m_unreachableBlocks)
            WASM_FAIL_IF_HELPER_FAILS(parseUnreachableExpression(static_cast<OpType>(op)));
        else
            WASM_FAIL_IF_HELPER_FAILS(parseExpression(static_cast<OpType>(op)));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Context>
auto FunctionParser<Context>::addReturn() -> PartialResult
{
    ExpressionList returnValues;
    if (m_signature->returnType != Void) {
        ExpressionType returnValue;
        WASM_TRY_POP_EXPRESSION_STACK_INTO(returnValue, "return");
        returnValues.append(returnValue);
    }

    m_unreachableBlocks = 1;
    WASM_TRY_ADD_TO_CONTEXT(addReturn(returnValues));
    return { };
}

template<typename Context>
template<OpType op>
auto FunctionParser<Context>::binaryCase() -> PartialResult
{
    ExpressionType right;
    ExpressionType left;
    ExpressionType result;

    WASM_TRY_POP_EXPRESSION_STACK_INTO(right, "binary right");
    WASM_TRY_POP_EXPRESSION_STACK_INTO(left, "binary left");
    WASM_TRY_ADD_TO_CONTEXT(template addOp<op>(left, right, result));

    m_expressionStack.append(result);
    return { };
}

template<typename Context>
template<OpType op>
auto FunctionParser<Context>::unaryCase() -> PartialResult
{
    ExpressionType value;
    ExpressionType result;

    WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "unary");
    WASM_TRY_ADD_TO_CONTEXT(template addOp<op>(value, result));

    m_expressionStack.append(result);
    return { };
}

template<typename Context>
auto FunctionParser<Context>::parseExpression(OpType op) -> PartialResult
{
    switch (op) {
#define CREATE_CASE(name, id, b3op, inc) case OpType::name: return binaryCase<OpType::name>();
    FOR_EACH_WASM_SIMPLE_BINARY_OP(CREATE_CASE)
#undef CREATE_CASE

    case OpType::F32ConvertUI64: return unaryCase<OpType::F32ConvertUI64>();
    case OpType::F64ConvertUI64: return unaryCase<OpType::F64ConvertUI64>();
    case OpType::F32Nearest: return unaryCase<OpType::F32Nearest>();
    case OpType::F64Nearest: return unaryCase<OpType::F64Nearest>();
    case OpType::F32Trunc: return unaryCase<OpType::F32Trunc>();
    case OpType::F64Trunc: return unaryCase<OpType::F64Trunc>();
    case OpType::I32Ctz: return unaryCase<OpType::I32Ctz>();
    case OpType::I64Ctz: return unaryCase<OpType::I64Ctz>();
    case OpType::I32Popcnt: return unaryCase<OpType::I32Popcnt>();
    case OpType::I64Popcnt: return unaryCase<OpType::I64Popcnt>();
    case OpType::I32TruncSF32: return unaryCase<OpType::I32TruncSF32>();
    case OpType::I32TruncUF32: return unaryCase<OpType::I32TruncUF32>();
    case OpType::I32TruncSF64: return unaryCase<OpType::I32TruncSF64>();
    case OpType::I32TruncUF64: return unaryCase<OpType::I32TruncUF64>();
    case OpType::I64TruncSF32: return unaryCase<OpType::I64TruncSF32>();
    case OpType::I64TruncUF32: return unaryCase<OpType::I64TruncUF32>();
    case OpType::I64TruncSF64: return unaryCase<OpType::I64TruncSF64>();
    case OpType::I64TruncUF64: return unaryCase<OpType::I64TruncUF64>();
#define CREATE_CASE(name, id, b3op, inc) case OpType::name: return unaryCase<OpType::name>();
    FOR_EACH_WASM_SIMPLE_UNARY_OP(CREATE_CASE)
#undef CREATE_CASE

    case OpType::Select: {
        ExpressionType condition;
        ExpressionType zero;
        ExpressionType nonZero;

        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "select condition");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(zero, "select zero");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(nonZero, "select non-zero");

        ExpressionType result;
        WASM_TRY_ADD_TO_CONTEXT(addSelect(condition, nonZero, zero, result));

        m_expressionStack.append(result);
        return { };
    }

#define CREATE_CASE(name, id, b3op, inc) case OpType::name:
    FOR_EACH_WASM_MEMORY_LOAD_OP(CREATE_CASE) {
        uint32_t alignment;
        uint32_t offset;
        ExpressionType pointer;
        ExpressionType result;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get load alignment");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get load offset");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "load pointer");
        WASM_TRY_ADD_TO_CONTEXT(load(static_cast<LoadOpType>(op), pointer, result, offset));
        m_expressionStack.append(result);
        return { };
    }

    FOR_EACH_WASM_MEMORY_STORE_OP(CREATE_CASE) {
        uint32_t alignment;
        uint32_t offset;
        ExpressionType value;
        ExpressionType pointer;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(alignment), "can't get store alignment");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(offset), "can't get store offset");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "store value");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(pointer, "store pointer");
        WASM_TRY_ADD_TO_CONTEXT(store(static_cast<StoreOpType>(op), pointer, value, offset));
        return { };
    }
#undef CREATE_CASE

    case OpType::F32Const: {
        uint32_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt32(constant), "can't parse 32-bit floating-point constant");
        m_expressionStack.append(m_context.addConstant(F32, constant));
        return { };
    }

    case OpType::I32Const: {
        int32_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt32(constant), "can't parse 32-bit constant");
        m_expressionStack.append(m_context.addConstant(I32, constant));
        return { };
    }

    case OpType::F64Const: {
        uint64_t constant;
        WASM_PARSER_FAIL_IF(!parseUInt64(constant), "can't parse 64-bit floating-point constant");
        m_expressionStack.append(m_context.addConstant(F64, constant));
        return { };
    }

    case OpType::I64Const: {
        int64_t constant;
        WASM_PARSER_FAIL_IF(!parseVarInt64(constant), "can't parse 64-bit constant");
        m_expressionStack.append(m_context.addConstant(I64, constant));
        return { };
    }

    case OpType::GetLocal: {
        uint32_t index;
        ExpressionType result;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for get_local");
        WASM_PARSER_FAIL_IF(!m_context.getLocal(index, result), "can't get_local at index", index);
        m_expressionStack.append(result);
        return { };
    }

    case OpType::SetLocal: {
        uint32_t index;
        ExpressionType value;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for set_local");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "set_local");
        WASM_TRY_ADD_TO_CONTEXT(setLocal(index, value));
        return { };
    }

    case OpType::TeeLocal: {
        uint32_t index;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get index for tee_local");
        WASM_PARSER_FAIL_IF(m_expressionStack.isEmpty(), "can't tee_local on empty expression stack");
        WASM_TRY_ADD_TO_CONTEXT(setLocal(index, m_expressionStack.last()));
        return { };
    }

    case OpType::GetGlobal: {
        uint32_t index;
        ExpressionType result;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get get_global's index");
        WASM_TRY_ADD_TO_CONTEXT(getGlobal(index, result));
        m_expressionStack.append(result);
        return { };
    }

    case OpType::SetGlobal: {
        uint32_t index;
        ExpressionType value;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(index), "can't get set_global's index");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(value, "set_global value");
        WASM_TRY_ADD_TO_CONTEXT(setGlobal(index, value));
        return m_context.setGlobal(index, value);
    }

    case OpType::Call: {
        uint32_t functionIndex;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(functionIndex), "can't parse call's function index");
        WASM_PARSER_FAIL_IF(functionIndex >= m_functionIndexSpace.size, "call function index ", functionIndex, " exceeds function index space ", m_functionIndexSpace.size);

        const Signature* calleeSignature = m_functionIndexSpace.buffer.get()[functionIndex].signature;
        WASM_PARSER_FAIL_IF(calleeSignature->arguments.size() > m_expressionStack.size(), "call function index ", functionIndex, " has ", calleeSignature->arguments.size(), " arguments, but the expression stack currently holds ", m_expressionStack.size(), " values");

        size_t firstArgumentIndex = m_expressionStack.size() - calleeSignature->arguments.size();
        Vector<ExpressionType> args;
        WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(calleeSignature->arguments.size()), "can't allocate enough memory for call's ", calleeSignature->arguments.size(), " arguments");
        for (size_t i = firstArgumentIndex; i < m_expressionStack.size(); ++i)
            args.uncheckedAppend(m_expressionStack[i]);
        m_expressionStack.shrink(firstArgumentIndex);

        ExpressionType result = Context::emptyExpression;
        WASM_TRY_ADD_TO_CONTEXT(addCall(functionIndex, calleeSignature, args, result));

        if (result != Context::emptyExpression)
            m_expressionStack.append(result);

            return { };
    }

    case OpType::CallIndirect: {
        uint32_t signatureIndex;
        uint8_t reserved;
        WASM_PARSER_FAIL_IF(!m_info.tableInformation, "call_indirect is only valid when a table is defined or imported");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(signatureIndex), "can't get call_indirect's signature index");
        WASM_PARSER_FAIL_IF(!parseVarUInt1(reserved), "can't get call_indirect's reserved byte");
        WASM_PARSER_FAIL_IF(reserved, "call_indirect's 'reserved' varuint1 must be 0x0");
        WASM_PARSER_FAIL_IF(m_info.signatures.size() <= signatureIndex, "call_indirect's signature index ", signatureIndex, " exceeds known signatures ", m_info.signatures.size());

        const Signature* calleeSignature = &m_info.signatures[signatureIndex];
        size_t argumentCount = calleeSignature->arguments.size() + 1; // Add the callee's index.
        WASM_PARSER_FAIL_IF(argumentCount > m_expressionStack.size(), "call_indirect expects ", argumentCount, " arguments, but the expression stack currently holds ", m_expressionStack.size(), " values");

        Vector<ExpressionType> args;
        WASM_PARSER_FAIL_IF(!args.tryReserveCapacity(argumentCount), "can't allocate enough memory for ", argumentCount, " call_indirect arguments");
        size_t firstArgumentIndex = m_expressionStack.size() - argumentCount;
        for (size_t i = firstArgumentIndex; i < m_expressionStack.size(); ++i)
            args.uncheckedAppend(m_expressionStack[i]);
        m_expressionStack.shrink(firstArgumentIndex);

        ExpressionType result = Context::emptyExpression;
        WASM_TRY_ADD_TO_CONTEXT(addCallIndirect(calleeSignature, args, result));

        if (result != Context::emptyExpression)
            m_expressionStack.append(result);

        return { };
    }

    case OpType::Block: {
        Type inlineSignature;
        WASM_PARSER_FAIL_IF(!parseResultType(inlineSignature), "can't get block's inline signature");
        m_controlStack.append({ WTFMove(m_expressionStack), m_context.addBlock(inlineSignature) });
        m_expressionStack = ExpressionList();
        return { };
    }

    case OpType::Loop: {
        Type inlineSignature;
        WASM_PARSER_FAIL_IF(!parseResultType(inlineSignature), "can't get loop's inline signature");
        m_controlStack.append({ WTFMove(m_expressionStack), m_context.addLoop(inlineSignature) });
        m_expressionStack = ExpressionList();
        return { };
    }

    case OpType::If: {
        Type inlineSignature;
        ExpressionType condition;
        ControlType control;
        WASM_PARSER_FAIL_IF(!parseResultType(inlineSignature), "can't get if's inline signature");
        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "if condition");
        WASM_TRY_ADD_TO_CONTEXT(addIf(condition, inlineSignature, control));
        m_controlStack.append({ WTFMove(m_expressionStack), control });
        m_expressionStack = ExpressionList();
        return { };
    }

    case OpType::Else: {
        WASM_PARSER_FAIL_IF(m_controlStack.isEmpty(), "can't use else block at the top-level of a function");
        WASM_TRY_ADD_TO_CONTEXT(addElse(m_controlStack.last().controlData, m_expressionStack));
        m_expressionStack.shrink(0);
        return { };
    }

    case OpType::Br:
    case OpType::BrIf: {
        uint32_t target;
        ExpressionType condition = Context::emptyExpression;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(target), "can't get br / br_if's target");
        WASM_PARSER_FAIL_IF(target >= m_controlStack.size(), "br / br_if's target ", target, " exceeds control stack size ", m_controlStack.size());
        if (op == OpType::BrIf)
            WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "br / br_if condition");
        else
            m_unreachableBlocks = 1;

        ControlType& data = m_controlStack[m_controlStack.size() - 1 - target].controlData;

        WASM_TRY_ADD_TO_CONTEXT(addBranch(data, condition, m_expressionStack));
        return { };
    }

    case OpType::BrTable: {
        uint32_t numberOfTargets;
        ExpressionType condition;
        uint32_t defaultTarget;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(numberOfTargets), "can't get the number of targets for br_table");
        WASM_PARSER_FAIL_IF(numberOfTargets == std::numeric_limits<uint32_t>::max(), "br_table's number of targets is too big ", numberOfTargets);

        Vector<ControlType*> targets;
        WASM_PARSER_FAIL_IF(!targets.tryReserveCapacity(numberOfTargets), "can't allocate memory for ", numberOfTargets, " br_table targets");
        for (uint32_t i = 0; i < numberOfTargets; ++i) {
            uint32_t target;
            WASM_PARSER_FAIL_IF(!parseVarUInt32(target), "can't get ", i, "th target for br_table");
            WASM_PARSER_FAIL_IF(target >= m_controlStack.size(), "br_table's ", i, "th target ", target, " exceeds control stack size ", m_controlStack.size());
            targets.uncheckedAppend(&m_controlStack[m_controlStack.size() - 1 - target].controlData);
        }

        WASM_PARSER_FAIL_IF(!parseVarUInt32(defaultTarget), "can't get default target for br_table");
        WASM_PARSER_FAIL_IF(defaultTarget >= m_controlStack.size(), "br_table's default target ", defaultTarget, " exceeds control stack size ", m_controlStack.size());
        WASM_TRY_POP_EXPRESSION_STACK_INTO(condition, "br_table condition");
        WASM_TRY_ADD_TO_CONTEXT(addSwitch(condition, targets, m_controlStack[m_controlStack.size() - 1 - defaultTarget].controlData, m_expressionStack));

        m_unreachableBlocks = 1;
        return { };
    }

    case OpType::Return: {
        return addReturn();
    }

    case OpType::End: {
        ControlEntry data = m_controlStack.takeLast();
        // FIXME: This is a little weird in that it will modify the expressionStack for the result of the block.
        // That's a little too effectful for me but I don't have a better API right now.
        // see: https://bugs.webkit.org/show_bug.cgi?id=164353
        WASM_TRY_ADD_TO_CONTEXT(endBlock(data, m_expressionStack));
        m_expressionStack.swap(data.enclosedExpressionStack);
        return { };
    }

    case OpType::Unreachable: {
        WASM_TRY_ADD_TO_CONTEXT(addUnreachable());
        m_unreachableBlocks = 1;
        return { };
    }

    case OpType::Drop: {
        WASM_PARSER_FAIL_IF(!m_expressionStack.size(), "can't drop on empty stack");
        m_expressionStack.takeLast();
        return { };
    }

    case OpType::Nop: {
        return { };
    }

    case OpType::GrowMemory:
        return fail("not yet implemented: grow_memory"); // FIXME: Not yet implemented.

    case OpType::CurrentMemory:
        return fail("not yet implemented: current_memory"); // FIXME: Not yet implemented.

    }

    ASSERT_NOT_REACHED();
}

template<typename Context>
auto FunctionParser<Context>::parseUnreachableExpression(OpType op) -> PartialResult
{
    ASSERT(m_unreachableBlocks);
    switch (op) {
    case OpType::Else: {
        if (m_unreachableBlocks > 1)
            return { };

        ControlEntry& data = m_controlStack.last();
        m_unreachableBlocks = 0;
        WASM_TRY_ADD_TO_CONTEXT(addElseToUnreachable(data.controlData));
        m_expressionStack.shrink(0);
        return { };
    }

    case OpType::End: {
        if (m_unreachableBlocks == 1) {
            ControlEntry data = m_controlStack.takeLast();
            WASM_TRY_ADD_TO_CONTEXT(addEndToUnreachable(data));
            m_expressionStack.swap(data.enclosedExpressionStack);
        }
        m_unreachableBlocks--;
        return { };
    }

    case OpType::Loop:
    case OpType::If:
    case OpType::Block: {
        m_unreachableBlocks++;
        return { };
    }

    // two immediate cases
    case OpType::Br:
    case OpType::BrIf: {
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get br / br_if in unreachable context");
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get br / br_if in unreachable context");
        return { };
    }

    // one immediate cases
    case OpType::F32Const:
    case OpType::I32Const:
    case OpType::F64Const:
    case OpType::I64Const:
    case OpType::SetLocal:
    case OpType::GetLocal: {
        uint32_t unused;
        WASM_PARSER_FAIL_IF(!parseVarUInt32(unused), "can't get const / local in unreachable context");
        return { };
    }

    default:
        break;
    }
    return { };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
