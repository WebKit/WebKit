/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "AllowMacroScratchRegisterUsageIf.h"
#include "B3BasicBlockInlines.h"
#include "B3CCallValue.h"
#include "B3Compile.h"
#include "B3ConstPtrValue.h"
#include "B3FixSSA.h"
#include "B3Generate.h"
#include "B3InsertionSet.h"
#include "B3SlotBaseValue.h"
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
#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "JSWebAssemblyRuntimeError.h"
#include "ScratchRegisterAllocator.h"
#include "VirtualRegister.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmExceptionType.h"
#include "WasmFunctionParser.h"
#include "WasmMemory.h"
#include "WasmOMGPlan.h"
#include "WasmOpcodeOrigin.h"
#include "WasmThunks.h"
#include <limits>
#include <wtf/Optional.h>
#include <wtf/StdLibExtras.h>

void dumpProcedure(void* ptr)
{
    JSC::B3::Procedure* proc = static_cast<JSC::B3::Procedure*>(ptr);
    proc->dump(WTF::dataFile());
}

namespace JSC { namespace Wasm {

using namespace B3;

namespace {
const bool verbose = false;
}

class B3IRGenerator {
public:
    struct ControlData {
        ControlData(Procedure& proc, Origin origin, Type signature, BlockType type, BasicBlock* continuation, BasicBlock* special = nullptr)
            : blockType(type)
            , continuation(continuation)
            , special(special)
        {
            if (signature != Void)
                result.append(proc.add<Value>(Phi, toB3Type(signature), origin));
        }

        ControlData()
        {
        }

        void dump(PrintStream& out) const
        {
            switch (type()) {
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

        BlockType type() const { return blockType; }

        bool hasNonVoidSignature() const { return result.size(); }

        BasicBlock* targetBlockForBranch()
        {
            if (type() == BlockType::Loop)
                return special;
            return continuation;
        }

        void convertIfToBlock()
        {
            ASSERT(type() == BlockType::If);
            blockType = BlockType::Block;
            special = nullptr;
        }

        using ResultList = Vector<Value*, 1>; // Value must be a Phi

        ResultList resultForBranch() const
        {
            if (type() == BlockType::Loop)
                return ResultList();
            return result;
        }

    private:
        friend class B3IRGenerator;
        BlockType blockType;
        BasicBlock* continuation;
        BasicBlock* special;
        ResultList result;
    };

    typedef Value* ExpressionType;
    typedef ControlData ControlType;
    typedef Vector<ExpressionType, 1> ExpressionList;
    typedef ControlData::ResultList ResultList;
    typedef FunctionParser<B3IRGenerator>::ControlEntry ControlEntry;

    static constexpr ExpressionType emptyExpression = nullptr;

    typedef String ErrorType;
    typedef UnexpectedType<ErrorType> UnexpectedResult;
    typedef Expected<std::unique_ptr<InternalFunction>, ErrorType> Result;
    typedef Expected<void, ErrorType> PartialResult;
    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in WasmParser.h.
        return UnexpectedResult(makeString(ASCIILiteral("WebAssembly.Module failed compiling: "), makeString(args)...));
    }
#define WASM_COMPILE_FAIL_IF(condition, ...) do { \
        if (UNLIKELY(condition))                  \
            return fail(__VA_ARGS__);             \
    } while (0)

    B3IRGenerator(const ModuleInformation&, Procedure&, InternalFunction*, Vector<UnlinkedWasmToWasmCall>&, MemoryMode, CompilationMode, unsigned functionIndex, TierUpCount*);

    PartialResult WARN_UNUSED_RETURN addArguments(const Signature&);
    PartialResult WARN_UNUSED_RETURN addLocal(Type, uint32_t);
    ExpressionType addConstant(Type, uint64_t);

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
    ControlData WARN_UNUSED_RETURN addTopLevel(Type signature);
    ControlData WARN_UNUSED_RETURN addBlock(Type signature);
    ControlData WARN_UNUSED_RETURN addLoop(Type signature);
    PartialResult WARN_UNUSED_RETURN addIf(ExpressionType condition, Type signature, ControlData& result);
    PartialResult WARN_UNUSED_RETURN addElse(ControlData&, const ExpressionList&);
    PartialResult WARN_UNUSED_RETURN addElseToUnreachable(ControlData&);

    PartialResult WARN_UNUSED_RETURN addReturn(const ControlData&, const ExpressionList& returnValues);
    PartialResult WARN_UNUSED_RETURN addBranch(ControlData&, ExpressionType condition, const ExpressionList& returnValues);
    PartialResult WARN_UNUSED_RETURN addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTargets, const ExpressionList& expressionStack);
    PartialResult WARN_UNUSED_RETURN endBlock(ControlEntry&, ExpressionList& expressionStack);
    PartialResult WARN_UNUSED_RETURN addEndToUnreachable(ControlEntry&);

    // Calls
    PartialResult WARN_UNUSED_RETURN addCall(uint32_t calleeIndex, const Signature&, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addCallIndirect(const Signature&, Vector<ExpressionType>& args, ExpressionType& result);
    PartialResult WARN_UNUSED_RETURN addUnreachable();

    void dump(const Vector<ControlEntry>& controlStack, const ExpressionList* expressionStack);
    void setParser(FunctionParser<B3IRGenerator>* parser) { m_parser = parser; };

    Value* constant(B3::Type, uint64_t bits, std::optional<Origin> = std::nullopt);
    void insertConstants();

private:
    void emitExceptionCheck(CCallHelpers&, ExceptionType);

    void emitTierUpCheck(uint32_t decrementCount, Origin);

    ExpressionType emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOp);
    B3::Kind memoryKind(B3::Opcode memoryOp);
    ExpressionType emitLoadOp(LoadOpType, ExpressionType pointer, uint32_t offset);
    void emitStoreOp(StoreOpType, ExpressionType pointer, ExpressionType value, uint32_t offset);

    void unify(const ExpressionType phi, const ExpressionType source);
    void unifyValuesWithBlock(const ExpressionList& resultStack, const ResultList& stack);

    void emitChecksForModOrDiv(B3::Opcode, ExpressionType left, ExpressionType right);

    int32_t WARN_UNUSED_RETURN fixupPointerPlusOffset(ExpressionType&, uint32_t);

    void restoreWasmContext(Procedure&, BasicBlock*, Value*);
    void restoreWebAssemblyGlobalState(const MemoryInformation&, Value* instance, Procedure&, BasicBlock*);

    Origin origin();

    FunctionParser<B3IRGenerator>* m_parser { nullptr };
    const ModuleInformation& m_info;
    const MemoryMode m_mode { MemoryMode::BoundsChecking };
    const CompilationMode m_compilationMode { CompilationMode::BBQMode };
    const unsigned m_functionIndex { UINT_MAX };
    const TierUpCount* m_tierUp { nullptr };

    Procedure& m_proc;
    BasicBlock* m_currentBlock { nullptr };
    Vector<Variable*> m_locals;
    Vector<UnlinkedWasmToWasmCall>& m_unlinkedWasmToWasmCalls; // List each call site and the function index whose address it should be patched with.
    HashMap<ValueKey, Value*> m_constantPool;
    InsertionSet m_constantInsertionValues;
    GPRReg m_memoryBaseGPR { InvalidGPRReg };
    GPRReg m_memorySizeGPR { InvalidGPRReg };
    GPRReg m_wasmContextGPR { InvalidGPRReg };
    bool m_makesCalls { false };

    Value* m_instanceValue { nullptr }; // Always use the accessor below to ensure the instance value is materialized when used.
    bool m_usesInstanceValue { false };
    Value* instanceValue()
    {
        m_usesInstanceValue = true;
        return m_instanceValue;
    }

    uint32_t m_maxNumJSCallArguments { 0 };
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

void B3IRGenerator::restoreWasmContext(Procedure& proc, BasicBlock* block, Value* arg)
{
    if (useFastTLSForContext()) {
        PatchpointValue* patchpoint = block->appendNew<PatchpointValue>(proc, B3::Void, Origin());
        if (CCallHelpers::storeWasmContextNeedsMacroScratchRegister())
            patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->append(ConstrainedValue(arg, ValueRep::SomeRegister));
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsageIf allowScratch(jit, CCallHelpers::storeWasmContextNeedsMacroScratchRegister());
                jit.storeWasmContext(params[0].gpr());
            });
        return;
    }

    // FIXME: Because WasmToWasm call clobbers wasmContext register and does not restore it, we need to restore it in the caller side.
    // This prevents us from using ArgumentReg to this (logically) immutable pinned register.
    PatchpointValue* patchpoint = block->appendNew<PatchpointValue>(proc, B3::Void, Origin());
    Effects effects = Effects::none();
    effects.writesPinned = true;
    effects.reads = B3::HeapRange::top();
    patchpoint->effects = effects;
    patchpoint->clobberLate(RegisterSet(m_wasmContextGPR));
    patchpoint->append(instanceValue(), ValueRep::SomeRegister);
    GPRReg wasmContextGPR = m_wasmContextGPR;
    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& param) {
        jit.move(param[0].gpr(), wasmContextGPR);
    });
}

B3IRGenerator::B3IRGenerator(const ModuleInformation& info, Procedure& procedure, InternalFunction* compilation, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, MemoryMode mode, CompilationMode compilationMode, unsigned functionIndex, TierUpCount* tierUp)
    : m_info(info)
    , m_mode(mode)
    , m_compilationMode(compilationMode)
    , m_functionIndex(functionIndex)
    , m_tierUp(tierUp)
    , m_proc(procedure)
    , m_unlinkedWasmToWasmCalls(unlinkedWasmToWasmCalls)
    , m_constantInsertionValues(m_proc)
{
    m_currentBlock = m_proc.addBlock();

    // FIXME we don't really need to pin registers here if there's no memory. It makes wasm -> wasm thunks simpler for now. https://bugs.webkit.org/show_bug.cgi?id=166623
    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();

    m_memoryBaseGPR = pinnedRegs.baseMemoryPointer;
    m_proc.pinRegister(m_memoryBaseGPR);

    m_wasmContextGPR = pinnedRegs.wasmContextPointer;
    if (!useFastTLSForContext())
        m_proc.pinRegister(m_wasmContextGPR);

    if (mode != MemoryMode::Signaling) {
        ASSERT(!pinnedRegs.sizeRegisters[0].sizeOffset);
        m_memorySizeGPR = pinnedRegs.sizeRegisters[0].sizeRegister;
        for (const PinnedSizeRegisterInfo& regInfo : pinnedRegs.sizeRegisters)
            m_proc.pinRegister(regInfo.sizeRegister);
    }

    if (info.memory) {
        m_proc.setWasmBoundsCheckGenerator([=] (CCallHelpers& jit, GPRReg pinnedGPR) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            switch (m_mode) {
            case MemoryMode::BoundsChecking:
                ASSERT_UNUSED(pinnedGPR, m_memorySizeGPR == pinnedGPR);
                break;
            case MemoryMode::Signaling:
                ASSERT_UNUSED(pinnedGPR, InvalidGPRReg == pinnedGPR);
                break;
            }
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsMemoryAccess);
        });
    }

    wasmCallingConvention().setupFrameInPrologue(&compilation->calleeMoveLocation, m_proc, Origin(), m_currentBlock);

    {
        B3::Value* framePointer = m_currentBlock->appendNew<B3::Value>(m_proc, B3::FramePointer, Origin());
        B3::PatchpointValue* stackOverflowCheck = m_currentBlock->appendNew<B3::PatchpointValue>(m_proc, pointerType(), Origin());
        m_instanceValue = stackOverflowCheck;
        stackOverflowCheck->appendSomeRegister(framePointer);
        stackOverflowCheck->clobber(RegisterSet::macroScratchRegisters());
        if (!useFastTLSForContext()) {
            // FIXME: Because WasmToWasm call clobbers wasmContext register and does not restore it, we need to restore it in the caller side.
            // This prevents us from using ArgumentReg to this (logically) immutable pinned register.
            stackOverflowCheck->effects.writesPinned = false;
            stackOverflowCheck->effects.readsPinned = true;
            stackOverflowCheck->resultConstraint = ValueRep::reg(m_wasmContextGPR);
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
                // This allows us to elide stack checks in the Wasm -> JS call IC stub. Since these will
                // spill all arguments to the stack, we ensure that a stack check here covers the
                // stack that such a stub would use.
                (Checked<uint32_t>(m_maxNumJSCallArguments) * sizeof(Register) + jscCallingConvention().headerSizeInBytes()).unsafeGet()
            ));
            const int32_t checkSize = m_makesCalls ? (wasmFrameSize + extraFrameSize).unsafeGet() : wasmFrameSize.unsafeGet();
            bool needUnderflowCheck = static_cast<unsigned>(checkSize) > Options::reservedZoneSize();
            bool needsOverflowCheck = m_makesCalls || wasmFrameSize >= minimumParentCheckSize || needUnderflowCheck;

            GPRReg context = useFastTLSForContext() ? params[0].gpr() : m_wasmContextGPR;

            // This allows leaf functions to not do stack checks if their frame size is within
            // certain limits since their caller would have already done the check.
            if (needsOverflowCheck) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                GPRReg fp = params[1].gpr();
                GPRReg scratch1 = params.gpScratch(0);
                GPRReg scratch2 = params.gpScratch(1);

                if (useFastTLSForContext())
                    jit.loadWasmContext(context);

                jit.loadPtr(CCallHelpers::Address(context, Context::offsetOfCachedStackLimit()), scratch2);
                jit.addPtr(CCallHelpers::TrustedImm32(-checkSize), fp, scratch1);
                MacroAssembler::JumpList overflow;
                if (UNLIKELY(needUnderflowCheck))
                    overflow.append(jit.branchPtr(CCallHelpers::Above, scratch1, fp));
                overflow.append(jit.branchPtr(CCallHelpers::Below, scratch1, scratch2));
                jit.addLinkTask([overflow] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(overflow, CodeLocationLabel(Thunks::singleton().stub(throwStackOverflowFromWasmThunkGenerator).code()));
                });
            } else if (m_usesInstanceValue && useFastTLSForContext()) {
                // No overflow check is needed, but the instance values still needs to be correct.
                AllowMacroScratchRegisterUsageIf allowScratch(jit, CCallHelpers::loadWasmContextNeedsMacroScratchRegister());
                jit.loadWasmContext(context);
            } else {
                // We said we'd return a pointer. We don't actually need to because it isn't used, but the patchpoint conservatively said it had effects (potential stack check) which prevent it from getting removed.
            }
        });
    }

    emitTierUpCheck(TierUpCount::functionEntryDecrement(), Origin());
}

void B3IRGenerator::restoreWebAssemblyGlobalState(const MemoryInformation& memory, Value* instance, Procedure& proc, BasicBlock* block)
{
    restoreWasmContext(proc, block, instance);

    if (!!memory) {
        const PinnedRegisterInfo* pinnedRegs = &PinnedRegisterInfo::get();
        RegisterSet clobbers;
        clobbers.set(pinnedRegs->baseMemoryPointer);
        for (auto info : pinnedRegs->sizeRegisters)
            clobbers.set(info.sizeRegister);

        B3::PatchpointValue* patchpoint = block->appendNew<B3::PatchpointValue>(proc, B3::Void, origin());
        Effects effects = Effects::none();
        effects.writesPinned = true;
        effects.reads = B3::HeapRange::top();
        patchpoint->effects = effects;
        patchpoint->clobber(clobbers);

        patchpoint->append(instance, ValueRep::SomeRegister);

        patchpoint->setGenerator([pinnedRegs] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            GPRReg baseMemory = pinnedRegs->baseMemoryPointer;
            jit.loadPtr(CCallHelpers::Address(params[0].gpr(), JSWebAssemblyInstance::offsetOfMemory()), baseMemory);
            const auto& sizeRegs = pinnedRegs->sizeRegisters;
            ASSERT(sizeRegs.size() >= 1);
            ASSERT(!sizeRegs[0].sizeOffset); // The following code assumes we start at 0, and calculates subsequent size registers relative to 0.
            jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyMemory::offsetOfSize()), sizeRegs[0].sizeRegister);
            jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyMemory::offsetOfMemory()), baseMemory);
            for (unsigned i = 1; i < sizeRegs.size(); ++i)
                jit.add64(CCallHelpers::TrustedImm32(-sizeRegs[i].sizeOffset), sizeRegs[0].sizeRegister, sizeRegs[i].sizeRegister);
        });
    }
}

void B3IRGenerator::emitExceptionCheck(CCallHelpers& jit, ExceptionType type)
{
    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(type)), GPRInfo::argumentGPR1);
    auto jumpToExceptionStub = jit.jump();

    jit.addLinkTask([jumpToExceptionStub] (LinkBuffer& linkBuffer) {
        linkBuffer.link(jumpToExceptionStub, CodeLocationLabel(Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator).code()));
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

void B3IRGenerator::insertConstants()
{
    m_constantInsertionValues.execute(m_proc.at(0));
}

auto B3IRGenerator::addLocal(Type type, uint32_t count) -> PartialResult
{
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(m_locals.size() + count), "can't allocate memory for ", m_locals.size() + count, " locals");

    for (uint32_t i = 0; i < count; ++i) {
        Variable* local = m_proc.addVariable(toB3Type(type));
        m_locals.uncheckedAppend(local);
        m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), local, constant(toB3Type(type), 0, Origin()));
    }
    return { };
}

auto B3IRGenerator::addArguments(const Signature& signature) -> PartialResult
{
    ASSERT(!m_locals.size());
    WASM_COMPILE_FAIL_IF(!m_locals.tryReserveCapacity(signature.argumentCount()), "can't allocate memory for ", signature.argumentCount(), " arguments");

    m_locals.grow(signature.argumentCount());
    wasmCallingConvention().loadArguments(signature, m_proc, m_currentBlock, Origin(),
        [=] (ExpressionType argument, unsigned i) {
            Variable* argumentVariable = m_proc.addVariable(argument->type());
            m_locals[i] = argumentVariable;
            m_currentBlock->appendNew<VariableValue>(m_proc, Set, Origin(), argumentVariable, argument);
        });
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

auto B3IRGenerator::addGrowMemory(ExpressionType delta, ExpressionType& result) -> PartialResult
{
    int32_t (*growMemory) (Context*, int32_t) = [] (Context* wasmContext, int32_t delta) -> int32_t {
        VM& vm = *wasmContext->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        JSWebAssemblyMemory* wasmMemory = wasmContext->memory();

        if (delta < 0)
            return -1;

        bool shouldThrowExceptionsOnFailure = false;
        // grow() does not require ExecState* if it doesn't throw exceptions.
        ExecState* exec = nullptr;
        PageCount result = wasmMemory->grow(vm, exec, static_cast<uint32_t>(delta), shouldThrowExceptionsOnFailure);
        scope.releaseAssertNoException();
        if (!result)
            return -1;

        return result.pageCount();
    };

    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(),
        m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), bitwise_cast<void*>(growMemory)),
        instanceValue(), delta);

    restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_proc, m_currentBlock);

    return { };
}

auto B3IRGenerator::addCurrentMemory(ExpressionType& result) -> PartialResult
{
    Value* memoryObject = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfMemory()));

    static_assert(sizeof(decltype(static_cast<JSWebAssemblyInstance*>(nullptr)->memory()->memory().size())) == sizeof(uint64_t), "codegen relies on this size");
    Value* size = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int64, origin(), memoryObject, safeCast<int32_t>(JSWebAssemblyMemory::offsetOfSize()));
    
    constexpr uint32_t shiftValue = 16;
    static_assert(PageCount::pageSize == 1 << shiftValue, "This must hold for the code below to be correct.");
    Value* numPages = m_currentBlock->appendNew<Value>(m_proc, ZShr, origin(),
        size, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), shiftValue));

    result = m_currentBlock->appendNew<Value>(m_proc, Trunc, origin(), numPages);

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
    Value* globalsArray = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfGlobals()));
    result = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, toB3Type(m_info.globals[index].type), origin(), globalsArray, safeCast<int32_t>(index * sizeof(Register)));
    return { };
}

auto B3IRGenerator::setGlobal(uint32_t index, ExpressionType value) -> PartialResult
{
    ASSERT(toB3Type(m_info.globals[index].type) == value->type());
    Value* globalsArray = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfGlobals()));
    m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin(), value, globalsArray, safeCast<int32_t>(index * sizeof(Register)));
    return { };
}

inline Value* B3IRGenerator::emitCheckAndPreparePointer(ExpressionType pointer, uint32_t offset, uint32_t sizeOfOperation)
{
    ASSERT(m_memoryBaseGPR);

    switch (m_mode) {
    case MemoryMode::BoundsChecking:
        // We're not using signal handling at all, we must therefore check that no memory access exceeds the current memory size.
        ASSERT(m_memorySizeGPR);
        ASSERT(sizeOfOperation + offset > offset);
        m_currentBlock->appendNew<WasmBoundsCheckValue>(m_proc, origin(), m_memorySizeGPR, pointer, sizeOfOperation + offset - 1);
        break;

    case MemoryMode::Signaling:
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
    if (m_mode == MemoryMode::Signaling)
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

    // FIXME: B3 doesn't support Load16Z yet. We should lower to that value when
    // it's added. https://bugs.webkit.org/show_bug.cgi?id=165884
    case LoadOpType::I32Load16U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16S), origin(), pointer, offset);
        return m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), value,
            m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0x0000ffff));
    }
    case LoadOpType::I64Load16U: {
        Value* value = m_currentBlock->appendNew<MemoryValue>(m_proc, memoryKind(Load16S), origin(), pointer, offset);
        Value* partialResult = m_currentBlock->appendNew<Value>(m_proc, BitAnd, origin(), value,
            m_currentBlock->appendNew<Const32Value>(m_proc, origin(), 0x0000ffff));

        return m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), partialResult);
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

auto B3IRGenerator::addSelect(ExpressionType condition, ExpressionType nonZero, ExpressionType zero, ExpressionType& result) -> PartialResult
{
    result = m_currentBlock->appendNew<Value>(m_proc, B3::Select, origin(), condition, nonZero, zero);
    return { };
}

B3IRGenerator::ExpressionType B3IRGenerator::addConstant(Type type, uint64_t value)
{
    return constant(toB3Type(type), value);
}

void B3IRGenerator::emitTierUpCheck(uint32_t decrementCount, Origin origin)
{
    if (!m_tierUp)
        return;

    ASSERT(m_tierUp);
    Value* countDownLocation = constant(pointerType(), reinterpret_cast<uint64_t>(m_tierUp), origin);
    Value* oldCountDown = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin, countDownLocation);
    Value* newCountDown = m_currentBlock->appendNew<Value>(m_proc, Sub, origin, oldCountDown, constant(Int32, decrementCount, origin));
    m_currentBlock->appendNew<MemoryValue>(m_proc, Store, origin, newCountDown, countDownLocation);

    PatchpointValue* patch = m_currentBlock->appendNew<PatchpointValue>(m_proc, B3::Void, origin);
    Effects effects = Effects::none();
    // FIXME: we should have a more precise heap range for the tier up count.
    effects.reads = B3::HeapRange::top();
    effects.writes = B3::HeapRange::top();
    patch->effects = effects;

    patch->append(newCountDown, ValueRep::SomeRegister);
    patch->append(oldCountDown, ValueRep::SomeRegister);
    patch->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        MacroAssembler::Jump tierUp = jit.branch32(MacroAssembler::Above, params[0].gpr(), params[1].gpr());
        MacroAssembler::Label tierUpResume = jit.label();

        params.addLatePath([=] (CCallHelpers& jit) {
            tierUp.link(&jit);

            const unsigned extraPaddingBytes = 0;
            RegisterSet registersToSpill = RegisterSet();
            registersToSpill.add(GPRInfo::argumentGPR1);
            unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraPaddingBytes);

            jit.move(MacroAssembler::TrustedImm32(m_functionIndex), GPRInfo::argumentGPR1);
            MacroAssembler::Call call = jit.nearCall();

            ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToSpill, RegisterSet(), numberOfStackBytesUsedForRegisterPreservation, extraPaddingBytes);
            jit.jump(tierUpResume);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                MacroAssembler::repatchNearCall(linkBuffer.locationOfNearCall(call), CodeLocationLabel(Thunks::singleton().stub(triggerOMGTierUpThunkGenerator).code()));

            });
        });
    });
}

B3IRGenerator::ControlData B3IRGenerator::addLoop(Type signature)
{
    BasicBlock* body = m_proc.addBlock();
    BasicBlock* continuation = m_proc.addBlock();

    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), body);

    m_currentBlock = body;
    emitTierUpCheck(TierUpCount::loopDecrement(), origin());

    return ControlData(m_proc, origin(), signature, BlockType::Loop, continuation, body);
}

B3IRGenerator::ControlData B3IRGenerator::addTopLevel(Type signature)
{
    return ControlData(m_proc, Origin(), signature, BlockType::TopLevel, m_proc.addBlock());
}

B3IRGenerator::ControlData B3IRGenerator::addBlock(Type signature)
{
    return ControlData(m_proc, origin(), signature, BlockType::Block, m_proc.addBlock());
}

auto B3IRGenerator::addIf(ExpressionType condition, Type signature, ControlType& result) -> PartialResult
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
    result = ControlData(m_proc, origin(), signature, BlockType::If, continuation, notTaken);
    return { };
}

auto B3IRGenerator::addElse(ControlData& data, const ExpressionList& currentStack) -> PartialResult
{
    unifyValuesWithBlock(currentStack, data.result);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    return addElseToUnreachable(data);
}

auto B3IRGenerator::addElseToUnreachable(ControlData& data) -> PartialResult
{
    ASSERT(data.type() == BlockType::If);
    m_currentBlock = data.special;
    data.convertIfToBlock();
    return { };
}

auto B3IRGenerator::addReturn(const ControlData&, const ExpressionList& returnValues) -> PartialResult
{
    ASSERT(returnValues.size() <= 1);
    if (returnValues.size())
        m_currentBlock->appendNewControlValue(m_proc, B3::Return, origin(), returnValues[0]);
    else
        m_currentBlock->appendNewControlValue(m_proc, B3::Return, origin());
    return { };
}

auto B3IRGenerator::addBranch(ControlData& data, ExpressionType condition, const ExpressionList& returnValues) -> PartialResult
{
    unifyValuesWithBlock(returnValues, data.resultForBranch());

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

auto B3IRGenerator::addSwitch(ExpressionType condition, const Vector<ControlData*>& targets, ControlData& defaultTarget, const ExpressionList& expressionStack) -> PartialResult
{
    for (size_t i = 0; i < targets.size(); ++i)
        unifyValuesWithBlock(expressionStack, targets[i]->resultForBranch());
    unifyValuesWithBlock(expressionStack, defaultTarget.resultForBranch());

    SwitchValue* switchValue = m_currentBlock->appendNew<SwitchValue>(m_proc, origin(), condition);
    switchValue->setFallThrough(FrequentedBlock(defaultTarget.targetBlockForBranch()));
    for (size_t i = 0; i < targets.size(); ++i)
        switchValue->appendCase(SwitchCase(i, FrequentedBlock(targets[i]->targetBlockForBranch())));

    return { };
}

auto B3IRGenerator::endBlock(ControlEntry& entry, ExpressionList& expressionStack) -> PartialResult
{
    ControlData& data = entry.controlData;

    unifyValuesWithBlock(expressionStack, data.result);
    m_currentBlock->appendNewControlValue(m_proc, Jump, origin(), data.continuation);
    data.continuation->addPredecessor(m_currentBlock);

    return addEndToUnreachable(entry);
}


auto B3IRGenerator::addEndToUnreachable(ControlEntry& entry) -> PartialResult
{
    ControlData& data = entry.controlData;
    m_currentBlock = data.continuation;

    if (data.type() == BlockType::If) {
        data.special->appendNewControlValue(m_proc, Jump, origin(), m_currentBlock);
        m_currentBlock->addPredecessor(data.special);
    }

    for (Value* result : data.result) {
        m_currentBlock->append(result);
        entry.enclosedExpressionStack.append(result);
    }

    // TopLevel does not have any code after this so we need to make sure we emit a return here.
    if (data.type() == BlockType::TopLevel)
        return addReturn(entry.controlData, entry.enclosedExpressionStack);

    return { };
}

auto B3IRGenerator::addCall(uint32_t functionIndex, const Signature& signature, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    ASSERT(signature.argumentCount() == args.size());

    m_makesCalls = true;

    Type returnType = signature.returnType();
    Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls = &m_unlinkedWasmToWasmCalls;

    if (m_info.isImportedFunctionFromFunctionIndexSpace(functionIndex)) {
        m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

        // FIXME imports can be linked here, instead of generating a patchpoint, because all import stubs are generated before B3 compilation starts. https://bugs.webkit.org/show_bug.cgi?id=166462
        Value* functionImport = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfImportFunction(functionIndex)));
        Value* jsTypeOfImport = m_currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, origin(), functionImport, safeCast<int32_t>(JSCell::typeInfoTypeOffset()));
        Value* isWasmCall = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(), jsTypeOfImport, m_currentBlock->appendNew<Const32Value>(m_proc, origin(), WebAssemblyFunctionType));

        BasicBlock* isWasmBlock = m_proc.addBlock();
        BasicBlock* isJSBlock = m_proc.addBlock();
        BasicBlock* continuation = m_proc.addBlock();
        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(), isWasmCall, FrequentedBlock(isWasmBlock), FrequentedBlock(isJSBlock));

        Value* wasmCallResult = wasmCallingConvention().setupCall(m_proc, isWasmBlock, origin(), args, toB3Type(returnType),
            [=] (PatchpointValue* patchpoint) {
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
                        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall(call), functionIndex });
                    });
                });
            });
        UpsilonValue* wasmCallResultUpsilon = returnType == Void ? nullptr : isWasmBlock->appendNew<UpsilonValue>(m_proc, origin(), wasmCallResult);
        isWasmBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);

        // FIXME: Lets remove this indirection by creating a PIC friendly IC
        // for calls out to JS. This shouldn't be that hard to do. We could probably
        // implement the IC to be over Wasm::Context*.
        // https://bugs.webkit.org/show_bug.cgi?id=170375
        Value* codeBlock = isJSBlock->appendNew<MemoryValue>(m_proc,
            Load, pointerType(), origin(), instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfCodeBlock()));
        Value* jumpDestination = isJSBlock->appendNew<MemoryValue>(m_proc,
            Load, pointerType(), origin(), codeBlock, safeCast<int32_t>(JSWebAssemblyCodeBlock::offsetOfImportWasmToJSStub(functionIndex)));
        Value* jsCallResult = wasmCallingConvention().setupCall(m_proc, isJSBlock, origin(), args, toB3Type(returnType),
            [&] (PatchpointValue* patchpoint) {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;
                patchpoint->append(jumpDestination, ValueRep::SomeRegister);
                // We need to clobber all potential pinned registers since we might be leaving the instance.
                // We pessimistically assume we could be calling to something that is bounds checking.
                // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
                patchpoint->clobberLate(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
                patchpoint->setGenerator([returnType] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    jit.call(params[returnType == Void ? 0 : 1].gpr());
                });
            });
        UpsilonValue* jsCallResultUpsilon = returnType == Void ? nullptr : isJSBlock->appendNew<UpsilonValue>(m_proc, origin(), jsCallResult);
        isJSBlock->appendNewControlValue(m_proc, Jump, origin(), continuation);

        m_currentBlock = continuation;

        if (returnType == Void)
            result = nullptr;
        else {
            result = continuation->appendNew<Value>(m_proc, Phi, toB3Type(returnType), origin());
            wasmCallResultUpsilon->setPhi(result);
            jsCallResultUpsilon->setPhi(result);
        }

        // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
        restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_proc, continuation);
    } else {
        result = wasmCallingConvention().setupCall(m_proc, m_currentBlock, origin(), args, toB3Type(returnType),
            [=] (PatchpointValue* patchpoint) {
                patchpoint->effects.writesPinned = true;
                patchpoint->effects.readsPinned = true;

                patchpoint->setGenerator([unlinkedWasmToWasmCalls, functionIndex] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    CCallHelpers::Call call = jit.threadSafePatchableNearCall();
                    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndex] (LinkBuffer& linkBuffer) {
                        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall(call), functionIndex });
                    });
                });
            });
    }

    return { };
}

auto B3IRGenerator::addCallIndirect(const Signature& signature, Vector<ExpressionType>& args, ExpressionType& result) -> PartialResult
{
    ExpressionType calleeIndex = args.takeLast();
    ASSERT(signature.argumentCount() == args.size());

    m_makesCalls = true;
    // Note: call indirect can call either WebAssemblyFunction or WebAssemblyWrapperFunction. Because
    // WebAssemblyWrapperFunction is like calling into JS, we conservatively assume all call indirects
    // can be to JS for our stack check calculation.
    m_maxNumJSCallArguments = std::max(m_maxNumJSCallArguments, static_cast<uint32_t>(args.size()));

    ExpressionType callableFunctionBuffer;
    ExpressionType jsFunctionBuffer;
    ExpressionType callableFunctionBufferSize;
    {
        ExpressionType table = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
            instanceValue(), safeCast<int32_t>(JSWebAssemblyInstance::offsetOfTable()));
        callableFunctionBuffer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
            table, safeCast<int32_t>(JSWebAssemblyTable::offsetOfFunctions()));
        jsFunctionBuffer = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
            table, safeCast<int32_t>(JSWebAssemblyTable::offsetOfJSFunctions()));
        callableFunctionBufferSize = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(),
            table, safeCast<int32_t>(JSWebAssemblyTable::offsetOfSize()));
    }

    // Check the index we are looking for is valid.
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, AboveEqual, origin(), calleeIndex, callableFunctionBufferSize));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::OutOfBoundsCallIndirect);
        });
    }

    // Compute the offset in the table index space we are looking for.
    ExpressionType offset = m_currentBlock->appendNew<Value>(m_proc, Mul, origin(),
        m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), calleeIndex),
        constant(pointerType(), sizeof(CallableFunction)));
    ExpressionType callableFunction = m_currentBlock->appendNew<Value>(m_proc, Add, origin(), callableFunctionBuffer, offset);

    // Check that the CallableFunction is initialized. We trap if it isn't. An "invalid" SignatureIndex indicates it's not initialized.
    static_assert(sizeof(CallableFunction::signatureIndex) == sizeof(uint32_t), "Load codegen assumes i32");
    ExpressionType calleeSignatureIndex = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, Int32, origin(), callableFunction, safeCast<int32_t>(OBJECT_OFFSETOF(CallableFunction, signatureIndex)));
    {
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, Equal, origin(),
                calleeSignatureIndex,
                m_currentBlock->appendNew<Const32Value>(m_proc, origin(), Signature::invalidIndex)));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::NullTableEntry);
        });
    }

    // Check the signature matches the value we expect.
    {
        ExpressionType expectedSignatureIndex = m_currentBlock->appendNew<Const32Value>(m_proc, origin(), SignatureInformation::get(signature));
        CheckValue* check = m_currentBlock->appendNew<CheckValue>(m_proc, Check, origin(),
            m_currentBlock->appendNew<Value>(m_proc, NotEqual, origin(), calleeSignatureIndex, expectedSignatureIndex));

        check->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            this->emitExceptionCheck(jit, ExceptionType::BadSignature);
        });
    }

    // Do a context switch if needed.
    {
        Value* offset = m_currentBlock->appendNew<Value>(m_proc, Mul, origin(),
            m_currentBlock->appendNew<Value>(m_proc, ZExt32, origin(), calleeIndex),
            constant(pointerType(), sizeof(WriteBarrier<JSObject>)));
        Value* jsObject = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
            m_currentBlock->appendNew<Value>(m_proc, Add, origin(), jsFunctionBuffer, offset));

        BasicBlock* continuation = m_proc.addBlock();
        BasicBlock* doContextSwitch = m_proc.addBlock();

        Value* newContext = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
            jsObject, safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfInstance()));
        Value* isSameContext = m_currentBlock->appendNew<Value>(m_proc, Equal, origin(),
            newContext, instanceValue());
        m_currentBlock->appendNewControlValue(m_proc, B3::Branch, origin(),
            isSameContext, FrequentedBlock(continuation), FrequentedBlock(doContextSwitch));

        PatchpointValue* patchpoint = doContextSwitch->appendNew<PatchpointValue>(m_proc, B3::Void, origin());
        patchpoint->effects.writesPinned = true;
        // We pessimistically assume we're calling something with BoundsChecking memory.
        // FIXME: We shouldn't have to do this: https://bugs.webkit.org/show_bug.cgi?id=172181
        patchpoint->clobber(PinnedRegisterInfo::get().toSave(MemoryMode::BoundsChecking));
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->append(newContext, ValueRep::SomeRegister);
        patchpoint->append(instanceValue(), ValueRep::SomeRegister);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg newContext = params[0].gpr();
            GPRReg oldContext = params[1].gpr();
            const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
            const auto& sizeRegs = pinnedRegs.sizeRegisters;
            GPRReg baseMemory = pinnedRegs.baseMemoryPointer;
            ASSERT(newContext != baseMemory);
            jit.loadPtr(CCallHelpers::Address(oldContext, Context::offsetOfCachedStackLimit()), baseMemory);
            jit.storePtr(baseMemory, CCallHelpers::Address(newContext, Context::offsetOfCachedStackLimit()));
            jit.storeWasmContext(newContext);
            jit.loadPtr(CCallHelpers::Address(newContext, Context::offsetOfMemory()), baseMemory); // JSWebAssemblyMemory*.
            ASSERT(sizeRegs.size() == 1);
            ASSERT(sizeRegs[0].sizeRegister != baseMemory);
            ASSERT(sizeRegs[0].sizeRegister != newContext);
            ASSERT(!sizeRegs[0].sizeOffset);
            jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyMemory::offsetOfSize()), sizeRegs[0].sizeRegister); // Memory size.
            jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyMemory::offsetOfMemory()), baseMemory); // WasmMemory::void*.
        });
        doContextSwitch->appendNewControlValue(m_proc, Jump, origin(), continuation);

        m_currentBlock = continuation;
    }

    ExpressionType calleeCode = m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(),
        m_currentBlock->appendNew<MemoryValue>(m_proc, Load, pointerType(), origin(), callableFunction,
            safeCast<int32_t>(OBJECT_OFFSETOF(CallableFunction, code))));

    Type returnType = signature.returnType();
    result = wasmCallingConvention().setupCall(m_proc, m_currentBlock, origin(), args, toB3Type(returnType),
        [=] (PatchpointValue* patchpoint) {
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
                jit.call(params[returnType == Void ? 0 : 1].gpr());
            });
        });

    // The call could have been to another WebAssembly instance, and / or could have modified our Memory.
    restoreWebAssemblyGlobalState(m_info.memory, instanceValue(), m_proc, m_currentBlock);

    return { };
}

void B3IRGenerator::unify(const ExpressionType phi, const ExpressionType source)
{
    m_currentBlock->appendNew<UpsilonValue>(m_proc, origin(), source, phi);
}

void B3IRGenerator::unifyValuesWithBlock(const ExpressionList& resultStack, const ResultList& result)
{
    ASSERT(result.size() <= resultStack.size());

    for (size_t i = 0; i < result.size(); ++i)
        unify(result[result.size() - 1 - i], resultStack[resultStack.size() - 1 - i]);
}

static void dumpExpressionStack(const CommaPrinter& comma, const B3IRGenerator::ExpressionList& expressionStack)
{
    dataLog(comma, "ExpressionStack:");
    for (const auto& expression : expressionStack)
        dataLog(comma, *expression);
}

void B3IRGenerator::dump(const Vector<ControlEntry>& controlStack, const ExpressionList* expressionStack)
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

std::unique_ptr<InternalFunction> createJSToWasmWrapper(CompilationContext& compilationContext, const Signature& signature, Vector<UnlinkedWasmToWasmCall>* unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, unsigned functionIndex)
{
    CCallHelpers& jit = *compilationContext.jsEntrypointJIT;

    auto result = std::make_unique<InternalFunction>();
    jit.emitFunctionPrologue();

    // FIXME Stop using 0 as codeBlocks. https://bugs.webkit.org/show_bug.cgi?id=165321
    jit.store64(CCallHelpers::TrustedImm64(0), CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::codeBlock * static_cast<int>(sizeof(Register))));
    MacroAssembler::DataLabelPtr calleeMoveLocation = jit.moveWithPatch(MacroAssembler::TrustedImmPtr(nullptr), GPRInfo::nonPreservedNonReturnGPR);
    jit.storePtr(GPRInfo::nonPreservedNonReturnGPR, CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register))));
    CodeLocationDataLabelPtr* linkedCalleeMove = &result->calleeMoveLocation;
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        *linkedCalleeMove = linkBuffer.locationOf(calleeMoveLocation);
    });

    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
    RegisterSet toSave = pinnedRegs.toSave(mode);

#if !ASSERT_DISABLED
    unsigned toSaveSize = toSave.numberOfSetGPRs();
    // They should all be callee saves.
    toSave.filter(RegisterSet::calleeSaveRegisters());
    ASSERT(toSave.numberOfSetGPRs() == toSaveSize);
#endif

    RegisterAtOffsetList registersToSpill(toSave, RegisterAtOffsetList::OffsetBaseType::FramePointerBased);
    result->entrypoint.calleeSaveRegisters = registersToSpill;

    unsigned totalFrameSize = registersToSpill.size() * sizeof(void*);
    totalFrameSize += WasmCallingConvention::headerSizeInBytes();
    totalFrameSize -= sizeof(CallerFrameAndPC);
    unsigned numGPRs = 0;
    unsigned numFPRs = 0;
    for (unsigned i = 0; i < signature.argumentCount(); i++) {
        switch (signature.argument(i)) {
        case Wasm::I64:
        case Wasm::I32:
            if (numGPRs >= wasmCallingConvention().m_gprArgs.size())
                totalFrameSize += sizeof(void*);
            ++numGPRs;
            break;
        case Wasm::F32:
        case Wasm::F64:
            if (numFPRs >= wasmCallingConvention().m_fprArgs.size())
                totalFrameSize += sizeof(void*);
            ++numFPRs;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    totalFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), totalFrameSize);
    jit.subPtr(MacroAssembler::TrustedImm32(totalFrameSize), MacroAssembler::stackPointerRegister);

    // We save all these registers regardless of having a memory or not.
    // The reason is that we use one of these as a scratch. That said,
    // almost all real wasm programs use memory, so it's not really
    // worth optimizing for the case that they don't.
    for (const RegisterAtOffset& regAtOffset : registersToSpill) {
        GPRReg reg = regAtOffset.reg().gpr();
        ptrdiff_t offset = regAtOffset.offset();
        jit.storePtr(reg, CCallHelpers::Address(GPRInfo::callFrameRegister, offset));
    }

    GPRReg wasmContextGPR = pinnedRegs.wasmContextPointer;

    {
        CCallHelpers::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));
        numGPRs = 0;
        numFPRs = 0;
        // We're going to set the pinned registers after this. So
        // we can use this as a scratch for now since we saved it above.
        GPRReg scratchReg = pinnedRegs.baseMemoryPointer;

        ptrdiff_t jsOffset = CallFrameSlot::thisArgument * sizeof(EncodedJSValue);

        // vmEntryToWasm passes Wasm::Context* as the first JS argument when we're
        // not using fast TLS to hold the Wasm::Context*.
        if (!useFastTLSForContext()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmContextGPR);
            jsOffset += sizeof(EncodedJSValue);
        }

        ptrdiff_t wasmOffset = CallFrame::headerSizeInRegisters * sizeof(void*);
        for (unsigned i = 0; i < signature.argumentCount(); i++) {
            switch (signature.argument(i)) {
            case Wasm::I32:
            case Wasm::I64:
                if (numGPRs >= wasmCallingConvention().m_gprArgs.size()) {
                    if (signature.argument(i) == Wasm::I32) {
                        jit.load32(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchReg);
                        jit.store32(scratchReg, calleeFrame.withOffset(wasmOffset));
                    } else {
                        jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchReg);
                        jit.store64(scratchReg, calleeFrame.withOffset(wasmOffset));
                    }
                    wasmOffset += sizeof(void*);
                } else {
                    if (signature.argument(i) == Wasm::I32)
                        jit.load32(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmCallingConvention().m_gprArgs[numGPRs].gpr());
                    else
                        jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmCallingConvention().m_gprArgs[numGPRs].gpr());
                }
                ++numGPRs;
                break;
            case Wasm::F32:
            case Wasm::F64:
                if (numFPRs >= wasmCallingConvention().m_fprArgs.size()) {
                    if (signature.argument(i) == Wasm::F32) {
                        jit.load32(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchReg);
                        jit.store32(scratchReg, calleeFrame.withOffset(wasmOffset));
                    } else {
                        jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchReg);
                        jit.store64(scratchReg, calleeFrame.withOffset(wasmOffset));
                    }
                    wasmOffset += sizeof(void*);
                } else {
                    if (signature.argument(i) == Wasm::F32)
                        jit.loadFloat(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmCallingConvention().m_fprArgs[numFPRs].fpr());
                    else
                        jit.loadDouble(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), wasmCallingConvention().m_fprArgs[numFPRs].fpr());
                }
                ++numFPRs;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            jsOffset += sizeof(EncodedJSValue);
        }
    }

    if (!!info.memory) {
        GPRReg baseMemory = pinnedRegs.baseMemoryPointer;

        if (!useFastTLSForContext())
            jit.loadPtr(CCallHelpers::Address(wasmContextGPR, JSWebAssemblyInstance::offsetOfMemory()), baseMemory);
        else {
            jit.loadWasmContext(baseMemory);
            jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyInstance::offsetOfMemory()), baseMemory);
        }

        if (mode != MemoryMode::Signaling) {
            const auto& sizeRegs = pinnedRegs.sizeRegisters;
            ASSERT(sizeRegs.size() >= 1);
            ASSERT(!sizeRegs[0].sizeOffset); // The following code assumes we start at 0, and calculates subsequent size registers relative to 0.
            jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyMemory::offsetOfSize()), sizeRegs[0].sizeRegister);
            for (unsigned i = 1; i < sizeRegs.size(); ++i)
                jit.add64(CCallHelpers::TrustedImm32(-sizeRegs[i].sizeOffset), sizeRegs[0].sizeRegister, sizeRegs[i].sizeRegister);
        }

        jit.loadPtr(CCallHelpers::Address(baseMemory, JSWebAssemblyMemory::offsetOfMemory()), baseMemory);
    }

    CCallHelpers::Call call = jit.threadSafePatchableNearCall();
    unsigned functionIndexSpace = functionIndex + info.importFunctionCount();
    ASSERT(functionIndexSpace < info.functionIndexSpaceSize());
    jit.addLinkTask([unlinkedWasmToWasmCalls, call, functionIndexSpace] (LinkBuffer& linkBuffer) {
        unlinkedWasmToWasmCalls->append({ linkBuffer.locationOfNearCall(call), functionIndexSpace });
    });


    for (const RegisterAtOffset& regAtOffset : registersToSpill) {
        GPRReg reg = regAtOffset.reg().gpr();
        ASSERT(reg != GPRInfo::returnValueGPR);
        ptrdiff_t offset = regAtOffset.offset();
        jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, offset), reg);
    }

    switch (signature.returnType()) {
    case Wasm::F32:
        jit.moveFloatTo32(FPRInfo::returnValueFPR, GPRInfo::returnValueGPR);
        break;
    case Wasm::F64:
        jit.moveDoubleTo64(FPRInfo::returnValueFPR, GPRInfo::returnValueGPR);
        break;
    default:
        break;
    }

    jit.emitFunctionEpilogue();
    jit.ret();

    return result;
}

auto B3IRGenerator::origin() -> Origin
{
    OpcodeOrigin origin(m_parser->currentOpcode(), m_parser->currentOpcodeStartingOffset());
    ASSERT(isValidOpType(static_cast<uint8_t>(origin.opcode())));
    return bitwise_cast<Origin>(origin);
}

Expected<std::unique_ptr<InternalFunction>, String> parseAndCompile(CompilationContext& compilationContext, const uint8_t* functionStart, size_t functionLength, const Signature& signature, Vector<UnlinkedWasmToWasmCall>& unlinkedWasmToWasmCalls, const ModuleInformation& info, MemoryMode mode, CompilationMode compilationMode, uint32_t functionIndex, TierUpCount* tierUp)
{
    auto result = std::make_unique<InternalFunction>();

    compilationContext.jsEntrypointJIT = std::make_unique<CCallHelpers>();
    compilationContext.wasmEntrypointJIT = std::make_unique<CCallHelpers>();

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
        ? Options::webAssemblyBBQOptimizationLevel()
        : Options::webAssemblyOMGOptimizationLevel());

    B3IRGenerator context(info, procedure, result.get(), unlinkedWasmToWasmCalls, mode, compilationMode, functionIndex, tierUp);
    FunctionParser<B3IRGenerator> parser(context, functionStart, functionLength, signature, info);
    WASM_FAIL_IF_HELPER_FAILS(parser.parse());

    context.insertConstants();

    procedure.resetReachability();
    if (!ASSERT_DISABLED)
        validate(procedure, "After parsing:\n");

    dataLogIf(verbose, "Pre SSA: ", procedure);
    fixSSA(procedure);
    dataLogIf(verbose, "Post SSA: ", procedure);
    
    {
        B3::prepareForGeneration(procedure);
        B3::generate(procedure, *compilationContext.wasmEntrypointJIT);
        compilationContext.wasmEntrypointByproducts = procedure.releaseByproducts();
        result->entrypoint.calleeSaveRegisters = procedure.calleeSaveRegisterAtOffsetList();
    }

    return WTFMove(result);
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
    // FIXME: This should use the popcnt instruction if SSE4 is available but we don't have code to detect SSE4 yet.
    // see: https://bugs.webkit.org/show_bug.cgi?id=165363
    uint32_t (*popcount)(int32_t) = [] (int32_t value) -> uint32_t { return __builtin_popcount(value); };
    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), bitwise_cast<void*>(popcount));
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int32, origin(), Effects::none(), funcAddress, arg);
    return { };
}

template<>
auto B3IRGenerator::addOp<OpType::I64Popcnt>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    // FIXME: This should use the popcnt instruction if SSE4 is available but we don't have code to detect SSE4 yet.
    // see: https://bugs.webkit.org/show_bug.cgi?id=165363
    uint64_t (*popcount)(int64_t) = [] (int64_t value) -> uint64_t { return __builtin_popcountll(value); };
    Value* funcAddress = m_currentBlock->appendNew<ConstPtrValue>(m_proc, origin(), bitwise_cast<void*>(popcount));
    result = m_currentBlock->appendNew<CCallValue>(m_proc, Int64, origin(), Effects::none(), funcAddress, arg);
    return { };
}

template<>
auto B3IRGenerator::addOp<F64ConvertUI64>(ExpressionType arg, ExpressionType& result) -> PartialResult
{
    PatchpointValue* patchpoint = m_currentBlock->appendNew<PatchpointValue>(m_proc, Double, origin());
    if (isX86())
        patchpoint->numGPScratchRegisters = 1;
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
    Value* min = constant(Double, bitwise_cast<uint64_t>(static_cast<double>(std::numeric_limits<int32_t>::min())));
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

#endif // ENABLE(WEBASSEMBLY)
