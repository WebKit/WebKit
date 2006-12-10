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

#include <QWidget>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

struct WidgetPrivate
{
    WidgetPrivate() : m_parent(0), m_widget(0) { }
    ~WidgetPrivate() { delete m_widget; }

    QWidget* m_parent;
    QWidget* m_widget;
    Font m_font;
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
    notImplemented();
}

WidgetClient* Widget::client() const
{
    notImplemented();
    return 0;
}

IntRect Widget::frameGeometry() const
{
    if (!data->m_widget)
        return IntRect();

    return data->m_widget->geometry();
}

bool Widget::hasFocus() const
{
    if (!data->m_widget)
        return false;

    return data->m_widget->hasFocus();
}

void Widget::setFocus()
{
    if (data->m_widget)
        data->m_widget->setFocus();
}

void Widget::clearFocus()
{
    if (data->m_widget)
        data->m_widget->clearFocus();
}

const Font& Widget::font() const
{
    return data->m_font;
}

void Widget::setFont(const Font& font)
{
    if (data->m_widget)
        data->m_widget->setFont(font);
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
    delete data->m_widget;
    data->m_widget = child;
}

QWidget* Widget::qwidget()
{
    return data->m_widget;
}

void Widget::setParentWidget(QWidget* parent)
{
    data->m_parent = parent;
}

QWidget* Widget::parentWidget() const
{
    return data->m_parent;
}

void Widget::setFrameGeometry(const IntRect& r)
{
    if (!data->m_widget)
        return;

    data->m_widget->setGeometry(r);
}

GraphicsContext* Widget::lockDrawingFocus()
{
    notImplemented();
    return 0;
}

void Widget::unlockDrawingFocus(GraphicsContext*)
{
    notImplemented();
}

void Widget::paint(GraphicsContext*, IntRect const&)
{
    notImplemented();
}

void Widget::enableFlushDrawing()
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

void Widget::disableFlushDrawing()
{
    notImplemented();
}

void Widget::setEnabled(bool en)
{
    if (data->m_widget)
        data->m_widget->setEnabled(en);
}

Widget::FocusPolicy Widget::focusPolicy() const
{
    if (!data->m_widget)
        return NoFocus;

    switch (data->m_widget->focusPolicy())
    {
        case Qt::TabFocus:
            return TabFocus;
        case Qt::ClickFocus:
            return ClickFocus;
        case Qt::StrongFocus:
            return StrongFocus;
        case Qt::WheelFocus:
            return WheelFocus;
        case Qt::NoFocus:
            return NoFocus;    
    }

    return NoFocus;
}

void Widget::invalidate()
{
    notImplemented();
}

void Widget::invalidateRect(const IntRect& r)
{
    notImplemented();
}

void Widget::removeFromParent()
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
