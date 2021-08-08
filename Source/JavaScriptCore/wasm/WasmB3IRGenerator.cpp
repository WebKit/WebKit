/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "ScratchRegisterAllocator.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmExceptionType.h"
#include "WasmFunctionParser.h"
#include "WasmInstance.h"
#include "WasmMemory.h"
#include "WasmOSREntryData.h"
#include "WasmOpcodeOrigin.h"
#include "WasmOperations.h"
#include "WasmSignatureInlines.h"
#include "WasmThunks.h"
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
    using ExpressionType = Value*;
    using ResultList = Vector<ExpressionType, 8>;

    struct ControlData {
        ControlData(Procedure& proc, Origin origin, BlockSignature signature, BlockType type, BasicBlock* continuation, BasicBlock* special = nullptr)
            : controlBlockType(type)
            , m_signature(signature)
            , continuation(continuation)
            , special(special)
        {
            if (type == BlockType::Loop) {
                for (unsigned i = 0; i < signature->argumentCount(); ++i)
                    phis.append(proc.add<Value>(Phi, toB3Type(signature->argument(i)), origin));
            } else {
                for (unsigned i = 0; i < signature->returnCount(); ++i)
                    phis.append(proc.add<Value>(Phi, toB3Type(signature->returnType(i)), origin));
            }
        }

        ControlData()
        {
        }

        static bool isIf(const ControlData& control) { return control.blockType() == BlockType::If; }
        static bool isTopLevel(const ControlData& control) { return control.blockType() == BlockType::TopLevel; }

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
            out.print("Continuation: ", *continuation, ", Special: ");
            if (special)
                out.print(*special);
            else
                out.print("None");
        }

        BlockType blockType() const { return controlBlockType; }

        BlockSignature signature() const { return m_signature; }

        bool hasNonVoidresult() const { return m_signature->returnsVoid(); }

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

        SignatureArgCount branchTargetArity() const
        {
            if (blockType() == BlockType::Loop)
                return m_signature->argumentCount();
            return m_signature->returnCount();
        }

        Type branchTargetType(unsigned i) const
        {
            ASSERT(i < branchTargetArity());
            if (blockType() == BlockType::Loop)
                return m_signature->argument(i);
            return m_signature->returnType(i);
        }

    private:
        friend class B3IRGenerator;
        BlockType controlBlockType;
        BlockSignature m_signature;
        BasicBlock* continuation;
        BasicBlock* special;
        ResultList phis;
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

    B3IRGenerator(const ModuleInformation&, Procedure&, InternalFunction*, Vector<UnlinkedWasmToWasmCall>&, unsigned& osrEntryScratchBufferSize, MemoryMode, CompilationMode, unsigned functionIndex, unsigned loopIndexForOSREntry, TierUpCount*);

    PartialResult WARN_UNUSED_RETURN addArguments(const Signature&);
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

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const Stack& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, Stack& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&, const Stack& = { });

    PartialResult WARN_UNUSED_RETURN endTopLevel(BlockSignature, const Stack&) { return { }; }

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const Signature&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(unsigned tableIndex, const Signature&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addCallRef(const Signature&, Vector<ExpressionType>& args, ResultList& results);
    PartialResult WARN_UNUSED_RETURN addUnreachable();
    PartialResult WARN_UNUSED_RETURN emitIndirectCall(Value* calleeInstance, ExpressionType calleeCode, const Signature&, Vector<ExpressionType>& args, ResultList&);
    B3::Value* createCallPatchpoint(BasicBlock*, Origin, const Signature&, Vector<ExpressionType>& args, const ScopedLambda<void(PatchpointValue*)>& patchpointFunctor);

    void dump(const ControlStack&, const Stack* expressionStack);
    void setParser(FunctionParser<B3IRGenerator>* parser) { m_parser = parser; };
    void didFinishParsingLocals() { }
    void didPopValueFromStack() { }

    Value* constant(B3::Type, uint64_t bits, std::optional<Origin> = std::nullopt);
    Value* framePointer();
    void insertConstants();

    B3::Type toB3ResultType(BlockSignature);

private:
    void emitExceptionCheck(CCallHelpers&, ExceptionType);

    void emitEntryTierUpCheck();
    void emitLoopTierUpCheck(uint32_t loopIndex, const Stack& enclosingStack, const Stack& newStack);

    void emitWriteBarrierForJSWrapper();
    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    B3::Kind memoryKind(B3::Opcode memoryOp);
    ExpressionType emitLoadOp(LoadOpType, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    ExpressionType sanitizeAtomicResult(ExtAtomicOpType, Type, ExpressionType result);
    ExpressionType emitAtomicLoadOp(ExtAtomicOpType, Type, ExpressionType pointer, uint32_t offset);
    void emitAtomicStoreOp(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicBinaryRMWOp(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType value, uint32_t offset);
    ExpressionType emitAtomicCompareExchange(ExtAtomicOpType, Type, ExpressionType pointer, ExpressionType expected, ExpressionType value, uint32_t offset);

    void unify(const ExpressionType phi, const ExpressionType source);
    void unifyValuesWithBlock(const Stack& resultStack, const ResultList& stack);

    void emitChecksForModOrDiv(B3::Opcode, ExpressionType left, ExpressionType right);

    int32_t WARN_UNUSED_RETURN fixupPointerPlusOffset(ExpressionType&, uint32_t);
    ExpressionType WARN_UNUSED_RETURN fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType, ExpressionType, uint32_t);

    void restoreWasmContextInstance(Procedure&, BasicBlock*, Value*);
    enum class RestoreCachedStackLimit { No, Yes };
    void restoreWebAssemblyGlobalState(RestoreCachedStackLimit, const MemoryInformation&, Value* instance, Procedure&, BasicBlock*);

    Origin origin();

    uint32_t outerLoopIndex() const
    {
        if (m_outerLoops.isEmpty())
            return UINT32_MAX;
        return m_outerLoops.last();
    }

    FunctionParser<B3IRGenerator>* m_parser { nullptr };
    const ModuleInformation& m_info;
    const MemoryMode m_mode { MemoryMode::BoundsChecking };
    const CompilationMode m_compilationMode { CompilationMode::BBQMode };
    const unsigned m_functionIndex { UINT_MAX };
    const unsigned m_loopIndexForOSREntry { UINT_MAX };
    TierUpCount* m_tierUp { nullptr };

    Procedure& m_proc;
    BasicBlock* m_rootBlock { nullptr };
    BasicBlock* m_currentBlock { nullptr };
    Vector<uint32_t> m_outerLoops;
    Vector<Variable*> m_locals;
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

    Value* m_instanceValue { nullptr }; // Always use the accessor below to ensure the instance value is materialized when used.
    bool m_usesInstanceValue { false };
    Value* instanceValue()
    {
        m_usesInstanceValue = true;
        return m_instanceValue;
    }

    uint32_t m_maxNumJSCallArguments { 0 };
    unsigned m_numImportFunctions;
};

// Memory accesses in WebAssembly have unsigned 32-bit offsets, whereas they have signed 32-bit offsets in B3.
int32_t B3IRGenerator::fixupPointerPlusOffset(ExpressionType& ptr, uint32_t offset)
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
            patchpoint->clobber(RegisterSet::macroScratchRegisters());
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
    patchpoint->clobberLate(RegisterSet(m_wasmContextInstanceGPR));
    patchpoint->append(arg, ValueRep::SomeRegister);
    GPRReg wasmContextInstanceGPR = m_wasmContextInstanceGPR;
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& param) {
        jit.move(param[0].gpr(), wasmContextInstanceGPR);
    });
}

B3IRGenerator::B3IRGenerator(const ModuleInformation& info, Procedure& procedure, InternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, unsigned& osrEntryScratchBufferSize, MemoryMode mode, CompilationMode compilationMode, unsigned functionIndex, unsigned loopIndexForOSREntry, TierUpCount* tierUp)
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
    , m_numImportFunctions(info.importFunctionCount())
{
    m_rootBlock = m_proc.addBlock();
    m_currentBlock = m_rootBlock;

    // FIXME we don't really need to pin registers here if there's no memory. It makes wasm -> wasm thunks simpler for now. https://bugs.webkit.org/show_bug.cgi?id=166623
    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();

    m_memoryBaseGPR = pinnedRegs.baseMemoryPointer;
    m_proc.pinRegister(m_memoryBaseGPR);

    m_wasmContextInstanceGPR = pinnedRegs.wasmContextInstancePointer;
    if (!Context::useFastTLS())
        m_proc.pinRegister(m_wasmContextInstanceGPR);

    if (mode != MemoryMode::Signaling) {
        m_boundsCheckingSizeGPR = pinnedRegs.boundsCheckingSizeRegister;
        m_proc.pinRegister(m_boundsCheckingSizeGPR);
    }

    if (info.memory) {
        m_proc.setWasmBoundsCheckGenerator([=] (CCallHelpers& jit, GPRReg pinnedGPR) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            switch (m_mode) {
            case MemoryMode::BoundsChecking:
                ASSERT_UNUSED(pinnedGPR, m_boundsCheckingSizeGPR == pinnedGPR);
                break;
            case MemoryMode::Signaling:
                ASSERT_UNUSED(pinnedGPR, InvalidGPRReg == pinnedGPR);
                break;
            }
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    {
        auto* calleeMoveLocation = &compilation->calleeMoveLocation;
        static_assert(CallFrameSlot::codeBlock * sizeof(Register) < WasmCallingConvention::headerSizeInBytes, "We rely on this here for now.");
        static_assert(CallFrameSlot::callee * sizeof(Register) < WasmCallingConvention::headerSizeInBytes, "We rely on this here for now.");
        B3::PatchpointValue* getCalleePatchpoint = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Int64, Origin());
        getCalleePatchpoint->resultConstraints = { B3::ValueRep::SomeRegister };
        getCalleePatchpoint->effects = B3::Effects::none();
        getCalleePatchpoint->setGenerator(
            [=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                GPRReg result = params[0].gpr();
                MacroAssembler::DataLabelPtr moveLocation = jit.moveWithPatch(MacroAssembler::TrustedImmPtr(nullptr), result);
                jit.addLinkTask([calleeMoveLocation, moveLocation] (LinkBuffer& linkBuffer) {
                    *calleeMoveLocation = linkBuffer.locationOf<WasmEntryPtrTag>(moveLocation);
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
        B3::PatchpointValue* stackOverflowCheck = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, pointerType(), Origin());
        m_instanceValue = stackOverflowCheck;
        stackOverflowCheck->appendSomeRegister(framePointer());
        stackOverflowCheck->clobber(RegisterSet::macroScratchRegisters());
        if (!Context::useFastTLS()) {
            // FIXME: Because WasmToWasm call clobbers wasmContextInstance register and does not restore it, we need to restore it in the caller side.
            // This prevents us from using ArgumentReg to this (logically) immutable pinned register.
            stackOverflowCheck->effects.writesPinned = false;
            stackOverflowCheck->effects.readsPinned = true;
            stackOverflowCheck->resultConstraints = { ValueRep::reg(m_wasmContextInstanceGPR) };
        }
        stackOverflowCheck->numGPScratchRegisters = 2;
        stackOverflowCheck->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
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

            GPRReg contextInstance = Context::useFastTLS() ? params[0].gpr() : m_wasmContextInstanceGPR;

            // This allows leaf functions to not do stack checks if their frame size is within
            // certain limits since their caller would have already done the check.
            if (needsOverflowCheck) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                GPRReg fp = params[1].gpr();
                GPRReg scratch1 = params.gpScratch(0);
                GPRReg scratch2 = params.gpScratch(1);

                if (Context::useFastTLS())
                    jit.loadWasmContextInstance(contextInstance);

                jit.loadPtr(CCallHelpers::Address(contextInstance, Instance::offsetOfCachedStackLimit()), scratch2);
                jit.addPtr(CCallHelpers::TrustedImm32(-checkSize), fp, scratch1);
                MacroAssembler::JumpList overflow;
                if (UNLIKELY(needUnderflowCheck))
                    overflow.append(jit.branchPtr(CCallHelpers::Above, scratch1, fp));
                overflow.append(jit.branchPtr(CCallHelpers::Below, scratch1, scratch2));
                jit.addLinkTask([overflow] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(overflow, CodeLocationLabel<JITThunkPtrTag>(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
                });
            } else if (m_usesInstanceValue && Context::useFastTLS()) {
                // No overflow check is needed, but the instance values still needs to be correct.
                AllowMacroScratchRegisterUsageIf allowScratch(jit, CCallHelpers::loadWasmContextInstanceNeedsMacroScratchRegister());
                jit.loadWasmContextInstance(contextInstance);
            } else {
                // We said we'd return a pointer. We don't actually need to because it isn't used, but the patchpoint conservatively said it had effects (potential stack check) which prevent it from getting removed.
            }
        });
    }

    emitEntryTierUpCheck();

    if (m_compilationMode == CompilationMode::OMGForOSREntryMode)
        m_currentBlock = m_proc.addBlock();
}

void B3IRGenerator::restoreWebAssemblyGlobalState(RestoreCachedStackLimit restoreCachedStackLimit, const MemoryInformation& memory, Value* instance, Procedure& proc, BasicBlock* block)
{
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
        clobbers.set(pinnedRegs->baseMemoryPointer);
        clobbers.set(pinnedRegs->boundsCheckingSizeRegister);
        clobbers.set(RegisterSet::macroScratchRegisters());

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

void B3IRGenerator::insertConstants()
{
    m_constantInsertionValues.execute(m_proc.at(0));
}

B3::Type B3IRGenerator::toB3ResultType(BlockSignature returnType)
{
    if (returnType->returnsVoid())
        return B3::Void;

    if (returnType->returnCount() == 1)
        return toB3Type(returnType->returnType(0));

    auto result = m_tupleMap.ensure(returnType, [&] {
        Vector<B3::Type> result;
        for (unsigned i = 0; i < returnType->returnCount(); ++i)
            result.append(toB3Type(returnType->returnType(i)));
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

auto B3IRGenerator::addArguments(const Signature& signature) -> PartialResult
{
    ASSERT(!m_locals.size());
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(signature.argumentCount()), "can't allocate memory for ", signature.argumentCount(), " arguments");

    m_locals.grow(signature.argumentCount());
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature, CallRole::Callee);

    for (size_t i = 0; i < signature.argumentCount(); ++i) {
        B3::Type type = toB3Type(signature.argument(i));
        B3::Value* argument;
        auto rep = wasmCallInfo.params[i];
        if (rep.isReg()) {
            argument = m_currentBlock->appendNew<B3::ArgumentRegValue>(m_proc, Origin(), rep.reg());
            if (type == B3::Int32 || type == B3::Float)
                argument = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Trunc, Origin(), argument);
        } else {
            ASSERT(rep.isStack());
            B3::Value* address = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Add, Origin(), framePointer(),
                m_currentBlock->appendNew<B3::Const64Value>(m_proc, Origin(), rep.offsetFromFP()));
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
    result = m_currentBlock->appendNew<Value>(m_proc, B3::Equal, origin(), value, m_currentBlock->appendNew<Const64Value>(m_proc, origin(), JSValue::encode(jsNull())));
    return { };
}

auto B3IRGenerator::addTableGet(unsigned tableIndex, ExpressionType index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.
    result = m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::Externref), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationGetWasmTableElement)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), index);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), result, m_currentBlock->appendNew<Const64Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), index, value);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), shouldThrow, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addRefFunc(uint32_t index, ExpressionType& result) -> PartialResult
{
    // FIXME: Emit this inline <https://bugs.webkit.org/show_bug.cgi?id=198506>.

    result = m_currentBlock->appendNew<CCallValue>(m_proc, B3::Int64, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmRefFunc)),
        instanceValue(), addConstant(Types::I32, index));

    return { };
}

auto B3IRGenerator::addTableInit(unsigned elementIndex, unsigned tableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    auto result = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmTableInit)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), elementIndex),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex),
        dstOffset, srcOffset, length);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), result, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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
    result = m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationGetWasmTableSize)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex));

    return { };
}

auto B3IRGenerator::addTableGrow(unsigned tableIndex, ExpressionType fill, ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmTableGrow)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), fill, delta);

    return { };
}

auto B3IRGenerator::addTableFill(unsigned tableIndex, ExpressionType offset, ExpressionType fill, ExpressionType count) -> PartialResult
{
    auto result = m_currentBlock->appendNew<CCallValue>(m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmTableFill)),
        instanceValue(), m_currentBlock->appendNew<Const32Value>(m_proc, origin(), tableIndex), offset, fill, count);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), result, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addTableCopy(unsigned dstTableIndex, unsigned srcTableIndex, ExpressionType dstOffset, ExpressionType srcOffset, ExpressionType length) -> PartialResult
{
    auto result = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmTableCopy)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), dstTableIndex),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), srcTableIndex),
        dstOffset, srcOffset, length);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), result, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::getLocal(uint32_t index, ExpressionType& result) -> PartialResult
{
    ASSERT(m_locals[index]);
    result = m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, origin(), m_locals[index]);
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

auto B3IRGenerator::emitIndirectCall(Value* calleeInstance, ExpressionType calleeCode, const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
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
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
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
    ExpressionType callResult = createCallPatchpoint(m_currentBlock, origin(), signature, args,
        scopedLambdaRef<void(PatchpointValue*)>([=] (PatchpointValue* patchpoint) -> void {
            patchpoint->effects.writesPinned = true;
            patchpoint->effects.readsPinned = true;
            // We need to clobber all potential pinned registers since we might be leaving the instance.
            // We pessimistically assume we're always calling something that is bounds checking so
            // because the wasm->wasm thunk unconditionally overrides the size registers.
            // FIXME: We should not have to do this, but the wasm->wasm stub assumes it can
            // use all the pinned registers as scratch: https://bugs.webkit.org/show_bug.cgi?id=172181
            patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));

            patchpoint->append(calleeCode, ValueRep::SomeRegister);
            patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                jit.call(params[params.proc().resultCount(returnType)].gpr(), WasmEntryPtrTag);
            });
        }));

    switch (returnType.kind()) {
    case B3::Void: {
        break;
    }
    case B3::Tuple: {
        const Vector<B3::Type>& tuple = m_proc.tupleForType(returnType);
        for (unsigned i = 0; i < signature.returnCount(); ++i)
            results.append(m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), tuple[i], callResult, i));
        break;
    }
    default: {
        results.append(callResult);
        break;
    }
    }

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::Yes, m_info.memory, instanceValue(), m_proc, m_currentBlock);

    return { };
}

auto B3IRGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationGrowMemory)),
        framePointer(), instanceValue(), delta);

    restoreWebAssemblyGlobalState(RestoreCachedStackLimit::No, m_info.memory, instanceValue(), m_proc, m_currentBlock);

    return { };
}

auto B3IRGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    static_assert(sizeof(std::declval<Memory*>()->size()) == sizeof(uint64_t), "codegen relies on this size");

    Value* memory = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfMemory()));
    Value* handle = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), memory, safeCast<int32_t>(Memory::offsetOfHandle()));
    Value* size = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), handle, safeCast<int32_t>(MemoryHandle::offsetOfSize()));

    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1ull << shiftValue, "This must hold for the code below to be correct.");
    Value* numPages = m_currentBlock->appendNew<Value>(m_proc, ZShr, origin(),
        size, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), shiftValue));

    result = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), numPages);

    return { };
}

auto B3IRGenerator::addMemoryFill(ExpressionType dstAddress, ExpressionType targetValue, ExpressionType count) -> PartialResult
{
    auto result = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmMemoryFill)),
        instanceValue(),
        dstAddress, targetValue, count);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), result, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addMemoryInit(unsigned dataSegmentIndex, ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType length) -> PartialResult
{
    auto result = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmMemoryInit)),
        instanceValue(),
        m_currentBlock->appendNew<Const32Value>(m_proc, origin(), dataSegmentIndex),
        dstAddress, srcAddress, length);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), result, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
        });
    }

    return { };
}

auto B3IRGenerator::addMemoryCopy(ExpressionType dstAddress, ExpressionType srcAddress, ExpressionType count) -> PartialResult
{
    auto result = m_currentBlock->appendNew<CCallValue>(
        m_proc, toB3Type(Types::I32), origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationWasmMemoryCopy)),
        instanceValue(),
        dstAddress, srcAddress, count);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), result, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTableAccess);
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
    m_currentBlock->appendNew<VariableValue>(m_proc, B3::Set, origin(), m_locals[index], value);
    return { };
}

auto B3IRGenerator::getGlobal(uint32_t index, ExpressionType& result) -> PartialResult
{
    const Wasm::GlobalInformation& global = m_info.globals[index];
    Value* globalsArray = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfGlobals()));
    switch (global.bindingMode) {
    case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance:
        result = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(global.type), origin(), globalsArray, safeCast<int32_t>(index * sizeof(Register)));
        break;
    case Wasm::GlobalInformation::BindingMode::Portable: {
        ASSERT(global.mutability == Wasm::GlobalInformation::Mutability::Mutable);
        Value* pointer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, B3::Int64, origin(), globalsArray, safeCast<int32_t>(index * sizeof(Register)));
        result = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(global.type), origin(), pointer);
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
        m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), value, globalsArray, safeCast<int32_t>(index * sizeof(Register)));
        if (isRefType(global.type))
            emitWriteBarrierForJSWrapper();
        break;
    case Wasm::GlobalInformation::BindingMode::Portable: {
        ASSERT(global.mutability == Wasm::GlobalInformation::Mutability::Mutable);
        Value* pointer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, B3::Int64, origin(), globalsArray, safeCast<int32_t>(index * sizeof(Register)));
        m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), value, pointer);
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
    Value* cell = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(Instance::offsetOfOwner()));
    Value* cellState = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, origin(), cell, safeCast<int32_t>(JSCell::cellStateOffset()));
    Value* vm = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), cell, safeCast<int32_t>(JSWebAssemblyInstance::offsetOfVM()));
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

inline Value* B3IRGenerator::emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOperation)
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
    if (m_mode == MemoryMode::Signaling || m_info.memory.isShared())
        return trapping(memoryOp);
    return memoryOp;
}

inline Value* B3IRGenerator::emitLoadOp(LoadOpType op, ExpressionType pointer, uint32_t uoffset)
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

auto B3IRGenerator::load(LoadOpType op, ExpressionType pointer, ExpressionType& result, uint32_t offset) -> PartialResult
{
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
            result = constant(Int32, 0);
            break;
        case LoadOpType::I64Load8S:
        case LoadOpType::I64Load8U:
        case LoadOpType::I64Load16S:
        case LoadOpType::I64Load32U:
        case LoadOpType::I64Load32S:
        case LoadOpType::I64Load:
        case LoadOpType::I64Load16U:
            result = constant(Int64, 0);
            break;
        case LoadOpType::F32Load:
            result = constant(Float, 0);
            break;
        case LoadOpType::F64Load:
            result = constant(Double, 0);
            break;
        }

    } else
        result = emitLoadOp(op, emitCheckAndPreparePointer(pointer, offset, sizeOfLoadOp(op)), offset);

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


inline void B3IRGenerator::emitStoreOp(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
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

auto B3IRGenerator::store(StoreOpType op, ExpressionType pointer, ExpressionType value, uint32_t offset) -> PartialResult
{
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

inline B3::Width accessWidth(ExtAtomicOpType op)
{
    return static_cast<B3::Width>(memoryLog2Alignment(op));
}

inline uint32_t sizeOfAtomicOpMemoryAccess(ExtAtomicOpType op)
{
    return bytesForWidth(accessWidth(op));
}

inline Value* B3IRGenerator::sanitizeAtomicResult(ExtAtomicOpType op, Type valueType, ExpressionType result)
{
    auto sanitize32 = [&](ExpressionType result) {
        switch (accessWidth(op)) {
        case B3::Width8:
            return m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), result, constant(Int32, 0xff));
        case B3::Width16:
            return m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), result, constant(Int32, 0xffff));
        default:
            return result;
        }
    };

    switch (valueType.kind) {
    case TypeKind::I64: {
        if (accessWidth(op) == B3::Width64)
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

Value* B3IRGenerator::fixupPointerPlusOffsetForAtomicOps(ExtAtomicOpType op, ExpressionType ptr, uint32_t offset)
{
    auto pointer = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), ptr, m_currentBlock->appendNew<Const64Value>(m_proc, origin(), offset));
    if (accessWidth(op) != B3::Width8) {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), pointer, constant(pointerType(), sizeOfAtomicOpMemoryAccess(op) - 1)));
        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }
    return pointer;
}

inline Value* B3IRGenerator::emitAtomicLoadOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    ExpressionType value = nullptr;
    switch (accessWidth(op)) {
    case B3::Width8:
    case B3::Width16:
    case B3::Width32:
        value = constant(Int32, 0);
        break;
    case B3::Width64:
        value = constant(Int64, 0);
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
            result = constant(Int32, 0);
            break;
        case TypeKind::I64:
            result = constant(Int64, 0);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = emitAtomicLoadOp(op, valueType, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), offset);

    return { };
}

inline void B3IRGenerator::emitAtomicStoreOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    if (valueType.isI64() && accessWidth(op) != B3::Width64)
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
        emitAtomicStoreOp(op, valueType, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), value, offset);

    return { };
}

inline Value* B3IRGenerator::emitAtomicBinaryRMWOp(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType value, uint32_t uoffset)
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

    if (valueType.isI64() && accessWidth(op) != B3::Width64)
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
            result = constant(Int32, 0);
            break;
        case TypeKind::I64:
            result = constant(Int64, 0);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = emitAtomicBinaryRMWOp(op, valueType, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), value, offset);

    return { };
}

Value* B3IRGenerator::emitAtomicCompareExchange(ExtAtomicOpType op, Type valueType, ExpressionType pointer, ExpressionType expected, ExpressionType value, uint32_t uoffset)
{
    pointer = fixupPointerPlusOffsetForAtomicOps(op, pointer, uoffset);

    B3::Width accessWidth = Wasm::accessWidth(op);

    if (widthForType(toB3Type(valueType)) == accessWidth)
        return sanitizeAtomicResult(op, valueType, m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicStrongCAS), origin(), accessWidth, expected, value, pointer));

    Value* maximum = nullptr;
    switch (valueType.kind) {
    case TypeKind::I64: {
        switch (accessWidth) {
        case B3::Width8:
            maximum = constant(Int64, UINT8_MAX);
            break;
        case B3::Width16:
            maximum = constant(Int64, UINT16_MAX);
            break;
        case B3::Width32:
            maximum = constant(Int64, UINT32_MAX);
            break;
        case B3::Width64:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    }
    case TypeKind::I32:
        switch (accessWidth) {
        case B3::Width8:
            maximum = constant(Int32, UINT8_MAX);
            break;
        case B3::Width16:
            maximum = constant(Int32, UINT16_MAX);
            break;
        case B3::Width32:
        case B3::Width64:
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
        case B3::Width8:
        case B3::Width16:
        case B3::Width32:
            addingValue = constant(Int32, 0);
            break;
        case B3::Width64:
            addingValue = constant(Int64, 0);
            break;
        }
        auto result = m_currentBlock->appendNew<AtomicValue>(m_proc, memoryKind(AtomicXchgAdd), origin(), accessWidth, addingValue, pointer);
        failureValue = m_currentBlock->appendNew<B3::UpsilonValue>(m_proc, origin(), result);
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);
        continuation->addPredecessor(m_currentBlock);
    }

    m_currentBlock = continuation;
    Value* phi = continuation->appendNew<Value>(m_proc, Phi, accessWidth == B3::Width64 ? Int64 : Int32, origin());
    successValue->setPhi(phi);
    failureValue->setPhi(phi);
    return sanitizeAtomicResult(op, valueType, phi);
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
            result = constant(Int32, 0);
            break;
        case TypeKind::I64:
            result = constant(Int64, 0);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    } else
        result = emitAtomicCompareExchange(op, valueType, emitCheckAndPreparePointer(pointer, offset, sizeOfAtomicOpMemoryAccess(op)), expected, value, offset);

    return { };
}

auto B3IRGenerator::atomicWait(ExtAtomicOpType op, ExpressionType pointer, ExpressionType value, ExpressionType timeout, ExpressionType& result, uint32_t offset) -> PartialResult
{
    if (op == ExtAtomicOpType::MemoryAtomicWait32) {
        result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
            m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationMemoryAtomicWait32)),
            instanceValue(), pointer, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), offset), value, timeout);
    } else {
        result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
            m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationMemoryAtomicWait64)),
            instanceValue(), pointer, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), offset), value, timeout);
    }

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), result, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    return { };
}

auto B3IRGenerator::atomicNotify(ExtAtomicOpType, ExpressionType pointer, ExpressionType count, ExpressionType& result, uint32_t offset) -> PartialResult
{
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationMemoryAtomicNotify)),
        instanceValue(), pointer, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), offset), count);

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), result, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
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

auto B3IRGenerator::truncSaturated(Ext1OpType op, ExpressionType arg, ExpressionType& result, Type returnType, Type) -> PartialResult
{
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
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
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

    result = m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, minFloat),
        m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(),
            m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, maxFloat),
            patchpoint, maxResult),
        requiresNaNCheck ? m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), arg, arg), minResult, zero) : minResult);

    return { };
}

auto B3IRGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    result = m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), condition, nonZero, zero);
    return { };
}

B3IRGenerator::ExpressionType B3IRGenerator::addConstant(Type type, uint64_t value)
{

    return constant(toB3Type(type), value);
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
    patch->clobber(RegisterSet::macroScratchRegisters());

    patch->append(countDownLocation, ValueRep::SomeRegister);
    patch->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::functionEntryIncrement()), CCallHelpers::Address(params[0].gpr()));
        CCallHelpers::Label tierUpResume = jit.label();

        params.addLatePath([=] (CCallHelpers& jit) {
            tierUp.link(&jit);

            const unsigned extraPaddingBytes = 0;
            RegisterSet registersToSpill = { };
            registersToSpill.add(GPRInfo::argumentGPR1);
            unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraPaddingBytes);

            jit.move(MacroAssembler::TrustedImm32(m_functionIndex), GPRInfo::argumentGPR1);
            MacroAssembler::Call call = jit.nearCall();

            ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToSpill, RegisterSet(), numberOfStackBytesUsedForRegisterPreservation, extraPaddingBytes);
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

    Vector<ExpressionType> stackmap;
    for (auto& local : m_locals) {
        Value* result = m_currentBlock->appendNew<VariableValue>(m_proc, B3::Get, origin, local);
        stackmap.append(result);
    }
    for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
        auto& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
        for (TypedExpression value : expressionStack)
            stackmap.append(value);
    }
    for (TypedExpression value : enclosingStack)
        stackmap.append(value);
    for (TypedExpression value : newStack)
        stackmap.append(value);

    PatchpointValue* patch = m_currentBlock->appendNew<PatchpointValue>(m_proc, B3::Void, origin);
    Effects effects = Effects::none();
    // FIXME: we should have a more precise heap range for the tier up count.
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    effects.exitsSideways = true;
    patch->effects = effects;

    patch->clobber(RegisterSet::macroScratchRegisters());
    RegisterSet clobberLate;
    clobberLate.add(GPRInfo::argumentGPR0);
    patch->clobberLate(clobberLate);

    patch->append(countDownLocation, ValueRep::SomeRegister);
    patch->appendVectorWithRep(stackmap, ValueRep::ColdAny);

    TierUpCount::TriggerReason* forceEntryTrigger = &(m_tierUp->osrEntryTriggers().last());
    static_assert(!static_cast<uint8_t>(TierUpCount::TriggerReason::DontTrigger), "the JIT code assumes non-zero means 'enter'");
    static_assert(sizeof(TierUpCount::TriggerReason) == 1, "branchTest8 assumes this size");
    patch->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        CCallHelpers::Jump forceOSREntry = jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::AbsoluteAddress(forceEntryTrigger));
        CCallHelpers::Jump tierUp = jit.branchAdd32(CCallHelpers::PositiveOrZero, CCallHelpers::TrustedImm32(TierUpCount::loopIncrement()), CCallHelpers::Address(params[0].gpr()));
        MacroAssembler::Label tierUpResume = jit.label();

        OSREntryData& osrEntryData = m_tierUp->addOSREntryData(m_functionIndex, loopIndex);
        // First argument is the countdown location.
        for (unsigned i = 1; i < params.value()->numChildren(); ++i)
            osrEntryData.values().constructAndAppend(params[i], params.value()->child(i)->type());
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

auto B3IRGenerator::addLoop(BlockSignature signature, Stack& enclosingStack, ControlType& block, Stack& newStack, uint32_t loopIndex) -> PartialResult
{
    BasicBlock* body = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    block = ControlData(m_proc, origin(), signature, BlockType::Loop, continuation, body);
    unsigned offset = enclosingStack.size() - signature->argumentCount();
    for (unsigned i = 0; i < signature->argumentCount(); ++i) {
        TypedExpression value = enclosingStack.at(offset + i);
        auto* upsilon = m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), value);
        Value* phi = block.phis[i];
        body->append(phi);
        upsilon->setPhi(phi);
        newStack.constructAndAppend(value.type(), phi);
    }

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), body);
    if (loopIndex == m_loopIndexForOSREntry) {
        dataLogLnIf(WasmB3IRGeneratorInternal::verbose, "Setting up for OSR entry");

        m_currentBlock = m_rootBlock;
        Value* pointer = m_rootBlock->appendNew<ArgumentRegValue>(m_proc, Origin(), GPRInfo::argumentGPR0);

        unsigned indexInBuffer = 0;
        auto loadFromScratchBuffer = [&] (B3::Type type) {
            size_t offset = sizeof(uint64_t) * indexInBuffer++;
            RELEASE_ASSERT(type.isNumeric());
            return m_currentBlock->appendNew<MemoryValue>(m_proc, Load, type, origin(), pointer, offset);
        };

        for (auto& local : m_locals)
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, loadFromScratchBuffer(local->type()));

        auto connectControlEntry = [&](const ControlData& data, Stack& expressionStack) {
            // For each stack entry enclosed by this loop we need to replace the value with a phi so we can fill it on OSR entry.
            BasicBlock* sourceBlock = nullptr;
            unsigned blockIndex = 0;
            B3::InsertionSet insertionSet(m_proc);
            for (unsigned i = 0; i < expressionStack.size(); i++) {
                TypedExpression value = expressionStack[i];
                if (value->isConstant()) {
                    ++indexInBuffer;
                    continue;
                }

                if (value->owner != sourceBlock) {
                    if (sourceBlock)
                        insertionSet.execute(sourceBlock);
                    ASSERT(insertionSet.isEmpty());
                    dataLogLnIf(WasmB3IRGeneratorInternal::verbose && sourceBlock, "Executed insertion set into: ", *sourceBlock);
                    blockIndex = 0;
                    sourceBlock = value->owner;
                }

                while (sourceBlock->at(blockIndex++) != value)
                    ASSERT(blockIndex < sourceBlock->size());
                ASSERT(sourceBlock->at(blockIndex - 1) == value);

                auto* phi = data.continuation->appendNew<Value>(m_proc, Phi,  value->type(), value->origin());
                expressionStack[i] = TypedExpression { value.type(), phi };
                m_currentBlock->appendNew<UpsilonValue>(m_proc, value->origin(), loadFromScratchBuffer(value->type()), phi);

                auto* sourceUpsilon = m_proc.add<UpsilonValue>(value->origin(), value, phi);
                insertionSet.insertValue(blockIndex, sourceUpsilon);
            }
            if (sourceBlock)
                insertionSet.execute(sourceBlock);
        };

        for (unsigned controlIndex = 0; controlIndex < m_parser->controlStack().size(); ++controlIndex) {
            auto& data = m_parser->controlStack()[controlIndex].controlData;
            auto& expressionStack = m_parser->controlStack()[controlIndex].enclosedExpressionStack;
            connectControlEntry(data, expressionStack);
        }
        for (unsigned i = 0; i < signature->argumentCount(); ++i) {
            TypedExpression value = enclosingStack.at(offset + i);
            Value* phi = block.phis[i];
            m_currentBlock->appendNew<UpsilonValue>(m_proc, value->origin(), loadFromScratchBuffer(value->type()), phi);
        }
        enclosingStack.shrink(offset);
        connectControlEntry(block, enclosingStack);

        m_osrEntryScratchBufferSize = indexInBuffer;
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), body);
        body->addPredecessor(m_currentBlock);
    } else
        enclosingStack.shrink(offset);

    m_currentBlock = body;
    emitLoopTierUpCheck(loopIndex, enclosingStack, newStack);
    return { };
}

B3IRGenerator::ControlData B3IRGenerator::addTopLevel(BlockSignature signature)
{
    return ControlData(m_proc, Origin(), signature, BlockType::TopLevel, m_proc.addBlock());
}

auto B3IRGenerator::addBlock(BlockSignature signature, Stack& enclosingStack, ControlType& newBlock, Stack& newStack) -> PartialResult
{
    BasicBlock* continuation = m_proc.addBlock();

    splitStack(signature, enclosingStack, newStack);
    newBlock = ControlData(m_proc, origin(), signature, BlockType::Block, continuation);
    return { };
}

auto B3IRGenerator::addIf(ExpressionType condition, BlockSignature signature, Stack& enclosingStack, ControlType& result, Stack& newStack) -> PartialResult
{
    // FIXME: This needs to do some kind of stack passing.

    BasicBlock* taken = m_proc.addBlock();
    BasicBlock* notTaken = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    m_currentBlock->appendNew<Value>(m_proc, B3::Branch, origin(), condition);
    m_currentBlock->setSuccessors(FrequentedBlock(taken), FrequentedBlock(notTaken));
    taken->addPredecessor(m_currentBlock);
    notTaken->addPredecessor(m_currentBlock);

    m_currentBlock = taken;
    splitStack(signature, enclosingStack, newStack);
    result = ControlData(m_proc, origin(), signature, BlockType::If, continuation, notTaken);
    return { };
}

auto B3IRGenerator::addElse(ControlData& data, const Stack& currentStack) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.phis);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    return addElseToUnreachable(data);
}

auto B3IRGenerator::addElseToUnreachable(ControlData& data) -> PartialResult
{
    ASSERT(data.blockType() == BlockType::If);
    m_currentBlock = data.special;
    data.convertIfToBlock();
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
        B3::ValueRep rep = wasmCallInfo.results[i];
        if (rep.isStack()) {
            B3::Value* address = m_currentBlock->appendNew<B3::Value>(m_proc, B3::Add, Origin(), framePointer(), constant(pointerType(), rep.offsetFromFP()));
            m_currentBlock->appendNew<B3::MemoryValue>(m_proc, B3::Store, Origin(), returnValues[offset + i], address);
        } else {
            ASSERT(rep.isReg());
            patch->append(returnValues[offset + i], rep);
        }
    }

    m_currentBlock->append(patch);
    return { };
}

auto B3IRGenerator::addBranch(ControlData& data, ExpressionType condition, const Stack& returnValues) -> PartialResult
{
    unifyValuesWithBlock(returnValues, data.phis);

    BasicBlock* target = data.targetBlockForBranch();
    if (condition) {
        BasicBlock* continuation = m_proc.addBlock();
        m_currentBlock->appendNew<Value>(m_proc, B3::Branch, origin(), condition);
        m_currentBlock->setSuccessors(FrequentedBlock(target), FrequentedBlock(continuation));
        target->addPredecessor(m_currentBlock);
        continuation->addPredecessor(m_currentBlock);
        m_currentBlock = continuation;
    } else {
        m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), FrequentedBlock(target));
        target->addPredecessor(m_currentBlock);
    }

    return { };
}

auto B3IRGenerator::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const Stack& expressionStack) -> PartialResult
{
    for (size_t i = 0; i < targets.size(); ++i)
        unifyValuesWithBlock(expressionStack, targets[i]->phis);
    unifyValuesWithBlock(expressionStack, defaultTarget.phis);

    SwitchValue* switchValue = m_currentBlock->appendNew<SwitchValue>(m_proc, origin(), condition);
    switchValue->setFallThrough(FrequentedBlock(defaultTarget.targetBlockForBranch()));
    for (size_t i = 0; i < targets.size(); ++i)
        switchValue->appendCase(SwitchCase(i, FrequentedBlock(targets[i]->targetBlockForBranch())));

    return { };
}

auto B3IRGenerator::endBlock(ControlEntry& entry, Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;

    ASSERT(expressionStack.size() == data.signature()->returnCount());
    if (data.blockType() != BlockType::Loop)
        unifyValuesWithBlock(expressionStack, data.phis);

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    data.continuation->addPredecessor(m_currentBlock);

    return addEndToUnreachable(entry, expressionStack);
}

auto B3IRGenerator::addEndToUnreachable(ControlEntry& entry, const Stack& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;
    m_currentBlock = data.continuation;

    if (data.blockType() == BlockType::If) {
        data.special->appendNewControlValue(m_proc, Jump, origin(), m_currentBlock);
        m_currentBlock->addPredecessor(data.special);
    }

    if (data.blockType() != BlockType::Loop) {
        for (unsigned i = 0; i < data.signature()->returnCount(); ++i) {
            Value* result = data.phis[i];
            m_currentBlock->append(result);
            entry.enclosedExpressionStack.constructAndAppend(data.signature()->returnType(i), result);
        }
    } else {
        m_outerLoops.removeLast();
        for (unsigned i = 0; i < data.signature()->returnCount(); ++i) {
            if (i < expressionStack.size())
                entry.enclosedExpressionStack.append(expressionStack[i]);
            else {
                Type returnType = data.signature()->returnType(i);
                entry.enclosedExpressionStack.constructAndAppend(returnType, constant(toB3Type(returnType), 0xbbadbeef));
            }
        }
    }

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.blockType() == BlockType::TopLevel)
        return addReturn(entry.controlData, entry.enclosedExpressionStack);

    return { };
}


B3::Value* B3IRGenerator::createCallPatchpoint(BasicBlock* block, Origin origin, const Signature& signature, Vector<ExpressionType>& args, const ScopedLambda<void(PatchpointValue*)>& patchpointFunctor)
{
    Vector<B3::ConstrainedValue> constrainedArguments;
    CallInformation wasmCallInfo = wasmCallingConvention().callInformationFor(signature);
    for (unsigned i = 0; i < args.size(); ++i)
        constrainedArguments.append(B3::ConstrainedValue(args[i], wasmCallInfo.params[i]));

    m_proc.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf(stackAlignmentBytes(), wasmCallInfo.headerAndArgumentStackSizeInBytes));

    B3::Type returnType = toB3ResultType(&signature);
    B3::PatchpointValue* patchpoint = block->appendNew<B3::PatchpointValue>(m_proc, returnType, origin);
    patchpoint->clobberEarly(RegisterSet::macroScratchRegisters());
    patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());
    patchpointFunctor(patchpoint);
    patchpoint->appendVector(constrainedArguments);

    if (returnType != B3::Void) {
        Vector<B3::ValueRep, 1> resultConstraints;
        for (auto valueLocation : wasmCallInfo.results)
            resultConstraints.append(B3::ValueRep(valueLocation));
        patchpoint->resultConstraints = WTFMove(resultConstraints);
    }
    return patchpoint;
}

auto B3IRGenerator::addCall(uint32_t functionIndex, const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ASSERT(signature.argumentCount() == args.size());

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
            ASSERT(signature.returnCount() == tuple.size());
            for (unsigned i = 0; i < signature.returnCount(); ++i)
                results.append(m_currentBlock->appendNew<ExtractValue>(m_proc, origin(), tuple[i], callResult, i));
            break;
        }
        default: {
            results.append(callResult);
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
            scopedLambdaRef<void(PatchpointValue*)>([=] (PatchpointValue* patchpoint) -> void {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;
                // We need to clobber all potential pinned registers since we might be leaving the instance.
                // We pessimistically assume we could be calling to something that is bounds checking.
                // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
                patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
                patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
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
            scopedLambdaRef<void(PatchpointValue*)>([=] (PatchpointValue* patchpoint) -> void {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;
                patchpoint->append(jumpDestination, ValueRep::SomeRegister);
                // We need to clobber all potential pinned registers since we might be leaving the instance.
                // We pessimistically assume we could be calling to something that is bounds checking.
                // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
                patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
                patchpoint->setGenerator([returnType] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
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
            scopedLambdaRef<void(PatchpointValue*)>([=] (PatchpointValue* patchpoint) -> void {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;

                // We need to clobber the size register since the LLInt always bounds checks
                if (m_mode == MemoryMode::Signaling || m_info.memory.isShared())
                    patchpoint->clobberLate(RegisterSet { PinnedRegisterInfo::get().boundsCheckingSizeRegister });
                patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
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

auto B3IRGenerator::addCallIndirect(unsigned tableIndex, const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();
    ASSERT(signature.argumentCount() == args.size());

    m_makesCalls = true;
    // Note: call indirect can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    ExpressionType callableFunctionBuffer;
    ExpressionType instancesBuffer;
    ExpressionType callableFunctionBufferLength;
    {
        ExpressionType table = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
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

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsCallIndirect);
        });
    }

    calleeIndex = m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), calleeIndex);

    ExpressionType callableFunction;
    {
        // Compute the offset in the table index space we are looking for.
        ExpressionType offset = m_currentBlock->appendNew<Value>(m_proc, Mul, origin(),
            calleeIndex, constant(pointerType(), sizeof(WasmToWasmImportableFunction)));
        callableFunction = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), callableFunctionBuffer, offset);

        // Check that the WasmToWasmImportableFunction is initialized. We trap if it isn't. An "invalid" SignatureIndex indicates it's not initialized.
        // FIXME: when we have trap handlers, we can just let the call fail because Signature::invalidIndex is 0. https://bugs.webkit.org/show_bug.cgi?id=177210
        static_assert(sizeof(WasmToWasmImportableFunction::signatureIndex) == sizeof(uint64_t), "Load codegen assumes i64");
        ExpressionType calleeSignatureIndex = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), callableFunction, safeCast<int32_t>(WasmToWasmImportableFunction::offsetOfSignatureIndex()));
        {
            CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Equal, origin(),
                    calleeSignatureIndex,
                    m_currentBlock->appendNew<Const64Value>(m_proc, origin(), Signature::invalidIndex)));

            check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitExceptionCheck(jit, ExceptionType::NullTableEntry);
            });
        }

        // Check the signature matches the value we expect.
        {
            ExpressionType expectedSignatureIndex = m_currentBlock->appendNew<Const64Value>(m_proc, origin(), SignatureInformation::get(signature));
            CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
                m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), calleeSignatureIndex, expectedSignatureIndex));

            check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                this->emitExceptionCheck(jit, ExceptionType::BadSignature);
            });
        }
    }

    Value* offset = m_currentBlock->appendNew<Value>(m_proc, Mul, origin(),
        calleeIndex, constant(pointerType(), sizeof(Instance*)));
    Value* calleeInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<Value>(m_proc, Add, origin(), instancesBuffer, offset));
    ExpressionType calleeCode = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callableFunction,
            safeCast<int32_t>(WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation())));

    return emitIndirectCall(calleeInstance, calleeCode, signature, args, results);
}

auto B3IRGenerator::addCallRef(const Signature& signature, Vector<ExpressionType>& args, ResultList& results) -> PartialResult
{
    ExpressionType callee = args.takeLast();
    ASSERT(signature.argumentCount() == args.size());
    m_makesCalls = true;

    // Note: call ref can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into the embedder, we conservatively assume all call indirects
    // can be to the embedder for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    Value* jsInstanceOffset = constant(pointerType(), safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfInstance()));
    Value* jsCalleeInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<Value>(m_proc, Add, origin(), callee, jsInstanceOffset));

    Value* instanceOffset = constant(pointerType(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfInstance()));
    Value* calleeInstance = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<Value>(m_proc, Add, origin(), jsCalleeInstance, instanceOffset));

    ExpressionType calleeCode = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callee,
            safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfEntrypointLoadLocation())));

    return emitIndirectCall(calleeInstance, calleeCode, signature, args, results);
}

void B3IRGenerator::unify(const ExpressionType phi, const ExpressionType source)
{
    m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), source, phi);
}

void B3IRGenerator::unifyValuesWithBlock(const Stack& resultStack, const ResultList& result)
{
    ASSERT(result.size() <= resultStack.size());

    for (size_t i = 0; i < result.size(); ++i)
        unify(result[result.size() - 1 - i], resultStack.at(resultStack.size() - 1 - i));
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

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompile(CompilationContext& compilationContext, const FunctionData& function, const Signature& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, unsigned& osrEntryScratchBufferSize, const ModuleInformation& info, MemoryMode mode, CompilationMode compilationMode, uint32_t functionIndex, uint32_t loopIndexForOSREntry, TierUpCount* tierUp)
{
    auto result = makeUnique<InternalFunction>();

    compilationContext.embedderEntrypointJIT = makeUnique<CCallHelpers>();
    compilationContext.wasmEntrypointJIT = makeUnique<CCallHelpers>();

    Procedure procedure;

    procedure.setOriginPrinter([] (PrintStream& out, Origin origin) {
        if (origin.data())
            out.print("Wasm: ", bitwise_cast<OpcodeOrigin>(origin));
    });
    
    // This means we cannot use either StackmapGenerationParams::usedRegisters() or
    // StackmapGenerationParams::unavailableRegisters(). In exchange for this concession, we
    // don't strictly need to run Air::reportUsedRegisters(), which saves a bit of CPU time at
    // optLevel=1.
    procedure.setNeedsUsedRegisters(false);
    
    procedure.setOptLevel(compilationMode == CompilationMode::BBQMode
        ? Options::webAssemblyBBQB3OptimizationLevel()
        : Options::webAssemblyOMGOptimizationLevel());

    B3IRGenerator irGenerator(info, procedure, result.get(), unlinkedWasmToWasmCalls, osrEntryScratchBufferSize, mode, compilationMode, functionIndex, loopIndexForOSREntry, tierUp);
    FunctionParser<B3IRGenerator> parser(irGenerator, function.data.data(), function.data.size(), signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    irGenerator.insertConstants();

    procedure.resetReachability();
    if (ASSERT_ENABLED)
        validate(procedure, "After parsing:\n");

    dataLogIf(WasmB3IRGeneratorInternal::verbose, "Pre SSA: ", procedure);
    fixSSA(procedure);
    dataLogIf(WasmB3IRGeneratorInternal::verbose, "Post SSA: ", procedure);
    
    {
        B3::prepareForGeneration(procedure);
        B3::generate(procedure, *compilationContext.wasmEntrypointJIT);
        compilationContext.wasmEntrypointByproducts = procedure.releaseByproducts();
        result->entrypoint.calleeSaveRegisters = procedure.calleeSaveRegisterAtOffsetList();
    }

    return result;
}

// Custom wasm ops. These are the ones too messy to do in wasm.json.

void B3IRGenerator::emitChecksForModOrDiv(B3::Opcode operation, ExpressionType left, ExpressionType right)
{
    ASSERT(operation == Div || operation == Mod || operation == UDiv || operation == UMod);
    const B3::Type type = left->type();

    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), right, constant(type, 0)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::DivisionByZero);
        });
    }

    if (operation == Div) {
        int64_t min = type == Int32 ? std::numeric_limits<int32_t>::min() : std::numeric_limits<int64_t>::min();

        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
                m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), left, constant(type, min)),
                m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), right, constant(type, -1))));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::IntegerOverflow);
        });
    }
}

template<>
auto B3IRGenerator::addOp<OpType::I32DivS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Div;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32RemS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Mod;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, chill(op), origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32DivU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UDiv;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32RemU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UMod;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64DivS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Div;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64RemS>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = Mod;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, chill(op), origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64DivU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UDiv;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64RemU>(ExpressionType left, ExpressionType right, ExpressionType& result) -> PartialResult
{
    const B3::Opcode op = UMod;
    emitChecksForModOrDiv(op, left, right);
    result = m_currentBlock->appendNew<Value>(m_proc, op, origin(), left, right);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32Ctz>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros32(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64Ctz>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.countTrailingZeros64(params[1].gpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32Popcnt>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
#if CPU(X86_64)
    if (MacroAssembler::supportsCountPopulation()) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
        patchpoint->append(arg, ValueRep::SomeRegister);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.countPopulation32(params[1].gpr(), params[0].gpr());
        });
        patchpoint->effects = Effects::none();
        result = patchpoint;
        return { };
    }
#endif

    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationPopcount32));
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(), Effects::none(), funcAddress, arg);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64Popcnt>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
#if CPU(X86_64)
    if (MacroAssembler::supportsCountPopulation()) {
        PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
        patchpoint->append(arg, ValueRep::SomeRegister);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.countPopulation64(params[1].gpr(), params[0].gpr());
        });
        patchpoint->effects = Effects::none();
        result = patchpoint;
        return { };
    }
#endif

    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), tagCFunction<OperationPtrTag>(operationPopcount64));
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int64, origin(), Effects::none(), funcAddress, arg);
    return { };
}

template<>
auto B3IRGenerator::addOp<F64ConvertUI64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
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
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32ConvertUI64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
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
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F64Nearest>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardNearestIntDouble(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32Nearest>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardNearestIntFloat(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F64Trunc>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardZeroDouble(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::F32Trunc>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Float, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.roundTowardZeroFloat(params[1].fpr(), params[0].fpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncSF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = constant(Double, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int32_t>::min())));
    Value* min = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) - 1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncSF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = constant(Float, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int32_t>::min())));
    Value* min = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}


template<>
auto B3IRGenerator::addOp<OpType::I32TruncUF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min()) * -2.0));
    Value* min = constant(Double, bitwise_cast<uint64_t>(-1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I32TruncUF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int32_t>::min()) * static_cast<float>(-2.0)));
    Value* min = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int32, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToUint32(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncSF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = constant(Double, bitwise_cast<uint64_t>(-static_cast<double>(std::numeric_limits<int64_t>::min())));
    Value* min = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateDoubleToInt64(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncUF64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int64_t>::min()) * -2.0));
    Value* min = constant(Double, bitwise_cast<uint64_t>(-1.0));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
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
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
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
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncSF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = constant(Float, bitwise_cast<uint32_t>(-static_cast<float>(std::numeric_limits<int64_t>::min())));
    Value* min = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min())));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterEqual, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
        this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsTrunc);
    });
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Int64, origin());
    patchpoint->append(arg, ValueRep::SomeRegister);
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        jit.truncateFloatToInt64(params[1].fpr(), params[0].gpr());
    });
    patchpoint->effects = Effects::none();
    result = patchpoint;
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64TruncUF32>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    Value* max = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(std::numeric_limits<int64_t>::min()) * static_cast<float>(-2.0)));
    Value* min = constant(Float, bitwise_cast<uint32_t>(static_cast<float>(-1.0)));
    Value* outOfBounds = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(),
        m_currentBlock->appendNew<Value>(m_proc, LessThan, origin(), arg, max),
        m_currentBlock->appendNew<Value>(m_proc, GreaterThan, origin(), arg, min));
    outOfBounds = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), outOfBounds, constant(Int32, 0));
    CheckValue* trap = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(), outOfBounds);
    trap->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams&) {
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
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
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
    result = patchpoint;
    return { };
}

} } // namespace JSC::Wasm

#include "WasmB3IRGeneratorInlines.h"

#endif // ENABLE(WEBASSEMBLY_B3JIT)
