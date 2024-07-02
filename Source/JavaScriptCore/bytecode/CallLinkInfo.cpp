/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "CallLinkInfo.h"

#include "CCallHelpers.h"
#include "CallFrameShuffleData.h"
#include "DFGJITCode.h"
#include "DisallowMacroScratchRegisterUsage.h"
#include "FunctionCodeBlock.h"
#include "JITThunks.h"
#include "JSCellInlines.h"
#include "JSWebAssemblyModule.h"
#include "LLIntEntrypoint.h"
#include "LinkBuffer.h"
#include "Opcode.h"
#include "Repatch.h"
#include "ThunkGenerators.h"
#include <wtf/ListDump.h>

namespace JSC {

CallLinkInfo::CallType CallLinkInfo::callTypeFor(OpcodeID opcodeID)
{
    switch (opcodeID) {
    case op_tail_call_varargs:
    case op_tail_call_forward_arguments:
        return TailCallVarargs;        

    case op_call:
    case op_call_ignore_result:
    case op_call_direct_eval:
    case op_iterator_open:
    case op_iterator_next:
        return Call;

    case op_call_varargs:
        return CallVarargs;

    case op_construct:
        return Construct;

    case op_construct_varargs:
        return ConstructVarargs;

    case op_tail_call:
        return TailCall;

    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return Call;
}

CallLinkInfo::~CallLinkInfo()
{
    clearStub();
}

void CallLinkInfo::clearStub()
{
    if (!stub())
        return;

    m_stub->clearCallNodesFor(this);
    m_stub = nullptr;
}

void CallLinkInfo::unlinkOrUpgradeImpl(VM& vm, CodeBlock* oldCodeBlock, CodeBlock* newCodeBlock)
{
    // We could be called even if we're not linked anymore because of how polymorphic calls
    // work. Each callsite within the polymorphic call stub may separately ask us to unlink().
    if (isOnList())
        remove();

    dataLogLnIf(Options::dumpDisassembly(), "Unlinking CallLinkInfo: ", RawPointer(this));

    Mode mode = this->mode();
    switch (mode) {
    case Mode::Monomorphic: {
        if (newCodeBlock && oldCodeBlock == m_codeBlock) {
            // Upgrading Monomorphic DataIC with newCodeBlock.
            ArityCheckMode arityCheck = oldCodeBlock->jitCode()->addressForCall(ArityCheckNotRequired) == m_monomorphicCallDestination ? ArityCheckNotRequired : MustCheckArity;
            auto target = newCodeBlock->jitCode()->addressForCall(arityCheck);
            m_codeBlock = newCodeBlock;
            m_monomorphicCallDestination = target;
            newCodeBlock->linkIncomingCall(nullptr, this); // This is just relinking. So owner and caller frame can be nullptr.
            return;
        }
        revertCall(vm);
        break;
    }
    case Mode::Polymorphic: {
        revertCall(vm);
        break;
    }
    case Mode::Init:
    case Mode::Virtual: {
        break;
    }
    }

    // Either we were unlinked, in which case we should not have been on any list, or we unlinked
    // ourselves so that we're not on any list anymore.
    RELEASE_ASSERT(!isOnList(), static_cast<unsigned>(mode));
}

void CallLinkInfo::setMonomorphicCallee(VM& vm, JSCell* owner, JSObject* callee, CodeBlock* codeBlock, CodePtr<JSEntryPtrTag> codePtr)
{
    RELEASE_ASSERT(!(bitwise_cast<uintptr_t>(callee) & polymorphicCalleeMask));
    m_callee.set(vm, owner, callee);
    m_codeBlock = codeBlock;
    m_monomorphicCallDestination = codePtr;
    m_mode = static_cast<unsigned>(Mode::Monomorphic);
}

void CallLinkInfo::clearCallee()
{
    m_callee.clear();
    m_codeBlock = nullptr;
    m_monomorphicCallDestination = nullptr;
}

JSObject* CallLinkInfo::callee()
{
    RELEASE_ASSERT(!(bitwise_cast<uintptr_t>(m_callee.get()) & polymorphicCalleeMask));
    return m_callee.get();
}

void CallLinkInfo::setLastSeenCallee(VM& vm, const JSCell* owner, JSObject* callee)
{
    m_lastSeenCallee.set(vm, owner, callee);
}

JSObject* CallLinkInfo::lastSeenCallee() const
{
    return m_lastSeenCallee.get();
}

bool CallLinkInfo::haveLastSeenCallee() const
{
    return !!m_lastSeenCallee;
}

void CallLinkInfo::visitWeak(VM& vm)
{
    auto handleSpecificCallee = [&] (JSFunction* callee) {
        if (vm.heap.isMarked(callee->executable()))
            m_hasSeenClosure = true;
        else
            m_clearedByGC = true;
    };
    
    switch (mode()) {
    case Mode::Init:
    case Mode::Virtual:
        break;
    case Mode::Polymorphic: {
        if (stub()) {
            if (!stub()->visitWeak(vm)) {
                dataLogLnIf(Options::verboseOSR(), "At ", codeOrigin(), ", ", RawPointer(this), ": clearing call stub to ", listDump(stub()->variants()), ", stub routine ", RawPointer(stub()), ".");
                unlinkOrUpgrade(vm, nullptr, nullptr);
                m_clearedByGC = true;
            }
        }
        break;
    }
    case Mode::Monomorphic: {
        auto* callee = m_callee.get();
        if (callee && !vm.heap.isMarked(callee)) {
            if (callee->type() == JSFunctionType) {
                dataLogLnIf(Options::verboseOSR(), "Clearing call to ", RawPointer(callee), " (", static_cast<JSFunction*>(callee)->executable()->hashFor(specializationKind()), ").");
                handleSpecificCallee(static_cast<JSFunction*>(callee));
            } else {
                dataLogLnIf(Options::verboseOSR(), "Clearing call to ", RawPointer(callee), ".");
                m_clearedByGC = true;
            }
            unlinkOrUpgrade(vm, nullptr, nullptr);
        }
        break;
    }
    }

    if (haveLastSeenCallee() && !vm.heap.isMarked(lastSeenCallee())) {
        if (lastSeenCallee()->type() == JSFunctionType)
            handleSpecificCallee(jsCast<JSFunction*>(lastSeenCallee()));
        else
            m_clearedByGC = true;
        m_lastSeenCallee.clear();
    }
}

void CallLinkInfo::revertCallToStub()
{
    RELEASE_ASSERT(stub());
    // The start of our JIT code is now a jump to the polymorphic stub. Rewrite the first instruction
    // to be what we need for non stub ICs.

    // this runs into some branch compaction crap I'd like to avoid for now. Essentially, the branch
    // doesn't know if it can be compacted or not. So we end up with 28 bytes of machine code, for
    // what in all likelihood fits in 24. So we just splat out the first instruction. Long term, we
    // need something cleaner. But this works on arm64 for now.

    m_callee.clear();
    m_codeBlock = nullptr;
    m_monomorphicCallDestination = nullptr;
}

void DataOnlyCallLinkInfo::initialize(VM& vm, CodeBlock* owner, CallType callType, CodeOrigin codeOrigin)
{
    m_owner = owner;
    m_type = static_cast<unsigned>(Type::DataOnly);
    ASSERT(Type::DataOnly == type());
    m_codeOrigin = codeOrigin;
    m_callType = callType;
    m_mode = static_cast<unsigned>(Mode::Init);
    if (UNLIKELY(!Options::useLLIntICs()))
        setVirtualCall(vm);
}

std::tuple<CodeBlock*, BytecodeIndex> CallLinkInfo::retrieveCaller(JSCell* owner)
{
    auto* codeBlock = jsDynamicCast<CodeBlock*>(owner);
    if (!codeBlock)
        return { };
    CodeOrigin codeOrigin = this->codeOrigin();
    if (auto* baselineCodeBlock = codeOrigin.codeOriginOwner())
        return std::tuple { baselineCodeBlock, codeOrigin.bytecodeIndex() };
    return std::tuple { codeBlock, codeOrigin.bytecodeIndex() };
}

void CallLinkInfo::reset(VM&)
{
    if (stub())
        revertCallToStub();
    clearCallee(); // This also clears the inline cache both for data and code-based caches.
    clearSeen();
    clearStub();
    if (isOnList())
        remove();
    m_mode = static_cast<unsigned>(Mode::Init);
}

void CallLinkInfo::revertCall(VM& vm)
{
    if (UNLIKELY(!Options::useLLIntICs() && type() == CallLinkInfo::Type::DataOnly))
        setVirtualCall(vm);
    else
        reset(vm);
}

void CallLinkInfo::setVirtualCall(VM& vm)
{
    reset(vm);
    m_callee.clear();
    *bitwise_cast<uintptr_t*>(m_callee.slot()) = polymorphicCalleeMask;
    m_codeBlock = nullptr; // PolymorphicCallStubRoutine will set CodeBlock inside it.
    m_monomorphicCallDestination = vm.getCTIVirtualCall(callMode()).code().template retagged<JSEntryPtrTag>();

    setClearedByVirtual();
    m_mode = static_cast<unsigned>(Mode::Virtual);
}

JSGlobalObject* CallLinkInfo::globalObjectForSlowPath(JSCell* owner)
{
    auto [codeBlock, bytecodeIndex] = retrieveCaller(owner);
    if (codeBlock)
        return codeBlock->globalObject();
#if ENABLE(WEBASSEMBLY)
    auto* module = jsDynamicCast<JSWebAssemblyModule*>(owner);
    if (module)
        return module->globalObject();
#endif
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void CallLinkInfo::setStub(Ref<PolymorphicCallStubRoutine>&& newStub)
{
    clearStub();
    m_stub = WTFMove(newStub);

    m_callee.clear();
    *bitwise_cast<uintptr_t*>(m_callee.slot()) = polymorphicCalleeMask;
    m_codeBlock = nullptr; // PolymorphicCallStubRoutine will set CodeBlock inside it.
    m_monomorphicCallDestination = m_stub->code().code().retagged<JSEntryPtrTag>();

    // The call link info no longer has a call cache apart from the jump to the polymorphic call stub.
    if (isOnList())
        remove();

    m_mode = static_cast<unsigned>(Mode::Polymorphic);
}

#if ENABLE(JIT)

void CallLinkInfo::emitFastPathImpl(CallLinkInfo* callLinkInfo, CCallHelpers& jit, bool isTailCall, ScopedLambda<void()>&& prepareForTailCall)
{
    if (callLinkInfo)
        jit.move(CCallHelpers::TrustedImmPtr(callLinkInfo), BaselineJITRegisters::Call::callLinkInfoGPR);
#if USE(JSVALUE32_64)
    // We need this on JSVALUE32_64 only as on JSVALUE64 a pointer comparison in the DataIC fast
    // path catches this.
    auto failed = jit.branchIfNotCell(BaselineJITRegisters::Call::calleeJSR);
#endif

    // For RISCV64, scratch register usage here collides with MacroAssembler's internal usage
    // that's necessary for the test-and-branch operation but is avoidable by loading from the callee
    // address for each branch operation. Other MacroAssembler implementations handle this better by
    // using a wider range of scratch registers or more potent branching instructions.
    CCallHelpers::JumpList found;
    jit.loadPtr(CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfMonomorphicCallDestination()), BaselineJITRegisters::Call::callTargetGPR);
    if constexpr (isRISCV64()) {
        CCallHelpers::Address calleeAddress(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfCallee());
        found.append(jit.branchPtr(CCallHelpers::Equal, calleeAddress, BaselineJITRegisters::Call::calleeGPR));
        found.append(jit.branchTestPtr(CCallHelpers::NonZero, calleeAddress, CCallHelpers::TrustedImm32(polymorphicCalleeMask)));
    } else {
        GPRReg scratchGPR = jit.scratchRegister();
        DisallowMacroScratchRegisterUsage disallowScratch(jit);
        jit.loadPtr(CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfCallee()), scratchGPR);
        found.append(jit.branchPtr(CCallHelpers::Equal, scratchGPR, BaselineJITRegisters::Call::calleeGPR));
        found.append(jit.branchTestPtr(CCallHelpers::NonZero, scratchGPR, CCallHelpers::TrustedImm32(polymorphicCalleeMask)));
    }

#if USE(JSVALUE32_64)
    failed.link(&jit);
#endif
    jit.move(CCallHelpers::TrustedImmPtr(LLInt::defaultCall().code().taggedPtr()), BaselineJITRegisters::Call::callTargetGPR);

    found.link(&jit);
    if (isTailCall) {
        prepareForTailCall();
        jit.transferPtr(CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfCodeBlock()), CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
        jit.farJump(BaselineJITRegisters::Call::callTargetGPR, JSEntryPtrTag);
    } else {
        jit.transferPtr(CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfCodeBlock()), CCallHelpers::calleeFrameCodeBlockBeforeCall());
        jit.call(BaselineJITRegisters::Call::callTargetGPR, JSEntryPtrTag);
    }
    return;
}

void CallLinkInfo::emitDataICFastPath(CCallHelpers& jit)
{
    emitFastPathImpl(nullptr, jit, false, nullptr);
}

void CallLinkInfo::emitTailCallDataICFastPath(CCallHelpers& jit, ScopedLambda<void()>&& prepareForTailCall)
{
    emitFastPathImpl(nullptr, jit, true, WTFMove(prepareForTailCall));
}

void CallLinkInfo::emitFastPath(CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo))
        return std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitFastPath(jit);

    return CallLinkInfo::emitDataICFastPath(jit);
}

void CallLinkInfo::emitTailCallFastPath(CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo, ScopedLambda<void()>&& prepareForTailCall)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo))
        return std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitTailCallFastPath(jit, WTFMove(prepareForTailCall));

    return CallLinkInfo::emitTailCallDataICFastPath(jit, WTFMove(prepareForTailCall));
}

void OptimizingCallLinkInfo::emitFastPath(CCallHelpers& jit)
{
    RELEASE_ASSERT(!isTailCall());
    emitFastPathImpl(this, jit, isTailCall(), nullptr);
}

void OptimizingCallLinkInfo::emitTailCallFastPath(CCallHelpers& jit, ScopedLambda<void()>&& prepareForTailCall)
{
    RELEASE_ASSERT(isTailCall());
    emitFastPathImpl(this, jit, isTailCall(), WTFMove(prepareForTailCall));
}

#if ENABLE(DFG_JIT)
void OptimizingCallLinkInfo::initializeFromDFGUnlinkedCallLinkInfo(VM&, const DFG::UnlinkedCallLinkInfo& unlinkedCallLinkInfo, CodeBlock* owner)
{
    m_owner = owner;
    m_codeOrigin = unlinkedCallLinkInfo.codeOrigin;
    m_callType = unlinkedCallLinkInfo.callType;
}
#endif

void DirectCallLinkInfo::reset()
{
    if (isOnList())
        remove();
#if ENABLE(JIT)
    if (!isDataIC())
        initialize();
#endif
    m_target = { };
    m_codeBlock = nullptr;
}

void DirectCallLinkInfo::unlinkOrUpgradeImpl(VM&, CodeBlock* oldCodeBlock, CodeBlock* newCodeBlock)
{
    if (isOnList())
        remove();

    if (!!m_target) {
        if (m_codeBlock && newCodeBlock && oldCodeBlock == m_codeBlock) {
            ArityCheckMode arityCheck = oldCodeBlock->jitCode()->addressForCall(ArityCheckNotRequired) == m_target ? ArityCheckNotRequired : MustCheckArity;
            auto target = newCodeBlock->jitCode()->addressForCall(arityCheck);
            setCallTarget(newCodeBlock, CodeLocationLabel { target });
            newCodeBlock->linkIncomingCall(nullptr, this); // This is just relinking. So owner and caller frame can be nullptr.
            return;
        }
        dataLogLnIf(Options::dumpDisassembly(), "Unlinking CallLinkInfo: ", RawPointer(this));
        reset();
    }

    // Either we were unlinked, in which case we should not have been on any list, or we unlinked
    // ourselves so that we're not on any list anymore.
    RELEASE_ASSERT(!isOnList());
}

void DirectCallLinkInfo::visitWeak(VM& vm)
{
    if (m_codeBlock && !vm.heap.isMarked(m_codeBlock)) {
        dataLogLnIf(Options::verboseOSR(), "Clearing call to ", RawPointer(m_codeBlock), " (", pointerDump(m_codeBlock), ").");
        unlinkOrUpgrade(vm, nullptr, nullptr);
    }
}

CCallHelpers::JumpList DirectCallLinkInfo::emitDirectFastPath(CCallHelpers& jit)
{
    RELEASE_ASSERT(!isTailCall());

    if (isDataIC()) {
        CCallHelpers::JumpList slowPath;
        jit.move(CCallHelpers::TrustedImmPtr(this), BaselineJITRegisters::Call::callLinkInfoGPR);
        slowPath.append(jit.branchTestPtr(CCallHelpers::Zero, CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfTarget())));
        jit.transferPtr(CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfCodeBlock()), CCallHelpers::calleeFrameCodeBlockBeforeCall());
        jit.call(CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfTarget()), JSEntryPtrTag);
        return slowPath;
    }

    auto codeBlockStore = jit.storePtrWithPatch(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::calleeFrameCodeBlockBeforeCall());
    auto call = jit.nearCall();
    jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
        m_codeBlockLocation = linkBuffer.locationOf<JSInternalPtrTag>(codeBlockStore);
    });
    jit.addLateLinkTask([this](LinkBuffer&) {
        repatchSpeculatively();
    });
    return { };
}

CCallHelpers::JumpList DirectCallLinkInfo::emitDirectTailCallFastPath(CCallHelpers& jit, ScopedLambda<void()>&& prepareForTailCall)
{
    RELEASE_ASSERT(isTailCall());

    if (isDataIC()) {
        CCallHelpers::JumpList slowPath;
        jit.move(CCallHelpers::TrustedImmPtr(this), BaselineJITRegisters::Call::callLinkInfoGPR);
        slowPath.append(jit.branchTestPtr(CCallHelpers::Zero, CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfTarget())));
        prepareForTailCall();
        jit.transferPtr(CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfCodeBlock()), CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
        jit.farJump(CCallHelpers::Address(BaselineJITRegisters::Call::callLinkInfoGPR, offsetOfTarget()), JSEntryPtrTag);
        return slowPath;
    }

    auto fastPathStart = jit.label();

    // - If we're not yet linked, this is a jump to the slow path.
    // - Once we're linked to a fast path, this goes back to being nops so we fall through to the linked jump.
    jit.emitNops(CCallHelpers::patchableJumpSize());

    prepareForTailCall();
    auto codeBlockStore = jit.storePtrWithPatch(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
    auto call = jit.nearTailCall();
    jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        m_fastPathStart = linkBuffer.locationOf<JSInternalPtrTag>(fastPathStart);
        m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
        m_codeBlockLocation = linkBuffer.locationOf<JSInternalPtrTag>(codeBlockStore);
    });
    jit.addLateLinkTask([this](LinkBuffer&) {
        repatchSpeculatively();
    });
    return { };
}

void DirectCallLinkInfo::initialize()
{
    ASSERT(m_callLocation);
    ASSERT(m_codeBlockLocation);
    if (isTailCall()) {
        RELEASE_ASSERT(fastPathStart());
        CCallHelpers::replaceWithJump(fastPathStart(), slowPathStart());
    } else
        MacroAssembler::repatchNearCall(m_callLocation, slowPathStart());
}

void DirectCallLinkInfo::setCallTarget(CodeBlock* codeBlock, CodeLocationLabel<JSEntryPtrTag> target)
{
    m_codeBlock = codeBlock;
    m_target = target;

    if (!isDataIC()) {
        if (isTailCall()) {
            RELEASE_ASSERT(fastPathStart());
            // We reserved this many bytes for the jump at fastPathStart(). Make that
            // code nops now so we fall through to the jump to the fast path.
            CCallHelpers::replaceWithNops(fastPathStart(), CCallHelpers::patchableJumpSize());
        }

        MacroAssembler::repatchNearCall(m_callLocation, target);
        MacroAssembler::repatchPointer(m_codeBlockLocation, codeBlock);
    }
}

void DirectCallLinkInfo::setMaxArgumentCountIncludingThis(unsigned value)
{
    RELEASE_ASSERT(value);
    m_maxArgumentCountIncludingThis = value;
}

CodeBlock* DirectCallLinkInfo::retrieveCodeBlock(FunctionExecutable* functionExecutable)
{
    CodeSpecializationKind kind = specializationKind();
    CodeBlock* codeBlock = functionExecutable->codeBlockFor(kind);
    if (!codeBlock)
        return nullptr;

    CodeBlock* ownerCodeBlock = jsDynamicCast<CodeBlock*>(owner());
    if (!ownerCodeBlock)
        return nullptr;

    if (ownerCodeBlock->alternative() == codeBlock)
        return nullptr;

    return codeBlock;
}

CodePtr<JSEntryPtrTag> DirectCallLinkInfo::retrieveCodePtr(const ConcurrentJSLocker& locker, CodeBlock* codeBlock)
{
    unsigned argumentStackSlots = maxArgumentCountIncludingThis();
    ArityCheckMode arityCheckMode = (argumentStackSlots < static_cast<size_t>(codeBlock->numParameters())) ? MustCheckArity : ArityCheckNotRequired;
    return codeBlock->addressForCallConcurrently(locker, arityCheckMode);
}

void DirectCallLinkInfo::repatchSpeculatively()
{
    if (m_executable->isHostFunction()) {
        CodeSpecializationKind kind = specializationKind();
        CodePtr<JSEntryPtrTag> codePtr;
        if (kind == CodeForCall)
            codePtr = m_executable->generatedJITCodeWithArityCheckForCall();
        else
            codePtr = m_executable->generatedJITCodeWithArityCheckForConstruct();
        if (codePtr)
            setCallTarget(nullptr, CodeLocationLabel { codePtr });
        else
            initialize();
        return;
    }

    FunctionExecutable* functionExecutable = jsDynamicCast<FunctionExecutable*>(m_executable);
    if (!functionExecutable) {
        initialize();
        return;
    }

    auto* codeBlock = retrieveCodeBlock(functionExecutable);
    if (codeBlock) {
        auto codePtr = retrieveCodePtr(ConcurrentJSLocker { codeBlock->m_lock }, codeBlock);
        if (codePtr) {
            m_codeBlock = codeBlock;
            m_target = codePtr;
            // Do not chain |this| to the calle codeBlock concurrently. It will be done in the main thread if the speculatively repatched one is still valid.
            setCallTarget(codeBlock, CodeLocationLabel { codePtr });
            return;
        }
    }

    initialize();
}

void DirectCallLinkInfo::validateSpeculativeRepatchOnMainThread(VM&)
{
    constexpr bool verbose = false;
    FunctionExecutable* functionExecutable = jsDynamicCast<FunctionExecutable*>(m_executable);
    if (!functionExecutable)
        return;

    auto* codeBlock = retrieveCodeBlock(functionExecutable);
    CodePtr<JSEntryPtrTag> codePtr = nullptr;
    if (codeBlock)
        codePtr = retrieveCodePtr(ConcurrentJSLocker { NoLockingNecessary }, codeBlock);

    if (m_codeBlock != codeBlock || m_target != codePtr) {
        if (codeBlock && codePtr)
            setCallTarget(codeBlock, CodeLocationLabel { codePtr });
        else
            reset();
    } else
        dataLogLnIf(verbose, "Speculative repatching succeeded ", RawPointer(m_codeBlock), " ", m_target);

    if (m_codeBlock)
        m_codeBlock->linkIncomingCall(owner(), this);
}

#endif

} // namespace JSC
