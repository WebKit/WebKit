/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <array>

namespace WTF {

// This is an std::array where the indices of the array are values of an enum (rather than a size_t).
// This assumes the values of the enum start at 0 and monotonically increase by 1
// (so the conversion function between size_t and the enum is just a simple static_cast).
// LastValue is the maximum value of the enum, which determines the size of the array.
template <typename Key, typename T, Key LastValue>
class EnumeratedArray {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using value_type = T;
    using size_type = Key;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using UnderlyingType = std::array<T, static_cast<std::size_t>(LastValue) + 1>;
    using iterator = typename UnderlyingType::iterator;
    using const_iterator = typename UnderlyingType::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    EnumeratedArray() = default;

    EnumeratedArray(const EnumeratedArray& from)
        : m_storage(from.m_storage)
    {
    }

    EnumeratedArray(EnumeratedArray&& from)
        : m_storage(WTFMove(from.m_storage))
    {
    }

    EnumeratedArray(const UnderlyingType& from)
        : m_storage(from)
    {
    }

    EnumeratedArray(UnderlyingType&& from)
        : m_storage(WTFMove(from))
    {
    }

    EnumeratedArray& operator=(const EnumeratedArray& from)
    {
        m_storage = from.m_storage;
        return *this;
    }

    EnumeratedArray& operator=(EnumeratedArray&& from)
    {
        m_storage = WTFMove(from.m_storage);
        return *this;
    }

    constexpr reference at(size_type pos)
    {
        return m_storage.at(index(pos));
    }

    constexpr const_reference at(size_type pos) const
    {
        return m_storage.at(index(pos));
    }

    constexpr reference operator[](size_type pos)
    {
        return m_storage[index(pos)];
    }

    constexpr const_reference operator[](size_type pos) const
    {
        return m_storage[index(pos)];
    }

    constexpr reference front()
    {
        return m_storage.front();
    }

    constexpr const_reference front() const
    {
        return m_storage.front();
    }

    constexpr reference back()
    {
        return m_storage.back();
    }

    constexpr const_reference back() const
    {
        return m_storage.back();
    }

    constexpr T* data() noexcept
    {
        return m_storage.data();
    }

    constexpr const T* data() const noexcept
    {
        return m_storage.data();
    }

    constexpr iterator begin() noexcept
    {
        return m_storage.begin();
    }

    constexpr const_iterator begin() const noexcept
    {
        return m_storage.begin();
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return m_storage.cbegin();
    }

    constexpr iterator end() noexcept
    {
        return m_storage.end();
    }

    constexpr const_iterator end() const noexcept
    {
        return m_storage.end();
    }

    constexpr const_iterator cend() const noexcept
    {
        return m_storage.cend();
    }

    constexpr reverse_iterator rbegin() noexcept
    {
        return m_storage.rbegin();
    }

    constexpr const_reverse_iterator rbegin() const noexcept
    {
        return m_storage.rbegin();
    }

    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return m_storage.crbegin();
    }

    constexpr reverse_iterator rend() noexcept
    {
        return m_storage.rend();
    }

    constexpr const_reverse_iterator rend() const noexcept
    {
        return m_storage.rend();
    }

    constexpr const_reverse_iterator crend() const noexcept
    {
        return m_storage.crend();
    }

    constexpr bool empty() const noexcept
    {
        return m_storage.empty();
    }

    constexpr typename UnderlyingType::size_type size() const noexcept
    {
        return m_storage.size();
    }

    constexpr typename UnderlyingType::size_type max_size() const noexcept
    {
        return m_storage.max_size();
    }

    constexpr void fill(const T& value)
    {
        m_storage.fill(value);
    }

    constexpr void swap(EnumeratedArray& other) noexcept
    {
        return m_storage.swap(other.m_storage);
    }

    template <typename Key2, typename T2, Key2 LastValue2>
    constexpr bool operator==(const EnumeratedArray<Key2, T2, LastValue2>& rhs) const
    {
        return m_storage == rhs.m_storage;
    }

    template <typename Key2, typename T2, Key2 LastValue2>
    bool operator!=(const EnumeratedArray<Key2, T2, LastValue2>& rhs) const
    {
        return m_storage != rhs.m_storage;
    }

    template <typename Key2, typename T2, Key2 LastValue2>
    bool operator<(const EnumeratedArray<Key2, T2, LastValue2>& rhs) const
    {
        return m_storage < rhs.m_storage;
    }

    template <typename Key2, typename T2, Key2 LastValue2>
    bool operator<=(const EnumeratedArray<Key2, T2, LastValue2>& rhs) const
    {
        return m_storage <= rhs.m_storage;
    }

    template <typename Key2, typename T2, Key2 LastValue2>
    bool operator>(const EnumeratedArray<Key2, T2, LastValue2>& rhs) const
    {
        return m_storage > rhs.m_storage;
    }

    template <typename Key2, typename T2, Key2 LastValue2>
    bool operator>=(const EnumeratedArray<Key2, T2, LastValue2>& rhs) const
    {
        return m_storage >= rhs.m_storage;
    }

private:
    typename UnderlyingType::size_type index(size_type pos) const
    {
        return static_cast<typename UnderlyingType::size_type>(pos);
    }

    UnderlyingType m_storage;
};

} // namespace WTF

using WTF::EnumeratedArray;
