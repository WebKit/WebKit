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

#include "WasmEntryPlan.h"
#include "WasmFunctionCodeBlockGenerator.h"

namespace JSC {

class CallLinkInfo;

namespace Wasm {

class LLIntCallee;
class JSEntrypointCallee;
class StreamingCompiler;

using JSEntrypointCalleeMap = HashMap<uint32_t, RefPtr<JSEntrypointCallee>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

using TailCallGraph = HashMap<uint32_t, HashSet<uint32_t, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

class LLIntPlan final : public EntryPlan {
    using Base = EntryPlan;

public:
    JS_EXPORT_PRIVATE LLIntPlan(VM&, Vector<uint8_t>&&, CompilerMode, CompletionTask&&);
    LLIntPlan(VM&, Ref<ModuleInformation>, const Ref<LLIntCallee>*, CompletionTask&&);
    LLIntPlan(VM&, Ref<ModuleInformation>, CompilerMode, CompletionTask&&); // For StreamingCompiler.

    MacroAssemblerCodeRef<JITCompilationPtrTag>&& takeEntryThunks()
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return WTFMove(m_entryThunks);
    }

    Vector<Ref<LLIntCallee>>&& takeCallees()
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return WTFMove(m_calleesVector);
    }

    JSEntrypointCalleeMap&& takeJSCallees()
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return WTFMove(m_jsEntrypointCallees);
    }

    bool hasWork() const final
    {
        return m_state < State::Compiled;
    }

    void work(CompilationEffort) final;

    bool didReceiveFunctionData(unsigned, const FunctionData&) final;

    void compileFunction(uint32_t functionIndex) final;

    void completeInStreaming();
    void didCompileFunctionInStreaming();
    void didFailInStreaming(String&&);

private:
    bool prepareImpl() final;
    void didCompleteCompilation() WTF_REQUIRES_LOCK(m_lock) final;

    void addTailCallEdge(uint32_t, uint32_t);
    void computeTransitiveTailCalls() const;

    Vector<std::unique_ptr<FunctionCodeBlockGenerator>> m_wasmInternalFunctions;
    const Ref<LLIntCallee>* m_callees { nullptr };
    Vector<Ref<LLIntCallee>> m_calleesVector;
    JSEntrypointCalleeMap m_jsEntrypointCallees;
    TailCallGraph m_tailCallGraph;
    MacroAssemblerCodeRef<JITCompilationPtrTag> m_entryThunks;
};


} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
