/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2010, 2011, 2014 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef InlineTextBox_h
#define InlineTextBox_h

#include "InlineBox.h"
#include "RenderText.h"
#include "TextRun.h"

namespace WebCore {

struct CompositionUnderline;
class RenderCombineText;
class RenderedDocumentMarker;
class TextPainter;
struct TextPaintStyle;

const unsigned short cNoTruncation = USHRT_MAX;
const unsigned short cFullTruncation = USHRT_MAX - 1;

class InlineTextBox : public InlineBox {
public:
    explicit InlineTextBox(RenderText& renderer)
        : InlineBox(renderer)
    {
        setBehavesLikeText(true);
    }

    virtual ~InlineTextBox();

    RenderText& renderer() const { return downcast<RenderText>(InlineBox::renderer()); }
    const RenderStyle& lineStyle() const { return isFirstLine() ? renderer().firstLineStyle() : renderer().style(); }

    InlineTextBox* prevTextBox() const { return m_prevTextBox; }
    InlineTextBox* nextTextBox() const { return m_nextTextBox; }
    void setNextTextBox(InlineTextBox* n) { m_nextTextBox = n; }
    void setPreviousTextBox(InlineTextBox* p) { m_prevTextBox = p; }

    // FIXME: These accessors should ASSERT(!isDirty()). See https://bugs.webkit.org/show_bug.cgi?id=97264
    unsigned start() const { return m_start; }
    unsigned end() const { return m_len ? m_start + m_len - 1 : m_start; }
    unsigned len() const { return m_len; }

    void setStart(unsigned start) { m_start = start; }
    void setLen(unsigned len) { m_len = len; }

    void offsetRun(int d) { ASSERT(!isDirty()); ASSERT(d > 0 || m_start >= static_cast<unsigned>(-d)); m_start += d; }

    unsigned short truncation() const { return m_truncation; }

    void markDirty(bool dirty = true) final;

    using InlineBox::hasHyphen;
    using InlineBox::setHasHyphen;
    using InlineBox::canHaveLeadingExpansion;
    using InlineBox::setCanHaveLeadingExpansion;
    using InlineBox::canHaveTrailingExpansion;
    using InlineBox::setCanHaveTrailingExpansion;
    using InlineBox::forceTrailingExpansion;
    using InlineBox::setForceTrailingExpansion;
    using InlineBox::forceLeadingExpansion;
    using InlineBox::setForceLeadingExpansion;

    static inline bool compareByStart(const InlineTextBox* first, const InlineTextBox* second) { return first->start() < second->start(); }

    int baselinePosition(FontBaseline) const final;
    LayoutUnit lineHeight() const final;

    bool emphasisMarkExistsAndIsAbove(const RenderStyle&, bool& isAbove) const;

    LayoutRect logicalOverflowRect() const;
    void setLogicalOverflowRect(const LayoutRect&);
    LayoutUnit logicalTopVisualOverflow() const { return logicalOverflowRect().y(); }
    LayoutUnit logicalBottomVisualOverflow() const { return logicalOverflowRect().maxY(); }
    LayoutUnit logicalLeftVisualOverflow() const { return logicalOverflowRect().x(); }
    LayoutUnit logicalRightVisualOverflow() const { return logicalOverflowRect().maxX(); }

    virtual void dirtyOwnLineBoxes() { dirtyLineBoxes(); }

#if ENABLE(TREE_DEBUGGING)
    void showLineBox(bool mark, int depth) const final;
    const char* boxName() const final;
#endif

private:
    LayoutUnit selectionTop() const;
    LayoutUnit selectionBottom() const;
    LayoutUnit selectionHeight() const;

    StringView substringToRender(Optional<unsigned> overridingLength = { }) const;
    String hyphenatedStringForTextRun(const RenderStyle&, Optional<unsigned> alternateLength = { }) const;
    TextRun constructTextRun(const RenderStyle&, StringView alternateStringToRender = { }, Optional<unsigned> alternateLength = { }) const;
    TextRun constructTextRun(const RenderStyle&, StringView, unsigned maximumLength) const;

public:
    FloatRect calculateBoundaries() const override { return FloatRect(x(), y(), width(), height()); }

    virtual LayoutRect localSelectionRect(unsigned startPos, unsigned endPos) const;
    bool isSelected(unsigned startPos, unsigned endPos) const;
    std::pair<unsigned, unsigned> selectionStartEnd() const;

protected:
    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction) override;

private:
    void deleteLine() final;
    void extractLine() final;
    void attachLine() final;

public:
    RenderObject::SelectionState selectionState() final;

private:
    void clearTruncation() final { m_truncation = cNoTruncation; }
    float placeEllipsisBox(bool flowIsLTR, float visibleLeftEdge, float visibleRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox) final;

public:
    bool isLineBreak() const final;

private:
    bool isInlineTextBox() const final { return true; }

public:
    int caretMinOffset() const final;
    int caretMaxOffset() const final;

private:
    float textPos() const; // returns the x position relative to the left start of the text line.

public:
    virtual int offsetForPosition(float x, bool includePartialGlyphs = true) const;
    virtual float positionForOffset(unsigned offset) const;

protected:
    void paintCompositionBackground(GraphicsContext&, const FloatPoint& boxOrigin, const RenderStyle&, const FontCascade&, unsigned startPos, unsigned endPos);
    void paintDocumentMarkers(GraphicsContext&, const FloatPoint& boxOrigin, const RenderStyle&, const FontCascade&, bool background);
    void paintCompositionUnderline(GraphicsContext&, const FloatPoint& boxOrigin, const CompositionUnderline&);
    unsigned clampedOffset(unsigned) const;

private:
    void paintDecoration(GraphicsContext&, const FontCascade&, RenderCombineText*, const TextRun&, const FloatPoint& textOrigin, const FloatRect& boxRect,
        TextDecoration, TextPaintStyle, const ShadowData*);
    void paintSelection(GraphicsContext&, const FloatPoint& boxOrigin, const RenderStyle&, const FontCascade&, Color textColor);
    void paintDocumentMarker(GraphicsContext&, const FloatPoint& boxOrigin, RenderedDocumentMarker&, const RenderStyle&, const FontCascade&, bool grammar);
    void paintTextMatchMarker(GraphicsContext&, const FloatPoint& boxOrigin, RenderedDocumentMarker&, const RenderStyle&, const FontCascade&);

    ExpansionBehavior expansionBehavior() const;

    void behavesLikeText() const = delete;

    InlineTextBox* m_prevTextBox { nullptr }; // The previous box that also uses our RenderObject
    InlineTextBox* m_nextTextBox { nullptr }; // The next box that also uses our RenderObject

    unsigned m_start { 0 };
    unsigned short m_len { 0 };

    // Where to truncate when text overflow is applied. We use special constants to
    // denote no truncation (the whole run paints) and full truncation (nothing paints at all).
    unsigned short m_truncation { cNoTruncation };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INLINE_BOX(InlineTextBox, isInlineTextBox())

#endif // InlineTextBox_h
