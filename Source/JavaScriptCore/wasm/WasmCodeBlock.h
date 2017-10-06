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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "MacroAssemblerCodeRef.h"
#include "WasmTierUpCount.h"
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class VM;

namespace Wasm {

class Callee;
class BBQPlan;
class OMGPlan;
struct ModuleInformation;
struct UnlinkedWasmToWasmCall;
typedef void** WasmEntrypointLoadLocation;
enum class MemoryMode : uint8_t;
    
class CodeBlock : public ThreadSafeRefCounted<CodeBlock> {
public:
    typedef void CallbackType(VM&, Ref<CodeBlock>&&);
    using AsyncCompilationCallback = RefPtr<WTF::SharedTask<CallbackType>>;
    static Ref<CodeBlock> create(MemoryMode mode, ModuleInformation& moduleInformation)
    {
        return adoptRef(*new CodeBlock(mode, moduleInformation));
    }

    void waitUntilFinished();
    void compileAsync(VM&, AsyncCompilationCallback&&);

    bool compilationFinished()
    {
        auto locker = holdLock(m_lock);
        return !m_plan;
    }
    bool runnable() { return compilationFinished() && !m_errorMessage; }

    // Note, we do this copy to ensure it's thread safe to have this
    // called from multiple threads simultaneously.
    String errorMessage()
    {
        ASSERT(!runnable());
        CString cString = m_errorMessage.ascii();
        return String(cString.data());
    }

    unsigned functionImportCount() const { return m_wasmToWasmExitStubs.size(); }

    Callee& jsEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();

        auto callee = m_jsCallees.get(calleeIndex);
        RELEASE_ASSERT(callee);
        return *callee;
    }
    Callee& wasmEntrypointCalleeFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        if (m_optimizedCallees[calleeIndex])
            return *m_optimizedCallees[calleeIndex].get();
        return *m_callees[calleeIndex].get();
    }

    WasmEntrypointLoadLocation wasmEntrypointLoadLocationFromFunctionIndexSpace(unsigned functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return &m_wasmIndirectCallEntryPoints[calleeIndex];
    }

    TierUpCount& tierUpCount(uint32_t functionIndex)
    {
        return m_tierUpCounts[functionIndex];
    }

    bool isSafeToRun(MemoryMode);

    MemoryMode mode() const { return m_mode; }

    ~CodeBlock();
private:
    friend class OMGPlan;

    CodeBlock(MemoryMode, ModuleInformation&);
    unsigned m_calleeCount;
    MemoryMode m_mode;
    Vector<RefPtr<Callee>> m_callees;
    Vector<RefPtr<Callee>> m_optimizedCallees;
    HashMap<uint32_t, RefPtr<Callee>, typename DefaultHash<uint32_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_jsCallees;
    Vector<void*> m_wasmIndirectCallEntryPoints;
    Vector<TierUpCount> m_tierUpCounts;
    Vector<Vector<UnlinkedWasmToWasmCall>> m_wasmToWasmCallsites;
    Vector<MacroAssemblerCodeRef> m_wasmToWasmExitStubs;
    RefPtr<BBQPlan> m_plan;
    String m_errorMessage;
    Lock m_lock;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
