/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <type_traits>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WTF {

// ButterflyArray offers the feature trailing and leading array in the derived class.
// We can allocate a memory like the following layout.
//
//     [ Leading Array ][  DerivedClass  ][ Trailing Array ]
template<typename Derived, typename LeadingType, typename TrailingType>
class ButterflyArray {
    WTF_MAKE_NONCOPYABLE(ButterflyArray);
    friend class JSC::LLIntOffsetsExtractor;
protected:
    explicit ButterflyArray(unsigned leadingSize, unsigned trailingSize)
        : m_leadingSize(leadingSize)
        , m_trailingSize(trailingSize)
    {
        static_assert(std::is_final_v<Derived>);
        auto leadingSpan = this->leadingSpan();
        VectorTypeOperations<LeadingType>::initializeIfNonPOD(leadingSpan.data(), leadingSpan.data() + leadingSpan.size());
        auto trailingSpan = this->trailingSpan();
        VectorTypeOperations<TrailingType>::initializeIfNonPOD(trailingSpan.data(), trailingSpan.data() + trailingSpan.size());
    }

    template<typename... Args>
    static Derived* createImpl(unsigned leadingSize, unsigned trailingSize, Args&&... args)
    {
        uint8_t* memory = bitwise_cast<uint8_t*>(fastMalloc(allocationSize(leadingSize, trailingSize)));
        return new (NotNull, memory + memoryOffsetForDerived(leadingSize)) Derived(leadingSize, trailingSize, std::forward<Args>(args)...);
    }

public:
    static constexpr size_t allocationSize(unsigned leadingSize, unsigned trailingSize)
    {
        return memoryOffsetForDerived(leadingSize) + offsetOfTrailingData() + trailingSize * sizeof(TrailingType);
    }

    static ptrdiff_t offsetOfLeadingSize() { return OBJECT_OFFSETOF(Derived, m_leadingSize); }
    static ptrdiff_t offsetOfTrailingSize() { return OBJECT_OFFSETOF(Derived, m_trailingSize); }
    static constexpr ptrdiff_t offsetOfTrailingData()
    {
        return WTF::roundUpToMultipleOf<alignof(TrailingType)>(sizeof(Derived));
    }

    static constexpr ptrdiff_t memoryOffsetForDerived(unsigned leadingSize)
    {
        return WTF::roundUpToMultipleOf<alignof(Derived)>(sizeof(LeadingType) * leadingSize);
    }

    std::span<LeadingType> leadingSpan()
    {
        return std::span { leadingData(), leadingData() + m_leadingSize };
    }

    std::span<const LeadingType> leadingSpan() const
    {
        return std::span { leadingData(), leadingData() + m_leadingSize };
    }

    std::span<TrailingType> trailingSpan()
    {
        return std::span { trailingData(), trailingData() + m_trailingSize };
    }

    std::span<const TrailingType> trailingSpan() const
    {
        return std::span { trailingData(), trailingData() + m_trailingSize };
    }

    void operator delete(ButterflyArray* base, std::destroying_delete_t)
    {
        unsigned leadingSize = base->m_leadingSize;
        std::destroy_at(static_cast<Derived*>(base));
        fastFree(bitwise_cast<uint8_t*>(static_cast<Derived*>(base)) - memoryOffsetForDerived(leadingSize));
    }

    ~ButterflyArray()
    {
        auto leadingSpan = this->leadingSpan();
        VectorTypeOperations<LeadingType>::destruct(leadingSpan.data(), leadingSpan.data() + leadingSpan.size());
        auto trailingSpan = this->trailingSpan();
        VectorTypeOperations<TrailingType>::destruct(trailingSpan.data(), trailingSpan.data() + trailingSpan.size());
    }

protected:
    LeadingType* leadingData()
    {
        return bitwise_cast<LeadingType*>(static_cast<Derived*>(this)) - m_leadingSize;
    }

    const LeadingType leadingData() const
    {
        return bitwise_cast<const LeadingType*>(static_cast<const Derived*>(this)) - m_leadingSize;
    }

    TrailingType* trailingData()
    {
        return bitwise_cast<TrailingType*>(bitwise_cast<uint8_t*>(static_cast<Derived*>(this)) + offsetOfTrailingData());
    }

    const TrailingType* trailingData() const
    {
        return bitwise_cast<const TrailingType*>(bitwise_cast<const uint8_t*>(static_cast<const Derived*>(this)) + offsetOfTrailingData());
    }

    unsigned m_leadingSize { 0 };
    unsigned m_trailingSize { 0 };
};

} // namespace WTF

using WTF::ButterflyArray;
