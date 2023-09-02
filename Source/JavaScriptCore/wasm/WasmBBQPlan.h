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

#if ENABLE(WEBASSEMBLY_BBQJIT)

#include "CompilationResult.h"
#include "WasmEntryPlan.h"
#include "WasmModuleInformation.h"
#include "WasmTierUpCount.h"
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
class CalleeGroup;
class JSEntrypointCallee;

class BBQPlan final : public EntryPlan {
public:
    using Base = EntryPlan;

    using Base::Base;

    BBQPlan(VM&, Ref<ModuleInformation>, uint32_t functionIndex, std::optional<bool> hasExceptionHandlers, CalleeGroup*, CompletionTask&&);

    bool hasWork() const final
    {
        if (m_compilerMode == CompilerMode::Validation)
            return m_state < State::Validated;
        return m_state < State::Compiled;
    }

    void work(CompilationEffort) final;

    using CalleeInitializer = Function<void(uint32_t, RefPtr<JSEntrypointCallee>&&, Ref<BBQCallee>&&)>;
    void initializeCallees(const CalleeInitializer&);

    bool didReceiveFunctionData(unsigned, const FunctionData&) final;

    bool parseAndValidateModule()
    {
        return Base::parseAndValidateModule(m_source.data(), m_source.size());
    }

    static FunctionAllowlist& ensureGlobalBBQAllowlist();

private:
    bool prepareImpl() final;
    bool dumpDisassembly(CompilationContext&, LinkBuffer&, unsigned functionIndex, const TypeDefinition&, unsigned functionIndexSpace);
    void compileFunction(uint32_t functionIndex) final;
    void didCompleteCompilation() WTF_REQUIRES_LOCK(m_lock) final;

    std::unique_ptr<InternalFunction> compileFunction(uint32_t functionIndex, BBQCallee&, CompilationContext&, Vector<UnlinkedWasmToWasmCall>&, TierUpCount*);

    Vector<std::unique_ptr<InternalFunction>> m_wasmInternalFunctions;
    Vector<std::unique_ptr<LinkBuffer>> m_wasmInternalFunctionLinkBuffers;
    Vector<Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>> m_exceptionHandlerLocations;
    HashMap<uint32_t, std::tuple<RefPtr<JSEntrypointCallee>, std::unique_ptr<LinkBuffer>, std::unique_ptr<InternalFunction>>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_jsToWasmInternalFunctions;
    Vector<CompilationContext> m_compilationContexts;
    Vector<RefPtr<BBQCallee>> m_callees;
    Vector<Vector<CodeLocationLabel<WasmEntryPtrTag>>> m_allLoopEntrypoints;

    RefPtr<CalleeGroup> m_calleeGroup { nullptr };
    uint32_t m_functionIndex;
    std::optional<bool> m_hasExceptionHandlers;
};


} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_BBQJIT)
