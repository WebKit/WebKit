/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmCallee.h"
#include "WasmContext.h"
#include "WasmModule.h"
#include "WasmOperations.h"
#include "WasmPlan.h"

namespace JSC {

class CallLinkInfo;

namespace Wasm {

class OMGForOSREntryPlan final : public Plan {
public:
    using Base = Plan;

    bool hasWork() const override { return !m_completed; }
    void work(CompilationEffort) override;
    bool multiThreaded() const override { return false; }

    // Note: CompletionTask should not hold a reference to the Plan otherwise there will be a reference cycle.
    OMGForOSREntryPlan(Context*, Ref<Module>&&, Ref<Callee>&&, uint32_t functionIndex, uint32_t loopIndex, MemoryMode, CompletionTask&&);

private:
    // For some reason friendship doesn't extend to parent classes...
    using Base::m_lock;

    bool isComplete() const override { return m_completed; }
    void complete(const AbstractLocker& locker) override
    {
        m_completed = true;
        runCompletionTasks(locker);
    }

    Ref<Module> m_module;
    Ref<CodeBlock> m_codeBlock;
    Ref<Callee> m_callee;
    bool m_completed { false };
    uint32_t m_functionIndex;
    uint32_t m_loopIndex;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
