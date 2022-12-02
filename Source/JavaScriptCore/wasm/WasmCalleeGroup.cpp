/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "WasmLLIntPlan.h"
#include "WasmWorklist.h"

namespace JSC { namespace Wasm {

Ref<CalleeGroup> CalleeGroup::create(VM& vm, MemoryMode mode, ModuleInformation& moduleInformation, RefPtr<LLIntCallees> llintCallees)
{
    return adoptRef(*new CalleeGroup(vm, mode, moduleInformation, llintCallees));
}

Ref<CalleeGroup> CalleeGroup::createFromExisting(MemoryMode mode, const CalleeGroup& other)
{
    return adoptRef(*new CalleeGroup(mode, other));
}

CalleeGroup::CalleeGroup(MemoryMode mode, const CalleeGroup& other)
    : m_calleeCount(other.m_calleeCount)
    , m_mode(mode)
    , m_llintCallees(other.m_llintCallees)
    , m_embedderCallees(other.m_embedderCallees)
    , m_wasmIndirectCallEntryPoints(other.m_wasmIndirectCallEntryPoints)
    , m_wasmToWasmCallsites(other.m_wasmToWasmCallsites)
    , m_wasmToWasmExitStubs(other.m_wasmToWasmExitStubs)
{
    setCompilationFinished();
}

CalleeGroup::CalleeGroup(VM& vm, MemoryMode mode, ModuleInformation& moduleInformation, RefPtr<LLIntCallees> llintCallees)
    : m_calleeCount(moduleInformation.internalFunctionCount())
    , m_mode(mode)
    , m_llintCallees(llintCallees)
{
    RefPtr<CalleeGroup> protectedThis = this;

    if (Options::useWasmLLInt()) {
        m_plan = adoptRef(*new LLIntPlan(vm, moduleInformation, m_llintCallees->data(), createSharedTask<Plan::CallbackType>([this, protectedThis = WTFMove(protectedThis)] (Plan&) {
            Locker locker { m_lock };
            if (m_plan->failed()) {
                m_errorMessage = m_plan->errorMessage();
                setCompilationFinished();
                return;
            }

            m_wasmIndirectCallEntryPoints = FixedVector<CodePtr<WasmEntryPtrTag>>(m_calleeCount);

            for (unsigned i = 0; i < m_calleeCount; ++i)
                m_wasmIndirectCallEntryPoints[i] = m_llintCallees->at(i)->entrypoint();

            m_wasmToWasmExitStubs = m_plan->takeWasmToWasmExitStubs();
            m_wasmToWasmCallsites = m_plan->takeWasmToWasmCallsites();
            m_embedderCallees = static_cast<LLIntPlan*>(m_plan.get())->takeEmbedderCallees();

            setCompilationFinished();
        })));
    }
#if ENABLE(WEBASSEMBLY_B3JIT)
    else {
        m_plan = adoptRef(*new BBQPlan(vm, moduleInformation, CompilerMode::FullCompile, createSharedTask<Plan::CallbackType>([this, protectedThis = WTFMove(protectedThis)] (Plan&) {
            Locker locker { m_lock };
            if (m_plan->failed()) {
                m_errorMessage = m_plan->errorMessage();
                setCompilationFinished();
                return;
            }

            m_wasmIndirectCallEntryPoints = FixedVector<CodePtr<WasmEntryPtrTag>>(m_calleeCount);

            BBQPlan* bbqPlan = static_cast<BBQPlan*>(m_plan.get());
            bbqPlan->initializeCallees([&] (unsigned calleeIndex, RefPtr<EmbedderEntrypointCallee>&& embedderEntrypointCallee, RefPtr<BBQCallee>&& wasmEntrypoint) {
                if (embedderEntrypointCallee) {
                    auto result = m_embedderCallees.set(calleeIndex, WTFMove(embedderEntrypointCallee));
                    ASSERT_UNUSED(result, result.isNewEntry);
                }
                m_wasmIndirectCallEntryPoints[calleeIndex] = wasmEntrypoint->entrypoint();
                setBBQCallee(locker, calleeIndex, adoptRef(*static_cast<BBQCallee*>(wasmEntrypoint.leakRef())));
            });

            m_wasmToWasmExitStubs = m_plan->takeWasmToWasmExitStubs();
            m_wasmToWasmCallsites = m_plan->takeWasmToWasmCallsites();

            setCompilationFinished();
        })));
    }
#endif
    m_plan->setMode(mode);

    auto& worklist = Wasm::ensureWorklist();
    // Note, immediately after we enqueue the plan, there is a chance the above callback will be called.
    worklist.enqueue(*m_plan.get());
}

CalleeGroup::~CalleeGroup() { }

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

    if (plan) {
        // We don't need to keep a RefPtr on the Plan because the worklist will keep
        // a RefPtr on the Plan until the plan finishes notifying all of its callbacks.
        RefPtr<CalleeGroup> protectedThis = this;
        plan->addCompletionTask(vm, createSharedTask<Plan::CallbackType>([this, task = WTFMove(task), protectedThis = WTFMove(protectedThis)] (Plan&) {
            task->run(Ref { *this });
        }));
    } else
        task->run(Ref { *this });
}

bool CalleeGroup::isSafeToRun(MemoryMode memoryMode)
{
    UNUSED_PARAM(memoryMode);

    if (!runnable())
        return false;

    switch (m_mode) {
    case MemoryMode::BoundsChecking:
        return true;
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    case MemoryMode::Signaling:
        // Code being in Signaling mode means that it performs no bounds checks.
        // Its memory, even if empty, absolutely must also be in Signaling mode
        // because the page protection detects out-of-bounds accesses.
        return memoryMode == MemoryMode::Signaling;
#endif
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
