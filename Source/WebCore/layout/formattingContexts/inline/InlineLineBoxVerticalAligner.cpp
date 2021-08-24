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

#include "FontCascade.h"
#include "InlineFormattingContext.h"
#include "LayoutBoxGeometry.h"

namespace WebCore {
namespace Layout {

LineBoxVerticalAligner::LineBoxVerticalAligner(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingGeometry(inlineFormattingContext)
{
}

bool LineBoxVerticalAligner::canUseSimplifiedAlignmentForInlineLevelBox(const InlineLevelBox& rootInlineBox, const InlineLevelBox& inlineLevelBox, std::optional<const BoxGeometry> inlineLevelBoxGeometry)
{
    if (inlineLevelBox.isAtomicInlineLevelBox()) {
        ASSERT(inlineLevelBoxGeometry);
        // Baseline aligned, non-stretchy direct children are considered to be simple for now.
        auto& layoutBox = inlineLevelBox.layoutBox();
        return &layoutBox.parent() == &rootInlineBox.layoutBox()
            && layoutBox.style().verticalAlign() == VerticalAlign::Baseline
            && !inlineLevelBoxGeometry->marginBefore()
            && !inlineLevelBoxGeometry->marginAfter()
            && inlineLevelBoxGeometry->marginBoxHeight() <= rootInlineBox.baseline();
    }
    if (inlineLevelBox.isLineBreakBox()) {
        // Baseline aligned, non-stretchy line breaks e.g. <div><span><br></span></div> but not <div><span style="font-size: 100px;"><br></span></div>.
        return inlineLevelBox.layoutBox().style().verticalAlign() == VerticalAlign::Baseline && inlineLevelBox.baseline() <= rootInlineBox.baseline();
    }
    if (inlineLevelBox.isInlineBox()) {
        // Baseline aligned, non-stretchy inline boxes e.g. <div><span></span></div> but not <div><span style="font-size: 100px;"></span></div>.
        return inlineLevelBox.layoutBox().style().verticalAlign() == VerticalAlign::Baseline && inlineLevelBox.layoutBounds() == rootInlineBox.layoutBounds();
    }
    return false;
}

InlineLayoutUnit LineBoxVerticalAligner::computeLogicalHeightAndAlign(LineBox& lineBox, bool useSimplifiedAlignment) const
{
    if (useSimplifiedAlignment)
        return simplifiedVerticalAlignment(lineBox);
    // This function (partially) implements:
    // 2.2. Layout Within Line Boxes
    // https://www.w3.org/TR/css-inline-3/#line-layout
    // 1. Compute the line box height using the layout bounds geometry. This height computation strictly uses layout bounds and not normal inline level box geometries.
    // 2. Compute the baseline/logical top position of the root inline box. Aligned boxes push the root inline box around inside the line box.
    // 3. Finally align the inline level boxes using (mostly) normal inline level box geometries.
    ASSERT(lineBox.hasContent());
    auto lineBoxLogicalHeight = computeLineBoxLogicalHeight(lineBox);
    computeRootInlineBoxVerticalPosition(lineBox);
    alignInlineLevelBoxes(lineBox, lineBoxLogicalHeight);
    return lineBoxLogicalHeight;
}

InlineLayoutUnit LineBoxVerticalAligner::simplifiedVerticalAlignment(LineBox& lineBox) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto rootInlineBoxBaseline = rootInlineBox.baseline();

    if (!lineBox.hasContent()) {
        rootInlineBox.setLogicalTop(-rootInlineBoxBaseline);
        return { };
    }

    auto rootInlineBoxLayoutBounds = rootInlineBox.layoutBounds();

    auto lineBoxLogicalTop = InlineLayoutUnit { 0 };
    auto lineBoxLogicalBottom = rootInlineBoxLayoutBounds.height();
    auto rootInlineBoxLogicalTop = rootInlineBoxLayoutBounds.ascent - rootInlineBoxBaseline;
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        // Only baseline alignment for now.
        inlineLevelBox.setLogicalTop(rootInlineBoxBaseline - inlineLevelBox.baseline());
        auto layoutBounds = inlineLevelBox.layoutBounds();

        auto layoutBoundsLogicalTop = rootInlineBoxLayoutBounds.ascent - layoutBounds.ascent;
        lineBoxLogicalTop = std::min(lineBoxLogicalTop, layoutBoundsLogicalTop);
        lineBoxLogicalBottom = std::max(lineBoxLogicalBottom, layoutBoundsLogicalTop + layoutBounds.height());
        rootInlineBoxLogicalTop = std::max(rootInlineBoxLogicalTop, layoutBounds.ascent - rootInlineBoxBaseline);
    }
    rootInlineBox.setLogicalTop(rootInlineBoxLogicalTop);
    return lineBoxLogicalBottom - lineBoxLogicalTop;
}

InlineLayoutUnit LineBoxVerticalAligner::computeLineBoxLogicalHeight(LineBox& lineBox) const
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
        switch (inlineLevelBox.verticalAlign()) {
        case VerticalAlign::Baseline: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().ascent;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Middle: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().height() / 2 + parentInlineBox.style().fontMetrics().xHeight() / 2;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::BaselineMiddle: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().height() / 2;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Length: {
            auto& style = inlineLevelBox.style();
            auto logicalTopOffsetFromParentBaseline = floatValueForLength(style.verticalAlignLength(), style.computedLineHeight()) + inlineLevelBox.layoutBounds().ascent;
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::TextTop: {
            // Note that text-top aligns with the inline box's font metrics top (ascent) and not the layout bounds top.
            logicalTop = parentInlineBox.layoutBounds().ascent - parentInlineBox.baseline();
            break;
        }
        case VerticalAlign::TextBottom: {
            // Note that text-bottom aligns with the inline box's font metrics bottom (descent) and not the layout bounds bottom.
            auto parentInlineBoxLayoutBounds = parentInlineBox.layoutBounds();
            auto parentInlineBoxLogicalBottom = parentInlineBoxLayoutBounds.height() - parentInlineBoxLayoutBounds.descent + parentInlineBox.descent().value_or(InlineLayoutUnit());
            logicalTop = parentInlineBoxLogicalBottom - inlineLevelBox.layoutBounds().height();
            break;
        }
        case VerticalAlign::Sub: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().ascent - (parentInlineBox.style().fontCascade().size() / 5 + 1);
            logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Super: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().ascent + parentInlineBox.style().fontCascade().size() / 3 + 1;
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
    auto lineBoxLogicalHeight = maximumLogicalBottom.value_or(InlineLayoutUnit()) - minimumLogicalTop.value_or(InlineLayoutUnit());
    for (auto* lineBoxRelativeInlineLevelBox : lineBoxRelativeInlineLevelBoxes) {
        if (!formattingGeometry.inlineLevelBoxAffectsLineBox(*lineBoxRelativeInlineLevelBox, lineBox))
            continue;
        lineBoxLogicalHeight = std::max(lineBoxLogicalHeight, lineBoxRelativeInlineLevelBox->layoutBounds().height());
    }
    return lineBoxLogicalHeight;
}

void LineBoxVerticalAligner::computeRootInlineBoxVerticalPosition(LineBox& lineBox) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto& formattingGeometry = this->formattingGeometry();

    HashMap<const InlineLevelBox*, InlineLayoutUnit> inlineLevelBoxAbsoluteBaselineOffsetMap;
    inlineLevelBoxAbsoluteBaselineOffsetMap.add(&rootInlineBox, InlineLayoutUnit { });

    auto maximumTopOffsetFromRootInlineBoxBaseline = std::optional<InlineLayoutUnit> { };
    if (formattingGeometry.inlineLevelBoxAffectsLineBox(rootInlineBox, lineBox))
        maximumTopOffsetFromRootInlineBoxBaseline = rootInlineBox.layoutBounds().ascent;

    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        auto absoluteBaselineOffset = InlineLayoutUnit { };
        if (!inlineLevelBox.hasLineBoxRelativeAlignment()) {
            auto& layoutBox = inlineLevelBox.layoutBox();
            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
            auto baselineOffsetFromParentBaseline = InlineLayoutUnit { };

            switch (inlineLevelBox.verticalAlign()) {
            case VerticalAlign::Baseline:
                baselineOffsetFromParentBaseline = { };
                break;
            case VerticalAlign::Middle: {
                auto logicalTopOffsetFromParentBaseline = (inlineLevelBox.layoutBounds().height() / 2 + parentInlineBox.style().fontMetrics().xHeight() / 2);
                baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox.layoutBounds().ascent;
                break;
            }
            case VerticalAlign::BaselineMiddle: {
                auto logicalTopOffsetFromParentBaseline = inlineLevelBox.layoutBounds().height() / 2;
                baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox.layoutBounds().ascent;
                break;
            }
            case VerticalAlign::Length: {
                auto& style = inlineLevelBox.style();
                auto verticalAlignOffset = floatValueForLength(style.verticalAlignLength(), style.computedLineHeight());
                auto logicalTopOffsetFromParentBaseline = verticalAlignOffset + inlineLevelBox.baseline();
                baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox.baseline();
                break;
            }
            case VerticalAlign::TextTop:
                baselineOffsetFromParentBaseline = parentInlineBox.baseline() - inlineLevelBox.layoutBounds().ascent;
                break;
            case VerticalAlign::TextBottom:
                baselineOffsetFromParentBaseline = inlineLevelBox.layoutBounds().descent - *parentInlineBox.descent();
                break;
            case VerticalAlign::Sub:
                baselineOffsetFromParentBaseline = -(parentInlineBox.style().fontCascade().size() / 5 + 1);
                break;
            case VerticalAlign::Super:
                baselineOffsetFromParentBaseline = parentInlineBox.style().fontCascade().size() / 3 + 1;
                break;
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
            absoluteBaselineOffset = inlineLevelBoxAbsoluteBaselineOffsetMap.get(&parentInlineBox) + baselineOffsetFromParentBaseline;
        } else {
            switch (inlineLevelBox.verticalAlign()) {
            case VerticalAlign::Top: {
                absoluteBaselineOffset = rootInlineBox.layoutBounds().ascent - inlineLevelBox.layoutBounds().ascent;
                break;
            }
            case VerticalAlign::Bottom: {
                absoluteBaselineOffset = inlineLevelBox.layoutBounds().descent - rootInlineBox.layoutBounds().descent;
                break;
            }
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
        }
        inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, absoluteBaselineOffset);
        auto affectsRootInlineBoxVerticalPosition = formattingGeometry.inlineLevelBoxAffectsLineBox(inlineLevelBox, lineBox);
        if (affectsRootInlineBoxVerticalPosition) {
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
    auto rootInlineBoxLogicalTop = maximumTopOffsetFromRootInlineBoxBaseline.value_or(0.f) - rootInlineBox.baseline();
    rootInlineBox.setLogicalTop(rootInlineBoxLogicalTop);
}

void LineBoxVerticalAligner::alignInlineLevelBoxes(LineBox& lineBox, InlineLayoutUnit lineBoxLogicalHeight) const
{
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        auto& layoutBox = inlineLevelBox.layoutBox();
        auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
        auto logicalTop = InlineLayoutUnit { };
        switch (inlineLevelBox.verticalAlign()) {
        case VerticalAlign::Baseline:
            logicalTop = parentInlineBox.baseline() - inlineLevelBox.baseline();
            break;
        case VerticalAlign::Middle: {
            auto logicalTopOffsetFromParentBaseline = (inlineLevelBox.logicalHeight() / 2 + parentInlineBox.style().fontMetrics().xHeight() / 2);
            logicalTop = parentInlineBox.baseline() - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::BaselineMiddle: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.logicalHeight() / 2;
            logicalTop = parentInlineBox.baseline() - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Length: {
            auto& style = inlineLevelBox.style();
            auto verticalAlignOffset = floatValueForLength(style.verticalAlignLength(), style.computedLineHeight());
            auto logicalTopOffsetFromParentBaseline = verticalAlignOffset + inlineLevelBox.baseline();
            logicalTop = parentInlineBox.baseline() - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Sub: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.baseline() - (parentInlineBox.style().fontCascade().size() / 5 + 1);
            logicalTop = parentInlineBox.baseline() - logicalTopOffsetFromParentBaseline;
            break;
        }
        case VerticalAlign::Super: {
            auto logicalTopOffsetFromParentBaseline = inlineLevelBox.baseline() + parentInlineBox.style().fontCascade().size() / 3 + 1;
            logicalTop = parentInlineBox.baseline() - logicalTopOffsetFromParentBaseline;
            break;
        }
        // Note that (text)top/bottom align their layout bounds.
        case VerticalAlign::TextTop:
            logicalTop = inlineLevelBox.layoutBounds().ascent - inlineLevelBox.baseline();
            break;
        case VerticalAlign::TextBottom:
            logicalTop = parentInlineBox.logicalHeight() - inlineLevelBox.layoutBounds().descent - inlineLevelBox.baseline();
            break;
        case VerticalAlign::Top:
            // Note that this logical top is not relative to the parent inline box.
            logicalTop = inlineLevelBox.layoutBounds().ascent - inlineLevelBox.baseline();
            break;
        case VerticalAlign::Bottom:
            // Note that this logical top is not relative to the parent inline box.
            logicalTop = lineBoxLogicalHeight - inlineLevelBox.layoutBounds().descent - inlineLevelBox.baseline();
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
