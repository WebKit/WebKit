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
 * $Id$
 */
#ifndef render_replaced_h
#define render_replaced_h

#include "render_box.h"

#include <qobject.h>
class KHTMLView;
class QWidget;

namespace khtml {

class RenderReplaced : public RenderBox
{
public:
    RenderReplaced(DOM::NodeImpl* node);
    virtual ~RenderReplaced() {}

    virtual const char *renderName() const { return "RenderReplaced"; }

    virtual bool isRendered() const { return true; }

    virtual short lineHeight( bool firstLine) const;
    virtual short baselinePosition( bool firstLine ) const;

    virtual void calcMinMaxWidth();

    virtual void print( QPainter *, int x, int y, int w, int h,
                        int tx, int ty);
    virtual void printObject(QPainter *p, int x, int y, int w, int h, int tx, int ty) = 0;

    virtual short intrinsicWidth() const { return m_intrinsicWidth; }
    virtual int intrinsicHeight() const { return m_intrinsicHeight; }

    void setIntrinsicWidth(int w) {  m_intrinsicWidth = w; }
    void setIntrinsicHeight(int h) { m_intrinsicHeight = h; }

    virtual void position(int x, int y, int from, int len, int width, bool reverse, bool firstLine);

private:
    short m_intrinsicWidth;
    short m_intrinsicHeight;
};


class RenderWidget : public QObject, public RenderReplaced, public DOM::DomShared
{
    Q_OBJECT
public:
    RenderWidget(DOM::NodeImpl* node);
    virtual ~RenderWidget();

    virtual void setStyle(RenderStyle *style);

    virtual void printObject(QPainter *p, int x, int y, int w, int h, int tx, int ty);

    virtual bool isWidget() const { return true; };

    virtual void detach();
    virtual void layout( );

    virtual bool eventFilter(QObject *o, QEvent *e);

    virtual void handleDOMEvent(DOM::EventImpl *evt);

    bool sendWidgetEvent(QEvent *event);
    QWidget *widget() const { return m_widget; }
    KHTMLView* view() const { return m_view; }

public slots:
    void slotWidgetDestructed();

protected:
    void setQWidget(QWidget *widget);
    QWidget *m_widget;
    KHTMLView* m_view;

    bool m_paintingSelf : 1;
    bool m_ignorePaintEvents : 1;
    bool m_widgetShown : 1;
};

};

#endif
