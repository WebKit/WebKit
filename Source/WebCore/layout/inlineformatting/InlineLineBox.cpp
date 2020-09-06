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

static HangingContent collectHangingContent(const Line::RunList& runs, LineBox::IsLastLineWithInlineContent isLastLineWithInlineContent)
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

static Optional<InlineLayoutUnit> horizontalAlignmentOffset(const Line::RunList& runs, TextAlignMode textAlign, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit contentLogicalWidth, LineBox::IsLastLineWithInlineContent isLastLine)
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

LineBox::InlineBox::InlineBox(const Box& layoutBox, const Display::InlineRect& rect, InlineLayoutUnit baseline, InlineLayoutUnit descent, IsConsideredEmpty isConsideredEmpty)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect(rect)
    , m_baseline(baseline)
    , m_descent(descent)
    , m_isEmpty(isConsideredEmpty == IsConsideredEmpty::Yes)
{
}

LineBox::LineBox(const InlineFormattingContext& inlineFormattingContext, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit contentLogicalWidth, const Line::RunList& runs, IsLineVisuallyEmpty isLineVisuallyEmpty, IsLastLineWithInlineContent isLastLineWithInlineContent)
    : m_logicalSize(contentLogicalWidth, { })
    , m_isLineVisuallyEmpty(isLineVisuallyEmpty == IsLineVisuallyEmpty::Yes)
    , m_inlineFormattingContext(inlineFormattingContext)
{
    m_horizontalAlignmentOffset = Layout::horizontalAlignmentOffset(runs, root().style().textAlign(), lineLogicalWidth, contentLogicalWidth, isLastLineWithInlineContent);
    constructInlineBoxes(runs);
    computeInlineBoxesLogicalHeight();
    alignInlineBoxesVerticallyAndComputeLineBoxHeight();
}

Display::InlineRect LineBox::inlineRectForTextRun(const Line::Run& run) const
{
    ASSERT(run.isText() || run.isLineBreak());
    auto& parentInlineBox = inlineBoxForLayoutBox(run.layoutBox().parent());
    auto inlineBoxRect = parentInlineBox.logicalRect();
    return { inlineBoxRect.top(), m_horizontalAlignmentOffset.valueOr(InlineLayoutUnit { }) + run.logicalLeft(), run.logicalWidth(), inlineBoxRect.height() };
}

void LineBox::constructInlineBoxes(const Line::RunList& runs)
{
    auto constructRootInlineBox = [&] {
        auto& fontMetrics = root().style().fontMetrics();
        InlineLayoutUnit logicalHeight = fontMetrics.height();
        auto logicalRect = Display::InlineRect { { }, m_horizontalAlignmentOffset.valueOr(InlineLayoutUnit { }), logicalWidth(), logicalHeight };
        InlineLayoutUnit baseline = fontMetrics.ascent();
        auto descent = logicalHeight - baseline;
        auto lineHasImaginaryStrut = !layoutState().inQuirksMode();
        auto isInitiallyConsideredEmpty =  !m_isLineVisuallyEmpty && lineHasImaginaryStrut ? InlineBox::IsConsideredEmpty::No : InlineBox::IsConsideredEmpty::Yes;
        m_rootInlineBox = InlineBox { root(), logicalRect, baseline, descent, isInitiallyConsideredEmpty };
        m_inlineBoxRectMap.set(&root(), &m_rootInlineBox);
    };
    constructRootInlineBox();

    for (auto& run : runs) {
        auto& layoutBox = run.layoutBox();
        if (run.isBox()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            auto inlineBoxLogicalLeft = m_horizontalAlignmentOffset.valueOr(InlineLayoutUnit { }) + run.logicalLeft();
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
                auto inlineBoxRect = Display::InlineRect { { }, inlineBoxLogicalLeft, run.logicalWidth(), boxGeometry.marginBoxHeight() };
                auto inlineBox = makeUnique<InlineBox>(layoutBox, inlineBoxRect, adjustedBaseline, InlineLayoutUnit { }, InlineBox::IsConsideredEmpty::No);
                m_inlineBoxRectMap.set(&layoutBox, inlineBox.get());
                m_inlineBoxList.append(WTFMove(inlineBox));
            } else {
                auto runHeight = boxGeometry.marginBoxHeight();
                auto inlineBoxRect = Display::InlineRect { { }, inlineBoxLogicalLeft, run.logicalWidth(), runHeight };
                auto inlineBox = makeUnique<InlineBox>(layoutBox, inlineBoxRect, runHeight, InlineLayoutUnit { }, InlineBox::IsConsideredEmpty::No);
                m_inlineBoxRectMap.set(&layoutBox, inlineBox.get());
                m_inlineBoxList.append(WTFMove(inlineBox));
            }
        } else if (run.isContainerStart()) {
            auto inlineBoxLogicalLeft = m_horizontalAlignmentOffset.valueOr(InlineLayoutUnit { }) + run.logicalLeft();
            auto initialWidth = logicalWidth() - run.logicalLeft();
            ASSERT(initialWidth >= 0);
            auto& fontMetrics = layoutBox.style().fontMetrics();
            InlineLayoutUnit logicalHeight = fontMetrics.height();
            InlineLayoutUnit baseline = fontMetrics.ascent();
            auto inlineBoxRect = Display::InlineRect { { }, inlineBoxLogicalLeft, initialWidth, logicalHeight };
            auto inlineBox = makeUnique<InlineBox>(layoutBox, inlineBoxRect, baseline, logicalHeight - baseline, InlineBox::IsConsideredEmpty::Yes);
            m_inlineBoxRectMap.set(&layoutBox, inlineBox.get());
            m_inlineBoxList.append(WTFMove(inlineBox));
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

void LineBox::computeInlineBoxesLogicalHeight()
{
    // By traversing the inline box list backwards, it's guaranteed that descendant inline boxes are sized first.
    for (auto& inlineBox : WTF::makeReversedRange(m_inlineBoxList)) {
        if (inlineBox->isEmpty())
            continue;

        auto& layoutBox = inlineBox->layoutBox();
        switch (layoutBox.style().verticalAlign()) {
        case VerticalAlign::Top:
        case VerticalAlign::Bottom:
            // top and bottom alignments only stretch the line box. They don't stretch any of the inline boxes, not even the root inline box.
            break;
        case VerticalAlign::TextTop: {
            auto& parentInlineBox = inlineBoxForLayoutBox(layoutBox.parent());
            auto parentTextLogicalTop = parentInlineBox.baseline() - parentInlineBox.layoutBox().style().fontMetrics().ascent();
            parentInlineBox.setLogicalHeight(std::max(parentInlineBox.logicalHeight(), parentTextLogicalTop + inlineBox->logicalHeight()));
            break;
        }
        case VerticalAlign::Baseline: {
            auto& parentInlineBox = inlineBoxForLayoutBox(layoutBox.parent());
            auto baselineOverflow = std::max(0.0f, inlineBox->baseline() - parentInlineBox.baseline());
            if (baselineOverflow) {
                parentInlineBox.setBaseline(parentInlineBox.baseline() + baselineOverflow);
                parentInlineBox.setLogicalHeight(parentInlineBox.logicalHeight() + baselineOverflow);
            }
            auto parentLineBoxBelowBaseline = parentInlineBox.logicalHeight() - parentInlineBox.baseline();
            auto inlineBoxBelowBaseline = inlineBox->logicalHeight() - inlineBox->baseline();
            auto belowBaselineOverflow = std::max(0.0f, inlineBoxBelowBaseline - parentLineBoxBelowBaseline);
            if (belowBaselineOverflow)
                parentInlineBox.setLogicalHeight(parentInlineBox.logicalHeight() + belowBaselineOverflow);
            break;
        }
        case VerticalAlign::TextBottom: {
            auto& parentInlineBox = inlineBoxForLayoutBox(layoutBox.parent());
            auto parentTextLogicalBottom = parentInlineBox.baseline() + parentInlineBox.descent().valueOr(InlineLayoutUnit { });
            auto overflow = std::max(0.0f, inlineBox->logicalHeight() - parentTextLogicalBottom);
            if (overflow) {
                // TextBottom pushes the baseline downward the same way 'bottom' does.
                parentInlineBox.setLogicalHeight(parentInlineBox.logicalHeight() + overflow);
                parentInlineBox.setBaseline(parentInlineBox.baseline() + overflow);
            }
            break;
        }
        case VerticalAlign::Middle: {
            auto& parentLayoutBox = layoutBox.parent();
            auto& parentInlineBox = inlineBoxForLayoutBox(parentLayoutBox);
            auto logicalTop = parentInlineBox.baseline() - (inlineBox->logicalHeight() / 2 + parentLayoutBox.style().fontMetrics().xHeight() / 2);
            if (logicalTop < 0) {
                auto overflow = -logicalTop;
                // Child inline box with middle alignment pushes the baseline down when overflows. 
                parentInlineBox.setBaseline(parentInlineBox.baseline() + overflow);
                parentInlineBox.setLogicalHeight(parentInlineBox.logicalHeight() + overflow);
                logicalTop = { };
            }
            auto logicalBottom = logicalTop + inlineBox->logicalHeight();
            parentInlineBox.setLogicalHeight(std::max(parentInlineBox.logicalHeight(), logicalBottom));
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
    }
}

void LineBox::alignInlineBoxesVerticallyAndComputeLineBoxHeight()
{
    // Inline boxes are in the coordinate system of the line box (and not in the coordinate system of their parents).
    // Starting with the root inline box, position the ancestors first so that the descendant line boxes see absolute vertical positions.
    auto contentLogicalHeight = InlineLayoutUnit { };
    auto alignRootInlineBox = [&] {
        contentLogicalHeight = m_rootInlineBox.logicalHeight();
        for (auto& inlineBox : m_inlineBoxList) {
            auto verticalAlign = inlineBox->layoutBox().style().verticalAlign();
            if (verticalAlign == VerticalAlign::Bottom) {
                // bottom align always pushes the root inline box downwards.
                auto overflow = std::max(0.0f, inlineBox->logicalBottom() - m_rootInlineBox.logicalBottom());
                m_rootInlineBox.setLogicalTop(m_rootInlineBox.logicalTop() + overflow);
                contentLogicalHeight += overflow;
            } else if (verticalAlign == VerticalAlign::Top)
                contentLogicalHeight = std::max(contentLogicalHeight, inlineBox->logicalHeight());
        }
    };
    alignRootInlineBox();

    for (auto& inlineBox : m_inlineBoxList) {
        auto inlineBoxLogicalTop = InlineLayoutUnit { };
        auto& layoutBox = inlineBox->layoutBox();
        switch (layoutBox.style().verticalAlign()) {
        case VerticalAlign::Baseline: {
            auto& parentInlineBox = inlineBoxForLayoutBox(layoutBox.parent());
            inlineBoxLogicalTop = parentInlineBox.logicalTop() + parentInlineBox.baseline() - inlineBox->baseline();
            break;
        }
        case VerticalAlign::TextTop: {
            auto& parentLayoutBox = layoutBox.parent();
            auto& parentInlineBox = inlineBoxForLayoutBox(parentLayoutBox);
            inlineBoxLogicalTop = parentInlineBox.logicalTop() + parentInlineBox.baseline() - parentLayoutBox.style().fontMetrics().ascent();
            break;
        }
        case VerticalAlign::TextBottom: {
            auto& parentLayoutBox = layoutBox.parent();
            auto& parentInlineBox = inlineBoxForLayoutBox(parentLayoutBox);
            auto parentTextLogicalBottom = parentInlineBox.logicalTop() + parentInlineBox.baseline() + parentLayoutBox.style().fontMetrics().descent();
            inlineBoxLogicalTop = parentTextLogicalBottom - inlineBox->logicalHeight();
            break;
        }
        case VerticalAlign::Top:
            inlineBoxLogicalTop = InlineLayoutUnit { };
            break;
        case VerticalAlign::Bottom:
            inlineBoxLogicalTop = contentLogicalHeight - inlineBox->logicalHeight();
            break;
        case VerticalAlign::Middle: {
            auto& parentLayoutBox = layoutBox.parent();
            auto& parentInlineBox = inlineBoxForLayoutBox(parentLayoutBox);
            inlineBoxLogicalTop = parentInlineBox.logicalTop() + parentInlineBox.baseline() - (inlineBox->logicalHeight() / 2 + parentLayoutBox.style().fontMetrics().xHeight() / 2);
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        inlineBox->setLogicalTop(inlineBoxLogicalTop);
    }

    auto adjustContentBottomIfNeeded = [&] {
        if (!m_rootInlineBox.isEmpty())
            return;
        auto collapsedContentLogicalTop = contentLogicalHeight;
        auto collapsedContentLogicalBottom = InlineLayoutUnit { };
        for (auto& inlineBox : m_inlineBoxList) {
            auto verticalAlign = inlineBox->layoutBox().style().verticalAlign();
            auto stretchesLineBox = verticalAlign == VerticalAlign::Bottom || verticalAlign == VerticalAlign::Top || &inlineBox->layoutBox().parent() == &m_rootInlineBox.layoutBox();
            if (!stretchesLineBox)
                continue;
            if (inlineBox->isEmpty())
                continue;
            collapsedContentLogicalTop = std::min(collapsedContentLogicalTop, inlineBox->logicalTop());
            collapsedContentLogicalBottom = std::max(collapsedContentLogicalBottom, inlineBox->logicalBottom());
        }
        if (!collapsedContentLogicalTop) {
            contentLogicalHeight = collapsedContentLogicalBottom;
            return;
        }
        contentLogicalHeight = InlineLayoutUnit { };
        for (auto& inlineBox : m_inlineBoxList) {
            inlineBox->setLogicalTop(inlineBox->logicalTop() - collapsedContentLogicalTop);
            contentLogicalHeight = std::max(contentLogicalHeight, inlineBox->logicalBottom());
        }
    };
    adjustContentBottomIfNeeded();
    if (!m_isLineVisuallyEmpty)
        m_logicalSize.setHeight(contentLogicalHeight);
}

const InlineFormattingContext& LineBox::formattingContext() const
{
    return m_inlineFormattingContext;
}

const Box& LineBox::root() const
{
    return formattingContext().root();
}

LayoutState& LineBox::layoutState() const
{
    return formattingContext().layoutState();
}

}
}

#endif
