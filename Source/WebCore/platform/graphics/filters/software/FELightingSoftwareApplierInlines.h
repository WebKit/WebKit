/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2018-2024 Apple, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FELightingSoftwareApplier.h"

namespace WebCore {

inline IntSize FELightingSoftwareApplier::LightingData::topLeftNormal(int offset) const
{
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -2 * center + 2 * right - bottom + bottomRight,
        -2 * center - right + 2 * bottom + bottomRight
    };
}

inline IntSize FELightingSoftwareApplier::LightingData::topRowNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottomLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -2 * left + 2 * right - bottomLeft + bottomRight,
        -left - 2 * center - right + bottomLeft + 2 * bottom + bottomRight
    };
}

inline IntSize FELightingSoftwareApplier::LightingData::topRightNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottomLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    return {
        -2 * left + 2 * center - bottomLeft + bottom,
        -left - 2 * center + bottomLeft + 2 * bottom
    };
}

inline IntSize FELightingSoftwareApplier::LightingData::leftColumnNormal(int offset) const
{
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset += 2 * widthMultipliedByPixelSize;
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -top + topRight - 2 * center + 2 * right - bottom + bottomRight,
        -2 * top - topRight + 2 * bottom + bottomRight
    };
}

inline IntSize FELightingSoftwareApplier::LightingData::interiorNormal(int offset, AlphaWindow& alphaWindow) const
{
    int rightAlphaOffset = offset + cPixelSize + cAlphaChannelOffset;

    int right = static_cast<int>(pixels->item(rightAlphaOffset));
    int topRight = static_cast<int>(pixels->item(rightAlphaOffset - widthMultipliedByPixelSize));
    int bottomRight = static_cast<int>(pixels->item(rightAlphaOffset + widthMultipliedByPixelSize));

    int left = alphaWindow.left();
    int topLeft = alphaWindow.topLeft();
    int top = alphaWindow.top();

    int bottomLeft = alphaWindow.bottomLeft();
    int bottom = alphaWindow.bottom();

    // The alphaWindow has been shifted, and here we fill in the right column.
    alphaWindow.alpha[0][2] = topRight;
    alphaWindow.alpha[1][2] = right;
    alphaWindow.alpha[2][2] = bottomRight;

    // Check that the alphaWindow is working with some spot-checks.
    ASSERT(alphaWindow.topLeft() == pixels->item(offset - cPixelSize - widthMultipliedByPixelSize + cAlphaChannelOffset)); // topLeft
    ASSERT(alphaWindow.top() == pixels->item(offset - widthMultipliedByPixelSize + cAlphaChannelOffset)); // top

    return {
        -topLeft + topRight - 2 * left + 2 * right - bottomLeft + bottomRight,
        -topLeft - 2 * top - topRight + bottomLeft + 2 * bottom + bottomRight
    };
}

inline IntSize FELightingSoftwareApplier::LightingData::rightColumnNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    offset += 2 * widthMultipliedByPixelSize;
    int bottomLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    return {
        -topLeft + top - 2 * left + 2 * center - bottomLeft + bottom,
        -topLeft - 2 * top + bottomLeft + 2 * bottom
    };
}

inline IntSize FELightingSoftwareApplier::LightingData::bottomLeftNormal(int offset) const
{
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -top + topRight - 2 * center + 2 * right,
        -2 * top - topRight + 2 * center + right
    };
}

inline IntSize FELightingSoftwareApplier::LightingData::bottomRowNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -topLeft + topRight - 2 * left + 2 * right,
        -topLeft - 2 * top - topRight + left + 2 * center + right
    };
}

inline IntSize FELightingSoftwareApplier::LightingData::bottomRightNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    return {
        -topLeft + top - 2 * left + 2 * center,
        -topLeft - 2 * top + left + 2 * center
    };
}

} // namespace WebCore
