/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "LegacyInlineBox.h"
#include "MarkedText.h"
#include "RenderText.h"
#include "TextRun.h"

namespace WebCore {

class RenderCombineText;
class RenderedDocumentMarker;
class TextPainter;
struct CompositionUnderline;
struct MarkedText;
struct StyledMarkedText;
struct TextBoxSelectableRange;
struct TextPaintStyle;

const unsigned short cNoTruncation = USHRT_MAX;
const unsigned short cFullTruncation = USHRT_MAX - 1;

class LegacyInlineTextBox : public LegacyInlineBox {
    WTF_MAKE_ISO_ALLOCATED(LegacyInlineTextBox);
public:
    explicit LegacyInlineTextBox(RenderText& renderer)
        : LegacyInlineBox(renderer)
    {
        setBehavesLikeText(true);
    }

    virtual ~LegacyInlineTextBox();

    RenderText& renderer() const { return downcast<RenderText>(LegacyInlineBox::renderer()); }
    const RenderStyle& lineStyle() const { return isFirstLine() ? renderer().firstLineStyle() : renderer().style(); }

    LegacyInlineTextBox* prevTextBox() const { return m_prevTextBox; }
    LegacyInlineTextBox* nextTextBox() const { return m_nextTextBox; }
    void setNextTextBox(LegacyInlineTextBox* n) { m_nextTextBox = n; }
    void setPreviousTextBox(LegacyInlineTextBox* p) { m_prevTextBox = p; }

    bool hasTextContent() const;

    // These functions do not account for combined text. For combined text this box will always have len() == 1
    // regardless of whether the resulting composition is the empty string. Use hasTextContent() if you want to
    // know whether this box has text content.
    //
    // FIXME: These accessors should ASSERT(!isDirty()). See https://bugs.webkit.org/show_bug.cgi?id=97264
    // Note len() == 1 for combined text regardless of whether the composition is empty. Use hasTextContent() to
    unsigned start() const { return m_start; }
    unsigned end() const { return m_start + m_len; }
    unsigned len() const { return m_len; }

    void setStart(unsigned start) { m_start = start; }
    void setLen(unsigned len) { m_len = len; }

    void offsetRun(int d) { ASSERT(!isDirty()); ASSERT(d > 0 || m_start >= static_cast<unsigned>(-d)); m_start += d; }

    unsigned short truncation() const { return m_truncation; }

    TextBoxSelectableRange selectableRange() const;

    void markDirty(bool dirty = true) final;

    using LegacyInlineBox::hasHyphen;
    using LegacyInlineBox::setHasHyphen;
    using LegacyInlineBox::canHaveLeftExpansion;
    using LegacyInlineBox::setCanHaveLeftExpansion;
    using LegacyInlineBox::canHaveRightExpansion;
    using LegacyInlineBox::setCanHaveRightExpansion;
    using LegacyInlineBox::forceRightExpansion;
    using LegacyInlineBox::setForceRightExpansion;
    using LegacyInlineBox::forceLeftExpansion;
    using LegacyInlineBox::setForceLeftExpansion;

    static inline bool compareByStart(const LegacyInlineTextBox* first, const LegacyInlineTextBox* second) { return first->start() < second->start(); }

    LayoutUnit baselinePosition(FontBaseline) const final;
    LayoutUnit lineHeight() const final;

    std::optional<bool> emphasisMarkExistsAndIsAbove(const RenderStyle&) const;

    LayoutRect logicalOverflowRect() const;
    void setLogicalOverflowRect(const LayoutRect&);
    LayoutUnit logicalTopVisualOverflow() const { return logicalOverflowRect().y(); }
    LayoutUnit logicalBottomVisualOverflow() const { return logicalOverflowRect().maxY(); }
    LayoutUnit logicalLeftVisualOverflow() const { return logicalOverflowRect().x(); }
    LayoutUnit logicalRightVisualOverflow() const { return logicalOverflowRect().maxX(); }

    virtual void dirtyOwnLineBoxes() { dirtyLineBoxes(); }

#if ENABLE(TREE_DEBUGGING)
    void outputLineBox(WTF::TextStream&, bool mark, int depth) const final;
    const char* boxName() const final;
#endif

private:
    LayoutUnit selectionTop() const;
    LayoutUnit selectionBottom() const;
    LayoutUnit selectionHeight() const;

public:
    virtual LayoutRect localSelectionRect(unsigned startPos, unsigned endPos) const;
    bool isSelectable(unsigned startPosition, unsigned endPosition) const;
    std::pair<unsigned, unsigned> selectionStartEnd() const;

protected:
    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction) override;

private:
    void deleteLine() final;
    void extractLine() final;
    void attachLine() final;
    
public:
    RenderObject::HighlightState selectionState() final;

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

    bool hasMarkers() const;
    FloatRect calculateUnionOfAllDocumentMarkerBounds() const;
    FloatRect calculateDocumentMarkerBounds(const MarkedText&) const;

private:
    FloatPoint textOriginFromBoxRect(const FloatRect&) const;

    void paintMarkedTexts(PaintInfo&, MarkedText::PaintPhase, const FloatRect& boxRect, const Vector<StyledMarkedText>&, const FloatRect& decorationClipOutRect = { });

    void paintPlatformDocumentMarker(GraphicsContext&, const FloatPoint& boxOrigin, const MarkedText&);
    void paintPlatformDocumentMarkers(GraphicsContext&, const FloatPoint& boxOrigin);

    void paintCompositionBackground(PaintInfo&, const FloatPoint& boxOrigin);
    void paintCompositionUnderlines(PaintInfo&, const FloatPoint& boxOrigin) const;
    void paintCompositionUnderline(PaintInfo&, const FloatPoint& boxOrigin, const CompositionUnderline&) const;

    enum class MarkedTextBackgroundStyle : bool { Default, Rounded };
    void paintMarkedTextBackground(PaintInfo&, const FloatPoint& boxOrigin, const Color&, unsigned clampedStartOffset, unsigned clampedEndOffset, MarkedTextBackgroundStyle = MarkedTextBackgroundStyle::Default);
    void paintMarkedTextForeground(PaintInfo&, const FloatRect& boxRect, const StyledMarkedText&);
    void paintMarkedTextDecoration(PaintInfo&, const FloatRect& boxRect, const FloatRect& clipOutRect, const StyledMarkedText&);

    const RenderCombineText* combinedText() const;
    const FontCascade& lineFont() const;

    ShadowData* debugTextShadow();

    String text(bool ignoreCombinedText = false, bool ignoreHyphen = false) const; // The effective text for the run.
    TextRun createTextRun(bool ignoreCombinedText = false, bool ignoreHyphen = false) const;

    ExpansionBehavior expansionBehavior() const;

    void behavesLikeText() const = delete;

    LegacyInlineTextBox* m_prevTextBox { nullptr }; // The previous box that also uses our RenderObject
    LegacyInlineTextBox* m_nextTextBox { nullptr }; // The next box that also uses our RenderObject

    unsigned m_start { 0 };
    unsigned short m_len { 0 };

    // Where to truncate when text overflow is applied. We use special constants to
    // denote no truncation (the whole run paints) and full truncation (nothing paints at all).
    unsigned short m_truncation { cNoTruncation };
};

LayoutRect snappedSelectionRect(const LayoutRect&, float logicalRight, float selectionTop, float selectionHeight, bool isHorizontal);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INLINE_BOX(LegacyInlineTextBox, isInlineTextBox())
