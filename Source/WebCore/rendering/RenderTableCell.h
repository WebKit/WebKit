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

#include "RenderTableRow.h"
#include "RenderTableSection.h"

namespace WebCore {

static const unsigned unsetColumnIndex = 0x3FFFFFFF;
static const unsigned maxColumnIndex = 0x3FFFFFFE; // 1,073,741,823

enum IncludeBorderColorOrNot { DoNotIncludeBorderColor, IncludeBorderColor };

class RenderTableCell : public RenderBlock {
public:
    explicit RenderTableCell(Node*);

    // FIXME: need to implement cellIndex
    int cellIndex() const { return 0; }
    void setCellIndex(int) { }

    unsigned colSpan() const;
    unsigned rowSpan() const;

    // Called from HTMLTableCellElement.
    void colSpanOrRowSpanChanged();

    void setCol(unsigned column)
    {
        if (UNLIKELY(column > maxColumnIndex))
            CRASH();

        m_column = column;
    }

    unsigned col() const
    {
        ASSERT(m_column != unsetColumnIndex);
        return m_column;
    }

    RenderTableRow* row() const { return toRenderTableRow(parent()); }
    RenderTableSection* section() const { return toRenderTableSection(parent()->parent()); }
    RenderTable* table() const { return toRenderTable(parent()->parent()->parent()); }

    unsigned rowIndex() const
    {
        // This function shouldn't be called on a detached cell.
        ASSERT(row());
        return row()->rowIndex();
    }

    Length styleOrColLogicalWidth() const;

    LayoutUnit logicalHeightForRowSizing() const;

    virtual void computePreferredLogicalWidths();

    void updateLogicalWidth(LayoutUnit);

    virtual int borderLeft() const;
    virtual int borderRight() const;
    virtual int borderTop() const;
    virtual int borderBottom() const;
    virtual int borderStart() const;
    virtual int borderEnd() const;
    virtual int borderBefore() const;
    virtual int borderAfter() const;

    void collectBorderValues(RenderTable::CollapsedBorderValues&) const;
    static void sortBorderValues(RenderTable::CollapsedBorderValues&);

    virtual void layout();

    virtual void paint(PaintInfo&, const LayoutPoint&);

    void paintCollapsedBorders(PaintInfo&, const LayoutPoint&);
    void paintBackgroundsBehindCell(PaintInfo&, const LayoutPoint&, RenderObject* backgroundObject);

    LayoutUnit cellBaselinePosition() const;

    void setIntrinsicPaddingBefore(int p) { m_intrinsicPaddingBefore = p; }
    void setIntrinsicPaddingAfter(int p) { m_intrinsicPaddingAfter = p; }
    void setIntrinsicPadding(int before, int after) { setIntrinsicPaddingBefore(before); setIntrinsicPaddingAfter(after); }
    void clearIntrinsicPadding() { setIntrinsicPadding(0, 0); }

    int intrinsicPaddingBefore() const { return m_intrinsicPaddingBefore; }
    int intrinsicPaddingAfter() const { return m_intrinsicPaddingAfter; }

    virtual LayoutUnit paddingTop() const OVERRIDE;
    virtual LayoutUnit paddingBottom() const OVERRIDE;
    virtual LayoutUnit paddingLeft() const OVERRIDE;
    virtual LayoutUnit paddingRight() const OVERRIDE;
    
    // FIXME: For now we just assume the cell has the same block flow direction as the table.  It's likely we'll
    // create an extra anonymous RenderBlock to handle mixing directionality anyway, in which case we can lock
    // the block flow directionality of the cells to the table's directionality.
    virtual LayoutUnit paddingBefore() const OVERRIDE;
    virtual LayoutUnit paddingAfter() const OVERRIDE;

    void setOverrideHeightFromRowHeight(LayoutUnit);

    virtual void scrollbarsChanged(bool horizontalScrollbarChanged, bool verticalScrollbarChanged);

    bool cellWidthChanged() const { return m_cellWidthChanged; }
    void setCellWidthChanged(bool b = true) { m_cellWidthChanged = b; }

    static RenderTableCell* createAnonymousWithParentRenderer(const RenderObject*);
    virtual RenderBox* createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const OVERRIDE
    {
        return createAnonymousWithParentRenderer(parent);
    }

protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

private:
    virtual const char* renderName() const { return isAnonymous() ? "RenderTableCell (anonymous)" : "RenderTableCell"; }

    virtual bool isTableCell() const { return true; }

    virtual void willBeDestroyed();

    virtual void computeLogicalWidth();

    virtual void paintBoxDecorations(PaintInfo&, const LayoutPoint&);
    virtual void paintMask(PaintInfo&, const LayoutPoint&);

    virtual bool boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance, InlineFlowBox*) const OVERRIDE;

    virtual LayoutSize offsetFromContainer(RenderObject*, const LayoutPoint&, bool* offsetDependsOnPoint = 0) const;
    virtual LayoutRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer) const;
    virtual void computeRectForRepaint(RenderBoxModelObject* repaintContainer, LayoutRect&, bool fixed = false) const;

    int borderHalfLeft(bool outer) const;
    int borderHalfRight(bool outer) const;
    int borderHalfTop(bool outer) const;
    int borderHalfBottom(bool outer) const;

    int borderHalfStart(bool outer) const;
    int borderHalfEnd(bool outer) const;
    int borderHalfBefore(bool outer) const;
    int borderHalfAfter(bool outer) const;

    CollapsedBorderValue collapsedStartBorder(IncludeBorderColorOrNot = IncludeBorderColor) const;
    CollapsedBorderValue collapsedEndBorder(IncludeBorderColorOrNot = IncludeBorderColor) const;
    CollapsedBorderValue collapsedBeforeBorder(IncludeBorderColorOrNot = IncludeBorderColor) const;
    CollapsedBorderValue collapsedAfterBorder(IncludeBorderColorOrNot = IncludeBorderColor) const;

    CollapsedBorderValue cachedCollapsedLeftBorder(RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedRightBorder(RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedTopBorder(RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedBottomBorder(RenderStyle*) const;

    CollapsedBorderValue computeCollapsedStartBorder(IncludeBorderColorOrNot = IncludeBorderColor) const;
    CollapsedBorderValue computeCollapsedEndBorder(IncludeBorderColorOrNot = IncludeBorderColor) const;
    CollapsedBorderValue computeCollapsedBeforeBorder(IncludeBorderColorOrNot = IncludeBorderColor) const;
    CollapsedBorderValue computeCollapsedAfterBorder(IncludeBorderColorOrNot = IncludeBorderColor) const;

    unsigned m_column : 30;
    bool m_cellWidthChanged : 1;
    bool m_hasAssociatedTableCellElement : 1;
    int m_intrinsicPaddingBefore;
    int m_intrinsicPaddingAfter;
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
