/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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
 */

#ifndef RenderTableCell_h
#define RenderTableCell_h

#include "RenderTableSection.h"

namespace WebCore {

class RenderTableCell : public RenderBlock {
public:
    RenderTableCell(Node*);

    // FIXME: need to implement cellIndex
    int cellIndex() const { return 0; }
    void setCellIndex(int) { }

    int colSpan() const { return m_columnSpan; }
    void setColSpan(int c) { m_columnSpan = c; }

    int rowSpan() const { return m_rowSpan; }
    void setRowSpan(int r) { m_rowSpan = r; }

    int col() const { return m_column; }
    void setCol(int col) { m_column = col; }
    int row() const { return m_row; }
    void setRow(int row) { m_row = row; }

    RenderTableSection* section() const { return toRenderTableSection(parent()->parent()); }
    RenderTable* table() const { return toRenderTable(parent()->parent()->parent()); }

    Length styleOrColWidth() const;

    virtual void calcPrefWidths();

    void updateWidth(int);

    int borderLeft() const;
    int borderRight() const;
    int borderTop() const;
    int borderBottom() const;

    int borderHalfLeft(bool outer) const;
    int borderHalfRight(bool outer) const;
    int borderHalfTop(bool outer) const;
    int borderHalfBottom(bool outer) const;

    CollapsedBorderValue collapsedLeftBorder(bool rtl) const;
    CollapsedBorderValue collapsedRightBorder(bool rtl) const;
    CollapsedBorderValue collapsedTopBorder() const;
    CollapsedBorderValue collapsedBottomBorder() const;

    typedef Vector<CollapsedBorderValue, 100> CollapsedBorderStyles;
    void collectBorderStyles(CollapsedBorderStyles&) const;
    static void sortBorderStyles(CollapsedBorderStyles&);

    virtual void updateFromElement();

    virtual void layout();

    virtual void paint(PaintInfo&, int tx, int ty);

    void paintBackgroundsBehindCell(PaintInfo&, int tx, int ty, RenderObject* backgroundObject);

    virtual int baselinePosition(bool firstLine = false, bool isRootLineBox = false) const;

    void setIntrinsicPaddingTop(int p) { m_intrinsicPaddingTop = p; }
    void setIntrinsicPaddingBottom(int p) { m_intrinsicPaddingBottom = p; }
    void setIntrinsicPadding(int top, int bottom) { setIntrinsicPaddingTop(top); setIntrinsicPaddingBottom(bottom); }
    void clearIntrinsicPadding() { setIntrinsicPadding(0, 0); }

    int intrinsicPaddingTop() const { return m_intrinsicPaddingTop; }
    int intrinsicPaddingBottom() const { return m_intrinsicPaddingBottom; }

    virtual int paddingTop(bool includeIntrinsicPadding = true) const;
    virtual int paddingBottom(bool includeIntrinsicPadding = true) const;

    virtual void setOverrideSize(int);

    bool hasVisibleOverflow() const { return m_overflow; }

protected:
    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

private:
    virtual const char* renderName() const { return isAnonymous() ? "RenderTableCell (anonymous)" : "RenderTableCell"; }

    virtual bool isTableCell() const { return true; }

    virtual void destroy();

    virtual bool requiresLayer() const { return isPositioned() || isTransparent() || hasOverflowClip() || hasTransform() || hasMask() || hasReflection(); }

    virtual void calcWidth();

    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty);
    virtual void paintMask(PaintInfo&, int tx, int ty);

    virtual IntSize offsetFromContainer(RenderObject*, const IntPoint&) const;
    virtual IntRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer);
    virtual void computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect&, bool fixed = false);

    void paintCollapsedBorder(GraphicsContext*, int x, int y, int w, int h);

    int m_row;
    int m_column;
    int m_rowSpan;
    int m_columnSpan;
    int m_intrinsicPaddingTop;
    int m_intrinsicPaddingBottom;
    int m_percentageHeight;
};

inline RenderTableCell* toRenderTableCell(RenderObject* object)
{
    ASSERT(!object || object->isTableCell());
    return static_cast<RenderTableCell*>(object);
}

inline const RenderTableCell* toRenderTableCell(const RenderObject* object)
{
    ASSERT(!object || object->isTableCell());
    return static_cast<const RenderTableCell*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderTableCell(const RenderTableCell*);

} // namespace WebCore

#endif // RenderTableCell_h
