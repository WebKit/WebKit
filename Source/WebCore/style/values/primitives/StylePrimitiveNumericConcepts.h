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

#include <WebCore/CSSPrimitiveNumericConcepts.h>

namespace WebCore {
namespace Style {

// Forward declaration of PrimitiveNumeric to needed to create a hard constraint for the Numeric concept below.
template<CSS::Numeric> struct PrimitiveNumeric;

// Concept for use in generic contexts to filter on all numeric Style types.
template<typename T> concept Numeric = std::derived_from<T, PrimitiveNumeric<typename T::CSS>>;

// Concept for use in generic contexts to filter on non-composite numeric Style types.
template<typename T> concept NonCompositeNumeric = Numeric<T> && CSS::NonCompositeNumeric<typename T::CSS>;

// Concept for use in generic contexts to filter on dimension-percentage numeric Style types.
template<typename T> concept DimensionPercentageNumeric = Numeric<T> && VariantLike<T> && CSS::DimensionPercentageNumeric<typename T::CSS>;

// Forward declaration of UnevaluatedCalculation to needed to create a hard constraint for the Calc concept below.
template<CSS::Numeric> struct UnevaluatedCalculation;

// Concept for use in generic contexts to filter on UnevaluatedCalc Style types.
template<typename T> concept Calc = std::same_as<T, UnevaluatedCalculation<typename T::CSS>>;

} // namespace Style
} // namespace WebCore
