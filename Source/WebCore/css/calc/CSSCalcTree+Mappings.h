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
#include "CalculationTree.h"

namespace WebCore {
namespace CSSCalc {

template<typename> struct ToCalculationMapping;
template<> struct ToCalculationMapping<Sum> { using type = Calculation::Sum; };
template<> struct ToCalculationMapping<Product> { using type = Calculation::Product; };
template<> struct ToCalculationMapping<Negate> { using type = Calculation::Negate; };
template<> struct ToCalculationMapping<Invert> { using type = Calculation::Invert; };
template<> struct ToCalculationMapping<Min> { using type = Calculation::Min; };
template<> struct ToCalculationMapping<Max> { using type = Calculation::Max; };
template<> struct ToCalculationMapping<Clamp> { using type = Calculation::Clamp; };
template<> struct ToCalculationMapping<RoundNearest> { using type = Calculation::RoundNearest; };
template<> struct ToCalculationMapping<RoundUp> { using type = Calculation::RoundUp; };
template<> struct ToCalculationMapping<RoundDown> { using type = Calculation::RoundDown; };
template<> struct ToCalculationMapping<RoundToZero> { using type = Calculation::RoundToZero; };
template<> struct ToCalculationMapping<Mod> { using type = Calculation::Mod; };
template<> struct ToCalculationMapping<Rem> { using type = Calculation::Rem; };
template<> struct ToCalculationMapping<Sin> { using type = Calculation::Sin; };
template<> struct ToCalculationMapping<Cos> { using type = Calculation::Cos; };
template<> struct ToCalculationMapping<Tan> { using type = Calculation::Tan; };
template<> struct ToCalculationMapping<Asin> { using type = Calculation::Asin; };
template<> struct ToCalculationMapping<Acos> { using type = Calculation::Acos; };
template<> struct ToCalculationMapping<Atan> { using type = Calculation::Atan; };
template<> struct ToCalculationMapping<Atan2> { using type = Calculation::Atan2; };
template<> struct ToCalculationMapping<Pow> { using type = Calculation::Pow; };
template<> struct ToCalculationMapping<Sqrt> { using type = Calculation::Sqrt; };
template<> struct ToCalculationMapping<Hypot> { using type = Calculation::Hypot; };
template<> struct ToCalculationMapping<Log> { using type = Calculation::Log; };
template<> struct ToCalculationMapping<Exp> { using type = Calculation::Exp; };
template<> struct ToCalculationMapping<Abs> { using type = Calculation::Abs; };
template<> struct ToCalculationMapping<Sign> { using type = Calculation::Sign; };
template<> struct ToCalculationMapping<Random> { using type = Calculation::Random; };
template<> struct ToCalculationMapping<Progress> { using type = Calculation::Progress; };

template<typename> struct ToCalcMapping;
template<> struct ToCalcMapping<Calculation::Sum> { using type = Sum; };
template<> struct ToCalcMapping<Calculation::Product> { using type = Product; };
template<> struct ToCalcMapping<Calculation::Negate> { using type = Negate; };
template<> struct ToCalcMapping<Calculation::Invert> { using type = Invert; };
template<> struct ToCalcMapping<Calculation::Min> { using type = Min; };
template<> struct ToCalcMapping<Calculation::Max> { using type = Max; };
template<> struct ToCalcMapping<Calculation::Clamp> { using type = Clamp; };
template<> struct ToCalcMapping<Calculation::RoundNearest> { using type = RoundNearest; };
template<> struct ToCalcMapping<Calculation::RoundUp> { using type = RoundUp; };
template<> struct ToCalcMapping<Calculation::RoundDown> { using type = RoundDown; };
template<> struct ToCalcMapping<Calculation::RoundToZero> { using type = RoundToZero; };
template<> struct ToCalcMapping<Calculation::Mod> { using type = Mod; };
template<> struct ToCalcMapping<Calculation::Rem> { using type = Rem; };
template<> struct ToCalcMapping<Calculation::Sin> { using type = Sin; };
template<> struct ToCalcMapping<Calculation::Cos> { using type = Cos; };
template<> struct ToCalcMapping<Calculation::Tan> { using type = Tan; };
template<> struct ToCalcMapping<Calculation::Asin> { using type = Asin; };
template<> struct ToCalcMapping<Calculation::Acos> { using type = Acos; };
template<> struct ToCalcMapping<Calculation::Atan> { using type = Atan; };
template<> struct ToCalcMapping<Calculation::Atan2> { using type = Atan2; };
template<> struct ToCalcMapping<Calculation::Pow> { using type = Pow; };
template<> struct ToCalcMapping<Calculation::Sqrt> { using type = Sqrt; };
template<> struct ToCalcMapping<Calculation::Hypot> { using type = Hypot; };
template<> struct ToCalcMapping<Calculation::Log> { using type = Log; };
template<> struct ToCalcMapping<Calculation::Exp> { using type = Exp; };
template<> struct ToCalcMapping<Calculation::Abs> { using type = Abs; };
template<> struct ToCalcMapping<Calculation::Sign> { using type = Sign; };
template<> struct ToCalcMapping<Calculation::Random> { using type = Random; };
template<> struct ToCalcMapping<Calculation::Progress> { using type = Progress; };

template<typename T> using ToCalculationTreeOp = typename ToCalculationMapping<T>::type;
template<typename T> using ToCalcTreeOp = typename ToCalcMapping<T>::type;

} // namespace CSSCalc
} // namespace WebCore
