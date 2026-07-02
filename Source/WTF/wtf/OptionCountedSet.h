/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WTF {

template<typename E> class OptionCountedSet {
public:
    constexpr bool isEmpty() const { return m_optionSet.isEmpty(); }

    constexpr bool contains(E option) const { return m_optionSet.contains(option); }

    constexpr void add(E option)
    {
        auto i = index(option);
        const auto currentSize = m_counts.size();
        const auto newSize = i + 1;
        if (currentSize < newSize)
            m_counts.insertFill(currentSize, 0, newSize - currentSize);

        m_counts[i] += 1;
        if (m_counts[i] == 1)
            m_optionSet.add(option);
    }

    constexpr void remove(E option)
    {
        if (!m_optionSet.contains(option))
            return;

        auto i = index(option);
        ASSERT(i < m_counts.size());
        ASSERT(m_counts[i] > 0);
        m_counts[i] -= 1;
        if (!m_counts[i])
            m_optionSet.remove(option);
    }

    constexpr void add(OptionSet<E> optionSet)
    {
        for (auto option : optionSet)
            add(option);
    }

    constexpr void remove(OptionSet<E> optionSet)
    {
        for (auto option : optionSet)
            remove(option);
    }

private:
    static constexpr size_t index(E option)
    {
        auto value = static_cast<std::underlying_type_t<E>>(option);
        return value ? std::countr_zero(value) : 0;
    }

    OptionSet<E> m_optionSet;
    Vector<unsigned, 8> m_counts;
};

} // namespace WTF

using WTF::OptionCountedSet;
