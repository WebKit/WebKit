/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingContext.h"
#include "FormattingContext.h"
#include "InlineLineBox.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutReplacedBox.h"
#include "LengthFunctions.h"

namespace WebCore {
namespace Layout {

class LineBoxBuilder {
public:
    LineBoxBuilder(const InlineFormattingContext&);
    LineBox build(const LineBuilder::LineContent&);

private:
    void setVerticalGeometryForInlineBox(LineBox::InlineLevelBox&) const;
    void constructInlineLevelBoxes(LineBox&, const Line::RunList&);
    void computeLineBoxHeightAndAlignInlineLevelBoxesVertically(LineBox&);

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const Box& rootBox() const { return formattingContext().root(); }
    LayoutState& layoutState() const { return formattingContext().layoutState(); }

    bool isRootLayoutBox(const ContainerBox& containerBox) const { return &containerBox == &rootBox(); }

private:
    const InlineFormattingContext& m_inlineFormattingContext;
    bool m_inlineLevelBoxesNeedVerticalAlignment { false };
};

struct HangingTrailingWhitespaceContent {
public:
    void reset();

    InlineLayoutUnit width() const { return m_width; }
    bool isConditional() const { return m_isConditional; }

    void setIsConditional() { m_isConditional = true; }
    void expand(InlineLayoutUnit width) { m_width += width; }

private:
    bool m_isConditional { false };
    InlineLayoutUnit m_width { 0 };
};

void HangingTrailingWhitespaceContent::reset()
{
    m_isConditional = false;
    m_width =  0;
}

static HangingTrailingWhitespaceContent collectHangingTrailingWhitespaceContent(const Line::RunList& runs, bool isLastLineWithInlineContent)
{
    auto hangingContent = HangingTrailingWhitespaceContent { };
    if (isLastLineWithInlineContent)
        hangingContent.setIsConditional();
    for (auto& run : WTF::makeReversedRange(runs)) {
        if (run.isInlineBoxStart() || run.isInlineBoxEnd())
            continue;
        if (run.isLineBreak()) {
            hangingContent.setIsConditional();
            continue;
        }
        if (!run.hasTrailingWhitespace())
            break;
        // Check if we have a preserved or hung whitespace.
        if (run.style().whiteSpace() != WhiteSpace::PreWrap)
            break;
        // This is either a normal or conditionally hanging trailing whitespace.
        hangingContent.expand(run.trailingWhitespaceWidth());
    }
    return hangingContent;
}

static Optional<InlineLayoutUnit> horizontalAlignmentOffset(const Line::RunList& runs, TextAlignMode textAlign, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit contentLogicalWidth, bool isLastLine)
{
    auto availableWidth = lineLogicalWidth - contentLogicalWidth;
    auto hangingTrailingWhitespaceContent = collectHangingTrailingWhitespaceContent(runs, isLastLine);
    availableWidth += hangingTrailingWhitespaceContent.width();
    if (availableWidth <= 0)
        return { };

    auto computedHorizontalAlignment = [&] {
        if (textAlign != TextAlignMode::Justify)
            return textAlign;
        // Text is justified according to the method specified by the text-justify property,
        // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
        // the last line before a forced break or the end of the block is start-aligned.
        if (isLastLine || (!runs.isEmpty() && runs.last().isLineBreak()))
            return TextAlignMode::Start;
        return TextAlignMode::Justify;
    };

    switch (computedHorizontalAlignment()) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Start:
        return { };
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
    case TextAlignMode::End:
        return availableWidth;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return availableWidth / 2;
    case TextAlignMode::Justify:
        // TextAlignMode::Justify is a run alignment (and we only do inline box alignment here)
        return { };
    default:
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

LineBoxBuilder::LineBoxBuilder(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineLevelBoxesNeedVerticalAlignment(!inlineFormattingContext.layoutState().inStandardsMode())
{
}

LineBox LineBoxBuilder::build(const LineBuilder::LineContent& lineContent)
{
    auto& runs = lineContent.runs;
    auto lineLogicalWidth = lineContent.lineLogicalWidth;
    auto contentLogicalWidth = lineContent.contentLogicalWidth;
    auto lineBox = LineBox { lineContent.logicalTopLeft, lineLogicalWidth, contentLogicalWidth, runs.size() };

    if (auto horizontalAlignmentOffset = Layout::horizontalAlignmentOffset(runs, rootBox().style().textAlign(), lineLogicalWidth, contentLogicalWidth, lineContent.isLastLineWithInlineContent))
        lineBox.setHorizontalAlignmentOffset(*horizontalAlignmentOffset);
    constructInlineLevelBoxes(lineBox, runs);
    if (m_inlineLevelBoxesNeedVerticalAlignment)
        computeLineBoxHeightAndAlignInlineLevelBoxesVertically(lineBox);
    return lineBox;
}

void LineBoxBuilder::setVerticalGeometryForInlineBox(LineBox::InlineLevelBox& inlineLevelBox) const
{
    ASSERT(inlineLevelBox.isInlineBox() || inlineLevelBox.isLineBreakBox());
    auto& fontMetrics = inlineLevelBox.style().fontMetrics();
    InlineLayoutUnit ascent = fontMetrics.ascent();
    InlineLayoutUnit descent = fontMetrics.descent();
    auto logicalHeight = ascent + descent;
    // We need floor/ceil to match legacy layout integral positioning.
    inlineLevelBox.setBaseline(floorf(ascent));
    inlineLevelBox.setDescent(ceil(descent));
    inlineLevelBox.setLogicalHeight(logicalHeight);

    // FIXME: Adjust layout bounds with fallback font when applicable.
    auto& style = inlineLevelBox.layoutBox().style();
    auto lineHeight = style.lineHeight();
    if (lineHeight.isNegative()) {
        // If line-height computes to normal and either text-edge is leading or this is the root inline box,
        // the font’s line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
        // https://www.w3.org/TR/css-inline-3/#inline-height
        // Since text-edge is not supported yet and the initial value is leading, we should just apply it to
        // all inline boxes.
        auto halfLineGap = (fontMetrics.lineSpacing() - logicalHeight) / 2;
        ascent += halfLineGap;
        descent += halfLineGap;
    } else {
        InlineLayoutUnit lineHeight = style.computedLineHeight();
        InlineLayoutUnit halfLeading = (lineHeight - (ascent + descent)) / 2;
        ascent += halfLeading;
        descent += halfLeading;
    }
    // We need floor/ceil to match legacy layout integral positioning.
    inlineLevelBox.setLayoutBounds(LineBox::InlineLevelBox::LayoutBounds { floorf(ascent), ceil(descent) });
}

void LineBoxBuilder::constructInlineLevelBoxes(LineBox& lineBox, const Line::RunList& runs)
{
    auto horizontalAligmentOffset = lineBox.horizontalAlignmentOffset().valueOr(InlineLayoutUnit { });
    struct SimplifiedVerticalAlignment {
        InlineLayoutUnit lineBoxTop { 0 };
        InlineLayoutUnit lineBoxBottom { 0 };
        InlineLayoutUnit rootInlineBoxTop { 0 };
    };
    auto simplifiedVerticalAlignment = SimplifiedVerticalAlignment { };

    auto createRootInlineBox = [&] {
        auto rootInlineBox = LineBox::InlineLevelBox::createRootInlineBox(rootBox(), horizontalAligmentOffset, lineBox.contentLogicalWidth());
        setVerticalGeometryForInlineBox(*rootInlineBox);
        simplifiedVerticalAlignment = { { } , rootInlineBox->layoutBounds().height(), rootInlineBox->layoutBounds().ascent - rootInlineBox->baseline() };
        lineBox.addRootInlineBox(WTFMove(rootInlineBox));
    };
    createRootInlineBox();

    auto createWrappedInlineBoxes = [&] {
        if (runs.isEmpty())
            return;
        // An inline box may not necessarily start on the current line:
        // <span id=outer>line break<br>this content's parent inline box('outer') <span id=inner>starts on the previous line</span></span>
        // We need to make sure that there's an LineBox::InlineLevelBox for every inline box that's present on the current line.
        // In nesting case we need to create LineBox::InlineLevelBoxes for the inline box ancestors.
        // We only have to do it on the first run as any subsequent inline content is either at the same/higher nesting level or
        // nested with a [container start] run.
        auto& firstRun = runs[0];
        auto& firstRunParentLayoutBox = firstRun.layoutBox().parent();
        // If the parent is the formatting root, we can stop here. This is root inline box content, there's no nesting inline box from the previous line(s)
        // unless the inline box closing (container end run) is forced over to the current line.
        // e.g.
        // <span>normally the inline box closing forms a continuous content</span>
        // <span>unless it's forced to the next line<br></span>
        auto firstRunNeedsInlineBox = firstRun.isInlineBoxEnd();
        if (!firstRunNeedsInlineBox && isRootLayoutBox(firstRunParentLayoutBox))
            return;
        Vector<const Box*> layoutBoxesWithoutInlineBoxes;
        if (firstRunNeedsInlineBox)
            layoutBoxesWithoutInlineBoxes.append(&firstRun.layoutBox());
        auto* ancestor = &firstRunParentLayoutBox;
        while (!isRootLayoutBox(*ancestor)) {
            layoutBoxesWithoutInlineBoxes.append(ancestor);
            ancestor = &ancestor->parent();
        }
        // Construct the missing LineBox::InlineBoxes starting with the topmost layout box.
        for (auto* layoutBox : WTF::makeReversedRange(layoutBoxesWithoutInlineBoxes)) {
            auto inlineBox = LineBox::InlineLevelBox::createInlineBox(*layoutBox, horizontalAligmentOffset, lineBox.contentLogicalWidth());
            setVerticalGeometryForInlineBox(*inlineBox);
            lineBox.addInlineLevelBox(WTFMove(inlineBox));
        }
    };
    createWrappedInlineBoxes();

    auto& rootInlineBox = lineBox.rootInlineBox();
    auto lineHasContent = false;
    for (auto& run : runs) {
        auto& layoutBox = run.layoutBox();
        auto runHasContent = [&] () -> bool {
            ASSERT(!lineHasContent);
            if (run.isText() || run.isBox() || run.isSoftLineBreak() || run.isHardLineBreak())
                return true;
            auto& inlineBoxGeometry = formattingContext().geometryForBox(layoutBox);
            // Even negative horizontal margin makes the line "contentful".
            if (run.isInlineBoxStart())
                return inlineBoxGeometry.marginStart() || inlineBoxGeometry.borderLeft() || inlineBoxGeometry.paddingLeft().valueOr(0_lu);
            if (run.isInlineBoxEnd())
                return inlineBoxGeometry.marginEnd() || inlineBoxGeometry.borderRight() || inlineBoxGeometry.paddingRight().valueOr(0_lu);
            if (run.isWordBreakOpportunity())
                return false;
            ASSERT_NOT_REACHED();
            return true;
        };
        lineHasContent = lineHasContent || runHasContent();

        auto logicalLeft = horizontalAligmentOffset + run.logicalLeft();
        if (run.isBox()) {
            auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
            auto marginBoxHeight = inlineLevelBoxGeometry.marginBoxHeight();
            auto ascent = InlineLayoutUnit { };
            if (layoutState().shouldNotSynthesizeInlineBlockBaseline()) {
                // Integration codepath constructs replaced boxes for inline-block content.
                ASSERT(layoutBox.isReplacedBox());
                ascent = *downcast<ReplacedBox>(layoutBox).baseline();
            } else if (layoutBox.isInlineBlockBox()) {
                // The baseline of an 'inline-block' is the baseline of its last line box in the normal flow, unless it has either no in-flow line boxes or
                // if its 'overflow' property has a computed value other than 'visible', in which case the baseline is the bottom margin edge.
                auto synthesizeBaseline = !layoutBox.establishesInlineFormattingContext() || !layoutBox.style().isOverflowVisible();
                if (synthesizeBaseline)
                    ascent = marginBoxHeight;
                else {
                    auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                    auto& lastLine = formattingState.lines().last();
                    auto inlineBlockBaseline = lastLine.lineBoxLogicalRect().top() + lastLine.baseline();
                    ascent = inlineLevelBoxGeometry.marginBefore() + inlineLevelBoxGeometry.borderTop() + inlineLevelBoxGeometry.paddingTop().valueOr(0) + inlineBlockBaseline;
                }
            } else if (layoutBox.isReplacedBox())
                ascent = downcast<ReplacedBox>(layoutBox).baseline().valueOr(marginBoxHeight);
            else
                ascent = marginBoxHeight;
            logicalLeft += std::max(0_lu, inlineLevelBoxGeometry.marginStart());
            auto atomicInlineLevelBox = LineBox::InlineLevelBox::createAtomicInlineLevelBox(layoutBox, logicalLeft, { inlineLevelBoxGeometry.borderBoxWidth(), marginBoxHeight });
            atomicInlineLevelBox->setBaseline(ascent);
            atomicInlineLevelBox->setLayoutBounds(LineBox::InlineLevelBox::LayoutBounds { ascent, marginBoxHeight - ascent });
            // Let's pre-compute the logical top so that we can avoid running the alignment on simple inline boxes.
            auto alignInlineBoxIfEligible = [&] {
                if (m_inlineLevelBoxesNeedVerticalAlignment)
                    return;
                // Baseline aligned, non-stretchy direct children are considered to be simple for now.
                auto isConsideredSimple = &layoutBox.parent() == &rootBox()
                    && layoutBox.style().verticalAlign() == VerticalAlign::Baseline
                    && !inlineLevelBoxGeometry.marginBefore()
                    && !inlineLevelBoxGeometry.marginAfter()
                    && marginBoxHeight <= rootInlineBox.baseline();
                if (!isConsideredSimple) {
                    m_inlineLevelBoxesNeedVerticalAlignment = true;
                    return;
                }
                // Only baseline alignment for now.
                auto logicalTop = rootInlineBox.baseline() - ascent;
                auto layoutBoundsTop = rootInlineBox.layoutBounds().ascent - ascent;
                simplifiedVerticalAlignment.lineBoxTop = std::min(simplifiedVerticalAlignment.lineBoxTop, layoutBoundsTop);
                simplifiedVerticalAlignment.lineBoxBottom = std::max(simplifiedVerticalAlignment.lineBoxBottom, layoutBoundsTop + marginBoxHeight);
                simplifiedVerticalAlignment.rootInlineBoxTop = std::max(simplifiedVerticalAlignment.rootInlineBoxTop, ascent - rootInlineBox.baseline());
                atomicInlineLevelBox->setLogicalTop(logicalTop);
            };
            alignInlineBoxIfEligible();
            lineBox.addInlineLevelBox(WTFMove(atomicInlineLevelBox));
            continue;
        }
        // FIXME: Add support for simple inline boxes too.
        // We can do simplified vertical alignment with non-atomic inline boxes as long as the line has no content.
        // e.g. <div><span></span><span></span></div> is still okay.
        m_inlineLevelBoxesNeedVerticalAlignment = m_inlineLevelBoxesNeedVerticalAlignment || lineHasContent;
        if (run.isInlineBoxStart()) {
            // At this point we don't know yet how wide this inline box is. Let's assume it's as long as the line is
            // and adjust it later if we come across an inlineBoxEnd run (see below).
            auto initialLogicalWidth = lineBox.contentLogicalWidth() - run.logicalLeft();
            ASSERT(initialLogicalWidth >= 0);
            // Inline box run is based on margin box. Let's convert it to border box.
            auto marginStart = std::max(0_lu, formattingContext().geometryForBox(layoutBox).marginStart());
            logicalLeft += marginStart;
            initialLogicalWidth -= marginStart;
            auto inlineBox = LineBox::InlineLevelBox::createInlineBox(layoutBox, logicalLeft, initialLogicalWidth);
            setVerticalGeometryForInlineBox(*inlineBox);
            lineBox.addInlineLevelBox(WTFMove(inlineBox));
            continue;
        }
        if (run.isInlineBoxEnd()) {
            // Adjust the logical width when the inline level container closes on this line.
            auto& inlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox);
            ASSERT(inlineBox.isInlineBox());
            // Inline box run is based on margin box. Let's convert it to border box.
            auto marginEnd = std::max(0_lu, formattingContext().geometryForBox(layoutBox).marginEnd());
            auto inlineBoxLogicalRight = logicalLeft + run.logicalWidth() - marginEnd;
            inlineBox.setLogicalWidth(inlineBoxLogicalRight - inlineBox.logicalLeft());
            continue;
        }
        if (run.isText() || run.isSoftLineBreak()) {
            // FIXME: Adjust non-empty inline box height when glyphs from the non-primary font stretch the box.
            lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent()).setHasContent();
            continue;
        }
        if (run.isHardLineBreak()) {
            auto lineBreakBox = LineBox::InlineLevelBox::createLineBreakBox(layoutBox, logicalLeft);
            setVerticalGeometryForInlineBox(*lineBreakBox);
            lineBox.addInlineLevelBox(WTFMove(lineBreakBox));
            continue;
        }
        if (run.isWordBreakOpportunity()) {
            lineBox.addInlineLevelBox(LineBox::InlineLevelBox::createGenericInlineLevelBox(layoutBox, logicalLeft));
            continue;
        }
        ASSERT_NOT_REACHED();
    }

    lineBox.setHasContent(lineHasContent);
    if (!lineHasContent) {
        // We should always be able to exercise the fast path when the line has no content at all.
        m_inlineLevelBoxesNeedVerticalAlignment = false;
    } else if (!m_inlineLevelBoxesNeedVerticalAlignment) {
        // FIXME: Add fast path support for line-height content.
        m_inlineLevelBoxesNeedVerticalAlignment = !rootBox().style().lineHeight().isNegative();
    }

    if (!m_inlineLevelBoxesNeedVerticalAlignment) {
        if (!lineHasContent) {
            simplifiedVerticalAlignment.rootInlineBoxTop = -rootInlineBox.baseline();
            simplifiedVerticalAlignment.lineBoxTop = { };
            simplifiedVerticalAlignment.lineBoxBottom = { };
        }
        rootInlineBox.setLogicalTop(simplifiedVerticalAlignment.rootInlineBoxTop);
        lineBox.setLogicalHeight(simplifiedVerticalAlignment.lineBoxBottom - simplifiedVerticalAlignment.lineBoxTop);
    }
}

void LineBoxBuilder::computeLineBoxHeightAndAlignInlineLevelBoxesVertically(LineBox& lineBox)
{
    ASSERT(lineBox.hasContent());
    // This function (partially) implements:
    // 2.2. Layout Within Line Boxes
    // https://www.w3.org/TR/css-inline-3/#line-layout
    // 1. Compute the line box height using the layout bounds geometry. This height computation strictly uses layout bounds and not normal inline level box geometries.
    // 2. Compute the baseline/logical top position of the root inline box. Aligned boxes push the root inline box around inside the line box.
    // 3. Finally align the inline level boxes using (mostly) normal inline level box geometries.
    auto quirks = formattingContext().quirks();
    auto& rootInlineBox = lineBox.rootInlineBox();

    auto computeLineBoxLogicalHeight = [&] {
        // Line box height computation is based on the layout bounds of the inline boxes and not their logical (ascent/descent) dimensions.
        struct AbsoluteTopAndBottom {
            InlineLayoutUnit top { 0 };
            InlineLayoutUnit bottom { 0 };
        };
        HashMap<LineBox::InlineLevelBox*, AbsoluteTopAndBottom> inlineLevelBoxAbsoluteTopAndBottomMap;

        auto minimumLogicalTop = Optional<InlineLayoutUnit> { };
        auto maximumLogicalBottom = Optional<InlineLayoutUnit> { };
        if (quirks.inlineLevelBoxAffectsLineBox(rootInlineBox, lineBox)) {
            minimumLogicalTop = InlineLayoutUnit { };
            maximumLogicalBottom = rootInlineBox.layoutBounds().height();
            inlineLevelBoxAbsoluteTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { *minimumLogicalTop, *maximumLogicalBottom });
        } else
            inlineLevelBoxAbsoluteTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { });

        Vector<LineBox::InlineLevelBox*> lineBoxRelativeInlineLevelBoxes;
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            auto& layoutBox = inlineLevelBox->layoutBox();
            if (inlineLevelBox->hasLineBoxRelativeAlignment()) {
                lineBoxRelativeInlineLevelBoxes.append(inlineLevelBox.get());
                continue;
            }
            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
            // Logical top is relative to the parent inline box's layout bounds.
            // Note that this logical top is not the final logical top of the inline level box.
            // This is the logical top in the context of the layout bounds geometry which may be very different from the inline box's normal geometry.
            auto logicalTop = InlineLayoutUnit { };
            switch (inlineLevelBox->verticalAlign()) {
            case VerticalAlign::Baseline: {
                auto logicalTopOffsetFromParentBaseline = inlineLevelBox->layoutBounds().ascent;
                logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
                break;
            }
            case VerticalAlign::Middle: {
                auto logicalTopOffsetFromParentBaseline = inlineLevelBox->layoutBounds().height() / 2 + parentInlineBox.style().fontMetrics().xHeight() / 2;
                logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
                break;
            }
            case VerticalAlign::BaselineMiddle: {
                auto logicalTopOffsetFromParentBaseline = inlineLevelBox->layoutBounds().height() / 2;
                logicalTop = parentInlineBox.layoutBounds().ascent - logicalTopOffsetFromParentBaseline;
                break;
            }
            case VerticalAlign::Length: {
                auto& style = inlineLevelBox->style();
                auto logicalTopOffsetFromParentBaseline = floatValueForLength(style.verticalAlignLength(), style.computedLineHeight()) + inlineLevelBox->layoutBounds().ascent;
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
                auto parentInlineBoxLogicalBottom = parentInlineBoxLayoutBounds.height() - parentInlineBoxLayoutBounds.descent + parentInlineBox.descent().valueOr(InlineLayoutUnit());
                logicalTop = parentInlineBoxLogicalBottom - inlineLevelBox->layoutBounds().height();
                break;
            }
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
            auto parentInlineBoxAbsoluteTopAndBottom = inlineLevelBoxAbsoluteTopAndBottomMap.get(&parentInlineBox);
            auto absoluteLogicalTop = parentInlineBoxAbsoluteTopAndBottom.top + logicalTop;
            auto absoluteLogicalBottom = absoluteLogicalTop + inlineLevelBox->layoutBounds().height();
            inlineLevelBoxAbsoluteTopAndBottomMap.add(inlineLevelBox.get(), AbsoluteTopAndBottom { absoluteLogicalTop, absoluteLogicalBottom });
            // Stretch the min/max absolute values if applicable.
            if (quirks.inlineLevelBoxAffectsLineBox(*inlineLevelBox, lineBox)) {
                minimumLogicalTop = std::min(minimumLogicalTop.valueOr(absoluteLogicalTop), absoluteLogicalTop);
                maximumLogicalBottom = std::max(maximumLogicalBottom.valueOr(absoluteLogicalBottom), absoluteLogicalBottom);
            }
        }
        // The line box height computation is as follows:
        // 1. Stretch the line box with the non-line-box relative aligned inline box absolute top and bottom values.
        // 2. Check if the line box relative aligned inline boxes (top, bottom etc) have enough room and stretch the line box further if needed.
        auto lineBoxLogicalHeight = maximumLogicalBottom.valueOr(InlineLayoutUnit()) - minimumLogicalTop.valueOr(InlineLayoutUnit());
        for (auto* lineBoxRelativeInlineLevelBox : lineBoxRelativeInlineLevelBoxes) {
            if (!quirks.inlineLevelBoxAffectsLineBox(*lineBoxRelativeInlineLevelBox, lineBox))
                continue;
            lineBoxLogicalHeight = std::max(lineBoxLogicalHeight, lineBoxRelativeInlineLevelBox->layoutBounds().height());
        }
        lineBox.setLogicalHeight(lineBoxLogicalHeight);
    };
    computeLineBoxLogicalHeight();

    auto computeRootInlineBoxVerticalPosition = [&] {
        HashMap<LineBox::InlineLevelBox*, InlineLayoutUnit> inlineLevelBoxAbsoluteBaselineOffsetMap;
        inlineLevelBoxAbsoluteBaselineOffsetMap.add(&rootInlineBox, InlineLayoutUnit { });

        auto maximumTopOffsetFromRootInlineBoxBaseline = Optional<InlineLayoutUnit> { };
        if (quirks.inlineLevelBoxAffectsLineBox(rootInlineBox, lineBox))
            maximumTopOffsetFromRootInlineBoxBaseline = rootInlineBox.layoutBounds().ascent;

        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            auto absoluteBaselineOffset = InlineLayoutUnit { };
            if (!inlineLevelBox->hasLineBoxRelativeAlignment()) {
                auto& layoutBox = inlineLevelBox->layoutBox();
                auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
                auto baselineOffsetFromParentBaseline = InlineLayoutUnit { };

                switch (inlineLevelBox->verticalAlign()) {
                case VerticalAlign::Baseline:
                    baselineOffsetFromParentBaseline = { };
                    break;
                case VerticalAlign::Middle: {
                    auto logicalTopOffsetFromParentBaseline = (inlineLevelBox->layoutBounds().height() / 2 + parentInlineBox.style().fontMetrics().xHeight() / 2);
                    baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox->baseline();
                    break;
                }
                case VerticalAlign::BaselineMiddle: {
                    auto logicalTopOffsetFromParentBaseline = inlineLevelBox->layoutBounds().height() / 2;
                    baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox->baseline();
                    break;
                }
                case VerticalAlign::Length: {
                    auto& style = inlineLevelBox->style();
                    auto verticalAlignOffset = floatValueForLength(style.verticalAlignLength(), style.computedLineHeight());
                    auto logicalTopOffsetFromParentBaseline = verticalAlignOffset + inlineLevelBox->baseline();
                    baselineOffsetFromParentBaseline = logicalTopOffsetFromParentBaseline - inlineLevelBox->baseline();
                    break;
                }
                case VerticalAlign::TextTop:
                    baselineOffsetFromParentBaseline = parentInlineBox.baseline() - inlineLevelBox->layoutBounds().ascent;
                    break;
                case VerticalAlign::TextBottom:
                    baselineOffsetFromParentBaseline = inlineLevelBox->layoutBounds().descent - *parentInlineBox.descent();
                    break;
                default:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    break;
                }
                absoluteBaselineOffset = inlineLevelBoxAbsoluteBaselineOffsetMap.get(&parentInlineBox) + baselineOffsetFromParentBaseline;
            } else {
                switch (inlineLevelBox->verticalAlign()) {
                case VerticalAlign::Top: {
                    absoluteBaselineOffset = rootInlineBox.layoutBounds().ascent - inlineLevelBox->layoutBounds().ascent;
                    break;
                }
                case VerticalAlign::Bottom: {
                    absoluteBaselineOffset = inlineLevelBox->layoutBounds().descent - rootInlineBox.layoutBounds().descent;
                    break;
                }
                default:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    break;
                }
            }
            inlineLevelBoxAbsoluteBaselineOffsetMap.add(inlineLevelBox.get(), absoluteBaselineOffset);
            auto affectsRootInlineBoxVerticalPosition = quirks.inlineLevelBoxAffectsLineBox(*inlineLevelBox, lineBox);
            if (affectsRootInlineBoxVerticalPosition) {
                auto topOffsetFromRootInlineBoxBaseline = absoluteBaselineOffset + inlineLevelBox->layoutBounds().ascent;
                if (maximumTopOffsetFromRootInlineBoxBaseline)
                    maximumTopOffsetFromRootInlineBoxBaseline = std::max(*maximumTopOffsetFromRootInlineBoxBaseline, topOffsetFromRootInlineBoxBaseline);
                else {
                    // We are is quirk mode and the root inline box has no content. The root inline box's baseline is anchored at 0.
                    // However negative ascent (e.g negative top margin) can "push" the root inline box upwards and have a negative value.
                    maximumTopOffsetFromRootInlineBoxBaseline = inlineLevelBox->layoutBounds().ascent >= 0
                        ? std::max(0.0f, topOffsetFromRootInlineBoxBaseline)
                        : topOffsetFromRootInlineBoxBaseline;
                }
            }
        }
        auto rootInlineBoxLogicalTop = maximumTopOffsetFromRootInlineBoxBaseline.valueOr(0.f) - rootInlineBox.baseline();
        rootInlineBox.setLogicalTop(rootInlineBoxLogicalTop);
    };
    computeRootInlineBoxVerticalPosition();

    auto alignInlineLevelBoxes = [&] {
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            auto& layoutBox = inlineLevelBox->layoutBox();
            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
            auto logicalTop = InlineLayoutUnit { };
            switch (inlineLevelBox->verticalAlign()) {
            case VerticalAlign::Baseline:
                logicalTop = parentInlineBox.baseline() - inlineLevelBox->baseline();
                break;
            case VerticalAlign::Middle: {
                auto logicalTopOffsetFromParentBaseline = (inlineLevelBox->logicalHeight() / 2 + parentInlineBox.style().fontMetrics().xHeight() / 2);
                logicalTop = parentInlineBox.baseline() - logicalTopOffsetFromParentBaseline;
                break;
            }
            case VerticalAlign::BaselineMiddle: {
                auto logicalTopOffsetFromParentBaseline = inlineLevelBox->logicalHeight() / 2;
                logicalTop = parentInlineBox.baseline() - logicalTopOffsetFromParentBaseline;
                break;
            }
            case VerticalAlign::Length: {
                auto& style = inlineLevelBox->style();
                auto verticalAlignOffset = floatValueForLength(style.verticalAlignLength(), style.computedLineHeight());
                auto logicalTopOffsetFromParentBaseline = verticalAlignOffset + inlineLevelBox->baseline();
                logicalTop = parentInlineBox.baseline() - logicalTopOffsetFromParentBaseline;
                break;
            }
            // Note that (text)top/bottom align their layout bounds.
            case VerticalAlign::TextTop:
                logicalTop = inlineLevelBox->layoutBounds().ascent - inlineLevelBox->baseline();
                break;
            case VerticalAlign::TextBottom:
                logicalTop = parentInlineBox.logicalHeight() - inlineLevelBox->layoutBounds().descent - inlineLevelBox->baseline();
                break;
            case VerticalAlign::Top:
                // Note that this logical top is not relative to the parent inline box.
                logicalTop = inlineLevelBox->layoutBounds().ascent - inlineLevelBox->baseline();
                break;
            case VerticalAlign::Bottom:
                // Note that this logical top is not relative to the parent inline box.
                logicalTop = lineBox.logicalHeight() - inlineLevelBox->layoutBounds().descent - inlineLevelBox->baseline();
                break;
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
            inlineLevelBox->setLogicalTop(logicalTop);
        }
    };
    alignInlineLevelBoxes();
}

LineBox InlineFormattingContext::Geometry::lineBoxForLineContent(const LineBuilder::LineContent& lineContent)
{
    return LineBoxBuilder(formattingContext()).build(lineContent);
}

InlineLayoutUnit InlineFormattingContext::Geometry::logicalTopForNextLine(const LineBuilder::LineContent& lineContent, InlineLayoutUnit previousLineLogicalBottom, const FloatingContext& floatingContext) const
{
    // Normally the next line's logical top is the previous line's logical bottom, but when the line ends
    // with the clear property set, the next line needs to clear the existing floats.
    if (lineContent.runs.isEmpty())
        return previousLineLogicalBottom;
    auto& lastRunLayoutBox = lineContent.runs.last().layoutBox(); 
    if (lastRunLayoutBox.style().clear() == Clear::None)
        return previousLineLogicalBottom;
    auto positionWithClearance = floatingContext.verticalPositionWithClearance(lastRunLayoutBox);
    if (!positionWithClearance)
        return previousLineLogicalBottom;
    return std::max(previousLineLogicalBottom, InlineLayoutUnit(positionWithClearance->position));
}

ContentWidthAndMargin InlineFormattingContext::Geometry::inlineBlockContentWidthAndMargin(const Box& formattingContextRoot, const HorizontalConstraints& horizontalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues)
{
    ASSERT(formattingContextRoot.isInFlow());

    // 10.3.10 'Inline-block', replaced elements in normal flow

    // Exactly as inline replaced elements.
    if (formattingContextRoot.isReplacedBox())
        return inlineReplacedContentWidthAndMargin(downcast<ReplacedBox>(formattingContextRoot), horizontalConstraints, { }, overriddenHorizontalValues);

    // 10.3.9 'Inline-block', non-replaced elements in normal flow

    // If 'width' is 'auto', the used value is the shrink-to-fit width as for floating elements.
    // A computed value of 'auto' for 'margin-left' or 'margin-right' becomes a used value of '0'.
    // #1
    auto width = computedValue(formattingContextRoot.style().logicalWidth(), horizontalConstraints.logicalWidth);
    if (!width)
        width = shrinkToFitWidth(formattingContextRoot, horizontalConstraints.logicalWidth);

    // #2
    auto computedHorizontalMargin = Geometry::computedHorizontalMargin(formattingContextRoot, horizontalConstraints);

    return ContentWidthAndMargin { *width, { computedHorizontalMargin.start.valueOr(0_lu), computedHorizontalMargin.end.valueOr(0_lu) } };
}

ContentHeightAndMargin InlineFormattingContext::Geometry::inlineBlockContentHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(layoutBox.isInFlow());

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block' replaced elements in normal flow and floating replaced elements
    if (layoutBox.isReplacedBox())
        return inlineReplacedContentHeightAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, overriddenVerticalValues);

    // 10.6.6 Complicated cases
    // - 'Inline-block', non-replaced elements.
    return complicatedCases(layoutBox, horizontalConstraints, overriddenVerticalValues);
}

}
}

#endif
