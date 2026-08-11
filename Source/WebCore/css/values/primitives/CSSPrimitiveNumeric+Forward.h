/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
namespace CSS {

// MARK: Integer Primitive

template<Range = All, typename = int> struct Integer;

// MARK: Number Primitive

template<Range = All, typename = double> struct Number;

// MARK: Percentage Primitive

template<Range = All, typename = double> struct Percentage;

// MARK: Dimension Primitives

template<Range = All, typename = double> struct Angle;
template<Range = All, typename = float> struct Length;
template<Range = All, typename = double> struct Time;
template<Range = All, typename = double> struct Frequency;
template<Range = Nonnegative, typename = double> struct Resolution;
template<Range = All, typename = double> struct Flex;

// MARK: Dimension + Percentage Primitives

template<Range = All, typename = float> struct AnglePercentage;
template<Range = All, typename = float> struct LengthPercentage;

} // namespace CSS
} // namespace WebCore
