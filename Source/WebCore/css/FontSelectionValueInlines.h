/*
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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
    case CSSValueID::Normal:
        return normalWeightValue();
    case CSSValueID::Bold:
    case CSSValueID::Bolder:
        return boldWeightValue();
    case CSSValueID::Lighter:
        return lightWeightValue();
    default:
        return std::nullopt;
    }
}

inline std::optional<CSSValueID> fontWidthKeyword(FontSelectionValue width)
{
    if (width == ultraCondensedWidthValue())
        return CSSValueID::UltraCondensed;
    if (width == extraCondensedWidthValue())
        return CSSValueID::ExtraCondensed;
    if (width == condensedWidthValue())
        return CSSValueID::Condensed;
    if (width == semiCondensedWidthValue())
        return CSSValueID::SemiCondensed;
    if (width == normalWidthValue())
        return CSSValueID::Normal;
    if (width == semiExpandedWidthValue())
        return CSSValueID::SemiExpanded;
    if (width == expandedWidthValue())
        return CSSValueID::Expanded;
    if (width == extraExpandedWidthValue())
        return CSSValueID::ExtraExpanded;
    if (width == ultraExpandedWidthValue())
        return CSSValueID::UltraExpanded;
    return std::nullopt;
}

inline std::optional<FontSelectionValue> fontWidthValue(CSSValueID value)
{
    switch (value) {
    case CSSValueID::UltraCondensed:
        return ultraCondensedWidthValue();
    case CSSValueID::ExtraCondensed:
        return extraCondensedWidthValue();
    case CSSValueID::Condensed:
        return condensedWidthValue();
    case CSSValueID::SemiCondensed:
        return semiCondensedWidthValue();
    case CSSValueID::Normal:
        return normalWidthValue();
    case CSSValueID::SemiExpanded:
        return semiExpandedWidthValue();
    case CSSValueID::Expanded:
        return expandedWidthValue();
    case CSSValueID::ExtraExpanded:
        return extraExpandedWidthValue();
    case CSSValueID::UltraExpanded:
        return ultraExpandedWidthValue();
    default:
        return std::nullopt;
    }
}

inline std::optional<CSSValueID> fontStyleKeyword(std::optional<FontSelectionValue> style, FontStyleAxis axis)
{
    if (axis == FontStyleAxis::normal)
        return CSSValueID::Normal;
    if (style && style.value() == italicValue())
        return axis == FontStyleAxis::ital ? CSSValueID::Italic : CSSValueID::Oblique;
    return std::nullopt;
}

inline FontSelectionValue normalizedFontItalicValue(float inputValue)
{
    return FontSelectionValue { std::clamp(inputValue, -90.0f, 90.0f) };
}

}
