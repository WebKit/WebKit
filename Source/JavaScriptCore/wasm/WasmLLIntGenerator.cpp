/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WasmLLIntGenerator.h"

#if ENABLE(WEBASSEMBLY)

#include "BytecodeGeneratorBaseInlines.h"
#include "BytecodeStructs.h"
#include "InstructionStream.h"
#include "Label.h"
#include "RegisterID.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmFunctionCodeBlock.h"
#include "WasmFunctionParser.h"
#include "WasmGeneratorTraits.h"
#include "WasmThunks.h"
#include <wtf/RefPtr.h>
#include <wtf/StdUnorderedMap.h>
#include <wtf/Variant.h>

namespace JSC { namespace Wasm {

class LLIntGenerator : public BytecodeGeneratorBase<GeneratorTraits> {
public:
    using ExpressionType = RefPtr<RegisterID>;
    using ExpressionList = Vector<ExpressionType, 1>;
    using Stack = ExpressionList;

    struct ControlLoop  {
        Ref<Label> m_body;
    };

    struct ControlTopLevel  {
    };

    struct ControlBlock  {
    };

    struct ControlIf  {
        Ref<Label> m_alternate;
    };

    struct ControlType : public Variant<ControlLoop, ControlTopLevel, ControlBlock, ControlIf> {
        using Base = Variant<ControlLoop, ControlTopLevel, ControlBlock, ControlIf>;

        ControlType()
            : Base(ControlBlock { })
        {
        }

        static ControlType loop(BlockSignature signature, const ExpressionList& results, Ref<Label> body, RefPtr<Label> continuation)
        {
            return ControlType(signature, results, WTFMove(continuation), ControlLoop { WTFMove(body) });
        }

        static ControlType topLevel(BlockSignature signature, const ExpressionList& results, RefPtr<Label> continuation)
        {
            return ControlType(signature, results, WTFMove(continuation), ControlTopLevel { });
        }

        static ControlType block(BlockSignature signature, const ExpressionList& results, RefPtr<Label> continuation)
        {
            return ControlType(signature, results, WTFMove(continuation), ControlBlock { });
        }

        static ControlType if_(BlockSignature signature, const ExpressionList& results, Ref<Label> alternate, RefPtr<Label> continuation)
        {
            return ControlType(signature, results, WTFMove(continuation), ControlIf { WTFMove(alternate) });
        }

        RefPtr<Label> targetLabelForBranch() const
        {
            if (WTF::holds_alternative<ControlLoop>(*this))
                return WTF::get<ControlLoop>(*this).m_body.ptr();
            return m_continuation;
        }

        BlockSignature m_signature;
        ExpressionList m_results;
        RefPtr<Label> m_continuation;

    private:
        template<typename T>
        ControlType(BlockSignature signature, const ExpressionList& results, RefPtr<Label> continuation, T t)
            : Base(WTFMove(t))
            , m_signature(signature)
            , m_results(results)
            , m_continuation(WTFMove(continuation))
        {
        }
    };

    using ErrorType = String;
    using PartialResult = Expected<void, ErrorType>;
    using ResultList = ExpressionList;
    using UnexpectedResult = Unexpected<ErrorType>;

    using ControlEntry = FunctionParser<LLIntGenerator>::ControlEntry;

    LLIntGenerator(const ModuleInformation&, unsigned functionIndex, ThrowWasmException, const Signature&);

    std::unique_ptr<FunctionCodeBlock> finalize();

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module failed compiling: "_s, makeString(args)...));
    }

    template<typename ExpressionListA, typename ExpressionListB>
    void unifyValuesWithBlock(const ExpressionListA& destinations, const ExpressionListB& values)
    {
        ASSERT(destinations.size() <= values.size());
        for (size_t i = 0; i < destinations.size(); ++i)
            WasmMov::emit(this, destinations[destinations.size() - i - 1], values[values.size() - i - 1]);
    }


    static ExpressionType emptyExpression() { return nullptr; };
    Stack createStack() { return Stack(); }
    bool isControlTypeIf(const ControlType& control) { return WTF::holds_alternative<ControlIf>(control); }

    PartialResult WARN_UNUSED_RETURN addArguments(const Signature&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

    // References
    PartialResult WARN_UNUSED_RETURN addRefIsNull(ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addRefFunc(uint32_t index, ExpressionType& result);

    // Tables
    PartialResult WARN_UNUSED_RETURN addTableGet(unsigned, ExpressionType index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableSet(unsigned, ExpressionType index, ExpressionType value);
    PartialResult WARN_UNUSED_RETURN addTableSize(unsigned, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableGrow(unsigned, ExpressionType fill, ExpressionType delta, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableFill(unsigned, ExpressionType offset, ExpressionType fill, ExpressionType count);

    // Locals
    PartialResult WARN_UNUSED_RETURN getLocal(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN setLocal(uint32_t index, ExpressionType value);

    // Globals
    PartialResult WARN_UNUSED_RETURN getGlobal(uint32_t index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN setGlobal(uint32_t index, ExpressionType value);

    // Memory
    PartialResult WARN_UNUSED_RETURN load(LoadOpType, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN store(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN addGrowMemory(ExpressionType delta, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addCurrentMemory(ExpressionType& result);

    // Basic operators
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType arg, ExpressionType& result);
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType left, ExpressionType right, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result);

    // Control flow
    ControlType WARN_UNUSED_RETURN addTopLevel(BlockSignature);
    PartialResult WARN_UNUSED_RETURN addBlock(BlockSignature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addLoop(BlockSignature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex);
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType condition, BlockSignature, Stack& enclosingStack, ControlType& result, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addElse(ControlType&, const ExpressionList&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlType&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlType&, const ExpressionList& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlType&, ExpressionType condition, const ExpressionList& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlType*>& targets, ControlType& defaultTargets, const ExpressionList& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, ExpressionList& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, const Stack& expressionStack = { });

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const Signature&, Vector<ExpressionType>& args, ExpressionList& results);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const Signature&, Vector<ExpressionType>& args, ExpressionList& results);
    PartialResult WARN_UNUSED_RETURN addUnreachable();

    void didFinishParsingLocals();

    void setParser(FunctionParser<LLIntGenerator>* parser) { m_parser = parser; };

    void dump(const Vector<ControlEntry>&, const ExpressionList*) { }

private:
    friend GenericLabel<Wasm::GeneratorTraits>;

    struct LLIntCallInformation {
        unsigned stackOffset;
        unsigned numberOfStackArguments;
        ExpressionList arguments;
        ExpressionList results;
    };

    LLIntCallInformation callInformationFor(const Signature&, CallRole = CallRole::Caller);

    VirtualRegister virtualRegisterForLocal(uint32_t index)
    {
        if (index < m_codeBlock->m_numArguments)
            return m_normalizedArguments[index];

        const auto& callingConvention = wasmCallingConvention();
        const uint32_t gprCount = callingConvention.gprArgs.size();
        const uint32_t fprCount = callingConvention.fprArgs.size();
        return ::JSC::virtualRegisterForLocal(index - m_codeBlock->m_numArguments + gprCount + fprCount + numberOfLLIntCalleeSaveRegisters);
    }

    ExpressionList tmpsForSignature(BlockSignature signature)
    {
        ExpressionList result(signature->returnCount());
        for (unsigned i = 0; i < signature->returnCount(); ++i)
            result[i] = newTemporary();
        return result;
    }

    ExpressionType jsNullConstant()
    {
        if (!m_jsNullConstant)
            m_jsNullConstant = addConstant(Type::Anyref, JSValue::encode(jsNull()));
        return m_jsNullConstant;
    }

    struct SwitchEntry {
        InstructionStream::Offset offset;
        InstructionStream::Offset* jumpTarget;
    };

    FunctionParser<LLIntGenerator>* m_parser { nullptr };
    const ModuleInformation& m_info;
    const unsigned m_functionIndex { UINT_MAX };
    Vector<VirtualRegister> m_normalizedArguments;
    HashMap<Label*, Vector<SwitchEntry>> m_switches;
    ExpressionType m_jsNullConstant;
    ExpressionList m_unitializedLocals;
    StdUnorderedMap<uint64_t, VirtualRegister> m_constantMap;
};

Expected<std::unique_ptr<FunctionCodeBlock>, String> parseAndCompileBytecode(const uint8_t* functionStart, size_t functionLength, const Signature& signature, const ModuleInformation& info, uint32_t functionIndex, ThrowWasmException throwWasmException)
{
    LLIntGenerator llintGenerator(info, functionIndex, throwWasmException, signature);
    FunctionParser<LLIntGenerator> parser(llintGenerator, functionStart, functionLength, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    return llintGenerator.finalize();
}

LLIntGenerator::LLIntGenerator(const ModuleInformation& info, unsigned functionIndex, ThrowWasmException throwWasmException, const Signature&)
    : BytecodeGeneratorBase(makeUnique<FunctionCodeBlock>(functionIndex), numberOfLLIntCalleeSaveRegisters)
    , m_info(info)
    , m_functionIndex(functionIndex)
{
    if (throwWasmException)
        Thunks::singleton().setThrowWasmException(throwWasmException);

    WasmEnter::emit(this);
}

std::unique_ptr<FunctionCodeBlock> LLIntGenerator::finalize()
{
    RELEASE_ASSERT(m_codeBlock);
    m_codeBlock->setInstructions(m_writer.finalize());
    return WTFMove(m_codeBlock);
}

// Generated from wasm.json
#include "WasmLLIntGeneratorInlines.h"

auto LLIntGenerator::callInformationFor(const Signature& signature, CallRole role) -> LLIntCallInformation
{
    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.gprArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();

    uint32_t stackCount = 0;
    uint32_t gprIndex = 0;
    uint32_t fprIndex = 0;

    Vector<RefPtr<RegisterID>, 16> registers;

    ExpressionList arguments(signature.argumentCount());
    ExpressionList results(signature.returnCount());

    auto allocateStackRegister = [&](Type type) {
        ASSERT(role == CallRole::Caller);
        switch (type) {
        case Type::I32:
        case Type::I64:
        case Type::Anyref:
        case Type::Funcref:
            if (gprIndex < gprCount)
                ++gprIndex;
            else {
                registers.append(newTemporary());
                ++stackCount;
            }
            break;
        case Type::F32:
        case Type::F64:
            if (fprIndex < fprCount)
                ++fprIndex;
            else {
                registers.append(newTemporary());
                ++stackCount;
            }
            break;
        case Void:
        case Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };


    if (role == CallRole::Callee) {
        // Reuse the slots we allocated to spill the registers in addArguments
        for (uint32_t i = gprCount + fprCount; i--;)
            registers.append(new RegisterID(::JSC::virtualRegisterForLocal(numberOfLLIntCalleeSaveRegisters + i)));
    } else {
        for (uint32_t i = 0; i < gprCount; i++)
            registers.append(newTemporary());
        for (uint32_t i = 0; i < fprCount; i++)
            registers.append(newTemporary());

        for (uint32_t i = 0; i < signature.argumentCount(); i++)
            allocateStackRegister(signature.argument(i));
        gprIndex = 0;
        fprIndex = 0;
        for (uint32_t i = 0; i < signature.returnCount(); i++)
            allocateStackRegister(signature.returnType(i));
    }

    unsigned stackOffset;
    if (role == CallRole::Callee)
        stackOffset = static_cast<unsigned>(-registers.last()->index());
    else {
        // Align the stack
        auto computeStackOffset = [&] {
            return static_cast<unsigned>(-registers.last()->index()) + CallFrame::headerSizeInRegisters;
        };
        while (computeStackOffset() % stackAlignmentRegisters())
            registers.append(newTemporary());
        stackOffset = computeStackOffset();
    }

    ASSERT(role == CallRole::Caller || !stackCount);
    const uint32_t maxGPRIndex = stackCount + gprCount;
    const uint32_t maxFPRIndex = maxGPRIndex + fprCount;
    uint32_t stackIndex = 0;
    auto appendForType = [&](Type type, unsigned index, auto& vector) {
        switch (type) {
        case Type::I32:
        case Type::I64:
        case Type::Anyref:
        case Type::Funcref:
            if (gprIndex < maxGPRIndex)
                vector[index] = registers[registers.size() - gprIndex++ - 1];
            else {
                if (role == CallRole::Caller)
                    vector[index] = registers[registers.size() - stackIndex++ - 1];
                else
                    vector[index] = new RegisterID(virtualRegisterForArgument(stackIndex++));
            }
            break;
        case Type::F32:
        case Type::F64:
            if (fprIndex < maxFPRIndex)
                vector[index] = registers[registers.size() - fprIndex++ - 1];
            else {
                if (role == CallRole::Caller)
                    vector[index] = registers[registers.size() - stackIndex++ - 1];
                else
                    vector[index] = new RegisterID(virtualRegisterForArgument(stackIndex++));
            }
            break;
        case Void:
        case Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    gprIndex = stackCount;
    fprIndex = maxGPRIndex;
    for (uint32_t i = 0; i < signature.argumentCount(); i++)
        appendForType(signature.argument(i), i, arguments);
    gprIndex = stackCount;
    fprIndex = maxGPRIndex;
    for (uint32_t i = 0; i < signature.returnCount(); i++)
        appendForType(signature.returnType(i), i, results);

    if (role == CallRole::Caller) {
        // Reserve space for call frame.
        Vector<RefPtr<RegisterID>, CallFrame::headerSizeInRegisters + 2, UnsafeVectorOverflow> callFrame;
        for (int i = 0; i < CallFrame::headerSizeInRegisters; ++i)
            callFrame.append(newTemporary());
    }

    return LLIntCallInformation { stackOffset, stackCount, WTFMove(arguments), WTFMove(results) };
}

auto LLIntGenerator::addArguments(const Signature& signature) -> PartialResult
{
    m_codeBlock->m_numArguments = signature.argumentCount();
    m_normalizedArguments.resize(m_codeBlock->m_numArguments);

    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.gprArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();
    const uint32_t maxGPRIndex = gprCount;
    const uint32_t maxFPRIndex = gprCount + fprCount;
    uint32_t gprIndex = 0;
    uint32_t fprIndex = maxGPRIndex;
    uint32_t stackIndex = 0;

    Vector<RefPtr<RegisterID>> registerArguments(gprCount + fprCount);
    for (uint32_t i = 0; i < gprCount + fprCount; i++)
        registerArguments[i] = addVar();

    const auto addArgument = [&](uint32_t index, uint32_t& count, uint32_t max) {
        if (count < max)
            m_normalizedArguments[index] = registerArguments[count++];
        else
            m_normalizedArguments[index] = virtualRegisterForArgument(stackIndex++);
    };

    for (uint32_t i = 0; i < signature.argumentCount(); i++) {
        switch (signature.argument(i)) {
        case Type::I32:
        case Type::I64:
        case Type::Anyref:
        case Type::Funcref:
            addArgument(i, gprIndex, maxGPRIndex);
            break;
        case Type::F32:
        case Type::F64:
            addArgument(i, fprIndex, maxFPRIndex);
            break;
        case Void:
        case Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    return { };
}

auto LLIntGenerator::addLocal(Type type, uint32_t count) -> PartialResult
{
    while (count--) {
        auto local = addVar();
        switch (type) {
        case Type::Anyref:
        case Type::Funcref:
            m_unitializedLocals.append(local);
            break;
        default:
            break;
        }
    }
    return { };
}

void LLIntGenerator::didFinishParsingLocals()
{
    auto null = jsNullConstant();
    for (auto local : m_unitializedLocals)
        WasmMov::emit(this, local, null);
    m_unitializedLocals.clear();
}

auto LLIntGenerator::addConstant(Type type, uint64_t value) -> ExpressionType
{
    VirtualRegister source(FirstConstantRegisterIndex + m_codeBlock->m_constants.size());
    auto result = m_constantMap.emplace(value, source);
    if (result.second) {
        m_codeBlock->m_constants.append(value);
        if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
            m_codeBlock->m_constantTypes.append(type);
    } else
        source = result.first->second;
    auto target = newTemporary();
    WasmMov::emit(this, target, source);
    return target;
}

auto LLIntGenerator::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    // FIXME: Remove unnecessary moves
    // https://bugs.webkit.org/show_bug.cgi?id=203657
    result = newTemporary();
    WasmMov::emit(this, result, virtualRegisterForLocal(index));
    return { };
}

auto LLIntGenerator::setLocal(uint32_t index, ExpressionType value) -> PartialResult
{
    WasmMov::emit(this, virtualRegisterForLocal(index), value);
    return { };
}

auto LLIntGenerator::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    result = newTemporary();
    WasmGetGlobal::emit(this, result, index);
    return { };
}

auto LLIntGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    Type type = m_info.globals[index].type;
    if (isSubtype(type, Anyref))
        WasmSetGlobalRef::emit(this, index, value);
    else
        WasmSetGlobal::emit(this, index, value);
    return { };
}

auto LLIntGenerator::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex) -> PartialResult
{
    Ref<Label> body = newEmittedLabel();
    Ref<Label> continuation = newLabel();

    newStack = splitStack(signature, enclosingStack);
    block = ControlType::loop(signature, newStack, WTFMove(body), WTFMove(continuation));

    Vector<VirtualRegister> osrEntryData;
    for (uint32_t i = 0; i < m_codeBlock->m_numArguments; i++)
        osrEntryData.append(m_normalizedArguments[i]);

    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.gprArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();
    for (int32_t i = gprCount + fprCount + numberOfLLIntCalleeSaveRegisters; i < m_codeBlock->m_numVars; i++)
        osrEntryData.append(::JSC::virtualRegisterForLocal(i));
    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        ExpressionList& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        for (auto& expression : expressionStack)
            osrEntryData.append(expression->virtualRegister());
    }

    WasmLoopHint::emit(this);

    m_codeBlock->tierUpCounter().addOSREntryDataForLoop(m_lastInstruction.offset(), { loopIndex, WTFMove(osrEntryData) });

    return { };
}

auto LLIntGenerator::addTopLevel(BlockSignature signature) -> ControlType
{
    return ControlType::topLevel(signature, tmpsForSignature(signature), newLabel());
}

auto LLIntGenerator::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> PartialResult
{
    newStack = splitStack(signature, enclosingStack);
    newBlock = ControlType::block(signature, tmpsForSignature(signature), newLabel());
    return { };
}

auto LLIntGenerator::addIf(ExpressionType condition, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    Ref<Label> alternate = newLabel();
    Ref<Label> continuation = newLabel();

    WasmJfalse::emit(this, condition, alternate->bind(this));

    newStack = splitStack(signature, enclosingStack);
    result = ControlType::if_(signature, tmpsForSignature(signature), WTFMove(alternate), WTFMove(continuation));
    return { };
}

auto LLIntGenerator::addElse(ControlType& data, const ExpressionList& currentStack) -> PartialResult
{
    ASSERT(WTF::holds_alternative<ControlIf>(data));
    unifyValuesWithBlock(data.m_results, currentStack);
    WasmJmp::emit(this, data.m_continuation->bind(this));
    return addElseToUnreachable(data);
}

auto LLIntGenerator::addElseToUnreachable(ControlType& data) -> PartialResult
{
    ControlIf& control = WTF::get<ControlIf>(data);
    emitLabel(control.m_alternate.get());
    data = ControlType::block(data.m_signature, WTFMove(data.m_results), WTFMove(data.m_continuation));
    return { };
}

auto LLIntGenerator::addReturn(const ControlType& data, const ExpressionList& returnValues) -> PartialResult
{
    if (!data.m_signature->returnCount()) {
        WasmRetVoid::emit(this);
        return { };
    }

    LLIntCallInformation info = callInformationFor(*data.m_signature, CallRole::Callee);
    unifyValuesWithBlock(info.results, returnValues);
    WasmRet::emit(this);

    return { };
}

auto LLIntGenerator::addBranch(ControlType& data, ExpressionType condition, const ExpressionList& returnValues) -> PartialResult
{
    unifyValuesWithBlock(data.m_results, returnValues);

    RefPtr<Label> target = data.targetLabelForBranch();
    if (condition)
        WasmJtrue::emit(this, condition, target->bind(this));
    else
        WasmJmp::emit(this, target->bind(this));

    return { };
}

auto LLIntGenerator::addSwitch(ExpressionType condition, const Vector<ControlType*>& targets, ControlType& defaultTarget, const ExpressionList& expressionStack) -> PartialResult
{
    unsigned tableIndex = m_codeBlock->numberOfJumpTables();
    FunctionCodeBlock::JumpTable& jumpTable = m_codeBlock->addJumpTable(targets.size());

    for (const auto& target : targets)
        unifyValuesWithBlock(target->m_results, expressionStack);
    unifyValuesWithBlock(defaultTarget.m_results, expressionStack);

    WasmSwitch::emit(this, condition, tableIndex, defaultTarget.targetLabelForBranch()->bind(this));

    unsigned index = 0;
    InstructionStream::Offset offset = m_lastInstruction.offset();
    for (const auto& target : targets) {
        RefPtr<Label> targetLabel = target->targetLabelForBranch();
        if (targetLabel->isForward()) {
            auto result = m_switches.add(targetLabel.get(), Vector<SwitchEntry>());
            ASSERT(!jumpTable[index]);
            result.iterator->value.append({ offset, &jumpTable[index++] });
        } else {
            int jumpTarget = targetLabel->bind(this).target();
            ASSERT(jumpTarget);
            jumpTable[index++] = jumpTarget;
        }
    }


    return { };
}

auto LLIntGenerator::endBlock(ControlEntry& entry, ExpressionList& expressionStack) -> PartialResult
{
    ControlType& data = entry.controlData;

    if (!WTF::holds_alternative<ControlLoop>(data))
        unifyValuesWithBlock(data.m_results, expressionStack);

    return addEndToUnreachable(entry, expressionStack);
}


auto LLIntGenerator::addEndToUnreachable(ControlEntry& entry, const Stack& expressionStack) -> PartialResult
{
    ControlType& data = entry.controlData;

    emitLabel(*data.m_continuation);

    if (!WTF::holds_alternative<ControlLoop>(data))
        entry.enclosedExpressionStack.appendVector(data.m_results);
    else {
        for (unsigned i = 0; i < data.m_signature->returnCount(); ++i) {
            if (i < expressionStack.size())
                entry.enclosedExpressionStack.append(expressionStack[i]);
            else
                entry.enclosedExpressionStack.append(newTemporary());
        }
    }

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (WTF::holds_alternative<ControlTopLevel>(data))
        return addReturn(data, entry.enclosedExpressionStack);

    return { };
}

auto LLIntGenerator::addCall(uint32_t functionIndex, const Signature& signature, Vector<ExpressionType>& args, ExpressionList& results) -> PartialResult
{
    ASSERT(signature.argumentCount() == args.size());
    LLIntCallInformation info = callInformationFor(signature);
    unifyValuesWithBlock(info.arguments, args);
    results = WTFMove(info.results);
    if (Context::useFastTLS())
        WasmCall::emit(this, functionIndex, info.stackOffset, info.numberOfStackArguments);
    else
        WasmCallNoTls::emit(this, functionIndex, info.stackOffset, info.numberOfStackArguments);

    return { };
}

auto LLIntGenerator::addCallIndirect(unsigned tableIndex, const Signature& signature, Vector<ExpressionType>& args, ExpressionList& results) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();

    ASSERT(signature.argumentCount() == args.size());
    ASSERT(m_info.tableCount() > tableIndex);
    ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

    LLIntCallInformation info = callInformationFor(signature);
    unifyValuesWithBlock(info.arguments, args);
    results = WTFMove(info.results);
    if (Context::useFastTLS())
        WasmCallIndirect::emit(this, calleeIndex, m_codeBlock->addSignature(signature), info.stackOffset, info.numberOfStackArguments, tableIndex);
    else
        WasmCallIndirectNoTls::emit(this, calleeIndex, m_codeBlock->addSignature(signature), info.stackOffset, info.numberOfStackArguments, tableIndex);

    return { };
}

auto LLIntGenerator::addRefIsNull(ExpressionType value, ExpressionType& result) -> PartialResult
{
    result = value;
    WasmRefIsNull::emit(this, result, value);

    return { };
}

auto LLIntGenerator::addRefFunc(uint32_t index, ExpressionType& result) -> PartialResult
{
    result = newTemporary();
    WasmRefFunc::emit(this, result, index);

    return { };
}

auto LLIntGenerator::addTableGet(unsigned tableIndex, ExpressionType index, ExpressionType& result) -> PartialResult
{
    result = index;
    WasmTableGet::emit(this, result, index, tableIndex);

    return { };
}

auto LLIntGenerator::addTableSet(unsigned tableIndex, ExpressionType index, ExpressionType value) -> PartialResult
{
    WasmTableSet::emit(this, index, value, tableIndex);

    return { };
}

auto LLIntGenerator::addTableSize(unsigned tableIndex, ExpressionType& result) -> PartialResult
{
    result = newTemporary();
    WasmTableSize::emit(this, result, tableIndex);

    return { };
}

auto LLIntGenerator::addTableGrow(unsigned tableIndex, ExpressionType fill, ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = fill;
    WasmTableGrow::emit(this, result, fill, delta, tableIndex);

    return { };
}

auto LLIntGenerator::addTableFill(unsigned tableIndex, ExpressionType offset, ExpressionType fill, ExpressionType count) -> PartialResult
{
    WasmTableFill::emit(this, offset, fill, count, tableIndex);

    return { };
}

auto LLIntGenerator::addUnreachable() -> PartialResult
{
    WasmUnreachable::emit(this);

    return { };
}

auto LLIntGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    result = newTemporary();
    WasmCurrentMemory::emit(this, result);

    return { };
}

auto LLIntGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = delta;
    WasmGrowMemory::emit(this, result, delta);

    return { };
}

auto LLIntGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    result = condition;
    WasmSelect::emit(this, result, condition, nonZero, zero);

    return { };
}

auto LLIntGenerator::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = pointer;
    switch (op) {
    case LoadOpType::I32Load8S:
        WasmI32Load8S::emit(this, result, pointer, offset);
        break;

    case LoadOpType::I64Load8S:
        WasmI64Load8S::emit(this, result, pointer, offset);
        break;

    case LoadOpType::I32Load8U:
    case LoadOpType::I64Load8U:
        WasmLoad8U::emit(this, result, pointer, offset);
        break;

    case LoadOpType::I32Load16S:
        WasmI32Load16S::emit(this, result, pointer, offset);
        break;

    case LoadOpType::I64Load16S:
        WasmI64Load16S::emit(this, result, pointer, offset);
        break;

    case LoadOpType::I32Load16U:
    case LoadOpType::I64Load16U:
        WasmLoad16U::emit(this, result, pointer, offset);
        break;

    case LoadOpType::I32Load:
    case LoadOpType::F32Load:
    case LoadOpType::I64Load32U:
        WasmLoad32U::emit(this, result, pointer, offset);
        break;

    case LoadOpType::I64Load32S:
        WasmI64Load32S::emit(this, result, pointer, offset);
        break;

    case LoadOpType::I64Load:
    case LoadOpType::F64Load:
        WasmLoad64U::emit(this, result, pointer, offset);
        break;
    }

    return { };
}

auto LLIntGenerator::store(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    switch (op) {
    case StoreOpType::I64Store8:
    case StoreOpType::I32Store8:
        WasmStore8::emit(this, pointer, value, offset);
        break;

    case StoreOpType::I64Store16:
    case StoreOpType::I32Store16:
        WasmStore16::emit(this, pointer, value, offset);
        break;

    case StoreOpType::I64Store32:
    case StoreOpType::I32Store:
    case StoreOpType::F32Store:
        WasmStore32::emit(this, pointer, value, offset);
        break;

    case StoreOpType::I64Store:
    case StoreOpType::F64Store:
        WasmStore64::emit(this, pointer, value, offset);
        break;
    }

    return { };
}

}

template<>
void GenericLabel<Wasm::GeneratorTraits>::setLocation(BytecodeGeneratorBase<Wasm::GeneratorTraits>& generator, unsigned location)
{
    RELEASE_ASSERT(isForward());

    m_location = location;

    Wasm::LLIntGenerator* llintGenerator = static_cast<Wasm::LLIntGenerator*>(&generator);

    auto it = llintGenerator->m_switches.find(this);
    if (it != llintGenerator->m_switches.end()) {
        for (const auto& entry : it->value) {
            ASSERT(!*entry.jumpTarget);
            *entry.jumpTarget = m_location - entry.offset;
        }
        llintGenerator->m_switches.remove(it);
    }


    for (auto offset : m_unresolvedJumps) {
        auto instruction = generator.m_writer.ref(offset);
        int target = m_location - offset;

#define CASE(__op) \
    case __op::opcodeID:  \
        instruction->cast<__op, WasmOpcodeTraits>()->setTargetLabel(BoundLabel(target), [&]() { \
            generator.m_codeBlock->addOutOfLineJumpTarget(instruction.offset(), target); \
            return BoundLabel(); \
        }); \
        break;

        switch (instruction->opcodeID<WasmOpcodeTraits>()) {
        CASE(WasmJmp)
        CASE(WasmJtrue)
        CASE(WasmJfalse)
        case WasmSwitch::opcodeID: {
            ASSERT((!instruction->as<WasmSwitch, WasmOpcodeTraits>().m_defaultTarget));
            instruction->cast<WasmSwitch, WasmOpcodeTraits>()->setDefaultTarget(BoundLabel(target), [&]() {
                generator.m_codeBlock->addOutOfLineJumpTarget(instruction.offset(), target);
                return BoundLabel();
            });
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
#undef CASE
    }
}

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
