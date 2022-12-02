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
#include "WasmFunctionCodeBlockGenerator.h"
#include "WasmFunctionParser.h"
#include "WasmGeneratorTraits.h"
#include <variant>
#include <wtf/CompletionHandler.h>
#include <wtf/RefPtr.h>

namespace JSC { namespace Wasm {

class LLIntGenerator : public BytecodeGeneratorBase<GeneratorTraits> {
public:
    using ExpressionType = VirtualRegister;

    static constexpr bool tierSupportsSIMD = false;

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

    struct ControlTry {
        Ref<Label> m_try;
        unsigned m_tryDepth;
    };

    struct ControlCatch {
        CatchKind m_kind;
        Ref<Label> m_tryStart;
        Ref<Label> m_tryEnd;
        unsigned m_tryDepth;
        VirtualRegister m_exception;
    };

    struct CatchRewriteInfo {
        unsigned m_instructionOffset;
        unsigned m_tryDepth;
    };

    struct ControlType : public std::variant<ControlLoop, ControlTopLevel, ControlBlock, ControlIf, ControlTry, ControlCatch> {
        using Base = std::variant<ControlLoop, ControlTopLevel, ControlBlock, ControlIf, ControlTry, ControlCatch>;

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
            return ControlType(signature, stackSize - signature->as<FunctionSignature>()->argumentCount(), WTFMove(continuation), ControlLoop { WTFMove(body) });
        }

        static ControlType block(BlockSignature signature, unsigned stackSize, RefPtr<Label>&& continuation)
        {
            return ControlType(signature, stackSize - signature->as<FunctionSignature>()->argumentCount(), WTFMove(continuation), ControlBlock { });
        }

        static ControlType if_(BlockSignature signature, unsigned stackSize, Ref<Label>&& alternate, RefPtr<Label>&& continuation)
        {
            return ControlType(signature, stackSize - signature->as<FunctionSignature>()->argumentCount(), WTFMove(continuation), ControlIf { WTFMove(alternate) });
        }

        static ControlType createTry(BlockSignature signature, unsigned stackSize, Ref<Label>&& tryLabel, RefPtr<Label>&& continuation, unsigned tryDepth)
        {
            return ControlType(signature, stackSize - signature->as<FunctionSignature>()->argumentCount(), WTFMove(continuation), ControlTry { WTFMove(tryLabel), tryDepth });
        }

        static bool isLoop(const ControlType& control) { return std::holds_alternative<ControlLoop>(control); }
        static bool isTopLevel(const ControlType& control) { return std::holds_alternative<ControlTopLevel>(control); }
        static bool isBlock(const ControlType& control) { return std::holds_alternative<ControlBlock>(control); }
        static bool isIf(const ControlType& control) { return std::holds_alternative<ControlIf>(control); }
        static bool isTry(const ControlType& control) { return std::holds_alternative<ControlTry>(control); }
        static bool isAnyCatch(const ControlType& control) { return std::holds_alternative<ControlCatch>(control); }
        static bool isCatch(const ControlType& control)
        {
            if (!isAnyCatch(control))
                return false;
            ControlCatch catchData = std::get<ControlCatch>(control);
            return catchData.m_kind == CatchKind::Catch;
        }
        static bool isCatchAll(const ControlType& control)
        {
            if (!isAnyCatch(control))
                return false;
            ControlCatch catchData = std::get<ControlCatch>(control);
            return catchData.m_kind == CatchKind::CatchAll;
        }

        unsigned stackSize() const { return m_stackSize; }
        BlockSignature signature() const { return m_signature; }

        RefPtr<Label> targetLabelForBranch() const
        {
            if (isLoop(*this))
                return std::get<ControlLoop>(*this).m_body.ptr();
            return m_continuation;
        }

        FunctionArgCount branchTargetArity() const
        {
            if (isLoop(*this))
                return m_signature->as<FunctionSignature>()->argumentCount();
            return m_signature->as<FunctionSignature>()->returnCount();
        }

        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            if (isLoop(*this))
                return m_signature->as<FunctionSignature>()->argumentType(i);
            return m_signature->as<FunctionSignature>()->returnType(i);
        }

        void dump(PrintStream& out) const
        {
            if (isIf(*this))
                out.print("If:       ");
            else if (isBlock(*this))
                out.print("Block:    ");
            else if (isLoop(*this))
                out.print("Loop:     ");
            else if (isTopLevel(*this))
                out.print("TopLevel: ");
            else if (isTry(*this))
                out.print("Try:      ");
            else if (isCatch(*this))
                out.print("Catch:    ");
            else if (isCatchAll(*this))
                out.print("CatchAll: ");

            out.print("stackSize:(", stackSize(), ") ");
        }

        void convertToCatch(Ref<Label> tryEnd, VirtualRegister exception)
        {
            ASSERT(isTry(*this));
            auto& tryData = std::get<ControlTry>(*this);
            auto tryStart = WTFMove(tryData.m_try);
            auto tryDepth = WTFMove(tryData.m_tryDepth);
            auto continuation = WTFMove(m_continuation);
            *this = ControlType(m_signature, m_stackSize, WTFMove(continuation), ControlCatch { CatchKind::Catch, WTFMove(tryStart), WTFMove(tryEnd), WTFMove(tryDepth), exception });
        }

        BlockSignature m_signature;
        unsigned m_stackSize;
        RefPtr<Label> m_continuation;

    private:
        template<typename T>
        ControlType(BlockSignature signature, unsigned stackSize, RefPtr<Label>&& continuation, T&& t)
            : Base(std::forward<T>(t))
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

    LLIntGenerator(ModuleInformation&, unsigned functionIndex, const TypeDefinition&);

    std::unique_ptr<FunctionCodeBlockGenerator> finalize();

    template<typename ExpressionListA, typename ExpressionListB>
    void unifyValuesWithBlock(const ExpressionListA& destinations, const ExpressionListB& values)
    {
        ASSERT(destinations.size() <= values.size());
        auto offset = values.size() - destinations.size();
        for (size_t i = 0; i < destinations.size(); ++i) {
            auto& src = values[offset + i];
            auto& dst = destinations[i];
            if (static_cast<VirtualRegister>(src) != static_cast<VirtualRegister>(dst))
                WasmMov::emit(this, dst, src);
        }
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
    void notifyFunctionUsesSIMD() { ASSERT(Options::useWebAssemblySIMD()); m_usesSIMD = true; }

    PartialResult WARN_UNUSED_RETURN addArguments(const TypeDefinition&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, int64_t);

    // References
    PartialResult WARN_UNUSED_RETURN addRefIsNull(ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addRefFunc(uint32_t index, ExpressionType& result);

    // Tables
    PartialResult WARN_UNUSED_RETURN addTableGet(unsigned, ExpressionType index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableSet(unsigned, ExpressionType index, ExpressionType value);
    PartialResult WARN_UNUSED_RETURN addTableInit(unsigned, unsigned, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length);
    PartialResult WARN_UNUSED_RETURN addElemDrop(unsigned);
    PartialResult WARN_UNUSED_RETURN addTableSize(unsigned, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableGrow(unsigned, ExpressionType fill, ExpressionType delta, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addTableFill(unsigned, ExpressionType offset, ExpressionType fill, ExpressionType count);
    PartialResult WARN_UNUSED_RETURN addTableCopy(unsigned, unsigned, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length);

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
    PartialResult WARN_UNUSED_RETURN addMemoryFill(ExpressionType dstAddress, ExpressionType targetValue, ExpressionType count);
    PartialResult WARN_UNUSED_RETURN addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count);
    PartialResult WARN_UNUSED_RETURN addMemoryInit(unsigned, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length);
    PartialResult WARN_UNUSED_RETURN addDataDrop(unsigned);

    // Atomics
    PartialResult WARN_UNUSED_RETURN atomicLoad(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicStore(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicBinaryRMW(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicCompareExchange(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicWait(ExtAtomicOpType, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicNotify(ExtAtomicOpType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset);
    PartialResult WARN_UNUSED_RETURN atomicFence(ExtAtomicOpType, uint8_t flags);

    // Saturated truncation.
    PartialResult WARN_UNUSED_RETURN truncSaturated(Ext1OpType, ExpressionType operand, ExpressionType& result, Type, Type);

    // GC
    PartialResult WARN_UNUSED_RETURN addI31New(ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI31GetS(ExpressionType ref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI31GetU(ExpressionType ref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNew(uint32_t index, ExpressionType size, ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNewDefault(uint32_t index, ExpressionType size, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayGet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArraySet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType value);
    PartialResult WARN_UNUSED_RETURN addArrayLen(ExpressionType arrayref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructNew(uint32_t index, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructGet(ExpressionType structReference, const StructType&, uint32_t fieldIndex, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructSet(ExpressionType structReference, const StructType&, uint32_t fieldIndex, ExpressionType value);

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

    PartialResult WARN_UNUSED_RETURN addTry(BlockSignature, Stack& enclosingStack, ControlType& result, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addCatch(unsigned exceptionIndex, const TypeDefinition&, Stack&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchAll(Stack&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addCatchAllToUnreachable(ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegate(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegateToUnreachable(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&);
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlType&, Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlType&, ExpressionType condition, Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlType*>& targets, ControlType& defaultTargets, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, Stack& expressionStack, bool unreachable = true);
    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&);

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallRef(const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN addCrash();

    void didFinishParsingLocals();

    void setParser(FunctionParser<LLIntGenerator>* parser) { m_parser = parser; };

    // We need this for autogenerated templates used by JS bytecodes.
    void setUsesCheckpoints() const { UNREACHABLE_FOR_PLATFORM(); }

    void dump(const ControlStack&, const Stack*);

private:
    friend GenericLabel<Wasm::GeneratorTraits>;

    struct LLIntCallInformation {
        unsigned stackOffset;
        unsigned numberOfStackArguments;
        ResultList arguments;
        CompletionHandler<void(ResultList&)> commitResults;
    };

    LLIntCallInformation callInformationForCaller(const FunctionSignature&);
    Vector<VirtualRegister, 2> callInformationForCallee(const FunctionSignature&);
    void linkSwitchTargets(Label&, unsigned location);

    VirtualRegister virtualRegisterForWasmLocal(uint32_t index)
    {
        if (index < m_codeBlock->m_numArguments)
            return m_normalizedArguments[index];

        const auto& callingConvention = wasmCallingConvention();
        const uint32_t gprCount = callingConvention.jsrArgs.size();
        const uint32_t fprCount = callingConvention.fprArgs.size();
        return virtualRegisterForLocal(index - m_codeBlock->m_numArguments + gprCount + fprCount + numberOfLLIntCalleeSaveRegisters);
    }

    ExpressionType jsNullConstant()
    {
        if (UNLIKELY(!m_jsNullConstant.isValid())) {
            m_jsNullConstant = VirtualRegister(FirstConstantRegisterIndex + m_codeBlock->m_constants.size());
            m_codeBlock->m_constants.append(JSValue::encode(jsNull()));
            if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
                m_codeBlock->m_constantTypes.append(Types::Externref);
        }
        return m_jsNullConstant;
    }

    ExpressionType zeroConstant()
    {
        if (UNLIKELY(!m_zeroConstant.isValid())) {
            m_zeroConstant = VirtualRegister(FirstConstantRegisterIndex + m_codeBlock->m_constants.size());
            m_codeBlock->m_constants.append(0);
            if (UNLIKELY(Options::dumpGeneratedWasmBytecodes()))
                m_codeBlock->m_constantTypes.append(Types::I32);
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

    template<typename Stack, typename Functor>
    void walkExpressionStack(Stack& expressionStack, unsigned stackSize, const Functor& functor)
    {
        for (unsigned i = expressionStack.size(); i > 0; --i) {
            VirtualRegister slot = virtualRegisterForLocal(stackSize - i);
            functor(expressionStack[expressionStack.size() - i], slot);
        }
    }

    template<typename Stack, typename Functor>
    void walkExpressionStack(Stack& expressionStack, const Functor& functor)
    {
        walkExpressionStack(expressionStack, m_stackSize, functor);
    }

    template<typename Functor>
    void walkExpressionStack(ControlEntry& entry, const Functor& functor)
    {
        unsigned stackSize = entry.controlData.stackSize();
        walkExpressionStack(entry.enclosedExpressionStack, stackSize, functor);
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
            ASSERT(expression == slot || expression.isConstant() || expression.isArgument() || static_cast<unsigned>(expression.toLocal()) < m_codeBlock->m_numVars);
        });
#endif // ASSERT_ENABLED
    }

    void materializeConstantsAndLocals(Stack& expressionStack, NoConsistencyCheckTag)
    {
        walkExpressionStack(expressionStack, [&](auto& expression, VirtualRegister slot) {
            ASSERT(expression.value() == slot || expression.value().isConstant() || expression.value().isArgument() || static_cast<unsigned>(expression.value().toLocal()) < m_codeBlock->m_numVars);
            if (expression.value() == slot)
                return;
            WasmMov::emit(this, slot, expression);
            expression = TypedExpression { expression.type(), slot };
        });
    }

    void materializeConstantsAndLocals(Stack& expressionStack)
    {
        if (expressionStack.isEmpty())
            return;

        checkConsistency();
        materializeConstantsAndLocals(expressionStack, NoConsistencyCheck);
        checkConsistency();
    }

    void splitStack(BlockSignature signature, Stack& enclosingStack, Stack& newStack)
    {
        JSC::Wasm::splitStack(signature, enclosingStack, newStack);

        m_stackSize -= newStack.size();
        checkConsistency();
        walkExpressionStack(enclosingStack, [&](TypedExpression& expression, VirtualRegister slot) {
            ASSERT(expression.value() == slot || expression.value().isConstant() || expression.value().isArgument() || static_cast<unsigned>(expression.value().toLocal()) < m_codeBlock->m_numVars);
            if (expression.value() == slot || expression.value().isConstant())
                return;
            WasmMov::emit(this, slot, expression);
            expression = TypedExpression { expression.type(), slot };
        });
        checkConsistency();
        m_stackSize += newStack.size();
    }

    void finalizePreviousBlockForCatch(ControlType&, Stack&);

    struct SwitchEntry {
        WasmInstructionStream::Offset offset;
        int* jumpTarget;
    };

    struct ConstantMapHashTraits : HashTraits<EncodedJSValue> {
        static constexpr bool emptyValueIsZero = true;
        static void constructDeletedValue(EncodedJSValue& slot) { slot = JSValue::encode(jsNull()); }
        static bool isDeletedValue(EncodedJSValue value) { return value == JSValue::encode(jsNull()); }
    };

    FunctionParser<LLIntGenerator>* m_parser { nullptr };
    ModuleInformation& m_info;
    const unsigned m_functionIndex { UINT_MAX };
    Vector<VirtualRegister> m_normalizedArguments;
    HashMap<Label*, Vector<SwitchEntry>> m_switches;
    ExpressionType m_jsNullConstant;
    ExpressionType m_zeroConstant;
    ResultList m_unitializedLocals;
    HashMap<EncodedJSValue, VirtualRegister, WTF::IntHash<EncodedJSValue>, ConstantMapHashTraits> m_constantMap;
    Vector<VirtualRegister, 2> m_results;
    Checked<unsigned> m_stackSize { 0 };
    Checked<unsigned> m_maxStackSize { 0 };
    Checked<unsigned> m_tryDepth { 0 };
    bool m_usesExceptions { false };
    bool m_usesSIMD { false };
};

Expected<std::unique_ptr<FunctionCodeBlockGenerator>, String> parseAndCompileBytecode(const uint8_t* functionStart, size_t functionLength, const TypeDefinition& signature, ModuleInformation& info, uint32_t functionIndex)
{
    LLIntGenerator llintGenerator(info, functionIndex, signature);
    FunctionParser<LLIntGenerator> parser(llintGenerator, functionStart, functionLength, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    return llintGenerator.finalize();
}


using Buffer = WasmInstructionStream::InstructionBuffer;
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

LLIntGenerator::LLIntGenerator(ModuleInformation& info, unsigned functionIndex, const TypeDefinition&)
    : BytecodeGeneratorBase(makeUnique<FunctionCodeBlockGenerator>(functionIndex), 0)
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

std::unique_ptr<FunctionCodeBlockGenerator> LLIntGenerator::finalize()
{
    RELEASE_ASSERT(m_codeBlock);

    size_t numCalleeLocals = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_maxStackSize);
    m_codeBlock->m_numCalleeLocals = numCalleeLocals;
    RELEASE_ASSERT(numCalleeLocals == m_codeBlock->m_numCalleeLocals);

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

auto LLIntGenerator::callInformationForCaller(const FunctionSignature& signature) -> LLIntCallInformation
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
    const uint32_t gprCount = callingConvention.jsrArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();

    uint32_t stackCount = 0;
    uint32_t gprIndex = 0;
    uint32_t fprIndex = 0;
    uint32_t stackIndex = 0;

    auto allocateStackRegister = [&](Type type) {
        switch (type.kind) {
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::RefNull:
        case TypeKind::Ref:
            if (gprIndex < gprCount)
                ++gprIndex;
            else if (stackIndex++ >= stackCount)
                ++stackCount;
            break;
        case TypeKind::F32:
        case TypeKind::F64:
        case TypeKind::V128:
            if (fprIndex < fprCount)
                ++fprIndex;
            else if (stackIndex++ >= stackCount)
                ++stackCount;
            break;
        case TypeKind::Void:
        case TypeKind::Func:
        case TypeKind::Struct:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::I31ref:
        case TypeKind::Rec:
        case TypeKind::Sub:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };


    for (uint32_t i = 0; i < signature.argumentCount(); i++)
        allocateStackRegister(signature.argumentType(i));

    gprIndex = 0;
    fprIndex = 0;
    stackIndex = 0;
    for (uint32_t i = 0; i < signature.returnCount(); i++)
        allocateStackRegister(signature.returnType(i));

    // FIXME: we are allocating the extra space for the argument/return count in order to avoid interference, but we could do better
    // NOTE: We increase arg count by 1 for the case of indirect calls
    m_stackSize += std::max(signature.argumentCount() + 1, signature.returnCount()) + gprCount + fprCount + stackCount + CallFrame::headerSizeInRegisters + 1;
    if (m_stackSize.value() % stackAlignmentRegisters())
        ++m_stackSize;
    if (m_maxStackSize < m_stackSize)
        m_maxStackSize = m_stackSize;


    ResultList arguments(signature.argumentCount());
    ResultList temporaryResults(signature.returnCount());

    const unsigned stackOffset = m_stackSize;
    const unsigned base = stackOffset - CallFrame::headerSizeInRegisters - 1;

    const uint32_t gprLimit = base - stackCount - gprCount;
    const uint32_t fprLimit = gprLimit - fprCount;

    stackIndex = base;
    gprIndex = base - stackCount;
    fprIndex = gprIndex - gprCount;
    for (uint32_t i = 0; i < signature.argumentCount(); i++) {
        switch (signature.argumentType(i).kind) {
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::RefNull:
        case TypeKind::Ref:
            if (gprIndex > gprLimit)
                arguments[i] = virtualRegisterForLocal(--gprIndex);
            else
                arguments[i] = virtualRegisterForLocal(--stackIndex);
            break;
        case TypeKind::F32:
        case TypeKind::F64:
        case TypeKind::V128:
            if (fprIndex > fprLimit)
                arguments[i] = virtualRegisterForLocal(--fprIndex);
            else
                arguments[i] = virtualRegisterForLocal(--stackIndex);
            break;
        case TypeKind::Void:
        case TypeKind::Func:
        case TypeKind::Struct:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::I31ref:
        case TypeKind::Rec:
        case TypeKind::Sub:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    stackIndex = base;
    gprIndex = base - stackCount;
    fprIndex = gprIndex - gprCount;
    for (uint32_t i = 0; i < signature.returnCount(); i++) {
        switch (signature.returnType(i).kind) {
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::RefNull:
        case TypeKind::Ref:
            if (gprIndex > gprLimit)
                temporaryResults[i] = virtualRegisterForLocal(--gprIndex);
            else
                temporaryResults[i] = virtualRegisterForLocal(--stackIndex);
            break;
        case TypeKind::F32:
        case TypeKind::F64:
        case TypeKind::V128:
            if (fprIndex > fprLimit)
                temporaryResults[i] = virtualRegisterForLocal(--fprIndex);
            else
                temporaryResults[i] = virtualRegisterForLocal(--stackIndex);
            break;
        case TypeKind::Void:
        case TypeKind::Func:
        case TypeKind::Struct:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::I31ref:
        case TypeKind::Rec:
        case TypeKind::Sub:
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

auto LLIntGenerator::callInformationForCallee(const FunctionSignature& signature) -> Vector<VirtualRegister, 2>
{
    if (m_results.size())
        return m_results;

    m_results.reserveInitialCapacity(signature.returnCount());

    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.jsrArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();

    uint32_t gprIndex = 0;
    uint32_t fprIndex = gprCount;
    uint32_t stackIndex = 1;
    const uint32_t maxGPRIndex = gprCount;
    const uint32_t maxFPRIndex = maxGPRIndex + fprCount;

    for (uint32_t i = 0; i < signature.returnCount(); i++) {
        switch (signature.returnType(i).kind) {
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::RefNull:
        case TypeKind::Ref:
            if (gprIndex < maxGPRIndex)
                m_results.append(virtualRegisterForLocal(numberOfLLIntCalleeSaveRegisters + gprIndex++));
            else
                m_results.append(virtualRegisterForArgumentIncludingThis(stackIndex++));
            break;
        case TypeKind::F32:
        case TypeKind::F64:
        case TypeKind::V128:
            if (fprIndex < maxFPRIndex)
                m_results.append(virtualRegisterForLocal(numberOfLLIntCalleeSaveRegisters + fprIndex++));
            else
                m_results.append(virtualRegisterForArgumentIncludingThis(stackIndex++));
            break;
        case TypeKind::Void:
        case TypeKind::Func:
        case TypeKind::Struct:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::I31ref:
        case TypeKind::Rec:
        case TypeKind::Sub:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    return m_results;
}

auto LLIntGenerator::addArguments(const TypeDefinition& signature) -> PartialResult
{
    checkConsistency();

    m_codeBlock->m_numArguments = signature.as<FunctionSignature>()->argumentCount();
    m_normalizedArguments.resize(m_codeBlock->m_numArguments);

    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.jsrArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();
    const uint32_t maxGPRIndex = gprCount;
    const uint32_t maxFPRIndex = gprCount + fprCount;
    uint32_t gprIndex = 0;
    uint32_t fprIndex = maxGPRIndex;
    uint32_t stackIndex = 1;

    Vector<VirtualRegister> registerArguments(gprCount + fprCount);
    for (uint32_t i = 0; i < gprCount + fprCount; i++)
        registerArguments[i] = push(NoConsistencyCheck);

    const auto addArgument = [&](uint32_t index, uint32_t& count, uint32_t max) {
        if (count < max)
            m_normalizedArguments[index] = registerArguments[count++];
        else
            m_normalizedArguments[index] = virtualRegisterForArgumentIncludingThis(stackIndex++);
    };

    for (uint32_t i = 0; i < signature.as<FunctionSignature>()->argumentCount(); i++) {
        switch (signature.as<FunctionSignature>()->argumentType(i).kind) {
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::Externref:
        case TypeKind::Funcref:
        case TypeKind::RefNull:
        case TypeKind::Ref:
            addArgument(i, gprIndex, maxGPRIndex);
            break;
        case TypeKind::F32:
        case TypeKind::F64:
        case TypeKind::V128:
            addArgument(i, fprIndex, maxFPRIndex);
            break;
        case TypeKind::Void:
        case TypeKind::Func:
        case TypeKind::Struct:
        case TypeKind::Array:
        case TypeKind::Arrayref:
        case TypeKind::I31ref:
        case TypeKind::Rec:
        case TypeKind::Sub:
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
    if (isFuncref(type) || isExternref(type)) {
        while (count--)
            m_unitializedLocals.append(push(NoConsistencyCheck));
    } else
        m_stackSize += count;
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
        if (isRefType(type))
            WasmSetGlobalRef::emit(this, index, value);
        else
            WasmSetGlobal::emit(this, index, value);
        break;
    case Wasm::GlobalInformation::BindingMode::Portable:
        if (isRefType(type))
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
    materializeConstantsAndLocals(newStack, NoConsistencyCheck);
#if ASSERT_ENABLED
    // We cannot yet call checkConsistency, since the arguments we are
    // materializing for the loop are not neither in the expression
    // nor the control stack, and it won't know what to do in this
    // intermediate state. As a sanity check just verify that
    // everything in newStack is a virtual register that is actually
    // pointing to each stack position, which is what we should have
    // after we split the stack and the previous call materializes
    // constants and aliases if needed.
    walkExpressionStack(newStack, [](VirtualRegister expression, VirtualRegister slot) {
        ASSERT(expression == slot);
    });
#endif

    Ref<Label> body = newEmittedLabel();
    Ref<Label> continuation = newLabel();

    block = ControlType::loop(signature, m_stackSize, WTFMove(body), WTFMove(continuation));

    Vector<unsigned> unresolvedOffsets;
    Vector<VirtualRegister> osrEntryData;
    for (uint32_t i = 0; i < m_codeBlock->m_numArguments; i++)
        osrEntryData.append(m_normalizedArguments[i]);

    const auto& callingConvention = wasmCallingConvention();
    const uint32_t gprCount = callingConvention.jsrArgs.size();
    const uint32_t fprCount = callingConvention.fprArgs.size();
    for (uint32_t i = gprCount + fprCount + numberOfLLIntCalleeSaveRegisters; i < m_codeBlock->m_numVars; i++)
        osrEntryData.append(virtualRegisterForLocal(i));
    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        ControlType& data = m_parser->controlStack()[controlIndex].controlData;
        Stack& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        for (TypedExpression expression : expressionStack)
            osrEntryData.append(expression);
        if (ControlType::isAnyCatch(data))
            osrEntryData.append(std::get<ControlCatch>(data).m_exception);
    }
    for (TypedExpression expression : enclosingStack)
        osrEntryData.append(expression);
    for (TypedExpression expression : newStack)
        osrEntryData.append(expression);

    WasmLoopHint::emit(this);

    m_codeBlock->tierUpCounter().add(m_lastInstruction.offset(), LLIntTierUpCounter::OSREntryData { loopIndex, WTFMove(osrEntryData) });

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
    ASSERT(ControlType::isIf(data));
    materializeConstantsAndLocals(expressionStack);
    WasmJmp::emit(this, data.m_continuation->bind(this));
    return addElseToUnreachable(data);
}

auto LLIntGenerator::addElseToUnreachable(ControlType& data) -> PartialResult
{
    m_stackSize = data.stackSize() + data.m_signature->as<FunctionSignature>()->argumentCount();

    ControlIf& control = std::get<ControlIf>(data);
    emitLabel(control.m_alternate.get());
    data = ControlType::block(data.m_signature, m_stackSize, WTFMove(data.m_continuation));
    return { };
}

auto LLIntGenerator::addTry(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    m_usesExceptions = true;
    ++m_tryDepth;

    splitStack(signature, enclosingStack, newStack);
    Ref<Label> tryLabel = newEmittedLabel();
    Ref<Label> continuation = newLabel();
    result = ControlType::createTry(signature, m_stackSize, WTFMove(tryLabel), WTFMove(continuation), m_tryDepth);
    return { };
}

void LLIntGenerator::finalizePreviousBlockForCatch(ControlType& data, Stack& expressionStack)
{
    if (!ControlType::isAnyCatch(data))
        materializeConstantsAndLocals(expressionStack);
    else {
        checkConsistency();
        VirtualRegister dst = virtualRegisterForLocal(data.stackSize());
        for (TypedExpression& value : expressionStack) {
            WasmMov::emit(this, dst, value);
            value = TypedExpression { value.type(), dst };
            dst -= 1;
        }
    }
    WasmJmp::emit(this, data.m_continuation->bind(this));
}

auto LLIntGenerator::addCatch(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, Stack& expressionStack, ControlType& data, ResultList& results) -> PartialResult
{
    finalizePreviousBlockForCatch(data, expressionStack);
    return addCatchToUnreachable(exceptionIndex, exceptionSignature, data, results);
}

auto LLIntGenerator::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& exceptionSignature, ControlType& data, ResultList& results) -> PartialResult
{
    m_usesExceptions = true;
    Ref<Label> catchLabel = newEmittedLabel();

    m_stackSize = data.stackSize();
    VirtualRegister exception = push();
    if (ControlType::isTry(data))
        data.convertToCatch(catchLabel, exception);
    for (unsigned i = 0; i < exceptionSignature.as<FunctionSignature>()->argumentCount(); ++i)
        results.append(push());

    if (Context::useFastTLS())
        WasmCatch::emit(this, exceptionIndex, exception, exceptionSignature.as<FunctionSignature>()->argumentCount(), results.isEmpty() ? 0 : -results[0].offset());
    else
        WasmCatchNoTls::emit(this, exceptionIndex, exception, exceptionSignature.as<FunctionSignature>()->argumentCount(), results.isEmpty() ? 0 : -results[0].offset());

    for (unsigned i = 0; i < exceptionSignature.as<FunctionSignature>()->argumentCount(); ++i) {
        VirtualRegister dst = results[i];
        Type type = exceptionSignature.as<FunctionSignature>()->argumentType(i);
        switch (type.kind) {
        case Wasm::TypeKind::F32:
            WasmF32ReinterpretI32::emit(this, dst, dst);
            break;
        case Wasm::TypeKind::F64:
            WasmF64ReinterpretI64::emit(this, dst, dst);
            break;
        case Wasm::TypeKind::I32:
        case Wasm::TypeKind::I64:
        case Wasm::TypeKind::Externref:
        case Wasm::TypeKind::Funcref:
        case Wasm::TypeKind::V128:
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    ControlCatch& catchData = std::get<ControlCatch>(data);
    catchData.m_kind = CatchKind::Catch;
    m_codeBlock->addExceptionHandler({ HandlerType::Catch, catchData.m_tryStart->location(), catchData.m_tryEnd->location(), catchLabel->location(), m_tryDepth, exceptionIndex });

    return { };
}

auto LLIntGenerator::addCatchAll(Stack& expressionStack, ControlType& data) -> PartialResult
{
    finalizePreviousBlockForCatch(data, expressionStack);
    WasmJmp::emit(this, data.m_continuation->bind(this));
    return addCatchAllToUnreachable(data);
}

auto LLIntGenerator::addCatchAllToUnreachable(ControlType& data) -> PartialResult
{
    m_usesExceptions = true;
    Ref<Label> catchLabel = newEmittedLabel();
    m_stackSize = data.stackSize();
    VirtualRegister exception = push();
    if (ControlType::isTry(data))
        data.convertToCatch(catchLabel, exception);
    ControlCatch& catchData = std::get<ControlCatch>(data);
    catchData.m_kind = CatchKind::CatchAll;

    if (Context::useFastTLS())
        WasmCatchAll::emit(this, exception);
    else
        WasmCatchAllNoTls::emit(this, exception);

    m_codeBlock->addExceptionHandler({ HandlerType::CatchAll, catchData.m_tryStart->location(), catchData.m_tryEnd->location(), catchLabel->location(), m_tryDepth, 0 });
    return { };
}

auto LLIntGenerator::addDelegate(ControlType& target, ControlType& data) -> PartialResult
{
    return addDelegateToUnreachable(target, data);
}

auto LLIntGenerator::addDelegateToUnreachable(ControlType& target, ControlType& data) -> PartialResult
{
    m_usesExceptions = true;
    Ref<Label> delegateLabel = newEmittedLabel();

    ASSERT(ControlType::isTry(target) || ControlType::isTopLevel(target));
    unsigned targetDepth = ControlType::isTry(target) ? std::get<ControlTry>(target).m_tryDepth : 0;

    ControlTry& tryData = std::get<ControlTry>(data);
    m_codeBlock->addExceptionHandler({ HandlerType::Delegate, tryData.m_try->location(), delegateLabel->location(), 0, m_tryDepth, targetDepth });
    return { };
}

auto LLIntGenerator::addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&) -> PartialResult
{
    m_usesExceptions = true;
    // We have to materialize the arguments here since it might include constants or
    // delayed moves, but the wasm_throw opcode expects all the arguments to be contiguous
    // in the stack. The reason we don't call materializeConstantsAndLocals here is that
    // it expects a stack, not a vector of ExpressionType arguments.
    walkExpressionStack(args, [&](VirtualRegister& arg, VirtualRegister slot) {
        if (arg == slot)
            return;
        WasmMov::emit(this, slot, arg);
        arg = slot;
    });
    WasmThrow::emit(this, exceptionIndex, args.isEmpty() ? virtualRegisterForLocal(0) : args[0]);
    return { };
}

auto LLIntGenerator::addRethrow(unsigned, ControlType& data) -> PartialResult
{
    m_usesExceptions = true;
    ASSERT(ControlType::isAnyCatch(data));
    ControlCatch catchData = std::get<ControlCatch>(data);
    WasmRethrow::emit(this, catchData.m_exception);
    return { };
}

auto LLIntGenerator::addReturn(const ControlType& data, Stack& returnValues) -> PartialResult
{
    if (!data.m_signature->as<FunctionSignature>()->returnCount()) {
        WasmRetVoid::emit(this);
        return { };
    }

    // no need to drop keep here, since we have to move anyway
    unifyValuesWithBlock(callInformationForCallee(*data.m_signature->as<FunctionSignature>()), returnValues);
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
    auto& jumpTable = m_codeBlock->addJumpTable(targets.size() + 1);

    WasmSwitch::emit(this, condition, tableIndex);

    unsigned index = 0;
    WasmInstructionStream::Offset offset = m_lastInstruction.offset();

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
    finalizePreviousBlockForCatch(entry.controlData, expressionStack);
    return addEndToUnreachable(entry, expressionStack, false);
}


auto LLIntGenerator::addEndToUnreachable(ControlEntry& entry, Stack& expressionStack, bool unreachable) -> PartialResult
{
    ControlType& data = entry.controlData;

    unsigned stackSize = data.stackSize();
    if (ControlType::isAnyCatch(entry.controlData))
        ++stackSize; // Account for the caught exception
    RELEASE_ASSERT(unreachable || m_stackSize ==  stackSize + data.m_signature->as<FunctionSignature>()->returnCount());

    m_stackSize = data.stackSize();

    if (ControlType::isTry(data) || ControlType::isAnyCatch(data))
        --m_tryDepth;

    for (unsigned i = 0; i < data.m_signature->as<FunctionSignature>()->returnCount(); ++i) {
        // We don't want to do a consistency check here because we just reset the stack size
        // are pushing new values, while we already have the same values in the stack.
        // The only reason we do things this way is so that it also works for unreachable blocks,
        // since they might not have the right number of values in the expression stack.
        // Instead, we do a stricter consistency check below.
        auto tmp = push(NoConsistencyCheck);
        ASSERT(unreachable || tmp == expressionStack[i].value());
        if (unreachable)
            entry.enclosedExpressionStack.constructAndAppend(data.m_signature->as<FunctionSignature>()->returnType(i), tmp);
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
    RELEASE_ASSERT(expressionStack.size() == signature->as<FunctionSignature>()->returnCount());
    if (m_usesSIMD)
        m_info.addSIMDFunction(m_functionIndex);
    m_info.doneSeeingFunction(m_functionIndex);

    if (!signature->as<FunctionSignature>()->returnCount()) {
        WasmRetVoid::emit(this);
        return { };
    }

    checkConsistency();
    unifyValuesWithBlock(callInformationForCallee(*signature->as<FunctionSignature>()), expressionStack);
    WasmRet::emit(this);

    return { };
}

auto LLIntGenerator::addCall(uint32_t functionIndex, const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());
    LLIntCallInformation info = callInformationForCaller(*signature.as<FunctionSignature>());
    unifyValuesWithBlock(info.arguments, args);
    if (Context::useFastTLS())
        WasmCall::emit(this, functionIndex, info.stackOffset, info.numberOfStackArguments);
    else
        WasmCallNoTls::emit(this, functionIndex, info.stackOffset, info.numberOfStackArguments);
    info.commitResults(results);

    return { };
}

auto LLIntGenerator::addCallIndirect(unsigned tableIndex, const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();

    const auto& functionSignature = *signature.expand().as<FunctionSignature>();
    ASSERT(functionSignature.argumentCount() == args.size());
    ASSERT(m_info.tableCount() > tableIndex);
    ASSERT(m_info.tables[tableIndex].type() == TableElementType::Funcref);

    LLIntCallInformation info = callInformationForCaller(functionSignature);
    unifyValuesWithBlock(info.arguments, args);
    if (Context::useFastTLS())
        WasmCallIndirect::emit(this, calleeIndex, m_codeBlock->addSignature(signature), info.stackOffset, info.numberOfStackArguments, tableIndex);
    else
        WasmCallIndirectNoTls::emit(this, calleeIndex, m_codeBlock->addSignature(signature), info.stackOffset, info.numberOfStackArguments, tableIndex);
    info.commitResults(results);

    return { };
}

auto LLIntGenerator::addCallRef(const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ExpressionType callee = args.takeLast();

    const auto& functionSignature = *signature.expand().as<FunctionSignature>();
    LLIntCallInformation info = callInformationForCaller(functionSignature);
    unifyValuesWithBlock(info.arguments, args);
    if (Context::useFastTLS())
        WasmCallRef::emit(this, callee, m_codeBlock->addSignature(signature), info.stackOffset, info.numberOfStackArguments);
    else
        WasmCallRefNoTls::emit(this, callee, m_codeBlock->addSignature(signature), info.stackOffset, info.numberOfStackArguments);
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

auto LLIntGenerator::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    WasmTableInit::emit(this, dstOffset, srcOffset, length, elementIndex, tableIndex);

    return { };
}

auto LLIntGenerator::addElemDrop(unsigned elementIndex) -> PartialResult
{
    WasmElemDrop::emit(this, elementIndex);

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

auto LLIntGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    WasmTableCopy::emit(this, dstOffset, srcOffset, length, dstTableIndex, srcTableIndex);
    return { };
}

auto LLIntGenerator::addUnreachable() -> PartialResult
{
    WasmUnreachable::emit(this);

    return { };
}

auto LLIntGenerator::addCrash() -> PartialResult
{
    WasmCrash::emit(this);

    return { };
}

auto LLIntGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    result = push();
    WasmCurrentMemory::emit(this, result);

    return { };
}

auto LLIntGenerator::addMemoryInit(unsigned dataSegmentIndex, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length) -> PartialResult
{
    WasmMemoryInit::emit(this, dstAddress, srcAddress, length, dataSegmentIndex);

    return { };
}

auto LLIntGenerator::addDataDrop(unsigned dataSegmentIndex) -> PartialResult
{
    WasmDataDrop::emit(this, dataSegmentIndex);

    return { };
}

auto LLIntGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmGrowMemory::emit(this, result, delta);

    return { };
}

auto LLIntGenerator::addMemoryFill(ExpressionType dstAddress, ExpressionType targetValue, ExpressionType count) -> PartialResult
{
    WasmMemoryFill::emit(this, dstAddress, targetValue, count);
    return { };
}

auto LLIntGenerator::addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count) -> PartialResult
{
    WasmMemoryCopy::emit(this, dstAddress, srcAddress, count);
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

auto LLIntGenerator::atomicLoad(ExtAtomicOpType op, Type, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = push();
    switch (op) {
    case ExtAtomicOpType::I32AtomicLoad8U:
    case ExtAtomicOpType::I64AtomicLoad8U:
        WasmI64AtomicRmw8AddU::emit(this, result, pointer, offset, zeroConstant());
        break;
    case ExtAtomicOpType::I32AtomicLoad16U:
    case ExtAtomicOpType::I64AtomicLoad16U:
        WasmI64AtomicRmw16AddU::emit(this, result, pointer, offset, zeroConstant());
        break;
    case ExtAtomicOpType::I32AtomicLoad:
    case ExtAtomicOpType::I64AtomicLoad32U:
        WasmI64AtomicRmw32AddU::emit(this, result, pointer, offset, zeroConstant());
        break;
    case ExtAtomicOpType::I64AtomicLoad:
        WasmI64AtomicRmwAdd::emit(this, result, pointer, offset, zeroConstant());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return { };
}

auto LLIntGenerator::atomicStore(ExtAtomicOpType op, Type, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    auto result = push();
    switch (op) {
    case ExtAtomicOpType::I32AtomicStore8U:
    case ExtAtomicOpType::I64AtomicStore8U:
        WasmI64AtomicRmw8XchgU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicStore16U:
    case ExtAtomicOpType::I64AtomicStore16U:
        WasmI64AtomicRmw16XchgU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicStore:
    case ExtAtomicOpType::I64AtomicStore32U:
        WasmI64AtomicRmw32XchgU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I64AtomicStore:
        WasmI64AtomicRmwXchg::emit(this, result, pointer, offset, value);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    didPopValueFromStack(); // Ignore the result.
    return { };
}

auto LLIntGenerator::atomicBinaryRMW(ExtAtomicOpType op, Type, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = push();
    switch (op) {
    case ExtAtomicOpType::I32AtomicRmw8AddU:
    case ExtAtomicOpType::I64AtomicRmw8AddU:
        WasmI64AtomicRmw8AddU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw16AddU:
    case ExtAtomicOpType::I64AtomicRmw16AddU:
        WasmI64AtomicRmw16AddU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmwAdd:
    case ExtAtomicOpType::I64AtomicRmw32AddU:
        WasmI64AtomicRmw32AddU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I64AtomicRmwAdd:
        WasmI64AtomicRmwAdd::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw8SubU:
    case ExtAtomicOpType::I64AtomicRmw8SubU:
        WasmI64AtomicRmw8SubU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw16SubU:
    case ExtAtomicOpType::I64AtomicRmw16SubU:
        WasmI64AtomicRmw16SubU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmwSub:
    case ExtAtomicOpType::I64AtomicRmw32SubU:
        WasmI64AtomicRmw32SubU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I64AtomicRmwSub:
        WasmI64AtomicRmwSub::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw8AndU:
    case ExtAtomicOpType::I64AtomicRmw8AndU:
        WasmI64AtomicRmw8AndU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw16AndU:
    case ExtAtomicOpType::I64AtomicRmw16AndU:
        WasmI64AtomicRmw16AndU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmwAnd:
    case ExtAtomicOpType::I64AtomicRmw32AndU:
        WasmI64AtomicRmw32AndU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I64AtomicRmwAnd:
        WasmI64AtomicRmwAnd::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw8OrU:
    case ExtAtomicOpType::I64AtomicRmw8OrU:
        WasmI64AtomicRmw8OrU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw16OrU:
    case ExtAtomicOpType::I64AtomicRmw16OrU:
        WasmI64AtomicRmw16OrU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmwOr:
    case ExtAtomicOpType::I64AtomicRmw32OrU:
        WasmI64AtomicRmw32OrU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I64AtomicRmwOr:
        WasmI64AtomicRmwOr::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw8XorU:
    case ExtAtomicOpType::I64AtomicRmw8XorU:
        WasmI64AtomicRmw8XorU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw16XorU:
    case ExtAtomicOpType::I64AtomicRmw16XorU:
        WasmI64AtomicRmw16XorU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmwXor:
    case ExtAtomicOpType::I64AtomicRmw32XorU:
        WasmI64AtomicRmw32XorU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I64AtomicRmwXor:
        WasmI64AtomicRmwXor::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw8XchgU:
    case ExtAtomicOpType::I64AtomicRmw8XchgU:
        WasmI64AtomicRmw8XchgU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw16XchgU:
    case ExtAtomicOpType::I64AtomicRmw16XchgU:
        WasmI64AtomicRmw16XchgU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I32AtomicRmwXchg:
    case ExtAtomicOpType::I64AtomicRmw32XchgU:
        WasmI64AtomicRmw32XchgU::emit(this, result, pointer, offset, value);
        break;
    case ExtAtomicOpType::I64AtomicRmwXchg:
        WasmI64AtomicRmwXchg::emit(this, result, pointer, offset, value);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return { };
}

auto LLIntGenerator::atomicCompareExchange(ExtAtomicOpType op, Type, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = push();
    switch (op) {
    case ExtAtomicOpType::I32AtomicRmw8CmpxchgU:
    case ExtAtomicOpType::I64AtomicRmw8CmpxchgU:
        WasmI64AtomicRmw8CmpxchgU::emit(this, result, pointer, offset, expected, value);
        break;
    case ExtAtomicOpType::I32AtomicRmw16CmpxchgU:
    case ExtAtomicOpType::I64AtomicRmw16CmpxchgU:
        WasmI64AtomicRmw16CmpxchgU::emit(this, result, pointer, offset, expected, value);
        break;
    case ExtAtomicOpType::I32AtomicRmwCmpxchg:
    case ExtAtomicOpType::I64AtomicRmw32CmpxchgU:
        WasmI64AtomicRmw32CmpxchgU::emit(this, result, pointer, offset, expected, value);
        break;
    case ExtAtomicOpType::I64AtomicRmwCmpxchg:
        WasmI64AtomicRmwCmpxchg::emit(this, result, pointer, offset, expected, value);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return { };
}

auto LLIntGenerator::atomicWait(ExtAtomicOpType op, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = push();
    switch (op) {
    case ExtAtomicOpType::MemoryAtomicWait32:
        WasmMemoryAtomicWait32::emit(this, result, pointer, offset, value, timeout);
        break;
    case ExtAtomicOpType::MemoryAtomicWait64:
        WasmMemoryAtomicWait64::emit(this, result, pointer, offset, value, timeout);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

auto LLIntGenerator::atomicNotify(ExtAtomicOpType op, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = push();
    RELEASE_ASSERT(op == ExtAtomicOpType::MemoryAtomicNotify);
    WasmMemoryAtomicNotify::emit(this, result, pointer, offset, count);
    return { };
}

auto LLIntGenerator::atomicFence(ExtAtomicOpType, uint8_t) -> PartialResult
{
    WasmAtomicFence::emit(this);
    return { };
}

auto LLIntGenerator::truncSaturated(Ext1OpType op, ExpressionType operand, ExpressionType& result, Type, Type) -> PartialResult
{
    result = push();
    switch (op) {
    case Ext1OpType::I32TruncSatF32S:
        WasmI32TruncSatF32S::emit(this, result, operand);
        break;
    case Ext1OpType::I32TruncSatF32U:
        WasmI32TruncSatF32U::emit(this, result, operand);
        break;
    case Ext1OpType::I32TruncSatF64S:
        WasmI32TruncSatF64S::emit(this, result, operand);
        break;
    case Ext1OpType::I32TruncSatF64U:
        WasmI32TruncSatF64U::emit(this, result, operand);
        break;
    case Ext1OpType::I64TruncSatF32S:
        WasmI64TruncSatF32S::emit(this, result, operand);
        break;
    case Ext1OpType::I64TruncSatF32U:
        WasmI64TruncSatF32U::emit(this, result, operand);
        break;
    case Ext1OpType::I64TruncSatF64S:
        WasmI64TruncSatF64S::emit(this, result, operand);
        break;
    case Ext1OpType::I64TruncSatF64U:
        WasmI64TruncSatF64U::emit(this, result, operand);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

auto LLIntGenerator::addI31New(ExpressionType value, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmI31New::emit(this, result, value);

    return { };
}

auto LLIntGenerator::addI31GetS(ExpressionType ref, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmI31GetS::emit(this, result, ref);

    return { };
}

auto LLIntGenerator::addI31GetU(ExpressionType ref, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmI31GetU::emit(this, result, ref);

    return { };
}

auto LLIntGenerator::addArrayNew(uint32_t index, ExpressionType size, ExpressionType value, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmArrayNew::emit(this, result, size, value, index);

    return { };
}

auto LLIntGenerator::addArrayNewDefault(uint32_t index, ExpressionType size, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmArrayNewDefault::emit(this, result, size, index);

    return { };
}

auto LLIntGenerator::addArrayGet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmArrayGet::emit(this, result, arrayref, index, typeIndex);

    return { };
}

auto LLIntGenerator::addArraySet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType value) -> PartialResult
{
    WasmArraySet::emit(this, arrayref, index, value, typeIndex);

    return { };
}

auto LLIntGenerator::addArrayLen(ExpressionType arrayref, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmArrayLen::emit(this, result, arrayref);

    return { };
}

auto LLIntGenerator::addStructNew(uint32_t index, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    result = push();

    // We have to materialize the arguments here since it might include constants or
    // delayed moves, but the wasm_struct_new opcode expects all the arguments to be contiguous
    // in the stack.
    for (unsigned i = args.size(); i > 0; --i) {
        auto& arg = args[i - 1];
        ExpressionType argLoc = push();
        WasmMov::emit(this, argLoc, arg);
        arg = argLoc;
    }

    WasmStructNew::emit(this, result, index, args.isEmpty() ? VirtualRegister() : args[0]);

    m_stackSize -= args.size();
    return { };
}

auto LLIntGenerator::addStructGet(ExpressionType structReference, const StructType&, uint32_t fieldIndex, ExpressionType& result) -> PartialResult
{
    result = push();
    WasmStructGet::emit(this, result, structReference, fieldIndex);

    return { };
}

auto LLIntGenerator::addStructSet(ExpressionType structReference, const StructType&, uint32_t fieldIndex, ExpressionType value) -> PartialResult
{
    WasmStructSet::emit(this, structReference, fieldIndex, value);

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

static void dumpExpressionStack(const CommaPrinter& comma, const LLIntGenerator::Stack& expressionStack)
{
    dataLog(comma, "ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, expression.value());
}

void LLIntGenerator::dump(const ControlStack& controlStack, const Stack* stack)
{
    dataLogLn("Control stack: stackSize:(", m_stackSize.value(), ")");
    for (size_t i = controlStack.size(); i--;) {
        dataLog("  ", controlStack[i].controlData, ": ");
        CommaPrinter comma(", ", "");
        dumpExpressionStack(comma, *stack);
        stack = &controlStack[i].enclosedExpressionStack;
        dataLogLn();
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
        instruction->cast<__op>()->setTargetLabel(BoundLabel(target), [&]() { \
            generator.m_codeBlock->addOutOfLineJumpTarget(instruction.offset(), target); \
            return BoundLabel(); \
        }); \
        break;

        switch (instruction->opcodeID()) {
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
