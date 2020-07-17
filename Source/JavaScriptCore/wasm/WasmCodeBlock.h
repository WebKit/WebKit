/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "MacroAssemblerCodeRef.h"
#include "WasmCallee.h"
#include "WasmEmbedder.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace JSC {

namespace Wasm {

struct Context;
class EntryPlan;
struct ModuleInformation;
struct UnlinkedWasmToWasmCall;
enum class MemoryMode : uint8_t;
    
// FIXME: Rename this, since it's not a CodeBlock
// https://bugs.webkit.org/show_bug.cgi?id=203694
class CodeBlock : public ThreadSafeRefCounted<CodeBlock> {
public:
    typedef void CallbackType(Ref<CodeBlock>&&);
    using AsyncCompilationCallback = RefPtr<WTF::SharedTask<CallbackType>>;
    static Ref<CodeBlock> create(Context*, MemoryMode, ModuleInformation&, RefPtr<LLIntCallees>);

    void waitUntilFinished();
    void compileAsync(Context*, AsyncCompilationCallback&&);

    bool compilationFinished()
    {
        return m_compilationFinished.load();
    }
    bool runnable() { return compilationFinished() && !m_errorMessage; }

    // Note, we do this copy to ensure it's thread safe to have this
    // called from multiple threads simultaneously.
    String errorMessage()
    {
        ASSERT(!runnable());
        return crossThreadCopy(m_errorMessage);
    }

    unsigned functionImportCount() const { return m_wasmToWasmExitStubs.size(); }

    // These two callee getters are only valid once the callees have been populated.

    Callee& embedderEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        ASSERT(runnable());
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();

        auto callee = m_embedderCallees.get(calleeIndex);
        RELEASE_ASSERT(callee);
        return *callee;
    }
    Callee& wasmEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        ASSERT(runnable());
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        if (m_omgCallees[calleeIndex])
            return *m_omgCallees[calleeIndex].get();
        if (m_bbqCallees[calleeIndex])
            return *m_bbqCallees[calleeIndex].get();
        return m_llintCallees->at(calleeIndex).get();
    }

    BBQCallee& wasmBBQCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        ASSERT(runnable());
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return *m_bbqCallees[calleeIndex].get();
    }

    MacroAssemblerCodePtr<WasmEntryPtrTag>* entrypointLoadLocationFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return &m_wasmIndirectCallEntryPoints[calleeIndex];
    }

    MacroAssemblerCodePtr<WasmEntryPtrTag> wasmToWasmExitStub(unsigned functionIndex)
    {
        return m_wasmToWasmExitStubs[functionIndex].code();
    }

    bool isSafeToRun(MemoryMode);

    MemoryMode mode() const { return m_mode; }

    ~CodeBlock();
private:
    friend class Plan;
    friend class BBQPlan;
    friend class OMGPlan;
    friend class OMGForOSREntryPlan;

    CodeBlock(Context*, MemoryMode, ModuleInformation&, RefPtr<LLIntCallees>);
    void setCompilationFinished();
    unsigned m_calleeCount;
    MemoryMode m_mode;
    Vector<RefPtr<OMGCallee>> m_omgCallees;
    Vector<RefPtr<BBQCallee>> m_bbqCallees;
    RefPtr<LLIntCallees> m_llintCallees;
    HashMap<uint32_t, RefPtr<EmbedderEntrypointCallee>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_embedderCallees;
    Vector<MacroAssemblerCodePtr<WasmEntryPtrTag>> m_wasmIndirectCallEntryPoints;
    Vector<Vector<UnlinkedWasmToWasmCall>> m_wasmToWasmCallsites;
    Vector<MacroAssemblerCodeRef<WasmEntryPtrTag>> m_wasmToWasmExitStubs;
    RefPtr<EntryPlan> m_plan;
    std::atomic<bool> m_compilationFinished { false };
    String m_errorMessage;
    Lock m_lock;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
