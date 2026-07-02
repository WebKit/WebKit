/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/WasmBaselineData.h>
#include <JavaScriptCore/WasmCallProfile.h>
#include <JavaScriptCore/WasmCallee.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace JSC::Wasm {

class Module;

class MergedProfile {
    WTF_MAKE_TZONE_ALLOCATED(MergedProfile);
    WTF_MAKE_NONMOVABLE(MergedProfile);
    friend class Module;
public:
    class Candidates {
    public:
        Candidates() = default;

        std::span<const std::tuple<Callee*, uint32_t>> callees() const LIFETIME_BOUND
        {
            return std::span { m_callees }.first(m_size);
        }

        bool isCalled() const { return !!m_totalCount; }
        bool isEmpty() const { return !m_size; }
        bool isMegamorphic() const { return m_isMegamorphic; }
        uint32_t totalCount() const { return m_totalCount; }

        void merge(IPIntCallee*, const CallProfile&);
        Candidates finalize() const;

    private:
        bool NODELETE add(Callee*, uint32_t);
        void markAsMegamorphic(uint32_t count);

        uint32_t m_totalCount { 0 };
        uint8_t m_size { 0 };
        bool m_isMegamorphic { false };
        std::array<std::tuple<Callee*, uint32_t>, CallProfile::maxPolymorphicCallees> m_callees { };
    };

    MergedProfile(const IPIntCallee&);
    unsigned size() const { return m_callSites.size(); }
    bool isCalled(size_t index) const { return m_callSites[index].isCalled(); }
    Candidates candidates(size_t index) const { return m_callSites[index].finalize(); }
    bool isMegamorphic(size_t index) const { return m_callSites[index].isMegamorphic(); }

    void merge(const Module&, const IPIntCallee&, BaselineData&);
    bool merged() const { return m_merged; }
    uint32_t totalCount() const { return m_totalCount; }

private:
    Vector<Candidates> m_callSites;
    uint32_t m_totalCount { 0 };
    bool m_merged { false };
};

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
