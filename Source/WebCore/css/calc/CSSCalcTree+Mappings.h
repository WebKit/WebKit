/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSCalcTree.h"
#include "StyleCalculationTree.h"

namespace WebCore {
namespace CSSCalc {

template<typename> struct ToCalculationMapping;
template<> struct ToCalculationMapping<Sum> { using type = Style::Calculation::Sum; };
template<> struct ToCalculationMapping<Product> { using type = Style::Calculation::Product; };
template<> struct ToCalculationMapping<Negate> { using type = Style::Calculation::Negate; };
template<> struct ToCalculationMapping<Invert> { using type = Style::Calculation::Invert; };
template<> struct ToCalculationMapping<Min> { using type = Style::Calculation::Min; };
template<> struct ToCalculationMapping<Max> { using type = Style::Calculation::Max; };
template<> struct ToCalculationMapping<Clamp> { using type = Style::Calculation::Clamp; };
template<> struct ToCalculationMapping<RoundNearest> { using type = Style::Calculation::RoundNearest; };
template<> struct ToCalculationMapping<RoundUp> { using type = Style::Calculation::RoundUp; };
template<> struct ToCalculationMapping<RoundDown> { using type = Style::Calculation::RoundDown; };
template<> struct ToCalculationMapping<RoundToZero> { using type = Style::Calculation::RoundToZero; };
template<> struct ToCalculationMapping<Mod> { using type = Style::Calculation::Mod; };
template<> struct ToCalculationMapping<Rem> { using type = Style::Calculation::Rem; };
template<> struct ToCalculationMapping<Sin> { using type = Style::Calculation::Sin; };
template<> struct ToCalculationMapping<Cos> { using type = Style::Calculation::Cos; };
template<> struct ToCalculationMapping<Tan> { using type = Style::Calculation::Tan; };
template<> struct ToCalculationMapping<Asin> { using type = Style::Calculation::Asin; };
template<> struct ToCalculationMapping<Acos> { using type = Style::Calculation::Acos; };
template<> struct ToCalculationMapping<Atan> { using type = Style::Calculation::Atan; };
template<> struct ToCalculationMapping<Atan2> { using type = Style::Calculation::Atan2; };
template<> struct ToCalculationMapping<Pow> { using type = Style::Calculation::Pow; };
template<> struct ToCalculationMapping<Sqrt> { using type = Style::Calculation::Sqrt; };
template<> struct ToCalculationMapping<Hypot> { using type = Style::Calculation::Hypot; };
template<> struct ToCalculationMapping<Log> { using type = Style::Calculation::Log; };
template<> struct ToCalculationMapping<Exp> { using type = Style::Calculation::Exp; };
template<> struct ToCalculationMapping<Abs> { using type = Style::Calculation::Abs; };
template<> struct ToCalculationMapping<Sign> { using type = Style::Calculation::Sign; };
template<> struct ToCalculationMapping<Random> { using type = Style::Calculation::Random; };
template<> struct ToCalculationMapping<Progress> { using type = Style::Calculation::Progress; };

template<typename> struct ToCalcMapping;
template<> struct ToCalcMapping<Style::Calculation::Sum> { using type = Sum; };
template<> struct ToCalcMapping<Style::Calculation::Product> { using type = Product; };
template<> struct ToCalcMapping<Style::Calculation::Negate> { using type = Negate; };
template<> struct ToCalcMapping<Style::Calculation::Invert> { using type = Invert; };
template<> struct ToCalcMapping<Style::Calculation::Min> { using type = Min; };
template<> struct ToCalcMapping<Style::Calculation::Max> { using type = Max; };
template<> struct ToCalcMapping<Style::Calculation::Clamp> { using type = Clamp; };
template<> struct ToCalcMapping<Style::Calculation::RoundNearest> { using type = RoundNearest; };
template<> struct ToCalcMapping<Style::Calculation::RoundUp> { using type = RoundUp; };
template<> struct ToCalcMapping<Style::Calculation::RoundDown> { using type = RoundDown; };
template<> struct ToCalcMapping<Style::Calculation::RoundToZero> { using type = RoundToZero; };
template<> struct ToCalcMapping<Style::Calculation::Mod> { using type = Mod; };
template<> struct ToCalcMapping<Style::Calculation::Rem> { using type = Rem; };
template<> struct ToCalcMapping<Style::Calculation::Sin> { using type = Sin; };
template<> struct ToCalcMapping<Style::Calculation::Cos> { using type = Cos; };
template<> struct ToCalcMapping<Style::Calculation::Tan> { using type = Tan; };
template<> struct ToCalcMapping<Style::Calculation::Asin> { using type = Asin; };
template<> struct ToCalcMapping<Style::Calculation::Acos> { using type = Acos; };
template<> struct ToCalcMapping<Style::Calculation::Atan> { using type = Atan; };
template<> struct ToCalcMapping<Style::Calculation::Atan2> { using type = Atan2; };
template<> struct ToCalcMapping<Style::Calculation::Pow> { using type = Pow; };
template<> struct ToCalcMapping<Style::Calculation::Sqrt> { using type = Sqrt; };
template<> struct ToCalcMapping<Style::Calculation::Hypot> { using type = Hypot; };
template<> struct ToCalcMapping<Style::Calculation::Log> { using type = Log; };
template<> struct ToCalcMapping<Style::Calculation::Exp> { using type = Exp; };
template<> struct ToCalcMapping<Style::Calculation::Abs> { using type = Abs; };
template<> struct ToCalcMapping<Style::Calculation::Sign> { using type = Sign; };
template<> struct ToCalcMapping<Style::Calculation::Random> { using type = Random; };
template<> struct ToCalcMapping<Style::Calculation::Progress> { using type = Progress; };

template<typename T> using ToCalculationTreeOp = typename ToCalculationMapping<T>::type;
template<typename T> using ToCalcTreeOp = typename ToCalcMapping<T>::type;

} // namespace CSSCalc
} // namespace WebCore
