/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#include <wtf/text/StringBuilder.h>

namespace WebCore {

struct CompositionUnderline;
class DocumentMarker;
class TextPainter;

const unsigned short cNoTruncation = USHRT_MAX;
const unsigned short cFullTruncation = USHRT_MAX - 1;

class BufferForAppendingHyphen : public StringBuilder {
public:
    BufferForAppendingHyphen() { reserveCapacity(256); }
};

class InlineTextBox : public InlineBox {
public:
    explicit InlineTextBox(RenderText& renderer)
        : InlineBox(renderer)
        , m_prevTextBox(0)
        , m_nextTextBox(0)
        , m_start(0)
        , m_len(0)
        , m_truncation(cNoTruncation)
    {
        setBehavesLikeText(true);
    }

    virtual ~InlineTextBox();

    RenderText& renderer() const { return toRenderText(InlineBox::renderer()); }
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

    void offsetRun(int d) { ASSERT(!isDirty()); m_start += d; }

    unsigned short truncation() const { return m_truncation; }

    virtual void markDirty(bool dirty = true) override final;

    using InlineBox::hasHyphen;
    using InlineBox::setHasHyphen;
    using InlineBox::canHaveLeadingExpansion;
    using InlineBox::setCanHaveLeadingExpansion;

    static inline bool compareByStart(const InlineTextBox* first, const InlineTextBox* second) { return first->start() < second->start(); }

    virtual int baselinePosition(FontBaseline) const override final;
    virtual LayoutUnit lineHeight() const override final;

    bool emphasisMarkExistsAndIsAbove(const RenderStyle&, bool& isAbove) const;

    LayoutRect logicalOverflowRect() const;
    void setLogicalOverflowRect(const LayoutRect&);
    LayoutUnit logicalTopVisualOverflow() const { return logicalOverflowRect().y(); }
    LayoutUnit logicalBottomVisualOverflow() const { return logicalOverflowRect().maxY(); }
    LayoutUnit logicalLeftVisualOverflow() const { return logicalOverflowRect().x(); }
    LayoutUnit logicalRightVisualOverflow() const { return logicalOverflowRect().maxX(); }

    virtual void dirtyOwnLineBoxes() { dirtyLineBoxes(); }

#ifndef NDEBUG
    virtual void showBox(int = 0) const;
    virtual const char* boxName() const;
#endif

private:
    LayoutUnit selectionTop() const;
    LayoutUnit selectionBottom() const;
    LayoutUnit selectionHeight() const;

    TextRun constructTextRun(const RenderStyle&, const Font&, BufferForAppendingHyphen* = 0) const;
    TextRun constructTextRun(const RenderStyle&, const Font&, String, int maximumLength, BufferForAppendingHyphen* = 0) const;

public:
    virtual FloatRect calculateBoundaries() const { return FloatRect(x(), y(), width(), height()); }

    virtual LayoutRect localSelectionRect(int startPos, int endPos) const;
    bool isSelected(int startPos, int endPos) const;
    void selectionStartEnd(int& sPos, int& ePos);

protected:
    virtual void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom) override;

private:
    virtual void deleteLine() override final;
    virtual void extractLine() override final;
    virtual void attachLine() override final;

public:
    virtual RenderObject::SelectionState selectionState() override final;

private:
    virtual void clearTruncation() override final { m_truncation = cNoTruncation; }
    virtual float placeEllipsisBox(bool flowIsLTR, float visibleLeftEdge, float visibleRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox) override final;

public:
    virtual bool isLineBreak() const override final;

    void setExpansion(int newExpansion)
    {
        m_logicalWidth -= expansion();
        InlineBox::setExpansion(newExpansion);
        m_logicalWidth += newExpansion;
    }

private:
    virtual bool isInlineTextBox() const override final { return true; }

public:
    virtual int caretMinOffset() const override final;
    virtual int caretMaxOffset() const override final;

private:
    float textPos() const; // returns the x position relative to the left start of the text line.

public:
    virtual int offsetForPosition(float x, bool includePartialGlyphs = true) const;
    virtual float positionForOffset(int offset) const;

    // Needs to be public, so the static paintTextWithShadows() function can use it.
    static FloatSize applyShadowToGraphicsContext(GraphicsContext*, const ShadowData*, const FloatRect& textRect, bool stroked, bool opaque, bool horizontal);

protected:
    void paintCompositionBackground(GraphicsContext*, const FloatPoint& boxOrigin, const RenderStyle&, const Font&, int startPos, int endPos);
    void paintDocumentMarkers(GraphicsContext*, const FloatPoint& boxOrigin, const RenderStyle&, const Font&, bool background);
    void paintCompositionUnderline(GraphicsContext*, const FloatPoint& boxOrigin, const CompositionUnderline&);

private:
    void paintDecoration(GraphicsContext&, const FloatPoint& boxOrigin, TextDecoration, TextDecorationStyle, const ShadowData*, TextPainter&);
    void paintSelection(GraphicsContext*, const FloatPoint& boxOrigin, const RenderStyle&, const Font&, Color textColor);
    void paintDocumentMarker(GraphicsContext*, const FloatPoint& boxOrigin, DocumentMarker*, const RenderStyle&, const Font&, bool grammar);
    void paintTextMatchMarker(GraphicsContext*, const FloatPoint& boxOrigin, DocumentMarker*, const RenderStyle&, const Font&);

    void computeRectForReplacementMarker(DocumentMarker*, const RenderStyle&, const Font&);

    TextRun::ExpansionBehavior expansionBehavior() const
    {
        return (canHaveLeadingExpansion() ? TextRun::AllowLeadingExpansion : TextRun::ForbidLeadingExpansion)
            | (expansion() && nextLeafChild() && !nextLeafChild()->isLineBreak() ? TextRun::AllowTrailingExpansion : TextRun::ForbidTrailingExpansion);
    }

    void behavesLikeText() const = delete;

    InlineTextBox* m_prevTextBox; // The previous box that also uses our RenderObject
    InlineTextBox* m_nextTextBox; // The next box that also uses our RenderObject

    int m_start;
    unsigned short m_len;

    // Where to truncate when text overflow is applied. We use special constants to
    // denote no truncation (the whole run paints) and full truncation (nothing paints at all).
    unsigned short m_truncation;
};

INLINE_BOX_OBJECT_TYPE_CASTS(InlineTextBox, isInlineTextBox())

void alignSelectionRectToDevicePixels(FloatRect&);

} // namespace WebCore

#endif // InlineTextBox_h
