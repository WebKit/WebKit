/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CSSValueKeywords.h"
#include "FontSelectionAlgorithm.h"

namespace WebCore {

inline std::optional<FontSelectionValue> fontWeightValue(CSSValueID value)
{
    switch (value) {
    case CSSValueNormal:
        return normalWeightValue();
    case CSSValueBold:
    case CSSValueBolder:
        return boldWeightValue();
    case CSSValueLighter:
        return lightWeightValue();
    default:
        return std::nullopt;
    }
}

inline std::optional<CSSValueID> fontWidthKeyword(FontSelectionValue width)
{
    if (width == ultraCondensedWidthValue())
        return CSSValueUltraCondensed;
    if (width == extraCondensedWidthValue())
        return CSSValueExtraCondensed;
    if (width == condensedWidthValue())
        return CSSValueCondensed;
    if (width == semiCondensedWidthValue())
        return CSSValueSemiCondensed;
    if (width == normalWidthValue())
        return CSSValueNormal;
    if (width == semiExpandedWidthValue())
        return CSSValueSemiExpanded;
    if (width == expandedWidthValue())
        return CSSValueExpanded;
    if (width == extraExpandedWidthValue())
        return CSSValueExtraExpanded;
    if (width == ultraExpandedWidthValue())
        return CSSValueUltraExpanded;
    return std::nullopt;
}

inline std::optional<FontSelectionValue> fontWidthValue(CSSValueID value)
{
    switch (value) {
    case CSSValueUltraCondensed:
        return ultraCondensedWidthValue();
    case CSSValueExtraCondensed:
        return extraCondensedWidthValue();
    case CSSValueCondensed:
        return condensedWidthValue();
    case CSSValueSemiCondensed:
        return semiCondensedWidthValue();
    case CSSValueNormal:
        return normalWidthValue();
    case CSSValueSemiExpanded:
        return semiExpandedWidthValue();
    case CSSValueExpanded:
        return expandedWidthValue();
    case CSSValueExtraExpanded:
        return extraExpandedWidthValue();
    case CSSValueUltraExpanded:
        return ultraExpandedWidthValue();
    default:
        return std::nullopt;
    }
}

inline std::optional<CSSValueID> fontStyleKeyword(std::optional<FontSelectionValue> style, FontStyleAxis axis)
{
    if (!style)
        return CSSValueNormal;
    if (style.value() == italicValue())
        return axis == FontStyleAxis::ital ? CSSValueItalic : CSSValueOblique;
    return std::nullopt;
}

inline FontSelectionValue normalizedFontItalicValue(float inputValue)
{
    return FontSelectionValue { std::clamp(inputValue, -90.0f, 90.0f) };
}

}
