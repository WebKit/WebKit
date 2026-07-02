/*
 *  Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <ranges>

namespace WTF {

// This our WTF version of c++23's std::from_range and std::from_range_t
// The std version is not exposed on every compiler yet, so we roll our own for now
// Replace with std::from_range and std::from_range_t if/when it is implemented
// in the major compilers
// see: https://en.cppreference.com/w/cpp/ranges/from_range.html
struct FromRange {
    explicit FromRange() = default;
};

// Global instance for use as tag
inline constexpr FromRange fromRange { };

// This is our WTF version of c++23's std::ranges::range_adaptor closure
// We should remove this and switch to the std version once it is implemented
// in the major compilers
// see: https://en.cppreference.com/w/cpp/ranges/range_adaptor_closure.html
template<typename Derived>
struct RangeAdaptorClosure {
    template<std::ranges::input_range Range>
    friend auto operator|(Range&& range, const Derived& adaptor)
    {
        return adaptor(std::forward<Range>(range));
    }

    template<typename OtherClosure>
    friend auto operator|(const Derived& left, const OtherClosure& right)
    {
        return [=](auto&& range) {
            return right(left(std::forward<decltype(range)>(range)));
        };
    }
};

template<typename Container>
struct FromRangeConverter : RangeAdaptorClosure<FromRangeConverter<Container>> {
    template<std::ranges::input_range Range>
        requires requires(Range&& range) { Container(fromRange, std::forward<Range>(range)); }
    auto operator()(Range&& range) const
    {
        return Container(fromRange, std::forward<Range>(range));
    }
};

template<typename Container>
constexpr inline FromRangeConverter<Container> rangeTo()
{
    return FromRangeConverter<Container> { };
};

}

using WTF::fromRange;
using WTF::FromRange;
using WTF::rangeTo;
