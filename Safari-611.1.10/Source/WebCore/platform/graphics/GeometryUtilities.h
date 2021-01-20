/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatRect.h"
#include "IntRect.h"
#include <wtf/Forward.h>

namespace WebCore {

float euclidianDistance(const FloatPoint&, const FloatPoint&);

float findSlope(const FloatPoint& p1, const FloatPoint& p2, float& c);

// Find point where lines through the two pairs of points intersect. Returns false if the lines don't intersect.
WEBCORE_EXPORT bool findIntersection(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& d1, const FloatPoint& d2, FloatPoint& intersection);

WEBCORE_EXPORT IntRect unionRect(const Vector<IntRect>&);
WEBCORE_EXPORT FloatRect unionRect(const Vector<FloatRect>&);
WEBCORE_EXPORT FloatRect unionRectIgnoringZeroRects(const Vector<FloatRect>&);

// Map point from srcRect to an equivalent point in destRect.
FloatPoint mapPoint(FloatPoint, const FloatRect& srcRect, const FloatRect& destRect);

// Map rect from srcRect to an equivalent rect in destRect.
FloatRect mapRect(const FloatRect&, const FloatRect& srcRect, const FloatRect& destRect);

WEBCORE_EXPORT FloatRect largestRectWithAspectRatioInsideRect(float aspectRatio, const FloatRect&);
WEBCORE_EXPORT FloatRect smallestRectWithAspectRatioAroundRect(float aspectRatio, const FloatRect&);

FloatSize sizeWithAreaAndAspectRatio(float area, float aspectRatio);

// Compute a rect that encloses all points covered by the given rect if it were rotated a full turn around (0,0).
FloatRect boundsOfRotatingRect(const FloatRect&);

bool ellipseContainsPoint(const FloatPoint& center, const FloatSize& radii, const FloatPoint&);
}
