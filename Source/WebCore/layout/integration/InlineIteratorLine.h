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

#pragma once

#include "FontBaseline.h"
#include "InlineIteratorLineLegacyPath.h"
#include "InlineIteratorLineModernPath.h"
#include "RenderBlockFlow.h"
#include <variant>

namespace WebCore {
namespace InlineIterator {

class LineIterator;
class PathIterator;
class LeafBoxIterator;

struct EndLineIterator { };

class Line {
public:
    using PathVariant = std::variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        LineIteratorModernPath,
#endif
        LineIteratorLegacyPath
    >;

    Line(PathVariant&&);

    LayoutUnit top() const;
    LayoutUnit bottom() const;

    LayoutUnit lineBoxTop() const;
    LayoutUnit lineBoxBottom() const;
    LayoutUnit lineBoxHeight() const { return lineBoxBottom() - lineBoxTop(); }

    LayoutUnit selectionTop() const;
    LayoutUnit selectionTopForHitTesting() const;
    LayoutUnit selectionBottom() const;
    LayoutUnit selectionHeight() const;

    LayoutRect selectionLogicalRect() const;
    LayoutRect selectionPhysicalRect() const;

    LayoutUnit selectionTopAdjustedForPrecedingBlock() const;
    LayoutUnit selectionHeightAdjustedForPrecedingBlock() const;

    RenderObject::HighlightState selectionState() const;

    float contentLogicalLeft() const;
    float contentLogicalRight() const;
    float contentLogicalWidth() const;

    int blockDirectionPointInLine() const;

    bool isHorizontal() const;

    FontBaseline baselineType() const;

    const RenderBlockFlow& containingBlock() const;

    // FIXME: We may move these multi-column bits to some dedicated structures.
    RenderFragmentContainer* containingFragment() const;
    bool isFirstAfterPageBreak() const;

    bool isFirst() const;

    LeafBoxIterator firstLeafBox() const;
    LeafBoxIterator lastLeafBox() const;

    LineIterator next() const;
    LineIterator previous() const;

    LeafBoxIterator closestRunForPoint(const IntPoint& pointInContents, bool editableOnly) const;
    LeafBoxIterator closestRunForLogicalLeftPosition(int position, bool editableOnly = false) const;
    
    LeafBoxIterator firstSelectedBox() const;
    LeafBoxIterator lastSelectedBox() const;

private:
    friend class LineIterator;

    PathVariant m_pathVariant;
};

class LineIterator {
public:
    LineIterator() : m_line(LineIteratorLegacyPath { nullptr }) { };
    LineIterator(const LegacyRootInlineBox* rootInlineBox) : m_line(LineIteratorLegacyPath { rootInlineBox }) { };
    LineIterator(Line::PathVariant&&);
    LineIterator(const Line&);

    LineIterator& operator++() { return traverseNext(); }
    LineIterator& traverseNext();
    LineIterator& traversePrevious();

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const LineIterator&) const;
    bool operator!=(const LineIterator& other) const { return !(*this == other); }

    bool operator==(EndLineIterator) const { return atEnd(); }
    bool operator!=(EndLineIterator) const { return !atEnd(); }

    const Line& operator*() const { return m_line; }
    const Line* operator->() const { return &m_line; }

    bool atEnd() const;

private:
    Line m_line;
};

LineIterator firstLineFor(const RenderBlockFlow&);
LineIterator lastLineFor(const RenderBlockFlow&);

// -----------------------------------------------

inline Line::Line(PathVariant&& path)
    : m_pathVariant(WTFMove(path))
{
}

inline LayoutUnit Line::top() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.top();
    });
}

inline LayoutUnit Line::bottom() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.bottom();
    });
}

inline LayoutUnit Line::selectionTop() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.selectionTop();
    });
}

inline LayoutUnit Line::selectionTopForHitTesting() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.selectionTopForHitTesting();
    });
}

inline LayoutUnit Line::selectionBottom() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.selectionBottom();
    });
}

inline LayoutUnit Line::selectionHeight() const
{
    return selectionBottom() - selectionTop();
}

inline LayoutUnit Line::lineBoxTop() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.lineBoxTop();
    });
}

inline LayoutUnit Line::lineBoxBottom() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.lineBoxBottom();
    });
}

inline LayoutRect Line::selectionLogicalRect() const
{
    return { LayoutPoint { contentLogicalLeft(), selectionTop() }, LayoutPoint { contentLogicalRight(), selectionBottom() } };
}

inline LayoutRect Line::selectionPhysicalRect() const
{
    auto physicalRect = selectionLogicalRect();
    if (!isHorizontal())
        physicalRect = physicalRect.transposedRect();
    containingBlock().flipForWritingMode(physicalRect);
    return physicalRect;
}

inline float Line::contentLogicalLeft() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.contentLogicalLeft();
    });
}

inline float Line::contentLogicalRight() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.contentLogicalRight();
    });
}

inline float Line::contentLogicalWidth() const
{
    return contentLogicalRight() - contentLogicalLeft();
}

inline bool Line::isHorizontal() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.isHorizontal();
    });
}

inline FontBaseline Line::baselineType() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.baselineType();
    });
}

inline const RenderBlockFlow& Line::containingBlock() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) -> const RenderBlockFlow& {
        return path.containingBlock();
    });
}

inline RenderFragmentContainer* Line::containingFragment() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.containingFragment();
    });
}

inline bool Line::isFirstAfterPageBreak() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.isFirstAfterPageBreak();
    });
}

inline bool Line::isFirst() const
{
    return !previous();
}

}
}

