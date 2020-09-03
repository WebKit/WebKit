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

struct AscentAndDescent {
    InlineLayoutUnit height() const { return ascent + descent; }

    InlineLayoutUnit ascent { 0 };
    InlineLayoutUnit descent { 0 };
};

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

static HangingContent collectHangingContent(const LineBuilder::RunList& runs, LineBox::IsLastLineWithInlineContent isLastLineWithInlineContent)
{
    auto hangingContent = HangingContent { };
    if (isLastLineWithInlineContent == LineBox::IsLastLineWithInlineContent::Yes)
        hangingContent.setIsConditional();
    for (auto& run : WTF::makeReversedRange(runs)) {
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

static Optional<InlineLayoutUnit> horizontalAlignmentOffset(const LineBuilder::RunList& runs, TextAlignMode textAlign, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit contentLogicalWidth, LineBox::IsLastLineWithInlineContent isLastLine)
{
    auto availableWidth = lineLogicalWidth - contentLogicalWidth;
    auto hangingContent = collectHangingContent(runs, isLastLine);
    availableWidth += hangingContent.width();
    if (availableWidth <= 0)
        return { };

    auto computedHorizontalAlignment = [&] {
        if (textAlign != TextAlignMode::Justify)
            return textAlign;
        // Text is justified according to the method specified by the text-justify property,
        // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
        // the last line before a forced break or the end of the block is start-aligned.
        if (runs.last().isLineBreak() || isLastLine == LineBox::IsLastLineWithInlineContent::Yes)
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

inline static AscentAndDescent halfLeadingMetrics(const FontMetrics& fontMetrics, InlineLayoutUnit lineLogicalHeight)
{
    InlineLayoutUnit ascent = fontMetrics.ascent();
    InlineLayoutUnit descent = fontMetrics.descent();
    // 10.8.1 Leading and half-leading
    auto halfLeading = (lineLogicalHeight - (ascent + descent)) / 2;
    // Inline tree height is all integer based.
    return { floorf(ascent + halfLeading), ceilf(descent + halfLeading) };
}

LineBox::InlineBox::InlineBox(const Display::InlineRect& rect, InlineLayoutUnit syntheticBaseline)
    : m_logicalRect(rect)
    , m_baseline(syntheticBaseline)
    , m_isEmpty(false)
    , m_isAtomic(true)
{
}

LineBox::InlineBox::InlineBox(const Display::InlineRect& rect, InlineLayoutUnit baseline, InlineLayoutUnit descent, IsConsideredEmpty isConsideredEmpty)
    : m_logicalRect(rect)
    , m_baseline(baseline)
    , m_descent(descent)
    , m_isEmpty(isConsideredEmpty == IsConsideredEmpty::Yes)
{
}

LineBox::LineBox(const InlineFormattingContext& inlineFormattingContext, const InlineLayoutPoint& topLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit contentLogicalWidth, const LineBuilder::RunList& runs, IsLineVisuallyEmpty isLineVisuallyEmpty, IsLastLineWithInlineContent isLastLineWithInlineContent)
    : m_rect(topLeft, logicalWidth, { })
    , m_contentLogicalWidth(contentLogicalWidth)
    , m_inlineFormattingContext(inlineFormattingContext)
{
    m_lineAlignmentOffset = horizontalAlignmentOffset(runs, root().style().textAlign(), logicalWidth, contentLogicalWidth, isLastLineWithInlineContent);
    if (m_lineAlignmentOffset) {
        // FIXME: line box should not need to be moved, only the inline boxes.
        m_rect.moveHorizontally(*m_lineAlignmentOffset);
    }
    constructInlineBoxes(runs, isLineVisuallyEmpty);
    alignVertically(isLineVisuallyEmpty);
    // Compute scrollable overflow.
    m_scrollableOverflow = Display::InlineRect { topLeft, std::max(logicalWidth, m_rootInlineBox.logicalWidth()), { } };
    auto logicalBottom = m_rootInlineBox.logicalBottom();
    for (auto& inlineBoxEntry : m_inlineBoxRectMap)
        logicalBottom = std::max(logicalBottom, inlineBoxEntry.value->logicalBottom());
    m_scrollableOverflow.expandVertically(logicalBottom);

}

const LineBox::InlineBox& LineBox::inlineBoxForLayoutBox(const Box& layoutBox) const
{
    ASSERT(&layoutBox != &root());
    return *m_inlineBoxRectMap.get(&layoutBox);
}

Display::InlineRect LineBox::inlineRectForTextRun(const LineBuilder::Run& run) const
{
    ASSERT(run.isText() || run.isLineBreak());
    auto inlineBoxRect = m_rootInlineBox.logicalRect();
    if (&run.layoutBox().parent() != &root())
        inlineBoxRect = m_inlineBoxRectMap.get(&run.layoutBox().parent())->logicalRect();
    return { inlineBoxRect.top(), run.logicalLeft(), run.logicalWidth(), inlineBoxRect.height() };
}

void LineBox::constructInlineBoxes(const LineBuilder::RunList& runs, IsLineVisuallyEmpty isLineVisuallyEmpty)
{
    auto lineHasImaginaryStrut = !layoutState().inQuirksMode();
    auto lineIsConsideredEmpty = !lineHasImaginaryStrut || isLineVisuallyEmpty == IsLineVisuallyEmpty::Yes ? InlineBox::IsConsideredEmpty::Yes : InlineBox::IsConsideredEmpty::No;
    auto& fontMetrics = root().style().fontMetrics();
    InlineLayoutUnit rootInlineBoxHeight = fontMetrics.height();
    auto rootInlineRect = Display::InlineRect { { }, { }, contentLogicalWidth(), rootInlineBoxHeight };
    InlineLayoutUnit rootInlineBaseline = fontMetrics.ascent();
    auto rootInlineDescent = rootInlineBoxHeight - rootInlineBaseline;
    m_rootInlineBox = InlineBox { rootInlineRect, rootInlineBaseline, rootInlineDescent, lineIsConsideredEmpty };

    for (auto& run : runs) {
        auto& layoutBox = run.layoutBox();
        if (run.isBox()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                // Spec makes us generate at least one line -even if it is empty.
                auto& lastLineBox = formattingState.displayInlineContent()->lineBoxes.last();
                auto inlineBlockBaseline = lastLineBox.top() + lastLineBox.baseline();
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
                auto adjustedBaseline = boxGeometry.marginBefore() + boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0) + inlineBlockBaseline;
                auto inlineBoxRect = Display::InlineRect { { }, run.logicalLeft(), run.logicalWidth(), boxGeometry.marginBoxHeight() };
                m_inlineBoxRectMap.set(&layoutBox, makeUnique<InlineBox>(inlineBoxRect, adjustedBaseline));
            } else {
                auto runHeight = boxGeometry.marginBoxHeight();
                auto inlineBoxRect = Display::InlineRect { { }, run.logicalLeft(), run.logicalWidth(), runHeight };
                m_inlineBoxRectMap.set(&layoutBox, makeUnique<InlineBox>(inlineBoxRect, runHeight));
            }
        } else if (run.isContainerStart()) {
            auto initialWidth = contentLogicalWidth() - run.logicalLeft();
            ASSERT(initialWidth >= 0);
            auto& fontMetrics = layoutBox.style().fontMetrics();
            InlineLayoutUnit logicalHeight = fontMetrics.height();
            InlineLayoutUnit baseline = fontMetrics.ascent();
            auto inlineBoxRect = Display::InlineRect { { }, run.logicalLeft(), initialWidth, logicalHeight };
            m_inlineBoxRectMap.set(&layoutBox, makeUnique<InlineBox>(inlineBoxRect, baseline, logicalHeight - baseline, InlineBox::IsConsideredEmpty::Yes));
        } else if (run.isContainerEnd()) {
            // Adjust the logical width when the inline level container closes on this line.
            auto& inlineBox = *m_inlineBoxRectMap.get(&layoutBox);
            inlineBox.setLogicalWidth(run.logicalRight() - inlineBox.logicalLeft());
        } else if ((run.isText() || run.isLineBreak())) {
            auto& containerBox = run.layoutBox().parent();
            &containerBox == &root() ? m_rootInlineBox.setIsNonEmpty() : m_inlineBoxRectMap.get(&containerBox)->setIsNonEmpty();
        }
    }
}

void LineBox::alignVertically(IsLineVisuallyEmpty isLineVisuallyEmpty)
{
    if (isLineVisuallyEmpty == IsLineVisuallyEmpty::Yes)
        return;
    // 1. Compute line box height/alignment baseline by placing the inline boxes vertically.
    // 2. Adjust the inline box vertical position.
    auto& rootStyle = root().style();
    // FIXME: We should let line box fully contain the inline boxes and move line spacing/overflow out to Display::LineBox.
    // see webkit.org/b/215087#c9
    InlineLayoutUnit recommendedLineBoxHeight = rootStyle.lineHeight().isNegative() ? rootStyle.fontMetrics().lineSpacing() : rootStyle.computedLineHeight();

    auto contentLogicalHeight = InlineLayoutUnit { };
    auto alignmentBaseline = InlineLayoutUnit { };
    auto computeLineBoxHeight = [&](auto& inlineBox, auto textAlignMode) {
        switch (textAlignMode) {
        case VerticalAlign::Baseline: {
            auto baselineOverflow = inlineBox.baseline() - alignmentBaseline;
            if (baselineOverflow > 0) {
                contentLogicalHeight += baselineOverflow;
                alignmentBaseline += baselineOverflow;
            }
            // Table cells, inline-block boxes may stretch the line beyond their baseline.
            auto belowBaseline = inlineBox.descent().valueOr(inlineBox.logicalHeight() - inlineBox.baseline());
            auto belowBaselineOverflow = belowBaseline - (contentLogicalHeight - alignmentBaseline);
            if (belowBaselineOverflow > 0)
                contentLogicalHeight += belowBaselineOverflow;
            break;
        }
        case VerticalAlign::Top: {
            auto overflow = inlineBox.logicalHeight() - contentLogicalHeight;
            if (overflow > 0)
                contentLogicalHeight += overflow;
            break;
        }
        case VerticalAlign::Bottom: {
            auto overflow = inlineBox.logicalHeight() - contentLogicalHeight;
            if (overflow > 0) {
                contentLogicalHeight += overflow;
                alignmentBaseline += overflow;
            }
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
    };

    auto shouldRootInlineBoxStretchLineBox = !m_rootInlineBox.isEmpty();
    if (shouldRootInlineBoxStretchLineBox)
        computeLineBoxHeight(m_rootInlineBox, VerticalAlign::Baseline);
    for (auto& inlineBoxEntry : m_inlineBoxRectMap) {
        auto& inlineBox = *inlineBoxEntry.value;
        auto shouldInlineBoxStretchLineBox = !inlineBox.isEmpty();
        if (shouldInlineBoxStretchLineBox)
            computeLineBoxHeight(inlineBox, inlineBoxEntry.key->style().verticalAlign());
    }

    auto lineBoxLogicalHeight = rootStyle.lineHeight().isNegative() ? std::max(recommendedLineBoxHeight, contentLogicalHeight) : recommendedLineBoxHeight;
    m_rect.setHeight(lineBoxLogicalHeight);

    auto adjustInlineBoxTop = [&](auto& inlineBox, auto textAlignMode) {
        auto inlineBoxLogicalTop = InlineLayoutUnit { };
        switch (textAlignMode) {
        case VerticalAlign::Baseline:
            inlineBoxLogicalTop = alignmentBaseline - inlineBox.baseline();
            break;
        case VerticalAlign::Top:
            inlineBoxLogicalTop = { };
            break;
        case VerticalAlign::Bottom:
            inlineBoxLogicalTop = logicalBottom() - inlineBox.logicalHeight();
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        inlineBox.setLogicalTop(inlineBoxLogicalTop);
    };

    alignmentBaseline = halfLeadingMetrics(rootStyle.fontMetrics(), lineBoxLogicalHeight).ascent;
    adjustInlineBoxTop(m_rootInlineBox, VerticalAlign::Baseline);
    for (auto& inlineBoxEntry : m_inlineBoxRectMap) {
        auto& layoutBox = *inlineBoxEntry.key;
        auto& inlineBox = *inlineBoxEntry.value;
        adjustInlineBoxTop(inlineBox, layoutBox.style().verticalAlign());
    }
}

}
}

#endif
