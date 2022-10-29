/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InlineLineBoxVerticalAligner.h"

#include "InlineFormattingContext.h"
#include "LayoutBoxGeometry.h"

namespace WebCore {
namespace Layout {

LineBoxVerticalAligner::LineBoxVerticalAligner(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineFormattingGeometry(inlineFormattingContext)
{
}

InlineLayoutUnit LineBoxVerticalAligner::computeLogicalHeightAndAlign(LineBox& lineBox) const
{
    auto canUseSimplifiedAlignment = [&] {
        if (!lineBox.hasContent())
            return true;

        auto& rootInlineBox = lineBox.rootInlineBox();
        if (rootInlineBox.hasLineBoxContain())
            return false;
        if (!layoutState().inStandardsMode() || !rootInlineBox.isPreferredLineHeightFontMetricsBased() || rootInlineBox.verticalAlign().type != VerticalAlign::Baseline)
            return false;
        if (rootInlineBox.hasAnnotation())
            return false;

        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            auto shouldUseSimplifiedAlignmentForInlineLevelBox = [&] {
                if (inlineLevelBox.hasAnnotation())
                    return false;
                if (inlineLevelBox.isAtomicInlineLevelBox()) {
                    // Baseline aligned, non-stretchy direct children are considered to be simple for now.
                    auto& layoutBox = inlineLevelBox.layoutBox();
                    if (&layoutBox.parent() != &rootInlineBox.layoutBox() || inlineLevelBox.verticalAlign().type != VerticalAlign::Baseline)
                        return false;
                    auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
                    return !inlineLevelBoxGeometry.marginBefore() && !inlineLevelBoxGeometry.marginAfter() && inlineLevelBoxGeometry.marginBoxHeight() <= rootInlineBox.ascent();
                }
                if (inlineLevelBox.isLineBreakBox()) {
                    // Baseline aligned, non-stretchy line breaks e.g. <div><span><br></span></div> but not <div><span style="font-size: 100px;"><br></span></div>.
                    return inlineLevelBox.verticalAlign().type == VerticalAlign::Baseline && inlineLevelBox.ascent() <= rootInlineBox.ascent();
                }
                if (inlineLevelBox.isInlineBox()) {
                    // Baseline aligned, non-stretchy inline boxes e.g. <div><span></span></div> but not <div><span style="font-size: 100px;"></span></div>.
                    return inlineLevelBox.verticalAlign().type == VerticalAlign::Baseline && *inlineLevelBox.layoutBounds() == *rootInlineBox.layoutBounds();
                }
                return false;
            };
            if (!shouldUseSimplifiedAlignmentForInlineLevelBox())
                return false;
        }
        return true;
    };

    if (canUseSimplifiedAlignment())
        return simplifiedVerticalAlignment(lineBox);
    // This function (partially) implements:
    // 2.2. Layout Within Line Boxes
    // https://www.w3.org/TR/css-inline-3/#line-layout
    // 1. Compute the line box height using the layout bounds geometry. This height computation strictly uses layout bounds and not normal inline level box geometries.
    // 2. Compute the baseline/logical top position of the root inline box. Aligned boxes push the root inline box around inside the line box.
    // 3. Finally align the inline level boxes using (mostly) normal inline level box geometries.
    ASSERT(lineBox.hasContent());
    auto lineBoxAlignmentContent = computeLineBoxLogicalHeight(lineBox);
    computeRootInlineBoxVerticalPosition(lineBox, lineBoxAlignmentContent);

    auto lineBoxHeight = lineBoxAlignmentContent.height();
    alignInlineLevelBoxes(lineBox, lineBoxHeight);
    if (lineBoxAlignmentContent.hasAnnotation)
        lineBoxHeight = adjustForAnnotationIfNeeded(lineBox, lineBoxHeight);
    return lineBoxHeight;
}

InlineLayoutUnit LineBoxVerticalAligner::simplifiedVerticalAlignment(LineBox& lineBox) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto rootInlineBoxAscent = rootInlineBox.ascent();

    if (!lineBox.hasContent()) {
        rootInlineBox.setLogicalTop(-rootInlineBoxAscent);
        return { };
    }

    auto rootInlineBoxLayoutBounds = *rootInlineBox.layoutBounds();

    auto lineBoxLogicalTop = InlineLayoutUnit { 0 };
    auto lineBoxLogicalBottom = rootInlineBoxLayoutBounds.height();
    auto rootInlineBoxLogicalTop = rootInlineBoxLayoutBounds.ascent - rootInlineBoxAscent;
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        // Only baseline alignment for now.
        inlineLevelBox.setLogicalTop(rootInlineBoxAscent - inlineLevelBox.ascent());
        auto layoutBounds = inlineLevelBox.layoutBounds();
        auto inlineLevelBoxAscent = layoutBounds ? layoutBounds->ascent : inlineLevelBox.ascent();
        auto inlineLevelBoxHeight = layoutBounds ? layoutBounds->height() : inlineLevelBox.logicalHeight();

        auto layoutBoundsLogicalTop = rootInlineBoxLayoutBounds.ascent - inlineLevelBoxAscent;
        lineBoxLogicalTop = std::min(lineBoxLogicalTop, layoutBoundsLogicalTop);
        lineBoxLogicalBottom = std::max(lineBoxLogicalBottom, layoutBoundsLogicalTop + inlineLevelBoxHeight);
        rootInlineBoxLogicalTop = std::max(rootInlineBoxLogicalTop, inlineLevelBoxAscent - rootInlineBoxAscent);
    }
    rootInlineBox.setLogicalTop(rootInlineBoxLogicalTop);
    return lineBoxLogicalBottom - lineBoxLogicalTop;
}

LineBoxVerticalAligner::LineBoxAlignmentContent LineBoxVerticalAligner::computeLineBoxLogicalHeight(LineBox& lineBox) const
{
    // This function (partially) implements:
    // 2.2. Layout Within Line Boxes
    // https://www.w3.org/TR/css-inline-3/#line-layout
    // 1. Compute the line box height using the layout bounds geometry. This height computation strictly uses layout bounds and not normal inline level box geometries.
    // 2. Compute the baseline/logical top position of the root inline box. Aligned boxes push the root inline box around inside the line box.
    // 3. Finally align the inline level boxes using (mostly) normal inline level box geometries.
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto& formattingGeometry = this->formattingGeometry();
    auto contentHasAnnotation = rootInlineBox.hasAnnotation();

    // Line box height computation is based on the layout bounds of the inline boxes and not their logical (ascent/descent) dimensions.
    struct AbsoluteTopAndBottom {
        InlineLayoutUnit top { 0 };
        InlineLayoutUnit bottom { 0 };
    };
    HashMap<const InlineLevelBox*, AbsoluteTopAndBottom> inlineLevelBoxAbsoluteTopAndBottomMap;

    auto minimumLogicalTop = std::optional<InlineLayoutUnit> { };
    auto maximumLogicalBottom = std::optional<InlineLayoutUnit> { };
    if (formattingGeometry.inlineLevelBoxAffectsLineBox(rootInlineBox, lineBox)) {
        minimumLogicalTop = InlineLayoutUnit { };
        maximumLogicalBottom = rootInlineBox.layoutBounds()->height();
        inlineLevelBoxAbsoluteTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { *minimumLogicalTop, *maximumLogicalBottom });
    } else
        inlineLevelBoxAbsoluteTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { });

    Vector<InlineLevelBox*> lineBoxRelativeInlineLevelBoxes;
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        auto& layoutBox = inlineLevelBox.layoutBox();
        contentHasAnnotation = contentHasAnnotation || inlineLevelBox.hasAnnotation();

        if (inlineLevelBox.hasLineBoxRelativeAlignment()) {
            lineBoxRelativeInlineLevelBoxes.append(&inlineLevelBox);
            continue;
        }
        auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
        // Logical top is relative to the parent inline box's layout bounds.
        // Note that this logical top is not the final logical top of the inline level box.
        // This is the logical top in the context of the layout bounds geometry which may be very different from the inline box's normal geometry.
        auto logicalTop = InlineLayoutUnit { };

        auto verticalAlign = inlineLevelBox.verticalAlign();
        auto layoutBounds = inlineLevelBox.layoutBounds();
        auto logicalHeight = layoutBounds ? layoutBounds->height() : inlineLevelBox.logicalHeight();
        auto ascent = layoutBounds ? layoutBounds->ascent : inlineLevelBox.ascent();
        auto parentInlineBoxAscent = parentInlineBox.layoutBounds()->ascent;

        switch (verticalAlign.type) {
        case VerticalAlign::Baseline:
            logicalTop = parentInlineBoxAscent - ascent;
            break;
        case VerticalAlign::Middle: {
            auto logicalTopOffsetFromParentBaseline = logicalHeight / 2 + parentInlineBox.primarymetricsOfPrimaryFont().xHeight() / 2;
            logicalTop = parentInlineBoxAscent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::BaselineMiddle: {
            auto logicalTopOffsetFromParentBaseline = logicalHeight / 2;
            logicalTop = parentInlineBoxAscent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Length: {
            auto logicalTopOffsetFromParentBaseline = *verticalAlign.baselineOffset + ascent;
            logicalTop = parentInlineBoxAscent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::TextTop: {
            // Note that text-top aligns with the inline box's font metrics top (ascent) and not the layout bounds top.
            logicalTop = parentInlineBoxAscent - parentInlineBox.ascent();
            break;
        }
        case VerticalAlign::TextBottom: {
            // Note that text-bottom aligns with the inline box's font metrics bottom (descent) and not the layout bounds bottom.
            auto parentInlineBoxLayoutBounds = *parentInlineBox.layoutBounds();
            auto parentInlineBoxLogicalBottom = parentInlineBoxLayoutBounds.height() - parentInlineBoxLayoutBounds.descent + valueOrDefault(parentInlineBox.descent());
            logicalTop = parentInlineBoxLogicalBottom - logicalHeight;
            break;
        }
        case VerticalAlign::Sub: {
            auto logicalTopOffsetFromParentBaseline = ascent - (parentInlineBox.fontSize() / 5 + 1);
            logicalTop = parentInlineBoxAscent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Super: {
            auto logicalTopOffsetFromParentBaseline = ascent + parentInlineBox.fontSize() / 3 + 1;
            logicalTop = parentInlineBoxAscent - logicalTopOffsetFromParentBaseline;
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        auto parentInlineBoxAbsoluteTopAndBottom = inlineLevelBoxAbsoluteTopAndBottomMap.get(&parentInlineBox);
        auto absoluteLogicalTop = parentInlineBoxAbsoluteTopAndBottom.top + logicalTop;
        auto absoluteLogicalBottom = absoluteLogicalTop + logicalHeight;
        inlineLevelBoxAbsoluteTopAndBottomMap.add(&inlineLevelBox, AbsoluteTopAndBottom { absoluteLogicalTop, absoluteLogicalBottom });
        // Stretch the min/max absolute values if applicable.
        if (formattingGeometry.inlineLevelBoxAffectsLineBox(inlineLevelBox, lineBox)) {
            minimumLogicalTop = std::min(minimumLogicalTop.value_or(absoluteLogicalTop), absoluteLogicalTop);
            maximumLogicalBottom = std::max(maximumLogicalBottom.value_or(absoluteLogicalBottom), absoluteLogicalBottom);
        }
    }
    // The line box height computation is as follows:
    // 1. Stretch the line box with the non-line-box relative aligned inline box absolute top and bottom values.
    // 2. Check if the line box relative aligned inline boxes (top, bottom etc) have enough room and stretch the line box further if needed.
    auto nonBottomAlignedBoxesMaximumHeight = valueOrDefault(maximumLogicalBottom) - valueOrDefault(minimumLogicalTop);
    auto bottomAlignedBoxesMaximumHeight = std::optional<InlineLayoutUnit> { };
    for (auto* lineBoxRelativeInlineLevelBox : lineBoxRelativeInlineLevelBoxes) {
        if (!formattingGeometry.inlineLevelBoxAffectsLineBox(*lineBoxRelativeInlineLevelBox, lineBox))
            continue;
        // This line box relative aligned inline level box stretches the line box.
        auto inlineLevelBoxHeight = lineBoxRelativeInlineLevelBox->layoutBounds() ? lineBoxRelativeInlineLevelBox->layoutBounds()->height() : lineBoxRelativeInlineLevelBox->logicalHeight();
        if (lineBoxRelativeInlineLevelBox->verticalAlign().type == VerticalAlign::Top) {
            nonBottomAlignedBoxesMaximumHeight = std::max(inlineLevelBoxHeight, nonBottomAlignedBoxesMaximumHeight);
            continue;
        }
        if (lineBoxRelativeInlineLevelBox->verticalAlign().type == VerticalAlign::Bottom) {
            bottomAlignedBoxesMaximumHeight = std::max(inlineLevelBoxHeight, bottomAlignedBoxesMaximumHeight.value_or(0.f));
            continue;
        }
        ASSERT_NOT_REACHED();
    }
    return { nonBottomAlignedBoxesMaximumHeight, bottomAlignedBoxesMaximumHeight, contentHasAnnotation };
}

void LineBoxVerticalAligner::computeRootInlineBoxVerticalPosition(LineBox& lineBox, const LineBoxAlignmentContent& lineBoxAlignmentContent) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto& formattingGeometry = this->formattingGeometry();
    auto hasTopAlignedInlineLevelBox = false;

    HashMap<const InlineLevelBox*, InlineLayoutUnit> inlineLevelBoxAbsoluteBaselineOffsetMap;
    inlineLevelBoxAbsoluteBaselineOffsetMap.add(&rootInlineBox, InlineLayoutUnit { });

    auto maximumTopOffsetFromRootInlineBoxBaseline = std::optional<InlineLayoutUnit> { };
    if (formattingGeometry.inlineLevelBoxAffectsLineBox(rootInlineBox, lineBox))
        maximumTopOffsetFromRootInlineBoxBaseline = rootInlineBox.layoutBounds()->ascent;

    auto affectsRootInlineBoxVerticalPosition = [&](auto& inlineLevelBox) {
        auto inlineLevelBoxStrechesLineBox = formattingGeometry.inlineLevelBoxAffectsLineBox(inlineLevelBox, lineBox);
        return inlineLevelBoxStrechesLineBox || (inlineLevelBox.isAtomicInlineLevelBox() && inlineLevelBox.ascent());
    };

    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        auto verticalAlign = inlineLevelBox.verticalAlign();
        auto layoutBounds = inlineLevelBox.layoutBounds();
        auto logicalHeight = layoutBounds ? layoutBounds->height() : inlineLevelBox.logicalHeight();
        auto ascent = layoutBounds ? layoutBounds->ascent : inlineLevelBox.ascent();
        auto descent = layoutBounds ? layoutBounds->descent : inlineLevelBox.descent().value_or(0.f);

        if (inlineLevelBox.hasLineBoxRelativeAlignment()) {
            if (verticalAlign.type == VerticalAlign::Top) {
                hasTopAlignedInlineLevelBox = hasTopAlignedInlineLevelBox || affectsRootInlineBoxVerticalPosition(inlineLevelBox);
                inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, rootInlineBox.layoutBounds()->ascent - ascent);
            } else if (verticalAlign.type == VerticalAlign::Bottom)
                inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, descent - rootInlineBox.layoutBounds()->descent);
            else
                ASSERT_NOT_REACHED();
            continue;
        }
        auto& layoutBox = inlineLevelBox.layoutBox();
        auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
        auto baselineOffsetFromParentBaseline = InlineLayoutUnit { };

        switch (verticalAlign.type) {
        case VerticalAlign::Baseline:
            baselineOffsetFromParentBaseline = { };
            break;
        case VerticalAlign::Middle: {
            auto logicalTopOffsetFromParentBaseline = (logicalHeight / 2 + parentInlineBox.primarymetricsOfPrimaryFont().xHeight() / 2);
            baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - ascent;
            break;
        }
        case VerticalAlign::BaselineMiddle: {
            auto logicalTopOffsetFromParentBaseline = logicalHeight / 2;
            baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - ascent;
            break;
        }
        case VerticalAlign::Length: {
            auto logicalTopOffsetFromParentBaseline = *verticalAlign.baselineOffset + inlineLevelBox.ascent();
            baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox.ascent();
            break;
        }
        case VerticalAlign::TextTop:
            baselineOffsetFromParentBaseline = parentInlineBox.ascent() - ascent;
            break;
        case VerticalAlign::TextBottom:
            baselineOffsetFromParentBaseline = descent - *parentInlineBox.descent();
            break;
        case VerticalAlign::Sub:
            baselineOffsetFromParentBaseline = -(parentInlineBox.fontSize() / 5 + 1);
            break;
        case VerticalAlign::Super:
            baselineOffsetFromParentBaseline = parentInlineBox.fontSize() / 3 + 1;
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        auto absoluteBaselineOffset = inlineLevelBoxAbsoluteBaselineOffsetMap.get(&parentInlineBox) + baselineOffsetFromParentBaseline;
        inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, absoluteBaselineOffset);

        if (affectsRootInlineBoxVerticalPosition(inlineLevelBox)) {
            auto topOffsetFromRootInlineBoxBaseline = absoluteBaselineOffset + ascent;
            if (maximumTopOffsetFromRootInlineBoxBaseline)
                maximumTopOffsetFromRootInlineBoxBaseline = std::max(*maximumTopOffsetFromRootInlineBoxBaseline, topOffsetFromRootInlineBoxBaseline);
            else {
                // We are is quirk mode and the root inline box has no content. The root inline box's baseline is anchored at 0.
                // However negative ascent (e.g negative top margin) can "push" the root inline box upwards and have a negative value.
                maximumTopOffsetFromRootInlineBoxBaseline = ascent >= 0
                    ? std::max(0.0f, topOffsetFromRootInlineBoxBaseline)
                    : topOffsetFromRootInlineBoxBaseline;
            }
        }
    }

    if (!maximumTopOffsetFromRootInlineBoxBaseline && hasTopAlignedInlineLevelBox) {
        // vertical-align: top is a line box relative alignment. It stretches the line box downwards meaning that it does not affect
        // the root inline box's baseline position, but in quirks mode we have to ensure that the root inline box does not end up at 0px.
        maximumTopOffsetFromRootInlineBoxBaseline = rootInlineBox.ascent();
    }
    // vertical-align: bottom stretches the top of the line box pushing the root inline box downwards.
    auto bottomAlignedBoxStretch = InlineLayoutUnit { };
    if (lineBoxAlignmentContent.bottomAlignedBoxesMaximumHeight) {
        // Negative vertical margin can make aligned boxes have negative height. 
        bottomAlignedBoxStretch = std::max(0.f, lineBoxAlignmentContent.nonBottomAlignedBoxesMaximumHeight < 0
            ? *lineBoxAlignmentContent.bottomAlignedBoxesMaximumHeight
            : std::max(0.f, *lineBoxAlignmentContent.bottomAlignedBoxesMaximumHeight - lineBoxAlignmentContent.nonBottomAlignedBoxesMaximumHeight));
    }
    auto rootInlineBoxLogicalTop = bottomAlignedBoxStretch + maximumTopOffsetFromRootInlineBoxBaseline.value_or(0.f) - rootInlineBox.ascent();
    rootInlineBox.setLogicalTop(rootInlineBoxLogicalTop);
}

void LineBoxVerticalAligner::alignInlineLevelBoxes(LineBox& lineBox, InlineLayoutUnit lineBoxLogicalHeight) const
{
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        auto& layoutBox = inlineLevelBox.layoutBox();
        auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
        auto logicalTop = InlineLayoutUnit { };
        auto verticalAlign = inlineLevelBox.verticalAlign();

        switch (verticalAlign.type) {
        case VerticalAlign::Baseline:
            logicalTop = parentInlineBox.ascent() - inlineLevelBox.ascent();
            break;
        case VerticalAlign::Middle: {
            auto logicalTopOffsetFromParentBaseline = (inlineLevelBox.logicalHeight() / 2 + parentInlineBox.primarymetricsOfPrimaryFont().xHeight() / 2);
            logicalTop = parentInlineBox.ascent() - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::BaselineMiddle: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.logicalHeight() / 2;
            logicalTop = parentInlineBox.ascent() - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Length: {
            auto logicalTopOffsetFromParentBaseline = *verticalAlign.baselineOffset + inlineLevelBox.ascent();
            logicalTop = parentInlineBox.ascent() - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Sub: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.ascent() - (parentInlineBox.fontSize() / 5 + 1);
            logicalTop = parentInlineBox.ascent() - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Super: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.ascent() + parentInlineBox.fontSize() / 3 + 1;
            logicalTop = parentInlineBox.ascent() - logicalTopOffsetFromParentBaseline;
            break;
        }
        // Note that (text)top/bottom align their layout bounds.
        case VerticalAlign::TextTop: {
            auto ascent = inlineLevelBox.layoutBounds() ? inlineLevelBox.layoutBounds()->ascent : inlineLevelBox.ascent();
            logicalTop = ascent - inlineLevelBox.ascent();
            break;
        }
        case VerticalAlign::TextBottom: {
            auto descent = inlineLevelBox.layoutBounds() ? inlineLevelBox.layoutBounds()->descent : inlineLevelBox.descent().value_or(0.f);
            logicalTop = parentInlineBox.logicalHeight() - descent - inlineLevelBox.ascent();
            break;
        }
        case VerticalAlign::Top: {
            auto ascent = inlineLevelBox.layoutBounds() ? inlineLevelBox.layoutBounds()->ascent : inlineLevelBox.ascent();
            // Note that this logical top is not relative to the parent inline box.
            logicalTop = ascent - inlineLevelBox.ascent();
            break;
        }
        case VerticalAlign::Bottom: {
            auto descent = inlineLevelBox.layoutBounds() ? inlineLevelBox.layoutBounds()->descent : inlineLevelBox.descent().value_or(0.f);
            // Note that this logical top is not relative to the parent inline box.
            logicalTop = lineBoxLogicalHeight - (inlineLevelBox.ascent() + descent);
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        inlineLevelBox.setLogicalTop(logicalTop);
    }
}

InlineLayoutUnit LineBoxVerticalAligner::adjustForAnnotationIfNeeded(LineBox& lineBox, InlineLayoutUnit lineBoxHeight) const
{
    auto lineBoxTop = InlineLayoutUnit { };
    auto lineBoxBottom = lineBoxHeight;
    // At this point we have a properly aligned set of inline level boxes. Let's find out if annotation marks have enough space.
    auto adjustLineBoxHeightIfNeeded = [&] {
        auto adjustLineBoxTopAndBottomForInlineBox = [&](const InlineLevelBox& inlineBox) {
            ASSERT(inlineBox.isInlineBox());
            auto inlineBoxTop = lineBox.inlineLevelBoxAbsoluteTop(inlineBox);
            auto inlineBoxBottom = inlineBoxTop + inlineBox.logicalHeight();

            switch (inlineBox.verticalAlign().type) {
            case VerticalAlign::Baseline:
            case VerticalAlign::Middle:
            case VerticalAlign::BaselineMiddle:
            case VerticalAlign::Length:
            case VerticalAlign::Sub:
            case VerticalAlign::Super:
            case VerticalAlign::TextTop:
            case VerticalAlign::TextBottom:
            case VerticalAlign::Bottom:
                if (auto aboveSpace = inlineBox.annotationAbove())
                    lineBoxTop = std::min(lineBoxTop, inlineBoxTop - *aboveSpace);
                else if (auto underSpace = inlineBox.annotationUnder())
                    lineBoxBottom = std::max(lineBoxBottom, inlineBoxBottom + *underSpace);
                break;
            case VerticalAlign::Top: {
                // FIXME: Check if horizontal vs. vertical writing mode should be taking into account.
                auto annotationSpace = inlineBox.annotationAbove().value_or(0.f) + inlineBox.annotationUnder().value_or(0.f); 
                lineBoxBottom = std::max(lineBoxBottom, inlineBoxBottom + annotationSpace);
                break;
            }
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
        };

        adjustLineBoxTopAndBottomForInlineBox(lineBox.rootInlineBox());
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (inlineLevelBox.isInlineBox())
                adjustLineBoxTopAndBottomForInlineBox(inlineLevelBox);
        }

        return lineBoxBottom - lineBoxTop;
    };
    auto adjustedLineBoxHeight = adjustLineBoxHeightIfNeeded();

    if (lineBoxHeight != adjustedLineBoxHeight) {
        // Annotations needs some space.
        auto adjustContentTopWithAnnotationSpace = [&] {
            auto& rootInlineBox = lineBox.rootInlineBox();
            auto rootInlineBoxTop = rootInlineBox.logicalTop();
            auto annotationOffset = -lineBoxTop;
            rootInlineBox.setLogicalTop(annotationOffset + rootInlineBoxTop);

            for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
                switch (inlineLevelBox.verticalAlign().type) {
                case VerticalAlign::Top: {
                    auto inlineBoxTop = InlineLayoutUnit { };
                    if (auto layoutBounds = inlineLevelBox.layoutBounds())
                        inlineBoxTop = layoutBounds->ascent - inlineLevelBox.ascent();
                    inlineLevelBox.setLogicalTop(inlineLevelBox.annotationAbove().value_or(0.f) + inlineBoxTop);
                    break;
                }
                case VerticalAlign::Bottom: {
                    auto inlineBoxTop = adjustedLineBoxHeight - inlineLevelBox.ascent();
                    if (auto layoutBounds = inlineLevelBox.layoutBounds())
                        inlineBoxTop -= layoutBounds->descent;
                    inlineLevelBox.setLogicalTop(inlineBoxTop - inlineLevelBox.annotationUnder().value_or(0.f));
                    break;
                }
                default:
                    // These alignment positions are relative to the root inline box's baseline.
                    break;
                }
            }
        };
        adjustContentTopWithAnnotationSpace();
    }
    return adjustedLineBoxHeight;
}

}
}

