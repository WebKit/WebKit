/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "B3Compilation.h"
#include "RegisterAtOffsetList.h"
#include "WasmCompilationMode.h"
#include "WasmFormat.h"
#include "WasmFunctionCodeBlock.h"
#include "WasmIndexOrName.h"
#include "WasmTierUpCount.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class LLIntOffsetsExtractor;

namespace Wasm {

class OMGForOSREntryCallee;

class Callee : public ThreadSafeRefCounted<Callee> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    JS_EXPORT_PRIVATE virtual ~Callee();

    IndexOrName indexOrName() const { return m_indexOrName; }
    CompilationMode compilationMode() const { return m_compilationMode; }

    virtual MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint() const = 0;
    virtual RegisterAtOffsetList* calleeSaveRegisters() = 0;
    virtual std::tuple<void*, void*> range() const = 0;

    virtual void setOSREntryCallee(Ref<OMGForOSREntryCallee>&&)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    void dump(PrintStream&) const;

protected:
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode, size_t, std::pair<const Name*, RefPtr<NameSection>>&&);

private:
    CompilationMode m_compilationMode;
    IndexOrName m_indexOrName;
};

class JITCallee : public Callee {
public:
    MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint() const override { return m_entrypoint.compilation->code().retagged<WasmEntryPtrTag>(); }
    RegisterAtOffsetList* calleeSaveRegisters() override { return &m_entrypoint.calleeSaveRegisters; }
    Vector<UnlinkedWasmToWasmCall>& wasmToWasmCallsites() { return m_wasmToWasmCallsites; }

protected:
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode, Wasm::Entrypoint&&);
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode, Wasm::Entrypoint&&, size_t, std::pair<const Name*, RefPtr<NameSection>>&&, Vector<UnlinkedWasmToWasmCall>&&);

    std::tuple<void*, void*> range() const override
    {
        void* start = m_entrypoint.compilation->codeRef().executableMemory()->start().untaggedPtr();
        void* end = m_entrypoint.compilation->codeRef().executableMemory()->end().untaggedPtr();
        return { start, end };
    }

private:
    Vector<UnlinkedWasmToWasmCall> m_wasmToWasmCallsites;
    Wasm::Entrypoint m_entrypoint;
};

class OMGCallee final : public JITCallee {
public:
    static Ref<OMGCallee> create(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
    {
        return adoptRef(*new OMGCallee(WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls)));
    }

private:
    OMGCallee(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
        : JITCallee(Wasm::CompilationMode::OMGMode, WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls))
    {
    }
};

class OMGForOSREntryCallee final : public JITCallee {
public:
    static Ref<OMGForOSREntryCallee> create(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, unsigned osrEntryScratchBufferSize, uint32_t loopIndex, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
    {
        return adoptRef(*new OMGForOSREntryCallee(WTFMove(entrypoint), index, WTFMove(name), osrEntryScratchBufferSize, loopIndex, WTFMove(unlinkedCalls)));
    }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    uint32_t loopIndex() const { return m_loopIndex; }

private:
    OMGForOSREntryCallee(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, unsigned osrEntryScratchBufferSize, uint32_t loopIndex, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
        : JITCallee(Wasm::CompilationMode::OMGForOSREntryMode, WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls))
        , m_osrEntryScratchBufferSize(osrEntryScratchBufferSize)
        , m_loopIndex(loopIndex)
    {
    }

    unsigned m_osrEntryScratchBufferSize;
    uint32_t m_loopIndex;
};

class EmbedderEntrypointCallee final : public JITCallee {
public:
    static Ref<EmbedderEntrypointCallee> create(Wasm::Entrypoint&& entrypoint)
    {
        return adoptRef(*new EmbedderEntrypointCallee(WTFMove(entrypoint)));
    }

private:
    EmbedderEntrypointCallee(Wasm::Entrypoint&& entrypoint)
        : JITCallee(Wasm::CompilationMode::EmbedderEntrypointMode, WTFMove(entrypoint))
    {
    }
};

class BBQCallee final : public JITCallee {
public:
    static Ref<BBQCallee> create(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
    {
        return adoptRef(*new BBQCallee(WTFMove(entrypoint), index, WTFMove(name), WTFMove(tierUpCount), WTFMove(unlinkedCalls)));
    }

    OMGForOSREntryCallee* osrEntryCallee() { return m_osrEntryCallee.get(); }
    void setOSREntryCallee(Ref<OMGForOSREntryCallee>&& osrEntryCallee) override
    {
        m_osrEntryCallee = WTFMove(osrEntryCallee);
    }

    bool didStartCompilingOSREntryCallee() const { return m_didStartCompilingOSREntryCallee; }
    void setDidStartCompilingOSREntryCallee(bool value) { m_didStartCompilingOSREntryCallee = value; }

    OMGCallee* replacement() { return m_replacement.get(); }
    void setReplacement(Ref<OMGCallee>&& replacement)
    {
        m_replacement = WTFMove(replacement);
    }

    TierUpCount* tierUpCount() { return m_tierUpCount.get(); }

private:
    BBQCallee(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
        : JITCallee(Wasm::CompilationMode::BBQMode, WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls))
        , m_tierUpCount(WTFMove(tierUpCount))
    {
    }

    RefPtr<OMGForOSREntryCallee> m_osrEntryCallee;
    RefPtr<OMGCallee> m_replacement;
    std::unique_ptr<TierUpCount> m_tierUpCount;
    bool m_didStartCompilingOSREntryCallee { false };
};

class LLIntCallee final : public Callee {
    friend LLIntOffsetsExtractor;

public:
    static Ref<LLIntCallee> create(std::unique_ptr<FunctionCodeBlock> codeBlock, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new LLIntCallee(WTFMove(codeBlock), index, WTFMove(name)));
    }

    JS_EXPORT_PRIVATE void setEntrypoint(MacroAssemblerCodePtr<WasmEntryPtrTag>);
    JS_EXPORT_PRIVATE MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint() const override;
    JS_EXPORT_PRIVATE RegisterAtOffsetList* calleeSaveRegisters() override;
    JS_EXPORT_PRIVATE std::tuple<void*, void*> range() const override;

    JITCallee* replacement() { return m_replacement.get(); }
    void setReplacement(Ref<JITCallee>&& replacement)
    {
        m_replacement = WTFMove(replacement);
    }

    OMGForOSREntryCallee* osrEntryCallee() { return m_osrEntryCallee.get(); }
    void setOSREntryCallee(Ref<OMGForOSREntryCallee>&& osrEntryCallee) override
    {
        m_osrEntryCallee = WTFMove(osrEntryCallee);
    }

    LLIntTierUpCounter& tierUpCounter() { return m_codeBlock->tierUpCounter(); }

private:
    LLIntCallee(std::unique_ptr<FunctionCodeBlock> codeBlock, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
        : Callee(Wasm::CompilationMode::LLIntMode, index, WTFMove(name))
        , m_codeBlock(WTFMove(codeBlock))
    {
        RELEASE_ASSERT(m_codeBlock);
    }

    RefPtr<JITCallee> m_replacement;
    RefPtr<OMGForOSREntryCallee> m_osrEntryCallee;
    std::unique_ptr<FunctionCodeBlock> m_codeBlock;
    MacroAssemblerCodePtr<WasmEntryPtrTag> m_entrypoint;
};

class LLIntCallees : public ThreadSafeRefCounted<LLIntCallees> {
public:
    static Ref<LLIntCallees> create(Vector<Ref<LLIntCallee>>&& llintCallees)
    {
        return adoptRef(*new LLIntCallees(WTFMove(llintCallees)));
    }

    const Ref<LLIntCallee>& at(unsigned i) const
    {
        return m_llintCallees.at(i);
    }

    const Ref<LLIntCallee>* data() const
    {
        return m_llintCallees.data();
    }

private:
    LLIntCallees(Vector<Ref<LLIntCallee>>&& llintCallees)
        : m_llintCallees(WTFMove(llintCallees))
    {
    }

    Vector<Ref<LLIntCallee>> m_llintCallees;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
