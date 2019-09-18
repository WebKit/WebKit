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
#include "WasmIndexOrName.h"
#include "WasmTierUpCount.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace Wasm {

class Callee : public ThreadSafeRefCounted<Callee> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MacroAssemblerCodePtr<WasmEntryPtrTag> entrypoint() const { return m_entrypoint.compilation->code().retagged<WasmEntryPtrTag>(); }

    RegisterAtOffsetList* calleeSaveRegisters() { return &m_entrypoint.calleeSaveRegisters; }
    IndexOrName indexOrName() const { return m_indexOrName; }
    CompilationMode compilationMode() const { return m_compilationMode; }

    std::tuple<void*, void*> range() const
    {
        void* start = m_entrypoint.compilation->codeRef().executableMemory()->start().untaggedPtr();
        void* end = m_entrypoint.compilation->codeRef().executableMemory()->end().untaggedPtr();
        return { start, end };
    }

    JS_EXPORT_PRIVATE virtual ~Callee();

protected:
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode, Wasm::Entrypoint&&);
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode, Wasm::Entrypoint&&, size_t, std::pair<const Name*, RefPtr<NameSection>>&&);

private:
    CompilationMode m_compilationMode;
    Wasm::Entrypoint m_entrypoint;
    IndexOrName m_indexOrName;
};

class OMGCallee final : public Callee {
public:
    static Ref<OMGCallee> create(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
    {
        return adoptRef(*new OMGCallee(WTFMove(entrypoint), index, WTFMove(name), WTFMove(unlinkedCalls)));
    }

    Vector<UnlinkedWasmToWasmCall>& wasmToWasmCallsites() { return m_wasmToWasmCallsites; }

private:
    OMGCallee(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
        : Callee(Wasm::CompilationMode::OMGMode, WTFMove(entrypoint), index, WTFMove(name))
        , m_wasmToWasmCallsites(WTFMove(unlinkedCalls))
    {
    }

    Vector<UnlinkedWasmToWasmCall> m_wasmToWasmCallsites;
};

class OMGForOSREntryCallee final : public Callee {
public:
    static Ref<OMGForOSREntryCallee> create(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, unsigned osrEntryScratchBufferSize, uint32_t loopIndex, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
    {
        return adoptRef(*new OMGForOSREntryCallee(WTFMove(entrypoint), index, WTFMove(name), osrEntryScratchBufferSize, loopIndex, WTFMove(unlinkedCalls)));
    }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    uint32_t loopIndex() const { return m_loopIndex; }
    Vector<UnlinkedWasmToWasmCall>& wasmToWasmCallsites() { return m_wasmToWasmCallsites; }

private:
    OMGForOSREntryCallee(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, unsigned osrEntryScratchBufferSize, uint32_t loopIndex, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls)
        : Callee(Wasm::CompilationMode::OMGForOSREntryMode, WTFMove(entrypoint), index, WTFMove(name))
        , m_wasmToWasmCallsites(WTFMove(unlinkedCalls))
        , m_osrEntryScratchBufferSize(osrEntryScratchBufferSize)
        , m_loopIndex(loopIndex)
    {
    }

    Vector<UnlinkedWasmToWasmCall> m_wasmToWasmCallsites;
    unsigned m_osrEntryScratchBufferSize;
    uint32_t m_loopIndex;
};

class EmbedderEntrypointCallee final : public Callee {
public:
    static Ref<EmbedderEntrypointCallee> create(Wasm::Entrypoint&& entrypoint)
    {
        return adoptRef(*new EmbedderEntrypointCallee(WTFMove(entrypoint)));
    }

private:
    EmbedderEntrypointCallee(Wasm::Entrypoint&& entrypoint)
        : Callee(Wasm::CompilationMode::EmbedderEntrypointMode, WTFMove(entrypoint))
    {
    }
};

class BBQCallee final : public Callee {
public:
    static Ref<BBQCallee> create(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount)
    {
        return adoptRef(*new BBQCallee(WTFMove(entrypoint), index, WTFMove(name), WTFMove(tierUpCount)));
    }

    OMGForOSREntryCallee* osrEntryCallee() { return m_osrEntryCallee.get(); }
    void setOSREntryCallee(Ref<OMGForOSREntryCallee>&& osrEntryCallee)
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
    BBQCallee(Wasm::Entrypoint&& entrypoint, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount)
        : Callee(Wasm::CompilationMode::BBQMode, WTFMove(entrypoint), index, WTFMove(name))
        , m_tierUpCount(WTFMove(tierUpCount))
    {
    }

    RefPtr<OMGForOSREntryCallee> m_osrEntryCallee;
    RefPtr<OMGCallee> m_replacement;
    std::unique_ptr<TierUpCount> m_tierUpCount;
    bool m_didStartCompilingOSREntryCallee { false };
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
