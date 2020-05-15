/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "WasmPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmCallee.h"
#include "WasmCodeBlock.h"
#include "WasmMachineThreads.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

namespace WasmPlanInternal {
static constexpr bool verbose = false;
}

Plan::Plan(Context* context, Ref<ModuleInformation> info, CompletionTask&& task)
    : m_moduleInformation(WTFMove(info))
{
    m_completionTasks.append(std::make_pair(context, WTFMove(task)));
}
Plan::Plan(Context* context, CompletionTask&& task)
    : m_moduleInformation(ModuleInformation::create())
{
    m_completionTasks.append(std::make_pair(context, WTFMove(task)));
}

void Plan::runCompletionTasks(const AbstractLocker&)
{
    ASSERT(isComplete() && !hasWork());

    for (auto& task : m_completionTasks)
        task.second->run(*this);
    m_completionTasks.clear();
    m_completed.notifyAll();
}

void Plan::addCompletionTask(Context* context, CompletionTask&& task)
{
    LockHolder locker(m_lock);
    if (!isComplete())
        m_completionTasks.append(std::make_pair(context, WTFMove(task)));
    else
        task->run(*this);
}

void Plan::waitForCompletion()
{
    LockHolder locker(m_lock);
    if (!isComplete()) {
        m_completed.wait(m_lock);
    }
}

bool Plan::tryRemoveContextAndCancelIfLast(Context& context)
{
    LockHolder locker(m_lock);

    if (ASSERT_ENABLED) {
        // We allow the first completion task to not have a Context.
        for (unsigned i = 1; i < m_completionTasks.size(); ++i)
            ASSERT(m_completionTasks[i].first);
    }

    bool removedAnyTasks = false;
    m_completionTasks.removeAllMatching([&] (const std::pair<Context*, CompletionTask>& pair) {
        bool shouldRemove = pair.first == &context;
        removedAnyTasks |= shouldRemove;
        return shouldRemove;
    });

    if (!removedAnyTasks)
        return false;

    if (isComplete()) {
        // We trivially cancel anything that's completed.
        return true;
    }

    // FIXME: Make 0 index not so magical: https://bugs.webkit.org/show_bug.cgi?id=171395
    if (m_completionTasks.isEmpty() || (m_completionTasks.size() == 1 && !m_completionTasks[0].first)) {
        fail(locker, "WebAssembly Plan was cancelled. If you see this error message please file a bug at bugs.webkit.org!"_s);
        return true;
    }

    return false;
}

void Plan::fail(const AbstractLocker& locker, String&& errorMessage)
{
    if (failed())
        return;
    ASSERT(errorMessage);
    dataLogLnIf(WasmPlanInternal::verbose, "failing with message: ", errorMessage);
    m_errorMessage = WTFMove(errorMessage);
    complete(locker);
}

void Plan::updateCallSitesToCallUs(CodeBlock& codeBlock, CodeLocationLabel<WasmEntryPtrTag> entrypoint, uint32_t functionIndex, uint32_t functionIndexSpace)
{
    HashMap<void*, CodeLocationLabel<WasmEntryPtrTag>> stagedCalls;
    auto stageRepatch = [&] (const Vector<UnlinkedWasmToWasmCall>& callsites) {
        for (auto& call : callsites) {
            if (call.functionIndexSpace == functionIndexSpace) {
                CodeLocationLabel<WasmEntryPtrTag> target = MacroAssembler::prepareForAtomicRepatchNearCallConcurrently(call.callLocation, entrypoint);
                stagedCalls.add(call.callLocation.dataLocation(), target);
            }
        }
    };
    for (unsigned i = 0; i < codeBlock.m_wasmToWasmCallsites.size(); ++i) {
        stageRepatch(codeBlock.m_wasmToWasmCallsites[i]);
        if (codeBlock.m_llintCallees) {
            LLIntCallee& llintCallee = codeBlock.m_llintCallees->at(i).get();
            if (JITCallee* replacementCallee = llintCallee.replacement())
                stageRepatch(replacementCallee->wasmToWasmCallsites());
            if (OMGForOSREntryCallee* osrEntryCallee = llintCallee.osrEntryCallee())
                stageRepatch(osrEntryCallee->wasmToWasmCallsites());
        }
        if (BBQCallee* bbqCallee = codeBlock.m_bbqCallees[i].get()) {
            if (OMGCallee* replacementCallee = bbqCallee->replacement())
                stageRepatch(replacementCallee->wasmToWasmCallsites());
            if (OMGForOSREntryCallee* osrEntryCallee = bbqCallee->osrEntryCallee())
                stageRepatch(osrEntryCallee->wasmToWasmCallsites());
        }
    }

    // It's important to make sure we do this before we make any of the code we just compiled visible. If we didn't, we could end up
    // where we are tiering up some function A to A' and we repatch some function B to call A' instead of A. Another CPU could see
    // the updates to B but still not have reset its cache of A', which would lead to all kinds of badness.
    resetInstructionCacheOnAllThreads();
    WTF::storeStoreFence(); // This probably isn't necessary but it's good to be paranoid.

    codeBlock.m_wasmIndirectCallEntryPoints[functionIndex] = entrypoint;

    auto repatchCalls = [&] (const Vector<UnlinkedWasmToWasmCall>& callsites) {
        for (auto& call : callsites) {
            dataLogLnIf(WasmPlanInternal::verbose, "Considering repatching call at: ", RawPointer(call.callLocation.dataLocation()), " that targets ", call.functionIndexSpace);
            if (call.functionIndexSpace == functionIndexSpace) {
                dataLogLnIf(WasmPlanInternal::verbose, "Repatching call at: ", RawPointer(call.callLocation.dataLocation()), " to ", RawPointer(entrypoint.executableAddress()));
                MacroAssembler::repatchNearCall(call.callLocation, stagedCalls.get(call.callLocation.dataLocation()));
            }
        }
    };

    for (unsigned i = 0; i < codeBlock.m_wasmToWasmCallsites.size(); ++i) {
        repatchCalls(codeBlock.m_wasmToWasmCallsites[i]);
        if (codeBlock.m_llintCallees) {
            LLIntCallee& llintCallee = codeBlock.m_llintCallees->at(i).get();
            if (JITCallee* replacementCallee = llintCallee.replacement())
                repatchCalls(replacementCallee->wasmToWasmCallsites());
            if (OMGForOSREntryCallee* osrEntryCallee = llintCallee.osrEntryCallee())
                repatchCalls(osrEntryCallee->wasmToWasmCallsites());
        }
        if (BBQCallee* bbqCallee = codeBlock.m_bbqCallees[i].get()) {
            if (OMGCallee* replacementCallee = bbqCallee->replacement())
                repatchCalls(replacementCallee->wasmToWasmCallsites());
            if (OMGForOSREntryCallee* osrEntryCallee = bbqCallee->osrEntryCallee())
                repatchCalls(osrEntryCallee->wasmToWasmCallsites());
        }
    }

}

Plan::~Plan() { }

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
