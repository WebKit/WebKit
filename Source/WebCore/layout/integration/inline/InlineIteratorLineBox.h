/*
 * Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/FontBaseline.h>
#include <WebCore/InlineIteratorBox.h>
#include <WebCore/LayoutIntegrationInlineContent.h>
#include <WebCore/RenderBlockFlow.h>

namespace WebCore {

class LineSelection;

namespace InlineIterator {

class LineBoxIterator;

class LineBox {
public:
    LineBox(const LayoutIntegration::InlineContent&, size_t lineIndex);

    float logicalTop() const;
    float logicalBottom() const;
    float logicalHeight() const { return logicalBottom() - logicalTop(); }
    float logicalWidth() const;

    float contentLogicalTop() const;
    float contentLogicalBottom() const;
    float contentLogicalLeft() const;
    float contentLogicalRight() const;
    float contentLogicalWidth() const;
    float contentLogicalHeight() const;

    float contentLogicalTopAdjustedForPrecedingLineBox() const;
    float contentLogicalBottomAdjustedForFollowingLineBox() const;

    float inkOverflowLogicalTop() const;
    float inkOverflowLogicalBottom() const;
    float scrollableOverflowTop() const;
    float scrollableOverflowBottom() const;

    const Style::ComputedStyle& style() const LIFETIME_BOUND { return isFirst() ? formattingContextRoot().firstLineStyle() : formattingContextRoot().style(); }

    bool hasEllipsis() const;
    enum AdjustedForSelection : bool { No, Yes };
    FloatRect ellipsisVisualRect(AdjustedForSelection = AdjustedForSelection::No) const;
    TextRun ellipsisText() const;
    RenderObject::HighlightState ellipsisSelectionState() const;

    const RenderBlockFlow& formattingContextRoot() const;

    bool isHorizontal() const;
    FontBaseline baselineType() const;

    bool isFirst() const;
    bool isFirstAfterPageBreak() const;

    bool hasBlockContent() const;
    LeafBoxIterator LEGACY_NODELETE blockLevelBox() const;
    inline bool hasContentfulInFlowBox() const;

    // Text-relative left/right
    LeafBoxIterator LEGACY_NODELETE lineLeftmostLeafBox() const;
    LeafBoxIterator LEGACY_NODELETE lineRightmostLeafBox() const;
    // Coordinate-relative left/right
    inline LeafBoxIterator logicalLeftmostLeafBox() const;
    inline LeafBoxIterator logicalRightmostLeafBox() const;

    // Leaf boxes from line-left to line-right.
    LeafBoxRange leafBoxes() const;

    LineBoxIterator LEGACY_NODELETE next() const;
    LineBoxIterator LEGACY_NODELETE previous() const;

    size_t lineIndex() const { return m_lineIndex; }

    bool operator==(const LineBox& other) const { return m_inlineContent == other.m_inlineContent && m_lineIndex == other.m_lineIndex; }

private:
    friend class LineBoxIterator;
    friend class LineBoxRange;

    LineBox() = default;

    bool atEnd() const { return !m_inlineContent || m_lineIndex == lines().size(); }
    void setAtEnd() { m_lineIndex = lines().size(); }

    const InlineDisplay::Lines& lines() const LIFETIME_BOUND { return m_inlineContent->displayContent().lines; }
    const InlineDisplay::Line& line() const LIFETIME_BOUND { return lines()[m_lineIndex]; }
    InlineDisplay::Line::Ellipsis lineEllipsis() const;

    SingleThreadWeakPtr<const LayoutIntegration::InlineContent> m_inlineContent;
    size_t m_lineIndex { 0 };
};

// Bidirectional and works with std::ranges. Like box iterators, it is also a nullable line box reference: it can be
// positioned at any line, tests false at the end, and is at the end once the inline content is destroyed.
// Decrementing past the first line also moves it to the end. Dereferencing returns the line box handle by value.
class LineBoxIterator {
public:
    using value_type = LineBox;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::bidirectional_iterator_tag;
    using iterator_category = std::input_iterator_tag;

    LineBoxIterator() = default;
    LineBoxIterator(const LineBox& lineBox)
        : m_lineBox(lineBox)
    {
    }

    const LineBox operator*() const { ASSERT(!m_lineBox.atEnd()); return m_lineBox; }
    const LineBox* operator->() const LIFETIME_BOUND { ASSERT(!m_lineBox.atEnd()); return &m_lineBox; }

    explicit operator bool() const { return !atEnd(); }
    bool atEnd() const { return m_lineBox.atEnd(); }

    LineBoxIterator& operator++()
    {
        ASSERT(!m_lineBox.atEnd());
        ++m_lineBox.m_lineIndex;
        return *this;
    }

    LineBoxIterator& operator--()
    {
        if (!m_lineBox.m_inlineContent)
            return *this;
        if (!m_lineBox.m_lineIndex) {
            m_lineBox.setAtEnd();
            return *this;
        }
        --m_lineBox.m_lineIndex;
        return *this;
    }

    LineBoxIterator& traverseNext() { return ++*this; }
    LineBoxIterator& traversePrevious() { return --*this; }

    LineBoxIterator operator++(int)
    {
        auto copy = *this;
        ++*this;
        return copy;
    }

    LineBoxIterator operator--(int)
    {
        auto copy = *this;
        --*this;
        return copy;
    }

    bool operator==(const LineBoxIterator& other) const
    {
        if (m_lineBox.atEnd() || other.m_lineBox.atEnd())
            return m_lineBox.atEnd() == other.m_lineBox.atEnd();
        return m_lineBox == other.m_lineBox;
    }

private:
    friend class LineBoxRange;

    LineBox m_lineBox;
};

class LineBoxRange : public std::ranges::view_interface<LineBoxRange> {
public:
    using Iterator = LineBoxIterator;

    LineBoxRange() = default;
    LineBoxRange(const LineBoxIterator& first)
    {
        if (!first)
            return;
        m_begin = first;
        m_end = LineBox { *first->m_inlineContent, first->lines().size() };
    }

    Iterator begin() const { return m_begin; }
    Iterator end() const { return m_end; }

private:
    Iterator m_begin;
    Iterator m_end;
};

WEBCORE_EXPORT LineBoxIterator firstLineBoxFor(const RenderBlockFlow&);
LineBoxIterator lastLineBoxFor(const RenderBlockFlow&);

inline LineBoxRange lineBoxesFor(const RenderBlockFlow&);
LineBox LEGACY_NODELETE lineBoxFor(const LayoutIntegration::InlineContent&, size_t lineIndex);

LeafBoxIterator closestBoxForHorizontalPosition(const LineBox&, float horizontalPosition, bool editableOnly = false);

inline float previousLineBoxContentBottomOrBorderAndPadding(const LineBox&);
inline float contentStartInBlockDirection(const LineBox&);

// -----------------------------------------------

inline LineBoxRange lineBoxesFor(const RenderBlockFlow& flow)
{
    return { firstLineBoxFor(flow) };
}

inline LineBox::LineBox(const LayoutIntegration::InlineContent& inlineContent, size_t lineIndex)
    : m_inlineContent(&inlineContent)
    , m_lineIndex(lineIndex)
{
    ASSERT(lineIndex <= lines().size());
}

inline float LineBox::contentLogicalTop() const
{
    return line().enclosingContentLogicalTop();
}

inline float LineBox::contentLogicalBottom() const
{
    return line().enclosingContentLogicalBottom();
}

inline float LineBox::contentLogicalTopAdjustedForPrecedingLineBox() const
{
    if (formattingContextRoot().writingMode().isLineInverted() || !m_lineIndex)
        return contentLogicalTop();
    for (auto precedingLineIndex = m_lineIndex; precedingLineIndex--;) {
        auto precedingLineBox = LineBox { *m_inlineContent, precedingLineIndex };
        if (!precedingLineBox.hasContentfulInFlowBox())
            continue;
        if (precedingLineBox.hasBlockContent())
            break;
        if (precedingLineBox.logicalBottom() < logicalTop())
            break;
        return precedingLineBox.contentLogicalBottom();
    }
    return contentLogicalTop();
}

inline float LineBox::contentLogicalBottomAdjustedForFollowingLineBox() const
{
    if (!formattingContextRoot().writingMode().isLineInverted() || m_lineIndex == lines().size() - 1)
        return contentLogicalBottom();
    auto followingLineBox = LineBox { *m_inlineContent, m_lineIndex + 1 };
    if (followingLineBox.hasBlockContent())
        return contentLogicalBottom();
    return followingLineBox.contentLogicalTop();
}

inline float LineBox::logicalTop() const
{
    return line().lineBoxLogicalRect().y();
}

inline float LineBox::logicalBottom() const
{
    return line().lineBoxLogicalRect().maxY();
}

inline float LineBox::logicalWidth() const
{
    return line().lineBoxLogicalRect().width();
}

inline float LineBox::inkOverflowLogicalTop() const
{
    return line().isHorizontal() ? line().inkOverflow().y() : line().inkOverflow().x();
}

inline float LineBox::inkOverflowLogicalBottom() const
{
    return line().isHorizontal() ? line().inkOverflow().maxY() : line().inkOverflow().maxX();
}

inline float LineBox::scrollableOverflowTop() const
{
    return line().scrollableOverflow().y();
}

inline float LineBox::scrollableOverflowBottom() const
{
    return line().scrollableOverflow().maxY();
}

inline bool LineBox::hasEllipsis() const
{
    return line().hasEllipsis();
}

inline FloatRect LineBox::ellipsisVisualRect(AdjustedForSelection adjustedForSelection) const
{
    ASSERT(hasEllipsis());

    auto visualRect = lineEllipsis().visualRect;

    // FIXME: Add pixel snapping here.
    if (adjustedForSelection == AdjustedForSelection::No) {
        formattingContextRoot().flipForWritingMode(visualRect);
        return visualRect;
    }
    auto selectionTop = formattingContextRoot().adjustEnclosingTopForPrecedingBlock(LayoutUnit { contentLogicalTopAdjustedForPrecedingLineBox() });
    auto selectionBottom = contentLogicalBottomAdjustedForFollowingLineBox();

    visualRect.setY(selectionTop);
    visualRect.setHeight(selectionBottom - selectionTop);
    formattingContextRoot().flipForWritingMode(visualRect);
    return visualRect;
}

inline TextRun LineBox::ellipsisText() const
{
    ASSERT(hasEllipsis());

    return TextRun { lineEllipsis().text.string(), 0, 0, ExpansionBehavior::defaultBehavior(),
        line().isLeftToRightInlineDirection() ? TextDirection::LTR : TextDirection::RTL };
}

inline float LineBox::contentLogicalLeft() const
{
    auto writingMode = formattingContextRoot().writingMode();
    if (writingMode.isLogicalLeftLineLeft())
        return line().lineBoxLogicalRect().x() + line().contentLogicalLeftIgnoringInlineDirection();
    ASSERT(writingMode.isVertical()); // Currently only sideways-lr gets this far.
    return line().bottom() - (line().contentLogicalLeftIgnoringInlineDirection() + line().contentLogicalWidth());
}

inline float LineBox::contentLogicalRight() const
{
    return contentLogicalLeft() + line().contentLogicalWidth();
}

inline float LineBox::contentLogicalWidth() const
{
    return contentLogicalRight() - contentLogicalLeft();
}

inline float LineBox::contentLogicalHeight() const
{
    return contentLogicalBottom() - contentLogicalTop();
}

inline bool LineBox::isHorizontal() const
{
    return line().isHorizontal();
}

inline FontBaseline LineBox::baselineType() const
{
    return line().baselineType();
}

inline const RenderBlockFlow& LineBox::formattingContextRoot() const
{
    return m_inlineContent->formattingContextRoot();
}

inline bool LineBox::isFirstAfterPageBreak() const
{
    return line().isFirstAfterPageBreak();
}

inline bool LineBox::isFirst() const
{
    return !previous();
}

inline bool LineBox::hasBlockContent() const
{
    return line().hasBlockLevelBox();
}

inline bool LineBox::hasContentfulInFlowBox() const
{
    return line().hasContentfulInFlowBox();
}

inline InlineDisplay::Line::Ellipsis LineBox::lineEllipsis() const
{
    if (auto ellipsis = m_inlineContent->displayContent().lineEllipsis(m_lineIndex))
        return *ellipsis;
    ASSERT_NOT_REACHED();
    return { };
}

}
}

template<>
inline constexpr bool std::ranges::enable_borrowed_range<WebCore::InlineIterator::LineBoxRange> = true;
