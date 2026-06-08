/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "AutosizeStatus.h"

#if ENABLE(TEXT_AUTOSIZING)

#include "RenderStyle+GettersInlines.h"

namespace WebCore {

auto AutosizeStatus::compute(const RenderStyle& style) -> AutosizeStatus
{
    auto result = style.autosizeStatus().fields();

    auto shouldAvoidAutosizingEntireSubtree = [&] {
        if (style.display() == Style::DisplayType::None)
            return true;

        const float maximumDifferenceBetweenFixedLineHeightAndFontSize = 5;
        auto& lineHeight = style.specifiedLineHeight();
        if (auto fixedLineHeight = lineHeight.tryFixed(); fixedLineHeight && fixedLineHeight->resolveZoom(style.usedZoomForLength()) - style.specifiedFontSize() > maximumDifferenceBetweenFixedLineHeightAndFontSize)
            return false;

        if (style.whiteSpaceCollapse() == WhiteSpaceCollapse::Collapse && style.textWrapMode() == TextWrapMode::NoWrap)
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

    if (style.floating() != Float::None)
        result.add(Fields::Floating);

    return AutosizeStatus(result);
}

auto AutosizeStatus::isIdempotentTextAutosizingCandidate(const RenderStyle& style) -> bool
{
    // Refer to <rdar://problem/51826266> for more information regarding how this function was generated.
    auto fields = this->fields();

    if (fields.contains(AutosizeStatus::Fields::AvoidSubtree))
        return false;

    constexpr auto smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText = 5.0f;
    constexpr auto largeMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText = 25.0f;

    if (fields.contains(AutosizeStatus::Fields::FixedHeight)) {
        if (fields.contains(AutosizeStatus::Fields::FixedWidth)) {
            if (style.whiteSpaceCollapse() == WhiteSpaceCollapse::Collapse && style.textWrapMode() == TextWrapMode::NoWrap) {
                if (style.width().isFixed())
                    return false;
                if (auto fixedHeight = style.height().tryFixed(); fixedHeight && style.specifiedLineHeight().isFixed()) {
                    if (auto fixedSpecifiedLineHeight = style.specifiedLineHeight().tryFixed()) {
                        auto specifiedSize = style.specifiedFontSize();
                        auto zoomFactor = style.usedZoomForLength();
                        if (fixedHeight->resolveZoom(zoomFactor) == specifiedSize && fixedSpecifiedLineHeight->resolveZoom(zoomFactor) == specifiedSize)
                            return false;
                    }
                }
                return true;
            }
            if (fields.contains(AutosizeStatus::Fields::Floating)) {
                if (auto fixedHeight = style.height().tryFixed(); style.specifiedLineHeight().isFixed() && fixedHeight) {
                    if (auto fixedSpecifiedLineHeight = style.specifiedLineHeight().tryFixed()) {
                        auto specifiedSize = style.specifiedFontSize();
                        auto zoomFactor = style.usedZoomForLength();
                        if (fixedSpecifiedLineHeight->resolveZoom(Style::ZoomFactor { 1.0f }) - specifiedSize > smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText
                            && fixedHeight->resolveZoom(zoomFactor) - specifiedSize > smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText)
                            return true;
                    }
                }
                return false;
            }
            if (fields.contains(AutosizeStatus::Fields::OverflowXHidden))
                return false;
            return true;
        }
        if (fields.contains(AutosizeStatus::Fields::OverflowXHidden)) {
            if (fields.contains(AutosizeStatus::Fields::Floating))
                return false;
            return true;
        }
        return true;
    }

    if (style.width().isFixed()) {
        if (style.breakWords())
            return true;
        return false;
    }

    if (auto percentage = style.textSizeAdjust().tryPercentage(); percentage == 100) {
        if (fields.contains(AutosizeStatus::Fields::Floating))
            return true;
        if (fields.contains(AutosizeStatus::Fields::FixedWidth))
            return true;
        if (auto fixedSpecifiedLineHeight = style.specifiedLineHeight().tryFixed(); fixedSpecifiedLineHeight && fixedSpecifiedLineHeight->resolveZoom(style.usedZoomForLength()) - style.specifiedFontSize() > largeMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText)
            return true;
        return false;
    }

    if (auto& backgroundLayers = style.backgroundLayers(); Style::hasImageInAnyLayer(backgroundLayers) && backgroundLayers.usedFirst().repeat() == FillRepeat::NoRepeat)
        return false;

    return true;
}

bool AutosizeStatus::probablyContainsASmallFixedNumberOfLines(const RenderStyle& style)
{
    auto& lineHeightAsLength = style.specifiedLineHeight();
    auto lineHeightAsFixed = lineHeightAsLength.tryFixed();
    auto lineHeightAsPercentage = lineHeightAsLength.tryPercentage();
    if (!lineHeightAsFixed && !lineHeightAsPercentage)
        return false;

    auto zoomFactor = style.usedZoomForLength();
    auto& maxHeight = style.maxHeight();
    std::optional<float> heightOrMaxHeightAsLength;
    if (auto fixedMaxHeight = maxHeight.tryFixed())
        heightOrMaxHeightAsLength = fixedMaxHeight->resolveZoom(zoomFactor);
    else if (auto fixedHeight = style.height().tryFixed(); fixedHeight && (!maxHeight.isSpecified() || maxHeight.isNone()))
        heightOrMaxHeightAsLength = fixedHeight->resolveZoom(zoomFactor);

    if (!heightOrMaxHeightAsLength)
        return false;

    float heightOrMaxHeight = *heightOrMaxHeightAsLength;
    if (heightOrMaxHeight <= 0)
        return false;

    float approximateLineHeight = lineHeightAsPercentage ? lineHeightAsPercentage->value * style.specifiedFontSize() / 100 : lineHeightAsFixed->resolveZoom(zoomFactor);
    if (approximateLineHeight <= 0)
        return false;

    float approximateNumberOfLines = heightOrMaxHeight / approximateLineHeight;
    if (auto integerLineClamp = style.lineClamp().tryInteger())
        return std::floor(approximateNumberOfLines) == integerLineClamp->value;

    constexpr auto maximumNumberOfLines = 5;
    constexpr auto thresholdForConsideringAnApproximateNumberOfLinesToBeCloseToAnInteger = 0.01f;

    return approximateNumberOfLines <= maximumNumberOfLines + thresholdForConsideringAnApproximateNumberOfLinesToBeCloseToAnInteger
        && approximateNumberOfLines - std::floor(approximateNumberOfLines) <= thresholdForConsideringAnApproximateNumberOfLinesToBeCloseToAnInteger;
}

float AutosizeStatus::idempotentTextSize(float specifiedSize, float pageScale)
{
    if (pageScale >= 1)
        return specifiedSize;

    // This describes a piecewise curve when the page scale is 2/3.
    static constexpr std::array points = {
        FloatPoint { 0.0f, 0.0f },
        FloatPoint { 6.0f, 9.0f },
        FloatPoint { 14.0f, 17.0f }
    };

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

    float result = scalePoint(points.back()).y();
    for (size_t i = 1; i < points.size(); ++i) {
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

} // namespace WebCore

#endif // ENABLE(TEXT_AUTOSIZING)
