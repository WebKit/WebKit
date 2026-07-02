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

#include <WebCore/CSSPrimitiveNumericRange.h>

namespace WebCore {
namespace Style {

// MARK: Integer Primitive

template<CSS::Range = CSS::All, typename = int> struct Integer;

// MARK: Number Primitive

template<CSS::Range = CSS::All, typename = double> struct Number;

// MARK: Percentage Primitive

template<CSS::Range = CSS::All, typename = double> struct Percentage;

// MARK: Dimension Primitives

template<CSS::Range = CSS::All, typename = double> struct Angle;
template<CSS::Range = CSS::All, typename = float> struct Length;
template<CSS::Range = CSS::All, typename = double> struct Time;
template<CSS::Range = CSS::All, typename = double> struct Frequency;
template<CSS::Range = CSS::Nonnegative, typename = double> struct Resolution;
template<CSS::Range = CSS::All, typename = double> struct Flex;

// MARK: Dimension + Percentage Primitives

template<CSS::Range = CSS::All, typename = float> struct AnglePercentage;
template<CSS::Range = CSS::All, typename = float> struct LengthPercentage;

} // namespace Style
} // namespace WebCore
