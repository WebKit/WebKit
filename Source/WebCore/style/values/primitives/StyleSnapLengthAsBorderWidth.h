/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StylePrimitiveNumeric.h>

namespace WebCore {
namespace Style {

// Snaps a non-negative length, in CSS pixels, to an integer number of device pixels.
// https://drafts.csswg.org/css-values-4/#snap-a-length-as-a-border-width
float snapLengthAsBorderWidth(float, float deviceScaleFactor);
Length<CSS::Nonnegative> snapLengthAsBorderWidth(Length<CSS::Nonnegative>, float deviceScaleFactor);

// As above, but for lengths that may be negative, such as `outline-offset`. The magnitude is
// snapped and the sign restored, so a magnitude smaller than 1 device pixel rounds away from
// zero and a larger magnitude rounds toward zero.
float snapSignedLengthAsBorderWidth(float, float deviceScaleFactor);
Length<> snapSignedLengthAsBorderWidth(Length<>, float deviceScaleFactor);

} // namespace Style
} // namespace WebCore
