/*
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
#ifndef render_replaced_h
#define render_replaced_h

#include "render_box.h"
#include <qobject.h>

namespace WebCore {

class FrameView;
class Position;
class Widget;

class RenderReplaced : public RenderBox
{
public:
    RenderReplaced(DOM::NodeImpl* node);

    virtual const char *renderName() const { return "RenderReplaced"; }

    virtual short lineHeight( bool firstLine, bool isRootLineBox=false ) const;
    virtual short baselinePosition( bool firstLine, bool isRootLineBox=false ) const;

    virtual void calcMinMaxWidth();

    bool shouldPaint(PaintInfo& i, int& _tx, int& _ty);
    virtual void paint(PaintInfo& i, int _tx, int _ty) = 0;

    virtual int intrinsicWidth() const { return m_intrinsicWidth; }
    virtual int intrinsicHeight() const { return m_intrinsicHeight; }

    void setIntrinsicWidth(int w) {  m_intrinsicWidth = w; }
    void setIntrinsicHeight(int h) { m_intrinsicHeight = h; }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    virtual VisiblePosition positionForCoordinates(int x, int y);
    
    virtual bool canBeSelectionLeaf() const { return true; }
    virtual SelectionState selectionState() const { return m_selectionState; }
    virtual void setSelectionState(SelectionState s);
    virtual IntRect selectionRect();
    bool isSelected();
    virtual Color selectionColor(QPainter *p) const;

protected:
    int m_intrinsicWidth;
    int m_intrinsicHeight;
    
    SelectionState m_selectionState : 3;
};


class RenderWidget : public QObject, public RenderReplaced
{
    Q_OBJECT
public:
    RenderWidget(DOM::NodeImpl* node);
    virtual ~RenderWidget();

    virtual void setStyle(RenderStyle *style);

    virtual void paint(PaintInfo& i, int tx, int ty);

    virtual bool isWidget() const { return true; };

    virtual void destroy();
    virtual void layout( );

    Widget *widget() const { return m_widget; }
    FrameView* view() const { return m_view; }

    RenderArena *ref() { ++m_refCount; return renderArena(); }
    void deref(RenderArena *arena);
    
    virtual void setSelectionState(SelectionState s);

    void sendConsumedMouseUp();
    virtual void updateWidgetPosition();

public slots:
    void slotWidgetDestructed();

protected:
    bool eventFilter(QObject* /*o*/, QEvent* e);
    void setQWidget(Widget *widget, bool deleteWidget = true);
    void resizeWidget( Widget *widget, int w, int h );

    bool m_deleteWidget;
    Widget *m_widget;
    FrameView* m_view;
    int m_refCount;
};

}

#endif
