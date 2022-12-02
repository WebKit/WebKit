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

#include "WasmLLIntPlan.h"
#include "WasmModuleInformation.h"
#include "WasmWorklist.h"

namespace JSC { namespace Wasm {

Module::Module(LLIntPlan& plan)
    : m_moduleInformation(plan.takeModuleInformation())
    , m_llintCallees(LLIntCallees::createFromVector(plan.takeCallees()))
    , m_llintEntryThunks(plan.takeEntryThunks())
{
}

Module::~Module() { }

Wasm::TypeIndex Module::typeIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const
{
    return m_moduleInformation->typeIndexFromFunctionIndexSpace(functionIndexSpace);
}

static Module::ValidationResult makeValidationResult(LLIntPlan& plan)
{
    ASSERT(!plan.hasWork());
    if (plan.failed())
        return Unexpected<String>(plan.errorMessage());
    return Module::ValidationResult(Module::create(plan));
}

static Plan::CompletionTask makeValidationCallback(Module::AsyncValidationCallback&& callback)
{
    return createSharedTask<Plan::CallbackType>([callback = WTFMove(callback)] (Plan& plan) {
        ASSERT(!plan.hasWork());
        callback->run(makeValidationResult(static_cast<LLIntPlan&>(plan)));
    });
}

Module::ValidationResult Module::validateSync(VM& vm, Vector<uint8_t>&& source)
{
    Ref<LLIntPlan> plan = adoptRef(*new LLIntPlan(vm, WTFMove(source), CompilerMode::Validation, Plan::dontFinalize()));
    Wasm::ensureWorklist().enqueue(plan.get());
    plan->waitForCompletion();
    return makeValidationResult(plan.get());
}

void Module::validateAsync(VM& vm, Vector<uint8_t>&& source, Module::AsyncValidationCallback&& callback)
{
    Ref<Plan> plan = adoptRef(*new LLIntPlan(vm, WTFMove(source), CompilerMode::Validation, makeValidationCallback(WTFMove(callback))));
    Wasm::ensureWorklist().enqueue(WTFMove(plan));
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
        RefPtr<LLIntCallees> llintCallees = nullptr;
        if (Options::useWasmLLInt())
            llintCallees = m_llintCallees.copyRef();
        calleeGroup = CalleeGroup::create(vm, mode, const_cast<ModuleInformation&>(moduleInformation()), WTFMove(llintCallees));
        m_calleeGroups[static_cast<uint8_t>(mode)] = calleeGroup;
    }
    return calleeGroup.releaseNonNull();
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
    calleeGroup->compileAsync(vm, WTFMove(task));
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

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
