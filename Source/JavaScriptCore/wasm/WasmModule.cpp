/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WasmModule.h"

#if ENABLE(WEBASSEMBLY)

#include "JSWebAssemblyInstance.h"
#include "WasmDebugServer.h"
#include "WasmIPIntPlan.h"
#include "WasmInstanceAnchor.h"
#include "WasmMergedProfile.h"
#include "WasmModuleInformation.h"
#include "WasmWorklist.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Wasm {

Module::Module(IPIntPlan& plan)
    : m_moduleInformation(plan.takeModuleInformation())
    , m_ipintCallees(IPIntCallees::createFromVector(plan.takeCallees()))
    , m_wasmToJSExitStubs(plan.takeWasmToJSExitStubs())
{
#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]]
        Wasm::DebugServer::singleton().trackModule(*this);
#endif
}

Module::~Module()
{
#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]]
        Wasm::DebugServer::singleton().untrackModule(*this);
#endif
}

Wasm::TypeIndex Module::typeIndexFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace) const
{
    return m_moduleInformation->typeIndexFromFunctionIndexSpace(functionIndexSpace);
}

static Module::ValidationResult makeValidationResult(IPIntPlan& plan)
{
    ASSERT(!plan.hasWork());
    if (plan.failed())
        return Unexpected<String>(plan.errorMessage());
    return Module::ValidationResult(Module::create(plan));
}

static Plan::CompletionTask makeValidationCallback(Module::AsyncValidationCallback&& callback)
{
    return createSharedTask<Plan::CallbackType>([callback = WTF::move(callback)] (Plan& plan) {
        ASSERT(!plan.hasWork());
        callback->run(makeValidationResult(static_cast<IPIntPlan&>(plan)));
    });
}

Module::ValidationResult Module::validateSync(VM& vm, Vector<uint8_t>&& source)
{
    Ref<IPIntPlan> plan = adoptRef(*new IPIntPlan(vm, WTF::move(source), CompilerMode::Validation, Plan::dontFinalize()));
    Wasm::ensureWorklist().enqueue(plan.get());
    plan->waitForCompletion();
    return makeValidationResult(plan.get());
}

void Module::validateAsync(VM& vm, Vector<uint8_t>&& source, Module::AsyncValidationCallback&& callback)
{
    Ref<Plan> plan = adoptRef(*new IPIntPlan(vm, WTF::move(source), CompilerMode::Validation, makeValidationCallback(WTF::move(callback))));
    Wasm::ensureWorklist().enqueue(WTF::move(plan));
}

Ref<CalleeGroup> Module::getOrCreateCalleeGroup(VM& vm, MemoryMode mode)
{
    RefPtr<CalleeGroup> calleeGroup;
    Locker locker { m_lock };
    calleeGroup = m_calleeGroups[static_cast<uint8_t>(mode)];
    // If a previous attempt at a compile errored out, let's try again.
    // Compilations from valid modules can fail because OOM and cancellation.
    // It's worth retrying.
    // FIXME: We might want to back off retrying at some point:
    // https://bugs.webkit.org/show_bug.cgi?id=170607
    if (!calleeGroup || (calleeGroup->compilationFinished() && !calleeGroup->runnable())) {
        m_calleeGroups[static_cast<uint8_t>(mode)] = calleeGroup = CalleeGroup::createFromIPInt(vm, mode, const_cast<ModuleInformation&>(moduleInformation()), m_ipintCallees.copyRef());
    }
    return calleeGroup.releaseNonNull();
}

void Module::applyCompileOptions(const WebAssemblyCompileOptions& options)
{
    m_moduleInformation->applyCompileOptions(options);
}

Ref<CalleeGroup> Module::compileSync(VM& vm, MemoryMode mode)
{
    Ref<CalleeGroup> calleeGroup = getOrCreateCalleeGroup(vm, mode);
    calleeGroup->waitUntilFinished();
    return calleeGroup;
}

void Module::compileAsync(VM& vm, MemoryMode mode, CalleeGroup::AsyncCompilationCallback&& task)
{
    Ref<CalleeGroup> calleeGroup = getOrCreateCalleeGroup(vm, mode);
    calleeGroup->compileAsync(vm, WTF::move(task));
}

void Module::copyInitialCalleeGroupToAllMemoryModes(MemoryMode initialMode)
{
    Locker locker { m_lock };
    ASSERT(m_calleeGroups[static_cast<uint8_t>(initialMode)]);
    const CalleeGroup& initialBlock = *m_calleeGroups[static_cast<uint8_t>(initialMode)];
    for (unsigned i = 0; i < numberOfMemoryModes; i++) {
        if (i == static_cast<uint8_t>(initialMode))
            continue;
        // We should only try to copy the group here if it hasn't already been created.
        // If it exists but is not runnable, it should get compiled during module evaluation.
        if (auto& group = m_calleeGroups[i]; !group)
            group = CalleeGroup::createFromExisting(static_cast<MemoryMode>(i), initialBlock);
    }
}


Ref<Wasm::InstanceAnchor> Module::registerAnchor(JSWebAssemblyInstance* instance)
{
    auto anchor = Wasm::InstanceAnchor::create(*this, instance);
    WTF::storeStoreFence();
    m_anchors.add(anchor.get());
    return anchor;
}

std::unique_ptr<MergedProfile> Module::createMergedProfile(const IPIntCallee& callee)
{
    auto result = makeUnique<MergedProfile>(callee);
    for (Ref anchor : m_anchors) {
        RefPtr<BaselineData> data;
        {
            Locker locker { anchor->m_lock };
            if (JSWebAssemblyInstance* instance = anchor->instance())
                data = instance->baselineData(callee.functionIndex());
        }
        if (!data)
            continue;
        result->merge(*this, callee, *data);
    }
    return result;
}

#if ENABLE(WEBASSEMBLY_DEBUGGER)
uint32_t Module::debugId() const { return m_moduleInformation->debugInfo->id; }
void Module::setDebugId(uint32_t id) { m_moduleInformation->debugInfo->id = id; }
#endif

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
