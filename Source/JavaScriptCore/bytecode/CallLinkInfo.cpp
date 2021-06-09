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
#include "DisallowMacroScratchRegisterUsage.h"
#include "FunctionCodeBlock.h"
#include "JSCellInlines.h"
#include "LinkBuffer.h"
#include "Opcode.h"
#include "Repatch.h"
#include "ThunkGenerators.h"
#include <wtf/ListDump.h>

#if ENABLE(JIT)
namespace JSC {

CallLinkInfo::CallType CallLinkInfo::callTypeFor(OpcodeID opcodeID)
{
    switch (opcodeID) {
    case op_tail_call_varargs:
    case op_tail_call_forward_arguments:
        return TailCallVarargs;        

    case op_call:
    case op_call_eval:
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

CallLinkInfo::CallLinkInfo(CodeOrigin codeOrigin)
    : m_codeOrigin(codeOrigin)
    , m_hasSeenShouldRepatch(false)
    , m_hasSeenClosure(false)
    , m_clearedByGC(false)
    , m_clearedByVirtual(false)
    , m_allowStubs(true)
    , m_clearedByJettison(false)
    , m_callType(None)
    , m_useDataIC(static_cast<unsigned>(UseDataIC::Yes))
{
}

CallLinkInfo::~CallLinkInfo()
{
    clearStub();
    
    if (isOnList())
        remove();
}

void CallLinkInfo::clearStub()
{
    if (!stub())
        return;

    m_stub->clearCallNodesFor(this);
    m_stub = nullptr;
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

CodeLocationLabel<JSInternalPtrTag> CallLinkInfo::fastPathStart()
{
    return CodeLocationDataLabelPtr<JSInternalPtrTag>(m_fastPathStart);
}

CodeLocationLabel<JSInternalPtrTag> CallLinkInfo::slowPathStart()
{
    RELEASE_ASSERT(!isDataIC());
    return u.codeIC.m_slowPathStart;
}

CodeLocationLabel<JSInternalPtrTag> CallLinkInfo::doneLocation()
{
    RELEASE_ASSERT(!isDirect());
    return m_doneLocation;
}

static constexpr uintptr_t polymorphicCalleeMask = 1;

void CallLinkInfo::setMonomorphicCallee(VM& vm, JSCell* owner, JSObject* callee, MacroAssemblerCodePtr<JSEntryPtrTag> codePtr)
{
    RELEASE_ASSERT(!isDirect());
    RELEASE_ASSERT(!(bitwise_cast<uintptr_t>(callee) & polymorphicCalleeMask));
    m_calleeOrCodeBlock.set(vm, owner, callee);

    if (isDataIC()) 
        u.dataIC.m_monomorphicCallDestination = codePtr;
    else {
        MacroAssembler::repatchNearCall(u.codeIC.m_callLocation, CodeLocationLabel<JSEntryPtrTag>(codePtr));
        MacroAssembler::repatchPointer(u.codeIC.m_calleeLocation, callee);
    }
}

void CallLinkInfo::clearCallee()
{
    RELEASE_ASSERT(!isDirect());
    m_calleeOrCodeBlock.clear();
    if (isDataIC())
        u.dataIC.m_monomorphicCallDestination = nullptr;
    else if (!clearedByJettison())
        MacroAssembler::repatchPointer(u.codeIC.m_calleeLocation, nullptr);
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

void CallLinkInfo::setFrameShuffleData(const CallFrameShuffleData& shuffleData)
{
    m_frameShuffleData = makeUnique<CallFrameShuffleData>(shuffleData);
    m_frameShuffleData->shrinkToFit();
}

MacroAssembler::JumpList CallLinkInfo::emitFastPathImpl(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC useDataIC, WTF::Function<void()> prepareForTailCall)
{
    setUsesDataICs(useDataIC);
    if (isDataIC()) {
        RELEASE_ASSERT(callLinkInfoGPR != GPRReg::InvalidGPRReg);
        jit.move(CCallHelpers::TrustedImmPtr(this), callLinkInfoGPR);
        u.dataIC.m_callLinkInfoGPR = callLinkInfoGPR;
    }

    auto fastPathStart = jit.label();
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        m_fastPathStart = linkBuffer.locationOf<JSInternalPtrTag>(fastPathStart);
    });

    CCallHelpers::JumpList slowPath;

    if (isDataIC()) {
        GPRReg scratchGPR = jit.scratchRegister();
        jit.loadPtr(CCallHelpers::Address(callLinkInfoGPR, offsetOfCallee()), scratchGPR); 
        CCallHelpers::Jump goPolymorphic;
        {
            DisallowMacroScratchRegisterUsage disallowScratch(jit);
            goPolymorphic = jit.branchTestPtr(CCallHelpers::NonZero, scratchGPR, CCallHelpers::TrustedImm32(polymorphicCalleeMask));
            slowPath.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, calleeGPR));
        }
        if (isTailCall()) {
            prepareForTailCall();
            goPolymorphic.link(&jit); // Polymorphic stub handles tail call stack prep.
            jit.farJump(CCallHelpers::Address(callLinkInfoGPR, offsetOfMonomorphicCallDestination()), JSEntryPtrTag);
        } else {
            goPolymorphic.link(&jit);
            jit.call(CCallHelpers::Address(callLinkInfoGPR, offsetOfMonomorphicCallDestination()), JSEntryPtrTag);
        }
    } else {
        CCallHelpers::DataLabelPtr calleeCheck;
        slowPath.append(jit.branchPtrWithPatch(CCallHelpers::NotEqual, calleeGPR, calleeCheck, CCallHelpers::TrustedImmPtr(nullptr)));

        CCallHelpers::Call call;
        if (isTailCall()) {
            prepareForTailCall();
            call = jit.nearTailCall();
        } else
            call = jit.nearCall();
        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            u.codeIC.m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
            u.codeIC.m_calleeLocation = linkBuffer.locationOf<JSInternalPtrTag>(calleeCheck);
        });
    }

    return slowPath;
}

CCallHelpers::JumpList CallLinkInfo::emitFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC useDataIC)
{
    RELEASE_ASSERT(!isTailCall());
    return emitFastPathImpl(jit, calleeGPR, callLinkInfoGPR, useDataIC, nullptr);
}

MacroAssembler::JumpList CallLinkInfo::emitTailCallFastPath(CCallHelpers& jit, GPRReg calleeGPR, GPRReg callLinkInfoGPR, UseDataIC useDataIC, WTF::Function<void()> prepareForTailCall)
{
    RELEASE_ASSERT(isTailCall());
    return emitFastPathImpl(jit, calleeGPR, callLinkInfoGPR, useDataIC, WTFMove(prepareForTailCall));
}

void CallLinkInfo::emitSlowPath(VM& vm, CCallHelpers& jit)
{
    setSlowPathCallDestination(vm.getCTIStub(linkCallThunkGenerator).template retaggedCode<JSEntryPtrTag>());
    jit.move(CCallHelpers::TrustedImmPtr(this), GPRInfo::regT2);
    jit.call(CCallHelpers::Address(GPRInfo::regT2, offsetOfSlowPathCallDestination()), JSEntryPtrTag);
}

void CallLinkInfo::emitDirectFastPath(CCallHelpers& jit)
{
    RELEASE_ASSERT(!isTailCall());

    setUsesDataICs(UseDataIC::No);

    auto fastPathStart = jit.label();
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        m_fastPathStart = linkBuffer.locationOf<JSInternalPtrTag>(fastPathStart);
    });

    auto call = jit.nearCall();
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        u.codeIC.m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
    });
    jit.addLateLinkTask([this] (LinkBuffer&) {
        initializeDirectCall();
    });
}

void CallLinkInfo::emitDirectTailCallFastPath(CCallHelpers& jit, WTF::Function<void()> prepareForTailCall)
{
    RELEASE_ASSERT(isTailCall());

    setUsesDataICs(UseDataIC::No);

    auto fastPathStart = jit.label();
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        m_fastPathStart = linkBuffer.locationOf<JSInternalPtrTag>(fastPathStart);
    });

    // - If we're not yet linked, this is a jump to the slow path.
    // - Once we're linked to a fast path, this goes back to being nops so we fall through to the linked jump.
    jit.emitNops(CCallHelpers::patchableJumpSize());

    prepareForTailCall();
    auto call = jit.nearTailCall();
    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        u.codeIC.m_callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
    });
    jit.addLateLinkTask([this] (LinkBuffer&) {
        initializeDirectCall();
    });
}

void CallLinkInfo::initializeDirectCall()
{
    RELEASE_ASSERT(isDirect());
    ASSERT(u.codeIC.m_callLocation);
    if (isTailCall()) {
        RELEASE_ASSERT(fastPathStart());
        CCallHelpers::emitJITCodeOver(fastPathStart(), scopedLambda<void(CCallHelpers&)>([&](CCallHelpers& jit) {
            auto jump = jit.jump();
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(jump, slowPathStart());
            });
        }), "initialize direct call");
    } else
        MacroAssembler::repatchNearCall(u.codeIC.m_callLocation, slowPathStart());
}

void CallLinkInfo::setDirectCallTarget(CodeLocationLabel<JSEntryPtrTag> target)
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

    MacroAssembler::repatchNearCall(u.codeIC.m_callLocation, target);
}

void CallLinkInfo::setSlowPathCallDestination(MacroAssemblerCodePtr<JSEntryPtrTag> codePtr)
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
        u.dataIC.m_monomorphicCallDestination = nullptr;
    } else {
        CCallHelpers::revertJumpReplacementToBranchPtrWithPatch(
            CCallHelpers::startOfBranchPtrWithPatchOnRegister(u.codeIC.m_calleeLocation), calleeGPR(), nullptr);
    }
}

void CallLinkInfo::setStub(Ref<PolymorphicCallStubRoutine>&& newStub)
{
    clearStub();
    m_stub = WTFMove(newStub);

    m_calleeOrCodeBlock.clear();

    if (isDataIC()) {
        *bitwise_cast<uintptr_t*>(m_calleeOrCodeBlock.slot()) = polymorphicCalleeMask;
        u.dataIC.m_monomorphicCallDestination = m_stub->code().code().retagged<JSEntryPtrTag>();
    } else {
        MacroAssembler::replaceWithJump(
            MacroAssembler::startOfBranchPtrWithPatchOnRegister(u.codeIC.m_calleeLocation),
            CodeLocationLabel<JITStubRoutinePtrTag>(m_stub->code().code()));
    }
}

} // namespace JSC
#endif // ENABLE(JIT)

