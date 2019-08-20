/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "CompilationResult.h"
#include "WasmB3IRGenerator.h"
#include "WasmModuleInformation.h"
#include "WasmPlan.h"
#include "WasmTierUpCount.h"
#include <wtf/Bag.h>
#include <wtf/Function.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace JSC {

class CallLinkInfo;

namespace Wasm {

class BBQPlan final : public Plan {
public:
    using Base = Plan;
    enum AsyncWork : uint8_t { FullCompile, Validation };

    // Note: CompletionTask should not hold a reference to the Plan otherwise there will be a reference cycle.
    BBQPlan(Context*, Ref<ModuleInformation>, AsyncWork, CompletionTask&&, CreateEmbedderWrapper&&, ThrowWasmException);
    JS_EXPORT_PRIVATE BBQPlan(Context*, Vector<uint8_t>&&, AsyncWork, CompletionTask&&, CreateEmbedderWrapper&&, ThrowWasmException);
    BBQPlan(Context*, AsyncWork, CompletionTask&&);


    bool parseAndValidateModule()
    {
        return parseAndValidateModule(m_source.data(), m_source.size());
    }
    bool parseAndValidateModule(const uint8_t*, size_t);

    JS_EXPORT_PRIVATE void prepare();
    void compileFunctions(CompilationEffort);

    template<typename Functor>
    void initializeCallees(const Functor&);

    Vector<Export>& exports() const
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return m_moduleInformation->exports;
    }

    size_t internalFunctionCount() const
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return m_moduleInformation->internalFunctionCount();
    }

    Ref<ModuleInformation>&& takeModuleInformation()
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return WTFMove(m_moduleInformation);
    }

    Bag<CallLinkInfo>&& takeCallLinkInfos()
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return WTFMove(m_callLinkInfos);
    }

    Vector<MacroAssemblerCodeRef<WasmEntryPtrTag>>&& takeWasmToWasmExitStubs()
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return WTFMove(m_wasmToWasmExitStubs);
    }

    Vector<Vector<UnlinkedWasmToWasmCall>> takeWasmToWasmCallsites()
    {
        RELEASE_ASSERT(!failed() && !hasWork());
        return WTFMove(m_unlinkedWasmToWasmCalls);
    }

    enum class State : uint8_t {
        Initial,
        Validated,
        Prepared,
        Compiled,
        Completed // We should only move to Completed if we are holding the lock.
    };

    bool hasWork() const override
    {
        if (m_asyncWork == AsyncWork::Validation)
            return m_state < State::Validated;
        return m_state < State::Compiled;
    }
    void work(CompilationEffort) override;
    bool hasBeenPrepared() const { return m_state >= State::Prepared; }
    bool multiThreaded() const override { return hasBeenPrepared(); }

private:
    class ThreadCountHolder;
    friend class ThreadCountHolder;
    // For some reason friendship doesn't extend to parent classes...
    using Base::m_lock;

    void moveToState(State);
    bool isComplete() const override { return m_state == State::Completed; }
    void complete(const AbstractLocker&) override;

    const char* stateString(State);
    
    Vector<uint8_t> m_source;
    Bag<CallLinkInfo> m_callLinkInfos;
    Vector<MacroAssemblerCodeRef<WasmEntryPtrTag>> m_wasmToWasmExitStubs;
    Vector<std::unique_ptr<InternalFunction>> m_wasmInternalFunctions;
    HashSet<uint32_t, typename DefaultHash<uint32_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_exportedFunctionIndices;
    HashMap<uint32_t, std::unique_ptr<InternalFunction>, typename DefaultHash<uint32_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_embedderToWasmInternalFunctions;
    Vector<CompilationContext> m_compilationContexts;
    Vector<std::unique_ptr<TierUpCount>> m_tierUpCounts;

    Vector<Vector<UnlinkedWasmToWasmCall>> m_unlinkedWasmToWasmCalls;
    State m_state;

    const AsyncWork m_asyncWork;
    uint8_t m_numberOfActiveThreads { 0 };
    uint32_t m_currentIndex { 0 };
};


} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
