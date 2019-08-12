/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TextSizeAdjustment.h"

#if ENABLE(TEXT_AUTOSIZING)

#include "RenderStyle.h"

namespace WebCore {

AutosizeStatus::AutosizeStatus(OptionSet<Fields> fields)
    : m_fields(fields)
{
}

bool AutosizeStatus::contains(Fields fields) const
{
    return m_fields.contains(fields);
}

bool AutosizeStatus::probablyContainsASmallFixedNumberOfLines(const RenderStyle& style)
{
    auto& lineHeightAsLength = style.specifiedLineHeight();
    if (!lineHeightAsLength.isFixed() && !lineHeightAsLength.isPercent())
        return false;

    auto& maxHeight = style.maxHeight();
    Optional<Length> heightOrMaxHeightAsLength;
    if (maxHeight.isFixed())
        heightOrMaxHeightAsLength = style.maxHeight();
    else if (style.height().isFixed() && (!maxHeight.isSpecified() || maxHeight.isUndefined()))
        heightOrMaxHeightAsLength = style.height();

    if (!heightOrMaxHeightAsLength)
        return false;

    float heightOrMaxHeight = heightOrMaxHeightAsLength->value();
    if (heightOrMaxHeight <= 0)
        return false;

    float approximateLineHeight = lineHeightAsLength.isPercent() ? lineHeightAsLength.percent() * style.specifiedFontSize() / 100 : lineHeightAsLength.value();
    if (approximateLineHeight <= 0)
        return false;

    float approximateNumberOfLines = heightOrMaxHeight / approximateLineHeight;
    auto& lineClamp = style.lineClamp();
    if (!lineClamp.isNone() && !lineClamp.isPercentage()) {
        int lineClampValue = lineClamp.value();
        return lineClampValue && std::floor(approximateNumberOfLines) == lineClampValue;
    }

    const int maximumNumberOfLines = 5;
    const float thresholdForConsideringAnApproximateNumberOfLinesToBeCloseToAnInteger = 0.01;
    return approximateNumberOfLines <= maximumNumberOfLines + thresholdForConsideringAnApproximateNumberOfLinesToBeCloseToAnInteger
        && approximateNumberOfLines - std::floor(approximateNumberOfLines) <= thresholdForConsideringAnApproximateNumberOfLinesToBeCloseToAnInteger;
}

void AutosizeStatus::updateStatus(RenderStyle& style)
{
    auto result = style.autosizeStatus().fields();

    auto shouldAvoidAutosizingEntireSubtree = [&] {
        if (style.display() == DisplayType::None)
            return true;

        const float maximumDifferenceBetweenFixedLineHeightAndFontSize = 5;
        auto& lineHeight = style.specifiedLineHeight();
        if (lineHeight.isFixed() && lineHeight.value() - style.specifiedFontSize() > maximumDifferenceBetweenFixedLineHeightAndFontSize)
            return false;

        if (style.whiteSpace() == WhiteSpace::NoWrap)
            return false;

        return probablyContainsASmallFixedNumberOfLines(style);
    };

    if (shouldAvoidAutosizingEntireSubtree())
        result.add(Fields::AvoidSubtree);

    if (style.height().isFixed())
        result.add(Fields::FixedHeight);

    if (style.width().isFixed())
        result.add(Fields::FixedWidth);

    if (style.overflowX() == Overflow::Hidden)
        result.add(Fields::OverflowXHidden);

    if (style.isFloating())
        result.add(Fields::Floating);

    style.setAutosizeStatus(result);
}

float AutosizeStatus::idempotentTextSize(float specifiedSize, float pageScale)
{
    if (pageScale >= 1)
        return specifiedSize;

    // This describes a piecewise curve when the page scale is 2/3.
    FloatPoint points[] = { {0.0f, 0.0f}, {6.0f, 9.0f}, {14.0f, 17.0f} };

    // When the page scale is 1, the curve should be the identity.
    // Linearly interpolate between the curve above and identity based on the page scale.
    // Beware that depending on the specific values picked in the curve, this interpolation might change the shape of the curve for very small pageScales.
    pageScale = std::min(std::max(pageScale, 0.5f), 1.0f);
    auto scalePoint = [&](FloatPoint point) {
        float fraction = 3.0f - 3.0f * pageScale;
        point.setY(point.x() + (point.y() - point.x()) * fraction);
        return point;
    };

    if (specifiedSize <= 0)
        return 0;

    float result = scalePoint(points[WTF_ARRAY_LENGTH(points) - 1]).y();
    for (size_t i = 1; i < WTF_ARRAY_LENGTH(points); ++i) {
        if (points[i].x() < specifiedSize)
            continue;
        auto leftPoint = scalePoint(points[i - 1]);
        auto rightPoint = scalePoint(points[i]);
        float fraction = (specifiedSize - leftPoint.x()) / (rightPoint.x() - leftPoint.x());
        result = leftPoint.y() + fraction * (rightPoint.y() - leftPoint.y());
        break;
    }

    return std::max(std::round(result), specifiedSize);
}

}

#endif
