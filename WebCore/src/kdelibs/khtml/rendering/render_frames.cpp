/**
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
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
 * $Id$
 */
#define DEBUG_LAYOUT

#include "render_frames.h"
#include "html_baseimpl.h"
#include "html_objectimpl.h"
#include "misc/htmlattrs.h"
#include "dom2_eventsimpl.h"
#include "htmltags.h"
#include "khtmlview.h"
#include "khtml_part.h"

#include <kapp.h>
#include <kdebug.h>

#include <qlabel.h>
#include <qstringlist.h>

#include <assert.h>
#include <kdebug.h>

using namespace khtml;
using namespace DOM;

RenderFrameSet::RenderFrameSet( HTMLFrameSetElementImpl *frameSet, KHTMLView *view,
                                QList<khtml::Length> *rows, QList<khtml::Length> *cols )
    : RenderBox()
{
  // init RenderObject attributes
    setInline(false);

  m_frameset = frameSet;

  m_rows = rows;
  m_cols = cols;

  // another one for bad html
  // handle <frameset cols="*" rows="100, ...">
  if ( m_rows && m_cols ) {
      // lets see if one of them is relative
      if ( m_rows->count() == 1 && m_rows->at( 0 )->isRelative() )
            m_rows = 0;
      if ( m_cols->count() == 1 && m_cols->at( 0 )->isRelative() )
          m_cols = 0;
  }

  m_rowHeight = 0;
  m_colWidth = 0;

  m_resizing = false;

  m_hSplit = -1;
  m_vSplit = -1;

  m_hSplitVar = 0;
  m_vSplitVar = 0;

  m_view = view;
}

RenderFrameSet::~RenderFrameSet()
{
  if ( m_rowHeight ) {
    delete [] m_rowHeight;
    m_rowHeight = 0;
  }
  if ( m_colWidth ) {
    delete [] m_colWidth;
    m_colWidth = 0;
  }

  delete [] m_hSplitVar;
  delete [] m_vSplitVar;
}

void RenderFrameSet::close()
{
    RenderBox::close();

    if(m_frameset->verifyLayout())
        setLayouted(false);
}

void RenderFrameSet::layout( )
{
    if ( strcmp( parent()->renderName(), "RenderFrameSet" ) != 0 )
    {
        m_width = m_view->visibleWidth();
        m_height = m_view->visibleHeight();
    }

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(FrameSet)::layout( ) width=" << width() << ", height=" << height() << endl;
#endif

    int remainingWidth = m_width - (m_frameset->totalCols()-1)*m_frameset->border();
    if(remainingWidth<0) remainingWidth=0;
    int remainingHeight = m_height - (m_frameset->totalRows()-1)*m_frameset->border();
    if(remainingHeight<0) remainingHeight=0;
    int widthAvailable = remainingWidth;
    int heightAvailable = remainingHeight;

    if(m_rowHeight) delete [] m_rowHeight;
    if(m_colWidth) delete [] m_colWidth;
    m_rowHeight = new int[m_frameset->totalRows()];
    m_colWidth = new int[m_frameset->totalCols()];

    int i;
    int totalRelative = 0;
    int colsRelative = 0;
    int rowsRelative = 0;
    int rowsPercent = 0;
    int colsPercent = 0;
    int remainingRelativeWidth = 0;

    if(m_rows)
    {
	// another one for bad html. If all rows have a fixed width, convert the numbers to percentages.
	bool allFixed = true;
	int totalFixed = 0;
	for(i = 0; i< m_frameset->totalRows(); i++) {
	    if(m_rows->at(i)->type != Fixed)
		allFixed = false;
	    else
		totalFixed += m_rows->at(i)->value;
	}
	if ( allFixed && totalFixed ) {
	    for(i = 0; i< m_frameset->totalRows(); i++) {
		m_rows->at(i)->type = Percent;
		m_rows->at(i)->value = m_rows->at(i)->value *100 / totalFixed;
	    }
	}

        // first distribute the available width for fixed rows, then handle the
        // percentage ones, to fix html like <framesrc rows="123,100%,123"> and
        // finally relative

        for(i = 0; i< m_frameset->totalRows(); i++)
        {
             if(m_rows->at(i)->type == Fixed)
            {
                m_rowHeight[i] = QMAX(m_rows->at(i)->width(heightAvailable), 14);
                if( m_rowHeight[i] > remainingHeight )
                    m_rowHeight[i] = remainingHeight;
                 remainingHeight -= m_rowHeight[i];
            }
            else if(m_rows->at(i)->type == Relative)
            {
                totalRelative += m_rows->at(i)->value;
                rowsRelative++;
            }
        }

        for(i = 0; i< m_frameset->totalRows(); i++)
        {
             if(m_rows->at(i)->type == Percent)
            {
                m_rowHeight[i] = QMAX(m_rows->at(i)->width(heightAvailable), 14);
                if( m_rowHeight[i] > remainingHeight )
                    m_rowHeight[i] = remainingHeight;
                 remainingHeight -= m_rowHeight[i];
                 rowsPercent++;
            }
        }

        // ###
        if(remainingHeight < 0) remainingHeight = 0;

        if ( !totalRelative && rowsRelative )
          remainingRelativeWidth = remainingHeight/rowsRelative;

        for(i = 0; i< m_frameset->totalRows(); i++)
         {
            if(m_rows->at(i)->type == Relative)
            {
                if ( totalRelative )
                  m_rowHeight[i] = m_rows->at(i)->value*remainingHeight/totalRelative;
                else
                  m_rowHeight[i] = remainingRelativeWidth;
                remainingHeight -= m_rowHeight[i];
                totalRelative--;
            }
        }

        // support for totally broken frame declarations
        if(remainingHeight)
        {
            // just distribute it over all columns...
            int rows = m_frameset->totalRows();
            if ( rowsPercent )
                rows = rowsPercent;
            for(i = 0; i< m_frameset->totalRows(); i++) {
                if( !rowsPercent || m_rows->at(i)->type == Percent ) {
                    int toAdd = remainingHeight/rows;
                    rows--;
                    m_rowHeight[i] += toAdd;
                    remainingHeight -= toAdd;
                }
            }
        }
    }
    else
        m_rowHeight[0] = m_height;

    if(m_cols)
    {
	// another one for bad html. If all cols have a fixed width, convert the numbers to percentages.
	// also reproduces IE and NS behaviour.
	bool allFixed = true;
	int totalFixed = 0;
	for(i = 0; i< m_frameset->totalCols(); i++) {
	    if(m_cols->at(i)->type != Fixed)
		allFixed = false;
	    else
		totalFixed += m_cols->at(i)->value;
	}
	if ( allFixed && totalFixed) {
	    for(i = 0; i< m_frameset->totalCols(); i++) {
		m_cols->at(i)->type = Percent;
		m_cols->at(i)->value = m_cols->at(i)->value * 100 / totalFixed;
	    }
	}

        totalRelative = 0;
        remainingRelativeWidth = 0;

        // first distribute the available width for fixed columns, then handle the
        // percentage ones, to fix html like <framesrc cols="123,100%,123"> and
        // finally relative

        for(i = 0; i< m_frameset->totalCols(); i++)
        {
            if (m_cols->at(i)->type == Fixed)
            {
                m_colWidth[i] = QMAX(m_cols->at(i)->width(widthAvailable), 14);
                if( m_colWidth[i] > remainingWidth )
                    m_colWidth[i] = remainingWidth;
                remainingWidth -= m_colWidth[i];
            }
            else if(m_cols->at(i)->type == Relative)
            {
                totalRelative += m_cols->at(i)->value;
                colsRelative++;
            }
        }

        for(i = 0; i< m_frameset->totalCols(); i++)
        {
            if(m_cols->at(i)->type == Percent)
            {
                m_colWidth[i] = QMAX(m_cols->at(i)->width(widthAvailable), 14);
                if( m_colWidth[i] > remainingWidth )
                    m_colWidth[i] = remainingWidth;
                remainingWidth -= m_colWidth[i];
                colsPercent++;
            }
        }
        // ###
        if(remainingWidth < 0) remainingWidth = 0;

        if ( !totalRelative && colsRelative )
          remainingRelativeWidth = remainingWidth/colsRelative;

        for(i = 0; i < m_frameset->totalCols(); i++)
        {
            if(m_cols->at(i)->type == Relative)
            {
                if ( totalRelative )
                  m_colWidth[i] = m_cols->at(i)->value*remainingWidth/totalRelative;
                else
                  m_colWidth[i] = remainingRelativeWidth;
                remainingWidth -= m_colWidth[i];
                totalRelative--;
            }
        }

        // support for totally broken frame declarations
        if(remainingWidth)
        {
            // just distribute it over all columns...
            int cols = m_frameset->totalCols();
            if ( colsPercent )
                cols = colsPercent;
            for(i = 0; i< m_frameset->totalCols(); i++) {
                if( !colsPercent || m_cols->at(i)->type == Percent ) {
                    int toAdd = remainingWidth/cols;
                    cols--;
                    m_colWidth[i] += toAdd;
                    remainingWidth -= toAdd;
                }
            }
        }

    }
    else
        m_colWidth[0] = m_width;

    positionFrames();

    RenderObject *child = firstChild();
    if ( !child )
      return;

    if(!m_hSplitVar && !m_vSplitVar)
    {
#ifdef DEBUG_LAYOUT
        kdDebug( 6031 ) << "calculationg fixed Splitters" << endl;
#endif
        if(!m_vSplitVar && m_frameset->totalCols() > 1)
        {
            m_vSplitVar = new bool[m_frameset->totalCols()];
            for(int i = 0; i < m_frameset->totalCols(); i++) m_vSplitVar[i] = true;
        }
        if(!m_hSplitVar && m_frameset->totalRows() > 1)
        {
            m_hSplitVar = new bool[m_frameset->totalRows()];
            for(int i = 0; i < m_frameset->totalRows(); i++) m_hSplitVar[i] = true;
        }

        for(int r = 0; r < m_frameset->totalRows(); r++)
        {
            for(int c = 0; c < m_frameset->totalCols(); c++)
            {
                bool fixed = false;

                if ( strcmp( child->renderName(), "RenderFrameSet" ) == 0 )
                  fixed = static_cast<RenderFrameSet *>(child)->frameSetImpl()->noResize();
                else
                  fixed = static_cast<RenderFrame *>(child)->frameImpl()->noResize();

                /*
                if(child->id() == ID_FRAMESET)
                    fixed = (static_cast<HTMLFrameSetElementImpl *>(child))->noResize();
                else if(child->id() == ID_FRAME)
                    fixed = (static_cast<HTMLFrameElementImpl *>(child))->noResize();
                */

                if(fixed)
                {
#ifdef DEBUG_LAYOUT
                    kdDebug( 6031 ) << "found fixed cell " << r << "/" << c << "!" << endl;
#endif
                    if( m_frameset->totalCols() > 1)
                    {
                        if(c>0) m_vSplitVar[c-1] = false;
                        m_vSplitVar[c] = false;
                    }
                    if( m_frameset->totalRows() > 1)
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
 end2:
    setLayouted();
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

  for(r = 0; r < m_frameset->totalRows(); r++)
  {
    int xPos = 0;
    for(c = 0; c < m_frameset->totalCols(); c++)
    {
    //      HTMLElementImpl *e = static_cast<HTMLElementImpl *>(child);
      child->setPos( xPos, yPos );
#ifdef DEBUG_LAYOUT
      kdDebug(6040) << "child frame at (" << xPos << "/" << yPos << ") size (" << m_colWidth[c] << "/" << m_rowHeight[r] << ")" << endl;
#endif
      child->setWidth( m_colWidth[c] );
      child->setHeight( m_rowHeight[r] );
      child->layout( );

      xPos += m_colWidth[c] + m_frameset->border();
      child = child->nextSibling();

      if ( !child )
        return;
    /*
            e->renderer()->setPos(xPos, yPos);
            e->setWidth(colWidth[c]);
            e->setAvailableWidth(colWidth[c]);
            e->setDescent(rowHeight[r]);
            e->layout();
            xPos += colWidth[c] + border;
            child = child->nextSibling();
            if(!child) return;
    */
        }
        yPos += m_rowHeight[r] + m_frameset->border();
    }
}

bool RenderFrameSet::userResize( MouseEventImpl *evt )
{
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
    int pos = m_colWidth[0];
    for(int c = 1; c < m_frameset->totalCols(); c++)
    {
      if(_x >= pos && _x <= pos+m_frameset->border())
      {
        if(m_vSplitVar && m_vSplitVar[c-1] == true) m_vSplit = c-1;
#ifdef DEBUG_LAYOUT
        kdDebug( 6031 ) << "vsplit!" << endl;
#endif
        res = true;
        break;
      }
      pos += m_colWidth[c] + m_frameset->border();
    }

    pos = m_rowHeight[0];
    for(int r = 1; r < m_frameset->totalRows(); r++)
    {
      if( _y >= pos && _y <= pos+m_frameset->border())
      {
        if(m_hSplitVar && m_hSplitVar[r-1] == true) m_hSplit = r-1;
#ifdef DEBUG_LAYOUT
        kdDebug( 6031 ) << "hsplitvar = " << m_hSplitVar << endl;
        kdDebug( 6031 ) << "hsplit!" << endl;
#endif
        res = true;
        break;
      }
      pos += m_rowHeight[r] + m_frameset->border();
    }
#ifdef DEBUG_LAYOUT
    kdDebug( 6031 ) << m_hSplit << "/" << m_vSplit << endl;
#endif

    QCursor cursor;
    if(m_hSplit != -1 && m_vSplit != -1)
    {
      cursor = Qt::sizeAllCursor;
    }
    else if( m_vSplit != -1 )
    {
      cursor = Qt::splitHCursor;
    }
    else if( m_hSplit != -1 )
    {
      cursor = Qt::splitVCursor;
    }

    if(evt->id() == EventImpl::MOUSEDOWN_EVENT)
    {
      m_resizing = true;
      KApplication::setOverrideCursor(cursor);
      m_vSplitPos = _x;
      m_hSplitPos = _y;
    }
    else
      static_cast<KHTMLView *>(m_view)->setCursor(cursor);

  }

  // ### need to draw a nice movin indicator for the resize.
  // ### check the resize is not going out of bounds.
  if(m_resizing && evt->id() == EventImpl::MOUSEUP_EVENT)
  {
    m_resizing = false;
    KApplication::restoreOverrideCursor();

    if(m_vSplit != -1 )
    {
#ifdef DEBUG_LAYOUT
      kdDebug( 6031 ) << "split xpos=" << _x << endl;
#endif
      int delta = m_vSplitPos - _x;
      m_colWidth[m_vSplit] -= delta;
      m_colWidth[m_vSplit+1] += delta;
    }
    if(m_hSplit != -1 )
    {
#ifdef DEBUG_LAYOUT
      kdDebug( 6031 ) << "split ypos=" << _y << endl;
#endif
      int delta = m_hSplitPos - _y;
      m_rowHeight[m_hSplit] -= delta;
      m_rowHeight[m_hSplit+1] += delta;
    }

    positionFrames( );
  }

  return res;
}

bool RenderFrameSet::canResize( int _x, int _y, DOM::NodeImpl::MouseEventType type )
{
   if(m_resizing || type == DOM::NodeImpl::MousePress)
     return true;

  if ( type != DOM::NodeImpl::MouseMove )
    return false;

  // check if we're over a horizontal or vertical boundary
  int pos = m_colWidth[0];
  for(int c = 1; c < m_frameset->totalCols(); c++)
    if(_x >= pos && _x <= pos+m_frameset->border())
      return true;

  pos = m_rowHeight[0];
  for(int r = 1; r < m_frameset->totalRows(); r++)
    if( _y >= pos && _y <= pos+m_frameset->border())
      return true;

  return false;
}

/**************************************************************************************/

RenderPart::RenderPart( QScrollView *view )
    : RenderWidget( view )
{
    // init RenderObject attributes
    setInline(false);

    m_view = view;
}

void RenderPart::setWidget( QWidget *widget )
{
#ifdef DEBUG_LAYOUT
    kdDebug(6031) << "RenderPart::setWidget()" << endl;
#endif
    setQWidget( widget );
    if(widget->inherits("KHTMLView"))
        connect( widget, SIGNAL( cleared() ), this, SLOT( slotViewCleared() ) );

    setLayouted( false );
    setMinMaxKnown( false );

    updateSize();

    // make sure the scrollbars are set correctly for restore
    // ### find better fix
    slotViewCleared();
}

void RenderPart::layout( )
{
    if ( m_widget )
        m_widget->resize( QMIN( m_width, 2000 ), QMIN( m_height, 3860 ) );
}

void RenderPart::partLoadingErrorNotify()
{
}

short RenderPart::intrinsicWidth() const
{
    return 300;
}

int RenderPart::intrinsicHeight() const
{
    return 200;
}

void RenderPart::slotViewCleared()
{
}

/***************************************************************************************/

RenderFrame::RenderFrame( QScrollView *view, DOM::HTMLFrameElementImpl *frame )
    : RenderPart( view ), m_frame( frame )
{
    setInline( false );

}

void RenderFrame::slotViewCleared()
{
    if(m_widget->inherits("QScrollView")) {
#ifdef DEBUG_LAYOUT
        kdDebug(6031) << "frame is a scrollview!" << endl;
#endif
        QScrollView *view = static_cast<QScrollView *>(m_widget);
        if(!m_frame->frameBorder || !((static_cast<HTMLFrameSetElementImpl *>(m_frame->_parent))->frameBorder()))
            view->setFrameStyle(QFrame::NoFrame);
        view->setVScrollBarMode(m_frame->scrolling);
        view->setHScrollBarMode(m_frame->scrolling);
        if(view->inherits("KHTMLView")) {
#ifdef DEBUG_LAYOUT
            kdDebug(6031) << "frame is a KHTMLview!" << endl;
#endif
            KHTMLView *htmlView = static_cast<KHTMLView *>(view);
            if(m_frame->marginWidth != -1) htmlView->setMarginWidth(m_frame->marginWidth);
            if(m_frame->marginHeight != -1) htmlView->setMarginHeight(m_frame->marginHeight);
        }
    }
}

/****************************************************************************************/

RenderPartObject::RenderPartObject( QScrollView *view, DOM::HTMLElementImpl *o )
    : RenderPart( view )
{
    // init RenderObject attributes
    setInline(true);

    m_obj = o;
}

void RenderPartObject::updateWidget()
{
  QString url;
  QString serviceType;

  if(m_obj->id() == ID_OBJECT) {

      // check for embed child object
     HTMLObjectElementImpl *o = static_cast<HTMLObjectElementImpl *>(m_obj);
     HTMLEmbedElementImpl *embed = 0;
     NodeImpl *child = o->firstChild();
     while ( child ) {
        if ( child->id() == ID_EMBED )
           embed = static_cast<HTMLEmbedElementImpl *>( child );

        child = child->nextSibling();
     }

     if ( !embed )
     {
        url = o->url;
        serviceType = o->serviceType;
    	if(serviceType.isEmpty() || serviceType.isNull()) {
           if(!o->classId.isEmpty()) {
              // We have a clsid, means this is activex (Niko)
              serviceType = "application/x-activex-handler";
              url = "dummy"; // Not needed, but KHTMLPart aborts the request if empty
           }

           if(o->classId.contains(QString::fromLatin1("D27CDB6E-AE6D-11cf-96B8-444553540000"))) {
              // It is ActiveX, but the nsplugin system handling
              // should also work, that's why we don't override the
              // serviceType with application/x-activex-handler
              // but let the KTrader in khtmlpart::createPart() detect
              // the user's preference: launch with activex viewer or
              // with nspluginviewer (Niko)
              serviceType = "application/x-shockwave-flash";
           }
           else if(o->classId.contains(QString::fromLatin1("CFCDAA03-8BE4-11cf-B84B-0020AFBBCCFA")))
              serviceType = "audio/x-pn-realaudio-plugin";

           // TODO: add more plugins here
        }

        if((url.isEmpty() || url.isNull())) {
           // look for a SRC attribute in the params
           NodeImpl *child = o->firstChild();
           while ( child ) {
              if ( child->id() == ID_PARAM ) {
                 HTMLParamElementImpl *p = static_cast<HTMLParamElementImpl *>( child );

                 if ( p->name().lower()==QString::fromLatin1("src") ||
                      p->name().lower()==QString::fromLatin1("movie") ||
                      p->name().lower()==QString::fromLatin1("code") )
                 {
                    url = p->value();
                    break;
                 }
              }
              child = child->nextSibling();
           }
        }

        // add all <param>'s to the QStringList argument of the part
        QStringList params;
        NodeImpl *child = o->firstChild();
        while ( child ) {
           if ( child->id() == ID_PARAM ) {
              HTMLParamElementImpl *p = static_cast<HTMLParamElementImpl *>( child );

              QString aStr = p->name();
              aStr += QString::fromLatin1("=\"");
              aStr += p->value();
              aStr += QString::fromLatin1("\"");
              params.append(aStr);
           }

           child = child->nextSibling();
        }

        if ( url.isEmpty() && serviceType.isEmpty() ) {
#ifdef DEBUG_LAYOUT
            kdDebug() << "RenderPartObject::close - empty url and serverType" << endl;
#endif
            return;
        }

        KHTMLPart *part = static_cast<KHTMLView *>(m_view)->part();

        params.append( QString::fromLatin1("__KHTML__PLUGINEMBED=\"YES\"") );
        params.append( QString::fromLatin1("__KHTML__PLUGINBASEURL=\"%1\"").arg( part->url().url() ) );
        params.append( QString::fromLatin1("__KHTML__CLASSID=\"%1\"").arg( o->classId ) );
        params.append( QString::fromLatin1("__KHTML__CODEBASE=\"%1\"").arg( static_cast<ElementImpl *>(o)->getAttribute(ATTR_CODEBASE).string() ) );

        part->requestObject( this, url, serviceType, params );
     }
     else {
        // render embed object
        url = embed->url;
        serviceType = embed->serviceType;

        if ( url.isEmpty() && serviceType.isEmpty() ) {
#ifdef DEBUG_LAYOUT
            kdDebug() << "RenderPartObject::close - empty url and serverType" << endl;
#endif
            return;
        }

        KHTMLPart *part = static_cast<KHTMLView *>(m_view)->part();

        embed->param.append( QString::fromLatin1("__KHTML__PLUGINEMBED=\"YES\"") );
        embed->param.append( QString::fromLatin1("__KHTML__PLUGINBASEURL=\"%1\"").arg( part->url().url() ) );
        embed->param.append( QString::fromLatin1("__KHTML__CLASSID=\"%1\"").arg( o->classId ) );
        embed->param.append( QString::fromLatin1("__KHTML__CODEBASE=\"%1\"").arg( static_cast<ElementImpl *>(o)->getAttribute(ATTR_CODEBASE).string() ) );

        // Check if serviceType can be handled by ie. nsplugin
		// else default to the activexhandler if there is a classid
        // and a codebase, where we may download the ocx if it's missing (Niko)
        bool retval = part->requestObject( this, url, serviceType, embed->param );

        if(!retval && !o->classId.isEmpty() && !( static_cast<ElementImpl *>(o)->getAttribute(ATTR_CODEBASE).string() ).isEmpty() )
        {
           serviceType = "application/x-activex-handler";
           part->requestObject( this, url, serviceType, embed->param );
        }
     }
  }
  else if ( m_obj->id() == ID_EMBED ) {

     HTMLEmbedElementImpl *o = static_cast<HTMLEmbedElementImpl *>(m_obj);
     url = o->url;
     serviceType = o->serviceType;

     if ( url.isEmpty() && serviceType.isEmpty() ) {
#ifdef DEBUG_LAYOUT
         kdDebug() << "RenderPartObject::close - empty url and serverType" << endl;
#endif
         return;
     }

     KHTMLPart *part = static_cast<KHTMLView *>(m_view)->part();

     o->param.append( QString::fromLatin1("__KHTML__PLUGINEMBED=\"YES\"") );
     o->param.append( QString::fromLatin1("__KHTML__PLUGINBASEURL=\"%1\"").arg( part->url().url() ) );

     part->requestObject( this, url, serviceType, o->param );

  } else {
      assert(m_obj->id() == ID_IFRAME);
      HTMLIFrameElementImpl *o = static_cast<HTMLIFrameElementImpl *>(m_obj);
      url = o->url.string();
      if( url.isEmpty()) return;
      KHTMLView *v = static_cast<KHTMLView *>(m_view);
      v->part()->requestFrame( this, url, o->name.string(), QStringList(), true );
  }
  setLayouted(false);

}

// ugly..
void RenderPartObject::close()
{
    updateWidget();
    RenderPart::close();
}


void RenderPartObject::partLoadingErrorNotify()
{
    HTMLEmbedElementImpl *embed = 0;

    if( m_obj->id()==ID_OBJECT ) {

        // check for embed child object
        HTMLObjectElementImpl *o = static_cast<HTMLObjectElementImpl *>(m_obj);
        NodeImpl *child = o->firstChild();
        while ( child ) {
            if ( child->id() == ID_EMBED )
                embed = static_cast<HTMLEmbedElementImpl *>( child );

            child = child->nextSibling();
        }

    } else if( m_obj->id()==ID_EMBED )
        embed = static_cast<HTMLEmbedElementImpl *>(m_obj);

    // Display vender download page
    if( embed && !embed->pluginPage.isEmpty() ) {
        KHTMLPart *part = static_cast<KHTMLView *>(m_view)->part();
        KParts::BrowserExtension *ext = part->browserExtension();
        if ( ext ) ext->createNewWindow( KURL(embed->pluginPage) );
    }
}

void RenderPartObject::layout( )
{
    if ( layouted() ) return;

    // minimum height
    m_height = 0;

    calcWidth();
    calcHeight();

    if ( !style()->width().isPercent() )
        setLayouted();

    RenderPart::layout();
}

void RenderPartObject::slotViewCleared()
{
  if(m_widget->inherits("QScrollView") ) {
#ifdef DEBUG_LAYOUT
      kdDebug(6031) << "iframe is a scrollview!" << endl;
#endif
      QScrollView *view = static_cast<QScrollView *>(m_widget);
      int frameStyle = QFrame::NoFrame;
      QScrollView::ScrollBarMode scroll = QScrollView::Auto;
      int marginw = 0;
      int marginh = 0;
      if ( m_obj->id() == ID_IFRAME) {
	  HTMLIFrameElementImpl *m_frame = static_cast<HTMLIFrameElementImpl *>(m_obj);
	  if(m_frame->frameBorder)
	      frameStyle = QFrame::Box;
	  scroll = m_frame->scrolling;
	  marginw = m_frame->marginWidth;
	  marginh = m_frame->marginHeight;
      }
      view->setFrameStyle(frameStyle);
      view->setVScrollBarMode(scroll);
      view->setHScrollBarMode(scroll);
      if(view->inherits("KHTMLView")) {
#ifdef DEBUG_LAYOUT
          kdDebug(6031) << "frame is a KHTMLview!" << endl;
#endif
          KHTMLView *htmlView = static_cast<KHTMLView *>(view);
          if(marginw != -1) htmlView->setMarginWidth(marginw);
          if(marginh != -1) htmlView->setMarginHeight(marginh);
        }
  }
}

#include "render_frames.moc"
