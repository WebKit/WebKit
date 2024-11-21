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

#include "StylePrimitiveNumericTypes.h"
#include <wtf/Brigand.h>

namespace WebCore {
namespace CSS {

namespace TypeTransform {

// MARK: - Type transforms

namespace Type {

// MARK: CSS type -> Style type mapping (Style type -> CSS type directly available via typename StyleType::CSS)

template<typename> struct ToStyleMapping;
template<auto R> struct ToStyleMapping<Number<R>> { using Style = Style::Number<R>; };
template<auto R> struct ToStyleMapping<Percentage<R>> { using Style = Style::Percentage<R>; };
template<auto R> struct ToStyleMapping<Angle<R>> { using Style = Style::Angle<R>; };
template<auto R> struct ToStyleMapping<Length<R>> { using Style = Style::Length<R>; };
template<auto R> struct ToStyleMapping<Time<R>> { using Style = Style::Time<R>; };
template<auto R> struct ToStyleMapping<Frequency<R>> { using Style = Style::Frequency<R>; };
template<auto R> struct ToStyleMapping<Resolution<R>> { using Style = Style::Resolution<R>; };
template<auto R> struct ToStyleMapping<Flex<R>> { using Style = Style::Flex<R>; };
template<auto R> struct ToStyleMapping<AnglePercentage<R>> { using Style = Style::AnglePercentage<R>; };
template<auto R> struct ToStyleMapping<LengthPercentage<R>> { using Style = Style::LengthPercentage<R>; };
template<> struct ToStyleMapping<None> { using Style = Style::None; };

// MARK: Transform CSS type -> Style type.

// Transform `css1`  -> `style1`
template<typename T> struct CSSToStyleLazy {
    using type = typename ToStyleMapping<T>::Style;
};
template<typename T> using CSSToStyle = typename CSSToStyleLazy<T>::type;

// MARK: Transform Raw type -> Style type.

// Transform `raw1`  -> `style1`
template<typename T> struct RawToStyleLazy {
    using type = typename ToStyleMapping<typename RawToCSSMapping<T>::CSS>::Style;
};
template<typename T> using RawToStyle = typename RawToStyleLazy<T>::type;

} // namespace Type

namespace List {

// MARK: - List transforms

// MARK: Transform CSS type list -> Style type list.

// Transform `brigand::list<css1, css2, ...>`  -> `brigand::list<style1, style2, ...>`
template<typename TypeList> struct CSSToStyleLazy {
    using type = brigand::transform<TypeList, Type::CSSToStyleLazy<brigand::_1>>;
};
template<typename TypeList> using CSSToStyle = typename CSSToStyleLazy<TypeList>::type;

// MARK: Transform Raw type list -> Style type list.

// Transform `brigand::list<raw1, raw2, ...>`  -> `brigand::list<style1, style2, ...>`
template<typename TypeList> struct RawToStyleLazy {
    using type = brigand::transform<TypeList, Type::RawToStyleLazy<brigand::_1>>;
};
template<typename TypeList> using RawToStyle = typename RawToStyleLazy<TypeList>::type;

} // namespace List

} // namespace TypeTransform

} // namespace CSS
} // namespace WebCore
