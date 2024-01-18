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
#if ENABLE(JIT)
    if (!stub())
        return;

    m_stub->clearCallNodesFor(this);
    m_stub = nullptr;
#endif
}

void CallLinkInfo::unlinkOrUpgradeImpl(VM& vm, CodeBlock* oldCodeBlock, CodeBlock* newCodeBlock)
{
    // We could be called even if we're not linked anymore because of how polymorphic calls
    // work. Each callsite within the polymorphic call stub may separately ask us to unlink().
    if (isLinked()) {
        if (newCodeBlock && isDataIC() && mode() == Mode::Monomorphic && oldCodeBlock == u.dataIC.m_codeBlock) {
            // Upgrading Monomorphic DataIC with newCodeBlock.
            remove();
            ArityCheckMode arityCheck = oldCodeBlock->jitCode()->addressForCall(ArityCheckNotRequired) == u.dataIC.m_monomorphicCallDestination ? ArityCheckNotRequired : MustCheckArity;
            auto target = newCodeBlock->jitCode()->addressForCall(arityCheck);
            u.dataIC.m_codeBlock = newCodeBlock;
            u.dataIC.m_monomorphicCallDestination = target;
            newCodeBlock->linkIncomingCall(owner(), this); // This is just relinking. So owner and caller frame can be nullptr.
            return;
        }
        dataLogLnIf(Options::dumpDisassembly(), "Unlinking CallLinkInfo: ", RawPointer(this));
        revertCall(vm);
    }

    // Either we were unlinked, in which case we should not have been on any list, or we unlinked
    // ourselves so that we're not on any list anymore.
    RELEASE_ASSERT(!isOnList());
}

CodeLocationLabel<JSInternalPtrTag> CallLinkInfo::doneLocation()
{
    RELEASE_ASSERT(!isDirect());
    return m_doneLocation;
}

void CallLinkInfo::setMonomorphicCallee(VM& vm, JSCell* owner, JSObject* callee, CodeBlock* codeBlock, CodePtr<JSEntryPtrTag> codePtr)
{
    RELEASE_ASSERT(!isDirect());
    RELEASE_ASSERT(!(bitwise_cast<uintptr_t>(callee) & polymorphicCalleeMask));
    m_calleeOrCodeBlock.set(vm, owner, callee);

    if (isDataIC()) {
        u.dataIC.m_codeBlock = codeBlock;
        u.dataIC.m_monomorphicCallDestination = codePtr;
    } else {
#if ENABLE(JIT)
        MacroAssembler::repatchNearCall(static_cast<OptimizingCallLinkInfo*>(this)->m_callLocation, CodeLocationLabel<JSEntryPtrTag>(codePtr));
        MacroAssembler::repatchPointer(u.codeIC.m_codeBlockLocation, codeBlock);
        MacroAssembler::repatchPointer(u.codeIC.m_calleeLocation, callee);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }
    m_mode = static_cast<unsigned>(Mode::Monomorphic);
}

void CallLinkInfo::clearCallee()
{
    RELEASE_ASSERT(!isDirect());
    m_calleeOrCodeBlock.clear();
    if (isDataIC()) {
        u.dataIC.m_codeBlock = nullptr;
        u.dataIC.m_monomorphicCallDestination = nullptr;
    } else {
#if ENABLE(JIT)
        MacroAssembler::repatchPointer(u.codeIC.m_codeBlockLocation, nullptr);
        MacroAssembler::repatchPointer(u.codeIC.m_calleeLocation, nullptr);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }
}

JSObject* CallLinkInfo::callee()
{
    RELEASE_ASSERT(!isDirect());
    RELEASE_ASSERT(!(bitwise_cast<uintptr_t>(m_calleeOrCodeBlock.get()) & polymorphicCalleeMask));
    return jsCast<JSObject*>(m_calleeOrCodeBlock.get());
}

void CallLinkInfo::setCodeBlock(VM& vm, JSCell* owner, FunctionCodeBlock* codeBlock)
{
    RELEASE_ASSERT(isDirect());
    m_calleeOrCodeBlock.setMayBeNull(vm, owner, codeBlock);
}

void CallLinkInfo::clearCodeBlock()
{
    RELEASE_ASSERT(isDirect());
    m_calleeOrCodeBlock.clear();
}

FunctionCodeBlock* CallLinkInfo::codeBlock()
{
    RELEASE_ASSERT(isDirect());
    return jsCast<FunctionCodeBlock*>(m_calleeOrCodeBlock.get());
}

void CallLinkInfo::setLastSeenCallee(VM& vm, const JSCell* owner, JSObject* callee)
{
    RELEASE_ASSERT(!isDirect());
    m_lastSeenCalleeOrExecutable.set(vm, owner, callee);
}

void CallLinkInfo::clearLastSeenCallee()
{
    RELEASE_ASSERT(!isDirect());
    m_lastSeenCalleeOrExecutable.clear();
}

JSObject* CallLinkInfo::lastSeenCallee() const
{
    RELEASE_ASSERT(!isDirect());
    return jsCast<JSObject*>(m_lastSeenCalleeOrExecutable.get());
}

bool CallLinkInfo::haveLastSeenCallee() const
{
    RELEASE_ASSERT(!isDirect());
    return !!m_lastSeenCalleeOrExecutable;
}

void CallLinkInfo::setExecutableDuringCompilation(ExecutableBase* executable)
{
    RELEASE_ASSERT(isDirect());
    m_lastSeenCalleeOrExecutable.setWithoutWriteBarrier(executable);
}

ExecutableBase* CallLinkInfo::executable()
{
    RELEASE_ASSERT(isDirect());
    return jsCast<ExecutableBase*>(m_lastSeenCalleeOrExecutable.get());
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
#if ENABLE(JIT)
            if (!stub()->visitWeak(vm)) {
                if (UNLIKELY(Options::verboseOSR())) {
                    dataLog(
                        "At ", codeOrigin(), ", ", RawPointer(this), ": clearing call stub to ",
                        listDump(stub()->variants()), ", stub routine ", RawPointer(stub()),
                        ".\n");
                }
                unlinkOrUpgrade(vm, nullptr, nullptr);
                m_clearedByGC = true;
            }
#else
            RELEASE_ASSERT_NOT_REACHED();
#endif
        }
        break;
    }
    case Mode::Monomorphic:
    case Mode::LinkedDirect: {
        auto* calleeOrCodeBlock = m_calleeOrCodeBlock.get();
        if (calleeOrCodeBlock && !vm.heap.isMarked(calleeOrCodeBlock)) {
            if (isDirect()) {
                if (UNLIKELY(Options::verboseOSR())) {
                    dataLog(
                        "Clearing call to ", RawPointer(codeBlock()), " (",
                        pointerDump(codeBlock()), ").\n");
                }
            } else {
                JSObject* callee = jsCast<JSObject*>(calleeOrCodeBlock);
                if (callee->type() == JSFunctionType) {
                    if (UNLIKELY(Options::verboseOSR())) {
                        dataLog(
                            "Clearing call to ",
                            RawPointer(callee), " (",
                            static_cast<JSFunction*>(callee)->executable()->hashFor(specializationKind()),
                            ").\n");
                    }
                    handleSpecificCallee(static_cast<JSFunction*>(callee));
                } else {
                    if (UNLIKELY(Options::verboseOSR()))
                        dataLog("Clearing call to ", RawPointer(callee), ".\n");
                    m_clearedByGC = true;
                }
            }
            unlinkOrUpgrade(vm, nullptr, nullptr);
        } else if (isDirect() && !vm.heap.isMarked(m_lastSeenCalleeOrExecutable.get())) {
            if (UNLIKELY(Options::verboseOSR())) {
                dataLog(
                    "Clearing call to ", RawPointer(executable()),
                    " because the executable is dead.\n");
            }
            unlinkOrUpgrade(vm, nullptr, nullptr);
            // We should only get here once the owning CodeBlock is dying, since the executable must
            // already be in the owner's weak references.
            m_lastSeenCalleeOrExecutable.clear();
        }
        break;
    }
    }

    if (!isDirect() && haveLastSeenCallee() && !vm.heap.isMarked(lastSeenCallee())) {
        if (lastSeenCallee()->type() == JSFunctionType)
            handleSpecificCallee(jsCast<JSFunction*>(lastSeenCallee()));
        else
            m_clearedByGC = true;
        clearLastSeenCallee();
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

    if (isDataIC()) {
        m_calleeOrCodeBlock.clear();
        u.dataIC.m_codeBlock = nullptr;
        u.dataIC.m_monomorphicCallDestination = nullptr;
    } else {
#if ENABLE(JIT)
        MacroAssembler::repatchPointer(u.codeIC.m_codeBlockLocation, nullptr);
        CCallHelpers::revertJumpReplacementToBranchPtrWithPatch(CCallHelpers::startOfBranchPtrWithPatchOnRegister(u.codeIC.m_calleeLocation), BaselineJITRegisters::Call::calleeGPR, nullptr);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }
}

void BaselineCallLinkInfo::initialize(VM& vm, CodeBlock* owner, CallType callType, BytecodeIndex bytecodeIndex)
{
    m_owner = owner;
    m_type = static_cast<unsigned>(Type::Baseline);
    ASSERT(Type::Baseline == type());
    m_useDataIC = static_cast<unsigned>(UseDataIC::Yes);
    ASSERT(UseDataIC::Yes == useDataIC());
    m_bytecodeIndex = bytecodeIndex;
    m_callType = callType;
    m_mode = static_cast<unsigned>(Mode::Init);
    // If JIT is disabled, we should not support dynamically generated call IC.
    if (!Options::useJIT())
        disallowStubs();
    if (UNLIKELY(!Options::useLLIntICs()))
        setVirtualCall(vm, owner);
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
    if (isDirect()) {
#if ENABLE(JIT)
        clearCodeBlock();
        static_cast<OptimizingCallLinkInfo&>(*this).initializeDirectCall();
#endif
    } else {
#if ENABLE(JIT)
        if (type() == CallLinkInfo::Type::Optimizing)
            static_cast<OptimizingCallLinkInfo*>(this)->setSlowPathCallDestination(LLInt::defaultCall().code());
#endif
        if (stub())
            revertCallToStub();
        clearCallee(); // This also clears the inline cache both for data and code-based caches.
    }
    clearSeen();
    clearStub();
    if (isOnList())
        remove();
    m_mode = static_cast<unsigned>(Mode::Init);
}

void CallLinkInfo::revertCall(VM& vm)
{
    if (UNLIKELY(!Options::useLLIntICs() && type() == CallLinkInfo::Type::Baseline))
        setVirtualCall(vm, owner()); // Since this is Baseline, we can always use owner().
    else
        reset(vm);
}

void CallLinkInfo::setVirtualCall(VM& vm, JSCell* owner)
{
    reset(vm);
#if ENABLE(JIT)
    if (type() == Type::Optimizing)
        static_cast<OptimizingCallLinkInfo*>(this)->setSlowPathCallDestination(vm.getCTIVirtualCall(callMode()).retagged<JSEntryPtrTag>().code());
#endif
    if (isDataIC()) {
        m_calleeOrCodeBlock.clear();
        *bitwise_cast<uintptr_t*>(m_calleeOrCodeBlock.slot()) = (bitwise_cast<uintptr_t>(owner) | polymorphicCalleeMask);
        u.dataIC.m_codeBlock = nullptr; // PolymorphicCallStubRoutine will set CodeBlock inside it.
        u.dataIC.m_monomorphicCallDestination = vm.getCTIVirtualCall(callMode()).code().template retagged<JSEntryPtrTag>();
    }
    setClearedByVirtual();
    m_mode = static_cast<unsigned>(Mode::Virtual);
}

#if ENABLE(JIT)

void OptimizingCallLinkInfo::setSlowPathCallDestination(CodePtr<JSEntryPtrTag> codePtr)
{
    m_slowPathCallDestination = codePtr;
}

std::tuple<MacroAssembler::JumpList, MacroAssembler::Label> CallLinkInfo::emitFastPathImpl(CallLinkInfo* callLinkInfo, CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC useDataIC, bool isTailCall, ScopedLambda<void()>&& prepareForTailCall)
{
    CCallHelpers::JumpList slowPath;

    if (useDataIC == UseDataIC::Yes) {
        ASSERT(calleeGPR == BaselineJITRegisters::Call::calleeGPR);
        ASSERT(callLinkInfoGPR == BaselineJITRegisters::Call::callLinkInfoGPR);
        // For RISCV64, scratch register usage here collides with MacroAssembler's internal usage
        // that's necessary for the test-and-branch operation but is avoidable by loading from the callee
        // address for each branch operation. Other MacroAssembler implementations handle this better by
        // using a wider range of scratch registers or more potent branching instructions.
        CCallHelpers::Jump found;
        jit.loadPtr(CCallHelpers::Address(callLinkInfoGPR, offsetOfMonomorphicCallDestination()), BaselineJITRegisters::Call::callTargetGPR);
        if constexpr (isRISCV64()) {
            CCallHelpers::Address calleeAddress(callLinkInfoGPR, offsetOfCallee());
            found = jit.branchPtr(CCallHelpers::Equal, calleeAddress, calleeGPR);
            slowPath.append(jit.branchTestPtr(CCallHelpers::Zero, calleeAddress, CCallHelpers::TrustedImm32(polymorphicCalleeMask)));
            jit.loadPtr(calleeAddress, BaselineJITRegisters::Call::globalObjectGPR);
            jit.loadPtr(CCallHelpers::Address(BaselineJITRegisters::Call::globalObjectGPR, CodeBlock::offsetOfGlobalObject() - polymorphicCalleeMask), BaselineJITRegisters::Call::globalObjectGPR);
        } else {
            GPRReg scratchGPR = jit.scratchRegister();
            DisallowMacroScratchRegisterUsage disallowScratch(jit);
            jit.loadPtr(CCallHelpers::Address(callLinkInfoGPR, offsetOfCallee()), scratchGPR);
            found = jit.branchPtr(CCallHelpers::Equal, scratchGPR, calleeGPR);
            slowPath.append(jit.branchTestPtr(CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(polymorphicCalleeMask)));
            jit.loadPtr(CCallHelpers::Address(scratchGPR, CodeBlock::offsetOfGlobalObject() - polymorphicCalleeMask), BaselineJITRegisters::Call::globalObjectGPR);
        }

        auto dispatch = jit.label();
        found.link(&jit);
        if (isTailCall) {
            prepareForTailCall();
            jit.transferPtr(CCallHelpers::Address(callLinkInfoGPR, offsetOfCodeBlock()), CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
            jit.farJump(BaselineJITRegisters::Call::callTargetGPR, JSEntryPtrTag);
        } else {
            jit.transferPtr(CCallHelpers::Address(callLinkInfoGPR, offsetOfCodeBlock()), CCallHelpers::calleeFrameCodeBlockBeforeCall());
            jit.call(BaselineJITRegisters::Call::callTargetGPR, JSEntryPtrTag);
        }
        return std::tuple { slowPath, dispatch };
    }

    CCallHelpers::DataLabelPtr calleeCheck;
    CCallHelpers::Call call;
    CCallHelpers::DataLabelPtr codeBlockStore;
    if (isTailCall) {
        prepareForTailCall();
        slowPath.append(jit.branchPtrWithPatch(CCallHelpers::NotEqual, GPRInfo::regT0, calleeCheck, CCallHelpers::TrustedImmPtr(nullptr)));
        codeBlockStore = jit.storePtrWithPatch(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
        call = jit.nearTailCall();
    } else {
        slowPath.append(jit.branchPtrWithPatch(CCallHelpers::NotEqual, GPRInfo::regT0, calleeCheck, CCallHelpers::TrustedImmPtr(nullptr)));
        codeBlockStore = jit.storePtrWithPatch(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::calleeFrameCodeBlockBeforeCall());
        call = jit.nearCall();
    }

    RELEASE_ASSERT(callLinkInfo);
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        static_cast<OptimizingCallLinkInfo*>(callLinkInfo)->m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
        callLinkInfo->u.codeIC.m_codeBlockLocation = linkBuffer.locationOf<JSInternalPtrTag>(codeBlockStore);
        callLinkInfo->u.codeIC.m_calleeLocation = linkBuffer.locationOf<JSInternalPtrTag>(calleeCheck);
    });
    return std::tuple { slowPath, CCallHelpers::Label() };
}

std::tuple<MacroAssembler::JumpList, MacroAssembler::Label> CallLinkInfo::emitDataICFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR)
{
    RELEASE_ASSERT(callLinkInfoGPR != InvalidGPRReg);
    return emitFastPathImpl(nullptr, jit, calleeGPR, callLinkInfoGPR, UseDataIC::Yes, false, nullptr);
}

std::tuple<MacroAssembler::JumpList, MacroAssembler::Label> CallLinkInfo::emitTailCallDataICFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall)
{
    RELEASE_ASSERT(callLinkInfoGPR != InvalidGPRReg);
    return emitFastPathImpl(nullptr, jit, calleeGPR, callLinkInfoGPR, UseDataIC::Yes, true, WTFMove(prepareForTailCall));
}

void CallLinkInfo::setStub(JSCell* owner, Ref<PolymorphicCallStubRoutine>&& newStub)
{
    clearStub();
    m_stub = WTFMove(newStub);

    m_calleeOrCodeBlock.clear();

    if (isDataIC()) {
        *bitwise_cast<uintptr_t*>(m_calleeOrCodeBlock.slot()) = (bitwise_cast<uintptr_t>(owner) | polymorphicCalleeMask);
        u.dataIC.m_codeBlock = nullptr; // PolymorphicCallStubRoutine will set CodeBlock inside it.
        u.dataIC.m_monomorphicCallDestination = m_stub->code().code().retagged<JSEntryPtrTag>();
    } else {
        MacroAssembler::repatchPointer(u.codeIC.m_codeBlockLocation, nullptr);
        MacroAssembler::replaceWithJump(
            MacroAssembler::startOfBranchPtrWithPatchOnRegister(u.codeIC.m_calleeLocation),
            CodeLocationLabel<JITStubRoutinePtrTag>(m_stub->code().code()));
    }
    m_mode = static_cast<unsigned>(Mode::Polymorphic);
}

void CallLinkInfo::emitSlowPathImpl(VM&, CCallHelpers& jit, GPRReg callLinkInfoGPR, UseDataIC useDataIC, bool isTailCall, MacroAssembler::Label dispatchLabel)
{
    if (useDataIC == UseDataIC::Yes) {
        if (isTailCall) {
            jit.move(CCallHelpers::TrustedImmPtr(LLInt::defaultCall().code().taggedPtr()), BaselineJITRegisters::Call::callTargetGPR);
            jit.jump().linkTo(dispatchLabel, &jit);
            return;
        }
        jit.nearCallThunk(CodeLocationLabel<JSEntryPtrTag> { LLInt::defaultCall().code() }.retagged<NoPtrTag>());
        return;
    }

    if (isTailCall)
        jit.farJump(CCallHelpers::Address(callLinkInfoGPR, OptimizingCallLinkInfo::offsetOfSlowPathCallDestination()), JSEntryPtrTag);
    else
        jit.call(CCallHelpers::Address(callLinkInfoGPR, OptimizingCallLinkInfo::offsetOfSlowPathCallDestination()), JSEntryPtrTag);
}

void CallLinkInfo::emitDataICSlowPath(VM& vm, CCallHelpers& jit, GPRReg callLinkInfoGPR, bool isTailCall, MacroAssembler::Label dispatchLabel)
{
    emitSlowPathImpl(vm, jit, callLinkInfoGPR, UseDataIC::Yes, isTailCall, dispatchLabel);
}

std::tuple<MacroAssembler::JumpList, MacroAssembler::Label> CallLinkInfo::emitFastPath(CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo, GPRReg calleeGPR, GPRReg callLinkInfoGPR)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo))
        return std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitFastPath(jit, calleeGPR, callLinkInfoGPR);

    return CallLinkInfo::emitDataICFastPath(jit, calleeGPR, callLinkInfoGPR);
}

std::tuple<MacroAssembler::JumpList, MacroAssembler::Label> CallLinkInfo::emitTailCallFastPath(CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo))
        return std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitTailCallFastPath(jit, calleeGPR, callLinkInfoGPR, WTFMove(prepareForTailCall));

    return CallLinkInfo::emitTailCallDataICFastPath(jit, calleeGPR, callLinkInfoGPR, WTFMove(prepareForTailCall));
}

void CallLinkInfo::emitSlowPath(VM& vm, CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo, GPRReg callLinkInfoGPR)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo)) {
        std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitSlowPath(vm, jit, callLinkInfoGPR);
        return;
    }
    emitDataICSlowPath(vm, jit, callLinkInfoGPR, /* isTailCall */ false, { });
}

void CallLinkInfo::emitTailCallSlowPath(VM& vm, CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo, GPRReg callLinkInfoGPR, MacroAssembler::Label dispatchLabel)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo)) {
        std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitTailCallSlowPath(vm, jit, callLinkInfoGPR, dispatchLabel);
        return;
    }
    emitDataICSlowPath(vm, jit, callLinkInfoGPR, /* isTailCall */ true, dispatchLabel);
}

std::tuple<CCallHelpers::JumpList, MacroAssembler::Label> OptimizingCallLinkInfo::emitFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR)
{
    RELEASE_ASSERT(!isTailCall());

    if (isDataIC()) {
        RELEASE_ASSERT(callLinkInfoGPR != GPRReg::InvalidGPRReg);
        jit.move(CCallHelpers::TrustedImmPtr(this), callLinkInfoGPR);
        setCallLinkInfoGPR(callLinkInfoGPR);
    }

    return emitFastPathImpl(this, jit, calleeGPR, callLinkInfoGPR, useDataIC(), isTailCall(), nullptr);
}

std::tuple<MacroAssembler::JumpList, MacroAssembler::Label> OptimizingCallLinkInfo::emitTailCallFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall)
{
    RELEASE_ASSERT(isTailCall());

    if (isDataIC()) {
        RELEASE_ASSERT(callLinkInfoGPR != GPRReg::InvalidGPRReg);
        jit.move(CCallHelpers::TrustedImmPtr(this), callLinkInfoGPR);
        setCallLinkInfoGPR(callLinkInfoGPR);
    }

    return emitFastPathImpl(this, jit, calleeGPR, callLinkInfoGPR, useDataIC(), isTailCall(), WTFMove(prepareForTailCall));
}

void OptimizingCallLinkInfo::emitSlowPath(VM& vm, CCallHelpers& jit, GPRReg callLinkInfoGPR)
{
    setSlowPathCallDestination(LLInt::defaultCall().code());
    RELEASE_ASSERT(!isTailCall());
    jit.move(CCallHelpers::TrustedImmPtr(this), callLinkInfoGPR);
    return emitSlowPathImpl(vm, jit, callLinkInfoGPR, useDataIC(), isTailCall(), { });
}

void OptimizingCallLinkInfo::emitTailCallSlowPath(VM& vm, CCallHelpers& jit, GPRReg callLinkInfoGPR, MacroAssembler::Label dispatchLabel)
{
    setSlowPathCallDestination(LLInt::defaultCall().code());
    RELEASE_ASSERT(isTailCall());
    jit.move(CCallHelpers::TrustedImmPtr(this), callLinkInfoGPR);
    return emitSlowPathImpl(vm, jit, callLinkInfoGPR, useDataIC(), isTailCall(), dispatchLabel);
}

CodeLocationLabel<JSInternalPtrTag> OptimizingCallLinkInfo::slowPathStart()
{
    RELEASE_ASSERT(isDirect() && !isDataIC());
    return m_slowPathStart;
}

CodeLocationLabel<JSInternalPtrTag> OptimizingCallLinkInfo::fastPathStart()
{
    RELEASE_ASSERT(isDirect() && isTailCall());
    return CodeLocationDataLabelPtr<JSInternalPtrTag>(m_fastPathStart);
}

void OptimizingCallLinkInfo::emitDirectFastPath(CCallHelpers& jit)
{
    RELEASE_ASSERT(isDirect() && !isTailCall());

    ASSERT(UseDataIC::No == this->useDataIC());

    auto codeBlockStore = jit.storePtrWithPatch(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::calleeFrameCodeBlockBeforeCall());
    auto call = jit.nearCall();
    jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
        u.codeIC.m_codeBlockLocation = linkBuffer.locationOf<JSInternalPtrTag>(codeBlockStore);
    });

    initializeDirectCallRepatch(jit);
}

void OptimizingCallLinkInfo::emitDirectTailCallFastPath(CCallHelpers& jit, ScopedLambda<void()>&& prepareForTailCall)
{
    RELEASE_ASSERT(isDirect() && isTailCall());

    ASSERT(UseDataIC::No == this->useDataIC());

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
        u.codeIC.m_codeBlockLocation = linkBuffer.locationOf<JSInternalPtrTag>(codeBlockStore);
    });

    initializeDirectCallRepatch(jit);
}

void OptimizingCallLinkInfo::initializeDirectCall()
{
    RELEASE_ASSERT(isDirect());
    ASSERT(m_callLocation);
    ASSERT(u.codeIC.m_codeBlockLocation);
    if (isTailCall()) {
        RELEASE_ASSERT(fastPathStart());
        CCallHelpers::replaceWithJump(fastPathStart(), slowPathStart());
    } else
        MacroAssembler::repatchNearCall(m_callLocation, slowPathStart());
}

void OptimizingCallLinkInfo::setDirectCallTarget(CodeBlock* codeBlock, CodeLocationLabel<JSEntryPtrTag> target)
{
    RELEASE_ASSERT(isDirect());

    if (isTailCall()) {
        RELEASE_ASSERT(fastPathStart());
        // We reserved this many bytes for the jump at fastPathStart(). Make that
        // code nops now so we fall through to the jump to the fast path.
        CCallHelpers::replaceWithNops(fastPathStart(), CCallHelpers::patchableJumpSize());
    }

    MacroAssembler::repatchNearCall(m_callLocation, target);
    MacroAssembler::repatchPointer(u.codeIC.m_codeBlockLocation, codeBlock);
    m_mode = static_cast<unsigned>(Mode::LinkedDirect);
}

void OptimizingCallLinkInfo::initializeDirectCallRepatch(CCallHelpers& jit)
{
    auto* executable = this->executable();
    if (executable->isHostFunction()) {
        CodeSpecializationKind kind = specializationKind();
        CodePtr<JSEntryPtrTag> codePtr;
        if (kind == CodeForCall)
            codePtr = executable->generatedJITCodeWithArityCheckForCall();
        else
            codePtr = executable->generatedJITCodeWithArityCheckForConstruct();
        if (codePtr) {
            jit.addLateLinkTask([this, codePtr](LinkBuffer&) {
                setDirectCallTarget(nullptr, CodeLocationLabel { codePtr });
            });
            return;
        }
    }

    jit.addLateLinkTask([this](LinkBuffer&) {
        initializeDirectCall();
    });
}

void OptimizingCallLinkInfo::setDirectCallMaxArgumentCountIncludingThis(unsigned value)
{
    RELEASE_ASSERT(isDirect());
    RELEASE_ASSERT(value);
    m_maxArgumentCountIncludingThisForDirectCall = value;
}

#if ENABLE(DFG_JIT)
void OptimizingCallLinkInfo::initializeFromDFGUnlinkedCallLinkInfo(VM&, const DFG::UnlinkedCallLinkInfo& unlinkedCallLinkInfo, CodeBlock* owner)
{
    m_owner = owner;
    m_doneLocation = unlinkedCallLinkInfo.doneLocation;
    setSlowPathCallDestination(LLInt::defaultCall().code());
    m_codeOrigin = unlinkedCallLinkInfo.codeOrigin;
    m_callType = unlinkedCallLinkInfo.callType;
    m_callLinkInfoGPR = unlinkedCallLinkInfo.callLinkInfoGPR;
}
#endif

#endif

} // namespace JSC
