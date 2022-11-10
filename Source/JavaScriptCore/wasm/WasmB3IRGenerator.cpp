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

#include "config.h"
#include "WasmB3IRGenerator.h"

#if ENABLE(WEBASSEMBLY_B3JIT)

#include "AirCode.h"
#include "AllowMacroScratchRegisterUsageIf.h"
#include "B3BasicBlockInlines.h"
#include "B3CCallValue.h"
#include "B3ConstPtrValue.h"
#include "B3EstimateStaticExecutionCounts.h"
#include "B3FixSSA.h"
#include "B3Generate.h"
#include "B3InsertionSet.h"
#include "B3StackmapGenerationParams.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3Validate.h"
#include "B3ValueInlines.h"
#include "B3ValueKey.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "B3WasmAddressValue.h"
#include "B3WasmBoundsCheckValue.h"
#include "FunctionAllowlist.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyStruct.h"
#include "ProbeContext.h"
#include "ScratchRegisterAllocator.h"
#include "WasmBranchHints.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmExceptionType.h"
#include "WasmFunctionParser.h"
#include "WasmIRGeneratorHelpers.h"
#include "WasmInstance.h"
#include "WasmMemory.h"
#include "WasmOSREntryData.h"
#include "WasmOpcodeOrigin.h"
#include "WasmOperations.h"
#include "WasmThunks.h"
#include "WasmTypeDefinitionInlines.h"
#include <limits>
#include <wtf/StdLibExtras.h>

#if !ENABLE(WEBASSEMBLY)
#error ENABLE(WEBASSEMBLY_B3JIT) is enabled, but ENABLE(WEBASSEMBLY) is not.
#endif

void dumpProcedure(void* ptr)
{
    JSC::B3::Procedure* proc = static_cast<JSC::B3::Procedure*>(ptr);
    proc->dump(WTF::dataFile());
}

namespace JSC { namespace Wasm {

using namespace B3;

namespace {
namespace WasmB3IRGeneratorInternal {
static constexpr bool verbose = false;
}
}

class B3IRGenerator {
public:
    using ExpressionType = Variable*;
    using ResultList = Vector<ExpressionType, 8>;

    static constexpr bool tierSupportsSIMD = false;

    struct ControlData {
        ControlData(Procedure& proc, Origin origin, BlockSignature signature, BlockType type, unsigned stackSize, BasicBlock* continuation, BasicBlock* special = nullptr)
            : controlBlockType(type)
            , m_signature(signature)
            , m_stackSize(stackSize)
            , continuation(continuation)
            , special(special)
        {
            ASSERT(type != BlockType::Try && type != BlockType::Catch);
            if (type != BlockType::TopLevel)
                m_stackSize -= signature->as<FunctionSignature>()->argumentCount();

            if (type == BlockType::Loop) {
                for (unsigned i = 0; i < signature->as<FunctionSignature>()->argumentCount(); ++i)
                    phis.append(proc.add<Value>(Phi, toB3Type(signature->as<FunctionSignature>()->argumentType(i)), origin));
            } else {
                for (unsigned i = 0; i < signature->as<FunctionSignature>()->returnCount(); ++i)
                    phis.append(proc.add<Value>(Phi, toB3Type(signature->as<FunctionSignature>()->returnType(i)), origin));
            }
        }

        ControlData(Procedure& proc, Origin origin, BlockSignature signature, BlockType type, unsigned stackSize, BasicBlock* continuation, unsigned tryStart, unsigned tryDepth)
            : controlBlockType(type)
            , m_signature(signature)
            , m_stackSize(stackSize)
            , continuation(continuation)
            , special(nullptr)
            , m_tryStart(tryStart)
            , m_tryCatchDepth(tryDepth)
        {
            for (unsigned i = 0; i < signature->as<FunctionSignature>()->returnCount(); ++i)
                phis.append(proc.add<Value>(Phi, toB3Type(signature->as<FunctionSignature>()->returnType(i)), origin));
        }

        ControlData()
        {
        }

        static bool isIf(const ControlData& control) { return control.blockType() == BlockType::If; }
        static bool isTry(const ControlData& control) { return control.blockType() == BlockType::Try; }
        static bool isAnyCatch(const ControlData& control) { return control.blockType() == BlockType::Catch; }
        static bool isTopLevel(const ControlData& control) { return control.blockType() == BlockType::TopLevel; }
        static bool isLoop(const ControlData& control) { return control.blockType() == BlockType::Loop; }
        static bool isBlock(const ControlData& control) { return control.blockType() == BlockType::Block; }
        static bool isCatch(const ControlData& control)
        {
            if (control.blockType() != BlockType::Catch)
                return false;
            return control.catchKind() == CatchKind::Catch;
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
            case BlockType::Try:
                out.print("Try: ");
                break;
            case BlockType::Catch:
                out.print("Catch: ");
                break;
            }
            out.print("Continuation: ", *continuation, ", Special: ");
            if (special)
                out.print(*special);
            else
                out.print("None");
        }

        BlockType blockType() const { return controlBlockType; }

        BlockSignature signature() const { return m_signature; }

        bool hasNonVoidresult() const { return m_signature->as<FunctionSignature>()->returnsVoid(); }

        BasicBlock* targetBlockForBranch()
        {
            if (blockType() == BlockType::Loop)
                return special;
            return continuation;
        }

        void convertIfToBlock()
        {
            ASSERT(blockType() == BlockType::If);
            controlBlockType = BlockType::Block;
            special = nullptr;
        }

        void convertTryToCatch(unsigned tryEndCallSiteIndex, Variable* exception)
        {
            ASSERT(blockType() == BlockType::Try);
            controlBlockType = BlockType::Catch;
            m_catchKind = CatchKind::Catch;
            m_tryEnd = tryEndCallSiteIndex;
            m_exception = exception;
        }

        void convertTryToCatchAll(unsigned tryEndCallSiteIndex, Variable* exception)
        {
            ASSERT(blockType() == BlockType::Try);
            controlBlockType = BlockType::Catch;
            m_catchKind = CatchKind::CatchAll;
            m_tryEnd = tryEndCallSiteIndex;
            m_exception = exception;
        }

        FunctionArgCount branchTargetArity() const
        {
            if (blockType() == BlockType::Loop)
                return m_signature->as<FunctionSignature>()->argumentCount();
            return m_signature->as<FunctionSignature>()->returnCount();
        }

        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            if (blockType() == BlockType::Loop)
                return m_signature->as<FunctionSignature>()->argumentType(i);
            return m_signature->as<FunctionSignature>()->returnType(i);
        }

        unsigned tryStart() const
        {
            ASSERT(controlBlockType == BlockType::Try || controlBlockType == BlockType::Catch);
            return m_tryStart;
        }

        unsigned tryEnd() const
        {
            ASSERT(controlBlockType == BlockType::Catch);
            return m_tryEnd;
        }

        unsigned tryDepth() const
        {
            ASSERT(controlBlockType == BlockType::Try || controlBlockType == BlockType::Catch);
            return m_tryCatchDepth;
        }

        CatchKind catchKind() const
        {
            ASSERT(controlBlockType == BlockType::Catch);
            return m_catchKind;
        }

        Variable* exception() const
        {
            ASSERT(controlBlockType == BlockType::Catch);
            return m_exception;
        }

        unsigned stackSize() const { return m_stackSize; }

    private:
        // FIXME: Compress B3IRGenerator::ControlData fields using an union
        // https://bugs.webkit.org/show_bug.cgi?id=231212
        friend class B3IRGenerator;
        BlockType controlBlockType;
        BlockSignature m_signature;
        unsigned m_stackSize;
        BasicBlock* continuation;
        BasicBlock* special;
        Vector<Value*> phis;
        unsigned m_tryStart;
        unsigned m_tryEnd;
        unsigned m_tryCatchDepth;
        CatchKind m_catchKind;
        Variable* m_exception;
    };

    using ControlType = ControlData;
    using ExpressionList = Vector<ExpressionType, 1>;

    using ControlEntry = FunctionParser<B3IRGenerator>::ControlEntry;
    using ControlStack = FunctionParser<B3IRGenerator>::ControlStack;
    using Stack = FunctionParser<B3IRGenerator>::Stack;
    using TypedExpression = FunctionParser<B3IRGenerator>::TypedExpression;

    static_assert(std::is_same_v<ResultList, FunctionParser<B3IRGenerator>::ResultList>);

    typedef String ErrorType;
    typedef Unexpected<ErrorType> UnexpectedResult;
    typedef Expected<std::unique_ptr<InternalFunction>, ErrorType> Result;
    typedef Expected<void, ErrorType> PartialResult;

    static ExpressionType emptyExpression() { return nullptr; };

    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString("WebAssembly.Module failed compiling: "_s, makeString(args)...));
    }
#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (UNLIKELY(condition))                  \
            return fail(__VA_ARGS__);             \
    } while (0)

    B3IRGenerator(const ModuleInformation&, Procedure&, InternalFunction*, Vector<UnlinkedWasmToWasmCall>&, unsigned& osrEntryScratchBufferSize, MemoryMode, CompilationMode, unsigned functionIndex, std::optional<bool> hasExceptionHandlers, unsigned loopIndexForOSREntry, TierUpCount*);

    PartialResult WARN_UNUSED_RETURN addArguments(const TypeDefinition&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

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
    PartialResult WARN_UNUSED_RETURN truncSaturated(Ext1OpType, ExpressionType operand, ExpressionType& result, Type returnType, Type operandType);

    // GC
    PartialResult WARN_UNUSED_RETURN addI31New(ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI31GetS(ExpressionType ref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addI31GetU(ExpressionType ref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNew(uint32_t index, ExpressionType size, ExpressionType value, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayNewDefault(uint32_t index, ExpressionType size, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArrayGet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addArraySet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType value);
    PartialResult WARN_UNUSED_RETURN addArrayLen(ExpressionType arrayref, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructNew(uint32_t typeIndex, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructGet(ExpressionType structReference, const StructType&, uint32_t fieldIndex, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addStructSet(ExpressionType structReference, const StructType&, uint32_t fieldIndex, ExpressionType value);

    // Basic operators
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType arg, ExpressionType& result);
    template<OpType>
    PartialResult WARN_UNUSED_RETURN addOp(ExpressionType left, ExpressionType right, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result);

    // Control flow
    ControlData WARN_UNUSED_RETURN addTopLevel(BlockSignature);
    PartialResult WARN_UNUSED_RETURN addBlock(BlockSignature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addLoop(BlockSignature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex);
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType condition, BlockSignature, Stack& enclosingStack, ControlType& result, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addElse(ControlData&, const Stack&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlData&);

    PartialResult WARN_UNUSED_RETURN addTry(BlockSignature, Stack& enclosingStack, ControlType& result, Stack& newStack);
    PartialResult WARN_UNUSED_RETURN addCatch(unsigned exceptionIndex, const TypeDefinition&, Stack&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition&, ControlType&, ResultList&);
    PartialResult WARN_UNUSED_RETURN addCatchAll(Stack&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addCatchAllToUnreachable(ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegate(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addDelegateToUnreachable(ControlType&, ControlType&);
    PartialResult WARN_UNUSED_RETURN addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&);
    PartialResult WARN_UNUSED_RETURN addRethrow(unsigned, ControlType&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, const Stack& = { });

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&) { return { }; }

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallRef(const TypeDefinition&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN emitIndirectCall(Value* calleeInstance, Value* calleeCode, const TypeDefinition&, Vector<ExpressionType>& args, ResultList&);
    B3::Value* createCallPatchpoint(BasicBlock*, Origin, const TypeDefinition&, Vector<ExpressionType>& args, const ScopedLambda<void(PatchpointValue*, Box<PatchpointExceptionHandle>)>& patchpointFunctor);

    void dump(const ControlStack&, const Stack* expressionStack);
    void setParser(FunctionParser<B3IRGenerator>* parser) { m_parser = parser; };
    void didFinishParsingLocals() { }
    void didPopValueFromStack() { --m_stackSize; }

    Value* constant(B3::Type, uint64_t bits, std::optional<Origin> = std::nullopt);
    Value* framePointer();
    void insertEntrySwitch();
    void insertConstants();

    B3::Type toB3ResultType(BlockSignature);

    void addStackMap(unsigned callSiteIndex, StackMap&& stackmap)
    {
        m_stackmaps.add(CallSiteIndex(callSiteIndex), WTFMove(stackmap));
    }

    StackMaps&& takeStackmaps()
    {
        return WTFMove(m_stackmaps);
    }

    Vector<UnlinkedHandlerInfo>&& takeExceptionHandlers()
    {
        return WTFMove(m_exceptionHandlers);
    }

private:
    void emitExceptionCheck(CCallHelpers&, ExceptionType);

    void emitEntryTierUpCheck();
    void emitLoopTierUpCheck(uint32_t loopIndex, const Stack& enclosingStack, const Stack& newStack);

    void emitWriteBarrierForJSWrapper();
    void emitWriteBarrier(Value* cell, Value* instanceCell);
    Value* emitCheckAndPreparePointer(Value* pointer, uint32_t offset, uint32_t sizeOfOp);
    B3::Kind memoryKind(B3::Opcode memoryOp);
    Value* emitLoadOp(LoadOpType, Value* pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, Value* pointer, Value*, uint32_t offset);

    Value* sanitizeAtomicResult(ExtAtomicOpType, Type, Value* result);
    Value* emitAtomicLoadOp(ExtAtomicOpType, Type, Value* pointer, uint32_t offset);
    void emitAtomicStoreOp(ExtAtomicOpType, Type, Value* pointer, Value*, uint32_t offset);
    Value* emitAtomicBinaryRMWOp(ExtAtomicOpType, Type, Value* pointer, Value*, uint32_t offset);
    Value* emitAtomicCompareExchange(ExtAtomicOpType, Type, Value* pointer, Value* expected, Value*, uint32_t offset);

    void emitStructSet(Value*, uint32_t, const StructType&, ExpressionType&);

    void unify(Value* phi, const ExpressionType source);
    void unifyValuesWithBlock(const Stack& resultStack, const ControlData& block);

    void emitChecksForModOrDiv(B3::Opcode, Value* left, Value* right);

    int32_t WARN_UNUSED_RETURN fixupPointerPlusOffset(Value*&, uint32_t);
    Value* WARN_UNUSED_RETURN fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType, Value*, uint32_t);

    void restoreWasmContextInstance(Procedure&, BasicBlock*, Value*);
    enum class RestoreCachedStackLimit { No, Yes };
    void restoreWebAssemblyGlobalState(RestoreCachedStackLimit, const MemoryInformation&, Value* instance, Procedure&, BasicBlock*, bool restoreInstance = true);

    Value* loadFromScratchBuffer(unsigned& indexInBuffer, Value* pointer, B3::Type);
    void connectControlAtEntrypoint(unsigned& indexInBuffer, Value* pointer, ControlData&, Stack& expressionStack, ControlData& currentData, bool fillLoopPhis = false);
    Value* emitCatchImpl(CatchKind, ControlType&, unsigned exceptionIndex = 0);
    PatchpointExceptionHandle preparePatchpointForExceptions(BasicBlock*, PatchpointValue*);

    Origin origin();

    uint32_t outerLoopIndex() const
    {
        if (m_outerLoops.isEmpty())
            return UINT32_MAX;
        return m_outerLoops.last();
    }

    ExpressionType push(Type type)
    {
        return push(toB3Type(type));
    }

    ExpressionType push(B3::Type type)
    {
        ++m_stackSize;
        if (m_stackSize > m_maxStackSize) {
            m_maxStackSize = m_stackSize;
            Variable* var = m_proc.addVariable(type);
            m_stack.append(var);
            return var;
        }

        Variable* var = m_stack[m_stackSize - 1];
        if (var->type() == type)
            return var;

        var = m_proc.addVariable(type);
        m_stack[m_stackSize - 1] = var;
        return var;
    }

    ExpressionType push(Value* value)
    {
        Variable* var = push(value->type());
        set(var, value);
        return var;
    }

    Value* get(BasicBlock* block, Variable* variable)
    {
        return block->appendNew<VariableValue>(m_proc, B3::Get, origin(), variable);
    }

    Value* get(Variable* variable)
    {
        return get(m_currentBlock, variable);
    }

    Value* set(BasicBlock* block, Variable* dst, Value* src)
    {
        return block->appendNew<VariableValue>(m_proc, B3::Set, origin(), dst, src);
    }

    Value* set(Variable* dst, Value* src)
    {
        return set(m_currentBlock, dst, src);
    }

    Value* set(Variable* dst, Variable* src)
    {
        return set(dst, get(src));
    }

    bool useSignalingMemory() const
    {
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
        return m_mode == MemoryMode::Signaling;
#else
        return false;
#endif
    }

    FunctionParser<B3IRGenerator>* m_parser { nullptr };
    const ModuleInformation& m_info;
    const MemoryMode m_mode { MemoryMode::BoundsChecking };
    const CompilationMode m_compilationMode;
    const unsigned m_functionIndex { UINT_MAX };
    const unsigned m_loopIndexForOSREntry { UINT_MAX };
    TierUpCount* m_tierUp { nullptr };

    Procedure& m_proc;
    Vector<BasicBlock*> m_rootBlocks;
    BasicBlock* m_topLevelBlock;
    BasicBlock* m_currentBlock { nullptr };
    Vector<uint32_t> m_outerLoops;
    Vector<Variable*> m_locals;
    Vector<Variable*> m_stack;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.
    unsigned& m_osrEntryScratchBufferSize;
    HashMap<ValueKey, Value*> m_constantPool;
    HashMap<BlockSignature, B3::Type> m_tupleMap;
    InsertionSet m_constantInsertionValues;
    Value* m_framePointer { nullptr };
    GPRReg m_memoryBaseGPR { InvalidGPRReg };
    GPRReg m_boundsCheckingSizeGPR { InvalidGPRReg };
    GPRReg m_wasmContextInstanceGPR { InvalidGPRReg };
    bool m_makesCalls { false };
    std::optional<bool> m_hasExceptionHandlers;

    Value* m_instanceValue { nullptr }; // Always use the accessor below to ensure the instance value is materialized when used.
    bool m_usesInstanceValue { false };
    Value* instanceValue()
    {
        m_usesInstanceValue = true;
        return m_instanceValue;
    }

    uint32_t m_maxNumJSCallArguments { 0 };
    unsigned m_numImportFunctions;

    Checked<unsigned> m_tryCatchDepth { 0 };
    Checked<unsigned> m_callSiteIndex { 0 };
    Checked<unsigned> m_stackSize { 0 };
    Checked<unsigned> m_maxStackSize { 0 };
    StackMaps m_stackmaps;
    Vector<UnlinkedHandlerInfo> m_exceptionHandlers;
};

// Memory accesses in WebAssembly have unsigned 32-bit offsets, whereas they have signed 32-bit offsets in B3.
int32_t B3IRGenerator::fixupPointerPlusOffset(Value*& ptr, uint32_t offset)
{
    if (static_cast<uint64_t>(offset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        ptr = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), ptr, m_currentBlock->appendNew<Const64Value>(m_proc, origin(), offset));
        return 0;
    }
    return offset;
}

void B3IRGenerator::restoreWasmContextInstance(Procedure& proc, BasicBlock* block, Value* arg)
{
    if (Context::useFastTLS()) {
        PatchpointValue* patchpoint = block->appendNew<PatchpointValue>(proc, B3::Void, Origin());
        if (CCallHelpers::storeWasmContextInstanceNeedsMacroScratchRegister())
            patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
        patchpoint->append(ConstrainedValue(arg, ValueRep::SomeRegister));
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsageIf allowScratch(jit, CCallHelpers::storeWasmContextInstanceNeedsMacroScratchRegister());
                jit.storeWasmContextInstance(params[0].gpr());
            });
        return;
    }

    // FIXME: Because WasmToWasm call clobbers wasmContextInstance register and does not restore it, we need to restore it in the caller side.
    // This prevents us from using ArgumentReg to this (logically) immutable pinned register.
    PatchpointValue* patchpoint = block->appendNew<PatchpointValue>(proc, B3::Void, Origin());
    Effects effects = Effects::none();
    effects.writesPinned = true;
    effects.reads = B3::HeapRange::top();
    patchpoint->effects = effects;
    patchpoint->clobberLate(RegisterSetBuilder(m_wasmContextInstanceGPR));
    patchpoint->append(arg, ValueRep::SomeRegister);
    GPRReg wasmContextInstanceGPR = m_wasmContextInstanceGPR;
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& param) {
        jit.move(param[0].gpr(), wasmContextInstanceGPR);
    });
}

B3IRGenerator::B3IRGenerator(const ModuleInformation& info, Procedure& procedure, InternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, unsigned& osrEntryScratchBufferSize, MemoryMode mode, CompilationMode compilationMode, unsigned functionIndex, std::optional<bool> hasExceptionHandlers, unsigned loopIndexForOSREntry, TierUpCount* tierUp)
    : m_info(info)
    , m_mode(mode)
    , m_compilationMode(compilationMode)
    , m_functionIndex(functionIndex)
    , m_loopIndexForOSREntry(loopIndexForOSREntry)
    , m_tierUp(tierUp)
    , m_proc(procedure)
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
    , m_osrEntryScratchBufferSize(osrEntryScratchBufferSize)
    , m_constantInsertionValues(m_proc)
    , m_hasExceptionHandlers(hasExceptionHandlers)
    , m_numImportFunctions(info.importFunctionCount())
{
    m_topLevelBlock = m_proc.addBlock();
    m_rootBlocks.append(m_proc.addBlock());
    m_currentBlock = m_rootBlocks[0];

    // FIXME we don't really need to pin registers here if there's no memory. It makes wasm -> wasm thunks simpler for now. https://bugs.webkit.org/show_bug.cgi?id=166623
    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();

    m_memoryBaseGPR = pinnedRegs.baseMemoryPointer;
    m_proc.pinRegister(m_memoryBaseGPR);

    m_wasmContextInstanceGPR = pinnedRegs.wasmContextInstancePointer;
    if (!Context::useFastTLS())
        m_proc.pinRegister(m_wasmContextInstanceGPR);

    if (mode == MemoryMode::BoundsChecking) {
        m_boundsCheckingSizeGPR = pinnedRegs.boundsCheckingSizeRegister;
        m_proc.pinRegister(m_boundsCheckingSizeGPR);
    }

    if (info.memory) {
        m_proc.setWasmBoundsCheckGenerator([=, this] (CCallHelpers& jit, GPRReg pinnedGPR) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            switch (m_mode) {
            case MemoryMode::BoundsChecking:
                ASSERT_UNUSED(pinnedGPR, m_boundsCheckingSizeGPR == pinnedGPR);
                break;
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
            case MemoryMode::Signaling:
                ASSERT_UNUSED(pinnedGPR, InvalidGPRReg == pinnedGPR);
                break;
#endif
            }
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    {
        B3::PatchpointValue* getInstance = m_topLevelBlock->appendNew<B3::PatchpointValue>(m_proc, pointerType(), Origin());
        if (Context::useFastTLS())
            getInstance->clobber(RegisterSetBuilder::macroClobberedRegisters());
        else {
            // FIXME: Because WasmToWasm call clobbers wasmContextInstance register and does not restore it, we need to restore it in the caller side.
            // This prevents us from using ArgumentReg to this (logically) immutable pinned register.
            getInstance->effects.writesPinned = false;
            getInstance->effects.readsPinned = true;
            getInstance->resultConstraints = { ValueRep::reg(m_wasmContextInstanceGPR) };
        }
        getInstance->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            if (Context::useFastTLS()) {
                AllowMacroScratchRegisterUsageIf allowScratch(jit, CCallHelpers::loadWasmContextInstanceNeedsMacroScratchRegister());
                jit.loadWasmContextInstance(params[0].gpr());
            }
        });
        m_instanceValue = getInstance;
    }

    {
        auto* calleeMoveLocations = &compilation->calleeMoveLocations;
        static_assert(CallFrameSlot::codeBlock * sizeof(Register) < WasmCallingConvention::headerSizeInBytes, "We rely on this here for now.");
        static_assert(CallFrameSlot::callee * sizeof(Register) < WasmCallingConvention::headerSizeInBytes, "We rely on this here for now.");
        B3::PatchpointValue* getCalleePatchpoint = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Int64, Origin());
        getCalleePatchpoint->resultConstraints = { B3::ValueRep::SomeRegister };
        getCalleePatchpoint->effects = B3::Effects::none();
        getCalleePatchpoint->setGenerator(
            [=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                GPRReg result = params[0].gpr();
                MacroAssembler::DataLabelPtr moveLocation = jit.moveWithPatch(MacroAssembler::TrustedImmPtr(nullptr), result);
                jit.addLinkTask([calleeMoveLocations, moveLocation] (LinkBuffer& linkBuffer) {
                    calleeMoveLocations->append(linkBuffer.locationOf<WasmEntryPtrTag>(moveLocation));
                });
            });

        B3::Value* offsetOfCallee = m_currentBlock->appendNew<B3::Const64Value>(m_proc, Origin(), CallFrameSlot::callee * sizeof(Register));
        m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, Origin(),
            getCalleePatchpoint,
            m_currentBlock->appendNew<B3::Value>(m_proc, B3::Add, Origin(), framePointer(), offsetOfCallee));

        // FIXME: We shouldn't have to store zero into the CodeBlock* spot in the call frame,
        // but there are places that interpret non-null CodeBlock slot to mean a valid CodeBlock.
        // When doing unwinding, we'll need to verify that the entire runtime is OK with a non-null
        // CodeBlock not implying that the CodeBlock is valid.
        // https://bugs.webkit.org/show_bug.cgi?id=165321
        B3::Value* offsetOfCodeBlock = m_currentBlock->appendNew<B3::Const64Value>(m_proc, Origin(), CallFrameSlot::codeBlock * sizeof(Register));
        m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, Origin(),
            m_currentBlock->appendNew<B3::Const64Value>(m_proc, Origin(), 0),
            m_currentBlock->appendNew<B3::Value>(m_proc, B3::Add, Origin(), framePointer(), offsetOfCodeBlock));
    }

    {
        B3::PatchpointValue* stackOverflowCheck = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, Void, Origin());
        stackOverflowCheck->appendSomeRegister(instanceValue());
        stackOverflowCheck->appendSomeRegister(framePointer());
        stackOverflowCheck->clobber(RegisterSetBuilder::macroClobberedRegisters());
        stackOverflowCheck->numGPScratchRegisters = 2;
        stackOverflowCheck->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            const Checked<int32_t> wasmFrameSize = params.proc().frameSize();
            const unsigned minimumParentCheckSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), 1024);
            const unsigned extraFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), std::max<uint32_t>(
                // This allows us to elide stack checks for functions that are terminal nodes in the call
                // tree, (e.g they don't make any calls) and have a small enough frame size. This works by
                // having any such terminal node have its parent caller include some extra size in its
                // own check for it. The goal here is twofold:
                // 1. Emit less code.
                // 2. Try to speed things up by skipping stack checks.
                minimumParentCheckSize,
                // This allows us to elide stack checks in the Wasm -> Embedder call IC stub. Since these will
                // spill all arguments to the stack, we ensure that a stack check here covers the
                // stack that such a stub would use.
                Checked<uint32_t>(m_maxNumJSCallArguments) * sizeof(Register) + JSCallingConvention::headerSizeInBytes
            ));
            const int32_t checkSize = m_makesCalls ? (wasmFrameSize + extraFrameSize).value() : wasmFrameSize.value();
            bool needUnderflowCheck = static_cast<unsigned>(checkSize) > Options::reservedZoneSize();
            bool needsOverflowCheck = m_makesCalls || wasmFrameSize >= static_cast<int32_t>(minimumParentCheckSize) || needUnderflowCheck;

            // This allows leaf functions to not do stack checks if their frame size is within
            // certain limits since their caller would have already done the check.
            if (needsOverflowCheck) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                GPRReg contextInstance = params[0].gpr();
                GPRReg fp = params[1].gpr();
                GPRReg scratch1 = params.gpScratch(0);
                GPRReg scratch2 = params.gpScratch(1);

                jit.loadPtr(CCallHelpers::Address(contextInstance, Instance::offsetOfCachedStackLimit()), scratch2);
                jit.addPtr(CCallHelpers::TrustedImm32(-checkSize), fp, scratch1);
                MacroAssembler::JumpList overflow;
                if (UNLIKELY(needUnderflowCheck))
                    overflow.append(jit.branchPtr(CCallHelpers::Above, scratch1, fp));
                overflow.append(jit.branchPtr(CCallHelpers::Below, scratch1, scratch2));
                jit.addLinkTask([overflow] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(overflow, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
                });
            }
        });
    }

    emitEntryTierUpCheck();

    if (isOSREntry(m_compilationMode))
        m_currentBlock = m_proc.addBlock();
}

void B3IRGenerator::restoreWebAssemblyGlobalState(RestoreCachedStackLimit restoreCachedStackLimit, const MemoryInformation& memory, Value* instance, Procedure& proc, BasicBlock* block, bool restoreInstance)
{
    if (restoreInstance)
        restoreWasmContextInstance(proc, block, instance);

    if (restoreCachedStackLimit == RestoreCachedStackLimit::Yes) {
        // The Instance caches the stack limit, but also knows where its canonical location is.
        Value* pointerToActualStackLimit = block->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfPointerToActualStackLimit()));
        Value* actualStackLimit = block->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), pointerToActualStackLimit);
        block->appendNew<MemoryValue>(m_proc, Store, origin(), actualStackLimit, instanceValue(), safeCast<int32_t>(Instance::offsetOfCachedStackLimit()));
    }

    if (!!memory) {
        const PinnedRegisterInfo* pinnedRegs = &PinnedRegisterInfo::get();
        RegisterSet clobbers;
        clobbers.add(pinnedRegs->baseMemoryPointer, IgnoreVectors);
        clobbers.add(pinnedRegs->boundsCheckingSizeRegister, IgnoreVectors);
        clobbers.merge(RegisterSetBuilder::macroClobberedRegisters());

        B3::PatchpointValue* patchpoint = block->appendNew<B3::PatchpointValue>(proc, B3::Void, origin());
        Effects effects = Effects::none();
        effects.writesPinned = true;
        effects.reads = B3::HeapRange::top();
        patchpoint->effects = effects;
        patchpoint->clobber(clobbers);
        patchpoint->numGPScratchRegisters = 1;

        patchpoint->append(instance, ValueRep::SomeRegister);
        patchpoint->setGenerator([pinnedRegs] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg baseMemory = pinnedRegs->baseMemoryPointer;
            GPRReg scratch = params.gpScratch(0);

            jit.loadPtr(CCallHelpers::Address(params[0].gpr(), Instance::offsetOfCachedBoundsCheckingSize()), pinnedRegs->boundsCheckingSizeRegister);
            jit.loadPtr(CCallHelpers::Address(params[0].gpr(), Instance::offsetOfCachedMemory()), baseMemory);

            jit.cageConditionallyAndUntag(Gigacage::Primitive, baseMemory, pinnedRegs->boundsCheckingSizeRegister, scratch);
        });
    }
}

void B3IRGenerator::emitExceptionCheck(CCallHelpers& jit, ExceptionType type)
{
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
    auto jumpToExceptionStub = jit.jump();

    jit.addLinkTask([jumpToExceptionStub] (LinkBuffer& linkBuffer) {
        linkBuffer.link(jumpToExceptionStub, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator).code()));
    });
}

Value* B3IRGenerator::constant(B3::Type type, uint64_t bits, std::optional<Origin> maybeOrigin)
{
    auto result = m_constantPool.ensure(ValueKey(opcodeForConstant(type), type, static_cast<int64_t>(bits)), [&] {
        Value* result = m_proc.addConstant(maybeOrigin ? *maybeOrigin : origin(), type, bits);
        m_constantInsertionValues.insertValue(0, result);
        return result;
    });
    return result.iterator->value;
}

Value* B3IRGenerator::framePointer()
{
    if (!m_framePointer) {
        m_framePointer = m_proc.add<B3::Value>(B3::FramePointer, Origin());
        ASSERT(m_framePointer);
        m_constantInsertionValues.insertValue(0, m_framePointer);
    }
    return m_framePointer;
}

void B3IRGenerator::insertEntrySwitch()
{
    m_proc.setNumEntrypoints(m_rootBlocks.size());

    Ref<B3::Air::PrologueGenerator> catchPrologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>([] (CCallHelpers& jit, B3::Air::Code& code) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        emitCatchPrologueShared(code, jit);
    });

    for (unsigned i = 1; i < m_rootBlocks.size(); ++i)
        m_proc.code().setPrologueForEntrypoint(i, catchPrologueGenerator.copyRef());

    m_currentBlock = m_topLevelBlock;
    m_currentBlock->appendNew<Value>(m_proc, EntrySwitch, Origin());
    for (BasicBlock* block : m_rootBlocks)
        m_currentBlock->appendSuccessor(FrequentedBlock(block));
}

void B3IRGenerator::insertConstants()
{
    bool mayHaveExceptionHandlers = !m_hasExceptionHandlers || m_hasExceptionHandlers.value();

    Value* invalidCallSiteIndex = nullptr;
    if (mayHaveExceptionHandlers)
        invalidCallSiteIndex = constant(B3::Int32, PatchpointExceptionHandle::s_invalidCallSiteIndex, Origin());
    m_constantInsertionValues.execute(m_proc.at(0));

    if (!mayHaveExceptionHandlers)
        return;

    Value* jsInstance = m_proc.add<MemoryValue>(Load, pointerType(), Origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfOwner()));
    Value* storeThis = m_proc.add<B3::MemoryValue>(B3::Store, Origin(), jsInstance, framePointer(), safeCast<int32_t>(CallFrameSlot::thisArgument * sizeof(Register)));
    Value* storeCallSiteIndex = m_proc.add<B3::MemoryValue>(B3::Store, Origin(), invalidCallSiteIndex, framePointer(), safeCast<int32_t>(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + TagOffset));

    BasicBlock* block = m_rootBlocks[0];
    m_constantInsertionValues.insertValue(0, jsInstance);
    m_constantInsertionValues.insertValue(0, storeThis);
    m_constantInsertionValues.insertValue(0, storeCallSiteIndex);
    m_constantInsertionValues.execute(block);
}

B3::Type B3IRGenerator::toB3ResultType(BlockSignature returnType)
{
    if (returnType->as<FunctionSignature>()->returnsVoid())
        return B3::Void;

    if (returnType->as<FunctionSignature>()->returnCount() == 1)
        return toB3Type(returnType->as<FunctionSignature>()->returnType(0));

    auto result = m_tupleMap.ensure(returnType, [&] {
        Vector<B3::Type> result;
        for (unsigned i = 0; i < returnType->as<FunctionSignature>()->returnCount(); ++i)
            result.append(toB3Type(returnType->as<FunctionSignature>()->returnType(i)));
        return m_proc.addTuple(WTFMove(result));
    });
    return result.iterator->value;
}

auto B3IRGenerator::addLocal(Type type, uint32_t count) -> PartialResult
{
    size_t newSize = m_locals.size() + count;
    ASSERT(!(CheckedUint32(count) + m_locals.size()).hasOverflowed());
    ASSERT(newSize <= maxFunctionLocals);
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(newSize), "can't allocate memory for ", newSize, " locals");

    for (uint32_t i = 0; i < count; ++i) {
        Variable* local = m_proc.addVariable(toB3Type(type));
        m_locals.uncheckedAppend(local);
        auto val = isRefType(type) ? JSValue::encode(jsNull()) : 0;
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, constant(toB3Type(type), val, Origin()));
    }
    return { };
}

auto B3IRGenerator::addArguments(const TypeDefinition& signature) -> PartialResult
{
    ASSERT(!m_locals.size());
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(signature.as<FunctionSignature>()->argumentCount()), "can't allocate memory for ", signature.as<FunctionSignature>()->argumentCount(), " arguments");

    m_locals.grow(signature.as<FunctionSignature>()->argumentCount());
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Callee);

    for (size_t i = 0; i < signature.as<FunctionSignature>()->argumentCount(); ++i) {
        B3::Type type = toB3Type(signature.as<FunctionSignature>()->argumentType(i));
        B3::Value* argument;
        auto rep = wasmCallInfo.params[i];
        if (rep.location.isGPR()) {
            argument = m_currentBlock->appendNew<B3::ArgumentRegValue>(m_proc, Origin(), rep.location.jsr().payloadGPR());
            if (type == B3::Int32)
                argument = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), argument);
        } else if (rep.location.isFPR()) {
            argument = m_currentBlock->appendNew<B3::ArgumentRegValue>(m_proc, Origin(), rep.location.fpr());
            if (type == B3::Float)
                argument = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), argument);
        } else {
            ASSERT(rep.location.isStack());
            B3::Value* address = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Add, Origin(), framePointer(),
                m_currentBlock->appendNew<B3::Const64Value>(m_proc, Origin(), rep.location.offsetFromFP()));
            argument = m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Load, type, Origin(), address);
        }

        Variable* argumentVariable = m_proc.addVariable(argument->type());
        m_locals[i] = argumentVariable;
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), argumentVariable, argument);
    }

    return { };
}

auto B3IRGenerator::addRefIsNull(ExpressionType value, ExpressionType& result) -> PartialResult
{
    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::Equal, origin(), get(value), m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull()))));
    return { };
}

auto B3IRGenerator::addTableGet(unsigned tableIndex, ExpressionType index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    Value* resultValue = m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::Externref), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationGetWasmTableElement)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), get(index));

    {
        result = push(resultValue);
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<Const64Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addTableSet(unsigned tableIndex, ExpressionType index, ExpressionType value) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    auto shouldThrow = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int32, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationSetWasmTableElement)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), get(index), get(value));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), shouldThrow, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addRefFunc(uint32_t index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.

    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmRefFunc)),
        instanceValue(), constant(toB3Type(Types::I32), index)));

    return { };
}

auto B3IRGenerator::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    Value* resultValue = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmTableInit)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), elementIndex),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex),
        get(dstOffset), get(srcOffset), get(length));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addElemDrop(unsigned elementIndex) -> PartialResult
{
    m_currentBlock->appendNew<CCallValue>(
        m_proc, B3::Void, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmElemDrop)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), elementIndex));

    return { };
}

auto B3IRGenerator::addTableSize(unsigned tableIndex, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationGetWasmTableSize)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex)));

    return { };
}

auto B3IRGenerator::addTableGrow(unsigned tableIndex, ExpressionType fill, ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmTableGrow)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), get(fill), get(delta)));

    return { };
}

auto B3IRGenerator::addTableFill(unsigned tableIndex, ExpressionType offset, ExpressionType fill, ExpressionType count) -> PartialResult
{
    Value* resultValue = m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmTableFill)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), get(offset), get(fill), get(count));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    Value* resultValue = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmTableCopy)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), dstTableIndex),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), srcTableIndex),
        get(dstOffset), get(srcOffset), get(length));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    ASSERT(m_locals[index]);
    result = push(m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, origin(), m_locals[index]));
    return { };
}

auto B3IRGenerator::addUnreachable() -> PartialResult
{
    B3::PatchpointValue* unreachable = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
    unreachable->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::Unreachable);
    });
    unreachable->effects.terminal = true;
    return { };
}

auto B3IRGenerator::emitIndirectCall(Value* calleeInstance, Value* calleeCode, const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    // Do a context switch if needed.
    {
        BasicBlock* continuation = m_proc.addBlock();
        BasicBlock* doContextSwitch = m_proc.addBlock();

        Value* isSameContextInstance = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(),
            calleeInstance, instanceValue());
        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
            isSameContextInstance, FrequentedBlock(continuation), FrequentedBlock(doContextSwitch));

        PatchpointValue* patchpoint = doContextSwitch->appendNew<PatchpointValue>(m_proc, B3::Void, origin());
        patchpoint->effects.writesPinned = true;
        // We pessimistically assume we're calling something with BoundsChecking memory.
        // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
        patchpoint->clobber(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
        patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
        patchpoint->append(calleeInstance, ValueRep::SomeRegister);
        patchpoint->append(instanceValue(), ValueRep::SomeRegister);
        patchpoint->numGPScratchRegisters = 1;

        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg calleeInstance = params[0].gpr();
            GPRReg oldContextInstance = params[1].gpr();
            const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
            GPRReg baseMemory = pinnedRegs.baseMemoryPointer;
            ASSERT(calleeInstance != baseMemory);
            jit.loadPtr(CCallHelpers::Address(oldContextInstance, Instance::offsetOfCachedStackLimit()), baseMemory);
            jit.storePtr(baseMemory, CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedStackLimit()));
            jit.storeWasmContextInstance(calleeInstance);
            ASSERT(pinnedRegs.boundsCheckingSizeRegister != baseMemory);
            // FIXME: We should support more than one memory size register
            //   see: https://bugs.webkit.org/show_bug.cgi?id=162952
            ASSERT(pinnedRegs.boundsCheckingSizeRegister != calleeInstance);
            GPRReg scratch = params.gpScratch(0);

            jit.loadPtr(CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedBoundsCheckingSize()), pinnedRegs.boundsCheckingSizeRegister); // Memory size.
            jit.loadPtr(CCallHelpers::Address(calleeInstance, Instance::offsetOfCachedMemory()), baseMemory); // Memory::void*.

            jit.cageConditionallyAndUntag(Gigacage::Primitive, baseMemory, pinnedRegs.boundsCheckingSizeRegister, scratch);
        });
        doContextSwitch->appendNewControlValue(m_proc, Jump, origin(), continuation);

        m_currentBlock = continuation;
    }

    B3::Type returnType = toB3ResultType(&signature);
    Value* callResult = createCallPatchpoint(m_currentBlock, origin(), signature, args,
        scopedLambdaRef<void(PatchpointValue*, Box<PatchpointExceptionHandle>)>([=, this] (PatchpointValue* patchpoint, Box<PatchpointExceptionHandle> handle) -> void {
            patchpoint->effects.writesPinned = true;
            patchpoint->effects.readsPinned = true;
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we're always calling something that is bounds checking so
            // because the wasm->wasm thunk unconditionally overrides the size registers.
            // FIXME: We should not have to do this, but the wasm->wasm stub assumes it can
            // use all the pinned registers as scratch: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

            patchpoint->append(calleeCode, ValueRep::SomeRegister);
            patchpoint->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                handle->generate(jit, params, this);
                jit.call(params[params.proc().resultCount(returnType)].gpr(), WasmEntryPtrTag);
            });
        }));

    switch (returnType.kind()) {
    case B3::Void: {
        break;
    }
    case B3::Tuple: {
        const Vector<B3::Type>& tuple = m_proc.tupleForType(returnType);
        for (unsigned i = 0; i < signature.as<FunctionSignature>()->returnCount(); ++i)
            results.append(push(m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), tuple[i], callResult, i)));
        break;
    }
    default: {
        results.append(push(callResult));
        break;
    }
    }

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, instanceValue(), m_proc, m_currentBlock);

    return { };
}

auto B3IRGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationGrowMemory)),
        framePointer(), instanceValue(), get(delta)));

    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::No, m_info.memory, instanceValue(), m_proc, m_currentBlock);

    return { };
}

auto B3IRGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    static_assert(sizeof(std::declval<Memory*>()->size()) == sizeof(uint64_t), "codegen relies on this size");

    Value* memory = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfMemory()));
    Value* handle = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), memory, safeCast<int32_t>(Memory::offsetOfHandle()));
    Value* size = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), handle, safeCast<int32_t>(BufferMemoryHandle::offsetOfSize()));

    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    Value* numPages = m_currentBlock->appendNew<Value>(m_proc, ZShr, origin(),
        size, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), shiftValue));

    result = push(m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), numPages));

    return { };
}

auto B3IRGenerator::addMemoryFill(ExpressionType dstAddress, ExpressionType targetValue, ExpressionType count) -> PartialResult
{
    Value* resultValue = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmMemoryFill)),
        instanceValue(),
        get(dstAddress), get(targetValue), get(count));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addMemoryInit(unsigned dataSegmentIndex, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length) -> PartialResult
{
    Value* resultValue = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmMemoryInit)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), dataSegmentIndex),
        get(dstAddress), get(srcAddress), get(length));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count) -> PartialResult
{
    Value* resultValue = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmMemoryCopy)),
        instanceValue(),
        get(dstAddress), get(srcAddress), get(count));

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addDataDrop(unsigned dataSegmentIndex) -> PartialResult
{
    m_currentBlock->appendNew<CCallValue>(
        m_proc, B3::Void, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmDataDrop)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), dataSegmentIndex));

    return { };
}

auto B3IRGenerator::setLocal(uint32_t index, ExpressionType value) -> PartialResult
{
    ASSERT(m_locals[index]);
    m_currentBlock->appendNew<VariableValue>(m_proc, B3::Set, origin(), m_locals[index], get(value));
    return { };
}

auto B3IRGenerator::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Value* globalsArray = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfGlobals()));
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        result = push(m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(global.type), origin(), globalsArray, safeCast<int32_t>(index * sizeof(Global::Value))));
        break;
    case Wasm::GlobalInformation::BindingMode::Portable: {
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        Value* pointer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, B3::Int64, origin(), globalsArray, safeCast<int32_t>(index * sizeof(Global::Value)));
        result = push(m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(global.type), origin(), pointer));
        break;
    }
    }
    return { };
}

auto B3IRGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    ASSERT(toB3Type(global.type) == value->type());
    Value* globalsArray = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfGlobals()));
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), get(value), globalsArray, safeCast<int32_t>(index * sizeof(Global::Value)));
        if (isRefType(global.type))
            emitWriteBarrierForJSWrapper();
        break;
    case Wasm::GlobalInformation::BindingMode::Portable: {
        ASSERT(global.mutability == Wasm::Mutability::Mutable);
        Value* pointer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, B3::Int64, origin(), globalsArray, safeCast<int32_t>(index * sizeof(Global::Value)));
        m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), get(value), pointer);
        // We emit a write-barrier onto JSWebAssemblyGlobal, not JSWebAssemblyInstance.
        if (isRefType(global.type)) {
            Value* instance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfOwner()));
            Value* cell = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), pointer, Wasm::Global::offsetOfOwner() - Wasm::Global::offsetOfValue());
            Value* cellState = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
            Value* vm = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instance, safeCast<int32_t>(JSWebAssemblyInstance::offsetOfVM()));
            Value* threshold = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapBarrierThreshold()));

            BasicBlock* fenceCheckPath = m_proc.addBlock();
            BasicBlock* fencePath = m_proc.addBlock();
            BasicBlock* doSlowPath = m_proc.addBlock();
            BasicBlock* continuation = m_proc.addBlock();

            m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Above, origin(), cellState, threshold),
                FrequentedBlock(continuation), FrequentedBlock(fenceCheckPath, FrequencyClass::Rare));
            fenceCheckPath->addPredecessor(m_currentBlock);
            continuation->addPredecessor(m_currentBlock);
            m_currentBlock = fenceCheckPath;

            Value* shouldFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapMutatorShouldBeFenced()));
            m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
                shouldFence,
                FrequentedBlock(fencePath), FrequentedBlock(doSlowPath));
            fencePath->addPredecessor(m_currentBlock);
            doSlowPath->addPredecessor(m_currentBlock);
            m_currentBlock = fencePath;

            B3::PatchpointValue* doFence = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
            doFence->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                jit.memoryFence();
            });

            Value* cellStateLoadAfterFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
            m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Above, origin(), cellStateLoadAfterFence, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), blackThreshold)),
                FrequentedBlock(continuation), FrequentedBlock(doSlowPath, FrequencyClass::Rare));
            doSlowPath->addPredecessor(m_currentBlock);
            continuation->addPredecessor(m_currentBlock);
            m_currentBlock = doSlowPath;

            Value* writeBarrierAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmWriteBarrierSlowPath));
            m_currentBlock->appendNew<CCallValue>(m_proc, B3::Void, origin(), writeBarrierAddress, cell, vm);
            m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);

            continuation->addPredecessor(m_currentBlock);
            m_currentBlock = continuation;
        }
        break;
    }
    }
    return { };
}

inline void B3IRGenerator::emitWriteBarrierForJSWrapper()
{
    Value* instance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfOwner()));
    emitWriteBarrier(instance, instance);
}

inline void B3IRGenerator::emitWriteBarrier(Value* cell, Value* instanceCell)
{
    Value* cellState = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
    Value* vm = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceCell, safeCast<int32_t>(JSWebAssemblyInstance::offsetOfVM()));
    Value* threshold = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapBarrierThreshold()));

    BasicBlock* fenceCheckPath = m_proc.addBlock();
    BasicBlock* fencePath = m_proc.addBlock();
    BasicBlock* doSlowPath = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
        m_currentBlock->appendNew<Value>(m_proc, Above, origin(), cellState, threshold),
        FrequentedBlock(continuation), FrequentedBlock(fenceCheckPath, FrequencyClass::Rare));
    fenceCheckPath->addPredecessor(m_currentBlock);
    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = fenceCheckPath;

    Value* shouldFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), vm, safeCast<int32_t>(VM::offsetOfHeapMutatorShouldBeFenced()));
    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
        shouldFence,
        FrequentedBlock(fencePath), FrequentedBlock(doSlowPath));
    fencePath->addPredecessor(m_currentBlock);
    doSlowPath->addPredecessor(m_currentBlock);
    m_currentBlock = fencePath;

    B3::PatchpointValue* doFence = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
    doFence->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
        jit.memoryFence();
    });

    Value* cellStateLoadAfterFence = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
        m_currentBlock->appendNew<Value>(m_proc, Above, origin(), cellStateLoadAfterFence, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), blackThreshold)),
        FrequentedBlock(continuation), FrequentedBlock(doSlowPath, FrequencyClass::Rare));
    doSlowPath->addPredecessor(m_currentBlock);
    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = doSlowPath;

    Value* writeBarrierAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmWriteBarrierSlowPath));
    m_currentBlock->appendNew<CCallValue>(m_proc, B3::Void, origin(), writeBarrierAddress, cell, vm);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);

    continuation->addPredecessor(m_currentBlock);
    m_currentBlock = continuation;
}

inline Value* B3IRGenerator::emitCheckAndPreparePointer(Value* pointer, uint32_t offset, uint32_t sizeOfOperation)
{
    ASSERT(m_memoryBaseGPR);

    switch (m_mode) {
    case MemoryMode::BoundsChecking: {
        // We're not using signal handling only when the memory is not shared.
        // Regardless of signaling, we must check that no memory access exceeds the current memory size.
        ASSERT(m_boundsCheckingSizeGPR);
        ASSERT(sizeOfOperation + offset > offset);
        m_currentBlock->appendNew<WasmBoundsCheckValue>(m_proc, origin(), m_boundsCheckingSizeGPR, pointer, sizeOfOperation + offset - 1);
        break;
    }

#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    case MemoryMode::Signaling: {
        // We've virtually mapped 4GiB+redzone for this memory. Only the user-allocated pages are addressable, contiguously in range [0, current],
        // and everything above is mapped PROT_NONE. We don't need to perform any explicit bounds check in the 4GiB range because WebAssembly register
        // memory accesses are 32-bit. However WebAssembly register + offset accesses perform the addition in 64-bit which can push an access above
        // the 32-bit limit (the offset is unsigned 32-bit). The redzone will catch most small offsets, and we'll explicitly bounds check any
        // register + large offset access. We don't think this will be generated frequently.
        //
        // We could check that register + large offset doesn't exceed 4GiB+redzone since that's technically the limit we need to avoid overflowing the
        // PROT_NONE region, but it's better if we use a smaller immediate because it can codegens better. We know that anything equal to or greater
        // than the declared 'maximum' will trap, so we can compare against that number. If there was no declared 'maximum' then we still know that
        // any access equal to or greater than 4GiB will trap, no need to add the redzone.
        if (offset >= Memory::fastMappedRedzoneBytes()) {
            size_t maximum = m_info.memory.maximum() ? m_info.memory.maximum().bytes() : std::numeric_limits<uint32_t>::max();
            m_currentBlock->appendNew<WasmBoundsCheckValue>(m_proc, origin(), pointer, sizeOfOperation + offset - 1, maximum);
        }
        break;
    }
#endif
    }

    pointer = m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), pointer);
    return m_currentBlock->appendNew<WasmAddressValue>(m_proc, origin(), pointer, m_memoryBaseGPR);
}

inline uint32_t sizeOfLoadOp(LoadOpType op)
{
    switch (op) {
    case LoadOpType::I32Load8S:
    case LoadOpType::I32Load8U:
    case LoadOpType::I64Load8S:
    case LoadOpType::I64Load8U:
        return 1;
    case LoadOpType::I32Load16S:
    case LoadOpType::I64Load16S:
    case LoadOpType::I32Load16U:
    case LoadOpType::I64Load16U:
        return 2;
    case LoadOpType::I32Load:
    case LoadOpType::I64Load32S:
    case LoadOpType::I64Load32U:
    case LoadOpType::F32Load:
        return 4;
    case LoadOpType::I64Load:
    case LoadOpType::F64Load:
        return 8;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline B3::Kind B3IRGenerator::memoryKind(B3::Opcode memoryOp)
{
    if (useSignalingMemory() || m_info.memory.isShared())
        return trapping(memoryOp);
    return memoryOp;
}

inline Value* B3IRGenerator::emitLoadOp(LoadOpType op, Value* pointer, uint32_t uoffset)
{
    int32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    switch (op) {
    case LoadOpType::I32Load8S: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load8S), origin(), pointer, offset);
    }

    case LoadOpType::I64Load8S: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load8S), origin(), pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin(), value);
    }

    case LoadOpType::I32Load8U: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load8Z), origin(), pointer, offset);
    }

    case LoadOpType::I64Load8U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load8Z), origin(), pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), value);
    }

    case LoadOpType::I32Load16S: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16S), origin(), pointer, offset);
    }

    case LoadOpType::I64Load16S: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16S), origin(), pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin(), value);
    }

    case LoadOpType::I32Load16U: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16Z), origin(), pointer, offset);
    }

    case LoadOpType::I64Load16U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16Z), origin(), pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), value);
    }

    case LoadOpType::I32Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int32, origin(), pointer, offset);
    }

    case LoadOpType::I64Load32U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int32, origin(), pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), value);
    }

    case LoadOpType::I64Load32S: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int32, origin(), pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, SExt32, origin(), value);
    }

    case LoadOpType::I64Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int64, origin(), pointer, offset);
    }

    case LoadOpType::F32Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Float, origin(), pointer, offset);
    }

    case LoadOpType::F64Load: {
        return m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Double, origin(), pointer, offset);
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto B3IRGenerator::load(LoadOpType op, ExpressionType pointerVar, ExpressionType& result, uint32_t offset) -> PartialResult
{
    Value* pointer = get(pointerVar);
    ASSERT(pointer->type() == Int32);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfLoadOp(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (op) {
        case LoadOpType::I32Load8S:
        case LoadOpType::I32Load16S:
        case LoadOpType::I32Load:
        case LoadOpType::I32Load16U:
        case LoadOpType::I32Load8U:
            result = push(constant(Int32, 0));
            break;
        case LoadOpType::I64Load8S:
        case LoadOpType::I64Load8U:
        case LoadOpType::I64Load16S:
        case LoadOpType::I64Load32U:
        case LoadOpType::I64Load32S:
        case LoadOpType::I64Load:
        case LoadOpType::I64Load16U:
            result = push(constant(Int64, 0));
            break;
        case LoadOpType::F32Load:
            result = push(constant(Float, 0));
            break;
        case LoadOpType::F64Load:
            result = push(constant(Double, 0));
            break;
        }

    } else
        result = push(emitLoadOp(op, emitCheckAndPreparePointer(pointer, offset, sizeOfLoadOp(op)), offset));

    return { };
}

inline uint32_t sizeOfStoreOp(StoreOpType op)
{
    switch (op) {
    case StoreOpType::I32Store8:
    case StoreOpType::I64Store8:
        return 1;
    case StoreOpType::I32Store16:
    case StoreOpType::I64Store16:
        return 2;
    case StoreOpType::I32Store:
    case StoreOpType::I64Store32:
    case StoreOpType::F32Store:
        return 4;
    case StoreOpType::I64Store:
    case StoreOpType::F64Store:
        return 8;
    }
    RELEASE_ASSERT_NOT_REACHED();
}


inline void B3IRGenerator::emitStoreOp(StoreOpType op, Value* pointer, Value* value, uint32_t uoffset)
{
    int32_t offset = fixupPointerPlusOffset(pointer, uoffset);

    switch (op) {
    case StoreOpType::I64Store8:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), value);
        FALLTHROUGH;

    case StoreOpType::I32Store8:
        m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Store8), origin(), value, pointer, offset);
        return;

    case StoreOpType::I64Store16:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), value);
        FALLTHROUGH;

    case StoreOpType::I32Store16:
        m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Store16), origin(), value, pointer, offset);
        return;

    case StoreOpType::I64Store32:
        value = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), value);
        FALLTHROUGH;

    case StoreOpType::I64Store:
    case StoreOpType::I32Store:
    case StoreOpType::F32Store:
    case StoreOpType::F64Store:
        m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Store), origin(), value, pointer, offset);
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto B3IRGenerator::store(StoreOpType op, ExpressionType pointerVar, ExpressionType valueVar, uint32_t offset) -> PartialResult
{
    Value* pointer = get(pointerVar);
    Value* value = get(valueVar);
    ASSERT(pointer->type() == Int32);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfStoreOp(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    } else
        emitStoreOp(op, emitCheckAndPreparePointer(pointer, offset, sizeOfStoreOp(op)), value, offset);

    return { };
}

inline Width accessWidth(ExtAtomicOpType op)
{
    return widthForBytes(1 << memoryLog2Alignment(op));
}

inline uint32_t sizeOfAtomicOpMemoryAccess(ExtAtomicOpType op)
{
    return bytesForWidth(accessWidth(op));
}

inline Value* B3IRGenerator::sanitizeAtomicResult(ExtAtomicOpType op, Type valueType, Value* result)
{
    auto sanitize32 = [&](Value* result) {
        switch (accessWidth(op)) {
        case Width8:
            return m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), result, constant(Int32, 0xff));
        case Width16:
            return m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), result, constant(Int32, 0xffff));
        default:
            return result;
        }
    };

    switch (valueType.kind) {
    case TypeKind::I64: {
        if (accessWidth(op) == Width64)
            return result;
        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), sanitize32(result));
    }
    case TypeKind::I32:
        return sanitize32(result);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

Value* B3IRGenerator::fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType op, Value* ptr, uint32_t offset)
{
    auto pointer = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), ptr, m_currentBlock->appendNew<Const64Value>(m_proc, origin(), offset));
    if (accessWidth(op) != Width8) {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), pointer, constant(pointerType(), sizeOfAtomicOpMemoryAccess(op) - 1)));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }
    return pointer;
}

inline Value* B3IRGenerator::emitAtomicLoadOp(ExtAtomicOpType op, Type valueType, Value* pointer, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    Value* value = nullptr;
    switch (accessWidth(op)) {
    case Width8:
    case Width16:
    case Width32:
        value = constant(Int32, 0);
        break;
    case Width64:
        value = constant(Int64, 0);
        break;
    case Width128:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return sanitizeAtomicResult(op, valueType, m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicXchgAdd), origin(), accessWidth(op), value, pointer));
}

auto B3IRGenerator::atomicLoad(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (valueType.kind) {
        case TypeKind::I32:
            result = push(constant(Int32, 0));
            break;
        case TypeKind::I64:
            result = push(constant(Int64, 0));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = push(emitAtomicLoadOp(op, valueType, emitCheckAndPreparePointer(get(pointer), offset, sizeOfAtomicOpMemoryAccess(op)), offset));

    return { };
}

inline void B3IRGenerator::emitAtomicStoreOp(ExtAtomicOpType op, Type valueType, Value* pointer, Value* value, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    if (valueType.isI64() && accessWidth(op) != Width64)
        value = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), value);
    m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicXchg), origin(), accessWidth(op), value, pointer);
}

auto B3IRGenerator::atomicStore(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    } else
        emitAtomicStoreOp(op, valueType, emitCheckAndPreparePointer(get(pointer), offset, sizeOfAtomicOpMemoryAccess(op)), get(value), offset);

    return { };
}

inline Value* B3IRGenerator::emitAtomicBinaryRMWOp(ExtAtomicOpType op, Type valueType, Value* pointer, Value* value, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    B3::Opcode opcode = B3::Nop;
    switch (op) {
    case ExtAtomicOpType::I32AtomicRmw8AddU:
    case ExtAtomicOpType::I32AtomicRmw16AddU:
    case ExtAtomicOpType::I32AtomicRmwAdd:
    case ExtAtomicOpType::I64AtomicRmw8AddU:
    case ExtAtomicOpType::I64AtomicRmw16AddU:
    case ExtAtomicOpType::I64AtomicRmw32AddU:
    case ExtAtomicOpType::I64AtomicRmwAdd:
        opcode = AtomicXchgAdd;
        break;
    case ExtAtomicOpType::I32AtomicRmw8SubU:
    case ExtAtomicOpType::I32AtomicRmw16SubU:
    case ExtAtomicOpType::I32AtomicRmwSub:
    case ExtAtomicOpType::I64AtomicRmw8SubU:
    case ExtAtomicOpType::I64AtomicRmw16SubU:
    case ExtAtomicOpType::I64AtomicRmw32SubU:
    case ExtAtomicOpType::I64AtomicRmwSub:
        opcode = AtomicXchgSub;
        break;
    case ExtAtomicOpType::I32AtomicRmw8AndU:
    case ExtAtomicOpType::I32AtomicRmw16AndU:
    case ExtAtomicOpType::I32AtomicRmwAnd:
    case ExtAtomicOpType::I64AtomicRmw8AndU:
    case ExtAtomicOpType::I64AtomicRmw16AndU:
    case ExtAtomicOpType::I64AtomicRmw32AndU:
    case ExtAtomicOpType::I64AtomicRmwAnd:
        opcode = AtomicXchgAnd;
        break;
    case ExtAtomicOpType::I32AtomicRmw8OrU:
    case ExtAtomicOpType::I32AtomicRmw16OrU:
    case ExtAtomicOpType::I32AtomicRmwOr:
    case ExtAtomicOpType::I64AtomicRmw8OrU:
    case ExtAtomicOpType::I64AtomicRmw16OrU:
    case ExtAtomicOpType::I64AtomicRmw32OrU:
    case ExtAtomicOpType::I64AtomicRmwOr:
        opcode = AtomicXchgOr;
        break;
    case ExtAtomicOpType::I32AtomicRmw8XorU:
    case ExtAtomicOpType::I32AtomicRmw16XorU:
    case ExtAtomicOpType::I32AtomicRmwXor:
    case ExtAtomicOpType::I64AtomicRmw8XorU:
    case ExtAtomicOpType::I64AtomicRmw16XorU:
    case ExtAtomicOpType::I64AtomicRmw32XorU:
    case ExtAtomicOpType::I64AtomicRmwXor:
        opcode = AtomicXchgXor;
        break;
    case ExtAtomicOpType::I32AtomicRmw8XchgU:
    case ExtAtomicOpType::I32AtomicRmw16XchgU:
    case ExtAtomicOpType::I32AtomicRmwXchg:
    case ExtAtomicOpType::I64AtomicRmw8XchgU:
    case ExtAtomicOpType::I64AtomicRmw16XchgU:
    case ExtAtomicOpType::I64AtomicRmw32XchgU:
    case ExtAtomicOpType::I64AtomicRmwXchg:
        opcode = AtomicXchg;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (valueType.isI64() && accessWidth(op) != Width64)
        value = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), value);

    return sanitizeAtomicResult(op, valueType, m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(opcode), origin(), accessWidth(op), value, pointer));
}

auto B3IRGenerator::atomicBinaryRMW(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (valueType.kind) {
        case TypeKind::I32:
            result = push(constant(Int32, 0));
            break;
        case TypeKind::I64:
            result = push(constant(Int64, 0));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = push(emitAtomicBinaryRMWOp(op, valueType, emitCheckAndPreparePointer(get(pointer), offset, sizeOfAtomicOpMemoryAccess(op)), get(value), offset));

    return { };
}

Value* B3IRGenerator::emitAtomicCompareExchange(ExtAtomicOpType op, Type valueType, Value* pointer, Value* expected, Value* value, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    Width accessWidth = Wasm::accessWidth(op);

    if (widthForType(toB3Type(valueType)) == accessWidth)
        return sanitizeAtomicResult(op, valueType, m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicStrongCAS), origin(), accessWidth, expected, value, pointer));

    Value* maximum = nullptr;
    switch (valueType.kind) {
    case TypeKind::I64: {
        switch (accessWidth) {
        case Width8:
            maximum = constant(Int64, UINT8_MAX);
            break;
        case Width16:
            maximum = constant(Int64, UINT16_MAX);
            break;
        case Width32:
            maximum = constant(Int64, UINT32_MAX);
            break;
        case Width64:
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    }
    case TypeKind::I32:
        switch (accessWidth) {
        case Width8:
            maximum = constant(Int32, UINT8_MAX);
            break;
        case Width16:
            maximum = constant(Int32, UINT16_MAX);
            break;
        case Width32:
        case Width64:
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    BasicBlock* failureCase = m_proc.addBlock();
    BasicBlock* successCase = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    auto condition = m_currentBlock->appendNew<Value>(m_proc, Above, origin(), expected, maximum);
    m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(), condition, FrequentedBlock(failureCase, FrequencyClass::Rare), FrequentedBlock(successCase, FrequencyClass::Normal));
    failureCase->addPredecessor(m_currentBlock);
    successCase->addPredecessor(m_currentBlock);

    m_currentBlock = successCase;
    B3::UpsilonValue* successValue = nullptr;
    {
        auto truncatedExpected = expected;
        auto truncatedValue = value;
        if (valueType.isI64()) {
            truncatedExpected = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), expected);
            truncatedValue = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), value);
        }

        auto result = m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicStrongCAS), origin(), accessWidth, truncatedExpected, truncatedValue, pointer);
        successValue = m_currentBlock->appendNew<B3::UpsilonValue>(m_proc, origin(), result);
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
        continuation->addPredecessor(m_currentBlock);
    }

    m_currentBlock = failureCase;
    B3::UpsilonValue* failureValue = nullptr;
    {
        Value* addingValue = nullptr;
        switch (accessWidth) {
        case Width8:
        case Width16:
        case Width32:
            addingValue = constant(Int32, 0);
            break;
        case Width64:
            addingValue = constant(Int64, 0);
            break;
        case Width128:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        auto result = m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicXchgAdd), origin(), accessWidth, addingValue, pointer);
        failureValue = m_currentBlock->appendNew<B3::UpsilonValue>(m_proc, origin(), result);
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
        continuation->addPredecessor(m_currentBlock);
    }

    m_currentBlock = continuation;
    Value* phi = continuation->appendNew<Value>(m_proc, Phi, accessWidth == Width64 ? Int64 : Int32, origin());
    successValue->setPhi(phi);
    failureValue->setPhi(phi);
    return sanitizeAtomicResult(op, valueType, phi);
}

void B3IRGenerator::emitStructSet(Value* structValue, uint32_t fieldIndex, const StructType& structType, ExpressionType& argument)
{
    Value* payloadBase = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int64, origin(), structValue, JSWebAssemblyStruct::offsetOfPayload());
    int32_t fieldOffset = fixupPointerPlusOffset(payloadBase, *structType.getFieldOffset(fieldIndex));
    const auto& fieldType = structType.field(fieldIndex).type;
    switch (fieldType.kind) {
    case TypeKind::I32:
    case TypeKind::Funcref:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::I64:
    case TypeKind::F32:
    case TypeKind::F64: {
        if (isRefType(fieldType)) {
            Value* instance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfOwner()));
            emitWriteBarrier(structValue, instance);
        }
        m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Store), origin(), get(argument), payloadBase, fieldOffset);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

auto B3IRGenerator::atomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, ExpressionType& result, uint32_t offset) -> PartialResult
{
    ASSERT(pointer->type() == Int32);

    if (UNLIKELY(sumOverflows<uint32_t>(offset, sizeOfAtomicOpMemoryAccess(op)))) {
        // FIXME: Even though this is provably out of bounds, it's not a validation error, so we have to handle it
        // as a runtime exception. However, this may change: https://bugs.webkit.org/show_bug.cgi?id=166435
        B3::PatchpointValue* throwException = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, origin());
        throwException->setGenerator([this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });

        switch (valueType.kind) {
        case TypeKind::I32:
            result = push(constant(Int32, 0));
            break;
        case TypeKind::I64:
            result = push(constant(Int64, 0));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = push(emitAtomicCompareExchange(op, valueType, emitCheckAndPreparePointer(get(pointer), offset, sizeOfAtomicOpMemoryAccess(op)), get(expected), get(value), offset));

    return { };
}

auto B3IRGenerator::atomicWait(ExtAtomicOpType op, ExpressionType pointerVar, ExpressionType valueVar, ExpressionType timeoutVar, ExpressionType& result, uint32_t offset) -> PartialResult
{
    Value* pointer = get(pointerVar);
    Value* value = get(valueVar);
    Value* timeout = get(timeoutVar);
    Value* resultValue = nullptr;
    if (op == ExtAtomicOpType::MemoryAtomicWait32) {
        resultValue = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
            m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationMemoryAtomicWait32)),
            instanceValue(), pointer, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), offset), value, timeout);
    } else {
        resultValue = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
            m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationMemoryAtomicWait64)),
            instanceValue(), pointer, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), offset), value, timeout);
    }

    {
        result = push(resultValue);
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto B3IRGenerator::atomicNotify(ExtAtomicOpType, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t offset) -> PartialResult
{
    Value* resultValue = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationMemoryAtomicNotify)),
        instanceValue(), get(pointer), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), offset), get(count));
    {
        result = push(resultValue);
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), resultValue, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto B3IRGenerator::atomicFence(ExtAtomicOpType, uint8_t) -> PartialResult
{
    m_currentBlock->appendNew<FenceValue>(m_proc, origin());
    return { };
}

auto B3IRGenerator::truncSaturated(Ext1OpType op, ExpressionType argVar, ExpressionType& result, Type returnType, Type) -> PartialResult
{
    Value* arg = get(argVar);
    Value* maxFloat = nullptr;
    Value* minFloat = nullptr;
    Value* signBitConstant = nullptr;
    bool requiresMacroScratchRegisters = false;
    switch (op) {
    case Ext1OpType::I32TruncSatF32S:
        maxFloat = constant(Float, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
        minFloat = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));
        break;
    case Ext1OpType::I32TruncSatF32U:
        maxFloat = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
        minFloat = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
        break;
    case Ext1OpType::I32TruncSatF64S:
        maxFloat = constant(Double, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
        minFloat = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0));
        break;
    case Ext1OpType::I32TruncSatF64U:
        maxFloat = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
        minFloat = constant(Double, bitwise_cast<uint64_t>(-1.0));
        break;
    case Ext1OpType::I64TruncSatF32S:
        maxFloat = constant(Float, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
        minFloat = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));
        break;
    case Ext1OpType::I64TruncSatF32U:
        maxFloat = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
        minFloat = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
        // Since x86 doesn't have an instruction to convert floating points to unsigned integers, we at least try to do the smart thing if
        // the numbers would be positive anyway as a signed integer. Since we cannot materialize constants into fprs we have b3 do it
        // so we can pool them if needed.
        if (isX86())
            signBitConstant = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));
        requiresMacroScratchRegisters = true;
        break;
    case Ext1OpType::I64TruncSatF64S:
        maxFloat = constant(Double, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
        minFloat = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));
        break;
    case Ext1OpType::I64TruncSatF64U:
        maxFloat = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
        minFloat = constant(Double, bitwise_cast<uint64_t>(-1.0));
        // Since x86 doesn't have an instruction to convert floating points to unsigned integers, we at least try to do the smart thing if
        // the numbers are would be positive anyway as a signed integer. Since we cannot materialize constants into fprs we have b3 do it
        // so we can pool them if needed.
        if (isX86())
            signBitConstant = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));
        requiresMacroScratchRegisters = true;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, toB3Type(returnType), origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    if (requiresMacroScratchRegisters) {
        if (isX86()) {
            ASSERT(signBitConstant);
            patchpoint->append(signBitConstant, ValueRep::SomeRegister);
            patchpoint->numFPScratchRegisters = 1;
        }
        patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    }
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        switch (op) {
        case Ext1OpType::I32TruncSatF32S:
            jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I32TruncSatF32U:
            jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I32TruncSatF64S:
            jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I32TruncSatF64U:
            jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I64TruncSatF32S:
            jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I64TruncSatF32U: {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            ASSERT(requiresMacroScratchRegisters);
            FPRReg scratch = InvalidFPRReg;
            FPRReg constant = InvalidFPRReg;
            if (isX86()) {
                scratch = params.fpScratch(0);
                constant = params[2].fpr();
            }
            jit.truncateFloatToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
            break;
        }
        case Ext1OpType::I64TruncSatF64S:
            jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
            break;
        case Ext1OpType::I64TruncSatF64U: {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            ASSERT(requiresMacroScratchRegisters);
            FPRReg scratch = InvalidFPRReg;
            FPRReg constant = InvalidFPRReg;
            if (isX86()) {
                scratch = params.fpScratch(0);
                constant = params[2].fpr();
            }
            jit.truncateDoubleToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    });
    patchpoint->effects = Effects::none();

    Value* maxResult = nullptr;
    Value* minResult = nullptr;
    Value* zero = nullptr;
    bool requiresNaNCheck = false;
    switch (op) {
    case Ext1OpType::I32TruncSatF32S:
    case Ext1OpType::I32TruncSatF64S:
        maxResult = constant(Int32, bitwise_cast<uint32_t>(INT32_MAX));
        minResult = constant(Int32, bitwise_cast<uint32_t>(INT32_MIN));
        zero = constant(Int32, 0);
        requiresNaNCheck = true;
        break;
    case Ext1OpType::I32TruncSatF32U:
    case Ext1OpType::I32TruncSatF64U:
        maxResult = constant(Int32, bitwise_cast<uint32_t>(UINT32_MAX));
        minResult = constant(Int32, bitwise_cast<uint32_t>(0U));
        break;
    case Ext1OpType::I64TruncSatF32S:
    case Ext1OpType::I64TruncSatF64S:
        maxResult = constant(Int64, bitwise_cast<uint64_t>(INT64_MAX));
        minResult = constant(Int64, bitwise_cast<uint64_t>(INT64_MIN));
        zero = constant(Int64, 0);
        requiresNaNCheck = true;
        break;
    case Ext1OpType::I64TruncSatF32U:
    case Ext1OpType::I64TruncSatF64U:
        maxResult = constant(Int64, bitwise_cast<uint64_t>(UINT64_MAX));
        minResult = constant(Int64, bitwise_cast<uint64_t>(0ULL));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, minFloat),
        m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, maxFloat),
            patchpoint, maxResult),
        requiresNaNCheck ? m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), arg, arg), minResult, zero) : minResult));

    return { };
}

auto B3IRGenerator::addI31New(ExpressionType value, ExpressionType& result) -> PartialResult
{
    Value* i64 = m_currentBlock->appendNew<Value>(m_proc, B3::ZExt32, origin(), get(value));
    Value* truncated = m_currentBlock->appendNew<Value>(m_proc, B3::BitAnd, origin(), i64, constant(Int64, 0x7fffffff));
    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::BitOr, origin(), truncated, constant(Int64, JSValue::NumberTag)));

    return { };
}

auto B3IRGenerator::addI31GetS(ExpressionType ref, ExpressionType& result) -> PartialResult
{
    // Trap on null reference.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), get(ref), m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull()))));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::NullI31Get);
        });
    }

    Value* truncated = m_currentBlock->appendNew<Value>(m_proc, B3::Trunc, origin(), get(ref));
    Value* shiftLeft = m_currentBlock->appendNew<Value>(m_proc, B3::Shl, origin(), truncated, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 1));
    Value* shiftRight = m_currentBlock->appendNew<Value>(m_proc, B3::SShr, origin(), shiftLeft, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 1));
    result = push(shiftRight);

    return { };
}

auto B3IRGenerator::addI31GetU(ExpressionType ref, ExpressionType& result) -> PartialResult
{
    // Trap on null reference.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), get(ref), m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull()))));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::NullI31Get);
        });
    }

    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::Trunc, origin(), get(ref)));
    return { };
}

auto B3IRGenerator::addArrayNew(uint32_t typeIndex, ExpressionType size, ExpressionType value, ExpressionType& result) -> PartialResult
{
#if ASSERT_ENABLED
    Wasm::TypeDefinition& arraySignature = m_info.typeSignatures[typeIndex];
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::Type elementType = arraySignature.as<ArrayType>()->elementType().type;
    ASSERT(toB3Type(elementType) == value->type());
#endif

    Value* initValue = get(value);
    if (value->type() == B3::Float || value->type() == B3::Double) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, B3::Int64, origin());
        patchpoint->appendSomeRegister(initValue);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.moveDoubleTo64(params[1].fpr(), params[0].gpr());
        });
        initValue = patchpoint;
    }

    // FIXME: Emit this inline.
    // https://bugs.webkit.org/show_bug.cgi?id=245405
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::Arrayref), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmArrayNew)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), typeIndex),
        get(size), initValue));

    return { };
}

auto B3IRGenerator::addArrayNewDefault(uint32_t typeIndex, ExpressionType size, ExpressionType& result) -> PartialResult
{
    // FIXME: Abstract and generalize with addArrayNew.
    Wasm::TypeDefinition& arraySignature = m_info.typeSignatures[typeIndex];
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::Type elementType = arraySignature.as<ArrayType>()->elementType().type;

    Value* initValue;
    if (Wasm::isRefType(elementType))
        initValue = m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull()));
    else
        initValue = m_currentBlock->appendNew<Const64Value>(m_proc, origin(), 0);

    // FIXME: Emit this inline.
    // https://bugs.webkit.org/show_bug.cgi?id=245405
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::Arrayref), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmArrayNew)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), typeIndex),
        get(size), initValue));

    return { };
}

auto B3IRGenerator::addArrayGet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType& result) -> PartialResult
{
    Wasm::TypeDefinition& arraySignature = m_info.typeSignatures[typeIndex];
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::Type elementType = arraySignature.as<ArrayType>()->elementType().type;

    // Ensure arrayref is non-null.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), get(arrayref), m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull()))));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::NullArrayGet);
        });
    }

    // Check array bounds.
    Value* arraySize = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(),
        get(arrayref), safeCast<int32_t>(JSWebAssemblyArray::offsetOfSize()));
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, AboveEqual, origin(), get(index), arraySize));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsArrayGet);
        });
    }

    // FIXME: Emit this inline.
    // https://bugs.webkit.org/show_bug.cgi?id=245405
    Value* arrayResult = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmArrayGet)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), typeIndex),
        get(arrayref), get(index));

    switch (toB3Type(elementType).kind()) {
    case B3::Float:
    case B3::Double: {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, toB3Type(elementType), origin());
        patchpoint->appendSomeRegister(arrayResult);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.move64ToDouble(params[1].gpr(), params[0].fpr());
        });
        result = push(patchpoint);
        break;
    }
    case B3::Int32: {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, toB3Type(elementType), origin());
        patchpoint->appendSomeRegister(arrayResult);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.move(params[1].gpr(), params[0].gpr());
        });
        result = push(patchpoint);
        break;
    }
    case B3::Int64:
        result = push(arrayResult);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return { };
}

auto B3IRGenerator::addArraySet(uint32_t typeIndex, ExpressionType arrayref, ExpressionType index, ExpressionType value) -> PartialResult
{
#if ASSERT_ENABLED
    Wasm::TypeDefinition& arraySignature = m_info.typeSignatures[typeIndex];
    ASSERT(arraySignature.is<ArrayType>());
#endif

    // Ensure arrayref is non-null.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), get(arrayref), m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull()))));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::NullArraySet);
        });
    }

    // Check array bounds.
    Value* arraySize = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(),
        get(arrayref), safeCast<int32_t>(JSWebAssemblyArray::offsetOfSize()));
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, AboveEqual, origin(), get(index), arraySize));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsArraySet);
        });
    }

    Value* setValue = get(value);
    if (value->type() == B3::Float || value->type() == B3::Double) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, B3::Int64, origin());
        patchpoint->appendSomeRegister(setValue);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.moveDoubleTo64(params[1].fpr(), params[0].gpr());
        });
        setValue = patchpoint;
    }

    // FIXME: Emit this inline.
    // https://bugs.webkit.org/show_bug.cgi?id=245405
    m_currentBlock->appendNew<CCallValue>(m_proc, B3::Void, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmArraySet)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), typeIndex),
        get(arrayref), get(index), setValue);

    return { };
}

auto B3IRGenerator::addArrayLen(ExpressionType arrayref, ExpressionType& result) -> PartialResult
{
    // Ensure arrayref is non-null.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), get(arrayref), m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull()))));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::NullArrayLen);
        });
    }

    result = push(m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), get(arrayref), safeCast<int32_t>(JSWebAssemblyArray::offsetOfSize())));

    return { };
}

auto B3IRGenerator::addStructNew(uint32_t typeIndex, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    const auto type = Type { TypeKind::Ref, m_info.typeSignatures[typeIndex]->index() };

    // FIXME: inline the allocation.
    // https://bugs.webkit.org/show_bug.cgi?id=244388
    Value* structValue = m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(type), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmStructNewEmpty)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), typeIndex));

    const auto& structType = *m_info.typeSignatures[typeIndex]->template as<StructType>();
    for (uint32_t i = 0; i < args.size(); ++i)
        emitStructSet(structValue, i, structType, args[i]);

    result = push(structValue);

    return { };
}

auto B3IRGenerator::addStructGet(ExpressionType structReference, const StructType& structType, uint32_t fieldIndex, ExpressionType& result) -> PartialResult
{
    Value* payloadBase = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), pointerType(), origin(), get(structReference), JSWebAssemblyStruct::offsetOfPayload());
    int32_t fieldOffset = fixupPointerPlusOffset(payloadBase, *structType.getFieldOffset(fieldIndex));
    switch (structType.field(fieldIndex).type.kind) {
    case TypeKind::I32:
        result = push(m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int32, origin(), payloadBase, fieldOffset));
        break;
    case TypeKind::Funcref:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::I64:
        result = push(m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Int64, origin(), payloadBase, fieldOffset));
        break;
    case TypeKind::F32: {
        result = push(m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Float, origin(), payloadBase, fieldOffset));
        break;
    }
    case TypeKind::F64:
        result = push(m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load), Double, origin(), payloadBase, fieldOffset));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return { };
}

auto B3IRGenerator::addStructSet(ExpressionType structReference, const StructType& structType, uint32_t fieldIndex, ExpressionType value) -> PartialResult
{
    emitStructSet(get(structReference), fieldIndex, structType, value);
    return { };
}

auto B3IRGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    result = push(m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), get(condition), get(nonZero), get(zero)));
    return { };
}

B3IRGenerator::ExpressionType B3IRGenerator::addConstant(Type type, uint64_t value)
{
    return push(constant(toB3Type(type), value));
}

void B3IRGenerator::emitEntryTierUpCheck()
{
    if (!m_tierUp)
        return;

    ASSERT(m_tierUp);
    Value* countDownLocation = constant(pointerType(), bitwise_cast<uintptr_t>(&m_tierUp->m_counter), Origin());

    PatchpointValue* patch = m_currentBlock->appendNew<PatchpointValue>(m_proc, B3::Void, Origin());
    Effects effects = Effects::none();
    // FIXME: we should have a more precise heap range for the tier up count.
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    patch->effects = effects;
    patch->clobber(RegisterSetBuilder::macroClobberedRegisters());

    patch->append(countDownLocation, ValueRep::SomeRegister);
    patch->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::functionEntryIncrement()), CCallHelpers::Address(params[0].gpr()));
        CCallHelpers::Label tierUpResume = jit.label();

        params.addLatePath([=, this] (CCallHelpers& jit) {
            tierUp.link(&jit);

            const unsigned extraPaddingBytes = 0;
            RegisterSet registersToSpill = { };
            registersToSpill.add(GPRInfo::argumentGPR1, IgnoreVectors);
            unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraPaddingBytes);

            jit.move(MacroAssembler::TrustedImm32(m_functionIndex), GPRInfo::argumentGPR1);
            MacroAssembler::Call call = jit.nearCall();

            ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToSpill, { }, numberOfStackBytesUsedForRegisterPreservation, extraPaddingBytes);
            jit.jump(tierUpResume);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                MacroAssembler::repatchNearCall(linkBuffer.locationOfNearCall<NoPtrTag>(call), CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(triggerOMGEntryTierUpThunkGenerator).code()));
            });
        });
    });
}

void B3IRGenerator::emitLoopTierUpCheck(uint32_t loopIndex, const Stack& enclosingStack, const Stack& newStack)
{
    uint32_t outerLoopIndex = this->outerLoopIndex();
    m_outerLoops.append(loopIndex);

    if (!m_tierUp)
        return;

    Origin origin = this->origin();
    ASSERT(m_tierUp->osrEntryTriggers().size() == loopIndex);
    m_tierUp->osrEntryTriggers().append(TierUpCount::TriggerReason::DontTrigger);
    m_tierUp->outerLoops().append(outerLoopIndex);

    Value* countDownLocation = constant(pointerType(), bitwise_cast<uintptr_t>(&m_tierUp->m_counter), origin);

    Vector<Value*> stackmap;
    for (auto& local : m_locals)
        stackmap.append(get(local));
    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        auto& data = m_parser->controlStack()[controlIndex].controlData;
        auto& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        for (TypedExpression value : expressionStack)
            stackmap.append(get(value));
        if (ControlType::isAnyCatch(data))
            stackmap.append(get(data.exception()));
    }
    for (TypedExpression value : enclosingStack)
        stackmap.append(get(value));
    for (TypedExpression value : newStack)
        stackmap.append(get(value));

    PatchpointValue* patch = m_currentBlock->appendNew<PatchpointValue>(m_proc, B3::Void, origin);
    Effects effects = Effects::none();
    // FIXME: we should have a more precise heap range for the tier up count.
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    effects.exitsSideways = true;
    patch->effects = effects;

    patch->clobber(RegisterSetBuilder::macroClobberedRegisters());
    RegisterSet clobberLate;
    clobberLate.add(GPRInfo::argumentGPR0, IgnoreVectors);
    patch->clobberLate(clobberLate);

    patch->append(countDownLocation, ValueRep::SomeRegister);
    patch->appendVectorWithRep(stackmap, ValueRep::ColdAny);

    TierUpCount::TriggerReason* forceEntryTrigger = &(m_tierUp->osrEntryTriggers().last());
    static_assert(!static_cast<uint8_t>(TierUpCount::TriggerReason::DontTrigger), "the JIT code assumes non-zero means 'enter'");
    static_assert(sizeof(TierUpCount::TriggerReason) == 1, "branchTest8 assumes this size");
    patch->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        CCallHelpers::Jump forceOSREntry = jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::AbsoluteAddress(forceEntryTrigger));
        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::loopIncrement()), CCallHelpers::Address(params[0].gpr()));
        MacroAssembler::Label tierUpResume = jit.label();

        // First argument is the countdown location.
        ASSERT(params.value()->numChildren() >= 1);
        StackMap values(params.value()->numChildren() - 1);
        for (unsigned i = 1; i < params.value()->numChildren(); ++i)
            values[i - 1] = OSREntryValue(params[i], params.value()->child(i)->type());

        OSREntryData& osrEntryData = m_tierUp->addOSREntryData(m_functionIndex, loopIndex, WTFMove(values));
        OSREntryData* osrEntryDataPtr = &osrEntryData;

        params.addLatePath([=] (CCallHelpers& jit) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            forceOSREntry.link(&jit);
            tierUp.link(&jit);

            jit.probe(tagCFunction<JITProbePtrTag>(operationWasmTriggerOSREntryNow), osrEntryDataPtr);
            jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::argumentGPR0).linkTo(tierUpResume, &jit);
            jit.farJump(GPRInfo::argumentGPR1, WasmEntryPtrTag);
        });
    });
}

Value* B3IRGenerator::loadFromScratchBuffer(unsigned& indexInBuffer, Value* pointer, B3::Type type)
{
    size_t offset = sizeof(uint64_t) * indexInBuffer++;
    RELEASE_ASSERT(type.isNumeric());
    return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, type, origin(), pointer, offset);
}

void B3IRGenerator::connectControlAtEntrypoint(unsigned& indexInBuffer, Value* pointer, ControlData& data, Stack& expressionStack, ControlData& currentData, bool fillLoopPhis)
{
    for (unsigned i = 0; i < expressionStack.size(); i++) {
        TypedExpression value = expressionStack[i];
        auto* load = loadFromScratchBuffer(indexInBuffer, pointer, value->type());
        if (fillLoopPhis)
            m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), load, data.phis[i]);
        else
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, origin(), value.value(), load);
    }
    if (ControlType::isAnyCatch(data) && &data != &currentData) {
        auto* load = loadFromScratchBuffer(indexInBuffer, pointer, pointerType());
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, origin(), data.exception(), load);
    }
};

auto B3IRGenerator::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex) -> PartialResult
{
    BasicBlock* body = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    block = ControlData(m_proc, origin(), signature, BlockType::Loop, m_stackSize, continuation, body);

    unsigned offset = enclosingStack.size() - signature->as<FunctionSignature>()->argumentCount();
    for (unsigned i = 0; i < signature->as<FunctionSignature>()->argumentCount(); ++i) {
        TypedExpression value = enclosingStack.at(offset + i);
        Value* phi = block.phis[i];
        m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), get(value), phi);
        body->append(phi);
        set(body, value, phi);
        newStack.append(value);
    }
    enclosingStack.shrink(offset);

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), body);
    if (loopIndex == m_loopIndexForOSREntry) {
        dataLogLnIf(WasmB3IRGeneratorInternal::verbose, "Setting up for OSR entry");

        m_currentBlock = m_rootBlocks[0];
        Value* pointer = m_rootBlocks[0]->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR0);

        unsigned indexInBuffer = 0;

        for (auto& local : m_locals)
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, loadFromScratchBuffer(indexInBuffer, pointer, local->type()));

        for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
            auto& data = m_parser->controlStack()[controlIndex].controlData;
            auto& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
            connectControlAtEntrypoint(indexInBuffer, pointer, data, expressionStack, block);
        }
        connectControlAtEntrypoint(indexInBuffer, pointer, block, enclosingStack, block);
        connectControlAtEntrypoint(indexInBuffer, pointer, block, newStack, block, true);

        m_osrEntryScratchBufferSize = indexInBuffer;
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), body);
        body->addPredecessor(m_currentBlock);
    }

    m_currentBlock = body;
    emitLoopTierUpCheck(loopIndex, enclosingStack, newStack);
    return { };
}

B3IRGenerator::ControlData B3IRGenerator::addTopLevel(BlockSignature signature)
{
    return ControlData(m_proc, Origin(), signature, BlockType::TopLevel, m_stackSize, m_proc.addBlock());
}

auto B3IRGenerator::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> PartialResult
{
    BasicBlock* continuation = m_proc.addBlock();

    splitStack(signature, enclosingStack, newStack);
    newBlock = ControlData(m_proc, origin(), signature, BlockType::Block, m_stackSize, continuation);
    return { };
}

auto B3IRGenerator::addIf(ExpressionType condition, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    // FIXME: This needs to do some kind of stack passing.

    BasicBlock* taken = m_proc.addBlock();
    BasicBlock* notTaken = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();
    FrequencyClass takenFrequency = FrequencyClass::Normal;
    FrequencyClass notTakenFrequency = FrequencyClass::Normal;

    if (Options::useWebAssemblyBranchHints()) {
        BranchHint hint = m_info.getBranchHint(m_functionIndex, m_parser->currentOpcodeStartingOffset());

        switch (hint) {
        case BranchHint::Unlikely:
            takenFrequency = FrequencyClass::Rare;
            break;
        case BranchHint::Likely:
            notTakenFrequency = FrequencyClass::Rare;
            break;
        case BranchHint::Invalid:
            break;
        }
    }

    m_currentBlock->appendNew<Value>(m_proc, B3::Branch, origin(), get(condition));
    m_currentBlock->setSuccessors(FrequentedBlock(taken, takenFrequency), FrequentedBlock(notTaken, notTakenFrequency));
    taken->addPredecessor(m_currentBlock);
    notTaken->addPredecessor(m_currentBlock);

    m_currentBlock = taken;
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(m_proc, origin(), signature, BlockType::If, m_stackSize, continuation, notTaken);
    return { };
}

auto B3IRGenerator::addElse(ControlData& data, const Stack& currentStack) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    return addElseToUnreachable(data);
}

auto B3IRGenerator::addElseToUnreachable(ControlData& data) -> PartialResult
{
    ASSERT(data.blockType() == BlockType::If);
    m_stackSize = data.stackSize() + data.m_signature->as<FunctionSignature>()->argumentCount();
    m_currentBlock = data.special;
    data.convertIfToBlock();
    return { };
}

auto B3IRGenerator::addTry(BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    ++m_tryCatchDepth;

    BasicBlock* continuation = m_proc.addBlock();
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(m_proc, origin(), signature, BlockType::Try, m_stackSize, continuation, ++m_callSiteIndex, m_tryCatchDepth);
    return { };
}

auto B3IRGenerator::addCatch(unsigned exceptionIndex, const TypeDefinition& signature, Stack& currentStack, ControlType& data, ResultList& results) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    return addCatchToUnreachable(exceptionIndex, signature, data, results);
}

PatchpointExceptionHandle B3IRGenerator::preparePatchpointForExceptions(BasicBlock* block, PatchpointValue* patch)
{
    ++m_callSiteIndex;
    if (!m_tryCatchDepth)
        return { m_hasExceptionHandlers };

    Vector<Value*> liveValues;
    Origin origin = this->origin();
    for (Variable* local : m_locals) {
        Value* result = block->appendNew<VariableValue>(m_proc, B3::Get, origin, local);
        liveValues.append(result);
    }
    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        ControlData& data = m_parser->controlStack()[controlIndex].controlData;
        Stack& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        for (Variable* value : expressionStack)
            liveValues.append(get(block, value));
        if (ControlType::isAnyCatch(data))
            liveValues.append(get(block, data.exception()));
    }

    patch->effects.exitsSideways = true;
    patch->appendVectorWithRep(liveValues, ValueRep::LateColdAny);

    return { m_hasExceptionHandlers, m_callSiteIndex, static_cast<unsigned>(liveValues.size()) };
}

auto B3IRGenerator::addCatchToUnreachable(unsigned exceptionIndex, const TypeDefinition& signature, ControlType& data, ResultList& results) -> PartialResult
{
    Value* operationResult = emitCatchImpl(CatchKind::Catch, data, exceptionIndex);
    Value* payload = m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), pointerType(), operationResult, 1);
    for (unsigned i = 0; i < signature.as<FunctionSignature>()->argumentCount(); ++i) {
        Type type = signature.as<FunctionSignature>()->argumentType(i);
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(type), origin(), payload, i * sizeof(uint64_t));
        results.append(push(value));
    }
    return { };
}

auto B3IRGenerator::addCatchAll(Stack& currentStack, ControlType& data) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    return addCatchAllToUnreachable(data);
}

auto B3IRGenerator::addCatchAllToUnreachable(ControlType& data) -> PartialResult
{
    emitCatchImpl(CatchKind::CatchAll, data);
    return { };
}

Value* B3IRGenerator::emitCatchImpl(CatchKind kind, ControlType& data, unsigned exceptionIndex)
{
    m_currentBlock = m_proc.addBlock();
    m_rootBlocks.append(m_currentBlock);
    m_stackSize = data.stackSize();

    if (ControlType::isTry(data)) {
        if (kind == CatchKind::Catch)
            data.convertTryToCatch(++m_callSiteIndex, m_proc.addVariable(pointerType()));
        else
            data.convertTryToCatchAll(++m_callSiteIndex, m_proc.addVariable(pointerType()));
    }
    // We convert from "try" to "catch" ControlType above. This doesn't
    // happen if ControlType is already a "catch". This can happen when
    // we have multiple catches like "try {} catch(A){} catch(B){}...CatchAll(E){}"
    ASSERT(ControlType::isAnyCatch(data));

    HandlerType handlerType = kind == CatchKind::Catch ? HandlerType::Catch : HandlerType::CatchAll;
    m_exceptionHandlers.append({ handlerType, data.tryStart(), data.tryEnd(), 0, m_tryCatchDepth, exceptionIndex });

    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, instanceValue(), m_proc, m_currentBlock, false);

    Value* pointer = m_currentBlock->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR0);

    unsigned indexInBuffer = 0;

    for (auto& local : m_locals)
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, loadFromScratchBuffer(indexInBuffer, pointer, local->type()));

    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        auto& controlData = m_parser->controlStack()[controlIndex].controlData;
        auto& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        connectControlAtEntrypoint(indexInBuffer, pointer, controlData, expressionStack, data);
    }

    PatchpointValue* result = m_currentBlock->appendNew<PatchpointValue>(m_proc, m_proc.addTuple({ pointerType(), pointerType() }), origin());
    result->effects.exitsSideways = true;
    result->clobber(RegisterSetBuilder::macroClobberedRegisters());
    auto clobberLate = RegisterSetBuilder::registersToSaveForJSCall(RegisterSetBuilder::allScalarRegisters());
    clobberLate.add(GPRInfo::argumentGPR0, IgnoreVectors);
    result->clobberLate(clobberLate);
    result->append(instanceValue(), ValueRep::SomeRegister);
    result->resultConstraints.append(ValueRep::reg(GPRInfo::returnValueGPR));
    result->resultConstraints.append(ValueRep::reg(GPRInfo::returnValueGPR2));
    result->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.move(params[2].gpr(), GPRInfo::argumentGPR0);
        CCallHelpers::Call call = jit.call(OperationPtrTag);
        jit.addLinkTask([call] (LinkBuffer& linkBuffer) {
            linkBuffer.link<OperationPtrTag>(call, operationWasmRetrieveAndClearExceptionIfCatchable);
        });
    });

    Value* exception = m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), pointerType(), result, 0);
    set(data.exception(), exception);

    return result;
}

auto B3IRGenerator::addDelegate(ControlType& target, ControlType& data) -> PartialResult
{
    return addDelegateToUnreachable(target, data);
}

auto B3IRGenerator::addDelegateToUnreachable(ControlType& target, ControlType& data) -> PartialResult
{
    unsigned targetDepth = 0;
    if (ControlType::isTry(target))
        targetDepth = target.tryDepth();

    m_exceptionHandlers.append({ HandlerType::Delegate, data.tryStart(), ++m_callSiteIndex, 0, m_tryCatchDepth, targetDepth });
    return { };
}

auto B3IRGenerator::addThrow(unsigned exceptionIndex, Vector<ExpressionType>& args, Stack&) -> PartialResult
{
    PatchpointValue* patch = m_proc.add<PatchpointValue>(B3::Void, origin());
    patch->effects.terminal = true;
    patch->append(instanceValue(), ValueRep::reg(GPRInfo::argumentGPR0));
    patch->append(framePointer(), ValueRep::reg(GPRInfo::argumentGPR1));
    for (unsigned i = 0; i < args.size(); ++i)
        patch->append(get(args[i]), ValueRep::stackArgument(i * sizeof(EncodedJSValue)));
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(RegisterSetBuilder::allScalarRegisters()));
    PatchpointExceptionHandle handle = preparePatchpointForExceptions(m_currentBlock, patch);
    patch->setGenerator([this, exceptionIndex, handle] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        handle.generate(jit, params, this);
        emitThrowImpl(jit, exceptionIndex); 
    });
    m_currentBlock->append(patch);

    return { };
}

auto B3IRGenerator::addRethrow(unsigned, ControlType& data) -> PartialResult
{
    PatchpointValue* patch = m_proc.add<PatchpointValue>(B3::Void, origin());
    patch->clobber(RegisterSetBuilder::registersToSaveForJSCall(RegisterSetBuilder::allScalarRegisters()));
    patch->effects.terminal = true;
    patch->append(instanceValue(), ValueRep::reg(GPRInfo::argumentGPR0));
    patch->append(framePointer(), ValueRep::reg(GPRInfo::argumentGPR1));
    patch->append(get(data.exception()), ValueRep::reg(GPRInfo::argumentGPR2));
    PatchpointExceptionHandle handle = preparePatchpointForExceptions(m_currentBlock, patch);
    patch->setGenerator([this, handle] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        handle.generate(jit, params, this);
        emitRethrowImpl(jit);
    });
    m_currentBlock->append(patch);

    return { };
}

auto B3IRGenerator::addReturn(const ControlData&, const Stack& returnValues) -> PartialResult
{
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(m_parser->signature(), CallRole::Callee);

    PatchpointValue* patch = m_proc.add<PatchpointValue>(B3::Void, origin());
    patch->setGenerator([] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
        auto calleeSaves = params.code().calleeSaveRegisterAtOffsetList();
        jit.emitRestore(calleeSaves);
        jit.emitFunctionEpilogue();
        jit.ret();
    });
    patch->effects.terminal = true;

    RELEASE_ASSERT(returnValues.size() >= wasmCallInfo.results.size());
    unsigned offset = returnValues.size() - wasmCallInfo.results.size();
    for (unsigned i = 0; i < wasmCallInfo.results.size(); ++i) {
        B3::ValueRep rep = wasmCallInfo.results[i].location;
        if (rep.isStack()) {
            B3::Value* address = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Add, Origin(), framePointer(), constant(pointerType(), rep.offsetFromFP()));
            m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, Origin(), get(returnValues[offset + i]), address);
        } else {
            ASSERT(rep.isReg());
            patch->append(get(returnValues[offset + i]), rep);
        }
    }

    m_currentBlock->append(patch);
    return { };
}

auto B3IRGenerator::addBranch(ControlData& data, ExpressionType condition, const Stack& returnValues) -> PartialResult
{
    unifyValuesWithBlock(returnValues, data);

    BasicBlock* target = data.targetBlockForBranch();
    FrequencyClass targetFrequency = FrequencyClass::Normal;
    FrequencyClass continuationFrequency = FrequencyClass::Normal;

    if (Options::useWebAssemblyBranchHints()) {
        BranchHint hint = m_info.getBranchHint(m_functionIndex, m_parser->currentOpcodeStartingOffset());

        switch (hint) {
        case BranchHint::Unlikely:
            targetFrequency = FrequencyClass::Rare;
            break;
        case BranchHint::Likely:
            continuationFrequency = FrequencyClass::Rare;
            break;
        case BranchHint::Invalid:
            break;
        }
    }

    if (condition) {
        BasicBlock* continuation = m_proc.addBlock();
        m_currentBlock->appendNew<Value>(m_proc, B3::Branch, origin(), get(condition));
        m_currentBlock->setSuccessors(FrequentedBlock(target, targetFrequency), FrequentedBlock(continuation, continuationFrequency));
        target->addPredecessor(m_currentBlock);
        continuation->addPredecessor(m_currentBlock);
        m_currentBlock = continuation;
    } else {
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), FrequentedBlock(target, targetFrequency));
        target->addPredecessor(m_currentBlock);
    }

    return { };
}

auto B3IRGenerator::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const Stack& expressionStack) -> PartialResult
{
    UNUSED_PARAM(expressionStack);
    for (size_t i = 0; i < targets.size(); ++i)
        unifyValuesWithBlock(expressionStack, *targets[i]);
    unifyValuesWithBlock(expressionStack, defaultTarget);

    SwitchValue* switchValue = m_currentBlock->appendNew<SwitchValue>(m_proc, origin(), get(condition));
    switchValue->setFallThrough(FrequentedBlock(defaultTarget.targetBlockForBranch()));
    for (size_t i = 0; i < targets.size(); ++i)
        switchValue->appendCase(SwitchCase(i, FrequentedBlock(targets[i]->targetBlockForBranch())));

    return { };
}

auto B3IRGenerator::endBlock(ControlEntry& entry, Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;

    ASSERT(expressionStack.size() == data.signature()->as<FunctionSignature>()->returnCount());
    if (data.blockType() != BlockType::Loop)
        unifyValuesWithBlock(expressionStack, data);

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    data.continuation->addPredecessor(m_currentBlock);

    return addEndToUnreachable(entry, expressionStack);
}

auto B3IRGenerator::addEndToUnreachable(ControlEntry& entry, const Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;
    m_currentBlock = data.continuation;
    m_stackSize = data.stackSize();

    if (data.blockType() == BlockType::If) {
        data.special->appendNewControlValue(m_proc, Jump, origin(), m_currentBlock);
        m_currentBlock->addPredecessor(data.special);
    } else if (data.blockType() == BlockType::Try || data.blockType() == BlockType::Catch)
        --m_tryCatchDepth;

    if (data.blockType() != BlockType::Loop) {
        for (unsigned i = 0; i < data.signature()->as<FunctionSignature>()->returnCount(); ++i) {
            Value* result = data.phis[i];
            m_currentBlock->append(result);
            entry.enclosedExpressionStack.constructAndAppend(data.signature()->as<FunctionSignature>()->returnType(i), push(result));
        }
    } else {
        m_outerLoops.removeLast();
        for (unsigned i = 0; i < data.signature()->as<FunctionSignature>()->returnCount(); ++i) {
            if (i < expressionStack.size()) {
                ++m_stackSize;
                entry.enclosedExpressionStack.append(expressionStack[i]);
            } else {
                Type returnType = data.signature()->as<FunctionSignature>()->returnType(i);
                entry.enclosedExpressionStack.constructAndAppend(returnType, push(constant(toB3Type(returnType), 0xbbadbeef)));
            }
        }
    }


    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.blockType() == BlockType::TopLevel)
        return addReturn(entry.controlData, entry.enclosedExpressionStack);

    return { };
}


B3::Value* B3IRGenerator::createCallPatchpoint(BasicBlock* block, Origin origin, const TypeDefinition& signature, Vector<ExpressionType>& args, const ScopedLambda<void(PatchpointValue*, Box<PatchpointExceptionHandle>)>& patchpointFunctor)
{
    Vector<B3::ConstrainedValue> constrainedArguments;
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature);
    for (unsigned i = 0; i < args.size(); ++i)
        constrainedArguments.append(B3::ConstrainedValue(get(block, args[i]), wasmCallInfo.params[i]));

    m_proc.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCallInfo.headerAndArgumentStackSizeInBytes));

    Box<PatchpointExceptionHandle> exceptionHandle = Box<PatchpointExceptionHandle>::create(m_hasExceptionHandlers);

    B3::Type returnType = toB3ResultType(&signature);
    PatchpointValue* patchpoint = m_proc.add<PatchpointValue>(returnType, origin);
    patchpoint->clobberEarly(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->clobberLate(RegisterSetBuilder::registersToSaveForJSCall(RegisterSetBuilder::allScalarRegisters()));
    patchpointFunctor(patchpoint, exceptionHandle);
    patchpoint->appendVector(constrainedArguments);

    *exceptionHandle = preparePatchpointForExceptions(block, patchpoint);

    if (returnType != B3::Void) {
        Vector<B3::ValueRep, 1> resultConstraints;
        for (auto valueLocation : wasmCallInfo.results)
            resultConstraints.append(B3::ValueRep(valueLocation.location));
        patchpoint->resultConstraints = WTFMove(resultConstraints);
    }
    block->append(patchpoint);
    return patchpoint;
}

auto B3IRGenerator::addCall(uint32_t functionIndex, const TypeDefinition& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());

    m_makesCalls = true;
    B3::Type returnType = toB3ResultType(&signature);

    auto fillResults = [&] (Value* callResult) {
        ASSERT(returnType == callResult->type());

        switch (returnType.kind()) {
        case B3::Void: {
            break;
        }
        case B3::Tuple: {
            const Vector<B3::Type>& tuple = m_proc.tupleForType(returnType);
            ASSERT(signature.as<FunctionSignature>()->returnCount() == tuple.size());
            for (unsigned i = 0; i < signature.as<FunctionSignature>()->returnCount(); ++i)
                results.append(push(m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), tuple[i], callResult, i)));
            break;
        }
        default: {
            results.append(push(callResult));
            break;
        }
        }
    };

    Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {
        m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

        // FIXME: imports can be linked here, instead of generating a patchpoint, because all import stubs are generated before B3 compilation starts. https://bugs.webkit.org/show_bug.cgi?id=166462
        Value* targetInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfTargetInstance(functionIndex)));
        // The target instance is 0 unless the call is wasm->wasm.
        Value* isWasmCall = m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), targetInstance, m_currentBlock->appendNew<Const64Value>(m_proc, origin(), 0));

        BasicBlock* isWasmBlock = m_proc.addBlock();
        BasicBlock* isEmbedderBlock = m_proc.addBlock();
        BasicBlock* continuation = m_proc.addBlock();
        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(), isWasmCall, FrequentedBlock(isWasmBlock), FrequentedBlock(isEmbedderBlock));

        Value* wasmCallResult = createCallPatchpoint(isWasmBlock, origin(), signature, args,
            scopedLambdaRef<void(PatchpointValue*, Box<PatchpointExceptionHandle>)>([=, this] (PatchpointValue* patchpoint, Box<PatchpointExceptionHandle> handle) -> void {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;
                // We need to clobber all potential pinned registers since we might be leaving the instance.
                // We pessimistically assume we could be calling to something that is bounds checking.
                // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
                patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
                patchpoint->setGenerator([this, handle, unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    handle->generate(jit, params, this);
                    CCallHelpers::Call call = jit.threadSafePatchableNearCall();
                    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
                    });
                });
            }));
        UpsilonValue* wasmCallResultUpsilon = returnType == B3::Void ? nullptr : isWasmBlock->appendNew<UpsilonValue>(m_proc, origin(), wasmCallResult);
        isWasmBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);

        // FIXME: Let's remove this indirection by creating a PIC friendly IC
        // for calls out to the embedder. This shouldn't be that hard to do. We could probably
        // implement the IC to be over Context*.
        // https://bugs.webkit.org/show_bug.cgi?id=170375
        Value* jumpDestination = isEmbedderBlock->appendNew<MemoryValue>(m_proc,
            Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfWasmToEmbedderStub(functionIndex)));

        Value* embedderCallResult = createCallPatchpoint(isEmbedderBlock, origin(), signature, args,
            scopedLambdaRef<void(PatchpointValue*, Box<PatchpointExceptionHandle>)>([=, this] (PatchpointValue* patchpoint, Box<PatchpointExceptionHandle> handle) -> void {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;
                patchpoint->append(jumpDestination, ValueRep::SomeRegister);
                // We need to clobber all potential pinned registers since we might be leaving the instance.
                // We pessimistically assume we could be calling to something that is bounds checking.
                // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
                patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
                patchpoint->setGenerator([this, handle, returnType] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    handle->generate(jit, params, this);
                    jit.call(params[params.proc().resultCount(returnType)].gpr(), WasmEntryPtrTag);
                });
            }));
        UpsilonValue* embedderCallResultUpsilon = returnType == B3::Void ? nullptr : isEmbedderBlock->appendNew<UpsilonValue>(m_proc, origin(), embedderCallResult);
        isEmbedderBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);

        m_currentBlock = continuation;

        if (returnType != B3::Void) {
            Value* phi = continuation->appendNew<Value>(m_proc, Phi, returnType, origin());
            wasmCallResultUpsilon->setPhi(phi);
            embedderCallResultUpsilon->setPhi(phi);
            fillResults(phi);
        }

        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, instanceValue(), m_proc, continuation);
    } else {

        Value* patch = createCallPatchpoint(m_currentBlock, origin(), signature, args,
            scopedLambdaRef<void(PatchpointValue*, Box<PatchpointExceptionHandle>)>([=, this] (PatchpointValue* patchpoint, Box<PatchpointExceptionHandle> handle) -> void {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;

                // We need to clobber the size register since the LLInt always bounds checks
                if (useSignalingMemory() || m_info.memory.isShared())
                    patchpoint->clobberLate(RegisterSetBuilder { PinnedRegisterInfo::get().boundsCheckingSizeRegister });
                patchpoint->setGenerator([this, handle, unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    handle->generate(jit, params, this);
                    CCallHelpers::Call call = jit.threadSafePatchableNearCall();
                    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall<WasmEntryPtrTag>(call), functionIndex });
                    });
                });
            }));
        fillResults(patch);
    }

    return { };
}

auto B3IRGenerator::addCallIndirect(unsigned tableIndex, const TypeDefinition& originalSignature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    Value* calleeIndex = get(args.takeLast());
    const TypeDefinition& signature = originalSignature.expand();
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());

    m_makesCalls = true;
    // Note: call indirect can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    Value* callableFunctionBuffer;
    Value* instancesBuffer;
    Value* callableFunctionBufferLength;
    {
        Value* table = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
            instanceValue(), safeCast<int32_t>(Instance::offsetOfTablePtr(m_numImportFunctions, tableIndex)));
        callableFunctionBuffer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
            table, safeCast<int32_t>(FuncRefTable::offsetOfFunctions()));
        instancesBuffer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
            table, safeCast<int32_t>(FuncRefTable::offsetOfInstances()));
        callableFunctionBufferLength = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(),
            table, safeCast<int32_t>(Table::offsetOfLength()));
    }

    // Check the index we are looking for is valid.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, AboveEqual, origin(), calleeIndex, callableFunctionBufferLength));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsCallIndirect);
        });
    }

    calleeIndex = m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), calleeIndex);

    Value* callableFunction;
    {
        // Compute the offset in the table index space we are looking for.
        Value* offset = m_currentBlock->appendNew<Value>(m_proc, Mul, origin(),
            calleeIndex, constant(pointerType(), sizeof(WasmToWasmImportableFunction)));
        callableFunction = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), callableFunctionBuffer, offset);

        // Check that the WasmToWasmImportableFunction is initialized. We trap if it isn't. An "invalid" SignatureIndex indicates it's not initialized.
        // FIXME: when we have trap handlers, we can just let the call fail because Signature::invalidIndex is 0. https://bugs.webkit.org/show_bug.cgi?id=177210
        static_assert(sizeof(WasmToWasmImportableFunction::typeIndex) == sizeof(uint64_t), "Load codegen assumes i64");
        Value* calleeSignatureIndex = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), callableFunction, safeCast<int32_t>(WasmToWasmImportableFunction::offsetOfSignatureIndex()));
        {
            CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Equal, origin(),
                    calleeSignatureIndex,
                    m_currentBlock->appendNew<Const64Value>(m_proc, origin(), TypeDefinition::invalidIndex)));

            check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitExceptionCheck(jit, ExceptionType::NullTableEntry);
            });
        }

        // Check the signature matches the value we expect.
        {
            Value* expectedSignatureIndex = m_currentBlock->appendNew<Const64Value>(m_proc, origin(), TypeInformation::get(originalSignature));
            CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
                m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), calleeSignatureIndex, expectedSignatureIndex));

            check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitExceptionCheck(jit, ExceptionType::BadSignature);
            });
        }
    }

    Value* offset = m_currentBlock->appendNew<Value>(m_proc, Mul, origin(),
        calleeIndex, constant(pointerType(), sizeof(Instance*)));
    Value* calleeInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<Value>(m_proc, Add, origin(), instancesBuffer, offset));
    Value* calleeCode = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callableFunction,
            safeCast<int32_t>(WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation())));

    return emitIndirectCall(calleeInstance, calleeCode, signature, args, results);
}

auto B3IRGenerator::addCallRef(const TypeDefinition& originalSignature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    Value* callee = get(args.takeLast());
    const TypeDefinition& signature = originalSignature.expand();
    ASSERT(signature.as<FunctionSignature>()->argumentCount() == args.size());
    m_makesCalls = true;

    // Note: call ref can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    // Check the target reference for null.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), callee, m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull()))));
        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::NullReference);
        });
    }

    Value* jsInstanceOffset = constant(pointerType(), safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfInstance()));
    Value* jsCalleeInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<Value>(m_proc, Add, origin(), callee, jsInstanceOffset));

    Value* instanceOffset = constant(pointerType(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfInstance()));
    Value* calleeInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<Value>(m_proc, Add, origin(), jsCalleeInstance, instanceOffset));

    Value* calleeCode = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callee,
            safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfEntrypointLoadLocation())));

    return emitIndirectCall(calleeInstance, calleeCode, signature, args, results);
}

void B3IRGenerator::unify(Value* phi, const ExpressionType source)
{
    m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), get(source), phi);
}

void B3IRGenerator::unifyValuesWithBlock(const Stack& resultStack, const ControlData& block)
{
    const Vector<Value*>& phis = block.phis;
    size_t resultSize = phis.size();

    ASSERT(resultSize <= resultStack.size());

    for (size_t i = 0; i < resultSize; ++i)
        unify(phis[resultSize - 1 - i], resultStack.at(resultStack.size() - 1 - i));
}

static void dumpExpressionStack(const CommaPrinter& comma, const B3IRGenerator::Stack& expressionStack)
{
    dataLog(comma, "ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, *expression);
}

void B3IRGenerator::dump(const ControlStack& controlStack, const Stack* expressionStack)
{
    dataLogLn("Constants:");
    for (const auto& constant : m_constantPool)
        dataLogLn(deepDump(m_proc, constant.value));

    dataLogLn("Processing Graph:");
    dataLog(m_proc);
    dataLogLn("With current block:", *m_currentBlock);
    dataLogLn("Control stack:");
    ASSERT(controlStack.size());
    for (size_t i = controlStack.size(); i--;) {
        dataLog("  ", controlStack[i].controlData, ": ");
        CommaPrinter comma(", ", "");
        dumpExpressionStack(comma, *expressionStack);
        expressionStack = &controlStack[i].enclosedExpressionStack;
        dataLogLn();
    }
    dataLogLn();
}

auto B3IRGenerator::origin() -> Origin
{
    OpcodeOrigin origin(m_parser->currentOpcode(), m_parser->currentOpcodeStartingOffset());
    ASSERT(isValidOpType(static_cast<uint8_t>(origin.opcode())));
    return bitwise_cast<Origin>(origin);
}

static bool shouldDumpIRFor(uint32_t functionIndex)
{
    static LazyNeverDestroyed<FunctionAllowlist> dumpAllowlist;
    static std::once_flag initializeAllowlistFlag;
    std::call_once(initializeAllowlistFlag, [] {
        const char* functionAllowlistFile = Options::wasmB3FunctionsToDump();
        dumpAllowlist.construct(functionAllowlistFile);
    });
    return dumpAllowlist->shouldDumpWasmFunction(functionIndex);
}

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompileB3(CompilationContext& compilationContext, const FunctionData& function, const TypeDefinition& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, CompilationMode compilationMode, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, uint32_t loopIndexForOSREntry, TierUpCount* tierUp)
{
    auto result = makeUnique<InternalFunction>();

    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();
    compilationContext.procedure = makeUnique<Procedure>();

    Procedure& procedure = *compilationContext.procedure;
    if (shouldDumpIRFor(functionIndex + info.importFunctionCount()))
        procedure.setShouldDumpIR();


    if (Options::useSamplingProfiler()) {
        // FIXME: We should do this based on VM relevant info.
        // But this is good enough for our own profiling for now.
        // When we start to show this data in web inspector, we'll
        // need other hooks into this besides the JSC option.
        procedure.setNeedsPCToOriginMap();
    }

    procedure.setOriginPrinter([] (PrintStream& out, Origin origin) {
        if (origin.data())
            out.print("Wasm: ", OpcodeOrigin(origin));
    });
    
    // This means we cannot use either StackmapGenerationParams::usedRegisters() or
    // StackmapGenerationParams::unavailableRegisters(). In exchange for this concession, we
    // don't strictly need to run Air::reportUsedRegisters(), which saves a bit of CPU time at
    // optLevel=1.
    procedure.setNeedsUsedRegisters(false);
    
    procedure.setOptLevel(isAnyBBQ(compilationMode)
        ? Options::webAssemblyBBQB3OptimizationLevel()
        : Options::webAssemblyOMGOptimizationLevel());

    procedure.code().setForceIRCRegisterAllocation();

    B3IRGenerator irGenerator(info, procedure, result.get(), unlinkedWasmToWasmCalls, result->osrEntryScratchBufferSize, mode, compilationMode, functionIndex, hasExceptionHandlers, loopIndexForOSREntry, tierUp);
    FunctionParser<B3IRGenerator> parser(irGenerator, function.data.data(), function.data.size(), signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    irGenerator.insertEntrySwitch();
    irGenerator.insertConstants();

    procedure.resetReachability();
    if (ASSERT_ENABLED)
        validate(procedure, "After parsing:\n");

    estimateStaticExecutionCounts(procedure);

    dataLogIf(WasmB3IRGeneratorInternal::verbose, "Pre SSA: ", procedure);
    fixSSA(procedure);
    dataLogIf(WasmB3IRGeneratorInternal::verbose, "Post SSA: ", procedure);
    
    {
        if (shouldDumpDisassemblyFor(compilationMode))
            procedure.code().setDisassembler(makeUnique<B3::Air::Disassembler>());
        B3::prepareForGeneration(procedure);
        B3::generate(procedure, *compilationContext.wasmEntrypointJIT);
        compilationContext.wasmEntrypointByproducts = procedure.releaseByproducts();
        result->entrypoint.calleeSaveRegisters = procedure.calleeSaveRegisterAtOffsetList();
    }

    result->stackmaps = irGenerator.takeStackmaps();
    result->exceptionHandlers = irGenerator.takeExceptionHandlers();

    return result;
}

void computePCToCodeOriginMap(CompilationContext& context, LinkBuffer& linkBuffer)
{
    if (context.procedure && context.procedure->needsPCToOriginMap()) {
        B3::PCToOriginMap originMap = context.procedure->releasePCToOriginMap();
        context.pcToCodeOriginMap = Box<PCToCodeOriginMap>::create(PCToCodeOriginMapBuilder(PCToCodeOriginMapBuilder::WasmCodeOriginMap, WTFMove(originMap)), linkBuffer);
    }
}

// Custom wasm ops. These are the ones too messy to do in wasm.json.

void B3IRGenerator::emitChecksForModOrDiv(B3::Opcode operation, Value* left, Value* right)
{
    ASSERT(operation == Div || operation == Mod || operation == UDiv || operation == UMod);
    const B3::Type type = left->type();

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), right, constant(type, 0)));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::DivisionByZero);
        });
    }

    if (operation == Div) {
        int64_t min = type == Int32 ? std::numeric_limits<int32_t>::min() : std::numeric_limits<int64_t>::min();

        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), left, constant(type, min)),
                m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), right, constant(type, -1))));

        check->setGenerator([=, this] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::IntegerOverflow);
        });
    }
}

template<>
auto B3IRGenerator::addOp<OpType::I32DivS>(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Div;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32RemS>(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Mod;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, chill(op), origin(), left, right));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32DivU>(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UDiv;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32RemU>(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UMod;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64DivS>(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Div;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64RemS>(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Mod;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, chill(op), origin(), left, right));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64DivU>(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UDiv;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64RemU>(ExpressionType leftVar, ExpressionType rightVar, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UMod;
    Value* left = get(leftVar);
    Value* right = get(rightVar);
    emitChecksForModOrDiv(op, left, right);
    result = push(m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32Ctz>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64Ctz>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros64(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32Popcnt>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
#if CPU(X86_64)
    if (MacroAssembler::supportsCountPopulation()) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
        patchpoint->append(arg, ValueRep::SomeRegister);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.countPopulation32(params[1].gpr(), params[0].gpr());
        });
        patchpoint->effects = Effects::none();
        result = push(patchpoint);
        return { };
    }
#endif

    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationPopcount32));
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(), Effects::none(), funcAddress, arg));
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64Popcnt>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
#if CPU(X86_64)
    if (MacroAssembler::supportsCountPopulation()) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
        patchpoint->append(arg, ValueRep::SomeRegister);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.countPopulation64(params[1].gpr(), params[0].gpr());
        });
        patchpoint->effects = Effects::none();
        result = push(patchpoint);
        return { };
    }
#endif

    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationPopcount64));
    result = push(m_currentBlock->appendNew<CCallValue>(m_proc, Int64, origin(), Effects::none(), funcAddress, arg));
    return { };
}

template<>
auto B3IRGenerator::addOp<F64ConvertUI64>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->append(ConstrainedValue(arg, ValueRep::SomeRegister));
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
#if CPU(X86_64)
        jit.convertUInt64ToDouble(params[1].gpr(), params[0].fpr(), params.gpScratch(0));
#else
        jit.convertUInt64ToDouble(params[1].gpr(), params[0].fpr());
#endif
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32ConvertUI64>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->append(ConstrainedValue(arg, ValueRep::SomeRegister));
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
#if CPU(X86_64)
        jit.convertUInt64ToFloat(params[1].gpr(), params[0].fpr(), params.gpScratch(0));
#else
        jit.convertUInt64ToFloat(params[1].gpr(), params[0].fpr());
#endif
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F64Nearest>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardNearestIntDouble(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32Nearest>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardNearestIntFloat(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F64Trunc>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardZeroDouble(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32Trunc>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardZeroFloat(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncSF64>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Double, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
    Value* min = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncSF32>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Float, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
    Value* min = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}


template<>
auto B3IRGenerator::addOp<OpType::I32TruncUF64>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
    Value* min = constant(Double, bitwise_cast<uint64_t>(-1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncUF32>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
    Value* min = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncSF64>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Double, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
    Value* min = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncUF64>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
    Value* min = constant(Double, bitwise_cast<uint64_t>(-1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });

    Value* signBitConstant;
    if (isX86()) {
        // Since x86 doesn't have an instruction to convert floating points to unsigned integers, we at least try to do the smart thing if
        // the numbers are would be positive anyway as a signed integer. Since we cannot materialize constants into fprs we have b3 do it
        // so we can pool them if needed.
        signBitConstant = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));
    }
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    if (isX86()) {
        patchpoint->append(signBitConstant, ValueRep::SomeRegister);
        patchpoint->numFPScratchRegisters = 1;
    }
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        FPRReg scratch = InvalidFPRReg;
        FPRReg constant = InvalidFPRReg;
        if (isX86()) {
            scratch = params.fpScratch(0);
            constant = params[2].fpr();
        }
        jit.truncateDoubleToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncSF32>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Float, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
    Value* min = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncUF32>(ExpressionType argVar, ExpressionType& result) -> PartialResult
{
    Value* arg = get(argVar);
    Value* max = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
    Value* min = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=, this] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });

    Value* signBitConstant;
    if (isX86()) {
        // Since x86 doesn't have an instruction to convert floating points to unsigned integers, we at least try to do the smart thing if
        // the numbers would be positive anyway as a signed integer. Since we cannot materialize constants into fprs we have b3 do it
        // so we can pool them if needed.
        signBitConstant = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<uint64_t>::max() - std::numeric_limits<int64_t>::max())));
    }
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    if (isX86()) {
        patchpoint->append(signBitConstant, ValueRep::SomeRegister);
        patchpoint->numFPScratchRegisters = 1;
    }
    patchpoint->clobber(RegisterSetBuilder::macroClobberedRegisters());
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        FPRReg scratch = InvalidFPRReg;
        FPRReg constant = InvalidFPRReg;
        if (isX86()) {
            scratch = params.fpScratch(0);
            constant = params[2].fpr();
        }
        jit.truncateFloatToUint64(params[1].fpr(), params[0].gpr(), scratch, constant);
    });
    patchpoint->effects = Effects::none();
    result = push(patchpoint);
    return { };
}

} } // namespace JSC::Wasm

#include "WasmB3IRGeneratorInlines.h"

#endif // ENABLE(WEBASSEMBLY_B3JIT)
