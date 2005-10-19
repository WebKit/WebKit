/*
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
 */
#ifndef __render_frames_h__
#define __render_frames_h__

#include "render_container.h"
#include "rendering/render_replaced.h"
#include "xml/dom_nodeimpl.h"
#include "html/html_baseimpl.h"
class KHTMLView;

namespace DOM
{
  class HTMLFrameElementImpl;
  class HTMLElementImpl;
  class MouseEventImpl;
};

namespace khtml
{
  struct ChildFrame;

class RenderFrameSet : public RenderContainer
{
  friend class DOM::HTMLFrameSetElementImpl;
public:
  RenderFrameSet( DOM::HTMLFrameSetElementImpl *frameSet );

  virtual ~RenderFrameSet();

  virtual const char *renderName() const { return "RenderFrameSet"; }
  virtual bool isFrameSet() const { return true; }

  virtual void layout();

  void positionFrames( );

  bool resizing() const { return m_resizing; }

  bool userResize( DOM::MouseEventImpl *evt );
  bool canResize( int _x, int _y);
  void setResizing(bool e);

  bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty,
                   HitTestAction hitTestAction);

    DOM::HTMLFrameSetElementImpl *element() const
    { return static_cast<DOM::HTMLFrameSetElementImpl*>(RenderObject::element()); }

#ifndef NDEBUG
  virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

private:
    int m_oldpos;
    int m_gridLen[2];
    int* m_gridDelta[2];
    int* m_gridLayout[2];

    bool *m_hSplitVar; // is this split variable?
    bool *m_vSplitVar;

    int m_hSplit;     // the split currently resized
    int m_vSplit;
    int m_hSplitPos;
    int m_vSplitPos;

    bool m_resizing;
    bool m_clientresizing;
};

class RenderPart : public RenderWidget
{
    Q_OBJECT
public:
    RenderPart(DOM::HTMLElementImpl* node);
    RenderPart::~RenderPart();
    
    virtual const char *renderName() const { return "RenderPart"; }

    virtual void setWidget( QWidget *widget );

    // FIXME: This should not be necessary.  Remove this once WebKit knows to properly schedule
    // layouts using WebCore when objects resize.
    void updateWidgetPosition();

    bool hasFallbackContent() const { return m_hasFallbackContent; }

public slots:
    virtual void slotViewCleared();

protected:
    bool m_hasFallbackContent;
};

class RenderFrame : public RenderPart
{
    Q_OBJECT
public:
    RenderFrame( DOM::HTMLFrameElementImpl *frame );

    virtual const char *renderName() const { return "RenderFrame"; }

    DOM::HTMLFrameElementImpl *element() const
    { return static_cast<DOM::HTMLFrameElementImpl*>(RenderObject::element()); }

public slots:
    void slotViewCleared();
};

// I can hardly call the class RenderObject ;-)
class RenderPartObject : public RenderPart
{
    Q_OBJECT
public:
    RenderPartObject( DOM::HTMLElementImpl * );

    virtual const char *renderName() const { return "RenderPartObject"; }

    virtual void layout( );
    virtual void updateWidget();

public slots:
    void slotViewCleared();
};

}

#endif
