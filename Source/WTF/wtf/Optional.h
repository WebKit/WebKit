/*
 *  Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <optional>

namespace WTF {

template<typename T> using Optional = std::optional<T>;
constexpr auto nullopt = std::nullopt;
using nullopt_t = std::nullopt_t;

template<typename T, typename... ArgumentTypes> constexpr std::optional<std::decay_t<T>> makeOptional(T&& value)
{
    return std::make_optional<std::decay_t<T>>(std::forward<T>(value));
}

template<typename T, typename... ArgumentTypes> constexpr std::optional<T> makeOptional(ArgumentTypes&&... arguments)
{
    return std::make_optional<T>(std::forward<ArgumentTypes>(arguments)...);
}

template<typename T, typename U, typename... ArgumentTypes> constexpr std::optional<T> makeOptional(std::initializer_list<U> list, ArgumentTypes&&... arguments)
{
    return std::make_optional<T>(list, std::forward<ArgumentTypes>(arguments)...);
}

// FIXME: We probably don't need this; it's only used in 5 places.
template<typename OptionalType, class Callback> auto valueOrCompute(OptionalType optional, Callback callback) -> typename OptionalType::value_type
{
    return optional ? *optional : callback();
}

} // namespace WTF

using WTF::Optional;
using WTF::makeOptional;
using WTF::valueOrCompute;

// FIXME: These macros are a workaround to allow us to use std::optional without a global replace; they can be removed after said global replace.
#define hasValue has_value
#define valueOr value_or
