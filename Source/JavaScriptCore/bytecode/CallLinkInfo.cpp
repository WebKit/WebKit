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
#include "JSCellInlines.h"
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
    
    if (isOnList())
        remove();
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

void CallLinkInfo::unlink(VM& vm)
{
    // We could be called even if we're not linked anymore because of how polymorphic calls
    // work. Each callsite within the polymorphic call stub may separately ask us to unlink().
    if (isLinked())
        unlinkCall(vm, *this);

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

void CallLinkInfo::setMaxArgumentCountIncludingThis(unsigned value)
{
    RELEASE_ASSERT(isDirect());
    RELEASE_ASSERT(value);
    m_maxArgumentCountIncludingThis = value;
}

void CallLinkInfo::visitWeak(VM& vm)
{
    auto handleSpecificCallee = [&] (JSFunction* callee) {
        if (vm.heap.isMarked(callee->executable()))
            m_hasSeenClosure = true;
        else
            m_clearedByGC = true;
    };
    
    if (isLinked()) {
        if (stub()) {
#if ENABLE(JIT)
            if (!stub()->visitWeak(vm)) {
                if (UNLIKELY(Options::verboseOSR())) {
                    dataLog(
                        "At ", m_codeOrigin, ", ", RawPointer(this), ": clearing call stub to ",
                        listDump(stub()->variants()), ", stub routine ", RawPointer(stub()),
                        ".\n");
                }
                unlink(vm);
                m_clearedByGC = true;
            }
#else
            RELEASE_ASSERT_NOT_REACHED();
#endif
        } else if (!vm.heap.isMarked(m_calleeOrCodeBlock.get())) {
            if (isDirect()) {
                if (UNLIKELY(Options::verboseOSR())) {
                    dataLog(
                        "Clearing call to ", RawPointer(codeBlock()), " (",
                        pointerDump(codeBlock()), ").\n");
                }
            } else {
                JSObject* callee = jsCast<JSObject*>(m_calleeOrCodeBlock.get());
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
            unlink(vm);
        } else if (isDirect() && !vm.heap.isMarked(m_lastSeenCalleeOrExecutable.get())) {
            if (UNLIKELY(Options::verboseOSR())) {
                dataLog(
                    "Clearing call to ", RawPointer(executable()),
                    " because the executable is dead.\n");
            }
            unlink(vm);
            // We should only get here once the owning CodeBlock is dying, since the executable must
            // already be in the owner's weak references.
            m_lastSeenCalleeOrExecutable.clear();
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

void CallLinkInfo::setSlowPathCallDestination(CodePtr<JSEntryPtrTag> codePtr)
{
    m_slowPathCallDestination = codePtr;
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
        CCallHelpers::revertJumpReplacementToBranchPtrWithPatch(
            CCallHelpers::startOfBranchPtrWithPatchOnRegister(u.codeIC.m_calleeLocation), calleeGPR(), nullptr);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }
}

void BaselineCallLinkInfo::initialize(VM& vm, CallType callType, BytecodeIndex bytecodeIndex)
{
    m_type = static_cast<unsigned>(Type::Baseline);
    ASSERT(Type::Baseline == type());
    m_useDataIC = static_cast<unsigned>(UseDataIC::Yes);
    ASSERT(UseDataIC::Yes == useDataIC());
    m_codeOrigin = CodeOrigin(bytecodeIndex);
    m_callType = callType;
    if (LIKELY(Options::useLLIntICs()))
        setSlowPathCallDestination(vm.getCTILinkCall().code());
    else
        setSlowPathCallDestination(vm.getCTIVirtualCall(callMode()).retagged<JSEntryPtrTag>().code());
    // If JIT is disabled, we should not support dynamically generated call IC.
    if (!Options::useJIT())
        disallowStubs();
}

#if ENABLE(JIT)

void OptimizingCallLinkInfo::setFrameShuffleData(const CallFrameShuffleData& shuffleData)
{
    m_frameShuffleData = makeUnique<CallFrameShuffleData>(shuffleData);
    m_frameShuffleData->shrinkToFit();
}

MacroAssembler::JumpList CallLinkInfo::emitFastPathImpl(CallLinkInfo* callLinkInfo, CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC useDataIC, bool isTailCall, ScopedLambda<void()>&& prepareForTailCall)
{
    CCallHelpers::JumpList slowPath;

    if (useDataIC == UseDataIC::Yes) {
        CCallHelpers::Jump goPolymorphic;

        // For RISCV64, scratch register usage here collides with MacroAssembler's internal usage
        // that's necessary for the test-and-branch operation but is avoidable by loading from the callee
        // address for each branch operation. Other MacroAssembler implementations handle this better by
        // using a wider range of scratch registers or more potent branching instructions.
        if constexpr (isRISCV64()) {
            CCallHelpers::Address calleeAddress(callLinkInfoGPR, offsetOfCallee());
            goPolymorphic = jit.branchTestPtr(CCallHelpers::NonZero, calleeAddress, CCallHelpers::TrustedImm32(polymorphicCalleeMask));
            slowPath.append(jit.branchPtr(CCallHelpers::NotEqual, calleeAddress, calleeGPR));
        } else {
            GPRReg scratchGPR = jit.scratchRegister();
            DisallowMacroScratchRegisterUsage disallowScratch(jit);
            jit.loadPtr(CCallHelpers::Address(callLinkInfoGPR, offsetOfCallee()), scratchGPR);
            goPolymorphic = jit.branchTestPtr(CCallHelpers::NonZero, scratchGPR, CCallHelpers::TrustedImm32(polymorphicCalleeMask));
            slowPath.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, calleeGPR));
        }

        if (isTailCall) {
            prepareForTailCall();

            GPRReg scratchGPR = CCallHelpers::selectScratchGPR(calleeGPR, callLinkInfoGPR);
            jit.loadPtr(CCallHelpers::Address(callLinkInfoGPR, offsetOfCodeBlock()), scratchGPR);
            jit.storePtr(scratchGPR, CCallHelpers::calleeFrameCodeBlockBeforeTailCall());

            goPolymorphic.link(&jit); // Polymorphic stub handles tail call stack prep.
            jit.farJump(CCallHelpers::Address(callLinkInfoGPR, offsetOfMonomorphicCallDestination()), JSEntryPtrTag);
        } else {
            GPRReg scratchGPR = CCallHelpers::selectScratchGPR(calleeGPR, callLinkInfoGPR);
            jit.loadPtr(CCallHelpers::Address(callLinkInfoGPR, offsetOfCodeBlock()), scratchGPR);
            jit.storePtr(scratchGPR, CCallHelpers::calleeFrameCodeBlockBeforeCall());

            goPolymorphic.link(&jit);
            jit.call(CCallHelpers::Address(callLinkInfoGPR, offsetOfMonomorphicCallDestination()), JSEntryPtrTag);
        }
    } else {
        CCallHelpers::DataLabelPtr calleeCheck;
        slowPath.append(jit.branchPtrWithPatch(CCallHelpers::NotEqual, calleeGPR, calleeCheck, CCallHelpers::TrustedImmPtr(nullptr)));

        CCallHelpers::Call call;
        CCallHelpers::DataLabelPtr codeBlockStore;
        if (isTailCall) {
            prepareForTailCall();
            codeBlockStore = jit.storePtrWithPatch(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
            call = jit.nearTailCall();
        } else {
            codeBlockStore = jit.storePtrWithPatch(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::calleeFrameCodeBlockBeforeCall());
            call = jit.nearCall();
        }

        RELEASE_ASSERT(callLinkInfo);
        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            static_cast<OptimizingCallLinkInfo*>(callLinkInfo)->m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
            callLinkInfo->u.codeIC.m_codeBlockLocation = linkBuffer.locationOf<JSInternalPtrTag>(codeBlockStore);
            callLinkInfo->u.codeIC.m_calleeLocation = linkBuffer.locationOf<JSInternalPtrTag>(calleeCheck);
        });
    }

    return slowPath;
}

MacroAssembler::JumpList CallLinkInfo::emitDataICFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR)
{
    RELEASE_ASSERT(callLinkInfoGPR != InvalidGPRReg);
    return emitFastPathImpl(nullptr, jit, calleeGPR, callLinkInfoGPR, UseDataIC::Yes, false, nullptr);
}

MacroAssembler::JumpList CallLinkInfo::emitTailCallDataICFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall)
{
    RELEASE_ASSERT(callLinkInfoGPR != InvalidGPRReg);
    return emitFastPathImpl(nullptr, jit, calleeGPR, callLinkInfoGPR, UseDataIC::Yes, true, WTFMove(prepareForTailCall));
}

void CallLinkInfo::setStub(Ref<PolymorphicCallStubRoutine>&& newStub)
{
    clearStub();
    m_stub = WTFMove(newStub);

    m_calleeOrCodeBlock.clear();

    if (isDataIC()) {
        *bitwise_cast<uintptr_t*>(m_calleeOrCodeBlock.slot()) = polymorphicCalleeMask;
        u.dataIC.m_codeBlock = nullptr; // PolymorphicCallStubRoutine will set CodeBlock inside it.
        u.dataIC.m_monomorphicCallDestination = m_stub->code().code().retagged<JSEntryPtrTag>();
    } else {
        MacroAssembler::repatchPointer(u.codeIC.m_codeBlockLocation, nullptr);
        MacroAssembler::replaceWithJump(
            MacroAssembler::startOfBranchPtrWithPatchOnRegister(u.codeIC.m_calleeLocation),
            CodeLocationLabel<JITStubRoutinePtrTag>(m_stub->code().code()));
    }
}

void CallLinkInfo::emitDataICSlowPath(VM&, CCallHelpers& jit, GPRReg callLinkInfoGPR)
{
    jit.move(callLinkInfoGPR, GPRInfo::regT2);
    jit.call(CCallHelpers::Address(GPRInfo::regT2, offsetOfSlowPathCallDestination()), JSEntryPtrTag);
}

MacroAssembler::JumpList CallLinkInfo::emitFastPath(CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo, GPRReg calleeGPR, GPRReg callLinkInfoGPR)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo))
        return std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitFastPath(jit, calleeGPR, callLinkInfoGPR);

    return CallLinkInfo::emitDataICFastPath(jit, calleeGPR, callLinkInfoGPR);
}

MacroAssembler::JumpList CallLinkInfo::emitTailCallFastPath(CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo))
        return std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitTailCallFastPath(jit, calleeGPR, callLinkInfoGPR, WTFMove(prepareForTailCall));

    return CallLinkInfo::emitTailCallDataICFastPath(jit, calleeGPR, callLinkInfoGPR, WTFMove(prepareForTailCall));
}

void CallLinkInfo::emitSlowPath(VM& vm, CCallHelpers& jit, CompileTimeCallLinkInfo callLinkInfo, GPRReg callLinkInfoGPR)
{
    if (std::holds_alternative<OptimizingCallLinkInfo*>(callLinkInfo)) {
        std::get<OptimizingCallLinkInfo*>(callLinkInfo)->emitSlowPath(vm, jit);
        return;
    }
    emitDataICSlowPath(vm, jit, callLinkInfoGPR);
}

CCallHelpers::JumpList OptimizingCallLinkInfo::emitFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR)
{
    RELEASE_ASSERT(!isTailCall());

    if (isDataIC()) {
        RELEASE_ASSERT(callLinkInfoGPR != GPRReg::InvalidGPRReg);
        jit.move(CCallHelpers::TrustedImmPtr(this), callLinkInfoGPR);
        setCallLinkInfoGPR(callLinkInfoGPR);
    }

    return emitFastPathImpl(this, jit, calleeGPR, callLinkInfoGPR, useDataIC(), isTailCall(), nullptr);
}

MacroAssembler::JumpList OptimizingCallLinkInfo::emitTailCallFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, ScopedLambda<void()>&& prepareForTailCall)
{
    RELEASE_ASSERT(isTailCall());

    if (isDataIC()) {
        RELEASE_ASSERT(callLinkInfoGPR != GPRReg::InvalidGPRReg);
        jit.move(CCallHelpers::TrustedImmPtr(this), callLinkInfoGPR);
        setCallLinkInfoGPR(callLinkInfoGPR);
    }

    return emitFastPathImpl(this, jit, calleeGPR, callLinkInfoGPR, useDataIC(), isTailCall(), WTFMove(prepareForTailCall));
}

void OptimizingCallLinkInfo::emitSlowPath(VM& vm, CCallHelpers& jit)
{
    setSlowPathCallDestination(vm.getCTILinkCall().code());
    jit.move(CCallHelpers::TrustedImmPtr(this), GPRInfo::regT2);
    jit.call(CCallHelpers::Address(GPRInfo::regT2, offsetOfSlowPathCallDestination()), JSEntryPtrTag);
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
    jit.addLateLinkTask([this] (LinkBuffer&) {
        initializeDirectCall();
    });
}

void OptimizingCallLinkInfo::emitDirectTailCallFastPath(CCallHelpers& jit, ScopedLambda<void()>&& prepareForTailCall)
{
    RELEASE_ASSERT(isDirect() && isTailCall());

    ASSERT(UseDataIC::No == this->useDataIC());

    auto fastPathStart = jit.label();
    jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        m_fastPathStart = linkBuffer.locationOf<JSInternalPtrTag>(fastPathStart);
    });

    // - If we're not yet linked, this is a jump to the slow path.
    // - Once we're linked to a fast path, this goes back to being nops so we fall through to the linked jump.
    jit.emitNops(CCallHelpers::patchableJumpSize());

    prepareForTailCall();
    auto codeBlockStore = jit.storePtrWithPatch(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
    auto call = jit.nearTailCall();
    jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
        u.codeIC.m_codeBlockLocation = linkBuffer.locationOf<JSInternalPtrTag>(codeBlockStore);
    });
    jit.addLateLinkTask([this] (LinkBuffer&) {
        initializeDirectCall();
    });
}

void OptimizingCallLinkInfo::initializeDirectCall()
{
    RELEASE_ASSERT(isDirect());
    ASSERT(m_callLocation);
    ASSERT(u.codeIC.m_codeBlockLocation);
    if (isTailCall()) {
        RELEASE_ASSERT(fastPathStart());
        CCallHelpers::emitJITCodeOver(fastPathStart(), scopedLambda<void(CCallHelpers&)>([&](CCallHelpers& jit) {
            auto jump = jit.jump();
            jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
                linkBuffer.link(jump, slowPathStart());
            });
        }), "initialize direct call");
    } else
        MacroAssembler::repatchNearCall(m_callLocation, slowPathStart());
}

void OptimizingCallLinkInfo::setDirectCallTarget(CodeBlock* codeBlock, CodeLocationLabel<JSEntryPtrTag> target)
{
    RELEASE_ASSERT(isDirect());

    if (isTailCall()) {
        RELEASE_ASSERT(fastPathStart());
        CCallHelpers::emitJITCodeOver(fastPathStart(), scopedLambda<void(CCallHelpers&)>([&](CCallHelpers& jit) {
            // We reserved this many bytes for the jump at fastPathStart(). Make that
            // code nops now so we fall through to the jump to the fast path.
            jit.emitNops(CCallHelpers::patchableJumpSize());
        }), "Setting direct call target");
    }

    MacroAssembler::repatchNearCall(m_callLocation, target);
    MacroAssembler::repatchPointer(u.codeIC.m_codeBlockLocation, codeBlock);
}

#if ENABLE(DFG_JIT)
void OptimizingCallLinkInfo::initializeFromDFGUnlinkedCallLinkInfo(VM& vm, const DFG::UnlinkedCallLinkInfo& unlinkedCallLinkInfo)
{
    m_doneLocation = unlinkedCallLinkInfo.doneLocation;
    setSlowPathCallDestination(vm.getCTILinkCall().code());
    m_codeOrigin = unlinkedCallLinkInfo.codeOrigin;
    m_callType = unlinkedCallLinkInfo.callType;
    m_calleeGPR = unlinkedCallLinkInfo.calleeGPR;
    m_callLinkInfoGPR = unlinkedCallLinkInfo.callLinkInfoGPR;
    if (unlinkedCallLinkInfo.m_frameShuffleData)
        m_frameShuffleData = makeUnique<CallFrameShuffleData>(*unlinkedCallLinkInfo.m_frameShuffleData);
}
#endif

#endif

} // namespace JSC
