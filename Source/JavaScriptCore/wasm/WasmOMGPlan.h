/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY_OMGJIT)

#include "WasmContext.h"
#include "WasmModule.h"
#include "WasmPlan.h"
#include "tools/FunctionAllowlist.h"

namespace JSC {

class CallLinkInfo;

namespace Wasm {

class OMGPlan final : public Plan {
public:
    using Base = Plan;

    bool hasWork() const final { return !m_completed; }
    void work() final;
    bool multiThreaded() const final { return false; }
    static FunctionAllowlist& ensureGlobalOMGAllowlist();

    // Note: CompletionTask should not hold a reference to the Plan otherwise there will be a reference cycle.
    OMGPlan(VM&, Ref<Module>&&, FunctionCodeIndex functionIndex, MemoryMode, CompletionTask&&);

private:
    // For some reason friendship doesn't extend to parent classes...
    using Base::m_lock;

    void dumpDisassembly(CompilationContext&, LinkBuffer&, const TypeDefinition&, FunctionSpaceIndex functionIndexSpace);
    bool isComplete() const final { return m_completed; }
    void complete() WTF_REQUIRES_LOCK(m_lock) final
    {
        m_completed = true;
        runCompletionTasks();
    }
    
    const Ref<Module> m_module;
    const Ref<CalleeGroup> m_calleeGroup;
    bool m_completed { false };
    FunctionCodeIndex m_functionIndex;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
