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

inline bool isCSS21Weight(FontSelectionValue weight)
{
    return weight == FontSelectionValue(100)
        || weight == FontSelectionValue(200)
        || weight == FontSelectionValue(300)
        || weight == FontSelectionValue(400)
        || weight == FontSelectionValue(500)
        || weight == FontSelectionValue(600)
        || weight == FontSelectionValue(700)
        || weight == FontSelectionValue(800)
        || weight == FontSelectionValue(900);
}

inline bool isCSS21Weight(int weight)
{
    return !((weight % 100) || weight < 100 || weight > 900);
}

inline Optional<CSSValueID> fontWeightKeyword(FontSelectionValue weight)
{
    if (weight == normalWeightValue())
        return CSSValueNormal;
    if (weight == boldWeightValue())
        return CSSValueBold;
    return WTF::nullopt;
}

inline Optional<FontSelectionValue> fontWeightValue(CSSValueID value)
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
        return WTF::nullopt;
    }
}

inline Optional<CSSValueID> fontStretchKeyword(FontSelectionValue stretch)
{
    if (stretch == ultraCondensedStretchValue())
        return CSSValueUltraCondensed;
    if (stretch == extraCondensedStretchValue())
        return CSSValueExtraCondensed;
    if (stretch == condensedStretchValue())
        return CSSValueCondensed;
    if (stretch == semiCondensedStretchValue())
        return CSSValueSemiCondensed;
    if (stretch == normalStretchValue())
        return CSSValueNormal;
    if (stretch == semiExpandedStretchValue())
        return CSSValueSemiExpanded;
    if (stretch == expandedStretchValue())
        return CSSValueExpanded;
    if (stretch == extraExpandedStretchValue())
        return CSSValueExtraExpanded;
    if (stretch == ultraExpandedStretchValue())
        return CSSValueUltraExpanded;
    return WTF::nullopt;
}

inline Optional<FontSelectionValue> fontStretchValue(CSSValueID value)
{
    switch (value) {
    case CSSValueUltraCondensed:
        return ultraCondensedStretchValue();
    case CSSValueExtraCondensed:
        return extraCondensedStretchValue();
    case CSSValueCondensed:
        return condensedStretchValue();
    case CSSValueSemiCondensed:
        return semiCondensedStretchValue();
    case CSSValueNormal:
        return normalStretchValue();
    case CSSValueSemiExpanded:
        return semiExpandedStretchValue();
    case CSSValueExpanded:
        return expandedStretchValue();
    case CSSValueExtraExpanded:
        return extraExpandedStretchValue();
    case CSSValueUltraExpanded:
        return ultraExpandedStretchValue();
    default:
        return WTF::nullopt;
    }
}

inline Optional<CSSValueID> fontStyleKeyword(Optional<FontSelectionValue> style, FontStyleAxis axis)
{
    if (!style || style.value() == normalItalicValue())
        return CSSValueNormal;
    if (style.value() == italicValue())
        return axis == FontStyleAxis::ital ? CSSValueItalic : CSSValueOblique;
    return WTF::nullopt;
}

}
