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

struct EndLineBoxIterator { };

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

    LineBoxIterator LEGACY_NODELETE next() const;
    LineBoxIterator LEGACY_NODELETE previous() const;

    size_t lineIndex() const { return m_lineIndex; }

private:
    friend class LineBoxIterator;

    LineBox() = default;

    bool atEnd() const { return !m_inlineContent || m_lineIndex == lines().size(); }
    void setAtEnd() { m_lineIndex = lines().size(); }

    const InlineDisplay::Lines& lines() const LIFETIME_BOUND { return m_inlineContent->displayContent().lines; }
    const InlineDisplay::Line& line() const LIFETIME_BOUND { return lines()[m_lineIndex]; }
    InlineDisplay::Line::Ellipsis lineEllipsis() const;

    WeakPtr<const LayoutIntegration::InlineContent> m_inlineContent;
    size_t m_lineIndex { 0 };
};

class LineBoxIterator {
public:
    LineBoxIterator() = default;
    LineBoxIterator(const LineBox&);

    LineBoxIterator& operator++() { return traverseNext(); }
    LineBoxIterator& operator--() { return traversePrevious(); }
    WEBCORE_EXPORT LineBoxIterator& LEGACY_NODELETE traverseNext();
    LineBoxIterator& LEGACY_NODELETE traversePrevious();

    WEBCORE_EXPORT explicit operator bool LEGACY_NODELETE () const;

    bool LEGACY_NODELETE operator==(const LineBoxIterator&) const;
    bool operator==(EndLineBoxIterator) const { return atEnd(); }

    const LineBox& operator*() const LIFETIME_BOUND { return m_lineBox; }
    const LineBox* operator->() const LIFETIME_BOUND { return &m_lineBox; }

    bool LEGACY_NODELETE atEnd() const;

private:
    LineBox m_lineBox;
};

WEBCORE_EXPORT LineBoxIterator firstLineBoxFor(const RenderBlockFlow&);
LineBoxIterator lastLineBoxFor(const RenderBlockFlow&);
LineBoxIterator LEGACY_NODELETE lineBoxFor(const LayoutIntegration::InlineContent&, size_t lineIndex);

LeafBoxIterator closestBoxForHorizontalPosition(const LineBox&, float horizontalPosition, bool editableOnly = false);

inline float previousLineBoxContentBottomOrBorderAndPadding(const LineBox&);
inline float contentStartInBlockDirection(const LineBox&);

// -----------------------------------------------

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
        return line().lineBoxLeft() + line().contentLogicalLeftIgnoringInlineDirection();
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

