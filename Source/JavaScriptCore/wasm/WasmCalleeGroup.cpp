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
#include "WasmWorklist.h"
#include <wtf/text/MakeString.h>

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
    , m_jsToWasmCallees(other.m_jsToWasmCallees)
    , m_callers(m_calleeCount)
    , m_wasmIndirectCallEntrypoints(other.m_wasmIndirectCallEntrypoints)
    , m_wasmIndirectCallWasmCallees(other.m_wasmIndirectCallWasmCallees)
    , m_wasmToWasmExitStubs(other.m_wasmToWasmExitStubs)
{
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
    m_plan = adoptRef(*new IPIntPlan(vm, moduleInformation, m_ipintCallees->span().data(), createSharedTask<Plan::CallbackType>([this, protectedThis = WTF::move(protectedThis)] (Plan&) {
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
        m_jsToWasmCallees = static_cast<IPIntPlan*>(m_plan.get())->takeJSToWasmCallees();

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

RefPtr<JITCallee> CalleeGroup::tryGetReplacementConcurrently(FunctionCodeIndex functionIndex) const
{
    if (m_optimizedCallees.isEmpty())
        return nullptr;

    // Do not use optimizedCalleesTuple. optimizedCalleesTuple handles currently-installing Callee. But we do not want to handle it actually.
    // We would like to peek the callee when it is stored into m_optimizedCallees without taking a lock.
    auto* tuple = &m_optimizedCallees[functionIndex];
    UNUSED_PARAM(tuple);
#if ENABLE(WEBASSEMBLY_OMGJIT)
    if (RefPtr callee = tuple->m_omgCallee)
        return callee;
#endif
#if ENABLE(WEBASSEMBLY_BBQJIT)
    {
        Locker locker { tuple->m_bbqCalleeLock };
        if (RefPtr callee = tuple->m_bbqCallee.get())
            return callee;
    }
#endif
    return nullptr;
}

#if ENABLE(WEBASSEMBLY_BBQJIT)
RefPtr<BBQCallee> CalleeGroup::tryGetBBQCalleeForLoopOSRConcurrently(VM& vm, FunctionCodeIndex functionIndex)
{
    if (m_optimizedCallees.isEmpty())
        return nullptr;

    // Do not use optimizedCalleesTuple. optimizedCalleesTuple adjusts the result with currently-installing Callee. But we do not want to handle it actually.
    // We would like to peek the callee when it is stored into m_optimizedCallees without taking a lock.
    auto* tuple = &m_optimizedCallees[functionIndex];
    RefPtr<BBQCallee> bbqCallee;
    {
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

    // We could have triggered a tier up from a BBQCallee has MemoryMode::BoundsChecking
    // but is currently running a MemoryMode::Signaling memory. In that case there may
    // be nothing to release.
    if (auto* tuple = optimizedCalleesTuple(locker, functionIndex)) [[likely]] {
        RefPtr<BBQCallee> bbqCallee;
        {
            Locker bbqLocker { tuple->m_bbqCalleeLock };
            bbqCallee = tuple->m_bbqCallee.convertToWeak();
        }
        bbqCallee->reportToVMsForDestruction();
        return;
    }

    ASSERT(mode() == MemoryMode::Signaling);
}
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT)
RefPtr<OMGCallee> CalleeGroup::tryGetOMGCalleeConcurrently(FunctionCodeIndex functionIndex)
{
    if (m_optimizedCallees.isEmpty())
        return nullptr;

    // Do not use optimizedCalleesTuple. optimizedCalleesTuple handles currently-installing Callee. But we do not want to handle it actually.
    // We would like to peek the callee when it is stored into m_optimizedCallees without taking a lock.
    auto* tuple = &m_optimizedCallees[functionIndex];
    return tuple->m_omgCallee;
}
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
bool CalleeGroup::startInstallingCallee(const AbstractLocker& locker, FunctionCodeIndex functionIndex, OptimizingJITCallee& callee)
{
    auto* slot = optimizedCalleesTuple(locker, functionIndex);
    if (!slot) [[unlikely]] {
        ensureOptimizedCalleesSlow(locker);
        slot = optimizedCalleesTuple(locker, functionIndex);
    }
    ASSERT(slot);

#if ENABLE(WEBASSEMBLY_OMGJIT)
    if (callee.compilationMode() == CompilationMode::OMGMode) {
        // Why does it happen? It is possible that some code is still running IPIntCallee, and OMGCallee is installed and BBQCallee gets retired.
        // But since IPIntCallee can only tier up to BBQCallee, it may spin up BBQCallee again.
        // And because of BBQCallee's new TierUpCounter, we may start introducing OMGCallee again.
        // For now, we make this defensive: making installation failed when OMGCallee is already installed.
        if (slot->m_omgCallee) [[unlikely]]
            return false;
        m_currentlyInstallingOptimizedCallees.m_omgCallee = Ref { uncheckedDowncast<OMGCallee>(callee) };
    } else
        m_currentlyInstallingOptimizedCallees.m_omgCallee = slot->m_omgCallee;
#endif // ENABLE(WEBASSEMBLY_OMGJIT)
    {
        Locker replacerLocker { m_currentlyInstallingOptimizedCallees.m_bbqCalleeLock };
        Locker locker { slot->m_bbqCalleeLock };
        if (callee.compilationMode() == CompilationMode::BBQMode)
            m_currentlyInstallingOptimizedCallees.m_bbqCallee = Ref { uncheckedDowncast<BBQCallee>(callee) };
        else
            m_currentlyInstallingOptimizedCallees.m_bbqCallee = slot->m_bbqCallee;
    }
    m_currentlyInstallingOptimizedCalleesIndex = functionIndex;
    return true;
}

void CalleeGroup::finalizeInstallingCallee(const AbstractLocker&, FunctionCodeIndex functionIndex)
{
    RELEASE_ASSERT(m_currentlyInstallingOptimizedCalleesIndex == functionIndex);
    auto* slot = &m_optimizedCallees[functionIndex];
    {
        Locker replacerLocker { m_currentlyInstallingOptimizedCallees.m_bbqCalleeLock };
        Locker locker { slot->m_bbqCalleeLock };
        slot->m_bbqCallee = m_currentlyInstallingOptimizedCallees.m_bbqCallee;
        m_currentlyInstallingOptimizedCallees.m_bbqCallee = nullptr;
    }
#if ENABLE(WEBASSEMBLY_OMGJIT)
    slot->m_omgCallee = std::exchange(m_currentlyInstallingOptimizedCallees.m_omgCallee, nullptr);
#endif
    m_currentlyInstallingOptimizedCalleesIndex = { };
}

bool CalleeGroup::installOptimizedCallee(const AbstractLocker& locker, const ModuleInformation& info, FunctionCodeIndex functionIndex, Ref<OptimizingJITCallee>&& callee, const FixedBitVector& outgoingJITDirectCallees)
{
    // We want to make sure we publish our callee at the same time as we link our callsites. This enables us to ensure we
    // always call the fastest code. Any function linked after us will see our new code and the new callsites, which they
    // will update. It's also ok if they publish their code before we reset the instruction caches because after we release
    // the lock our code is ready to be published too.

    if (!startInstallingCallee(locker, functionIndex, callee.get()))
        return false;
    reportCallees(locker, callee.ptr(), outgoingJITDirectCallees);

    for (auto& call : callee->wasmToWasmCallsites()) {
        CodePtr<WasmEntryPtrTag> entrypoint;
        if (call.functionIndexSpace < info.importFunctionCount())
            entrypoint = m_wasmToWasmExitStubs[call.functionIndexSpace].code();
        else {
            Ref calleeCallee = wasmEntrypointCalleeFromFunctionIndexSpace(locker, call.functionIndexSpace);
            entrypoint = calleeCallee->entrypoint().retagged<WasmEntryPtrTag>();
        }

        // FIXME: This does an icache flush for each of these... which doesn't make any sense since this code isn't runnable here
        // and any stale cache will be evicted when updateCallsitesToCallUs is called.
        MacroAssembler::repatchNearCall(call.callLocation, CodeLocationLabel<WasmEntryPtrTag>(entrypoint));
    }
    {
        Ref calleeCallee = wasmEntrypointCalleeFromFunctionIndexSpace(locker, callee->index());
        if (calleeCallee == callee) [[likely]] {
            auto entrypoint = calleeCallee->entrypoint().retagged<WasmEntryPtrTag>();
            updateCallsitesToCallUs(locker, CodeLocationLabel<WasmEntryPtrTag>(entrypoint), functionIndex);
        } else
            resetInstructionCacheOnAllThreads();
    }
    WTF::storeStoreFence();
    finalizeInstallingCallee(locker, functionIndex);
    return true;
}

void CalleeGroup::updateCallsitesToCallUs(const AbstractLocker& locker, CodeLocationLabel<WasmEntryPtrTag> entrypoint, FunctionCodeIndex functionIndex)
{
    constexpr bool verbose = false;
    dataLogLnIf(verbose, "Updating callsites for ", functionIndex, " to target ", RawPointer(entrypoint.taggedPtr()));
    struct Callsite {
        CodeLocationNearCall<WasmEntryPtrTag> callLocation;
        CodeLocationLabel<WasmEntryPtrTag> target;
    };

    // This is necessary since Callees are released under `Heap::stopThePeriphery()`, but that only stops JS compiler
    // threads and not wasm ones. So the OMGOSREntryCallee could die between the time we collect the callsites and when
    // we actually repatch its callsites.
    // FIXME: These inline capacities were picked semi-randomly. We should figure out if there's a better number.
    Vector<Ref<OMGOSREntryCallee>, 4> keepAliveOSREntryCallees;
    Vector<Callsite, 16> callsites;

    auto functionSpaceIndex = toSpaceIndex(functionIndex);
    auto collectCallsites = [&](JITCallee* caller) {
        if (!caller)
            return;

        // FIXME: This should probably be a variant of FixedVector<UnlinkedWasmToWasmCall> and UncheckedKeyHashMap<FunctionIndex, FixedVector<UnlinkedWasmToWasmCall>> for big functions.
        for (UnlinkedWasmToWasmCall& callsite : caller->wasmToWasmCallsites()) {
            if (callsite.functionIndexSpace == functionSpaceIndex) {
                dataLogLnIf(verbose, "Repatching call [", toCodeIndex(caller->index()), "] at: ", RawPointer(callsite.callLocation.dataLocation()), " to ", RawPointer(entrypoint.taggedPtr()));
                CodeLocationLabel<WasmEntryPtrTag> target = MacroAssembler::prepareForAtomicRepatchNearCallConcurrently(callsite.callLocation, entrypoint);
                callsites.append({ callsite.callLocation, target });
            }
        }
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
            Locker locker { tuple->m_bbqCalleeLock };
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
                collectCallsites(callee.get());
                keepAliveOSREntryCallees.append(callee.releaseNonNull());
            } else
                m_osrEntryCallees.remove(iter);
        }
#endif
    };

    WTF::switchOn(m_callers[functionIndex],
        [&](SparseCallers& callers) {
            callsites.reserveInitialCapacity(callers.size());
            for (uint32_t caller : callers)
                handleCallerIndex(caller);
        },
        [&](DenseCallers& callers) {
            callsites.reserveInitialCapacity(callers.bitCount());
            for (uint32_t caller : callers)
                handleCallerIndex(caller);
        }
    );

    // It's important to make sure we do this before we make any of the code we just compiled visible. If we didn't, we could end up
    // where we are tiering up some function A to A' and we repatch some function B to call A' instead of A. Another CPU could see
    // the updates to B but still not have reset its cache of A', which would lead to all kinds of badness.
    resetInstructionCacheOnAllThreads();
    WTF::storeStoreFence(); // This probably isn't necessary but it's good to be paranoid.

    m_wasmIndirectCallEntrypoints[functionIndex] = entrypoint;

    // FIXME: This does an icache flush for each repatch but we
    // 1) only need one at the end.
    // 2) probably don't need one at all because we don't compile wasm on mutator threads so we don't have to worry about cache coherency.
    for (auto& callsite : callsites) {
        dataLogLnIf(verbose, "Repatching call at: ", RawPointer(callsite.callLocation.dataLocation()), " to ", RawPointer(entrypoint.taggedPtr()));
        MacroAssembler::repatchNearCall(callsite.callLocation, callsite.target);
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

            Locker locker { tuple->m_bbqCalleeLock };
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
