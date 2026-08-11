/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
#include "WasmCalleeGroup.h"

#if ENABLE(WEBASSEMBLY)

#include "LinkBuffer.h"
#include "WasmBBQPlan.h"
#include "WasmCallee.h"
#include "WasmIPIntPlan.h"
#include "WasmMachineThreads.h"
#include "WasmModuleInformation.h"
#include "WasmWorklist.h"

namespace JSC { namespace Wasm {

Ref<CalleeGroup> CalleeGroup::createFromIPInt(VM& vm, MemoryMode mode, ModuleInformation& moduleInformation, Ref<IPIntCallees>&& ipintCallees)
{
    return adoptRef(*new CalleeGroup(vm, mode, moduleInformation, WTF::move(ipintCallees)));
}

Ref<CalleeGroup> CalleeGroup::createFromExisting(MemoryMode mode, const CalleeGroup& other)
{
    return adoptRef(*new CalleeGroup(mode, other));
}

CalleeGroup::CalleeGroup(MemoryMode mode, const CalleeGroup& other)
    : m_calleeCount(other.m_calleeCount)
    , m_mode(mode)
    , m_ipintCallees(other.m_ipintCallees)
    , m_callers(m_calleeCount)
    , m_wasmIndirectCallEntrypoints(other.m_wasmIndirectCallEntrypoints)
    , m_wasmIndirectCallWasmCallees(other.m_wasmIndirectCallWasmCallees)
    , m_wasmToWasmExitStubs(other.m_wasmToWasmExitStubs)
{
    {
        Locker otherLocker { other.m_jsToWasmCalleesLock };
        m_jsToWasmCallees = other.m_jsToWasmCallees;
    }
    Locker locker { m_lock };
    setCompilationFinished();
}

CalleeGroup::CalleeGroup(VM& vm, MemoryMode mode, ModuleInformation& moduleInformation, Ref<IPIntCallees>&& ipintCallees)
    : m_calleeCount(moduleInformation.internalFunctionCount())
    , m_mode(mode)
    , m_ipintCallees(WTF::move(ipintCallees))
    , m_callers(m_calleeCount)
{
    RefPtr<CalleeGroup> protectedThis = this;
    m_plan = adoptRef(*new IPIntPlan(vm, moduleInformation, m_ipintCallees.copyRef(), createSharedTask<Plan::CallbackType>([this, protectedThis = WTF::move(protectedThis)] (Plan&) {
        Locker locker { m_lock };
        if (m_plan->failed()) {
            m_errorMessage = m_plan->errorMessage();
            setCompilationFinished();
            return;
        }

        m_wasmIndirectCallEntrypoints = FixedVector<CodePtr<WasmEntryPtrTag>>(m_calleeCount);
        m_wasmIndirectCallWasmCallees = FixedVector<RefPtr<Wasm::IPIntCallee>>(m_calleeCount);

        for (unsigned i = 0; i < m_calleeCount; ++i) {
            m_wasmIndirectCallEntrypoints[i] = m_ipintCallees->at(i)->entrypoint();
            m_wasmIndirectCallWasmCallees[i] = m_ipintCallees->at(i).ptr();
        }

        m_wasmToWasmExitStubs = m_plan->takeWasmToWasmExitStubs();

        setCompilationFinished();
    })));
    m_plan->setMode(mode);
    {
        Ref plan { *m_plan };
        if (plan->completeSyncIfPossible())
            return;
    }

    auto& worklist = Wasm::ensureWorklist();
    // Note, immediately after we enqueue the plan, there is a chance the above callback will be called.
    worklist.enqueue(*m_plan.get());
}

CalleeGroup::~CalleeGroup() = default;

JSToWasmCallee& CalleeGroup::ensureJSToWasmCallee(const ModuleInformation& moduleInformation, FunctionSpaceIndex functionIndexSpace)
{
    ASSERT(runnable());
    ASSERT(functionIndexSpace >= functionImportCount());
    unsigned calleeIndex = functionIndexSpace - functionImportCount();

    Locker locker { m_jsToWasmCalleesLock };
    auto addResult = m_jsToWasmCallees.ensure(calleeIndex, [&] {
        auto& ipintCallee = m_ipintCallees->at(calleeIndex).get();
        bool usesSIMD = moduleInformation.usesSIMD(FunctionCodeIndex(calleeIndex));
        auto callee = JSToWasmCallee::create(Ref<const RTT> { ipintCallee.signatureRTT() }, usesSIMD);
        callee->setWasmCallee(CalleeBits::encodeNativeCallee(&ipintCallee));
        return callee;
    });
    return *addResult.iterator->value;
}

void CalleeGroup::waitUntilFinished()
{
    RefPtr<Plan> plan;
    {
        Locker locker { m_lock };
        plan = m_plan;
    }

    if (plan) {
        auto& worklist = Wasm::ensureWorklist();
        worklist.completePlanSynchronously(*plan.get());
    }
    // else, if we don't have a plan, we're already compiled.
}

void CalleeGroup::compileAsync(VM& vm, AsyncCompilationCallback&& task)
{
    RefPtr<Plan> plan;
    {
        Locker locker { m_lock };
        plan = m_plan;
    }

    bool isAsync = plan;
    if (isAsync) {
        // We don't need to keep a RefPtr on the Plan because the worklist will keep
        // a RefPtr on the Plan until the plan finishes notifying all of its callbacks.
        isAsync = plan->addCompletionTaskIfNecessary(vm, createSharedTask<Plan::CallbackType>([this, task, protectedThis = Ref { *this }, isAsync](Plan&) {
            task->run(Ref { *this }, isAsync);
        }));
        if (isAsync)
            return;
    }

    task->run(Ref { *this }, isAsync);
}

#if ENABLE(WEBASSEMBLY_BBQJIT)
RefPtr<BBQCallee> CalleeGroup::tryGetBBQCalleeForLoopOSRConcurrently(VM& vm, FunctionCodeIndex functionIndex)
{
    if (m_optimizedCallees.isEmpty())
        return nullptr;

    auto* tuple = &m_optimizedCallees[functionIndex];
    RefPtr<BBQCallee> bbqCallee;
    {
        // get() and isStrong() must be read as one atomic pair: split, we could see a strong callee
        // and then miss that it was retired, skipping the report below.
        Locker bbqLocker { tuple->m_bbqCalleeLock };
        bbqCallee = tuple->m_bbqCallee.get();
        if (!bbqCallee)
            return nullptr;

        if (tuple->m_bbqCallee.isStrong())
            return bbqCallee;
    }

    // This means this callee has been released but hasn't yet been destroyed. We're safe to use it
    // as long as this VM knows to look for it the next time it scans for conservative roots.
    vm.heap.reportWasmCalleePendingDestruction(Ref { *bbqCallee });
    return bbqCallee;
}

void CalleeGroup::releaseBBQCallee(const AbstractLocker& locker, FunctionCodeIndex functionIndex)
{
    ASSERT(Options::freeRetiredWasmCode());

    // It's possible there are still a IPIntCallee around even when the BBQCallee
    // is destroyed. Since this function was clearly hot enough to get to OMG we should
    // tier it up soon.
    m_ipintCallees->at(functionIndex)->tierUpCounter().resetAndOptimizeSoon(m_mode);

    // OMG may install without a same-mode BBQCallee (e.g. BoundsChecking BBQ while
    // running Signaling memory). In that case there may be nothing to release.
    auto* tuple = optimizedCalleesTuple(locker, functionIndex);
    if (!tuple) [[unlikely]]
        return;

    RefPtr<BBQCallee> bbqCallee;
    {
        Locker bbqLocker { tuple->m_bbqCalleeLock };
        if (!tuple->m_bbqCallee.isStrong() || !tuple->m_bbqCallee.get())
            return;
        bbqCallee = tuple->m_bbqCallee.convertToWeak();
    }
    // Reported outside m_bbqCalleeLock: this takes the heap's own lock, and nothing in the heap
    // reaches back into CalleeGroup.
    bbqCallee->reportToVMsForDestruction();
}
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT)
RefPtr<OMGCallee> CalleeGroup::tryGetOMGCalleeConcurrently(FunctionCodeIndex functionIndex)
{
    // See tryGetBBQCalleeForLoopOSRConcurrently for why reading m_optimizedCallees without m_lock is
    // safe. m_omgCallee itself is written once and never cleared, so the worst case is that we miss
    // a tier-up that just landed.
    if (m_optimizedCallees.isEmpty())
        return nullptr;

    return m_optimizedCallees[functionIndex].m_omgCallee;
}
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
void CalleeGroup::CallerCallsiteFlushes::flush()
{
    // This only invalidates the caller's instruction cache; it does not context-synchronize the
    // threads executing those callers. So a core that already prefetched the old branch target may
    // keep calling the previous callee for an architecturally unbounded time after this returns.
    // That is safe because retired code is not freed until Heap::finalizeWasmCalleeCleanup() runs
    // with the world stopped in every VM that could be running it, and stopping a thread is itself a
    // context-synchronizing event. Anything that shortens a retired callee's lifetime must preserve
    // that property.

    // FIXME: Maybe it's worth doing a cpuid here on X86_64. We don't do any earlier in the callee
    // publication process as it's not strictly necessary.
    for (auto& callsite : callsites)
        MacroAssembler::flushNearCall(callsite.callLocation);
}

// Reserves this callee's place before any of its code is linked, so a competing install for the same
// function loses here rather than part-way through publication. Must run before reportCallees, or a
// loser would leave itself permanently recorded as a caller of everything it calls.
bool CalleeGroup::tryReserveCalleeForInstall(const AbstractLocker& locker, FunctionCodeIndex functionIndex, OptimizingJITCallee& callee)
{
    switch (callee.compilationMode()) {
#if ENABLE(WEBASSEMBLY_OMGJIT)
    case CompilationMode::OMGMode: {
        // Why does it happen? It is possible that some code is still running IPIntCallee, and OMGCallee is installed and BBQCallee gets retired.
        // But since IPIntCallee can only tier up to BBQCallee, it may spin up BBQCallee again.
        // And because of BBQCallee's new TierUpCounter, we may start introducing OMGCallee again.
        // For now, we make this defensive: making installation failed when OMGCallee is already installed.
        // This is only a reservation; publishing happens after the code is flushed, so it is rechecked
        // there too.
        auto* slot = optimizedCalleesTuple(locker, functionIndex);
        ASSERT(slot);
        if (slot->m_omgCallee) [[unlikely]]
            return false;

        for (auto& pending : m_pendingPublishCallees) {
            if (toCodeIndex(pending->index()) == functionIndex && pending->compilationMode() == CompilationMode::OMGMode) [[unlikely]]
                return false;
        }
        break;
    }

    case CompilationMode::OMGForOSREntryMode:
        // This only reserves/registers the callee, OMGOSREntryCallee's are only reachable via the
        // BBQCallee (after BBQCallee::setOSREntryCallee is called).
        if (!m_osrEntryCallees.add(functionIndex, ThreadSafeWeakPtr<OMGOSREntryCallee> { downcast<OMGOSREntryCallee>(callee) }).isNewEntry)
            return false;
        break;
#endif
#if ENABLE(WEBASSEMBLY_BBQJIT)
    case CompilationMode::BBQMode:
        // The IPInt tier-up counter's compilation status already serializes BBQ plans for a function.
        break;
#endif
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    addPendingPublishCallee(locker, callee);
    return true;

}

bool CalleeGroup::installOptimizedCallee(Locker<Lock>& locker, const ModuleInformation& info, FunctionCodeIndex functionIndex, Ref<OptimizingJITCallee>&& callee, const FixedBitVector& outgoingJITDirectCallees, CallerCallsiteFlushes& deferred)
{
    auto* slot = optimizedCalleesTuple(locker, functionIndex);
    if (!slot) [[unlikely]] {
        ensureOptimizedCalleesSlow(locker);
        slot = optimizedCalleesTuple(locker, functionIndex);
    }
    ASSERT(slot);

    // Tracking callees in the process of getting published is required for correctness. It avoids the
    // race of callee A (which calls B) dropping the lock below, B's OMGCallee going through the whole
    // process and never updating A. A would be stuck linking calls to the now retired BBQCallee and
    // could end up calling released code.
    if (!tryReserveCalleeForInstall(locker, functionIndex, callee.get())) [[unlikely]]
        return false;

    // OSREntryCallees are only valid for the specific loop they were compiled for and can't be used as
    // a general entrypoint.
    const bool weAreTheEntrypoint = callee->compilationMode() != CompilationMode::OMGForOSREntryMode;

    // Record who we call before dropping the lock below. This is what lets another thread that
    // retires one of our callees find us and relink our callsites while we are unpublished.
    reportCallees(locker, callee.ptr(), outgoingJITDirectCallees);

    auto ourEntrypoint = CodeLocationLabel<WasmEntryPtrTag>(callee->entrypoint().retagged<WasmEntryPtrTag>());
    auto ourSpaceIndex = callee->index();
    for (auto& call : callee->wasmToWasmCallsites()) {
        CodePtr<WasmEntryPtrTag> entrypoint;
        if (call.functionIndexSpace < info.importFunctionCount())
            entrypoint = m_wasmToWasmExitStubs[call.functionIndexSpace].code();
        else if (weAreTheEntrypoint && call.functionIndexSpace == ourSpaceIndex) {
            // Recursive call. We are deliberately not in m_optimizedCallees yet, so resolve it against
            // ourselves rather than looking it up. An OSR entry callee shares this index but is not the
            // function's entrypoint, so a recursive call from it resolves like any other call.
            entrypoint = ourEntrypoint;
        } else {
            Ref calleeCallee = wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace);
            entrypoint = calleeCallee->entrypoint().retagged<WasmEntryPtrTag>();
        }

        // Link the call site without flushing. LinkBuffer skipped its finalize instruction-cache
        // flush for this code, and the whole function is flushed once below, after every outgoing
        // call is linked. A per-site flush would be wasteful; relying on the finalize flush would be
        // unsafe, because these targets are written after it and an adjacent live instruction sharing
        // a cache line with a call site could pull the stale pre-link bytes into the instruction cache.
        MacroAssembler::repatchNearCall<jitMemcpyRepatchAtomic>(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
    }

    {
        // Flushing the instruction cache and synchronizing every wasm thread is pure cache maintenance:
        // it touches no CalleeGroup state, so it does not need m_lock, and it is far too slow to hold
        // the lock across. Our code stays unreachable to execution threads while the lock is dropped,
        // so it is safe to publish it only after this completes.
        // FIXME: Maybe we shouldn't drop the lock on X86_64. The stuff below is essentially a no-op there.
        DropLockForScope unlocked(locker);

        // Every outgoing call is linked, so flush the finished function before it is published. This is
        // the single instruction-cache flush that makes this code coherent. It must cover the whole
        // range, not just the call sites as the LinkBuffer didn't do it.
        auto [start, end] = callee->range();
        MacroAssembler::cacheFlush(start, static_cast<size_t>(static_cast<char*>(end) - static_cast<char*>(start)));

        barrierInstructionCacheOnAllThreads();
    }

    // At this point the callee is ready to be published.
    removePendingPublishCallee(locker, callee.get());

    if (!weAreTheEntrypoint)
        return true;

    // Another callee for this index may have been installed while the lock was dropped, so re-read
    // the slot before deciding what to publish.
    slot = optimizedCalleesTuple(locker, functionIndex);
    ASSERT(slot);

    // FIXME: We should do a better job ensuring that we don't compile callees while they're already tiering up.
#if ENABLE(WEBASSEMBLY_OMGJIT)
    if (callee->compilationMode() == CompilationMode::OMGMode) {
        RELEASE_ASSERT(!slot->m_omgCallee);
        slot->m_omgCallee = Ref { uncheckedDowncast<OMGCallee>(callee.get()) };
    }
#endif
#if ENABLE(WEBASSEMBLY_BBQJIT)
    if (callee->compilationMode() == CompilationMode::BBQMode) {
        Locker bbqLocker { slot->m_bbqCalleeLock };
        // A retired BBQCallee may still be here: releaseBBQCallee re-arms IPInt's tier-up counter, so
        // a function can be compiled to BBQ again before the previous callee is destroyed. Overwriting
        // that weak reference is fine, it does not own the callee. Overwriting a strong one would drop
        // the last reference to code that may still be executing.
        RELEASE_ASSERT(!slot->m_bbqCallee.isStrong() || !slot->m_bbqCallee.get());
        slot->m_bbqCallee = Ref { uncheckedDowncast<BBQCallee>(callee.get()) };
    }
#endif

    // We dropped the lock so a higher tier callee could have installed in that time frame.
    Ref currentCallee = wasmEntrypointCalleeFromFunctionIndexSpace(locker, ourSpaceIndex);
    if (currentCallee.ptr() == callee.ptr()) [[likely]]
        updateCallsitesToCallUs(locker, ourEntrypoint, functionIndex, deferred);

    return true;
}

void CalleeGroup::updateCallsitesToCallUs(const AbstractLocker& locker, CodeLocationLabel<WasmEntryPtrTag> entrypoint, FunctionCodeIndex functionIndex, CallerCallsiteFlushes& deferred)
{
    constexpr bool verbose = false;
    dataLogLnIf(verbose, "Updating callsites for ", functionIndex, " to target ", RawPointer(entrypoint.taggedPtr()));

    // This is necessary since Callees are released under `Heap::stopThePeriphery()`, but that only stops JS compiler
    // threads and not wasm ones. So a weakly held BBQCallee and its OMGOSREntryCallee could die between the time we
    // collect the callsites and when we flush them. Additionally, after a re-tier (where a fresh
    // BBQCallee replaces a retired one), m_osrEntryCallees may hold a stale weak ref to an OMGOSREntryCallee not
    // owned by the current BBQCallee, so we always keep it alive unconditionally. These references live in `deferred`
    // so the caller code backing each callsite stays alive until the deferred flush runs after m_lock is released.

    auto functionSpaceIndex = toSpaceIndex(functionIndex);
    auto collectCallsites = [&](JITCallee* caller) {
        if (!caller)
            return;

        // FIXME: This should probably be a variant of FixedVector<UnlinkedWasmToWasmCall> and UncheckedKeyHashMap<FunctionIndex, FixedVector<UnlinkedWasmToWasmCall>> for big functions.
        bool hadCall = false;
        for (UnlinkedWasmToWasmCall& callsite : caller->wasmToWasmCallsites()) {
            if (callsite.functionIndexSpace == functionSpaceIndex) {
                dataLogLnIf(verbose, "Repatching call [", toCodeIndex(caller->index()), "] at: ", RawPointer(callsite.callLocation.dataLocation()), " to ", RawPointer(entrypoint.taggedPtr()));
                CodeLocationLabel<WasmEntryPtrTag> target = MacroAssembler::prepareForAtomicRepatchNearCallConcurrently(callsite.callLocation, entrypoint);
                deferred.callsites.append({ callsite.callLocation, target });
                hadCall = true;
            }
        }

        if (hadCall)
            deferred.keepAliveCallees.append(*caller);
    };

    auto handleCallerIndex = [&](size_t caller) {
        auto callerIndex = FunctionCodeIndex(caller);
        assertIsHeld(m_lock);
        auto* tuple = optimizedCalleesTuple(locker, callerIndex);
        if (!tuple)
            return;

#if ENABLE(WEBASSEMBLY_BBQJIT)
        // This callee could be weak but we still need to update it since it could call our BBQ callee
        // that we're going to want to destroy.
        RefPtr<BBQCallee> bbqCallee;
        {
            Locker bbqLocker { tuple->m_bbqCalleeLock };
            bbqCallee = tuple->m_bbqCallee.get();
        }
        if (bbqCallee) {
            collectCallsites(bbqCallee.get());
            ASSERT(!bbqCallee->osrEntryCallee() || m_osrEntryCallees.find(callerIndex) != m_osrEntryCallees.end());
        }
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
        collectCallsites(tuple->m_omgCallee.get());
        if (auto iter = m_osrEntryCallees.find(callerIndex); iter != m_osrEntryCallees.end()) {
            if (RefPtr callee = iter->value.get()) {
                // Since there is a OMGOSREntryCallee, we need to collect all the callsites there and also keep it alive until we patch it.
                collectCallsites(callee.get());
            } else
                m_osrEntryCallees.remove(iter);
        }
#endif
    };

    WTF::switchOn(m_callers[functionIndex],
        [&](SparseCallers& callers) {
            deferred.callsites.reserveInitialCapacity(callers.size());
            for (uint32_t caller : callers)
                handleCallerIndex(caller);
        },
        [&](DenseCallers& callers) {
            deferred.callsites.reserveInitialCapacity(callers.bitCount());
            for (uint32_t caller : callers)
                handleCallerIndex(caller);
        }
    );

    // Callees that are linked but not yet published are not reachable through m_optimizedCallees, yet
    // they may already hold direct calls to us. If we are being retired they must be relinked too, or
    // they would call code that is about to be freed once they publish.
    for (auto& pending : m_pendingPublishCallees)
        collectCallsites(pending.ptr());

    WTF::storeStoreFence(); // This probably isn't necessary but it's good to be paranoid.

    m_wasmIndirectCallEntrypoints[functionIndex] = entrypoint;

    // Store the new call targets now (a single atomic instruction each, no flush), but defer the
    // instruction-cache flush until m_lock is released, via `deferred`. The previous callee stays
    // valid, so a caller that has not yet observed the new target simply keeps calling it until the
    // deferred flush makes the store visible.
    for (auto& callsite : deferred.callsites) {
        dataLogLnIf(verbose, "Repatching call at: ", RawPointer(callsite.callLocation.dataLocation()), " to ", RawPointer(entrypoint.taggedPtr()));
        MacroAssembler::repatchNearCall<jitMemcpyRepatchAtomic>(callsite.callLocation, callsite.target);
    }
}

void CalleeGroup::reportCallees(const AbstractLocker&, JITCallee* caller, const FixedBitVector& callees)
{
#if ASSERT_ENABLED
    for (const auto& call : caller->wasmToWasmCallsites()) {
        if (call.functionIndexSpace < functionImportCount())
            continue;
        ASSERT(const_cast<FixedBitVector&>(callees).test(toCodeIndex(call.functionIndexSpace)));
    }
#endif
    auto callerIndex = toCodeIndex(caller->index());
    ASSERT_WITH_MESSAGE(callees.size() == FixedBitVector(m_calleeCount).size(), "Make sure we're not indexing callees with the space index");

    for (uint32_t calleeIndex : callees) {
        WTF::switchOn(m_callers[calleeIndex],
            [&](SparseCallers& callers) {
                assertIsHeld(m_lock);
                callers.add(callerIndex.rawIndex());
                // FIXME: We should do this when we would resize to be bigger than the bitvectors count rather than after we've already resized.
                if (callers.memoryUse() >= DenseCallers::outOfLineMemoryUse(m_calleeCount)) {
                    BitVector vector;
                    for (uint32_t caller : callers)
                        vector.set(caller);
                    m_callers[calleeIndex] = WTF::move(vector);
                }
            },
            [&](DenseCallers& callers) {
                callers.set(callerIndex);
            }
        );
    }
}
#endif

TriState CalleeGroup::calleeIsReferenced(const AbstractLocker& locker, Wasm::Callee* callee) const
{
    UNUSED_PARAM(locker);
    switch (callee->compilationMode()) {
    case CompilationMode::IPIntMode:
        return TriState::True;
#if ENABLE(WEBASSEMBLY_BBQJIT)
    case CompilationMode::BBQMode: {
        FunctionCodeIndex index = toCodeIndex(callee->index());
        const auto* tuple = optimizedCalleesTuple(locker, index);
        if (!tuple)
            return TriState::Indeterminate;

        Locker locker { tuple->m_bbqCalleeLock };
        RefPtr bbqCallee = tuple->m_bbqCallee.get();
        if (tuple->m_bbqCallee.isWeak())
            return bbqCallee ? TriState::Indeterminate : TriState::False;
        return triState(bbqCallee);
    }
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
    case CompilationMode::OMGMode: {
        FunctionCodeIndex index = toCodeIndex(callee->index());
        const auto* tuple = optimizedCalleesTuple(locker, index);
        if (!tuple)
            return TriState::Indeterminate;
        return triState(tuple->m_omgCallee.get());
    }
    case CompilationMode::OMGForOSREntryMode: {
        FunctionCodeIndex index = toCodeIndex(callee->index());
        if (m_osrEntryCallees.get(index).get()) {
            // The BBQCallee really owns the OMGOSREntryCallee so as long as that's around the OMGOSREntryCallee is referenced.
            const auto* tuple = optimizedCalleesTuple(locker, index);
            if (!tuple)
                return TriState::Indeterminate;

            Locker bbqLocker { tuple->m_bbqCalleeLock };
            if (tuple->m_bbqCallee.get())
                return TriState::True;
            return TriState::Indeterminate;
        }
        return TriState::False;
    }
#endif
    // FIXME: This doesn't record the index its associated with so we can't validate anything here.
    case CompilationMode::JSToWasmMode:
    // FIXME: These are owned by JS, it's not clear how to verify they're still alive here.
    case CompilationMode::JSToWasmICMode:
    case CompilationMode::WasmToJSMode:
    case CompilationMode::WasmBuiltinMode:
    case CompilationMode::RestoreFrameMode:
        return TriState::True;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

bool CalleeGroup::isSafeToRun(MemoryMode memoryMode)
{
    UNUSED_PARAM(memoryMode);

    if (!runnable())
        return false;

    switch (m_mode) {
    case MemoryMode::BoundsChecking:
        return true;
    case MemoryMode::Signaling:
        // Code being in Signaling mode means that it performs no bounds checks.
        // Its memory, even if empty, absolutely must also be in Signaling mode
        // because the page protection detects out-of-bounds accesses.
        return memoryMode == MemoryMode::Signaling;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

void CalleeGroup::setCompilationFinished()
{
    m_plan = nullptr;
    m_compilationFinished.store(true);
}

void CalleeGroup::ensureOptimizedCalleesSlow(const AbstractLocker&)
{
    // We must use FixedVector. This is pointer size, and we can ensure that we can expose it atomically.
    static_assert(sizeof(FixedVector<OptimizedCallees>) <= sizeof(CPURegister));
    FixedVector<OptimizedCallees> vector(m_calleeCount);

    // We would like to expose this vector concurrently for optimization. Thus we must ensure that fields are fully initialized.
    WTF::storeStoreFence();

    m_optimizedCallees = WTF::move(vector);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
