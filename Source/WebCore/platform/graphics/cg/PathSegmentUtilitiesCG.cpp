/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "PathSegmentUtilitiesCG.h"

#if USE(CG)

namespace WebCore {

void addEllipseToPlatformPath(PlatformPathPtr platformPath, const FloatPoint& center, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection direction)
{
    AffineTransform transform;
    transform.translate(center.x(), center.y()).rotate(rad2deg(rotation)).scale(radiusX, radiusY);

    CGAffineTransform cgTransform = transform;
    // CG coordinates system increases the angle in the anticlockwise direction.
    CGPathAddArc(platformPath, &cgTransform, 0, 0, 1, startAngle, endAngle, direction == RotationDirection::Counterclockwise);
}

static void addEvenCornersRoundedRect(PlatformPathPtr platformPath, const FloatRect& rect, const FloatSize& radius)
{
    // Ensure that CG can render the rounded rect.
    CGFloat radiusWidth = radius.width();
    CGFloat radiusHeight = radius.height();
    CGRect rectToDraw = rect;

    CGFloat rectWidth = CGRectGetWidth(rectToDraw);
    CGFloat rectHeight = CGRectGetHeight(rectToDraw);
    if (2 * radiusWidth > rectWidth)
        radiusWidth = rectWidth / 2 - std::numeric_limits<CGFloat>::epsilon();
    if (2 * radiusHeight > rectHeight)
        radiusHeight = rectHeight / 2 - std::numeric_limits<CGFloat>::epsilon();
    CGPathAddRoundedRect(platformPath, nullptr, rectToDraw, radiusWidth, radiusHeight);
}

#if HAVE(CG_PATH_UNEVEN_CORNERS_ROUNDEDRECT)
static void addUnevenCornersRoundedRect(PlatformPathPtr platformPath, const FloatRoundedRect& roundedRect)
{
    enum Corners {
        BottomLeft,
        BottomRight,
        TopRight,
        TopLeft
    };

    CGSize corners[4] = {
        roundedRect.radii().bottomLeft(),
        roundedRect.radii().bottomRight(),
        roundedRect.radii().topRight(),
        roundedRect.radii().topLeft()
    };

    CGRect rectToDraw = roundedRect.rect();
    CGFloat rectWidth = CGRectGetWidth(rectToDraw);
    CGFloat rectHeight = CGRectGetHeight(rectToDraw);

    // Clamp the radii after conversion to CGFloats.
    corners[TopRight].width = std::min(corners[TopRight].width, rectWidth - corners[TopLeft].width);
    corners[BottomRight].width = std::min(corners[BottomRight].width, rectWidth - corners[BottomLeft].width);
    corners[BottomLeft].height = std::min(corners[BottomLeft].height, rectHeight - corners[TopLeft].height);
    corners[BottomRight].height = std::min(corners[BottomRight].height, rectHeight - corners[TopRight].height);

    CGPathAddUnevenCornersRoundedRect(platformPath, nullptr, rectToDraw, corners);
}
#endif

bool addRoundedRectToPlatformPath(PlatformPathPtr platformPath, const FloatRoundedRect& roundedRect)
{
    const auto& radii = roundedRect.radii();

    if (radii.hasEvenCorners()) {
        addEvenCornersRoundedRect(platformPath, roundedRect.rect(), radii.topLeft());
        return true;
    }

#if HAVE(CG_PATH_UNEVEN_CORNERS_ROUNDEDRECT)
    addUnevenCornersRoundedRect(platformPath, roundedRect);
    return true;
#else
    return false;
#endif
}

} // namespace WebCore

#endif // USE(CG)

