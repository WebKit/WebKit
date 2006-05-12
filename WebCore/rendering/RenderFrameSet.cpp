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

#include "Cursor.h"
#include "EventNames.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLFrameSetElement.h"
#include "dom2_eventsimpl.h"
#include "RenderCanvas.h"
#include "RenderFrame.h"
#include "KWQTextStream.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

RenderFrameSet::RenderFrameSet(HTMLFrameSetElement* frameSet)
    : RenderContainer(frameSet)
    , m_hSplitVar(0)
    , m_vSplitVar(0)
    , m_hSplit(-1)
    , m_vSplit(-1)
    , m_resizing(false)
    , m_clientResizing(false)
{
  // init RenderObject attributes
    setInline(false);

  for (int k = 0; k < 2; ++k) {
      m_gridLen[k] = -1;
      m_gridDelta[k] = 0;
      m_gridLayout[k] = 0;
  }
}

RenderFrameSet::~RenderFrameSet()
{
    for (int k = 0; k < 2; ++k) {
        if (m_gridLayout[k])
            delete [] m_gridLayout[k];
        if (m_gridDelta[k])
            delete [] m_gridDelta[k];
    }
  if (m_hSplitVar)
      delete [] m_hSplitVar;
  if (m_vSplitVar)
      delete [] m_vSplitVar;
}

bool RenderFrameSet::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                                 HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    bool inside = RenderContainer::nodeAtPoint(info, _x, _y, _tx, _ty, hitTestAction) || 
                  m_resizing || canResize(_x, _y);
    if (inside && element() && !element()->noResize() && !info.readonly() && !info.innerNode()) {
        info.setInnerNode(element());
        info.setInnerNonSharedNode(element());
    }

    return inside || m_clientResizing;
}

void RenderFrameSet::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    if (!parent()->isFrameSet()) {
        FrameView* view = canvas()->view();
        m_width = view->visibleWidth();
        m_height = view->visibleHeight();
    }

    int remainingLen[2];
    remainingLen[1] = m_width - (element()->totalCols()-1)*element()->border();
    if (remainingLen[1] < 0)
        remainingLen[1] = 0;
    remainingLen[0] = m_height - (element()->totalRows()-1)*element()->border();
    if (remainingLen[0] < 0)
        remainingLen[0] = 0;

    int availableLen[2];
    availableLen[0] = remainingLen[0];
    availableLen[1] = remainingLen[1];

    if (m_gridLen[0] != element()->totalRows() || m_gridLen[1] != element()->totalCols()) {
        // number of rows or cols changed
        // need to zero out the deltas
        m_gridLen[0] = element()->totalRows();
        m_gridLen[1] = element()->totalCols();
        for (int k = 0; k < 2; ++k) {
            if (m_gridDelta[k]) delete [] m_gridDelta[k];
            m_gridDelta[k] = new int[m_gridLen[k]];
            if (m_gridLayout[k]) delete [] m_gridLayout[k];
            m_gridLayout[k] = new int[m_gridLen[k]];
            for (int i = 0; i < m_gridLen[k]; ++i)
                m_gridDelta[k][i] = 0;
        }
    }

    for (int k = 0; k < 2; ++k) {
        int totalRelative = 0;
        int totalFixed = 0;
        int totalPercent = 0;
        int countRelative = 0;
        int countFixed = 0;
        int countPercent = 0;
        int gridLen = m_gridLen[k];
        int* gridDelta = m_gridDelta[k];
        Length* grid =  k ? element()->m_cols : element()->m_rows;
        int* gridLayout = m_gridLayout[k];

        if (grid) {
            assert(gridLen);
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
                    gridLayout[i] = max(grid[i].calcValue(availableLen[k]), 0);
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

            // Fixed columns/rows are our first priority. If there is not enough space to fit all fixed
            // columns/rows we need to proportionally adjust their size. 
            if (totalFixed > remainingLen[k]) {
                int remainingFixed = remainingLen[k];

                for (int i = 0; i < gridLen; ++i) {
                    if (grid[i].isFixed()) {
                        gridLayout[i] = (gridLayout[i] * remainingFixed) / totalFixed;
                        remainingLen[k] -= gridLayout[i];
                    }
                }
            } else
                remainingLen[k] -= totalFixed;

            // Percentage columns/rows are our second priority. Divide the remaining space proportionally 
            // over all percentage columns/rows. IMPORTANT: the size of each column/row is not relative 
            // to 100%, but to the total percentage. For example, if there are three columns, each of 75%,
            // and the available space is 300px, each column will become 100px in width.
            if (totalPercent > remainingLen[k]) {
                int remainingPercent = remainingLen[k];

                for (int i = 0; i < gridLen; ++i) {
                    if (grid[i].isPercent()) {
                        gridLayout[i] = (gridLayout[i] * remainingPercent) / totalPercent;
                        remainingLen[k] -= gridLayout[i];
                    }
                }
            } else
                remainingLen[k] -= totalPercent;

            // Relative columns/rows are our last priority. Divide the remaining space proportionally
            // over all relative columns/rows. IMPORTANT: the relative value of 0* is treated as 1*.
            if (countRelative) {
                int lastRelative = 0;
                int remainingRelative = remainingLen[k];

                for (int i = 0; i < gridLen; ++i) {
                    if (grid[i].isRelative()) {
                        gridLayout[i] = (max(grid[i].value(), 1) * remainingRelative) / totalRelative;
                        remainingLen[k] -= gridLayout[i];
                        lastRelative = i;
                    }
                }
                
                // If we could not evently distribute the available space of all of the relative  
                // columns/rows, the remainder will be added to the last column/row.
                // For example: if we have a space of 100px and three columns (*,*,*), the remainder will
                // be 1px and will be added to the last column: 33px, 33px, 34px.
                if (remainingLen[k]) {
                    gridLayout[lastRelative] += remainingLen[k];
                    remainingLen[k] = 0;
                }
            }

            // If we still have some left over space we need to divide it over the already existing
            // columns/rows
            if (remainingLen[k]) {
                // Our first priority is to spread if over the percentage columns. The remaining
                // space is spread evenly, for example: if we have a space of 100px, the columns 
                // definition of 25%,25% used to result in two columns of 25px. After this the 
                // columns will each be 50px in width. 
                if (countPercent && totalPercent) {
                    int remainingPercent = remainingLen[k];
                    int changePercent = 0;

                    for (int i = 0; i < gridLen; ++i) {
                        if (grid[i].isPercent()) {
                            changePercent = (remainingPercent * gridLayout[i]) / totalPercent;
                            gridLayout[i] += changePercent;
                            remainingLen[k] -= changePercent;
                        }
                    }
                } else if (totalFixed) {
                    // Our last priority is to spread the remaining space over the fixed columns.
                    // For example if we have 100px of space and two column of each 40px, both
                    // columns will become exactly 50px.
                    int remainingFixed = remainingLen[k];
                    int changeFixed = 0;

                    for (int i = 0; i < gridLen; ++i) {
                        if (grid[i].isFixed()) {
                            changeFixed = (remainingFixed * gridLayout[i]) / totalFixed;
                            gridLayout[i] += changeFixed;
                            remainingLen[k] -= changeFixed;
                        } 
                    }
                }
            }
            
            // If we still have some left over space we probably ended up with a remainder of
            // a division. We can not spread it evenly anymore. If we have any percentage 
            // columns/rows simply spread the remainder equally over all available percentage columns, 
            // regardless of their size.
            if (remainingLen[k] && countPercent) {
                int remainingPercent = remainingLen[k];
                int changePercent = 0;

                for (int i = 0; i < gridLen; ++i) {
                    if (grid[i].isPercent()) {
                        changePercent = remainingPercent / countPercent;
                        gridLayout[i] += changePercent;
                        remainingLen[k] -= changePercent;
                    }
                }
            } 
            
            // If we don't have any percentage columns/rows we only have fixed columns. Spread
            // the remainder equally over all fixed columns/rows.
            else if (remainingLen[k] && countFixed) {
                int remainingFixed = remainingLen[k];
                int changeFixed = 0;
                
                for (int i = 0; i < gridLen; ++i) {
                    if (grid[i].isFixed()) {
                        changeFixed = remainingFixed / countFixed;
                        gridLayout[i] += changeFixed;
                        remainingLen[k] -= changeFixed;
                    }
                }
            }

            // Still some left over... simply add it to the last column, because it is impossible
            // spread it evenly or equally.
            if (remainingLen[k])
                gridLayout[gridLen - 1] += remainingLen[k];

            // now we have the final layout, distribute the delta over it
            bool worked = true;
            for (int i = 0; i < gridLen; ++i) {
                if (gridLayout[i] && gridLayout[i] + gridDelta[i] <= 0)
                    worked = false;
                gridLayout[i] += gridDelta[i];
            }
            // now the delta's broke something, undo it and reset deltas
            if (!worked) {
                for (int i = 0; i < gridLen; ++i) {
                    gridLayout[i] -= gridDelta[i];
                    gridDelta[i] = 0;
                }
            }
        }
        else
            gridLayout[0] = remainingLen[k];
    }

    positionFrames();

    RenderObject *child = firstChild();
    if (!child)
        goto end2;

    if (!m_hSplitVar && !m_vSplitVar) {
        if (!m_vSplitVar && element()->totalCols() > 1) {
            m_vSplitVar = new bool[element()->totalCols()];
            for (int i = 0; i < element()->totalCols(); i++) m_vSplitVar[i] = true;
        }
        if (!m_hSplitVar && element()->totalRows() > 1) {
            m_hSplitVar = new bool[element()->totalRows()];
            for (int i = 0; i < element()->totalRows(); i++) m_hSplitVar[i] = true;
        }

        for (int r = 0; r < element()->totalRows(); r++) {
            for (int c = 0; c < element()->totalCols(); c++) {
                bool fixed = false;

                if (child->isFrameSet())
                  fixed = static_cast<RenderFrameSet*>(child)->element()->noResize();
                else
                  fixed = static_cast<RenderFrame*>(child)->element()->noResize();

                if (fixed) {
                    if (element()->totalCols() > 1) {
                        if (c>0) m_vSplitVar[c-1] = false;
                        m_vSplitVar[c] = false;
                    }
                    if (element()->totalRows() > 1) {
                        if (r>0) m_hSplitVar[r-1] = false;
                        m_hSplitVar[r] = false;
                    }
                    child = child->nextSibling();
                    if (!child)
                        goto end1;
                }
            }
        }

    }
 end1:
    RenderContainer::layout();
 end2:
    setNeedsLayout(false);
}

void RenderFrameSet::positionFrames()
{
  int r;
  int c;

  RenderObject *child = firstChild();
  if (!child)
    return;

  //  Node *child = _first;
  //  if (!child) return;

  int yPos = 0;

  for (r = 0; r < element()->totalRows(); r++) {
    int xPos = 0;
    for (c = 0; c < element()->totalCols(); c++) {
      child->setPos(xPos, yPos);
      // has to be resized and itself resize its contents
      if ((m_gridLayout[1][c] != child->width()) || (m_gridLayout[0][r] != child->height())) {
          child->setWidth(m_gridLayout[1][c]);
          child->setHeight(m_gridLayout[0][r]);
          child->setNeedsLayout(true);
          child->layout();
      }

      xPos += m_gridLayout[1][c] + element()->border();
      child = child->nextSibling();

      if (!child)
        return;

    }

    yPos += m_gridLayout[0][r] + element()->border();
  }

  // all the remaining frames are hidden to avoid ugly
  // spurious unflowed frames
  while (child) {
      child->setWidth(0);
      child->setHeight(0);
      child->setNeedsLayout(false);

      child = child->nextSibling();
  }
}

bool RenderFrameSet::userResize(MouseEvent* evt)
{
    if (needsLayout())
        return false;
    
    bool res = false;
    int _x = evt->clientX();
    int _y = evt->clientY();
    
    if (!m_resizing && evt->type() == mousemoveEvent || evt->type() == mousedownEvent) {
        m_hSplit = -1;
        m_vSplit = -1;
        //bool resizePossible = true;
        
        // check if we're over a horizontal or vertical boundary
        int pos = m_gridLayout[1][0] + xPos();
        for (int c = 1; c < element()->totalCols(); c++) {
            if (_x >= pos && _x <= pos+element()->border()) {
                if (m_vSplitVar && m_vSplitVar[c - 1])
                    m_vSplit = c - 1;
                res = true;
                break;
            }
            pos += m_gridLayout[1][c] + element()->border();
        }
        
        pos = m_gridLayout[0][0] + yPos();
        for (int r = 1; r < element()->totalRows(); r++) {
            if (_y >= pos && _y <= pos+element()->border()) {
                if (m_hSplitVar && m_hSplitVar[r - 1])
                    m_hSplit = r - 1;
                res = true;
                break;
            }
            pos += m_gridLayout[0][r] + element()->border();
        }
        
        if (evt->type() == mousedownEvent) {
            setResizing(true);
            m_vSplitPos = _x;
            m_hSplitPos = _y;
            m_oldpos = -1;
        } else
            canvas()->view()->setCursor(pointerCursor());
    }
    
    // ### check the resize is not going out of bounds.
    if (m_resizing && evt->type() == mouseupEvent) {
        setResizing(false);
        
        if (m_vSplit != -1) {
            int delta = m_vSplitPos - _x;
            m_gridDelta[1][m_vSplit] -= delta;
            m_gridDelta[1][m_vSplit+1] += delta;
        }
        if (m_hSplit != -1) {
            int delta = m_hSplitPos - _y;
            m_gridDelta[0][m_hSplit] -= delta;
            m_gridDelta[0][m_hSplit+1] += delta;
        }
        
        // this just schedules the relayout
        // important, otherwise the moving indicator is not correctly erased
        setNeedsLayout(true);
    } else if (m_resizing || evt->type() == mouseupEvent) {
        FrameView* v = canvas()->view();        
        v->disableFlushDrawing();
        GraphicsContext* context = v->lockDrawingFocus();
        
        IntRect r(xPos(), yPos(), width(), height());
        const int rBord = 3;
        int sw = element()->border();
        int p = m_resizing ? (m_vSplit > -1 ? _x : _y) : -1;
        const RGBA32 greyQuarterOpacity = 0x40A0A0A0;
        if (m_vSplit > -1) {
            if (m_oldpos >= 0)
                v->updateContents(IntRect(m_oldpos + sw/2 - rBord, r.y(), 2 * rBord, r.height()), true);
            if (p >= 0) {
                context->setPen(Pen::NoPen);
                context->setFillColor(greyQuarterOpacity);
                context->drawRect(IntRect(p + sw/2 - rBord, r.y(), 2 * rBord, r.height()));
            }
        } else {
            if (m_oldpos >= 0)
                v->updateContents(IntRect(r.x(), m_oldpos + sw/2 - rBord, r.width(), 2 * rBord), true);
            if (p >= 0) {
                context->setPen(Pen::NoPen);
                context->setFillColor(greyQuarterOpacity);
                context->drawRect(IntRect(r.x(), p + sw/2 - rBord, r.width(), 2 * rBord));
            }
        }
        m_oldpos = p;

        v->unlockDrawingFocus(context);
        v->enableFlushDrawing();
    }
    
    return res;
}

void RenderFrameSet::setResizing(bool e)
{
    m_resizing = e;
    for (RenderObject* p = parent(); p; p = p->parent())
        if (p->isFrameSet())
            static_cast<RenderFrameSet*>(p)->m_clientResizing = m_resizing;
    canvas()->view()->setResizingFrameSet(e ? element() : 0);
}

bool RenderFrameSet::canResize(int _x, int _y)
{
    // if we haven't received a layout, then the gridLayout doesn't contain useful data yet
    if (needsLayout() || !m_gridLayout[0] || !m_gridLayout[1])
        return false;

    // check if we're over a horizontal or vertical boundary
    int pos = m_gridLayout[1][0];
    for (int c = 1; c < element()->totalCols(); c++)
        if (_x >= pos && _x <= pos+element()->border())
            return true;

    pos = m_gridLayout[0][0];
    for (int r = 1; r < element()->totalRows(); r++)
        if (_y >= pos && _y <= pos+element()->border())
            return true;

    return false;
}

#ifndef NDEBUG
void RenderFrameSet::dump(QTextStream* stream, DeprecatedString ind) const
{
  *stream << " totalrows=" << element()->totalRows();
  *stream << " totalcols=" << element()->totalCols();

  unsigned i;
  for (i = 0; i < (unsigned)element()->totalRows(); i++)
    *stream << " hSplitvar(" << i << ")=" << m_hSplitVar[i];

  for (i = 0; i < (unsigned)element()->totalCols(); i++)
    *stream << " vSplitvar(" << i << ")=" << m_vSplitVar[i];

  RenderContainer::dump(stream,ind);
}
#endif

}
