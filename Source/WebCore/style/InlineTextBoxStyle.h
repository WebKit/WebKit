/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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

#include "FontCascade.h"
#include "RenderStyleConstants.h"

namespace WebCore {
    
class InlineTextBox;
class RenderStyle;

inline float wavyOffsetFromDecoration()
{
    return 1;
}

struct WavyStrokeParameters {
    // Distance between decoration's axis and Bezier curve's control points.
    // The height of the curve is based on this distance. Increases the curve's height
    // as fontSize increases to make the curve look better.
    float controlPointDistance { 0 };

    // Increment used to form the diamond shape between start point (p1), control
    // points and end point (p2) along the axis of the decoration. The curve gets
    // wider as font size increases.
    float step { 0 };
};
WavyStrokeParameters getWavyStrokeParameters(float fontSize);
GlyphOverflow visualOverflowForDecorations(const RenderStyle& lineStyle, const InlineTextBox*);
float computeUnderlineOffset(TextUnderlinePosition, TextUnderlineOffset, const FontMetrics&, const InlineTextBox*, float textDecorationThickness);
    
} // namespace WebCore
