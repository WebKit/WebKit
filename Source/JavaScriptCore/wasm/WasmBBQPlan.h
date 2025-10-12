/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY_BBQJIT)

#include "CompilationResult.h"
#include "WasmCallee.h"
#include "WasmEntryPlan.h"
#include "WasmModuleInformation.h"
#include "tools/FunctionAllowlist.h"
#include <wtf/Bag.h>
#include <wtf/Function.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace JSC {

class CallLinkInfo;

namespace Wasm {

class BBQCallee;
class IPIntCallee;
class CalleeGroup;
class JSToWasmCallee;

class BBQPlan final : public Plan {
public:
    using Base = Plan;

    static Ref<BBQPlan> create(VM& vm, Ref<ModuleInformation>&& info, FunctionCodeIndex functionIndex, Ref<IPIntCallee>&& profiledCallee, Ref<Module>&& module, Ref<CalleeGroup>&& calleeGroup, CompletionTask&& completionTask)
    {
        return adoptRef(*new BBQPlan(vm, WTFMove(info), functionIndex, WTFMove(profiledCallee), WTFMove(module), WTFMove(calleeGroup), WTFMove(completionTask)));
    }

    bool hasWork() const final { return !m_completed; }
    void work() final;
    bool multiThreaded() const final { return false; }

    static FunctionAllowlist& ensureGlobalBBQAllowlist();


private:
    BBQPlan(VM&, Ref<ModuleInformation>&&, FunctionCodeIndex functionIndex, Ref<IPIntCallee>&&, Ref<Module>&&, Ref<CalleeGroup>&&, CompletionTask&&);

    bool dumpDisassembly(CompilationContext&, LinkBuffer&, const TypeDefinition&, FunctionSpaceIndex functionIndexSpace);

    std::unique_ptr<InternalFunction> compileFunction(FunctionCodeIndex functionIndex, BBQCallee&, CompilationContext&, Vector<UnlinkedWasmToWasmCall>&);
    bool isComplete() const final { return m_completed; }
    void complete() WTF_REQUIRES_LOCK(m_lock) final
    {
        m_completed = true;
        runCompletionTasks();
    }

    void fail(String&& errorMessage, CompilationError);

    const Ref<IPIntCallee> m_profiledCallee;
    const Ref<Module> m_module;
    const Ref<CalleeGroup> m_calleeGroup;
    FunctionCodeIndex m_functionIndex;
    bool m_completed { false };
};


} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_BBQJIT)
