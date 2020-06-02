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
#include "JSCJSValueInlines.h"
#include "Label.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmFunctionCodeBlock.h"
#include "WasmFunctionParser.h"
#include "WasmGeneratorTraits.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>

namespace JSC { namespace Wasm {

class LLIntGenerator : public BytecodeGeneratorBase<GeneratorTraits> {
public:
    using ExpressionType = VirtualRegister;

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

        static ControlType topLevel(BlockSignature signature, unsigned stackSize, RefPtr<Label>&& continuation)
        {
            return ControlType(signature, stackSize, WTFMove(continuation), ControlTopLevel { });
        }

        static ControlType loop(BlockSignature signature, unsigned stackSize, Ref<Label>&& body, RefPtr<Label>&& continuation)
        {
            return ControlType(signature, stackSize - signature->argumentCount(), WTFMove(continuation), ControlLoop { WTFMove(body) });
        }

        static ControlType block(BlockSignature signature, unsigned stackSize, RefPtr<Label>&& continuation)
        {
            return ControlType(signature, stackSize - signature->argumentCount(), WTFMove(continuation), ControlBlock { });
        }

        static ControlType if_(BlockSignature signature, unsigned stackSize, Ref<Label>&& alternate, RefPtr<Label>&& continuation)
        {
            return ControlType(signature, stackSize - signature->argumentCount(), WTFMove(continuation), ControlIf { WTFMove(alternate) });
        }

        static bool isIf(const ControlType& control) { return WTF::holds_alternative<ControlIf>(control); }
        static bool isTopLevel(const ControlType& control) { return WTF::holds_alternative<ControlTopLevel>(control); }

        unsigned stackSize() const { return m_stackSize; }
        BlockSignature signature() const { return m_signature; }

        RefPtr<Label> targetLabelForBranch() const
        {
            if (WTF::holds_alternative<ControlLoop>(*this))
                return WTF::get<ControlLoop>(*this).m_body.ptr();
            return m_continuation;
        }

        SignatureArgCount branchTargetArity() const
        {
            if (WTF::holds_alternative<ControlLoop>(*this))
                return m_signature->argumentCount();
            return m_signature->returnCount();
        }

        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            if (WTF::holds_alternative<ControlLoop>(*this))
                return m_signature->argument(i);
            return m_signature->returnType(i);
        }

        BlockSignature m_signature;
        unsigned m_stackSize;
        RefPtr<Label> m_continuation;

    private:
        template<typename T>
        ControlType(BlockSignature signature, unsigned stackSize, RefPtr<Label>&& continuation, T&& t)
            : Base(WTFMove(t))
            , m_signature(signature)
            , m_stackSize(stackSize)
            , m_continuation(WTFMove(continuation))
        {
        }
    };

    using ErrorType = String;
    using PartialResult = Expected<void, ErrorType>;
    using UnexpectedResult = Unexpected<ErrorType>;

    using ControlEntry = FunctionParser<LLIntGenerator>::ControlEntry;
    using ControlStack = FunctionParser<LLIntGenerator>::ControlStack;
    using ResultList = FunctionParser<LLIntGenerator>::ResultList;
    using Stack = FunctionParser<LLIntGenerator>::Stack;
    using TypedExpression = FunctionParser<LLIntGenerator>::TypedExpression;

    static ExpressionType emptyExpression() { return { }; };

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module failed compiling: "_s, makeString(args)...));
    }

    LLIntGenerator(const ModuleInformation&, unsigned functionIndex, const Signature&);

    std::unique_ptr<FunctionCodeBlock> finalize();

    template<typename ExpressionListA, typename ExpressionListB>
    void unifyValuesWithBlock(const ExpressionListA& destinations, const ExpressionListB& values)
    {
        ASSERT(destinations.size() <= values.size());
        auto offset = values.size() - destinations.size();
        for (size_t i = 0; i < destinations.size(); ++i)
            WasmMov::emit(this, destinations[i], values[offset + i]);
    }

    enum NoConsistencyCheckTag { NoConsistencyCheck };
    ExpressionType push(NoConsistencyCheckTag)
    {
        m_maxStackSize = std::max(m_maxStackSize, ++m_stackSize);
        return virtualRegisterForLocal(m_stackSize - 1);
    }

    ExpressionType push()
    {
        checkConsistency();
        return push(NoConsistencyCheck);
    }

    void didPopValueFromStack() { --m_stackSize; }

    PartialResult WARN_UNUSED_RETURN addArguments(const Signature&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, int64_t);

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
    PartialResult WARN_UNUSED_RETURN addElse(ControlType&, Stack&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlType&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlType&, Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlType&, ExpressionType condition, Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlType*>& targets, ControlType& defaultTargets, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, const Stack& expressionStack = { }, bool unreachable = true);
    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&);

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const Signature&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const Signature&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addUnreachable();

    void didFinishParsingLocals();

    void setParser(FunctionParser<LLIntGenerator>* parser) { m_parser = parser; };

    // We need this for autogenerated templates used by JS bytecodes.
    void setUsesCheckpoints() const { UNREACHABLE_FOR_PLATFORM(); }

    void dump(const ControlStack&, const Stack*) { }

private:
    friend GenericLabel<Wasm::GeneratorTraits>;

    struct LLIntCallInformation {
        unsigned stackOffset;
        unsigned numberOfStackArguments;
        ResultList arguments;
        CompletionHandler<void(ResultList&)> commitResults;
    };

    LLIntCallInformation callInformationForCaller(const Signature&);
    Vector<VirtualRegister, 2> callInformationForCallee(const Signature&);
    void linkSwitchTargets(Label&, unsigned location);

    VirtualRegister virtualRegisterForWasmLocal(uint32_t index)
    {
        if (index < m_codeBlock->m_numArguments)
            return m_normalizedArguments[index];

        const auto& callingConvention = wasmCallingConvention();
        const uint32_t gprCount = callingConvention.gprArgs.size();
        const uint32_t fprCount = callingConvention.fprArgs.size();
        return virtualRegisterForLocal(index - m_codeBlock->m_numArguments + gprCount + fprCount + numberOfLLIntCalleeSaveRegisters);
    }

    ExpressionType jsNullConstant()
    {
        if (UNLIKELY(!m_jsNullConstant.isValid())) {
            m_jsNullConstant = VirtualRegister(FirstConstantRegisterIndex + m_codeBlock->m_constants.size());
            m_codeBlock->m_constants.append(JSValue::encode(jsNull()));
            if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
                m_codeBlock->m_constantTypes.append(Type::Anyref);
        }
        return m_jsNullConstant;
    }

    ExpressionType zeroConstant()
    {
        if (UNLIKELY(!m_zeroConstant.isValid())) {
            m_zeroConstant = VirtualRegister(FirstConstantRegisterIndex + m_codeBlock->m_constants.size());
            m_codeBlock->m_constants.append(0);
            if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
                m_codeBlock->m_constantTypes.append(Type::I32);
        }
        return m_zeroConstant;
    }

    void getDropKeepCount(const ControlType& target, unsigned& startOffset, unsigned& drop, unsigned& keep)
    {
        startOffset = target.stackSize() + 1;
        keep = target.branchTargetArity();
        drop = m_stackSize - target.stackSize() - target.branchTargetArity();
    }

    void dropKeep(Stack& values, const ControlType& target, bool dropValues)
    {
        unsigned startOffset;
        unsigned keep;
        unsigned drop;

        getDropKeepCount(target, startOffset, drop, keep);

        if (dropValues)
            values.shrink(keep);

        if (!drop)
            return;

        if (keep)
            WasmDropKeep::emit(this, startOffset, drop, keep);
    }

    template<typename Functor>
    void walkExpressionStack(Stack& expressionStack, unsigned stackSize, const Functor& functor)
    {
        for (unsigned i = expressionStack.size(); i > 0; --i) {
            VirtualRegister slot = virtualRegisterForLocal(stackSize - i);
            functor(expressionStack[expressionStack.size() - i], slot);
        }
    }

    template<typename Functor>
    void walkExpressionStack(Stack& expressionStack, const Functor& functor)
    {
        walkExpressionStack(expressionStack, m_stackSize, functor);
    }

    template<typename Functor>
    void walkExpressionStack(ControlEntry& entry, const Functor& functor)
    {
        walkExpressionStack(entry.enclosedExpressionStack, entry.controlData.stackSize(), functor);
    }

    void checkConsistency()
    {
#if ASSERT_ENABLED
        // The rules for locals and constants in the stack are:
        // 1) Locals have to be materialized whenever a control entry is pushed to the control stack (i.e. every time we splitStack)
        //    NOTE: This is a trade-off so that set_local does not have to walk up the control stack looking for delayed get_locals
        // 2) If the control entry is a loop, we also need to materialize constants in the newStack, since those slots will be written
        //    to from loop back edges
        // 3) Both locals and constants have to be materialized before branches, since multiple branches might share the same target,
        //    we can't make any assumptions about the stack state at that point, so we materialize the stack.
        for (ControlEntry& controlEntry : m_parser->controlStack()) {
            walkExpressionStack(controlEntry, [&](VirtualRegister expression, VirtualRegister slot) {
                ASSERT(expression == slot || expression.isConstant());
            });
        }
        walkExpressionStack(m_parser->expressionStack(), [&](VirtualRegister expression, VirtualRegister slot) {
            ASSERT(expression == slot || expression.isConstant() || expression.isArgument() || expression.toLocal() < m_codeBlock->m_numVars);
        });
#endif // ASSERT_ENABLED
    }

    void materializeConstantsAndLocals(Stack& expressionStack)
    {
        if (expressionStack.isEmpty())
            return;

        checkConsistency();
        walkExpressionStack(expressionStack, [&](TypedExpression& expression, VirtualRegister slot) {
            ASSERT(expression.value() == slot || expression.value().isConstant() || expression.value().isArgument() || expression.value().toLocal() < m_codeBlock->m_numVars);
            if (expression.value() == slot)
                return;
            WasmMov::emit(this, slot, expression);
            expression = TypedExpression { expression.type(), slot };
        });
        checkConsistency();
    }

    void splitStack(BlockSignature signature, Stack& enclosingStack, Stack& newStack)
    {
        JSC::Wasm::splitStack(signature, enclosingStack, newStack);

        m_stackSize -= newStack.size();
        checkConsistency();
        walkExpressionStack(enclosingStack, [&](TypedExpression& expression, VirtualRegister slot) {
            ASSERT(expression.value() == slot || expression.value().isConstant() || expression.value().isArgument() || expression.value().toLocal() < m_codeBlock->m_numVars);
            if (expression.value() == slot || expression.value().isConstant())
                return;
            WasmMov::emit(this, slot, expression);
            expression = TypedExpression { expression.type(), slot };
        });
        checkConsistency();
        m_stackSize += newStack.size();
    }

    struct SwitchEntry {
        InstructionStream::Offset offset;
        int* jumpTarget;
    };

    struct ConstantMapHashTraits : WTF::GenericHashTraits<EncodedJSValue> {
        static constexpr bool emptyValueIsZero = true;
        static void constructDeletedValue(EncodedJSValue& slot) { slot = JSValue::encode(jsNull()); }
        static bool isDeletedValue(EncodedJSValue value) { return value == JSValue::encode(jsNull()); }
    };

    FunctionParser<LLIntGenerator>* m_parser { nullptr };
    const ModuleInformation& m_info;
    const unsigned m_functionIndex { UINT_MAX };
    Vector<VirtualRegister> m_normalizedArguments;
    HashMap<Label*, Vector<SwitchEntry>> m_switches;
    ExpressionType m_jsNullConstant;
    ExpressionType m_zeroConstant;
    ResultList m_unitializedLocals;
    HashMap<EncodedJSValue, VirtualRegister, WTF::IntHash<EncodedJSValue>, ConstantMapHashTraits> m_constantMap;
    Vector<VirtualRegister, 2> m_results;
    unsigned m_stackSize { 0 };
    unsigned m_maxStackSize { 0 };
};

Expected<std::unique_ptr<FunctionCodeBlock>, String> parseAndCompileBytecode(const uint8_t* functionStart, size_t functionLength, const Signature& signature, const ModuleInformation& info, uint32_t functionIndex)
{
    LLIntGenerator llintGenerator(info, functionIndex, signature);
    FunctionParser<LLIntGenerator> parser(llintGenerator, functionStart, functionLength, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    return llintGenerator.finalize();
}


using Buffer = InstructionStream::InstructionBuffer;
static ThreadSpecific<Buffer>* threadSpecificBufferPtr;

static ThreadSpecific<Buffer>& threadSpecificBuffer()
{
    static std::once_flag flag;
    std::call_once(
        flag,
        [] () {
            threadSpecificBufferPtr = new ThreadSpecific<Buffer>();
        });
    return *threadSpecificBufferPtr;
}

LLIntGenerator::LLIntGenerator(const ModuleInformation& info, unsigned functionIndex, const Signature&)
    : BytecodeGeneratorBase(makeUnique<FunctionCodeBlock>(functionIndex), 0)
    , m_info(info)
    , m_functionIndex(functionIndex)
{
    {
        auto& threadSpecific = threadSpecificBuffer();
        Buffer buffer = WTFMove(*threadSpecific);
        *threadSpecific = Buffer();
        m_writer.setInstructionBuffer(WTFMove(buffer));
    }

    m_codeBlock->m_numVars = numberOfLLIntCalleeSaveRegisters;
    m_stackSize = numberOfLLIntCalleeSaveRegisters;
    m_maxStackSize = numberOfLLIntCalleeSaveRegisters;

    WasmEnter::emit(this);
}

std::unique_ptr<FunctionCodeBlock> LLIntGenerator::finalize()
{
    RELEASE_ASSERT(m_codeBlock);
    m_codeBlock->m_numCalleeLocals = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_maxStackSize);

    auto& threadSpecific = threadSpecificBuffer();
    Buffer usedBuffer;
    m_codeBlock->setInstructions(m_writer.finalize(usedBuffer));
    size_t oldCapacity = usedBuffer.capacity();
    usedBuffer.resize(0);
    RELEASE_ASSERT(usedBuffer.capacity() == oldCapacity);
    *threadSpecific = WTFMove(usedBuffer);

    return WTFMove(m_codeBlock);
}

// Generated from wasm.json
#include "WasmLLIntGeneratorInlines.h"

auto LLIntGenerator::callInformationForCaller(const Signature& signature) -> LLIntCallInformation
{
    // This function sets up the stack layout for calls. The desired stack layout is:

    // FPRn
    // ...
    // FPR1
    // FPR0
    // ---
    // GPRn
    // ...
    // GPR1
    // GPR0
    // ----
    // stackN
    // ...
    // stack1
    // stack0
    // ---
    // call frame header

    // We need to allocate at least space for all GPRs and FPRs.
    // Return values use the same allocation layout.

    const auto initialStackSize = m_stackSize;

    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.gprArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();

    uint32_t stackCount = 0;
    uint32_t gprIndex = 0;
    uint32_t fprIndex = 0;
    uint32_t stackIndex = 0;

    auto allocateStackRegister = [&](Type type) {
        switch (type) {
        case Type::I32:
        case Type::I64:
        case Type::Anyref:
        case Type::Funcref:
            if (gprIndex < gprCount)
                ++gprIndex;
            else if (stackIndex++ >= stackCount)
                ++stackCount;
            break;
        case Type::F32:
        case Type::F64:
            if (fprIndex < fprCount)
                ++fprIndex;
            else if (stackIndex++ >= stackCount)
                ++stackCount;
            break;
        case Void:
        case Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };


    for (uint32_t i = 0; i < signature.argumentCount(); i++)
        allocateStackRegister(signature.argument(i));

    gprIndex = 0;
    fprIndex = 0;
    stackIndex = 0;
    for (uint32_t i = 0; i < signature.returnCount(); i++)
        allocateStackRegister(signature.returnType(i));

    // FIXME: we are allocating the extra space for the argument/return count in order to avoid interference, but we could do better
    // NOTE: We increase arg count by 1 for the case of indirect calls
    m_stackSize += std::max(signature.argumentCount() + 1, signature.returnCount()) + gprCount + fprCount + stackCount + CallFrame::headerSizeInRegisters;
    if (m_stackSize % stackAlignmentRegisters())
        ++m_stackSize;
    if (m_maxStackSize < m_stackSize)
        m_maxStackSize = m_stackSize;


    ResultList arguments(signature.argumentCount());
    ResultList temporaryResults(signature.returnCount());

    const unsigned stackOffset = m_stackSize;
    const unsigned base = stackOffset - CallFrame::headerSizeInRegisters;

    const uint32_t gprLimit = base - stackCount - gprCount;
    const uint32_t fprLimit = gprLimit - fprCount;

    stackIndex = base;
    gprIndex = base - stackCount;
    fprIndex = gprIndex - gprCount;
    for (uint32_t i = 0; i < signature.argumentCount(); i++) {
        switch (signature.argument(i)) {
        case Type::I32:
        case Type::I64:
        case Type::Anyref:
        case Type::Funcref:
            if (gprIndex > gprLimit)
                arguments[i] = virtualRegisterForLocal(--gprIndex);
            else
                arguments[i] = virtualRegisterForLocal(--stackIndex);
            break;
        case Type::F32:
        case Type::F64:
            if (fprIndex > fprLimit)
                arguments[i] = virtualRegisterForLocal(--fprIndex);
            else
                arguments[i] = virtualRegisterForLocal(--stackIndex);
            break;
        case Void:
        case Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    stackIndex = base;
    gprIndex = base - stackCount;
    fprIndex = gprIndex - gprCount;
    for (uint32_t i = 0; i < signature.returnCount(); i++) {
        switch (signature.returnType(i)) {
        case Type::I32:
        case Type::I64:
        case Type::Anyref:
        case Type::Funcref:
            if (gprIndex > gprLimit)
                temporaryResults[i] = virtualRegisterForLocal(--gprIndex);
            else
                temporaryResults[i] = virtualRegisterForLocal(--stackIndex);
            break;
        case Type::F32:
        case Type::F64:
            if (fprIndex > fprLimit)
                temporaryResults[i] = virtualRegisterForLocal(--fprIndex);
            else
                temporaryResults[i] = virtualRegisterForLocal(--stackIndex);
            break;
        case Void:
        case Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    m_stackSize = initialStackSize;

    auto commitResults = [this, temporaryResults = WTFMove(temporaryResults)](ResultList& results) {
        checkConsistency();
        for (auto temporaryResult : temporaryResults) {
            ExpressionType result = push(NoConsistencyCheck);
            WasmMov::emit(this, result, temporaryResult);
            results.append(result);
        }
    };

    return LLIntCallInformation { stackOffset, stackCount, WTFMove(arguments), WTFMove(commitResults) };
}

auto LLIntGenerator::callInformationForCallee(const Signature& signature) -> Vector<VirtualRegister, 2>
{
    if (m_results.size())
        return m_results;

    m_results.reserveInitialCapacity(signature.returnCount());

    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.gprArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();

    uint32_t gprIndex = 0;
    uint32_t fprIndex = gprCount;
    uint32_t stackIndex = 0;
    const uint32_t maxGPRIndex = gprCount;
    const uint32_t maxFPRIndex = maxGPRIndex + fprCount;

    for (uint32_t i = 0; i < signature.returnCount(); i++) {
        switch (signature.returnType(i)) {
        case Type::I32:
        case Type::I64:
        case Type::Anyref:
        case Type::Funcref:
            if (gprIndex < maxGPRIndex)
                m_results.append(virtualRegisterForLocal(numberOfLLIntCalleeSaveRegisters + gprIndex++));
            else
                m_results.append(virtualRegisterForArgumentIncludingThis(stackIndex++));
            break;
        case Type::F32:
        case Type::F64:
            if (fprIndex < maxFPRIndex)
                m_results.append(virtualRegisterForLocal(numberOfLLIntCalleeSaveRegisters + fprIndex++));
            else
                m_results.append(virtualRegisterForArgumentIncludingThis(stackIndex++));
            break;
        case Void:
        case Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    return m_results;
}

auto LLIntGenerator::addArguments(const Signature& signature) -> PartialResult
{
    checkConsistency();

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

    Vector<VirtualRegister> registerArguments(gprCount + fprCount);
    for (uint32_t i = 0; i < gprCount + fprCount; i++)
        registerArguments[i] = push(NoConsistencyCheck);

    const auto addArgument = [&](uint32_t index, uint32_t& count, uint32_t max) {
        if (count < max)
            m_normalizedArguments[index] = registerArguments[count++];
        else
            m_normalizedArguments[index] = virtualRegisterForArgumentIncludingThis(stackIndex++);
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

    m_codeBlock->m_numVars += gprCount + fprCount;

    return { };
}

auto LLIntGenerator::addLocal(Type type, uint32_t count) -> PartialResult
{
    checkConsistency();

    m_codeBlock->m_numVars += count;
    switch (type) {
    case Type::Anyref:
    case Type::Funcref:
        while (count--)
            m_unitializedLocals.append(push(NoConsistencyCheck));
        break;
    default:
        m_stackSize += count;
        break;
    }
    return { };
}

void LLIntGenerator::didFinishParsingLocals()
{
    if (m_unitializedLocals.isEmpty())
        return;

    auto null = jsNullConstant();
    for (auto local : m_unitializedLocals)
        WasmMov::emit(this, local, null);
    m_unitializedLocals.clear();
}

auto LLIntGenerator::addConstant(Type type, int64_t value) -> ExpressionType
{
    auto constant = [&] {
        if (!value)
            return zeroConstant();

        if (value == JSValue::encode(jsNull()))
            return jsNullConstant();

        VirtualRegister source(FirstConstantRegisterIndex + m_codeBlock->m_constants.size());
        auto result = m_constantMap.add(value, source);
        if (!result.isNewEntry)
            return result.iterator->value;
        m_codeBlock->m_constants.append(value);
        if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
            m_codeBlock->m_constantTypes.append(type);
        return source;
    };
    // leave a hole if we need to materialize the constant
    push();
    return constant();
}

auto LLIntGenerator::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    // leave a hole if we need to materialize the local
    push();
    result = virtualRegisterForWasmLocal(index);
    return { };
}

auto LLIntGenerator::setLocal(uint32_t index, ExpressionType value) -> PartialResult
{
    VirtualRegister target = virtualRegisterForWasmLocal(index);

    // If this local is currently on the stack we need to materialize it, otherwise it'll see the new value instead of the old one
    walkExpressionStack(m_parser->expressionStack(), [&](TypedExpression& expression, VirtualRegister slot) {
        if (expression.value() != target)
            return;
        WasmMov::emit(this, slot, expression);
        expression = TypedExpression { expression.type(), slot };
    });

    WasmMov::emit(this, target, value);

    return { };
}

auto LLIntGenerator::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    result = push();
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        WasmGetGlobal::emit(this, result, index);
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        WasmGetGlobalPortableBinding::emit(this, result, index);
        break;
    }
    return { };
}

auto LLIntGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Type type = global.type;
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        if (isSubtype(type, Anyref))
            WasmSetGlobalRef::emit(this, index, value);
        else
            WasmSetGlobal::emit(this, index, value);
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        if (isSubtype(type, Anyref))
            WasmSetGlobalRefPortableBinding::emit(this, index, value);
        else
            WasmSetGlobalPortableBinding::emit(this, index, value);
        break;
    }
    return { };
}

auto LLIntGenerator::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex) -> PartialResult
{
    splitStack(signature, enclosingStack, newStack);
    materializeConstantsAndLocals(newStack);

    Ref<Label> body = newEmittedLabel();
    Ref<Label> continuation = newLabel();

    block = ControlType::loop(signature, m_stackSize, WTFMove(body), WTFMove(continuation));

    Vector<VirtualRegister> osrEntryData;
    for (uint32_t i = 0; i < m_codeBlock->m_numArguments; i++)
        osrEntryData.append(m_normalizedArguments[i]);

    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.gprArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();
    for (int32_t i = gprCount + fprCount + numberOfLLIntCalleeSaveRegisters; i < m_codeBlock->m_numVars; i++)
        osrEntryData.append(virtualRegisterForLocal(i));
    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        Stack& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        for (TypedExpression expression : expressionStack)
            osrEntryData.append(expression);
    }
    for (TypedExpression expression : enclosingStack)
        osrEntryData.append(expression);

    WasmLoopHint::emit(this);

    m_codeBlock->tierUpCounter().addOSREntryDataForLoop(m_lastInstruction.offset(), { loopIndex, WTFMove(osrEntryData) });

    return { };
}

auto LLIntGenerator::addTopLevel(BlockSignature signature) -> ControlType
{
    return ControlType::topLevel(signature, m_stackSize, newLabel());
}

auto LLIntGenerator::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> PartialResult
{
    splitStack(signature, enclosingStack, newStack);
    newBlock = ControlType::block(signature, m_stackSize, newLabel());
    return { };
}

auto LLIntGenerator::addIf(ExpressionType condition, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    Ref<Label> alternate = newLabel();
    Ref<Label> continuation = newLabel();

    splitStack(signature, enclosingStack, newStack);

    WasmJfalse::emit(this, condition, alternate->bind(this));

    result = ControlType::if_(signature, m_stackSize, WTFMove(alternate), WTFMove(continuation));
    return { };
}

auto LLIntGenerator::addElse(ControlType& data, Stack& expressionStack) -> PartialResult
{
    ASSERT(WTF::holds_alternative<ControlIf>(data));
    materializeConstantsAndLocals(expressionStack);
    WasmJmp::emit(this, data.m_continuation->bind(this));
    return addElseToUnreachable(data);
}

auto LLIntGenerator::addElseToUnreachable(ControlType& data) -> PartialResult
{
    m_stackSize = data.stackSize() + data.m_signature->argumentCount();

    ControlIf& control = WTF::get<ControlIf>(data);
    emitLabel(control.m_alternate.get());
    data = ControlType::block(data.m_signature, m_stackSize, WTFMove(data.m_continuation));
    return { };
}

auto LLIntGenerator::addReturn(const ControlType& data, Stack& returnValues) -> PartialResult
{
    if (!data.m_signature->returnCount()) {
        WasmRetVoid::emit(this);
        return { };
    }

    // no need to drop keep here, since we have to move anyway
    unifyValuesWithBlock(callInformationForCallee(*data.m_signature), returnValues);
    WasmRet::emit(this);

    return { };
}

auto LLIntGenerator::addBranch(ControlType& data, ExpressionType condition, Stack& returnValues) -> PartialResult
{
    RefPtr<Label> target = data.targetLabelForBranch();
    RefPtr<Label> skip = nullptr;

    materializeConstantsAndLocals(returnValues);

    if (condition.isValid()) {
        skip = newLabel();
        WasmJfalse::emit(this, condition, skip->bind(this));
    }

    dropKeep(returnValues, data, !skip);
    WasmJmp::emit(this, target->bind(this));

    if (skip)
        emitLabel(*skip);

    return { };
}

auto LLIntGenerator::addSwitch(ExpressionType condition, const Vector<ControlType*>& targets, ControlType& defaultTarget, Stack& expressionStack) -> PartialResult
{
    materializeConstantsAndLocals(expressionStack);

    unsigned tableIndex = m_codeBlock->numberOfJumpTables();
    FunctionCodeBlock::JumpTable& jumpTable = m_codeBlock->addJumpTable(targets.size() + 1);

    WasmSwitch::emit(this, condition, tableIndex);

    unsigned index = 0;
    InstructionStream::Offset offset = m_lastInstruction.offset();

    auto addTarget = [&](ControlType& target) {
        RefPtr<Label> targetLabel = target.targetLabelForBranch();

        getDropKeepCount(target, jumpTable[index].startOffset, jumpTable[index].dropCount, jumpTable[index].keepCount);

        if (targetLabel->isForward()) {
            auto result = m_switches.add(targetLabel.get(), Vector<SwitchEntry>());
            ASSERT(!jumpTable[index].target);
            result.iterator->value.append(SwitchEntry { offset, &jumpTable[index++].target });
        } else {
            int jumpTarget = targetLabel->location() - offset;
            ASSERT(jumpTarget);
            jumpTable[index++].target = jumpTarget;
        }
    };

    for (const auto& target : targets)
        addTarget(*target);
    addTarget(defaultTarget);

    return { };
}

auto LLIntGenerator::endBlock(ControlEntry& entry, Stack& expressionStack) -> PartialResult
{
    // FIXME: We only need to materialize constants here if there exists a jump to this label
    // https://bugs.webkit.org/show_bug.cgi?id=203657
    materializeConstantsAndLocals(expressionStack);
    return addEndToUnreachable(entry, expressionStack, false);
}


auto LLIntGenerator::addEndToUnreachable(ControlEntry& entry, const Stack& expressionStack, bool unreachable) -> PartialResult
{
    ControlType& data = entry.controlData;

    RELEASE_ASSERT(unreachable || m_stackSize == data.stackSize() + data.m_signature->returnCount());

    m_stackSize = data.stackSize();

    for (unsigned i = 0; i < data.m_signature->returnCount(); ++i) {
        // We don't want to do a consistency check here because we just reset the stack size
        // are pushing new values, while we already have the same values in the stack.
        // The only reason we do things this way is so that it also works for unreachable blocks,
        // since they might not have the right number of values in the expression stack.
        // Instead, we do a stricter consistency check below.
        auto tmp = push(NoConsistencyCheck);
        ASSERT(unreachable || tmp == expressionStack[i].value());
        if (unreachable)
            entry.enclosedExpressionStack.constructAndAppend(data.m_signature->returnType(i), tmp);
        else
            entry.enclosedExpressionStack.append(expressionStack[i]);
    }

    if (m_lastOpcodeID == wasm_jmp && data.m_continuation->unresolvedJumps().size() == 1 && data.m_continuation->unresolvedJumps()[0] == static_cast<int>(m_lastInstruction.offset())) {
        linkSwitchTargets(*data.m_continuation, m_lastInstruction.offset());
        m_lastOpcodeID = wasm_unreachable;
        m_writer.rewind(m_lastInstruction);
    } else
        emitLabel(*data.m_continuation);

    return { };
}

auto LLIntGenerator::endTopLevel(BlockSignature signature, const Stack& expressionStack) -> PartialResult
{
    RELEASE_ASSERT(expressionStack.size() == signature->returnCount());

    if (!signature->returnCount()) {
        WasmRetVoid::emit(this);
        return { };
    }

    checkConsistency();
    unifyValuesWithBlock(callInformationForCallee(*signature), expressionStack);
    WasmRet::emit(this);

    return { };
}

auto LLIntGenerator::addCall(uint32_t functionIndex, const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ASSERT(signature.argumentCount() == args.size());
    LLIntCallInformation info = callInformationForCaller(signature);
    unifyValuesWithBlock(info.arguments, args);
    if (Context::useFastTLS())
        WasmCall::emit(this, functionIndex, info.stackOffset, info.numberOfStackArguments);
    else
        WasmCallNoTls::emit(this, functionIndex, info.stackOffset, info.numberOfStackArguments);
    info.commitResults(results);

    return { };
}

auto LLIntGenerator::addCallIndirect(unsigned tableIndex, const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();

    ASSERT(signature.argumentCount() == args.size());
    ASSERT(m_info.tableCount() > tableIndex);
    ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

    LLIntCallInformation info = callInformationForCaller(signature);
    unifyValuesWithBlock(info.arguments, args);
    if (Context::useFastTLS())
        WasmCallIndirect::emit(this, calleeIndex, m_codeBlock->addSignature(signature), info.stackOffset, info.numberOfStackArguments, tableIndex);
    else
        WasmCallIndirectNoTls::emit(this, calleeIndex, m_codeBlock->addSignature(signature), info.stackOffset, info.numberOfStackArguments, tableIndex);
    info.commitResults(results);

    return { };
}

auto LLIntGenerator::addRefIsNull(ExpressionType value, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmRefIsNull::emit(this, result, value);

    return { };
}

auto LLIntGenerator::addRefFunc(uint32_t index, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmRefFunc::emit(this, result, index);

    return { };
}

auto LLIntGenerator::addTableGet(unsigned tableIndex, ExpressionType index, ExpressionType& result) -> PartialResult
{
    result = push();
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
    result = push();
    WasmTableSize::emit(this, result, tableIndex);

    return { };
}

auto LLIntGenerator::addTableGrow(unsigned tableIndex, ExpressionType fill, ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = push();
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
    result = push();
    WasmCurrentMemory::emit(this, result);

    return { };
}

auto LLIntGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmGrowMemory::emit(this, result, delta);

    return { };
}

auto LLIntGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmSelect::emit(this, result, condition, nonZero, zero);

    return { };
}

auto LLIntGenerator::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = push();
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

void LLIntGenerator::linkSwitchTargets(Label& label, unsigned location)
{
    auto it = m_switches.find(&label);
    if (it != m_switches.end()) {
        for (const auto& entry : it->value) {
            ASSERT(!*entry.jumpTarget);
            *entry.jumpTarget = location - entry.offset;
        }
        m_switches.remove(it);
    }
}

}

template<>
void GenericLabel<Wasm::GeneratorTraits>::setLocation(BytecodeGeneratorBase<Wasm::GeneratorTraits>& generator, unsigned location)
{
    RELEASE_ASSERT(isForward());

    m_location = location;

    Wasm::LLIntGenerator* llintGenerator = static_cast<Wasm::LLIntGenerator*>(&generator);
    llintGenerator->linkSwitchTargets(*this, m_location);

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
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
#undef CASE
    }
}

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
