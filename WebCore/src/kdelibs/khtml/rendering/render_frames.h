/**
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
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
#ifndef __render_frames_h__
#define __render_frames_h__

#include "render_replaced.h"
#include "xml/dom_nodeimpl.h"

class KHTMLView;

namespace DOM
{
  class HTMLFrameSetElementImpl;
  class HTMLFrameElementImpl;
  class HTMLElementImpl;
  class MouseEventImpl;
};

namespace khtml
{

class RenderFrameSet : public RenderBox
{
  friend class DOM::HTMLFrameSetElementImpl;
public:
  RenderFrameSet( DOM::HTMLFrameSetElementImpl *frameSet, KHTMLView *view,
                  QList<khtml::Length> *rows, QList<khtml::Length> *cols );

  virtual ~RenderFrameSet();

  virtual const char *renderName() const { return "RenderFrameSet"; }

  virtual void layout();
    virtual void close();

  void positionFrames( );

  bool resizing() const { return m_resizing; }

  bool userResize( DOM::MouseEventImpl *evt );
  bool canResize( int _x, int _y, DOM::NodeImpl::MouseEventType type );

  DOM::HTMLFrameSetElementImpl *frameSetImpl() const { return m_frameset; }

private:
  DOM::HTMLFrameSetElementImpl *m_frameset;

  QList<khtml::Length> *m_rows;
  QList<khtml::Length> *m_cols;
  KHTMLView *m_view;
  int *m_rowHeight;
  int *m_colWidth;
  bool *m_hSplitVar; // is this split variable?
  bool *m_vSplitVar;

  int m_hSplit;     // the split currently resized
  int m_vSplit;
  int m_hSplitPos;
  int m_vSplitPos;

  bool m_resizing;
};

class RenderPart : public khtml::RenderWidget
{
    Q_OBJECT
public:
    RenderPart( QScrollView *view );

    virtual const char *renderName() const { return "RenderPart"; }

    virtual void layout();

    virtual void setWidget( QWidget *widget );

    /**
     * Called by KHTMLPart to notify the frame object that loading the
     * part was not successfuly. (called either asyncroniously after a
     * after the servicetype of the given url (the one passed with requestObject)
     * has been determined or syncroniously from within requestObject)
     *
     * The default implementation does nothing.
     */
    virtual void partLoadingErrorNotify();
    
    virtual short intrinsicWidth() const;
    virtual int intrinsicHeight() const;
    
public slots:
    virtual void slotViewCleared();
};

class RenderFrame : public khtml::RenderPart
{
    Q_OBJECT
public:
    RenderFrame( QScrollView *view, DOM::HTMLFrameElementImpl *frame );

    virtual const char *renderName() const { return "RenderFrame"; }

    DOM::HTMLFrameElementImpl *frameImpl() const { return m_frame; }

public slots:
    void slotViewCleared();

private:
    DOM::HTMLFrameElementImpl *m_frame;
};

// I can hardly call the class RenderObject ;-)
class RenderPartObject : public khtml::RenderPart
{
    Q_OBJECT
public:
    RenderPartObject( QScrollView *view, DOM::HTMLElementImpl *o );

    virtual const char *renderName() const { return "RenderPartObject"; }

    virtual void close();

    virtual void layout( );
    virtual void updateWidget();

    DOM::HTMLElementImpl *m_obj;

    virtual void partLoadingErrorNotify();
    
public slots:
    void slotViewCleared();
};

};

#endif
