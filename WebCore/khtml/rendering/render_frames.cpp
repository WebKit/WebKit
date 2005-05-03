/**
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004 Apple Computer, Inc.
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
//#define DEBUG_LAYOUT

#include "rendering/render_frames.h"

#include "css/cssproperties.h"
#include "rendering/render_canvas.h"
#include "html/html_baseimpl.h"
#include "html/html_objectimpl.h"
#include "html/htmltokenizer.h"
#include "misc/htmlattrs.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom_docimpl.h"
#include "misc/htmltags.h"
#include "khtmlview.h"
#include "khtml_part.h"
#include "render_arena.h"

#include <kapplication.h>
#include <kcursor.h>
#include <kmessagebox.h>
#include <kmimetype.h>
#include <klocale.h>
#include <kdebug.h>
#include <kglobal.h>
#include <qtimer.h>
#include <qpainter.h>
#include "qdict.h"

using namespace khtml;
using namespace DOM;

RenderFrameSet::RenderFrameSet( HTMLFrameSetElementImpl *frameSet)
    : RenderContainer(frameSet)
{
  // init RenderObject attributes
    setInline(false);

  for (int k = 0; k < 2; ++k) {
      m_gridLen[k] = -1;
      m_gridDelta[k] = 0;
      m_gridLayout[k] = 0;
  }

  m_resizing = m_clientresizing= false;

  m_hSplit = -1;
  m_vSplit = -1;

  m_hSplitVar = 0;
  m_vSplitVar = 0;
}

RenderFrameSet::~RenderFrameSet()
{
    for (int k = 0; k < 2; ++k) {
        if (m_gridLayout[k]) delete [] m_gridLayout[k];
        if (m_gridDelta[k]) delete [] m_gridDelta[k];
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

    return inside || m_clientresizing;
}

void RenderFrameSet::layout( )
{
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    if ( !parent()->isFrameSet() ) {
        KHTMLView* view = canvas()->view();
        m_width = view->visibleWidth();
        m_height = view->visibleHeight();
    }

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(FrameSet)::layout( ) width=" << width() << ", height=" << height() << endl;
#endif

    int remainingLen[2];
    remainingLen[1] = m_width - (element()->totalCols()-1)*element()->border();
    if(remainingLen[1]<0) remainingLen[1]=0;
    remainingLen[0] = m_height - (element()->totalRows()-1)*element()->border();
    if(remainingLen[0]<0) remainingLen[0]=0;

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
        int countPercent = 0;
        int gridLen = m_gridLen[k];
        int* gridDelta = m_gridDelta[k];
        khtml::Length* grid =  k ? element()->m_cols : element()->m_rows;
        int* gridLayout = m_gridLayout[k];

        if (grid) {
            // first distribute the available width for fixed rows, then handle the
            // percentage ones and distribute remaining over relative
            for(int i = 0; i< gridLen; ++i) {
                if (grid[i].isFixed()) {
                    gridLayout[i] = kMin(grid[i].value > 0 ? grid[i].value : 0, remainingLen[k]);
                    remainingLen[k] -= gridLayout[i];
                    totalFixed += gridLayout[i];
                }
                else if(grid[i].isRelative()) {
                    totalRelative += grid[i].value > 1 ? grid[i].value : 1;
                    countRelative++;
                }
                else if (grid[i].isPercent()) {
                    totalPercent += grid[i].value >= 0 ? grid[i].value : 0;
                    countPercent++;
                }
            }

            int currPercent = totalPercent;
            int percentLen = (countRelative && currPercent < 100) ? currPercent * remainingLen[k] / 100 : remainingLen[k];
            for(int i = 0; i < gridLen; i++)
                if (grid[i].isPercent() && grid[i].value >= 0 && currPercent) {
                    gridLayout[i] = grid[i].value * percentLen / currPercent;
                    remainingLen[k] -= gridLayout[i];
                    percentLen -= gridLayout[i];
                    currPercent -= grid[i].value;
                }

            assert(remainingLen[k] >= 0);

            if (countRelative) {
                int remaining = remainingLen[k];
                for (int i = 0; i < gridLen; ++i)
                    if (grid[i].isRelative()) {
                        gridLayout[i] = ((grid[i].value > 1 ? grid[i].value : 1) * remaining) / totalRelative;
                        remainingLen[k] -= gridLayout[i];
                    }
            }

            // distribute the rest
            if (remainingLen[k]) {
                LengthType distributeType = countPercent ? Percent : Fixed;
                int total = countPercent ? totalPercent : totalFixed;
                if (!total) total = 1;
                for (int i = 0; i < gridLen; ++i)
                    if (grid[i].type == distributeType) {
                        int toAdd = (remainingLen[k] * grid[i].value) / total;
                        gridLayout[i] += toAdd;
                    }
            }

            // now we have the final layout, distribute the delta over it
            bool worked = true;
            for (int i = 0; i < gridLen; ++i) {
                if (gridLayout[i] && gridLayout[i] + gridDelta[i] <= 0)
                    worked = false;
                gridLayout[i] += gridDelta[i];
            }
            // now the delta's broke something, undo it and reset deltas
            if (!worked)
                for (int i = 0; i < gridLen; ++i) {
                    gridLayout[i] -= gridDelta[i];
                    gridDelta[i] = 0;
                }
        }
        else
            gridLayout[0] = remainingLen[k];
    }

    positionFrames();

    RenderObject *child = firstChild();
    if ( !child )
	goto end2;

    if(!m_hSplitVar && !m_vSplitVar)
    {
#ifdef DEBUG_LAYOUT
        kdDebug( 6031 ) << "calculationg fixed Splitters" << endl;
#endif
        if(!m_vSplitVar && element()->totalCols() > 1)
        {
            m_vSplitVar = new bool[element()->totalCols()];
            for(int i = 0; i < element()->totalCols(); i++) m_vSplitVar[i] = true;
        }
        if(!m_hSplitVar && element()->totalRows() > 1)
        {
            m_hSplitVar = new bool[element()->totalRows()];
            for(int i = 0; i < element()->totalRows(); i++) m_hSplitVar[i] = true;
        }

        for(int r = 0; r < element()->totalRows(); r++)
        {
            for(int c = 0; c < element()->totalCols(); c++)
            {
                bool fixed = false;

                if ( child->isFrameSet() )
                  fixed = static_cast<RenderFrameSet *>(child)->element()->noResize();
                else
                  fixed = static_cast<RenderFrame *>(child)->element()->noResize();

                if(fixed)
                {
#ifdef DEBUG_LAYOUT
                    kdDebug( 6031 ) << "found fixed cell " << r << "/" << c << "!" << endl;
#endif
                    if( element()->totalCols() > 1)
                    {
                        if(c>0) m_vSplitVar[c-1] = false;
                        m_vSplitVar[c] = false;
                    }
                    if( element()->totalRows() > 1)
                    {
                        if(r>0) m_hSplitVar[r-1] = false;
                        m_hSplitVar[r] = false;
                    }
                    child = child->nextSibling();
                    if(!child) goto end2;
                }
#ifdef DEBUG_LAYOUT
                else
                    kdDebug( 6031 ) << "not fixed: " << r << "/" << c << "!" << endl;
#endif
            }
        }

    }
    RenderContainer::layout();
 end2:
    setNeedsLayout(false);
}

void RenderFrameSet::positionFrames()
{
  int r;
  int c;

  RenderObject *child = firstChild();
  if ( !child )
    return;

  //  NodeImpl *child = _first;
  //  if(!child) return;

  int yPos = 0;

  for(r = 0; r < element()->totalRows(); r++)
  {
    int xPos = 0;
    for(c = 0; c < element()->totalCols(); c++)
    {
      child->setPos( xPos, yPos );
#ifdef DEBUG_LAYOUT
      kdDebug(6040) << "child frame at (" << xPos << "/" << yPos << ") size (" << m_gridLayout[1][c] << "/" << m_gridLayout[0][r] << ")" << endl;
#endif
      // has to be resized and itself resize its contents
      if ((m_gridLayout[1][c] != child->width()) || (m_gridLayout[0][r] != child->height())) {
          child->setWidth( m_gridLayout[1][c] );
          child->setHeight( m_gridLayout[0][r] );
          child->setNeedsLayout(true);
	  child->layout();
      }

      xPos += m_gridLayout[1][c] + element()->border();
      child = child->nextSibling();

      if ( !child )
        return;

    }

    yPos += m_gridLayout[0][r] + element()->border();
  }

  // all the remaining frames are hidden to avoid ugly
  // spurious unflowed frames
  while ( child ) {
      child->setWidth( 0 );
      child->setHeight( 0 );
      child->setNeedsLayout(false);

      child = child->nextSibling();
  }
}

bool RenderFrameSet::userResize( MouseEventImpl *evt )
{
    if (needsLayout()) return false;
    
    bool res = false;
    int _x = evt->clientX();
    int _y = evt->clientY();
    
    if ( !m_resizing && evt->id() == EventImpl::MOUSEMOVE_EVENT || evt->id() == EventImpl::MOUSEDOWN_EVENT )
    {
#ifdef DEBUG_LAYOUT
        kdDebug( 6031 ) << "mouseEvent:check" << endl;
#endif
        
        m_hSplit = -1;
        m_vSplit = -1;
        //bool resizePossible = true;
        
        // check if we're over a horizontal or vertical boundary
        int pos = m_gridLayout[1][0] + xPos();
        for(int c = 1; c < element()->totalCols(); c++)
        {
            if(_x >= pos && _x <= pos+element()->border())
            {
            if(m_vSplitVar && m_vSplitVar[c-1] == true) m_vSplit = c-1;
#ifdef DEBUG_LAYOUT
            kdDebug( 6031 ) << "vsplit!" << endl;
#endif
            res = true;
            break;
            }
            pos += m_gridLayout[1][c] + element()->border();
        }
        
        pos = m_gridLayout[0][0] + yPos();
        for(int r = 1; r < element()->totalRows(); r++)
        {
            if( _y >= pos && _y <= pos+element()->border())
            {
            if(m_hSplitVar && m_hSplitVar[r-1] == true) m_hSplit = r-1;
#ifdef DEBUG_LAYOUT
            kdDebug( 6031 ) << "hsplitvar = " << m_hSplitVar << endl;
            kdDebug( 6031 ) << "hsplit!" << endl;
#endif
            res = true;
            break;
            }
            pos += m_gridLayout[0][r] + element()->border();
        }
#ifdef DEBUG_LAYOUT
        kdDebug( 6031 ) << m_hSplit << "/" << m_vSplit << endl;
#endif
        
        QCursor cursor;
        if(m_hSplit != -1 && m_vSplit != -1)
        {
            cursor = KCursor::sizeAllCursor();
        }
        else if( m_vSplit != -1 )
        {
            cursor = KCursor::sizeHorCursor();
        }
        else if( m_hSplit != -1 )
        {
            cursor = KCursor::sizeVerCursor();
        }
        
        if(evt->id() == EventImpl::MOUSEDOWN_EVENT)
        {
            setResizing(true);
            KApplication::setOverrideCursor(cursor);
            m_vSplitPos = _x;
            m_hSplitPos = _y;
            m_oldpos = -1;
        }
        else
            canvas()->view()->viewport()->setCursor(cursor);
        
    }
    
    // ### check the resize is not going out of bounds.
    if(m_resizing && evt->id() == EventImpl::MOUSEUP_EVENT)
    {
        setResizing(false);
        KApplication::restoreOverrideCursor();
        
        if(m_vSplit != -1 )
        {
        #ifdef DEBUG_LAYOUT
            kdDebug( 6031 ) << "split xpos=" << _x << endl;
#endif
            int delta = m_vSplitPos - _x;
            m_gridDelta[1][m_vSplit] -= delta;
            m_gridDelta[1][m_vSplit+1] += delta;
        }
        if(m_hSplit != -1 )
        {
#ifdef DEBUG_LAYOUT
            kdDebug( 6031 ) << "split ypos=" << _y << endl;
#endif
            int delta = m_hSplitPos - _y;
            m_gridDelta[0][m_hSplit] -= delta;
            m_gridDelta[0][m_hSplit+1] += delta;
        }
        
        // this just schedules the relayout
        // important, otherwise the moving indicator is not correctly erased
        setNeedsLayout(true);
    }
    
    else if (m_resizing || evt->id() == EventImpl::MOUSEUP_EVENT) {
#if APPLE_CHANGES
        KHTMLView *v = canvas()->view();
        QPainter paint;
        
        v->disableFlushDrawing();
        v->lockDrawingFocus();
#else
        QPainter paint( canvas()->view() );
#endif
        paint.setPen( Qt::gray );
        paint.setBrush( Qt::gray );
        
#if !APPLE_CHANGES
        paint.setRasterOp( Qt::XorROP );
#endif
        QRect r(xPos(), yPos(), width(), height());
        const int rBord = 3;
        int sw = element()->border();
        int p = m_resizing ? (m_vSplit > -1 ? _x : _y) : -1;
        if (m_vSplit > -1) {
            if ( m_oldpos >= 0 )
#if APPLE_CHANGES
                v->updateContents( m_oldpos + sw/2 - rBord , r.y(), 2*rBord, r.height(), true );
#else
                paint.drawRect( m_oldpos + sw/2 - rBord , r.y(),
                                2*rBord, r.height() );
#endif
            if ( p >= 0 ){
#if APPLE_CHANGES
                paint.setPen( Qt::NoPen );
                paint.setBrush( Qt::gray );
                v->setDrawingAlpha((float)0.25);
                paint.drawRect( p  + sw/2 - rBord, r.y(), 2*rBord, r.height() );
                v->setDrawingAlpha((float)1.0);
#else
                paint.drawRect( p  + sw/2 - rBord, r.y(), 2*rBord, r.height() );
#endif
            }
        } else {
            if ( m_oldpos >= 0 )
#if APPLE_CHANGES
                v->updateContents( r.x(), m_oldpos + sw/2 - rBord, r.width(), 2*rBord, true );
#else
                paint.drawRect( r.x(), m_oldpos + sw/2 - rBord,
                                r.width(), 2*rBord );
#endif
            if ( p >= 0 ){
#if APPLE_CHANGES
                paint.setPen( Qt::NoPen );
                paint.setBrush( Qt::gray );
                v->setDrawingAlpha((float)0.25);
                paint.drawRect( r.x(), p + sw/2 - rBord, r.width(), 2*rBord );
                v->setDrawingAlpha((float)1.0);
#else
                paint.drawRect( r.x(), p + sw/2 - rBord, r.width(), 2*rBord );
#endif
            }
        }
        m_oldpos = p;

#if APPLE_CHANGES
        v->unlockDrawingFocus();
        v->enableFlushDrawing();
#endif
    }
    
    return res;
}

void RenderFrameSet::setResizing(bool e)
{
      m_resizing = e;
      for (RenderObject* p = parent(); p; p = p->parent())
          if (p->isFrameSet()) static_cast<RenderFrameSet*>(p)->m_clientresizing = m_resizing;
}

bool RenderFrameSet::canResize( int _x, int _y )
{
    // if we haven't received a layout, then the gridLayout doesn't contain useful data yet
    if (needsLayout() || !m_gridLayout[0] || !m_gridLayout[1] ) return false;

    // check if we're over a horizontal or vertical boundary
    int pos = m_gridLayout[1][0];
    for(int c = 1; c < element()->totalCols(); c++)
        if(_x >= pos && _x <= pos+element()->border())
            return true;

    pos = m_gridLayout[0][0];
    for(int r = 1; r < element()->totalRows(); r++)
        if( _y >= pos && _y <= pos+element()->border())
            return true;

    return false;
}

#ifndef NDEBUG
void RenderFrameSet::dump(QTextStream *stream, QString ind) const
{
  *stream << " totalrows=" << element()->totalRows();
  *stream << " totalcols=" << element()->totalCols();

  uint i;
  for (i = 0; i < (uint)element()->totalRows(); i++)
    *stream << " hSplitvar(" << i << ")=" << m_hSplitVar[i];

  for (i = 0; i < (uint)element()->totalCols(); i++)
    *stream << " vSplitvar(" << i << ")=" << m_vSplitVar[i];

  RenderContainer::dump(stream,ind);
}
#endif

/**************************************************************************************/

RenderPart::RenderPart(DOM::HTMLElementImpl* node)
    : RenderWidget(node)
{
    // init RenderObject attributes
    setInline(false);
}

RenderPart::~RenderPart()
{
    if (m_widget && m_widget->inherits("KHTMLView")) {
	static_cast<KHTMLView *>(m_widget)->deref();
    }
}

void RenderPart::setWidget( QWidget *widget )
{
#ifdef DEBUG_LAYOUT
    kdDebug(6031) << "RenderPart::setWidget()" << endl;
#endif
    
    if (widget == m_widget) {
	return;
    }

    if (m_widget && m_widget->inherits("KHTMLView")) {
	static_cast<KHTMLView *>(m_widget)->deref();
    }
    
    if (widget && widget->inherits("KHTMLView")) {	
	static_cast<KHTMLView *>(widget)->ref();
	setQWidget( widget, false );
	connect( widget, SIGNAL( cleared() ), this, SLOT( slotViewCleared() ) );
    } else {
	setQWidget( widget );
    }
    setNeedsLayoutAndMinMaxRecalc();
    
    // make sure the scrollbars are set correctly for restore
    // ### find better fix
    slotViewCleared();
}

void RenderPart::slotViewCleared()
{
}

/***************************************************************************************/

RenderFrame::RenderFrame( DOM::HTMLFrameElementImpl *frame )
    : RenderPart(frame)
{
    setInline( false );
}

void RenderFrame::slotViewCleared()
{
    if (element() && m_widget && m_widget->inherits("QScrollView")) {
#ifdef DEBUG_LAYOUT
        kdDebug(6031) << "frame is a scrollview!" << endl;
#endif
        QScrollView *view = static_cast<QScrollView *>(m_widget);
        if(!element()->frameBorder || !((static_cast<HTMLFrameSetElementImpl *>(element()->parentNode()))->frameBorder()))
            view->setFrameStyle(QFrame::NoFrame);
#if APPLE_CHANGES
        // Qt creates QScrollView w/ a default style of QFrame::StyledPanel | QFrame::Sunken.
        else
            view->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );

#else
        view->setHScrollBarMode(element()->scrolling );
        view->setVScrollBarMode(element()->scrolling );
#endif

        if(view->inherits("KHTMLView")) {
#ifdef DEBUG_LAYOUT
            kdDebug(6031) << "frame is a KHTMLview!" << endl;
#endif
            KHTMLView *htmlView = static_cast<KHTMLView *>(view);
            if(element()->marginWidth != -1) htmlView->setMarginWidth(element()->marginWidth);
            if(element()->marginHeight != -1) htmlView->setMarginHeight(element()->marginHeight);
        }
    }
}

/****************************************************************************************/

RenderPartObject::RenderPartObject( DOM::HTMLElementImpl* element )
    : RenderPart( element )
{
    // init RenderObject attributes
    setInline(true);
    m_hasFallbackContent = false;
}

void RenderPartObject::updateWidget()
{
  QString url;
  QString serviceType;
  QStringList paramNames;
  QStringList paramValues;
  KHTMLPart *part = m_view->part();

  setNeedsLayoutAndMinMaxRecalc();

  if (element()->id() == ID_OBJECT) {

      HTMLObjectElementImpl *o = static_cast<HTMLObjectElementImpl *>(element());

      // Check for a child EMBED tag.
      HTMLEmbedElementImpl *embed = 0;
      for (NodeImpl *child = o->firstChild(); child; ) {
          if (child->id() == ID_EMBED) {
              embed = static_cast<HTMLEmbedElementImpl *>( child );
              break;
          } else if (child->id() == ID_OBJECT) {
              child = child->nextSibling();         // Don't descend into nested OBJECT tags
          } else {
              child = child->traverseNextNode(o);   // Otherwise descend (EMBEDs may be inside COMMENT tags)
          }
      }
      
      // Use the attributes from the EMBED tag instead of the OBJECT tag including WIDTH and HEIGHT.
      HTMLElementImpl *embedOrObject;
      if (embed) {
          embedOrObject = (HTMLElementImpl *)embed;
          DOMString attribute = embedOrObject->getAttribute(ATTR_WIDTH);
          if (!attribute.isEmpty()) {
              o->setAttribute(ATTR_WIDTH, attribute);
          }
          attribute = embedOrObject->getAttribute(ATTR_HEIGHT);
          if (!attribute.isEmpty()) {
              o->setAttribute(ATTR_HEIGHT, attribute);
          }
          url = embed->url;
          serviceType = embed->serviceType;
      } else {
          embedOrObject = (HTMLElementImpl *)o;
      }
      
      // If there was no URL or type defined in EMBED, try the OBJECT tag.
      if (url.isEmpty()) {
          url = o->url;
      }
      if (serviceType.isEmpty()) {
          serviceType = o->serviceType;
      }
      
      QDict<bool> uniqueParamNames(5, false);
      
      // Scan the PARAM children.
      // Get the URL and type from the params if we don't already have them.
      // Get the attributes from the params if there is no EMBED tag.
      NodeImpl *child = o->firstChild();
      while (child && (url.isEmpty() || serviceType.isEmpty() || !embed)) {
          if (child->id() == ID_PARAM) {
              HTMLParamElementImpl *p = static_cast<HTMLParamElementImpl *>( child );
              QString name = p->name().lower();
              if (url.isEmpty() && (name == "src" || name == "movie" || name == "code" || name == "url")) {
                  url = p->value();
              }
              if (serviceType.isEmpty() && name == "type") {
                  serviceType = p->value();
                  int pos = serviceType.find( ";" );
                  if (pos != -1) {
                      serviceType = serviceType.left(pos);
                  }
              }
              if (!embed) {
                  bool dummyValue = true;
                  uniqueParamNames.insert(p->name(), &dummyValue);
                  paramNames.append(p->name());
                  paramValues.append(p->value());
              }
          }
          child = child->nextSibling();
      }
      
      // When OBJECT is used for an applet via Sun's Java plugin, the CODEBASE attribute in the tag
      // points to the Java plugin itself (an ActiveX component) while the actual applet CODEBASE is
      // in a PARAM tag. See <http://java.sun.com/products/plugin/1.2/docs/tags.html>. This means
      // we have to explicitly suppress the tag's CODEBASE attribute if there is none in a PARAM,
      // else our Java plugin will misinterpret it. [4004531]
      if (!embed && serviceType.lower() == "application/x-java-applet") {
          bool dummyValue = true;
          uniqueParamNames.insert("codebase", &dummyValue); // pretend we found it in a PARAM already
      }
      
      // Turn the attributes of either the EMBED tag or OBJECT tag into arrays, but don't override PARAM values.
      NamedAttrMapImpl* attributes = embedOrObject->attributes();
      if (attributes) {
          for (unsigned long i = 0; i < attributes->length(); ++i) {
              AttributeImpl* it = attributes->attributeItem(i);
              QString name = o->getDocument()->attrName(it->id()).string();
              if (embed || uniqueParamNames.find(name) == 0) {
                  paramNames.append(name);
                  paramValues.append(it->value().string());
              }
          }
      }
      
      // If we still don't have a type, try to map from a specific CLASSID to a type.
      if (serviceType.isEmpty() && !o->classId.isEmpty()) {
          // It is ActiveX, but the nsplugin system handling
          // should also work, that's why we don't override the
          // serviceType with application/x-activex-handler
          // but let the KTrader in khtmlpart::createPart() detect
          // the user's preference: launch with activex viewer or
          // with nspluginviewer (Niko)          
          if (o->classId.contains("D27CDB6E-AE6D-11cf-96B8-444553540000")) {
              serviceType = "application/x-shockwave-flash";
          } else if (o->classId.contains("CFCDAA03-8BE4-11cf-B84B-0020AFBBCCFA")) {
              serviceType = "audio/x-pn-realaudio-plugin";
          } else if (o->classId.contains("02BF25D5-8C17-4B23-BC80-D3488ABDDC6B")) {
              serviceType = "video/quicktime";
          } else if (o->classId.contains("166B1BCA-3F9C-11CF-8075-444553540000")) {
              serviceType = "application/x-director";
          } else if (o->classId.contains("6BF52A52-394A-11d3-B153-00C04F79FAA6")) {
              serviceType = "application/x-mplayer2";
          } else {
              // We have a clsid, means this is activex (Niko)
              serviceType = "application/x-activex-handler";
          }
          // TODO: add more plugins here
      }
      
      // If no URL and type, abort.
      if (url.isEmpty() && serviceType.isEmpty()) {
#ifdef DEBUG_LAYOUT
          kdDebug() << "RenderPartObject::close - empty url and serverType" << endl;
#endif
          return;
      }
      // Avoid infinite recursion. If the plug-in's URL is the same as the part's URL, infinite frames may be created.
      if (!url.isEmpty() && part->completeURL(url) == part->baseURL()) {
          return;
      }
            
#if !APPLE_CHANGES      
      params.append( QString::fromLatin1("__KHTML__CLASSID=\"%1\"").arg( o->classId ) );
      params.append( QString::fromLatin1("__KHTML__CODEBASE=\"%1\"").arg( o->getAttribute(ATTR_CODEBASE).string() ) );
#endif

      // Find out if we support fallback content.
      m_hasFallbackContent = false;
      for (NodeImpl *child = o->firstChild(); child && !m_hasFallbackContent; child = child->nextSibling()) {
          if ((!child->isTextNode() && child->id() != ID_EMBED && child->id() != ID_PARAM) || // Discount <embed> and <param>
              (child->isTextNode() && !child->containsOnlyWhitespace()))
              m_hasFallbackContent = true;
      }
      bool success = part->requestObject( this, url, serviceType, paramNames, paramValues );
      if (!success && m_hasFallbackContent)
          o->renderFallbackContent();
  } else if ( element()->id() == ID_EMBED ) {

      HTMLEmbedElementImpl *o = static_cast<HTMLEmbedElementImpl *>(element());
      url = o->url;
      serviceType = o->serviceType;

      if ( url.isEmpty() && serviceType.isEmpty() ) {
#ifdef DEBUG_LAYOUT
          kdDebug() << "RenderPartObject::close - empty url and serverType" << endl;
#endif
          return;
      }
      // Avoid infinite recursion. If the plug-in's URL is the same as the part's URL, infinite frames may be created.
      if (!url.isEmpty() && part->completeURL(url) == part->baseURL()) {
          return;
      }
      // add all attributes set on the embed object
      NamedAttrMapImpl* a = o->attributes();
      if (a) {
          for (unsigned long i = 0; i < a->length(); ++i) {
              AttributeImpl* it = a->attributeItem(i);
              paramNames.append(o->getDocument()->attrName(it->id()).string());
              paramValues.append(it->value().string());
          }
      }
      part->requestObject( this, url, serviceType, paramNames, paramValues );
  } else {
      assert(element()->id() == ID_IFRAME);
      HTMLIFrameElementImpl *o = static_cast<HTMLIFrameElementImpl *>(element());
      url = o->url.string();
      if (url.isEmpty()) {
	  url = "about:blank";
      }
      KHTMLView *v = static_cast<KHTMLView *>(m_view);
      bool requestSucceeded = v->part()->requestFrame( this, url, o->name.string(), QStringList(), QStringList(), true );
      if (requestSucceeded && url == "about:blank") {
	  KHTMLPart *newPart = v->part()->findFrame( o->name.string() );
	  if (newPart && newPart->xmlDocImpl()) {
	      newPart->xmlDocImpl()->setBaseURL( v->part()->baseURL().url() );
	  }
      }
  }
}

void RenderPartObject::layout( )
{
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

#if !APPLE_CHANGES
    int m_oldwidth = m_width;
    int m_oldheight = m_height;
#endif

    calcWidth();
    calcHeight();

    RenderPart::layout();

    setNeedsLayout(false);
}

void RenderPartObject::slotViewCleared()
{
  if(element() && m_widget && m_widget->inherits("QScrollView") ) {
#ifdef DEBUG_LAYOUT
      kdDebug(6031) << "iframe is a scrollview!" << endl;
#endif
      QScrollView *view = static_cast<QScrollView *>(m_widget);
      int frameStyle = QFrame::NoFrame;
      QScrollView::ScrollBarMode scroll = QScrollView::Auto;
      int marginw = -1;
      int marginh = -1;
      if ( element()->id() == ID_IFRAME) {
	  HTMLIFrameElementImpl *frame = static_cast<HTMLIFrameElementImpl *>(element());
	  if(frame->frameBorder)
	      frameStyle = QFrame::Box;
          scroll = frame->scrolling;
	  marginw = frame->marginWidth;
	  marginh = frame->marginHeight;
      }
      view->setFrameStyle(frameStyle);

#if !APPLE_CHANGES
      view->setVScrollBarMode(scroll);
      view->setHScrollBarMode(scroll);
#endif

      if(view->inherits("KHTMLView")) {
#ifdef DEBUG_LAYOUT
          kdDebug(6031) << "frame is a KHTMLview!" << endl;
#endif
          KHTMLView *htmlView = static_cast<KHTMLView *>(view);
          htmlView->setIgnoreWheelEvents( element()->id() == ID_IFRAME );
          if(marginw != -1) htmlView->setMarginWidth(marginw);
          if(marginh != -1) htmlView->setMarginHeight(marginh);
        }
  }
}

#if APPLE_CHANGES
// FIXME: This should not be necessary.  Remove this once WebKit knows to properly schedule
// layouts using WebCore when objects resize.
void RenderPart::updateWidgetPositions()
{
    if (!m_widget)
        return;
    
    int x, y, width, height;
    absolutePosition(x,y);
    x += borderLeft() + paddingLeft();
    y += borderTop() + paddingTop();
    width = m_width - borderLeft() - borderRight() - paddingLeft() - paddingRight();
    height = m_height - borderTop() - borderBottom() - paddingTop() - paddingBottom();
    QRect newBounds(x,y,width,height);
    if (newBounds != m_widget->frameGeometry()) {
        // The widget changed positions.  Update the frame geometry.
        RenderArena *arena = ref();
        element()->ref();
        m_widget->setFrameGeometry(newBounds);
        element()->deref();
        deref(arena);
        
        QScrollView *view = static_cast<QScrollView *>(m_widget);
        if (view && view->inherits("KHTMLView"))
            static_cast<KHTMLView*>(view)->layout();
    }
}
#endif

#include "render_frames.moc"
