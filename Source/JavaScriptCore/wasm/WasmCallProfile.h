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

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/WasmCallingConvention.h>
#include <wtf/Expected.h>
#include <wtf/TrailingArray.h>
#include <wtf/text/WTFString.h>

namespace JSC::Wasm {

class CallProfile {
public:
    CallProfile() = default;
    ~CallProfile();

    uint32_t count() const { return m_count; }

    void incrementCount()
    {
        ++m_count;
    }

    void observeCrossInstanceCall()
    {
        makeMegamorphic();
    }

    void observeCallIndirect(EncodedJSValue boxedCallee)
    {
        if (m_boxedCallee == boxedCallee)
            return;

        if (!m_boxedCallee) {
            m_boxedCallee = boxedCallee;
            return;
        }

        if (isMegamorphic(m_boxedCallee))
            return;

        auto* poly = polymorphic(m_boxedCallee);
        if (!poly)
            poly = makePolymorphic();

        for (auto& profile : *poly) {
            if (profile.boxedCallee() == boxedCallee) {
                profile.incrementCount();
                return;
            }
            if (!profile.boxedCallee()) {
                profile.m_boxedCallee = boxedCallee;
                profile.incrementCount();
                return;
            }
        }
        makeMegamorphic();
    }

    EncodedJSValue boxedCallee() const { return m_boxedCallee; }

    static constexpr ptrdiff_t offsetOfCount() { return OBJECT_OFFSETOF(CallProfile, m_count); }
    static constexpr ptrdiff_t offsetOfBoxedCallee() { return OBJECT_OFFSETOF(CallProfile, m_boxedCallee); }

    enum State : EncodedJSValue {
        Monomorphic = 0b0000,
        Polymorphic = 0b0100,
        Megamorphic = 0b1000,
    };
    static constexpr EncodedJSValue calleeMask = Polymorphic | Megamorphic;
#if USE(JSVALUE64)
    static_assert(!(JSValue::NativeCalleeTag & calleeMask));
#endif

    static constexpr size_t maxPolymorphicCallees = 4;

    class alignas(16) PolymorphicCallee final : public TrailingArray<PolymorphicCallee, CallProfile> {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PolymorphicCallee);
        WTF_MAKE_NONMOVABLE(PolymorphicCallee);
        using TrailingArrayType = TrailingArray<PolymorphicCallee, CallProfile>;
        friend TrailingArrayType;
    public:

        static std::unique_ptr<PolymorphicCallee> create(unsigned size, CallProfile* profile)
        {
            return std::unique_ptr<PolymorphicCallee>(new (fastMalloc(allocationSize(size))) PolymorphicCallee(size, profile));
        }

        static constexpr ptrdiff_t offsetOfProfile() { return OBJECT_OFFSETOF(PolymorphicCallee, m_profile); }

    private:
        PolymorphicCallee(unsigned size, CallProfile* profile)
            : TrailingArrayType(size)
            , m_profile(profile)
        {
        }

        CallProfile* m_profile { nullptr };
    };

    static bool isMegamorphic(EncodedJSValue boxedCallee)
    {
        return boxedCallee & Megamorphic;
    }

    static Callee* monomorphic(EncodedJSValue boxedCallee)
    {
        if (boxedCallee & calleeMask)
            return nullptr;
        uintptr_t bits = static_cast<uintptr_t>(boxedCallee & ~calleeMask);
        if (!bits)
            return nullptr;
        return std::bit_cast<Callee*>(CalleeBits(bits).asNativeCallee());
    }

    static PolymorphicCallee* polymorphic(EncodedJSValue boxedCallee)
    {
        if (boxedCallee & Megamorphic)
            return nullptr;
        if (boxedCallee & Polymorphic)
            return std::bit_cast<PolymorphicCallee*>(static_cast<uintptr_t>(boxedCallee & ~calleeMask));
        return nullptr;
    }

private:
    void makeMegamorphic()
    {
        m_boxedCallee = (m_boxedCallee | Megamorphic);
    }

    PolymorphicCallee* makePolymorphic();

    uint32_t m_count { 0 };
#if USE(JSVALUE64)
    EncodedJSValue m_boxedCallee { Monomorphic };
#else
    EncodedJSValue m_boxedCallee { Megamorphic };
#endif
};

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
