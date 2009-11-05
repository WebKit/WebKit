/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
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

#ifndef AutoTableLayout_h
#define AutoTableLayout_h

#include "Length.h"
#include "TableLayout.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderTable;
class RenderTableCell;

class AutoTableLayout : public TableLayout {
public:
    AutoTableLayout(RenderTable*);
    ~AutoTableLayout();

    virtual void calcPrefWidths(int& minWidth, int& maxWidth);
    virtual void layout();

protected:
    void fullRecalc();
    void recalcColumn(int effCol);

    void calcPercentages() const;
    int totalPercent() const
    {
        if (m_percentagesDirty)
            calcPercentages();
        return m_totalPercent;
    }

    int calcEffectiveWidth();

    void insertSpanCell(RenderTableCell*);

    struct Layout {
        Layout()
            : minWidth(0)
            , maxWidth(0)
            , effMinWidth(0)
            , effMaxWidth(0)
            , calcWidth(0)
            , emptyCellsOnly(true) {}
        Length width;
        Length effWidth;
        int minWidth;
        int maxWidth;
        int effMinWidth;
        int effMaxWidth;
        int calcWidth;
        bool emptyCellsOnly;
    };

    Vector<Layout, 4> m_layoutStruct;
    Vector<RenderTableCell*, 4> m_spanCells;
    bool m_hasPercent : 1;
    mutable bool m_percentagesDirty : 1;
    mutable bool m_effWidthDirty : 1;
    mutable unsigned short m_totalPercent;
};

} // namespace WebCore

#endif // AutoTableLayout_h
