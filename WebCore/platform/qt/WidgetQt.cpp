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

#include "Font.h"
#include "Widget.h"
#include "Cursor.h"
#include "IntRect.h"
#include "RenderObject.h"
#include "GraphicsContext.h"
#include "WidgetClient.h"

#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QWidget>

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

struct WidgetPrivate
{
    WidgetPrivate() : m_client(0), m_widget(0), m_scrollArea(0) { }
    ~WidgetPrivate() { delete m_widget; }

    QWidget* canvas() const {
        return m_scrollArea ? m_scrollArea->viewport() : m_widget;
    }

    QAbstractScrollArea* parentScroll() const {
        QObject *parent = m_widget->parent();
        while (parent && !qobject_cast<QAbstractScrollArea*>(parent)) {
            parent = parent->parent();
        }
        if (parent)
            return static_cast<QAbstractScrollArea*>(parent);
        return 0;
    }
    void setGeometry(const QRect &rect) {
        QAbstractScrollArea *mapper = parentScroll();
        QRect r = rect;
        if (mapper)
            r = r.translated(-mapper->horizontalScrollBar()->value(),
                             -mapper->verticalScrollBar()->value());
        m_widget->setGeometry(r);
    }
    QRect geometry() const {
        QAbstractScrollArea *mapper = parentScroll();
        QRect r = m_widget->geometry();
        if (mapper)
            r = r.translated(-mapper->horizontalScrollBar()->value(),
                             -mapper->verticalScrollBar()->value());
        return r;
    }

    WidgetClient* m_client;

    QWidget*     m_widget;
    QAbstractScrollArea* m_scrollArea;
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
    if (!data->m_widget)
        return IntRect();
    return data->geometry();
}

void Widget::setFocus()
{
    if (data->canvas())
        data->canvas()->setFocus();
}

void Widget::setCursor(const Cursor& cursor)
{
#ifndef QT_NO_CURSOR
    if (data->m_widget)
        data->m_widget->setCursor(cursor.impl());
#endif
}

void Widget::show()
{
    if (data->m_widget)
        data->m_widget->show();
}

void Widget::hide()
{
    if (data->m_widget)
        data->m_widget->hide();
}

void Widget::setQWidget(QWidget* child)
{
    data->m_widget = child;
    data->m_scrollArea = qobject_cast<QAbstractScrollArea*>(child);

}

QWidget* Widget::qwidget() const
{
    return data->m_widget;
}

QWidget* Widget::canvas() const
{
    return data->canvas();
}

void Widget::setFrameGeometry(const IntRect& r)
{
    if (!data->m_widget)
        return;
    data->setGeometry(r);
}

void Widget::paint(GraphicsContext *, const IntRect &rect)
{
    notImplemented();
}

bool Widget::isEnabled() const
{
    if (!data->m_widget)
        return false;

    return data->m_widget->isEnabled();
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

void Widget::setEnabled(bool en)
{
    if (data->m_widget)
        data->m_widget->setEnabled(en);
}

void Widget::invalidate()
{
    if (data->m_widget)
        data->m_widget->update();
}

void Widget::invalidateRect(const IntRect& r)
{
    if (data->m_widget)
        data->m_widget->update(r);
}

void Widget::removeFromParent()
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
