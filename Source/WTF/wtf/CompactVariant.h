/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CompactVariantOperations.h>

namespace WTF {

// A `CompactVariant` acts like a `std::variant` with the following differences:
// - All alternatives must be pointers, smart pointers, have size of 56 bits or fewer, or be specialized for `CompactVariantTraits`.
// - Can only contain 254 or fewer alternatives.
// - Has a more limited API, only offering `holds_alternative()` for type checking and `switchOn()` for value access.

template<CompactVariantAlternative... Ts> class CompactVariant {
    static_assert(sizeof...(Ts) < 255);
    using StdVariant = std::variant<Ts...>;
    using Index = uint8_t;
    using Storage = uint64_t;
    using Operations = CompactVariantOperations<Ts...>;
public:
    template<typename U> constexpr CompactVariant(U&& value)
        requires
            std::constructible_from<StdVariant, U>
         && (!std::same_as<std::remove_cvref_t<U>, CompactVariant>)
         && (!IsStdInPlaceTypeV<std::remove_cvref_t<U>>)
         && (!IsStdInPlaceIndexV<std::remove_cvref_t<U>>)
    {
        m_data = Operations::template encode<typename VariantBestMatch<StdVariant, U>::type>(std::forward<U>(value));
    }

    template<typename T, typename... Args> explicit constexpr CompactVariant(std::in_place_type_t<T>, Args&&... args)
        requires std::constructible_from<StdVariant, T>
    {
        m_data = Operations::template encodeFromArguments<T>(std::forward<Args>(args)...);
    }

    template<size_t I, typename... Args> explicit constexpr CompactVariant(std::in_place_index_t<I>, Args&&... args)
        requires std::constructible_from<StdVariant, std::variant_alternative_t<I, StdVariant>>
    {
        m_data = Operations::template encodeFromArguments<std::variant_alternative_t<I, StdVariant>>(std::forward<Args>(args)...);
    }

    CompactVariant(const CompactVariant& other)
    {
        Operations::copy(m_data, other.m_data);
    }

    CompactVariant& operator=(const CompactVariant& other)
    {
        if (m_data == other.m_data)
            return *this;

        Operations::destruct(m_data);
        Operations::copy(m_data, other.m_data);

        return *this;
    }

    CompactVariant(CompactVariant&& other)
    {
        Operations::move(m_data, other.m_data);

        // Set `other` to the "moved from" state.
        other.m_data = Operations::movedFromDataValue;
    }

    CompactVariant& operator=(CompactVariant&& other)
    {
        if (m_data == other.m_data)
            return *this;

        Operations::destruct(m_data);
        Operations::move(m_data, other.m_data);

        // Set `other` to the "moved from" state.
        other.m_data = Operations::movedFromDataValue;

        return *this;
    }

    template<typename U> CompactVariant& operator=(U&& value)
        requires
            std::constructible_from<StdVariant, U>
         && (!std::same_as<std::remove_cvref_t<U>, CompactVariant>)
         && (!IsStdInPlaceTypeV<std::remove_cvref_t<U>>)
         && (!IsStdInPlaceIndexV<std::remove_cvref_t<U>>)
    {
        Operations::destruct(m_data);
        m_data = Operations::template encode<typename VariantBestMatch<StdVariant, U>::type>(std::forward<U>(value));

        return *this;
    }

    ~CompactVariant()
    {
        Operations::destruct(m_data);
    }

    void swap(CompactVariant& other)
    {
        std::swap(m_data, other.m_data);
    }

    template<typename T, typename... Args> void emplace(Args&&... args)
    {
        Operations::destruct(m_data);
        m_data = Operations::template encodeFromArguments<T>(std::forward<Args>(args)...);
    }

    template<size_t I, typename... Args> void emplace(Args&&... args)
    {
        Operations::destruct(m_data);
        m_data = Operations::template encodeFromArguments<std::variant_alternative_t<I, StdVariant>>(std::forward<Args>(args)...);
    }

    constexpr Index index() const
    {
        return Operations::decodedIndex(m_data);
    }

    constexpr bool valueless_by_move() const
    {
        return m_data == Operations::movedFromDataValue;
    }

    template<typename T> constexpr bool holdsAlternative() const
    {
        static_assert(alternativeIndexV<T, StdVariant> <= std::variant_size_v<StdVariant>);
        return index() == alternativeIndexV<T, StdVariant>;
    }

    template<size_t I> constexpr bool holdsAlternative() const
    {
        static_assert(I <= std::variant_size_v<StdVariant>);
        return index() == I;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return Operations::constPayloadForData(m_data, std::forward<F>(f)...);
    }

    bool operator==(const CompactVariant& other) const
    {
        if (index() != other.index())
            return false;

        return typeForIndex<StdVariant>(index(), [&]<typename T>() {
            return Operations::template equal<T>(m_data, other.m_data);
        });
    }

private:
    // FIXME: Use a smaller data type if values are small enough / empty.
    Storage m_data;
};

// Utility for making a CompactVariant directly from a parameter pack of types.
template<typename... Ts> using CompactVariantWrapper = CompactVariant<Ts...>;

} // namespace WTF

using WTF::CompactVariant;
using WTF::CompactVariantWrapper;
