/*
 * Copyright (C) 2012-2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Widget.h"

#include "ChromeClient.h"
#include "Cursor.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Page.h"

namespace WebCore {

Widget::Widget(PlatformWidget widget)
    : m_parent(0)
    , m_widget(0)
    , m_selfVisible(false)
    , m_parentVisible(false)
    , m_frame(0, 0, 0, 0)
{
    init(widget);
}

Widget::~Widget()
{
    ASSERT(!parent());
}

IntRect Widget::frameRect() const
{
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;
    Widget::frameRectsChanged();
}

void Widget::setFocus(bool)
{
}

void Widget::setCursor(const Cursor& cursor)
{
    ScrollView* view = root();
    if (!view)
        return;
    view->hostWindow()->setCursor(cursor);
}

void Widget::show()
{
    notImplemented();
}

void Widget::hide()
{
    notImplemented();
}

void Widget::paint(GraphicsContext*, const IntRect&)
{
    notImplemented();
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

}
