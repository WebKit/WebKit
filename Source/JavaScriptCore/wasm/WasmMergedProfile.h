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
#include <wtf/text/WTFString.h>

namespace JSC::Wasm {

class Module;

class MergedProfile {
    WTF_MAKE_TZONE_ALLOCATED(MergedProfile);
    WTF_MAKE_NONMOVABLE(MergedProfile);
    friend class Module;
public:
    class CallSite {
    public:
        void merge(const CallProfile&);
        uint32_t count() const { return m_count; }

        Callee* callee() const
        {
            if (m_callee == megamorphic)
                return nullptr;
            return std::bit_cast<Callee*>(m_callee);
        }

        bool isMegamorphic() const { return m_callee == megamorphic; }

    private:
        static constexpr uintptr_t megamorphic = 1;

        uint32_t m_count { 0 };
        uintptr_t m_callee { 0 };
    };

    MergedProfile(const IPIntCallee&);
    bool isCalled(size_t index) const { return !!m_callSites[index].count(); }
    Callee* callee(size_t index) const { return m_callSites[index].callee(); }
    bool isMegamorphic(size_t index) const { return m_callSites[index].isMegamorphic(); }

    std::span<CallSite> mutableSpan() { return m_callSites.mutableSpan(); }
    std::span<const CallSite> span() const { return m_callSites.span(); }

private:
    Vector<CallSite> m_callSites;
};

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
