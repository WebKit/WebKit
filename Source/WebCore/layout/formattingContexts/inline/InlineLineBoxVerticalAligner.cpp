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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

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
        if (!layoutState().inStandardsMode() || !rootInlineBox.isPreferredLineHeightFontMetricsBased() || rootInlineBox.verticalAlign().type != VerticalAlign::Baseline)
            return false;

        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            auto shouldUseSimplifiedAlignment = false;
            if (inlineLevelBox.isAtomicInlineLevelBox()) {
                // Baseline aligned, non-stretchy direct children are considered to be simple for now.
                auto& layoutBox = inlineLevelBox.layoutBox();
                if (&layoutBox.parent() != &rootInlineBox.layoutBox() || inlineLevelBox.verticalAlign().type != VerticalAlign::Baseline)
                    shouldUseSimplifiedAlignment = false;
                else {
                    auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
                    shouldUseSimplifiedAlignment = !inlineLevelBoxGeometry.marginBefore() && !inlineLevelBoxGeometry.marginAfter() && inlineLevelBoxGeometry.marginBoxHeight() <= rootInlineBox.ascent();
                }
            } else if (inlineLevelBox.isLineBreakBox()) {
                // Baseline aligned, non-stretchy line breaks e.g. <div><span><br></span></div> but not <div><span style="font-size: 100px;"><br></span></div>.
                shouldUseSimplifiedAlignment = inlineLevelBox.verticalAlign().type == VerticalAlign::Baseline && inlineLevelBox.ascent() <= rootInlineBox.ascent();
            } else if (inlineLevelBox.isInlineBox()) {
                // Baseline aligned, non-stretchy inline boxes e.g. <div><span></span></div> but not <div><span style="font-size: 100px;"></span></div>.
                shouldUseSimplifiedAlignment = inlineLevelBox.verticalAlign().type == VerticalAlign::Baseline && inlineLevelBox.layoutBounds() == rootInlineBox.layoutBounds();
            }
            if (!shouldUseSimplifiedAlignment)
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
    auto lineBoxLogicalHeight = computeLineBoxLogicalHeight(lineBox);
    computeRootInlineBoxVerticalPosition(lineBox, lineBoxLogicalHeight);
    alignInlineLevelBoxes(lineBox, lineBoxLogicalHeight.value());
    return lineBoxLogicalHeight.value();
}

InlineLayoutUnit LineBoxVerticalAligner::simplifiedVerticalAlignment(LineBox& lineBox) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto rootInlineBoxAscent = rootInlineBox.ascent();

    if (!lineBox.hasContent()) {
        rootInlineBox.setLogicalTop(-rootInlineBoxAscent);
        return { };
    }

    auto rootInlineBoxLayoutBounds = rootInlineBox.layoutBounds();

    auto lineBoxLogicalTop = InlineLayoutUnit { 0 };
    auto lineBoxLogicalBottom = rootInlineBoxLayoutBounds.height();
    auto rootInlineBoxLogicalTop = rootInlineBoxLayoutBounds.ascent - rootInlineBoxAscent;
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        // Only baseline alignment for now.
        inlineLevelBox.setLogicalTop(rootInlineBoxAscent - inlineLevelBox.ascent());
        auto layoutBounds = inlineLevelBox.layoutBounds();

        auto layoutBoundsLogicalTop = rootInlineBoxLayoutBounds.ascent - layoutBounds.ascent;
        lineBoxLogicalTop = std::min(lineBoxLogicalTop, layoutBoundsLogicalTop);
        lineBoxLogicalBottom = std::max(lineBoxLogicalBottom, layoutBoundsLogicalTop + layoutBounds.height());
        rootInlineBoxLogicalTop = std::max(rootInlineBoxLogicalTop, layoutBounds.ascent - rootInlineBoxAscent);
    }
    rootInlineBox.setLogicalTop(rootInlineBoxLogicalTop);
    return lineBoxLogicalBottom - lineBoxLogicalTop;
}

LineBoxVerticalAligner::LineBoxHeight LineBoxVerticalAligner::computeLineBoxLogicalHeight(LineBox& lineBox) const
{
    // This function (partially) implements:
    // 2.2. Layout Within Line Boxes
    // https://www.w3.org/TR/css-inline-3/#line-layout
    // 1. Compute the line box height using the layout bounds geometry. This height computation strictly uses layout bounds and not normal inline level box geometries.
    // 2. Compute the baseline/logical top position of the root inline box. Aligned boxes push the root inline box around inside the line box.
    // 3. Finally align the inline level boxes using (mostly) normal inline level box geometries.
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto& formattingGeometry = this->formattingGeometry();

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
        maximumLogicalBottom = rootInlineBox.layoutBounds().height();
        inlineLevelBoxAbsoluteTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { *minimumLogicalTop, *maximumLogicalBottom });
    } else
        inlineLevelBoxAbsoluteTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { });

    Vector<InlineLevelBox*> lineBoxRelativeInlineLevelBoxes;
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        auto& layoutBox = inlineLevelBox.layoutBox();
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
        switch (verticalAlign.type) {
        case VerticalAlign::Baseline: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().ascent;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Middle: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().height() / 2 + parentInlineBox.primarymetricsOfPrimaryFont().xHeight() / 2;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::BaselineMiddle: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().height() / 2;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Length: {
            auto logicalTopOffsetFromParentBaseline = *verticalAlign.baselineOffset + inlineLevelBox.layoutBounds().ascent;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::TextTop: {
            // Note that text-top aligns with the inline box's font metrics top (ascent) and not the layout bounds top.
            logicalTop = parentInlineBox.layoutBounds().ascent - parentInlineBox.ascent();
            break;
        }
        case VerticalAlign::TextBottom: {
            // Note that text-bottom aligns with the inline box's font metrics bottom (descent) and not the layout bounds bottom.
            auto parentInlineBoxLayoutBounds = parentInlineBox.layoutBounds();
            auto parentInlineBoxLogicalBottom = parentInlineBoxLayoutBounds.height() - parentInlineBoxLayoutBounds.descent + valueOrDefault(parentInlineBox.descent());
            logicalTop = parentInlineBoxLogicalBottom - inlineLevelBox.layoutBounds().height();
            break;
        }
        case VerticalAlign::Sub: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().ascent - (parentInlineBox.fontSize() / 5 + 1);
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Super: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().ascent + parentInlineBox.fontSize() / 3 + 1;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        auto parentInlineBoxAbsoluteTopAndBottom = inlineLevelBoxAbsoluteTopAndBottomMap.get(&parentInlineBox);
        auto absoluteLogicalTop = parentInlineBoxAbsoluteTopAndBottom.top + logicalTop;
        auto absoluteLogicalBottom = absoluteLogicalTop + inlineLevelBox.layoutBounds().height();
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
        auto inlineLevelBoxHeight = lineBoxRelativeInlineLevelBox->layoutBounds().height();
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
    return { nonBottomAlignedBoxesMaximumHeight, bottomAlignedBoxesMaximumHeight };
}

void LineBoxVerticalAligner::computeRootInlineBoxVerticalPosition(LineBox& lineBox, const LineBoxHeight& lineBoxHeight) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto& formattingGeometry = this->formattingGeometry();
    auto hasTopAlignedInlineLevelBox = false;

    HashMap<const InlineLevelBox*, InlineLayoutUnit> inlineLevelBoxAbsoluteBaselineOffsetMap;
    inlineLevelBoxAbsoluteBaselineOffsetMap.add(&rootInlineBox, InlineLayoutUnit { });

    auto maximumTopOffsetFromRootInlineBoxBaseline = std::optional<InlineLayoutUnit> { };
    if (formattingGeometry.inlineLevelBoxAffectsLineBox(rootInlineBox, lineBox))
        maximumTopOffsetFromRootInlineBoxBaseline = rootInlineBox.layoutBounds().ascent;

    auto affectsRootInlineBoxVerticalPosition = [&](auto& inlineLevelBox) {
        auto inlineLevelBoxStrechesLineBox = formattingGeometry.inlineLevelBoxAffectsLineBox(inlineLevelBox, lineBox);
        return inlineLevelBoxStrechesLineBox || (inlineLevelBox.isAtomicInlineLevelBox() && inlineLevelBox.ascent());
    };

    auto& nonRootInlineLevelBoxes = lineBox.nonRootInlineLevelBoxes();
    for (size_t index = 0; index < nonRootInlineLevelBoxes.size(); ++index) {
        auto& inlineLevelBox = nonRootInlineLevelBoxes[index];
        auto verticalAlign = inlineLevelBox.verticalAlign();

        if (inlineLevelBox.hasLineBoxRelativeAlignment()) {
            if (verticalAlign.type == VerticalAlign::Top) {
                hasTopAlignedInlineLevelBox = hasTopAlignedInlineLevelBox || affectsRootInlineBoxVerticalPosition(inlineLevelBox);
                inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, rootInlineBox.layoutBounds().ascent - inlineLevelBox.layoutBounds().ascent);
            } else if (verticalAlign.type == VerticalAlign::Bottom)
                inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, inlineLevelBox.layoutBounds().descent - rootInlineBox.layoutBounds().descent);
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
            auto logicalTopOffsetFromParentBaseline = (inlineLevelBox.layoutBounds().height() / 2 + parentInlineBox.primarymetricsOfPrimaryFont().xHeight() / 2);
            baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox.layoutBounds().ascent;
            break;
        }
        case VerticalAlign::BaselineMiddle: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().height() / 2;
            baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox.layoutBounds().ascent;
            break;
        }
        case VerticalAlign::Length: {
            auto logicalTopOffsetFromParentBaseline = *verticalAlign.baselineOffset + inlineLevelBox.ascent();
            baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox.ascent();
            break;
        }
        case VerticalAlign::TextTop:
            baselineOffsetFromParentBaseline = parentInlineBox.ascent() - inlineLevelBox.layoutBounds().ascent;
            break;
        case VerticalAlign::TextBottom:
            baselineOffsetFromParentBaseline = inlineLevelBox.layoutBounds().descent - *parentInlineBox.descent();
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
            auto topOffsetFromRootInlineBoxBaseline = absoluteBaselineOffset + inlineLevelBox.layoutBounds().ascent;
            if (maximumTopOffsetFromRootInlineBoxBaseline)
                maximumTopOffsetFromRootInlineBoxBaseline = std::max(*maximumTopOffsetFromRootInlineBoxBaseline, topOffsetFromRootInlineBoxBaseline);
            else {
                // We are is quirk mode and the root inline box has no content. The root inline box's baseline is anchored at 0.
                // However negative ascent (e.g negative top margin) can "push" the root inline box upwards and have a negative value.
                maximumTopOffsetFromRootInlineBoxBaseline = inlineLevelBox.layoutBounds().ascent >= 0
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
    if (lineBoxHeight.bottomAlignedBoxesMaximumHeight) {
        // Negative vertical margin can make aligned boxes have negative height. 
        bottomAlignedBoxStretch = std::max(0.f, lineBoxHeight.nonBottomAlignedBoxesMaximumHeight < 0
            ? *lineBoxHeight.bottomAlignedBoxesMaximumHeight
            : std::max(0.f, *lineBoxHeight.bottomAlignedBoxesMaximumHeight - lineBoxHeight.nonBottomAlignedBoxesMaximumHeight));
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
        case VerticalAlign::TextTop:
            logicalTop = inlineLevelBox.layoutBounds().ascent - inlineLevelBox.ascent();
            break;
        case VerticalAlign::TextBottom:
            logicalTop = parentInlineBox.logicalHeight() - inlineLevelBox.layoutBounds().descent - inlineLevelBox.ascent();
            break;
        case VerticalAlign::Top:
            // Note that this logical top is not relative to the parent inline box.
            logicalTop = inlineLevelBox.layoutBounds().ascent - inlineLevelBox.ascent();
            break;
        case VerticalAlign::Bottom:
            // Note that this logical top is not relative to the parent inline box.
            logicalTop = lineBoxLogicalHeight - inlineLevelBox.layoutBounds().descent - inlineLevelBox.ascent();
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        inlineLevelBox.setLogicalTop(logicalTop);
    }
}

}
}

#endif
