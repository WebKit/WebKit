/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <wtf/Vector.h>

namespace WTF {

// A set of integers in the range [0, size) optimized for repeated clear() operations.
// clear() is amortized O(1) by using a generation counter instead of clearing storage.

template<typename T> using DefaultGenerationalSetVector = Vector<T>;

template<typename GenerationType, template<typename> typename VectorType = DefaultGenerationalSetVector>
    requires std::is_unsigned_v<GenerationType>
class GenerationalSet {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    GenerationalSet() = default;

    explicit GenerationalSet(size_t size)
        : m_map(size, 0)
    {
    }

    void resize(size_t newSize)
    {
        size_t oldSize = m_map.size();
        m_map.resize(newSize);
        for (size_t i = oldSize; i < newSize; ++i)
            m_map[i] = GenerationType { 0 };
    }

    size_t size() const
    {
        return m_map.size();
    }

    void clear()
    {
        ++m_generation;
        if (!m_generation) [[unlikely]] {
            m_map.fill(0);
            m_generation = 1;
        }
    }

    bool contains(size_t index) const
    {
        return m_map[index] == m_generation;
    }

    void add(size_t index)
    {
        m_map[index] = m_generation;
    }

private:
    VectorType<GenerationType> m_map;
    GenerationType m_generation { 1 };
};

} // namespace WTF

using WTF::GenerationalSet;
