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
#include "WasmLLIntPlan.h"
#include "WasmMachineThreads.h"
#include "WasmWorklist.h"
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

Ref<CalleeGroup> CalleeGroup::createFromLLInt(VM& vm, MemoryMode mode, ModuleInformation& moduleInformation, RefPtr<LLIntCallees> llintCallees)
{
    return adoptRef(*new CalleeGroup(vm, mode, moduleInformation, llintCallees));
}

Ref<CalleeGroup> CalleeGroup::createFromIPInt(VM& vm, MemoryMode mode, ModuleInformation& moduleInformation, RefPtr<IPIntCallees> ipintCallees)
{
    return adoptRef(*new CalleeGroup(vm, mode, moduleInformation, ipintCallees));
}

Ref<CalleeGroup> CalleeGroup::createFromExisting(MemoryMode mode, const CalleeGroup& other)
{
    return adoptRef(*new CalleeGroup(mode, other));
}

CalleeGroup::CalleeGroup(MemoryMode mode, const CalleeGroup& other)
    : m_calleeCount(other.m_calleeCount)
    , m_mode(mode)
    , m_llintCallees(other.m_llintCallees)
    , m_jsEntrypointCallees(other.m_jsEntrypointCallees)
    , m_callers(m_calleeCount)
    , m_wasmIndirectCallEntryPoints(other.m_wasmIndirectCallEntryPoints)
    , m_wasmIndirectCallWasmCallees(other.m_wasmIndirectCallWasmCallees)
    , m_wasmToWasmExitStubs(other.m_wasmToWasmExitStubs)
{
    Locker locker { m_lock };
    setCompilationFinished();
}

CalleeGroup::CalleeGroup(VM& vm, MemoryMode mode, ModuleInformation& moduleInformation, RefPtr<LLIntCallees> llintCallees)
    : m_calleeCount(moduleInformation.internalFunctionCount())
    , m_mode(mode)
    , m_llintCallees(llintCallees)
    , m_callers(m_calleeCount)
{
    RefPtr<CalleeGroup> protectedThis = this;
    m_plan = adoptRef(*new LLIntPlan(vm, moduleInformation, m_llintCallees->data(), createSharedTask<Plan::CallbackType>([this, protectedThis = WTFMove(protectedThis)] (Plan&) {
        if (!m_plan) {
            m_errorMessage = makeString("Out of memory while creating LLInt CalleeGroup"_s);
            setCompilationFinished();
            return;
        }
        Locker locker { m_lock };
        if (m_plan->failed()) {
            m_errorMessage = m_plan->errorMessage();
            setCompilationFinished();
            return;
        }

        m_wasmIndirectCallEntryPoints = FixedVector<CodePtr<WasmEntryPtrTag>>(m_calleeCount);
        m_wasmIndirectCallWasmCallees = FixedVector<RefPtr<Wasm::Callee>>(m_calleeCount);

        for (unsigned i = 0; i < m_calleeCount; ++i) {
            m_wasmIndirectCallEntryPoints[i] = m_llintCallees->at(i)->entrypoint();
            m_wasmIndirectCallWasmCallees[i] = m_llintCallees->at(i).ptr();
        }

        m_wasmToWasmExitStubs = m_plan->takeWasmToWasmExitStubs();
        m_jsEntrypointCallees = static_cast<LLIntPlan*>(m_plan.get())->takeJSCallees();

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

CalleeGroup::CalleeGroup(VM& vm, MemoryMode mode, ModuleInformation& moduleInformation, RefPtr<IPIntCallees> ipintCallees)
    : m_calleeCount(moduleInformation.internalFunctionCount())
    , m_mode(mode)
    , m_ipintCallees(ipintCallees)
    , m_callers(m_calleeCount)
{
    RefPtr<CalleeGroup> protectedThis = this;
    m_plan = adoptRef(*new IPIntPlan(vm, moduleInformation, m_ipintCallees->data(), createSharedTask<Plan::CallbackType>([this, protectedThis = WTFMove(protectedThis)] (Plan&) {
        Locker locker { m_lock };
        if (m_plan->failed()) {
            m_errorMessage = m_plan->errorMessage();
            setCompilationFinished();
            return;
        }

        m_wasmIndirectCallEntryPoints = FixedVector<CodePtr<WasmEntryPtrTag>>(m_calleeCount);
        m_wasmIndirectCallWasmCallees = FixedVector<RefPtr<Wasm::Callee>>(m_calleeCount);

        for (unsigned i = 0; i < m_calleeCount; ++i) {
            m_wasmIndirectCallEntryPoints[i] = m_ipintCallees->at(i)->entrypoint();
            m_wasmIndirectCallWasmCallees[i] = m_ipintCallees->at(i).ptr();
        }

        m_wasmToWasmExitStubs = m_plan->takeWasmToWasmExitStubs();
        m_jsEntrypointCallees = static_cast<IPIntPlan*>(m_plan.get())->takeJSCallees();

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

#if ENABLE(WEBASSEMBLY_BBQJIT)
BBQCallee* CalleeGroup::tryGetBBQCalleeForLoopOSR(const AbstractLocker&, VM& vm, FunctionCodeIndex functionIndex)
{
    if (m_bbqCallees.isEmpty())
        return nullptr;

    auto& maybeCallee = m_bbqCallees[functionIndex];
    if (maybeCallee.isStrong())
        return maybeCallee.ptr();

    RefPtr bbqCallee = maybeCallee.get();
    if (!bbqCallee)
        return nullptr;

    // This means this callee has been released but hasn't yet been destroyed. We're safe to use it
    // as long as this VM knows to look for it the next time it scans for conservative roots.
    BBQCallee* result = bbqCallee.get();
    vm.heap.reportWasmCalleePendingDestruction(bbqCallee.releaseNonNull());
    return result;
}

void CalleeGroup::releaseBBQCallee(const AbstractLocker&, FunctionCodeIndex functionIndex)
{
    if (!Options::freeRetiredWasmCode())
        return;

    // It's possible there are still a LLInt/IPIntCallee around even when the BBQCallee
    // is destroyed. Since this function was clearly hot enough to get to OMG we should
    // tier it up soon.
    if (m_ipintCallees)
        m_ipintCallees->at(functionIndex)->tierUpCounter().resetAndOptimizeSoon(m_mode);
    else if (m_llintCallees)
        m_llintCallees->at(functionIndex)->tierUpCounter().resetAndOptimizeSoon(m_mode);

    // We could have triggered a tier up from a BBQCallee has MemoryMode::BoundsChecking
    // but is currently running a MemoryMode::Signaling memory. In that case there may
    // be nothing to release.
    if (LIKELY(!m_bbqCallees.isEmpty())) {
        if (RefPtr bbqCallee = m_bbqCallees[functionIndex].convertToWeak()) {
            bbqCallee->reportToVMsForDestruction();
            return;
        }
    }

    ASSERT(mode() == MemoryMode::Signaling);
}
#endif

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
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
    Vector<RefPtr<OMGOSREntryCallee>, 4> keepAliveOSREntryCallees;
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
#if ENABLE(WEBASSEMBLY_BBQJIT)
        // This callee could be weak but we still need to update it since it could call our BBQ callee
        // that we're going to want to destroy.
        if (RefPtr<BBQCallee> bbqCallee = m_bbqCallees[callerIndex].get()) {
            collectCallsites(bbqCallee.get());
            ASSERT(!bbqCallee->osrEntryCallee() || m_osrEntryCallees.find(callerIndex) != m_osrEntryCallees.end());
        }
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
        collectCallsites(omgCallee(locker, callerIndex));
        if (auto iter = m_osrEntryCallees.find(callerIndex); iter != m_osrEntryCallees.end()) {
            if (RefPtr callee = iter->value.get()) {
                collectCallsites(callee.get());
                keepAliveOSREntryCallees.append(WTFMove(callee));
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

    m_wasmIndirectCallEntryPoints[functionIndex] = entrypoint;

    if (auto iter = m_jsEntrypointCallees.find(functionIndex); iter != m_jsEntrypointCallees.end())
        iter->value->setReplacementTarget(entrypoint);

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
                    m_callers[calleeIndex] = WTFMove(vector);
                }
            },
            [&](DenseCallers& callers) {
                callers.set(callerIndex);
            }
        );
    }
}
#endif

TriState CalleeGroup::calleeIsReferenced(const AbstractLocker&, Wasm::Callee* callee) const
{
    switch (callee->compilationMode()) {
    case CompilationMode::LLIntMode:
    case CompilationMode::IPIntMode:
        return TriState::True;
#if ENABLE(WEBASSEMBLY_BBQJIT)
    case CompilationMode::BBQMode: {
        FunctionCodeIndex index = toCodeIndex(callee->index());
        if (m_bbqCallees.at(index).isWeak())
            return m_bbqCallees.at(index).get() ? TriState::Indeterminate : TriState::False;
        return triState(m_bbqCallees.at(index).ptr());
    }
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
    case CompilationMode::OMGMode:
        return triState(m_omgCallees.at(toCodeIndex(callee->index())).get());
    case CompilationMode::OMGForOSREntryMode: {
        FunctionCodeIndex index = toCodeIndex(callee->index());
        if (m_osrEntryCallees.get(index).get()) {
            // The BBQCallee really owns the OMGOSREntryCallee so as long as that's around the OMGOSREntryCallee is referenced.
            if (m_bbqCallees.at(index).get())
                return TriState::True;
            return TriState::Indeterminate;
        }
        return TriState::False;
    }
#endif
    // FIXME: This doesn't record the index its associated with so we can't validate anything here.
    case CompilationMode::JSToWasmEntrypointMode:
    // FIXME: These are owned by JS, it's not clear how to verify they're still alive here.
    case CompilationMode::JSToWasmICMode:
    case CompilationMode::WasmToJSMode:
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

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
