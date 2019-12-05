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
#include "WasmEntryPlan.h"
#include "WasmModuleInformation.h"
#include "WasmTierUpCount.h"
#include <wtf/Bag.h>
#include <wtf/Function.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace JSC {

class CallLinkInfo;

namespace Wasm {

class BBQCallee;
class CodeBlock;
class EmbedderEntrypointCallee;

class BBQPlan final : public EntryPlan {
public:
    using Base = EntryPlan;

    using Base::Base;

    BBQPlan(Context*, Ref<ModuleInformation>, uint32_t functionIndex, CodeBlock*, CompletionTask&&);

    bool hasWork() const override
    {
        if (m_asyncWork == AsyncWork::Validation)
            return m_state < State::Validated;
        return m_state < State::Compiled;
    }

    void work(CompilationEffort) override;

    using CalleeInitializer = Function<void(uint32_t, RefPtr<EmbedderEntrypointCallee>&&, Ref<BBQCallee>&&)>;
    void initializeCallees(const CalleeInitializer&);

    bool didReceiveFunctionData(unsigned, const FunctionData&) override;

    bool parseAndValidateModule()
    {
        return Base::parseAndValidateModule(m_source.data(), m_source.size());
    }

protected:
    bool prepareImpl() override;
    void compileFunction(uint32_t functionIndex) override;
    void didCompleteCompilation(const AbstractLocker&) override;

private:
    std::unique_ptr<InternalFunction> compileFunction(uint32_t functionIndex, CompilationContext&, Vector<UnlinkedWasmToWasmCall>&, TierUpCount*);

    Vector<std::unique_ptr<InternalFunction>> m_wasmInternalFunctions;
    HashMap<uint32_t, std::unique_ptr<InternalFunction>, typename DefaultHash<uint32_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_embedderToWasmInternalFunctions;
    Vector<CompilationContext> m_compilationContexts;
    Vector<std::unique_ptr<TierUpCount>> m_tierUpCounts;

    RefPtr<CodeBlock> m_codeBlock { nullptr };
    uint32_t m_functionIndex;
};


} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
