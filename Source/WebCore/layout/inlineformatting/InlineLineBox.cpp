/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "InlineLineBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

struct HangingContent {
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

void HangingContent::reset()
{
    m_isConditional = false;
    m_width =  0;
}

LineBox::LineBox(const InlineFormattingContext& inlineFormattingContext, const Display::InlineRect& lineRect, const LineBuilder::RunList& runs, IsLineVisuallyEmpty isLineVisuallyEmpty, IsLastLineWithInlineContent isLastLineWithInlineContent)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_runs(runs)
    , m_rect(lineRect)
    , m_scrollableOverflow(lineRect)
{
#if ASSERT_ENABLED
    m_hasValidAlignmentBaseline = true;
#endif
    auto& rootStyle = inlineFormattingContext.root().style();
    auto halfLeadingMetrics = this->halfLeadingMetrics(rootStyle.fontMetrics(), lineRect.height());

    m_alignmentBaseline = halfLeadingMetrics.ascent;
    m_rootInlineBox = InlineBox { halfLeadingMetrics };

    auto contentLogicalWidth = InlineLayoutUnit { };
    for (auto& run : m_runs) {
        auto runHeight = [&]() -> InlineLayoutUnit {
            auto& fontMetrics = run.style().fontMetrics();
            if (run.isText() || run.isLineBreak())
                return fontMetrics.height();

            if (run.isContainerStart() || run.isContainerEnd())
                return fontMetrics.height();

            auto& layoutBox = run.layoutBox();
            auto& boxGeometry = inlineFormattingContext.geometryForBox(layoutBox);
            if (layoutBox.isReplacedBox() || layoutBox.isFloatingPositioned())
                return boxGeometry.contentBoxHeight();

            // Non-replaced inline box (e.g. inline-block). It looks a bit misleading but their margin box is considered the content height here.
            return boxGeometry.marginBoxHeight();
        };
        m_runRectList.append({ 0, run.logicalLeft(), run.logicalWidth(), runHeight() });
        contentLogicalWidth += run.logicalWidth();
    }
    m_scrollableOverflow.setWidth(std::max(lineRect.width(), contentLogicalWidth));
    alignVertically();
    alignHorizontally(lineRect.width() - contentLogicalWidth, isLastLineWithInlineContent);

    if (isLineVisuallyEmpty == IsLineVisuallyEmpty::Yes) {
        m_rect.setHeight({ });
        m_rootInlineBox.ascentAndDescent = { };
    }
}

void LineBox::alignHorizontally(InlineLayoutUnit availableWidth, IsLastLineWithInlineContent isLastLine)
{
    auto hangingContent = collectHangingContent(isLastLine);
    availableWidth += hangingContent.width();
    if (m_runs.isEmpty() || availableWidth <= 0)
        return;

    auto computedHorizontalAlignment = [&] {
        auto& rootStyle = formattingContext().root().style();
        if (rootStyle.textAlign() != TextAlignMode::Justify)
            return rootStyle.textAlign();
        // Text is justified according to the method specified by the text-justify property,
        // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
        // the last line before a forced break or the end of the block is start-aligned.
        if (m_runs.last().isLineBreak() || isLastLine == IsLastLineWithInlineContent::Yes)
            return TextAlignMode::Start;
        return TextAlignMode::Justify;
    }();

    if (computedHorizontalAlignment == TextAlignMode::Justify) {
        auto accumulatedExpansion = InlineLayoutUnit { };
        for (size_t index = 0; index < m_runs.size(); ++index) {
            m_runRectList[index].moveHorizontally(accumulatedExpansion);

            auto horizontalExpansion = m_runs[index].expansion().horizontalExpansion;
            m_runRectList[index].expandHorizontally(horizontalExpansion);
            accumulatedExpansion += horizontalExpansion;
        }
        return;
    }

    auto adjustmentForAlignment = [&] (auto availableWidth) -> Optional<InlineLayoutUnit> {
        switch (computedHorizontalAlignment) {
        case TextAlignMode::Left:
        case TextAlignMode::WebKitLeft:
        case TextAlignMode::Start:
            return { };
        case TextAlignMode::Right:
        case TextAlignMode::WebKitRight:
        case TextAlignMode::End:
            return std::max<InlineLayoutUnit>(availableWidth, 0);
        case TextAlignMode::Center:
        case TextAlignMode::WebKitCenter:
            return std::max<InlineLayoutUnit>(availableWidth / 2, 0);
        case TextAlignMode::Justify:
            ASSERT_NOT_REACHED();
            break;
        }
        ASSERT_NOT_REACHED();
        return { };
    };

    if (auto adjustment = adjustmentForAlignment(availableWidth)) {
        // FIXME: line box should not need to be moved, only the runs.
        m_rect.moveHorizontally(*adjustment);
        for (auto& runRect : m_runRectList)
            runRect.moveHorizontally(*adjustment);
    }
}

void LineBox::alignVertically()
{
    adjustBaselineAndLineHeight();
    for (size_t index = 0; index < m_runs.size(); ++index) {
        auto& run = m_runs[index];
        auto& runRect = m_runRectList[index];
        auto logicalTop = InlineLayoutUnit { };
        auto& layoutBox = run.layoutBox();
        auto verticalAlign = layoutBox.style().verticalAlign();
        auto ascent = layoutBox.style().fontMetrics().ascent();

        switch (verticalAlign) {
        case VerticalAlign::Baseline:
            if (run.isLineBreak() || run.isText())
                logicalTop = alignmentBaseline() - ascent;
            else if (run.isContainerStart()) {
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                logicalTop = alignmentBaseline() - ascent - boxGeometry.borderTop() - boxGeometry.paddingTop().valueOr(0);
            } else if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                // Spec makes us generate at least one line -even if it is empty.
                auto inlineBlockBaseline = formattingState.displayInlineContent()->lineBoxes.last().baseline();
                // The inline-block's baseline offset is relative to its content box. Let's convert it relative to the margin box.
                //           _______________ <- margin box
                //          |
                //          |  ____________  <- border box
                //          | |
                //          | |  _________  <- content box
                //          | | |   ^
                //          | | |   |  <- baseline offset
                //          | | |   |
                //     text | | |   v text
                //     -----|-|-|---------- <- baseline
                //
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                auto baselineFromMarginBox = boxGeometry.marginBefore() + boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0) + inlineBlockBaseline;
                logicalTop = alignmentBaseline() - baselineFromMarginBox;
            } else {
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                logicalTop = alignmentBaseline() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().valueOr(0_lu) + runRect.height() + boxGeometry.marginAfter());
            }
            break;
        case VerticalAlign::Top:
            logicalTop = 0_lu;
            break;
        case VerticalAlign::Bottom:
            logicalTop = logicalBottom() - runRect.height();
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        runRect.setTop(logicalTop);
        // Adjust scrollable overflow if the run overflows the line.
        m_scrollableOverflow.expandVerticallyToContain(runRect);
        // Convert runs from relative to the line top/left to the formatting root's border box top/left.
        runRect.moveVertically(this->logicalTop());
        runRect.moveHorizontally(logicalLeft());
    }
}

void LineBox::adjustBaselineAndLineHeight()
{
    unsigned inlineContainerNestingLevel = 0;
    auto hasSeenDirectTextOrLineBreak = false;
    for (auto& run : m_runs) {
        auto& layoutBox = run.layoutBox();
        auto& style = layoutBox.style();
        if (run.isText() || run.isLineBreak()) {
            // For text content we set the baseline either through the initial strut (set by the formatting context root) or
            // through the inline container (start). Normally the text content itself does not stretch the line.
            if (inlineContainerNestingLevel) {
                // We've already adjusted the line height/baseline through the parent inline container. 
                continue;
            }
            if (hasSeenDirectTextOrLineBreak) {
                // e.g div>first text</div> or <div><span>nested<span>first direct text</div>.
                continue;
            }
            hasSeenDirectTextOrLineBreak = true;
            continue;
        }

        if (run.isContainerStart()) {
            ++inlineContainerNestingLevel;
            // Inline containers stretch the line by their font size.
            // Vertical margins, paddings and borders don't contribute to the line height.
            auto& fontMetrics = style.fontMetrics();
            if (style.verticalAlign() == VerticalAlign::Baseline) {
                auto halfLeading = LineBox::halfLeadingMetrics(fontMetrics, style.computedLineHeight());
                // Both halfleading ascent and descent could be negative (tall font vs. small line-height value)
                if (halfLeading.descent > 0)
                    setDescentIfGreater(halfLeading.descent);
                if (halfLeading.ascent > 0)
                    setAscentIfGreater(halfLeading.ascent);
                setLogicalHeightIfGreater(ascentAndDescent().height());
            } else
                setLogicalHeightIfGreater(fontMetrics.height());
            continue;
        }

        if (run.isContainerEnd()) {
            // The line's baseline and height have already been adjusted at ContainerStart.
            ASSERT(inlineContainerNestingLevel);
            --inlineContainerNestingLevel;
            continue;
        }

        if (run.isBox()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            auto marginBoxHeight = boxGeometry.marginBoxHeight();

            switch (style.verticalAlign()) {
            case VerticalAlign::Baseline: {
                if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                    // Inline-blocks with inline content always have baselines.
                    auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                    // There has to be at least one line -even if it is empty.
                    auto& lastLineBox = formattingState.displayInlineContent()->lineBoxes.last();
                    auto beforeHeight = boxGeometry.marginBefore() + boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0);
                    setAlignmentBaselineIfGreater(beforeHeight + lastLineBox.baseline());
                    setLogicalHeightIfGreater(marginBoxHeight);
                } else {
                    // Non inline-block boxes sit on the baseline (including their bottom margin).
                    setAscentIfGreater(marginBoxHeight);
                    // Ignore negative descent (yes, negative descent is a thing).
                    setLogicalHeightIfGreater(marginBoxHeight + std::max<InlineLayoutUnit>(0, ascentAndDescent().descent));
                }
                break;
            }
            case VerticalAlign::Top:
                // Top align content never changes the baseline, it only pushes the bottom of the line further down.
                setLogicalHeightIfGreater(marginBoxHeight);
                break;
            case VerticalAlign::Bottom: {
                // Bottom aligned, tall content pushes the baseline further down from the line top.
                auto lineLogicalHeight = logicalHeight();
                if (marginBoxHeight > lineLogicalHeight) {
                    setLogicalHeightIfGreater(marginBoxHeight);
                    setAlignmentBaselineIfGreater(alignmentBaseline() + (marginBoxHeight - lineLogicalHeight));
                }
                break;
            }
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
            continue;
        }
    }
}

HangingContent LineBox::collectHangingContent(IsLastLineWithInlineContent isLastLineWithInlineContent) const
{
    auto hangingContent = HangingContent { };
    if (isLastLineWithInlineContent == IsLastLineWithInlineContent::Yes)
        hangingContent.setIsConditional();
    for (auto& run : WTF::makeReversedRange(m_runs)) {
        if (run.isContainerStart() || run.isContainerEnd())
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

}
}

#endif
