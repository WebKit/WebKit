/**
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderFrameSet.h"

#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFrameSetElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "MouseEvent.h"
#include "RenderFrame.h"
#include "RenderView.h"
#include "TextStream.h"

namespace WebCore {

using namespace EventNames;

RenderFrameSet::RenderFrameSet(HTMLFrameSetElement* frameSet)
    : RenderContainer(frameSet)
    , m_isResizing(false)
    , m_isChildResizing(false)
{
    setInline(false);
}

RenderFrameSet::~RenderFrameSet()
{
}

RenderFrameSet::GridAxis::GridAxis()
    : m_splitBeingResized(noSplit)
{
}

inline HTMLFrameSetElement* RenderFrameSet::frameSet() const
{
    return static_cast<HTMLFrameSetElement*>(node());
}

bool RenderFrameSet::nodeAtPoint(const HitTestRequest& request, HitTestResult& result,
    int x, int y, int tx, int ty, HitTestAction action)
{
    if (action != HitTestForeground)
        return false;

    bool inside = RenderContainer::nodeAtPoint(request, result, x, y, tx, ty, action)
        || m_isResizing || canResize(IntPoint(x, y));

    if (inside && frameSet()->noResize()
            && !request.readonly && !result.innerNode()) {
        result.setInnerNode(node());
        result.setInnerNonSharedNode(node());
    }

    return inside || m_isChildResizing;
}

void RenderFrameSet::GridAxis::resize(int size)
{
    m_sizes.resize(size);
    m_deltas.resize(size);
    m_deltas.fill(0);
    m_isSplitResizable.resize(size);
}

void RenderFrameSet::layOutAxis(GridAxis& axis, const Length* grid, int availableLen)
{
    int* gridLayout = axis.m_sizes;

    if (!grid) {
        gridLayout[0] = availableLen;
        return;
    }

    int gridLen = axis.m_sizes.size();
    ASSERT(gridLen);

    int totalRelative = 0;
    int totalFixed = 0;
    int totalPercent = 0;
    int countRelative = 0;
    int countFixed = 0;
    int countPercent = 0;

    // First we need to investigate how many columns of each type we have and
    // how much space these columns are going to require.
    for (int i = 0; i < gridLen; ++i) {
        // Count the total length of all of the fixed columns/rows -> totalFixed
        // Count the number of columns/rows which are fixed -> countFixed
        if (grid[i].isFixed()) {
            gridLayout[i] = max(grid[i].value(), 0);
            totalFixed += gridLayout[i];
            countFixed++;
        }
        
        // Count the total percentage of all of the percentage columns/rows -> totalPercent
        // Count the number of columns/rows which are percentages -> countPercent
        if (grid[i].isPercent()) {
            gridLayout[i] = max(grid[i].calcValue(availableLen), 0);
            totalPercent += gridLayout[i];
            countPercent++;
        }

        // Count the total relative of all the relative columns/rows -> totalRelative
        // Count the number of columns/rows which are relative -> countRelative
        if (grid[i].isRelative()) {
            totalRelative += max(grid[i].value(), 1);
            countRelative++;
        }            
    }

    int remainingLen = availableLen;

    // Fixed columns/rows are our first priority. If there is not enough space to fit all fixed
    // columns/rows we need to proportionally adjust their size. 
    if (totalFixed > remainingLen) {
        int remainingFixed = remainingLen;

        for (int i = 0; i < gridLen; ++i) {
            if (grid[i].isFixed()) {
                gridLayout[i] = (gridLayout[i] * remainingFixed) / totalFixed;
                remainingLen -= gridLayout[i];
            }
        }
    } else
        remainingLen -= totalFixed;

    // Percentage columns/rows are our second priority. Divide the remaining space proportionally 
    // over all percentage columns/rows. IMPORTANT: the size of each column/row is not relative 
    // to 100%, but to the total percentage. For example, if there are three columns, each of 75%,
    // and the available space is 300px, each column will become 100px in width.
    if (totalPercent > remainingLen) {
        int remainingPercent = remainingLen;

        for (int i = 0; i < gridLen; ++i) {
            if (grid[i].isPercent()) {
                gridLayout[i] = (gridLayout[i] * remainingPercent) / totalPercent;
                remainingLen -= gridLayout[i];
            }
        }
    } else
        remainingLen -= totalPercent;

    // Relative columns/rows are our last priority. Divide the remaining space proportionally
    // over all relative columns/rows. IMPORTANT: the relative value of 0* is treated as 1*.
    if (countRelative) {
        int lastRelative = 0;
        int remainingRelative = remainingLen;

        for (int i = 0; i < gridLen; ++i) {
            if (grid[i].isRelative()) {
                gridLayout[i] = (max(grid[i].value(), 1) * remainingRelative) / totalRelative;
                remainingLen -= gridLayout[i];
                lastRelative = i;
            }
        }
        
        // If we could not evenly distribute the available space of all of the relative  
        // columns/rows, the remainder will be added to the last column/row.
        // For example: if we have a space of 100px and three columns (*,*,*), the remainder will
        // be 1px and will be added to the last column: 33px, 33px, 34px.
        if (remainingLen) {
            gridLayout[lastRelative] += remainingLen;
            remainingLen = 0;
        }
    }

    // If we still have some left over space we need to divide it over the already existing
    // columns/rows
    if (remainingLen) {
        // Our first priority is to spread if over the percentage columns. The remaining
        // space is spread evenly, for example: if we have a space of 100px, the columns 
        // definition of 25%,25% used to result in two columns of 25px. After this the 
        // columns will each be 50px in width. 
        if (countPercent && totalPercent) {
            int remainingPercent = remainingLen;
            int changePercent = 0;

            for (int i = 0; i < gridLen; ++i) {
                if (grid[i].isPercent()) {
                    changePercent = (remainingPercent * gridLayout[i]) / totalPercent;
                    gridLayout[i] += changePercent;
                    remainingLen -= changePercent;
                }
            }
        } else if (totalFixed) {
            // Our last priority is to spread the remaining space over the fixed columns.
            // For example if we have 100px of space and two column of each 40px, both
            // columns will become exactly 50px.
            int remainingFixed = remainingLen;
            int changeFixed = 0;

            for (int i = 0; i < gridLen; ++i) {
                if (grid[i].isFixed()) {
                    changeFixed = (remainingFixed * gridLayout[i]) / totalFixed;
                    gridLayout[i] += changeFixed;
                    remainingLen -= changeFixed;
                } 
            }
        }
    }
    
    // If we still have some left over space we probably ended up with a remainder of
    // a division. We can not spread it evenly anymore. If we have any percentage 
    // columns/rows simply spread the remainder equally over all available percentage columns, 
    // regardless of their size.
    if (remainingLen && countPercent) {
        int remainingPercent = remainingLen;
        int changePercent = 0;

        for (int i = 0; i < gridLen; ++i) {
            if (grid[i].isPercent()) {
                changePercent = remainingPercent / countPercent;
                gridLayout[i] += changePercent;
                remainingLen -= changePercent;
            }
        }
    } 
    
    // If we don't have any percentage columns/rows we only have fixed columns. Spread
    // the remainder equally over all fixed columns/rows.
    else if (remainingLen && countFixed) {
        int remainingFixed = remainingLen;
        int changeFixed = 0;
        
        for (int i = 0; i < gridLen; ++i) {
            if (grid[i].isFixed()) {
                changeFixed = remainingFixed / countFixed;
                gridLayout[i] += changeFixed;
                remainingLen -= changeFixed;
            }
        }
    }

    // Still some left over. Add it to the last column, because it is impossible
    // spread it evenly or equally.
    if (remainingLen)
        gridLayout[gridLen - 1] += remainingLen;

    // now we have the final layout, distribute the delta over it
    bool worked = true;
    int* gridDelta = axis.m_deltas;
    for (int i = 0; i < gridLen; ++i) {
        if (gridLayout[i] && gridLayout[i] + gridDelta[i] <= 0)
            worked = false;
        gridLayout[i] += gridDelta[i];
    }
    // if the deltas broke something, undo them
    if (!worked) {
        for (int i = 0; i < gridLen; ++i)
            gridLayout[i] -= gridDelta[i];
        axis.m_deltas.fill(0);
    }
}

void RenderFrameSet::findNonResizableSplits()
{
    m_rows.m_isSplitResizable.fill(true);
    m_cols.m_isSplitResizable.fill(true);

    RenderObject* child = firstChild();
    if (!child)
        return;

    int rows = frameSet()->totalRows();
    int cols = frameSet()->totalCols();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            bool fixed;
            if (child->isFrameSet())
                fixed = static_cast<RenderFrameSet*>(child)->frameSet()->noResize();
            else
                fixed = static_cast<RenderFrame*>(child)->element()->noResize();
            if (fixed) {
                if (cols > 1) {
                    if (c > 0)
                        m_cols.m_isSplitResizable[c - 1] = false;
                    m_cols.m_isSplitResizable[c] = false;
                }
                if (rows > 1) {
                    if (r > 0)
                        m_rows.m_isSplitResizable[r - 1] = false;
                    m_rows.m_isSplitResizable[r] = false;
                }
            }
            child = child->nextSibling();
            if (!child)
                return;
        }
    }
}

void RenderFrameSet::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    if (!parent()->isFrameSet()) {
        FrameView* v = view()->frameView();
        m_width = v->visibleWidth();
        m_height = v->visibleHeight();
    }

    size_t cols = frameSet()->totalCols();
    size_t rows = frameSet()->totalRows();

    if (m_rows.m_sizes.size() != rows || m_cols.m_sizes.size() != cols) {
        m_rows.resize(rows);
        m_cols.resize(cols);
        findNonResizableSplits();
    }

    int borderThickness = frameSet()->border();
    layOutAxis(m_rows, frameSet()->rowLengths(), m_height - (rows - 1) * borderThickness);
    layOutAxis(m_cols, frameSet()->colLengths(), m_width - (cols - 1) * borderThickness);

    positionFrames();

    RenderContainer::layout();

    setNeedsLayout(false);
}

void RenderFrameSet::positionFrames()
{
    RenderObject* child = firstChild();
    if (!child)
        return;

    int rows = frameSet()->totalRows();
    int cols = frameSet()->totalCols();

    int yPos = 0;
    int borderThickness = frameSet()->border();
    for (int r = 0; r < rows; r++) {
        int xPos = 0;
        int height = m_rows.m_sizes[r];
        for (int c = 0; c < cols; c++) {
            child->setPos(xPos, yPos);
            int width = m_cols.m_sizes[c];

            // has to be resized and itself resize its contents
            if (width != child->width() || height != child->height()) {
                child->setWidth(width);
                child->setHeight(height);
                child->setNeedsLayout(true);
                child->layout();
            }

            xPos += width + borderThickness;

            child = child->nextSibling();
            if (!child)
                return;
        }
        yPos += height + borderThickness;
    }

    // all the remaining frames are hidden to avoid ugly spurious unflowed frames
    for (; child; child = child->nextSibling()) {
        child->setWidth(0);
        child->setHeight(0);
        child->setNeedsLayout(false);
    }
}

void RenderFrameSet::startResizing(GridAxis& axis, int position)
{
    int split = hitTestSplit(axis, position);
    if (split == noSplit || !axis.m_isSplitResizable[split]) {
        axis.m_splitBeingResized = noSplit;
        return;
    }
    axis.m_splitBeingResized = split;
    axis.m_splitResizeOffset = position - splitPosition(axis, split);
}

void RenderFrameSet::continueResizing(GridAxis& axis, int position)
{
    if (needsLayout())
        return;
    if (axis.m_splitBeingResized == noSplit)
        return;
    int currentSplitPosition = splitPosition(axis, axis.m_splitBeingResized);
    int delta = (position - currentSplitPosition) - axis.m_splitResizeOffset;
    axis.m_deltas[axis.m_splitBeingResized] += delta;
    axis.m_deltas[axis.m_splitBeingResized + 1] -= delta;
    setNeedsLayout(true);
}

bool RenderFrameSet::userResize(MouseEvent* evt)
{
    if (needsLayout())
        return false;
    
    if (!m_isResizing) {
        if (evt->type() == mousedownEvent) {
            startResizing(m_cols, evt->pageX() - xPos());
            startResizing(m_rows, evt->pageY() - yPos());
            if (m_cols.m_splitBeingResized != noSplit || m_rows.m_splitBeingResized != noSplit) {
                setIsResizing(true);
                return true;
            }
        }
    } else {
        if (evt->type() == mousemoveEvent || evt->type() == mouseupEvent) {
            continueResizing(m_cols, evt->pageX() - xPos());
            continueResizing(m_rows, evt->pageY() - yPos());
            if (evt->type() == mouseupEvent) {
                setIsResizing(false);
                return true;
            }
        }
    }

    return false;
}

void RenderFrameSet::setIsResizing(bool isResizing)
{
    m_isResizing = isResizing;
    for (RenderObject* p = parent(); p; p = p->parent())
        if (p->isFrameSet())
            static_cast<RenderFrameSet*>(p)->m_isChildResizing = isResizing;
    if (Frame* frame = document()->frame())
        frame->eventHandler()->setResizingFrameSet(isResizing ? frameSet() : 0);
}

bool RenderFrameSet::isResizingRow() const
{
    return m_isResizing && m_rows.m_splitBeingResized != noSplit;
}

bool RenderFrameSet::isResizingColumn() const
{
    return m_isResizing && m_cols.m_splitBeingResized != noSplit;
}

bool RenderFrameSet::canResize(const IntPoint& p) const
{
    return hitTestSplit(m_cols, p.x()) != noSplit || hitTestSplit(m_rows, p.y()) != noSplit;
}

bool RenderFrameSet::canResizeRow(const IntPoint& p) const
{
    int r = hitTestSplit(m_rows, p.y() - yPos());
    return r != noSplit && m_rows.m_isSplitResizable[r];
}

bool RenderFrameSet::canResizeColumn(const IntPoint& p) const
{
    int c = hitTestSplit(m_cols, p.x() - xPos());
    return c != noSplit && m_cols.m_isSplitResizable[c];
}

int RenderFrameSet::splitPosition(const GridAxis& axis, int split) const
{
    if (needsLayout())
        return 0;

    int borderThickness = frameSet()->border();

    int size = axis.m_sizes.size();
    if (!size)
        return 0;

    int position = 0;
    for (int i = 0; i <= split && i < size; ++i)
        position += axis.m_sizes[i] + borderThickness;
    return position - borderThickness;
}

int RenderFrameSet::hitTestSplit(const GridAxis& axis, int position) const
{
    if (needsLayout())
        return noSplit;

    int borderThickness = frameSet()->border();
    if (borderThickness <= 0)
        return noSplit;

    size_t size = axis.m_sizes.size();
    if (!size)
        return noSplit;

    int splitPosition = axis.m_sizes[0];
    for (size_t i = 1; i < size; ++i) {
        if (position >= splitPosition && position < splitPosition + borderThickness)
            return i - 1;
        splitPosition += borderThickness + axis.m_sizes[i];
    }
    return noSplit;
}

#ifndef NDEBUG
void RenderFrameSet::dump(TextStream* stream, DeprecatedString ind) const
{
    *stream << " totalrows=" << frameSet()->totalRows();
    *stream << " totalcols=" << frameSet()->totalCols();

    for (int i = 0; i < frameSet()->totalRows(); i++)
        *stream << " hSplitvar(" << i << ")=" << m_rows.m_isSplitResizable[i];

    for (int i = 0; i < frameSet()->totalCols(); i++)
        *stream << " vSplitvar(" << i << ")=" << m_cols.m_isSplitResizable[i];

    RenderContainer::dump(stream,ind);
}
#endif

} // namespace WebCore
