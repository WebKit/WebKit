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

        static bool isIf(const ControlData& control) { return control.blockType() == BlockType::If; }
        static bool isTopLevel(const ControlData& control) { return control.blockType() == BlockType::TopLevel; }

        BlockType blockType() const { return m_blockType; }
        BlockSignature signature() const { return m_signature; }

        unsigned branchTargetArity() const { return blockType() == BlockType::Loop ? signature()->argumentCount() : signature()->returnCount(); }
        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            return blockType() == BlockType::Loop ? signature()->argument(i) : signature()->returnType(i);
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

    private:
        BlockType m_blockType;
        BlockSignature m_signature;
    };

    enum ExpressionType { EmptyExpression };

    using ErrorType = String;
    using UnexpectedResult = Unexpected<ErrorType>;
    using Result = Expected<void, ErrorType>;
    using ControlType = ControlData;

    using ControlEntry = FunctionParser<Validate>::ControlEntry;
    using ControlStack = FunctionParser<Validate>::ControlStack;
    using ResultList = FunctionParser<Validate>::ResultList;
    using Stack = FunctionParser<Validate>::Stack;

    static ExpressionType emptyExpression() { return EmptyExpression; };

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
    ExpressionType addConstant(Type, uint64_t) { return EmptyExpression; }

    // References
    Result WARN_UNUSED_RETURN addRefIsNull(ExpressionType value, ExpressionType& result);
    Result WARN_UNUSED_RETURN addRefFunc(uint32_t index, ExpressionType& result);

    // Tables
    Result WARN_UNUSED_RETURN addTableGet(unsigned, ExpressionType index, ExpressionType& result);
    Result WARN_UNUSED_RETURN addTableSet(unsigned, ExpressionType index, ExpressionType value);
    Result WARN_UNUSED_RETURN addTableSize(unsigned, ExpressionType& result);
    Result WARN_UNUSED_RETURN addTableGrow(unsigned, ExpressionType fill, ExpressionType delta, ExpressionType& result);
    Result WARN_UNUSED_RETURN addTableFill(unsigned, ExpressionType offset, ExpressionType fill, ExpressionType count);
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
    Result WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&) { return { }; }

    // Calls
    Result WARN_UNUSED_RETURN addCall(unsigned calleeIndex, const Signature&, const Vector<ExpressionType>& args, ResultList&);
    Result WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const Signature&, const Vector<ExpressionType>& args, ResultList&);

    Validate()
    {
    }

    void dump(const ControlStack&, const Stack*);
    void setParser(FunctionParser<Validate>*) { }
    void didFinishParsingLocals() { }
    void didPopValueFromStack() { }

private:
    Result WARN_UNUSED_RETURN unify(const Stack&, const ControlData&);
};

auto Validate::addArguments(const Signature&) -> Result
{
    return { };
}

auto Validate::addTableGet(unsigned, ExpressionType, ExpressionType&) -> Result
{
    return { };
}

auto Validate::addTableSet(unsigned, ExpressionType, ExpressionType) -> Result
{
    return { };
}

auto Validate::addTableSize(unsigned, ExpressionType&) -> Result
{
    return { };
}

auto Validate::addTableGrow(unsigned, ExpressionType, ExpressionType, ExpressionType&) -> Result
{
    return { };
}

auto Validate::addTableFill(unsigned, ExpressionType, ExpressionType, ExpressionType) -> Result
{
    return { };
}

auto Validate::addRefIsNull(ExpressionType, ExpressionType&) -> Result
{
    return { };
}

auto Validate::addRefFunc(uint32_t, ExpressionType&) -> Result
{
    return { };
}

auto Validate::addLocal(Type, uint32_t) -> Result
{
    return { };
}

auto Validate::getLocal(uint32_t, ExpressionType&) -> Result
{
    return { };
}

auto Validate::setLocal(uint32_t, ExpressionType) -> Result
{
    return { };
}

auto Validate::getGlobal(uint32_t, ExpressionType&) -> Result
{
    return { };
}

auto Validate::setGlobal(uint32_t, ExpressionType) -> Result
{
    return { };
}

Validate::ControlType Validate::addTopLevel(BlockSignature signature)
{
    return ControlData(BlockType::TopLevel, signature);
}

auto Validate::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> Result
{
    splitStack(signature, enclosingStack, newStack);
    newBlock = ControlData(BlockType::Block, signature);
    return { };
}

auto Validate::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& loop, Stack& newStack, uint32_t) -> Result
{
    splitStack(signature, enclosingStack, newStack);
    loop = ControlData(BlockType::Loop, signature);
    return { };
}

auto Validate::addSelect(ExpressionType, ExpressionType, ExpressionType, ExpressionType&) -> Result
{
    return { };
}

auto Validate::addIf(ExpressionType, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> Result
{
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(BlockType::If, signature);
    return { };
}

auto Validate::addElse(ControlType& current, const Stack&) -> Result
{
    return addElseToUnreachable(current);
}

auto Validate::addElseToUnreachable(ControlType& current) -> Result
{
    current = ControlData(BlockType::Block, current.signature());
    return { };
}

auto Validate::addReturn(ControlType&, const Stack&) -> Result
{
    return { };
}

auto Validate::addBranch(ControlType&, ExpressionType, const Stack&) -> Result
{
    return { };
}

auto Validate::addSwitch(ExpressionType, const Vector<ControlData*>&, ControlData&, const Stack&) -> Result
{
    return { };
}

auto Validate::addGrowMemory(ExpressionType, ExpressionType&) -> Result
{
    return { };
}

auto Validate::addCurrentMemory(ExpressionType&) -> Result
{
    return { };
}

auto Validate::endBlock(ControlEntry& entry, Stack&) -> Result
{
    return addEndToUnreachable(entry);
}

auto Validate::addEndToUnreachable(ControlEntry& entry) -> Result
{
    auto block = entry.controlData;
    for (unsigned i = 0; i < block.signature()->returnCount(); ++i)
        entry.enclosedExpressionStack.constructAndAppend(block.signature()->returnType(i), EmptyExpression);
    return { };
}

auto Validate::addCall(unsigned, const Signature& signature, const Vector<ExpressionType>&, ResultList& results) -> Result
{
    for (unsigned i = 0; i < signature.returnCount(); ++i)
        results.append(EmptyExpression);
    return { };
}

auto Validate::addCallIndirect(unsigned, const Signature& signature, const Vector<ExpressionType>&, ResultList& results) -> Result
{
    for (unsigned i = 0; i < signature.returnCount(); ++i)
        results.append(EmptyExpression);
    return { };
}

auto Validate::load(LoadOpType, ExpressionType, ExpressionType&, uint32_t) -> Result
{
    return { };
}

auto Validate::store(StoreOpType, ExpressionType, ExpressionType, uint32_t) -> Result
{
    return { };
}

template<OpType>
auto Validate::addOp(ExpressionType, ExpressionType&) -> Result
{
    return { };
}

template<OpType>
auto Validate::addOp(ExpressionType, ExpressionType, ExpressionType&) -> Result
{
    return { };
}

static void dumpExpressionStack(const CommaPrinter& comma, const Validate::Stack& expressionStack)
{
    dataLog(comma, " ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, makeString(expression.type()));
}

void Validate::dump(const ControlStack& controlStack, const Stack* expressionStack)
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
    Validate context;
    FunctionParser<Validate> validator(context, function.data.data(), function.data.size(), signature, module);
    WASM_FAIL_IF_HELPER_FAILS(validator.parse());
    return { };
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
