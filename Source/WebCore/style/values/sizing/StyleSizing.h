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

#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// Shared types and utilities for the sizing properties: preferred size, minimum size, maximum size and flex basis.
// https://drafts.csswg.org/css-sizing-3/#sizing-properties
// https://drafts.csswg.org/css-flexbox-1/#flex-basis-property

using FixedSize = LengthPercentage<CSS::Nonnegative>::Dimension;
using PercentageSize = LengthPercentage<CSS::Nonnegative>::Percentage;

LayoutUnit evaluateMinimum(const LengthPercentage<CSS::Nonnegative>::Dimension&, LayoutUnit maximumValue);
LayoutUnit evaluateMinimum(const LengthPercentage<CSS::Nonnegative>::Percentage&, LayoutUnit maximumValue);
LayoutUnit evaluateMinimum(const LengthPercentage<CSS::Nonnegative>::Calc&, LayoutUnit maximumValue);

} // namespace Style
} // namespace WebCore
