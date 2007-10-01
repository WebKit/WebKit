/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 George Stiakos <staikos@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Cursor.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "RenderObject.h"
#include "ScrollView.h"
#include "Widget.h"
#include "WidgetClient.h"
#include "PlatformScrollBar.h"
#include "NotImplemented.h"

#include "qwebframe.h"
#include "qwebpage.h"
#include <QPainter>

#include <QDebug>

namespace WebCore {

struct WidgetPrivate
{
    WidgetPrivate() : m_client(0), m_widget(0), m_webFrame(0), m_parentScrollView(0) { }
    ~WidgetPrivate() { delete m_webFrame; }

    void setGeometry(const QRect &rect) {
        if (m_widget)
            m_widget->setGeometry(rect);
        m_geometry = rect;
    }
    QRect geometry() const {
        if (m_widget)
            m_widget->geometry();
        return m_geometry;
    }

    WidgetClient* m_client;

    bool enabled;
    QRect m_geometry;
    QWidget *m_widget; //for plugins
    QWebFrame *m_webFrame;
    ScrollView *m_parentScrollView;
};

Widget::Widget()
    : data(new WidgetPrivate())
{
}

Widget::~Widget()
{
    delete data;
    data = 0;
}

void Widget::setClient(WidgetClient* c)
{
    data->m_client = c;
}

WidgetClient* Widget::client() const
{
    return data->m_client;
}

IntRect Widget::frameGeometry() const
{
    return data->geometry();
}

void Widget::setFocus()
{
}

void Widget::setCursor(const Cursor& cursor)
{
#ifndef QT_NO_CURSOR
    if (qwidget())
        qwidget()->setCursor(cursor.impl());
#endif
}

void Widget::show()
{
    if (data->m_widget)
        data->m_widget->show();
    else
        notImplemented();
}

void Widget::hide()
{
    if (data->m_widget)
        data->m_widget->hide();
    else
        notImplemented();
}

QWebFrame* Widget::qwebframe() const
{
    return data->m_webFrame;
}

void Widget::setQWebFrame(QWebFrame* webFrame)
{
    data->m_webFrame = webFrame;
}

QWidget* Widget::qwidget() const
{
    if (data->m_widget)
        return data->m_widget;

    if (data->m_webFrame)
        return data->m_webFrame->page();

    return 0;
}

void Widget::setQWidget(QWidget *widget)
{
    data->m_widget = widget;
}

void Widget::setFrameGeometry(const IntRect& r)
{
    data->setGeometry(r);
}

void Widget::paint(GraphicsContext *, const IntRect &rect)
{
}

bool Widget::isEnabled() const
{
    if (data->m_widget)
        return data->m_widget->isEnabled();
    return data->enabled;
}

void Widget::setEnabled(bool e)
{
    if (data->m_widget)
        data->m_widget->setEnabled(e);

    if (e != data->enabled) {
        data->enabled = e;
        invalidate();
    }
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

void Widget::invalidate()
{
    invalidateRect(IntRect(0, 0, width(), height()));
}

void Widget::invalidateRect(const IntRect& r)
{
    if (data->m_widget) //plugins
        return data->m_widget->update(r);

    IntRect windowRect = convertToContainingWindow(r);

    // Get our clip rect and intersect with it to ensure we don't invalidate too much.
    IntRect clipRect = windowClipRect();
    windowRect.intersect(clipRect);

    QWidget *canvas = qwidget(); //regular frameview
    if (!canvas && parent())
        canvas = parent()->qwidget(); //scrollbars

    if (!canvas)  // not visible anymore
        return;

    bool painting = canvas->testAttribute(Qt::WA_WState_InPaintEvent);
    if (painting) {
        QWebPage *page = qobject_cast<QWebPage*>(canvas);
        QPainter p(page);
        page->mainFrame()->render(&p, windowRect);
    } else {
        canvas->update(windowRect);
    }
}

void Widget::removeFromParent()
{
    if (parent())
        parent()->removeChild(this);
}

void Widget::setParent(ScrollView* sv)
{
    data->m_parentScrollView = sv;
}

ScrollView* Widget::parent() const
{
    return data->m_parentScrollView;
}

void Widget::geometryChanged() const
{
}

IntPoint Widget::convertToContainingWindow(const IntPoint& point) const
{
    IntPoint windowPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent()) {
        IntPoint oldPoint = windowPoint;
        windowPoint = parentWidget->convertChildToSelf(childWidget, oldPoint);
    }
    return windowPoint;
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& point) const
{
    IntPoint widgetPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent()) {
        IntPoint oldPoint = widgetPoint;
        widgetPoint = parentWidget->convertSelfToChild(childWidget, oldPoint);
    }
    return widgetPoint;
}

IntRect Widget::convertToContainingWindow(const IntRect& rect) const
{
    IntRect convertedRect = rect;
    convertedRect.setLocation(convertToContainingWindow(convertedRect.location()));
    return convertedRect;
}

IntPoint Widget::convertChildToSelf(const Widget* child, const IntPoint& point) const
{
    return IntPoint(point.x() + child->x(), point.y() + child->y());
}

IntPoint Widget::convertSelfToChild(const Widget* child, const IntPoint& point) const
{
    return IntPoint(point.x() - child->x(), point.y() - child->y());
}

QWidget *Widget::containingWindow() const
{
    return qwidget();
}

}

// vim: ts=4 sw=4 et
