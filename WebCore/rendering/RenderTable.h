/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef RenderTable_h
#define RenderTable_h

#include "RenderBlock.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderTableCol;
class RenderTableCell;
class RenderTableSection;
class TableLayout;

class RenderTable : public RenderBlock {
public:
    enum Rules {
        None    = 0x00,
        RGroups = 0x01,
        CGroups = 0x02,
        Groups  = 0x03,
        Rows    = 0x05,
        Cols    = 0x0a,
        All     = 0x0f
    };
    enum Frame {
        Void   = 0x00,
        Above  = 0x01,
        Below  = 0x02,
        Lhs    = 0x04,
        Rhs    = 0x08,
        Hsides = 0x03,
        Vsides = 0x0c,
        Box    = 0x0f
    };

    RenderTable(Node*);
    ~RenderTable();

    virtual const char* renderName() const { return "RenderTable"; }

    virtual bool isTable() const { return true; }

    virtual void setStyle(RenderStyle*);

    virtual bool avoidsFloats() const { return true; }

    int getColumnPos(int col) const { return m_columnPos[col]; }

    int hBorderSpacing() const { return m_hSpacing; }
    int vBorderSpacing() const { return m_vSpacing; }
    
    bool collapseBorders() const { return style()->borderCollapse(); }
    int borderLeft() const { return m_borderLeft; }
    int borderRight() const { return m_borderRight; }
    int borderTop() const;
    int borderBottom() const;
    
    Rules getRules() const { return static_cast<Rules>(m_rules); }

    const Color& bgColor() const { return style()->backgroundColor(); }

    int outerBorderTop() const;
    int outerBorderBottom() const;
    int outerBorderLeft() const;
    int outerBorderRight() const;
    
    int calcBorderLeft() const;
    int calcBorderRight() const;
    void recalcHorizontalBorders();

    // overrides
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0);
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty);
    virtual void layout();
    virtual void calcPrefWidths();

    virtual RenderBlock* firstLineBlock() const;
    virtual void updateFirstLetter();
    
    virtual void setCellWidths();

    virtual void calcWidth();

    struct ColumnStruct {
        enum {
            WidthUndefined = 0xffff
        };

        ColumnStruct()
            : span(1)
            , width(WidthUndefined)
        {
        }

        unsigned short span;
        unsigned width; // the calculated position of the column
    };

    Vector<ColumnStruct>& columns() { return m_columns; }
    Vector<int>& columnPositions() { return m_columnPos; }
    RenderTableSection* header() const { return m_head; }
    RenderTableSection* footer() const { return m_foot; }
    RenderTableSection* firstBody() const { return m_firstBody; }

    void splitColumn(int pos, int firstSpan);
    void appendColumn(int span);
    int numEffCols() const { return m_columns.size(); }
    int spanOfEffCol(int effCol) const { return m_columns[effCol].span; }
    
    int colToEffCol(int col) const
    {
        int i = 0;
        int effCol = numEffCols();
        for (int c = 0; c < col && i < effCol; ++i)
            c += m_columns[i].span;
        return i;
    }
    
    int effColToCol(int effCol) const
    {
        int c = 0;
        for (int i = 0; i < effCol; i++)
            c += m_columns[i].span;
        return c;
    }

    int bordersPaddingAndSpacing() const
    {
        return borderLeft() + borderRight() +
               (collapseBorders() ? 0 : (paddingLeft() + paddingRight() + (numEffCols() + 1) * hBorderSpacing()));
    }

    RenderTableCol* colElement(int col, bool* startEdge = 0, bool* endEdge = 0) const;

    bool needsSectionRecalc() const { return m_needsSectionRecalc; }
    void setNeedsSectionRecalc()
    {
        if (documentBeingDestroyed())
            return;
        m_needsSectionRecalc = true;
        setNeedsLayout(true);
    }

    virtual RenderObject* removeChildNode(RenderObject*, bool fullRemove = true);

    RenderTableSection* sectionAbove(const RenderTableSection*, bool skipEmptySections = false) const;
    RenderTableSection* sectionBelow(const RenderTableSection*, bool skipEmptySections = false) const;

    RenderTableCell* cellAbove(const RenderTableCell*) const;
    RenderTableCell* cellBelow(const RenderTableCell*) const;
    RenderTableCell* cellBefore(const RenderTableCell*) const;
    RenderTableCell* cellAfter(const RenderTableCell*) const;
 
    const CollapsedBorderValue* currentBorderStyle() const { return m_currentBorder; }
    
    bool hasSections() const { return m_head || m_foot || m_firstBody; }

    virtual IntRect getOverflowClipRect(int tx, int ty);

    void recalcSectionsIfNeeded() const
    {
        if (m_needsSectionRecalc)
            recalcSections();
    }

#ifndef NDEBUG
    virtual void dump(TextStream*, DeprecatedString ind = "") const;
#endif

private:
    void recalcSections() const;

    mutable Vector<int> m_columnPos;
    mutable Vector<ColumnStruct> m_columns;

    mutable RenderBlock* m_caption;
    mutable RenderTableSection* m_head;
    mutable RenderTableSection* m_foot;
    mutable RenderTableSection* m_firstBody;

    TableLayout* m_tableLayout;

    const CollapsedBorderValue* m_currentBorder;
    
    unsigned m_frame : 4; // Frame
    unsigned m_rules : 4; // Rules

    mutable bool m_hasColElements : 1;
    mutable bool m_needsSectionRecalc : 1;
    
    short m_hSpacing;
    short m_vSpacing;
    int m_borderLeft;
    int m_borderRight;
};

} // namespace WebCore

#endif // RenderTable_h
