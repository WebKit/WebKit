/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
#include "PolymorphicAccess.h"

#if ENABLE(JIT)

#include "BinarySwitch.h"
#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "FullCodeOrigin.h"
#include "Heap.h"
#include "JITOperations.h"
#include "LinkBuffer.h"
#include "StructureInlines.h"
#include "StructureStubClearingWatchpoint.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>

namespace JSC {

namespace PolymorphicAccessInternal {
static constexpr bool verbose = false;
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PolymorphicAccess);

void AccessGenerationResult::dump(PrintStream& out) const
{
    out.print(m_kind);
    if (m_code)
        out.print(":", m_code);
}

void AccessGenerationState::installWatchpoint(CodeBlock* codeBlock, const ObjectPropertyCondition& condition)
{
    WatchpointsOnStructureStubInfo::ensureReferenceAndInstallWatchpoint(watchpoints, codeBlock, stubInfo, condition);
}

void AccessGenerationState::restoreScratch()
{
    allocator->restoreReusedRegistersByPopping(*jit, preservedReusedRegisterState);
}

void AccessGenerationState::succeed()
{
    restoreScratch();
    if (jit->codeBlock()->useDataIC())
        jit->ret();
    else
        success.append(jit->jump());
}

const RegisterSet& AccessGenerationState::liveRegistersForCall()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();
    return m_liveRegistersForCall;
}

const RegisterSet& AccessGenerationState::liveRegistersToPreserveAtExceptionHandlingCallSite()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();
    return m_liveRegistersToPreserveAtExceptionHandlingCallSite;
}

static RegisterSet calleeSaveRegisters()
{
    RegisterSet result = RegisterSet::registersToNotSaveForJSCall();
    result.filter(RegisterSet::registersToNotSaveForCCall());
    return result;
}

const RegisterSet& AccessGenerationState::calculateLiveRegistersForCallAndExceptionHandling()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling) {
        m_calculatedRegistersForCallAndExceptionHandling = true;

        m_liveRegistersToPreserveAtExceptionHandlingCallSite = jit->codeBlock()->jitCode()->liveRegistersToPreserveAtExceptionHandlingCallSite(jit->codeBlock(), stubInfo->callSiteIndex);
        m_needsToRestoreRegistersIfException = m_liveRegistersToPreserveAtExceptionHandlingCallSite.numberOfSetRegisters() > 0;
        if (m_needsToRestoreRegistersIfException)
            RELEASE_ASSERT(JITCode::isOptimizingJIT(jit->codeBlock()->jitType()));

        m_liveRegistersForCall = RegisterSet(m_liveRegistersToPreserveAtExceptionHandlingCallSite, allocator->usedRegisters());
        m_liveRegistersForCall.exclude(calleeSaveRegisters());
    }
    return m_liveRegistersForCall;
}

auto AccessGenerationState::preserveLiveRegistersToStackForCall(const RegisterSet& extra) -> SpillState
{
    RegisterSet liveRegisters = liveRegistersForCall();
    liveRegisters.merge(extra);

    unsigned extraStackPadding = 0;
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(*jit, liveRegisters, extraStackPadding);
    return SpillState {
        WTFMove(liveRegisters),
        numberOfStackBytesUsedForRegisterPreservation
    };
}

auto AccessGenerationState::preserveLiveRegistersToStackForCallWithoutExceptions(const RegisterSet& extra) -> SpillState
{
    RegisterSet liveRegisters = allocator->usedRegisters();
    liveRegisters.exclude(calleeSaveRegisters());
    liveRegisters.merge(extra);

    constexpr unsigned extraStackPadding = 0;
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(*jit, liveRegisters, extraStackPadding);
    return SpillState {
        WTFMove(liveRegisters),
        numberOfStackBytesUsedForRegisterPreservation
    };
}

void AccessGenerationState::restoreLiveRegistersFromStackForCallWithThrownException(const SpillState& spillState)
{
    // Even if we're a getter, we don't want to ignore the result value like we normally do
    // because the getter threw, and therefore, didn't return a value that means anything.
    // Instead, we want to restore that register to what it was upon entering the getter
    // inline cache. The subtlety here is if the base and the result are the same register,
    // and the getter threw, we want OSR exit to see the original base value, not the result
    // of the getter call.
    RegisterSet dontRestore = spillState.spilledRegisters;
    // As an optimization here, we only need to restore what is live for exception handling.
    // We can construct the dontRestore set to accomplish this goal by having it contain only
    // what is live for call but not live for exception handling. By ignoring things that are
    // only live at the call but not the exception handler, we will only restore things live
    // at the exception handler.
    dontRestore.exclude(liveRegistersToPreserveAtExceptionHandlingCallSite());
    restoreLiveRegistersFromStackForCall(spillState, dontRestore);
}

void AccessGenerationState::restoreLiveRegistersFromStackForCall(const SpillState& spillState, const RegisterSet& dontRestore)
{
    unsigned extraStackPadding = 0;
    ScratchRegisterAllocator::restoreRegistersFromStackForCall(*jit, spillState.spilledRegisters, dontRestore, spillState.numberOfStackBytesUsedForRegisterPreservation, extraStackPadding);
}

CallSiteIndex AccessGenerationState::callSiteIndexForExceptionHandlingOrOriginal()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();

    if (!m_calculatedCallSiteIndex) {
        m_calculatedCallSiteIndex = true;

        if (m_needsToRestoreRegistersIfException)
            m_callSiteIndex = jit->codeBlock()->newExceptionHandlingCallSiteIndex(stubInfo->callSiteIndex);
        else
            m_callSiteIndex = originalCallSiteIndex();
    }

    return m_callSiteIndex;
}

DisposableCallSiteIndex AccessGenerationState::callSiteIndexForExceptionHandling()
{
    RELEASE_ASSERT(m_calculatedRegistersForCallAndExceptionHandling);
    RELEASE_ASSERT(m_needsToRestoreRegistersIfException);
    RELEASE_ASSERT(m_calculatedCallSiteIndex);
    return DisposableCallSiteIndex::fromCallSiteIndex(m_callSiteIndex);
}

const HandlerInfo& AccessGenerationState::originalExceptionHandler()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();

    RELEASE_ASSERT(m_needsToRestoreRegistersIfException);
    HandlerInfo* exceptionHandler = jit->codeBlock()->handlerForIndex(stubInfo->callSiteIndex.bits());
    RELEASE_ASSERT(exceptionHandler);
    return *exceptionHandler;
}

CallSiteIndex AccessGenerationState::originalCallSiteIndex() const { return stubInfo->callSiteIndex; }

void AccessGenerationState::emitExplicitExceptionHandler()
{
    restoreScratch();
    jit->pushToSave(GPRInfo::regT0);
    jit->loadPtr(&m_vm.topEntryFrame, GPRInfo::regT0);
    jit->copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRInfo::regT0);
    jit->popToRestore(GPRInfo::regT0);

    if (needsToRestoreRegistersIfException()) {
        // To the JIT that produces the original exception handling
        // call site, they will expect the OSR exit to be arrived
        // at from genericUnwind. Therefore we must model what genericUnwind
        // does here. I.e, set callFrameForCatch and copy callee saves.

        jit->storePtr(GPRInfo::callFrameRegister, m_vm.addressOfCallFrameForCatch());
        CCallHelpers::Jump jumpToOSRExitExceptionHandler = jit->jump();

        // We don't need to insert a new exception handler in the table
        // because we're doing a manual exception check here. i.e, we'll
        // never arrive here from genericUnwind().
        HandlerInfo originalHandler = originalExceptionHandler();
        jit->addLinkTask(
            [=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(jumpToOSRExitExceptionHandler, originalHandler.nativeCode);
            });
    } else {
#if ENABLE(EXTRA_CTI_THUNKS)
        CCallHelpers::Jump jumpToExceptionHandler = jit->jump();
        VM* vm = &m_vm;
        jit->addLinkTask(
            [=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(jumpToExceptionHandler, CodeLocationLabel(vm->getCTIStub(handleExceptionGenerator).retaggedCode<NoPtrTag>()));
            });
#else
        jit->setupArguments<decltype(operationLookupExceptionHandler)>(CCallHelpers::TrustedImmPtr(&m_vm));
        jit->prepareCallOperation(m_vm);
        CCallHelpers::Call lookupExceptionHandlerCall = jit->call(OperationPtrTag);
        jit->addLinkTask(
            [=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(lookupExceptionHandlerCall, FunctionPtr<OperationPtrTag>(operationLookupExceptionHandler));
            });
        jit->jumpToExceptionHandler(m_vm);
#endif
    }
}

PolymorphicAccess::PolymorphicAccess() { }
PolymorphicAccess::~PolymorphicAccess() { }

AccessGenerationResult PolymorphicAccess::addCases(
    const GCSafeConcurrentJSLocker& locker, VM& vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo,
    Vector<RefPtr<AccessCase>, 2> originalCasesToAdd)
{
    SuperSamplerScope superSamplerScope(false);
    
    // This method will add the originalCasesToAdd to the list one at a time while preserving the
    // invariants:
    // - If a newly added case canReplace() any existing case, then the existing case is removed before
    //   the new case is added. Removal doesn't change order of the list. Any number of existing cases
    //   can be removed via the canReplace() rule.
    // - Cases in the list always appear in ascending order of time of addition. Therefore, if you
    //   cascade through the cases in reverse order, you will get the most recent cases first.
    // - If this method fails (returns null, doesn't add the cases), then both the previous case list
    //   and the previous stub are kept intact and the new cases are destroyed. It's OK to attempt to
    //   add more things after failure.
    
    // First ensure that the originalCasesToAdd doesn't contain duplicates.
    Vector<RefPtr<AccessCase>> casesToAdd;
    for (unsigned i = 0; i < originalCasesToAdd.size(); ++i) {
        RefPtr<AccessCase> myCase = WTFMove(originalCasesToAdd[i]);

        // Add it only if it is not replaced by the subsequent cases in the list.
        bool found = false;
        for (unsigned j = i + 1; j < originalCasesToAdd.size(); ++j) {
            if (originalCasesToAdd[j]->canReplace(*myCase)) {
                found = true;
                break;
            }
        }

        if (found)
            continue;
        
        casesToAdd.append(WTFMove(myCase));
    }

    dataLogLnIf(PolymorphicAccessInternal::verbose, "casesToAdd: ", listDump(casesToAdd));

    // If there aren't any cases to add, then fail on the grounds that there's no point to generating a
    // new stub that will be identical to the old one. Returning null should tell the caller to just
    // keep doing what they were doing before.
    if (casesToAdd.isEmpty())
        return AccessGenerationResult::MadeNoChanges;

    if (stubInfo.accessType != AccessType::InstanceOf) {
        bool shouldReset = false;
        AccessGenerationResult resetResult(AccessGenerationResult::ResetStubAndFireWatchpoints);
        auto considerPolyProtoReset = [&] (Structure* a, Structure* b) {
            if (Structure::shouldConvertToPolyProto(a, b)) {
                // For now, we only reset if this is our first time invalidating this watchpoint.
                // The reason we don't immediately fire this watchpoint is that we may be already
                // watching the poly proto watchpoint, which if fired, would destroy us. We let
                // the person handling the result to do a delayed fire.
                ASSERT(a->rareData()->sharedPolyProtoWatchpoint().get() == b->rareData()->sharedPolyProtoWatchpoint().get());
                if (a->rareData()->sharedPolyProtoWatchpoint()->isStillValid()) {
                    shouldReset = true;
                    resetResult.addWatchpointToFire(*a->rareData()->sharedPolyProtoWatchpoint(), StringFireDetail("Detected poly proto optimization opportunity."));
                }
            }
        };

        for (auto& caseToAdd : casesToAdd) {
            for (auto& existingCase : m_list) {
                Structure* a = caseToAdd->structure();
                Structure* b = existingCase->structure();
                considerPolyProtoReset(a, b);
            }
        }
        for (unsigned i = 0; i < casesToAdd.size(); ++i) {
            for (unsigned j = i + 1; j < casesToAdd.size(); ++j) {
                Structure* a = casesToAdd[i]->structure();
                Structure* b = casesToAdd[j]->structure();
                considerPolyProtoReset(a, b);
            }
        }

        if (shouldReset)
            return resetResult;
    }

    // Now add things to the new list. Note that at this point, we will still have old cases that
    // may be replaced by the new ones. That's fine. We will sort that out when we regenerate.
    for (auto& caseToAdd : casesToAdd) {
        commit(locker, vm, m_watchpoints, codeBlock, stubInfo, *caseToAdd);
        m_list.append(WTFMove(caseToAdd));
    }
    
    dataLogLnIf(PolymorphicAccessInternal::verbose, "After addCases: m_list: ", listDump(m_list));

    return AccessGenerationResult::Buffered;
}

AccessGenerationResult PolymorphicAccess::addCase(
    const GCSafeConcurrentJSLocker& locker, VM& vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo, Ref<AccessCase> newAccess)
{
    Vector<RefPtr<AccessCase>, 2> newAccesses;
    newAccesses.append(WTFMove(newAccess));
    return addCases(locker, vm, codeBlock, stubInfo, WTFMove(newAccesses));
}

bool PolymorphicAccess::visitWeak(VM& vm) const
{
    for (unsigned i = 0; i < size(); ++i) {
        if (!at(i).visitWeak(vm))
            return false;
    }
    if (m_stubRoutine) {
        for (StructureID weakReference : m_stubRoutine->weakStructures()) {
            Structure* structure = vm.getStructure(weakReference);
            if (!vm.heap.isMarked(structure))
                return false;
        }
    }
    return true;
}

template<typename Visitor>
void PolymorphicAccess::propagateTransitions(Visitor& visitor) const
{
    for (unsigned i = 0; i < size(); ++i)
        at(i).propagateTransitions(visitor);
}

template void PolymorphicAccess::propagateTransitions(AbstractSlotVisitor&) const;
template void PolymorphicAccess::propagateTransitions(SlotVisitor&) const;

template<typename Visitor>
void PolymorphicAccess::visitAggregateImpl(Visitor& visitor)
{
    for (unsigned i = 0; i < size(); ++i)
        at(i).visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(PolymorphicAccess);

void PolymorphicAccess::dump(PrintStream& out) const
{
    out.print(RawPointer(this), ":[");
    CommaPrinter comma;
    for (auto& entry : m_list)
        out.print(comma, *entry);
    out.print("]");
}

void PolymorphicAccess::commit(
    const GCSafeConcurrentJSLocker&, VM& vm, std::unique_ptr<WatchpointsOnStructureStubInfo>& watchpoints, CodeBlock* codeBlock,
    StructureStubInfo& stubInfo, AccessCase& accessCase)
{
    // NOTE: We currently assume that this is relatively rare. It mainly arises for accesses to
    // properties on DOM nodes. For sure we cache many DOM node accesses, but even in
    // Real Pages (TM), we appear to spend most of our time caching accesses to properties on
    // vanilla objects or exotic objects from within JSC (like Arguments, those are super popular).
    // Those common kinds of JSC object accesses don't hit this case.
    
    for (WatchpointSet* set : accessCase.commit(vm)) {
        Watchpoint* watchpoint =
            WatchpointsOnStructureStubInfo::ensureReferenceAndAddWatchpoint(
                watchpoints, codeBlock, &stubInfo);
        
        set->add(watchpoint);
    }
}

AccessGenerationResult PolymorphicAccess::regenerate(const GCSafeConcurrentJSLocker& locker, VM& vm, JSGlobalObject* globalObject, CodeBlock* codeBlock, ECMAMode ecmaMode, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);
    
    dataLogLnIf(PolymorphicAccessInternal::verbose, "Regenerate with m_list: ", listDump(m_list));

    AccessGenerationState state(vm, globalObject, ecmaMode);

    state.access = this;
    state.stubInfo = &stubInfo;
    
    state.baseGPR = stubInfo.baseGPR;
    state.u.thisGPR = stubInfo.regs.thisGPR;
    state.valueRegs = stubInfo.valueRegs();

    // Regenerating is our opportunity to figure out what our list of cases should look like. We
    // do this here. The newly produced 'cases' list may be smaller than m_list. We don't edit
    // m_list in-place because we may still fail, in which case we want the PolymorphicAccess object
    // to be unmutated. For sure, we want it to hang onto any data structures that may be referenced
    // from the code of the current stub (aka previous).
    ListType cases;
    unsigned srcIndex = 0;
    unsigned dstIndex = 0;
    while (srcIndex < m_list.size()) {
        RefPtr<AccessCase> someCase = WTFMove(m_list[srcIndex++]);
        
        // If the case had been generated, then we have to keep the original in m_list in case we
        // fail to regenerate. That case may have data structures that are used by the code that it
        // had generated. If the case had not been generated, then we want to remove it from m_list.
        bool isGenerated = someCase->state() == AccessCase::Generated;
        
        [&] () {
            if (!someCase->couldStillSucceed())
                return;

            // Figure out if this is replaced by any later case. Given two cases A and B where A
            // comes first in the case list, we know that A would have triggered first if we had
            // generated the cases in a cascade. That's why this loop asks B->canReplace(A) but not
            // A->canReplace(B). If A->canReplace(B) was true then A would never have requested
            // repatching in cases where Repatch.cpp would have then gone on to generate B. If that
            // did happen by some fluke, then we'd just miss the redundancy here, which wouldn't be
            // incorrect - just slow. However, if A's checks failed and Repatch.cpp concluded that
            // this new condition could be handled by B and B->canReplace(A), then this says that we
            // don't need A anymore.
            //
            // If we can generate a binary switch, then A->canReplace(B) == B->canReplace(A). So,
            // it doesn't matter that we only do the check in one direction.
            for (unsigned j = srcIndex; j < m_list.size(); ++j) {
                if (m_list[j]->canReplace(*someCase))
                    return;
            }
            
            if (isGenerated)
                cases.append(someCase->clone());
            else
                cases.append(WTFMove(someCase));
        }();
        
        if (isGenerated)
            m_list[dstIndex++] = WTFMove(someCase);
    }
    m_list.resize(dstIndex);

    ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
    state.allocator = &allocator;
    allocator.lock(state.baseGPR);
    if (state.u.thisGPR != InvalidGPRReg)
        allocator.lock(state.u.thisGPR);
    if (state.valueRegs)
        allocator.lock(state.valueRegs);
#if USE(JSVALUE32_64)
    allocator.lock(stubInfo.baseTagGPR);
    if (stubInfo.v.thisTagGPR != InvalidGPRReg)
        allocator.lock(stubInfo.v.thisTagGPR);
#endif
    if (stubInfo.m_stubInfoGPR != InvalidGPRReg)
        allocator.lock(stubInfo.m_stubInfoGPR);
    if (stubInfo.m_arrayProfileGPR != InvalidGPRReg)
        allocator.lock(stubInfo.m_arrayProfileGPR);

    state.scratchGPR = allocator.allocateScratchGPR();

    for (auto& accessCase : cases) {
        if (accessCase->needsScratchFPR()) {
            state.scratchFPR = allocator.allocateScratchFPR();
            break;
        }
    }

    bool generatedFinalCode = false;

    // If the resulting set of cases is so big that we would stop caching and this is InstanceOf,
    // then we want to generate the generic InstanceOf and then stop.
    if (cases.size() >= Options::maxAccessVariantListSize()
        && stubInfo.accessType == AccessType::InstanceOf) {
        while (!cases.isEmpty())
            m_list.append(cases.takeLast());
        cases.append(AccessCase::create(vm, codeBlock, AccessCase::InstanceOfGeneric, nullptr));
        generatedFinalCode = true;
    }

    dataLogLnIf(PolymorphicAccessInternal::verbose, "Optimized cases: ", listDump(cases));

    bool doesCalls = false;
    bool doesJSGetterSetterCalls = false;
    bool canBeShared = Options::useDataICSharing();
    Vector<JSCell*> cellsToMark;
    FixedVector<RefPtr<AccessCase>> keys(cases.size());
    unsigned index = 0;
    for (auto& entry : cases) {
        doesCalls |= entry->doesCalls(vm, &cellsToMark);
        switch (entry->type()) {
        case AccessCase::Getter:
        case AccessCase::Setter:
            // Getter / Setter relies on stack-pointer adjustment, which is tied to the linked CodeBlock, which makes this code unshareable.
            canBeShared = false;
            doesJSGetterSetterCalls = true;
            break;
        case AccessCase::CustomValueGetter:
        case AccessCase::CustomAccessorGetter:
        case AccessCase::CustomValueSetter:
        case AccessCase::CustomAccessorSetter:
            // Custom getter / setter emits JSGlobalObject pointer, which is tied to the linked CodeBlock.
            canBeShared = false;
            break;
        default:
            break;
        }
        keys[index] = entry;
        ++index;
    }
    state.m_doesCalls = doesCalls;
    state.m_doesJSGetterSetterCalls = doesJSGetterSetterCalls;

    // At this point we're convinced that 'cases' contains the cases that we want to JIT now and we
    // won't change that set anymore.
    
    bool allGuardedByStructureCheck = true;
    bool needsInt32PropertyCheck = false;
    bool needsStringPropertyCheck = false;
    bool needsSymbolPropertyCheck = false;
    for (auto& newCase : cases) {
        if (!stubInfo.hasConstantIdentifier) {
            if (newCase->requiresIdentifierNameMatch()) {
                if (newCase->uid()->isSymbol())
                    needsSymbolPropertyCheck = true;
                else
                    needsStringPropertyCheck = true;
            } else if (newCase->requiresInt32PropertyCheck())
                needsInt32PropertyCheck = true; 
        }
        commit(locker, vm, state.watchpoints, codeBlock, stubInfo, *newCase);
        allGuardedByStructureCheck &= newCase->guardedByStructureCheck(stubInfo);
        if (newCase->usesPolyProto())
            canBeShared = false;
    }
    if (needsSymbolPropertyCheck || needsStringPropertyCheck || needsInt32PropertyCheck)
        canBeShared = false;

    auto finishCodeGeneration = [&](RefPtr<PolymorphicAccessJITStubRoutine>&& stub) {
        m_stubRoutine = WTFMove(stub);
        m_watchpoints = WTFMove(state.watchpoints);
        dataLogLnIf(PolymorphicAccessInternal::verbose, "Returning: ", m_stubRoutine->code());

        m_list = WTFMove(cases);
        m_list.shrinkToFit();

        AccessGenerationResult::Kind resultKind;
        if (m_list.size() >= Options::maxAccessVariantListSize() || generatedFinalCode)
            resultKind = AccessGenerationResult::GeneratedFinalCode;
        else
            resultKind = AccessGenerationResult::GeneratedNewCode;

        return AccessGenerationResult(resultKind, m_stubRoutine->code().code());
    };

    CCallHelpers jit(codeBlock);
    state.jit = &jit;

    if (codeBlock->useDataIC()) {
        if (state.m_doesJSGetterSetterCalls) {
            // We have no guarantee that stack-pointer is the expected one. This is not a problem if we do not have JS getter / setter calls since stack-pointer is
            // a callee-save register in the C calling convension. However, our JS executable call does not save stack-pointer. So we are adjusting stack-pointer after
            // JS getter / setter calls. But this could be different from the initial stack-pointer, and makes PAC tagging broken.
            // To ensure PAC-tagging work, we first adjust stack-pointer to the appropriate one.
            jit.addPtr(CCallHelpers::TrustedImm32(codeBlock->stackPointerOffset() * sizeof(Register)), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
            jit.tagReturnAddress();
        } else
            jit.tagReturnAddress();
    }

    state.preservedReusedRegisterState =
        allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
    
    if (cases.isEmpty()) {
        // This is super unlikely, but we make it legal anyway.
        state.failAndRepatch.append(jit.jump());
    } else if (!allGuardedByStructureCheck || cases.size() == 1) {
        // If there are any proxies in the list, we cannot just use a binary switch over the structure.
        // We need to resort to a cascade. A cascade also happens to be optimal if we only have just
        // one case.
        CCallHelpers::JumpList fallThrough;
        if (needsInt32PropertyCheck || needsStringPropertyCheck || needsSymbolPropertyCheck) {
            if (needsInt32PropertyCheck) {
                CCallHelpers::Jump notInt32;

                if (!stubInfo.propertyIsInt32) {
#if USE(JSVALUE64) 
                    notInt32 = jit.branchIfNotInt32(state.u.propertyGPR);
#else
                    notInt32 = jit.branchIfNotInt32(state.stubInfo->v.propertyTagGPR);
#endif
                }
                for (unsigned i = cases.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.clear();
                    if (cases[i]->requiresInt32PropertyCheck())
                        cases[i]->generateWithGuard(state, fallThrough);
                }

                if (needsStringPropertyCheck || needsSymbolPropertyCheck) {
                    if (notInt32.isSet())
                        notInt32.link(&jit);
                    fallThrough.link(&jit);
                    fallThrough.clear();
                } else {
                    if (notInt32.isSet())
                        state.failAndRepatch.append(notInt32);
                }
            }

            if (needsStringPropertyCheck) {
                CCallHelpers::JumpList notString;
                GPRReg propertyGPR = state.u.propertyGPR;
                if (!stubInfo.propertyIsString) {
#if USE(JSVALUE32_64)
                    GPRReg propertyTagGPR = state.stubInfo->v.propertyTagGPR;
                    notString.append(jit.branchIfNotCell(propertyTagGPR));
#else
                    notString.append(jit.branchIfNotCell(propertyGPR));
#endif
                    notString.append(jit.branchIfNotString(propertyGPR));
                }

                jit.loadPtr(MacroAssembler::Address(propertyGPR, JSString::offsetOfValue()), state.scratchGPR);

                state.failAndRepatch.append(jit.branchIfRopeStringImpl(state.scratchGPR));

                for (unsigned i = cases.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.clear();
                    if (cases[i]->requiresIdentifierNameMatch() && !cases[i]->uid()->isSymbol())
                        cases[i]->generateWithGuard(state, fallThrough);
                }

                if (needsSymbolPropertyCheck) {
                    notString.link(&jit);
                    fallThrough.link(&jit);
                    fallThrough.clear();
                } else
                    state.failAndRepatch.append(notString);
            }

            if (needsSymbolPropertyCheck) {
                CCallHelpers::JumpList notSymbol;
                if (!stubInfo.propertyIsSymbol) {
                    GPRReg propertyGPR = state.u.propertyGPR;
#if USE(JSVALUE32_64)
                    GPRReg propertyTagGPR = state.stubInfo->v.propertyTagGPR;
                    notSymbol.append(jit.branchIfNotCell(propertyTagGPR));
#else
                    notSymbol.append(jit.branchIfNotCell(propertyGPR));
#endif
                    notSymbol.append(jit.branchIfNotSymbol(propertyGPR));
                }

                for (unsigned i = cases.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.clear();
                    if (cases[i]->requiresIdentifierNameMatch() && cases[i]->uid()->isSymbol())
                        cases[i]->generateWithGuard(state, fallThrough);
                }

                state.failAndRepatch.append(notSymbol);
            }
        } else {
            // Cascade through the list, preferring newer entries.
            for (unsigned i = cases.size(); i--;) {
                fallThrough.link(&jit);
                fallThrough.clear();
                cases[i]->generateWithGuard(state, fallThrough);
            }
        }

        state.failAndRepatch.append(fallThrough);

    } else {
        jit.load32(
            CCallHelpers::Address(state.baseGPR, JSCell::structureIDOffset()),
            state.scratchGPR);
        
        Vector<int64_t> caseValues(cases.size());
        for (unsigned i = 0; i < cases.size(); ++i)
            caseValues[i] = bitwise_cast<int32_t>(cases[i]->structure()->id());
        
        BinarySwitch binarySwitch(state.scratchGPR, caseValues, BinarySwitch::Int32);
        while (binarySwitch.advance(jit))
            cases[binarySwitch.caseIndex()]->generate(state);
        state.failAndRepatch.append(binarySwitch.fallThrough());
    }

    if (!state.failAndIgnore.empty()) {
        state.failAndIgnore.link(&jit);
        
        // Make sure that the inline cache optimization code knows that we are taking slow path because
        // of something that isn't patchable. The slow path will decrement "countdown" and will only
        // patch things if the countdown reaches zero. We increment the slow path count here to ensure
        // that the slow path does not try to patch.
        if (codeBlock->useDataIC()) {
#if CPU(X86) || CPU(X86_64)
            jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()));
#else
            jit.load8(CCallHelpers::Address(stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()), state.scratchGPR);
            jit.add32(CCallHelpers::TrustedImm32(1), state.scratchGPR);
            jit.store8(state.scratchGPR, CCallHelpers::Address(stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()));
#endif
        } else {
#if CPU(X86) || CPU(X86_64)
            jit.move(CCallHelpers::TrustedImmPtr(&stubInfo.countdown), state.scratchGPR);
            jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(state.scratchGPR));
#else
            jit.load8(&stubInfo.countdown, state.scratchGPR);
            jit.add32(CCallHelpers::TrustedImm32(1), state.scratchGPR);
            jit.store8(state.scratchGPR, &stubInfo.countdown);
#endif
        }
    }

    CCallHelpers::JumpList failure;
    if (allocator.didReuseRegisters()) {
        state.failAndRepatch.link(&jit);
        state.restoreScratch();
    } else
        failure = state.failAndRepatch;
    failure.append(jit.jump());

    CodeBlock* codeBlockThatOwnsExceptionHandlers = nullptr;
    DisposableCallSiteIndex callSiteIndexForExceptionHandling;
    if (state.needsToRestoreRegistersIfException() && doesJSGetterSetterCalls) {
        // Emit the exception handler.
        // Note that this code is only reachable when doing genericUnwind from a pure JS getter/setter .
        // Note also that this is not reachable from custom getter/setter. Custom getter/setters will have 
        // their own exception handling logic that doesn't go through genericUnwind.
        MacroAssembler::Label makeshiftCatchHandler = jit.label();

        int stackPointerOffset = codeBlock->stackPointerOffset() * sizeof(EncodedJSValue);
        AccessGenerationState::SpillState spillStateForJSGetterSetter = state.spillStateForJSGetterSetter();
        ASSERT(!spillStateForJSGetterSetter.isEmpty());
        stackPointerOffset -= state.preservedReusedRegisterState.numberOfBytesPreserved;
        stackPointerOffset -= spillStateForJSGetterSetter.numberOfStackBytesUsedForRegisterPreservation;

        jit.loadPtr(vm.addressOfCallFrameForCatch(), GPRInfo::callFrameRegister);
        jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

        state.restoreLiveRegistersFromStackForCallWithThrownException(spillStateForJSGetterSetter);
        state.restoreScratch();
        CCallHelpers::Jump jumpToOSRExitExceptionHandler = jit.jump();

        HandlerInfo oldHandler = state.originalExceptionHandler();
        DisposableCallSiteIndex newExceptionHandlingCallSite = state.callSiteIndexForExceptionHandling();
        jit.addLinkTask(
            [=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(jumpToOSRExitExceptionHandler, oldHandler.nativeCode);

                HandlerInfo handlerToRegister = oldHandler;
                handlerToRegister.nativeCode = linkBuffer.locationOf<ExceptionHandlerPtrTag>(makeshiftCatchHandler);
                handlerToRegister.start = newExceptionHandlingCallSite.bits();
                handlerToRegister.end = newExceptionHandlingCallSite.bits() + 1;
                codeBlock->appendExceptionHandler(handlerToRegister);
            });

        // We set these to indicate to the stub to remove itself from the CodeBlock's
        // exception handler table when it is deallocated.
        codeBlockThatOwnsExceptionHandlers = codeBlock;
        ASSERT(JITCode::isOptimizingJIT(codeBlockThatOwnsExceptionHandlers->jitType()));
        callSiteIndexForExceptionHandling = state.callSiteIndexForExceptionHandling();
    }

    if (codeBlock->useDataIC()) {
        failure.link(&jit);
        // In ARM64, we do not push anything on stack specially.
        // So we can just jump to the slow-path even though this thunk is called (not jumped).
        // FIXME: We should tail call to the thunk which calls the slow path function.
        // And we should eliminate IC slow-path generation in BaselineJIT.
        jit.farJump(CCallHelpers::Address(stubInfo.m_stubInfoGPR, StructureStubInfo::offsetOfSlowPathStartLocation()), JITStubRoutinePtrTag);
    }

    RefPtr<PolymorphicAccessJITStubRoutine> stub;
    FixedVector<StructureID> weakStructures(WTFMove(state.weakStructures));
    if (codeBlock->useDataIC() && canBeShared) {
        SharedJITStubSet::Searcher searcher {
            stubInfo.baseGPR,
            stubInfo.valueGPR,
            stubInfo.regs.thisGPR,
            stubInfo.m_stubInfoGPR,
            stubInfo.m_arrayProfileGPR,
            stubInfo.usedRegisters,
            keys,
            weakStructures,
        };
        stub = vm.m_sharedJITStubs->find(searcher);
        if (stub) {
            dataLogLnIf(PolymorphicAccessInternal::verbose, "Found existing code stub ", stub->code());
            return finishCodeGeneration(WTFMove(stub));
        }
    }

    LinkBuffer linkBuffer(jit, codeBlock, LinkBuffer::Profile::InlineCache, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) {
        dataLogLnIf(PolymorphicAccessInternal::verbose, "Did fail to allocate.");
        return AccessGenerationResult::GaveUp;
    }

    CodeLocationLabel<JSInternalPtrTag> successLabel = stubInfo.doneLocation;

    if (codeBlock->useDataIC())
        ASSERT(state.success.empty());
    else {
        linkBuffer.link(state.success, successLabel);
        linkBuffer.link(failure, stubInfo.slowPathStartLocation);
    }
    
    dataLogLnIf(PolymorphicAccessInternal::verbose, FullCodeOrigin(codeBlock, stubInfo.codeOrigin), ": Generating polymorphic access stub for ", listDump(cases));

    MacroAssemblerCodeRef<JITStubRoutinePtrTag> code = FINALIZE_CODE_FOR(
        codeBlock, linkBuffer, JITStubRoutinePtrTag,
        "%s", toCString("Access stub for ", *codeBlock, " ", stubInfo.codeOrigin, " with return point ", successLabel, ": ", listDump(cases)).data());
    
    stub = createICJITStubRoutine(code, WTFMove(keys), WTFMove(weakStructures), vm, codeBlock, doesCalls, cellsToMark, WTFMove(state.m_callLinkInfos), codeBlockThatOwnsExceptionHandlers, callSiteIndexForExceptionHandling);

    if (codeBlock->useDataIC()) {
        if (canBeShared)
            vm.m_sharedJITStubs->add(SharedJITStubSet::Hash::Key(stubInfo.baseGPR, stubInfo.valueGPR, stubInfo.regs.thisGPR, stubInfo.m_stubInfoGPR, stubInfo.m_arrayProfileGPR, stubInfo.usedRegisters, stub.get()));
    }

    return finishCodeGeneration(WTFMove(stub));
}

void PolymorphicAccess::aboutToDie()
{
    if (m_stubRoutine)
        m_stubRoutine->aboutToDie();
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, AccessGenerationResult::Kind kind)
{
    switch (kind) {
    case AccessGenerationResult::MadeNoChanges:
        out.print("MadeNoChanges");
        return;
    case AccessGenerationResult::GaveUp:
        out.print("GaveUp");
        return;
    case AccessGenerationResult::Buffered:
        out.print("Buffered");
        return;
    case AccessGenerationResult::GeneratedNewCode:
        out.print("GeneratedNewCode");
        return;
    case AccessGenerationResult::GeneratedFinalCode:
        out.print("GeneratedFinalCode");
        return;
    case AccessGenerationResult::ResetStubAndFireWatchpoints:
        out.print("ResetStubAndFireWatchpoints");
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Load:
        out.print("Load");
        return;
    case AccessCase::Transition:
        out.print("Transition");
        return;
    case AccessCase::Delete:
        out.print("Delete");
        return;
    case AccessCase::DeleteNonConfigurable:
        out.print("DeleteNonConfigurable");
        return;
    case AccessCase::DeleteMiss:
        out.print("DeleteMiss");
        return;
    case AccessCase::Replace:
        out.print("Replace");
        return;
    case AccessCase::Miss:
        out.print("Miss");
        return;
    case AccessCase::GetGetter:
        out.print("GetGetter");
        return;
    case AccessCase::Getter:
        out.print("Getter");
        return;
    case AccessCase::Setter:
        out.print("Setter");
        return;
    case AccessCase::CustomValueGetter:
        out.print("CustomValueGetter");
        return;
    case AccessCase::CustomAccessorGetter:
        out.print("CustomAccessorGetter");
        return;
    case AccessCase::CustomValueSetter:
        out.print("CustomValueSetter");
        return;
    case AccessCase::CustomAccessorSetter:
        out.print("CustomAccessorSetter");
        return;
    case AccessCase::IntrinsicGetter:
        out.print("IntrinsicGetter");
        return;
    case AccessCase::InHit:
        out.print("InHit");
        return;
    case AccessCase::InMiss:
        out.print("InMiss");
        return;
    case AccessCase::CheckPrivateBrand:
        out.print("CheckPrivateBrand");
        return;
    case AccessCase::SetPrivateBrand:
        out.print("SetPrivateBrand");
        return;
    case AccessCase::ArrayLength:
        out.print("ArrayLength");
        return;
    case AccessCase::StringLength:
        out.print("StringLength");
        return;
    case AccessCase::DirectArgumentsLength:
        out.print("DirectArgumentsLength");
        return;
    case AccessCase::ScopedArgumentsLength:
        out.print("ScopedArgumentsLength");
        return;
    case AccessCase::ModuleNamespaceLoad:
        out.print("ModuleNamespaceLoad");
        return;
    case AccessCase::InstanceOfHit:
        out.print("InstanceOfHit");
        return;
    case AccessCase::InstanceOfMiss:
        out.print("InstanceOfMiss");
        return;
    case AccessCase::InstanceOfGeneric:
        out.print("InstanceOfGeneric");
        return;
    case AccessCase::IndexedInt32Load:
        out.print("IndexedInt32Load");
        return;
    case AccessCase::IndexedDoubleLoad:
        out.print("IndexedDoubleLoad");
        return;
    case AccessCase::IndexedContiguousLoad:
        out.print("IndexedContiguousLoad");
        return;
    case AccessCase::IndexedArrayStorageLoad:
        out.print("IndexedArrayStorageLoad");
        return;
    case AccessCase::IndexedScopedArgumentsLoad:
        out.print("IndexedScopedArgumentsLoad");
        return;
    case AccessCase::IndexedDirectArgumentsLoad:
        out.print("IndexedDirectArgumentsLoad");
        return;
    case AccessCase::IndexedTypedArrayInt8Load:
        out.print("IndexedTypedArrayInt8Load");
        return;
    case AccessCase::IndexedTypedArrayUint8Load:
        out.print("IndexedTypedArrayUint8Load");
        return;
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
        out.print("IndexedTypedArrayUint8ClampedLoad");
        return;
    case AccessCase::IndexedTypedArrayInt16Load:
        out.print("IndexedTypedArrayInt16Load");
        return;
    case AccessCase::IndexedTypedArrayUint16Load:
        out.print("IndexedTypedArrayUint16Load");
        return;
    case AccessCase::IndexedTypedArrayInt32Load:
        out.print("IndexedTypedArrayInt32Load");
        return;
    case AccessCase::IndexedTypedArrayUint32Load:
        out.print("IndexedTypedArrayUint32Load");
        return;
    case AccessCase::IndexedTypedArrayFloat32Load:
        out.print("IndexedTypedArrayFloat32Load");
        return;
    case AccessCase::IndexedTypedArrayFloat64Load:
        out.print("IndexedTypedArrayFloat64Load");
        return;
    case AccessCase::IndexedStringLoad:
        out.print("IndexedStringLoad");
        return;
    case AccessCase::IndexedInt32Store:
        out.print("IndexedInt32Store");
        return;
    case AccessCase::IndexedDoubleStore:
        out.print("IndexedDoubleStore");
        return;
    case AccessCase::IndexedContiguousStore:
        out.print("IndexedContiguousStore");
        return;
    case AccessCase::IndexedArrayStorageStore:
        out.print("IndexedArrayStorageStore");
        return;
    case AccessCase::IndexedTypedArrayInt8Store:
        out.print("IndexedTypedArrayInt8Store");
        return;
    case AccessCase::IndexedTypedArrayUint8Store:
        out.print("IndexedTypedArrayUint8Store");
        return;
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
        out.print("IndexedTypedArrayUint8ClampedStore");
        return;
    case AccessCase::IndexedTypedArrayInt16Store:
        out.print("IndexedTypedArrayInt16Store");
        return;
    case AccessCase::IndexedTypedArrayUint16Store:
        out.print("IndexedTypedArrayUint16Store");
        return;
    case AccessCase::IndexedTypedArrayInt32Store:
        out.print("IndexedTypedArrayInt32Store");
        return;
    case AccessCase::IndexedTypedArrayUint32Store:
        out.print("IndexedTypedArrayUint32Store");
        return;
    case AccessCase::IndexedTypedArrayFloat32Store:
        out.print("IndexedTypedArrayFloat32Store");
        return;
    case AccessCase::IndexedTypedArrayFloat64Store:
        out.print("IndexedTypedArrayFloat64Store");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, AccessCase::State state)
{
    switch (state) {
    case AccessCase::Primordial:
        out.print("Primordial");
        return;
    case AccessCase::Committed:
        out.print("Committed");
        return;
    case AccessCase::Generated:
        out.print("Generated");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(JIT)


