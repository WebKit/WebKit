/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InlineLineBoxVerticalAligner.h"

#include "InlineFormattingContext.h"
#include "InlineLevelBoxInlines.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"

namespace WebCore {
namespace Layout {

LineBoxVerticalAligner::LineBoxVerticalAligner(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
{
}

InlineLayoutUnit LineBoxVerticalAligner::computeLogicalHeightAndAlign(LineBox& lineBox) const
{
    auto canUseSimplifiedAlignment = [&] {
        if (!lineBox.hasContent())
            return true;

        if (rootBox().style().lineBoxContain() != RenderStyle::initialLineBoxContain())
            return false;
        auto& rootInlineBox = lineBox.rootInlineBox();
        if (!layoutState().inStandardsMode() || !WTF::holdsAlternative<CSS::Keyword::Baseline>(rootInlineBox.verticalAlign()))
            return false;
        if (rootInlineBox.hasTextEmphasis())
            return false;

        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            auto shouldUseSimplifiedAlignmentForInlineLevelBox = [&] {
                if (inlineLevelBox.hasTextEmphasis())
                    return false;
                // Baseline aligned, non-stretchy direct children are considered to be simple for now.
                auto& layoutBox = inlineLevelBox.layoutBox();
                if (&layoutBox.parent() != &rootInlineBox.layoutBox() || !WTF::holdsAlternative<CSS::Keyword::Baseline>(inlineLevelBox.verticalAlign()))
                    return false;

                if (inlineLevelBox.isAtomicInlineBox()) {
                    auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
                    return !inlineLevelBoxGeometry.marginBefore() && !inlineLevelBoxGeometry.marginAfter() && inlineLevelBoxGeometry.marginBoxHeight() <= rootInlineBox.layoutBounds().ascent;
                }
                if (inlineLevelBox.isLineBreakBox()) {
                    // Baseline aligned, non-stretchy line breaks e.g. <div><span><br></span></div> but not <div><span style="font-size: 100px;"><br></span></div>.
                    return inlineLevelBox.layoutBounds().ascent <= rootInlineBox.layoutBounds().ascent;
                }
                if (inlineLevelBox.isInlineBox()) {
                    // Baseline aligned, non-stretchy inline boxes e.g. <div><span></span></div> but not <div><span style="font-size: 100px;"></span></div>.
                    return inlineLevelBox.layoutBounds() == rootInlineBox.layoutBounds();
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
    if (lineBoxAlignmentContent.hasTextEmphasis)
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

InlineLayoutUnit LineBoxVerticalAligner::logicalTopOffsetFromParentBaseline(const InlineLevelBox& inlineLevelBox, const InlineLevelBox& parentInlineBox, IsInlineLevelBoxAlignment isInlineLevelBoxAlignment) const
{
    ASSERT(parentInlineBox.isInlineBox());

    auto ascent = isInlineLevelBoxAlignment == IsInlineLevelBoxAlignment::Yes ? inlineLevelBox.ascent() : inlineLevelBox.layoutBounds().ascent;
    auto height = isInlineLevelBoxAlignment == IsInlineLevelBoxAlignment::Yes ? inlineLevelBox.logicalHeight() : inlineLevelBox.layoutBounds().height();

    return WTF::switchOn(inlineLevelBox.verticalAlign(),
        [&](const CSS::Keyword::Baseline&) -> InlineLayoutUnit {
            return ascent;
        },
        [&](const CSS::Keyword::Sub&) -> InlineLayoutUnit {
            return ascent - (parentInlineBox.fontSize() / 5 + 1);
        },
        [&](const CSS::Keyword::Super&) -> InlineLayoutUnit {
            return ascent + parentInlineBox.fontSize() / 3 + 1;
        },
        [&](const CSS::Keyword::Top&) -> InlineLayoutUnit {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        },
        [&](const CSS::Keyword::TextTop&) -> InlineLayoutUnit {
            if (isInlineLevelBoxAlignment == IsInlineLevelBoxAlignment::No)
                return parentInlineBox.ascent();
            // Note that text-top aligns with the inline box's font metrics top (ascent) and not the layout bounds top.
            return parentInlineBox.ascent() + (inlineLevelBox.ascent() - inlineLevelBox.layoutBounds().ascent);
        },
        [&](const CSS::Keyword::Middle&) -> InlineLayoutUnit {
            return height / 2 + parentInlineBox.primarymetricsOfPrimaryFont().xHeight().value_or(0) / 2;
        },
        [&](const CSS::Keyword::Bottom&) -> InlineLayoutUnit {
            ASSERT_NOT_IMPLEMENTED_YET();
            return { };
        },
        [&](const CSS::Keyword::TextBottom&) -> InlineLayoutUnit {
            if (isInlineLevelBoxAlignment == IsInlineLevelBoxAlignment::No)
                return height - parentInlineBox.descent();
            // Note that text-bottom aligns with the inline box's font metrics bottom (descent) and not the layout bounds bottom.
            return (inlineLevelBox.ascent() + inlineLevelBox.layoutBounds().descent) - parentInlineBox.descent();
        },
        [&](const CSS::Keyword::WebkitBaselineMiddle&) -> InlineLayoutUnit {
            return height / 2;
        },
        [&](const InlineLayoutUnit& baselineOffset) -> InlineLayoutUnit {
            return baselineOffset + ascent;
        }
    );
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
    auto& formattingUtils = this->formattingUtils();
    auto contentHasTextEmphasis = rootInlineBox.hasTextEmphasis();

    // Line box height computation is based on the layout bounds of the inline boxes and not their logical (ascent/descent) dimensions.
    struct AbsoluteTopAndBottom {
        InlineLayoutUnit top { 0 };
        InlineLayoutUnit bottom { 0 };
    };
    HashMap<const InlineLevelBox*, AbsoluteTopAndBottom> inlineLevelBoxAbsoluteTopAndBottomMap;

    auto minimumLogicalTop = std::optional<InlineLayoutUnit> { };
    auto maximumLogicalBottom = std::optional<InlineLayoutUnit> { };
    if (formattingUtils.inlineLevelBoxAffectsLineBox(rootInlineBox)) {
        minimumLogicalTop = InlineLayoutUnit { };
        maximumLogicalBottom = rootInlineBox.layoutBounds().height();
        inlineLevelBoxAbsoluteTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { *minimumLogicalTop, *maximumLogicalBottom });
    } else
        inlineLevelBoxAbsoluteTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { });

    Vector<InlineLevelBox*> lineBoxRelativeInlineLevelBoxes;
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        contentHasTextEmphasis = contentHasTextEmphasis || inlineLevelBox.hasTextEmphasis();

        if (inlineLevelBox.hasLineBoxRelativeAlignment()) {
            lineBoxRelativeInlineLevelBoxes.append(&inlineLevelBox);
            continue;
        }
        auto& parentInlineBox = lineBox.parentInlineBox(inlineLevelBox);
        auto inlineBoxTopOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline(inlineLevelBox, parentInlineBox);
        // Logical top is relative to the parent inline box's layout bounds.
        // Note that this logical top is not the final logical top of the inline level box.
        // This is the logical top in the context of the layout bounds geometry which may be very different from the inline box's normal geometry.
        auto inlineLevelBoxLogicalTop = parentInlineBox.layoutBounds().ascent - inlineBoxTopOffsetFromParentBaseline;
        auto parentInlineBoxAbsoluteTopAndBottom = inlineLevelBoxAbsoluteTopAndBottomMap.get(&parentInlineBox);
        auto absoluteLogicalTop = parentInlineBoxAbsoluteTopAndBottom.top + inlineLevelBoxLogicalTop;
        auto absoluteLogicalBottom = absoluteLogicalTop + inlineLevelBox.layoutBounds().height();
        inlineLevelBoxAbsoluteTopAndBottomMap.add(&inlineLevelBox, AbsoluteTopAndBottom { absoluteLogicalTop, absoluteLogicalBottom });
        // Stretch the min/max absolute values if applicable.
        if (formattingUtils.inlineLevelBoxAffectsLineBox(inlineLevelBox)) {
            minimumLogicalTop = std::min(minimumLogicalTop.value_or(absoluteLogicalTop), absoluteLogicalTop);
            maximumLogicalBottom = std::max(maximumLogicalBottom.value_or(absoluteLogicalBottom), absoluteLogicalBottom);
        }
    }
    // The line box height computation is as follows:
    // 1. Stretch the line box with the non-line-box relative aligned inline box absolute top and bottom values.
    // 2. Check if the line box relative aligned inline boxes (top, bottom etc) have enough room and stretch the line box further if needed.
    auto nonLineBoxRelativeAlignedBoxesMaximumHeight = valueOrDefault(maximumLogicalBottom) - valueOrDefault(minimumLogicalTop);
    auto topAlignedBoxesMaximumHeight = std::optional<InlineLayoutUnit> { };
    auto bottomAlignedBoxesMaximumHeight = std::optional<InlineLayoutUnit> { };
    for (auto* lineBoxRelativeInlineLevelBox : lineBoxRelativeInlineLevelBoxes) {
        if (!formattingUtils.inlineLevelBoxAffectsLineBox(*lineBoxRelativeInlineLevelBox))
            continue;
        // This line box relative aligned inline level box stretches the line box.
        auto inlineLevelBoxHeight = lineBoxRelativeInlineLevelBox->layoutBounds().height();
        if (WTF::holdsAlternative<CSS::Keyword::Top>(lineBoxRelativeInlineLevelBox->verticalAlign())) {
            topAlignedBoxesMaximumHeight = std::max(inlineLevelBoxHeight, topAlignedBoxesMaximumHeight.value_or(0.f));
            continue;
        }
        if (WTF::holdsAlternative<CSS::Keyword::Bottom>(lineBoxRelativeInlineLevelBox->verticalAlign())) {
            bottomAlignedBoxesMaximumHeight = std::max(inlineLevelBoxHeight, bottomAlignedBoxesMaximumHeight.value_or(0.f));
            continue;
        }
        ASSERT_NOT_REACHED();
    }
    return { nonLineBoxRelativeAlignedBoxesMaximumHeight, { topAlignedBoxesMaximumHeight, bottomAlignedBoxesMaximumHeight }, contentHasTextEmphasis };
}

void LineBoxVerticalAligner::computeRootInlineBoxVerticalPosition(LineBox& lineBox, const LineBoxAlignmentContent& lineBoxAlignmentContent) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto& formattingUtils = this->formattingUtils();
    auto hasTopAlignedInlineLevelBox = false;

    HashMap<const InlineLevelBox*, InlineLayoutUnit> inlineLevelBoxAbsoluteBaselineOffsetMap;
    inlineLevelBoxAbsoluteBaselineOffsetMap.add(&rootInlineBox, InlineLayoutUnit { });

    auto maximumTopOffsetFromRootInlineBoxBaseline = std::optional<InlineLayoutUnit> { };
    if (formattingUtils.inlineLevelBoxAffectsLineBox(rootInlineBox))
        maximumTopOffsetFromRootInlineBoxBaseline = rootInlineBox.layoutBounds().ascent;

    auto affectsRootInlineBoxVerticalPosition = [&](auto& inlineLevelBox) {
        return formattingUtils.inlineLevelBoxAffectsLineBox(inlineLevelBox);
    };

    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        auto layoutBounds = inlineLevelBox.layoutBounds();

        if (inlineLevelBox.hasLineBoxRelativeAlignment()) {
            auto& verticalAlign = inlineLevelBox.verticalAlign();
            if (WTF::holdsAlternative<CSS::Keyword::Top>(verticalAlign)) {
                hasTopAlignedInlineLevelBox = hasTopAlignedInlineLevelBox || affectsRootInlineBoxVerticalPosition(inlineLevelBox);
                inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, rootInlineBox.layoutBounds().ascent - layoutBounds.ascent);
            } else if (WTF::holdsAlternative<CSS::Keyword::Bottom>(verticalAlign))
                inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, layoutBounds.descent - rootInlineBox.layoutBounds().descent);
            else
                ASSERT_NOT_REACHED();
            continue;
        }
        auto& parentInlineBox = lineBox.parentInlineBox(inlineLevelBox);
        auto inlineBoxTopOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline(inlineLevelBox, parentInlineBox);
        auto baselineOffsetFromParentBaseline = inlineBoxTopOffsetFromParentBaseline - layoutBounds.ascent;
        auto absoluteBaselineOffset = inlineLevelBoxAbsoluteBaselineOffsetMap.get(&parentInlineBox) + baselineOffsetFromParentBaseline;
        inlineLevelBoxAbsoluteBaselineOffsetMap.add(&inlineLevelBox, absoluteBaselineOffset);

        if (affectsRootInlineBoxVerticalPosition(inlineLevelBox)) {
            auto topOffsetFromRootInlineBoxBaseline = absoluteBaselineOffset + layoutBounds.ascent;
            if (maximumTopOffsetFromRootInlineBoxBaseline)
                maximumTopOffsetFromRootInlineBoxBaseline = std::max(*maximumTopOffsetFromRootInlineBoxBaseline, topOffsetFromRootInlineBoxBaseline);
            else {
                // We are is quirk mode and the root inline box has no content. The root inline box's baseline is anchored at 0.
                // However negative ascent (e.g negative top margin) can "push" the root inline box upwards and have a negative value.
                maximumTopOffsetFromRootInlineBoxBaseline = layoutBounds.ascent >= 0
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
    auto bottomAlignedBoxStretch = lineBoxAlignmentContent.topAndBottomAlignedMaximumHeight.bottom.value_or(0.f);
    if (bottomAlignedBoxStretch) {
        // If top happens to stretch the line box, we don't need to push the root inline box anymore.
        if (bottomAlignedBoxStretch <= lineBoxAlignmentContent.topAndBottomAlignedMaximumHeight.top.value_or(0.f))
            bottomAlignedBoxStretch = { };

        // However non-line box relative content needs some space. Root inline box may not end up being at the very bottom.
        if (bottomAlignedBoxStretch) {
            if (lineBoxAlignmentContent.nonLineBoxRelativeAlignedMaximumHeight > 0) {
                // Negative vertical margin can make aligned boxes have negative height.
                bottomAlignedBoxStretch -= std::max(0.f, lineBoxAlignmentContent.nonLineBoxRelativeAlignedMaximumHeight);
            }
            bottomAlignedBoxStretch = std::max(0.f, bottomAlignedBoxStretch);
        }
    }
    auto rootInlineBoxLogicalTop = bottomAlignedBoxStretch + maximumTopOffsetFromRootInlineBoxBaseline.value_or(0.f) - rootInlineBox.ascent();
    rootInlineBox.setLogicalTop(rootInlineBoxLogicalTop);
}

std::optional<InlineLevelBox::AscentAndDescent> LineBoxVerticalAligner::layoutBoundsForInlineBoxSubtree(const LineBox::InlineLevelBoxList& nonRootInlineLevelBoxes, size_t inlineBoxIndex) const
{
    // https://w3c.github.io/csswg-drafts/css2/#propdef-vertical-align
    //
    // top/bottom values align the element relative to the line box.
    // Since the element may have children aligned relative to it (which in turn may have descendants aligned relative to them),
    // these values use the bounds of the aligned subtree.
    // The aligned subtree of an inline element contains that element and the aligned subtrees of all children
    // inline elements whose computed vertical-align value is not top or bottom.
    // The top of the aligned subtree is the highest of the tops of the boxes in the subtree, and the bottom is analogous.
    ASSERT(nonRootInlineLevelBoxes[inlineBoxIndex].isInlineBox());
    auto& formattingUtils = this->formattingUtils();
    auto maximumAscent = std::optional<InlineLayoutUnit> { };
    auto maximumDescent = std::optional<InlineLayoutUnit> { };
    auto& inlineBox = nonRootInlineLevelBoxes[inlineBoxIndex];
    auto& inlineBoxParent = inlineBox.layoutBox().parent();
    for (size_t index = inlineBoxIndex + 1; index < nonRootInlineLevelBoxes.size(); ++index) {
        auto& descendantInlineLevelBox = nonRootInlineLevelBoxes[index];
        if (&descendantInlineLevelBox.layoutBox().parent() == &inlineBoxParent) {
            // We are at the end of the descendant list.
            break;
        }
        if (!formattingUtils.inlineLevelBoxAffectsLineBox(descendantInlineLevelBox) || descendantInlineLevelBox.hasLineBoxRelativeAlignment())
            continue;

        // ascent/descent here really mean enclosing geometry adjusted by vertical alignemnt, which is in case of baseline alignment is simply layout bounds but
        // e.g. with middle alignment, "ascent and descent" are inline level box height / 2.
        auto ascent = logicalTopOffsetFromParentBaseline(descendantInlineLevelBox, inlineBox);
        auto descent = descendantInlineLevelBox.layoutBounds().height() - ascent;
        maximumAscent = std::max(ascent, maximumAscent.value_or(ascent));
        maximumDescent = std::max(descent, maximumDescent.value_or(descent));
    }
    if (maximumAscent) {
        ASSERT(maximumDescent);
        return InlineLevelBox::AscentAndDescent { *maximumAscent, *maximumDescent };
    }
    return { };
}

void LineBoxVerticalAligner::alignInlineLevelBoxes(LineBox& lineBox, InlineLayoutUnit lineBoxLogicalHeight) const
{
    Vector<size_t> lineBoxRelativeInlineLevelBoxes;
    auto& nonRootInlineLevelBoxes = lineBox.nonRootInlineLevelBoxes();
    for (size_t index = 0; index < nonRootInlineLevelBoxes.size(); ++index) {
        auto& inlineLevelBox = nonRootInlineLevelBoxes[index];
        if (inlineLevelBox.hasLineBoxRelativeAlignment()) {
            lineBoxRelativeInlineLevelBoxes.append(index);
            continue;
        }
        auto& parentInlineBox = lineBox.parentInlineBox(inlineLevelBox);
        auto inlineBoxTopOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline(inlineLevelBox, parentInlineBox, IsInlineLevelBoxAlignment::Yes);
        auto inlineLevelBoxLogicalTop = parentInlineBox.ascent() - inlineBoxTopOffsetFromParentBaseline;
        inlineLevelBox.setLogicalTop(inlineLevelBoxLogicalTop);
    }

    for (auto index : lineBoxRelativeInlineLevelBoxes) {
        auto& inlineLevelBox = nonRootInlineLevelBoxes[index];
        auto logicalTop = InlineLayoutUnit { };
        WTF::switchOn(inlineLevelBox.verticalAlign(),
            [&](const CSS::Keyword::Top&) {
                auto ascent = inlineLevelBox.layoutBounds().ascent;
                if (inlineLevelBox.isInlineBox()) {
                    if (auto descendantsEnclosingGeometry = layoutBoundsForInlineBoxSubtree(nonRootInlineLevelBoxes, index))
                        ascent = !inlineLevelBox.hasContent() ? descendantsEnclosingGeometry->ascent : std::max(descendantsEnclosingGeometry->ascent, ascent);
                }
                // Note that this logical top is not relative to the parent inline box.
                logicalTop = ascent - inlineLevelBox.ascent();
            },
            [&](const CSS::Keyword::Bottom&) {
                auto descent = inlineLevelBox.layoutBounds().descent;
                if (inlineLevelBox.isInlineBox()) {
                    if (auto descendantsEnclosingGeometry = layoutBoundsForInlineBoxSubtree(nonRootInlineLevelBoxes, index))
                        descent = !inlineLevelBox.hasContent() ? descendantsEnclosingGeometry->descent : std::max(descendantsEnclosingGeometry->descent, descent);
                }
                // Note that this logical top is not relative to the parent inline box.
                logicalTop = lineBoxLogicalHeight - (inlineLevelBox.ascent() + descent);
            },
            [](const auto&) {
                ASSERT_NOT_REACHED();
            }
        );
        inlineLevelBox.setLogicalTop(logicalTop);
    }
}

InlineLayoutUnit LineBoxVerticalAligner::adjustForAnnotationIfNeeded(LineBox& lineBox, InlineLayoutUnit lineBoxHeight) const
{
    auto lineBoxTop = InlineLayoutUnit { };
    auto lineBoxBottom = lineBoxHeight;
    // At this point we have a properly aligned set of inline level boxes. Let's find out if annotation marks have enough space.
    auto adjustLineBoxHeightIfNeeded = [&] {
        auto adjustLineBoxTopAndBottomForInlineBox = [&](const InlineLevelBox& inlineLevelBox) {
            ASSERT(inlineLevelBox.isInlineBox() || inlineLevelBox.isAtomicInlineBox());
            auto inlineBoxTop = lineBox.inlineLevelBoxAbsoluteTop(inlineLevelBox);
            auto inlineBoxBottom = inlineBoxTop + inlineLevelBox.logicalHeight();

            auto defaultCase = [&] {
                if (auto aboveSpace = inlineLevelBox.textEmphasisAbove())
                    lineBoxTop = std::min(lineBoxTop, inlineBoxTop - *aboveSpace);
                if (auto belowSpace = inlineLevelBox.textEmphasisBelow())
                    lineBoxBottom = std::max(lineBoxBottom, inlineBoxBottom + *belowSpace);
            };

            WTF::switchOn(inlineLevelBox.verticalAlign(),
                [&](const CSS::Keyword::Baseline&) {
                    defaultCase();
                },
                [&](const CSS::Keyword::Sub&) {
                    defaultCase();
                },
                [&](const CSS::Keyword::Super&) {
                    defaultCase();
                },
                [&](const CSS::Keyword::Top&) {
                    // FIXME: Check if horizontal vs. vertical writing mode should be taking into account.
                    auto annotationSpace = inlineLevelBox.textEmphasisAbove().value_or(0.f) + inlineLevelBox.textEmphasisBelow().value_or(0.f);
                    lineBoxBottom = std::max(lineBoxBottom, inlineBoxBottom + annotationSpace);
                },
                [&](const CSS::Keyword::TextTop&) {
                    defaultCase();
                },
                [&](const CSS::Keyword::Middle&) {
                    defaultCase();
                },
                [&](const CSS::Keyword::Bottom&) {
                    defaultCase();
                },
                [&](const CSS::Keyword::TextBottom&) {
                    defaultCase();
                },
                [&](const CSS::Keyword::WebkitBaselineMiddle&) {
                    defaultCase();
                },
                [&](const InlineLayoutUnit&) {
                    defaultCase();
                }
            );
        };

        adjustLineBoxTopAndBottomForInlineBox(lineBox.rootInlineBox());
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (inlineLevelBox.isInlineBox() || inlineLevelBox.isAtomicInlineBox())
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
                WTF::switchOn(inlineLevelBox.verticalAlign(),
                    [&](const CSS::Keyword::Top&) {
                        auto inlineBoxTop = inlineLevelBox.layoutBounds().ascent - inlineLevelBox.ascent();
                        inlineLevelBox.setLogicalTop(inlineLevelBox.textEmphasisAbove().value_or(0.f) + inlineBoxTop);
                    },
                    [&](const CSS::Keyword::Bottom&) {
                        auto inlineBoxTop = adjustedLineBoxHeight - (inlineLevelBox.layoutBounds().descent + inlineLevelBox.ascent());
                        inlineLevelBox.setLogicalTop(inlineBoxTop - inlineLevelBox.textEmphasisBelow().value_or(0.f));
                    },
                    [](const auto&) {
                        // These alignment positions are relative to the root inline box's baseline.
                    }
                );
            }
        };
        adjustContentTopWithAnnotationSpace();
    }
    return adjustedLineBoxHeight;
}

}
}

