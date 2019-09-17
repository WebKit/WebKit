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
    Entrypoint& entrypoint() { return m_entrypoint; }
    MacroAssemblerCodePtr<WasmEntryPtrTag> code() const { return m_entrypoint.compilation->code().retagged<WasmEntryPtrTag>(); }
    IndexOrName indexOrName() const { return m_indexOrName; }
    CompilationMode compilationMode() const { return m_compilationMode; }

    RegisterAtOffsetList* calleeSaveRegisters() { return &m_entrypoint.calleeSaveRegisters; }

    std::tuple<void*, void*> range() const
    {
        void* start = m_entrypoint.compilation->codeRef().executableMemory()->start().untaggedPtr();
        void* end = m_entrypoint.compilation->codeRef().executableMemory()->end().untaggedPtr();
        return { start, end };
    }

    JS_EXPORT_PRIVATE virtual ~Callee();

protected:
    JS_EXPORT_PRIVATE Callee(CompilationMode);
    JS_EXPORT_PRIVATE Callee(CompilationMode, size_t, std::pair<const Name*, RefPtr<NameSection>>&&);

private:
    CompilationMode m_compilationMode;
    Entrypoint m_entrypoint;
    IndexOrName m_indexOrName;
};

class OMGCallee final : public Callee {
public:
    static Ref<OMGCallee> create(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new OMGCallee(index, WTFMove(name)));
    }

private:
    OMGCallee(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
        : Callee(CompilationMode::OMGMode, index, WTFMove(name))
    {
    }
};

class OMGForOSREntryCallee final : public Callee {
public:
    static Ref<OMGForOSREntryCallee> create(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, unsigned osrEntryScratchBufferSize, uint32_t loopIndex)
    {
        return adoptRef(*new OMGForOSREntryCallee(index, WTFMove(name), osrEntryScratchBufferSize, loopIndex));
    }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    uint32_t loopIndex() const { return m_loopIndex; }

private:
    OMGForOSREntryCallee(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, unsigned osrEntryScratchBufferSize, uint32_t loopIndex)
        : Callee(CompilationMode::OMGForOSREntryMode, index, WTFMove(name))
        , m_osrEntryScratchBufferSize(osrEntryScratchBufferSize)
        , m_loopIndex(loopIndex)
    {
    }

    unsigned m_osrEntryScratchBufferSize;
    uint32_t m_loopIndex;
};

class EmbedderEntrypointCallee final : public Callee {
public:
    static Ref<EmbedderEntrypointCallee> create()
    {
        return adoptRef(*new EmbedderEntrypointCallee());
    }

private:
    EmbedderEntrypointCallee()
        : Callee(CompilationMode::EmbedderEntrypointMode)
    {
    }
};

class BBQCallee final : public Callee {
public:
    static Ref<BBQCallee> create(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount)
    {
        return adoptRef(*new BBQCallee(index, WTFMove(name), WTFMove(tierUpCount)));
    }

    OMGForOSREntryCallee* osrEntryCallee() { return m_osrEntryCallee.get(); }
    void setOSREntryCallee(Ref<OMGForOSREntryCallee>&& osrEntryCallee)
    {
        m_osrEntryCallee = WTFMove(osrEntryCallee);
    }

    bool didStartCompilingOSREntryCallee() const { return m_didStartCompilingOSREntryCallee; }
    void setDidStartCompilingOSREntryCallee(bool value) { m_didStartCompilingOSREntryCallee = value; }

    OMGCallee* replacement() { return m_replacement.get(); }
    void setReplacement(const AbstractLocker&, Ref<OMGCallee>&& replacement)
    {
        m_replacement = WTFMove(replacement);
    }

    TierUpCount* tierUpCount() { return m_tierUpCount.get(); }

    void addCaller(const AbstractLocker&, LinkBuffer&, UnlinkedMoveAndCall);
    void addAndLinkCaller(const AbstractLocker&, LinkBuffer&, UnlinkedMoveAndCall);

    void repatchCallers(const AbstractLocker&, Callee&);

private:
    BBQCallee(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount)
        : Callee(CompilationMode::BBQMode, index, WTFMove(name))
        , m_tierUpCount(WTFMove(tierUpCount))
    {
    }

    Vector<CodeLocationDataLabelPtr<WasmEntryPtrTag>> m_moveLocations;
    Vector<CodeLocationNearCall<WasmEntryPtrTag>> m_callLocations;
    RefPtr<OMGForOSREntryCallee> m_osrEntryCallee;
    RefPtr<OMGCallee> m_replacement;
    std::unique_ptr<TierUpCount> m_tierUpCount;
    bool m_didStartCompilingOSREntryCallee { false };
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
