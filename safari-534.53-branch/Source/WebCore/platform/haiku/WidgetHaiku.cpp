/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
#include "Widget.h"

#include "Cursor.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include <View.h>

namespace WebCore {

class AutoPlatformWidgetLocker {
public:
    AutoPlatformWidgetLocker(PlatformWidget widget)
        : m_widget(widget)
    {
        if (!m_widget || m_widget->LockLooperWithTimeout(5000) != B_OK)
            m_widget = 0;
    }

    ~AutoPlatformWidgetLocker()
    {
        if (m_widget)
            m_widget->UnlockLooper();
    }

    bool isLocked() const
    {
        return m_widget;
    }

private:
    PlatformWidget m_widget;
};

Widget::Widget(PlatformWidget widget)
    : m_topLevelPlatformWidget(0)
{
    init(widget);
}

Widget::~Widget()
{
}

IntRect Widget::frameRect() const
{
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;
}

void Widget::setFocus(bool focused)
{
    if (focused) {
        AutoPlatformWidgetLocker locker(topLevelPlatformWidget());
        if (locker.isLocked())
            topLevelPlatformWidget()->MakeFocus();
    }
}

void Widget::setCursor(const Cursor& cursor)
{
    AutoPlatformWidgetLocker locker(topLevelPlatformWidget());
    if (locker.isLocked())
        topLevelPlatformWidget()->SetViewCursor(cursor.impl());
}

void Widget::show()
{
    setSelfVisible(true);
    if (!isParentVisible())
        return;

    AutoPlatformWidgetLocker locker(platformWidget());
    if (locker.isLocked() && platformWidget()->IsHidden())
        platformWidget()->Show();
}

void Widget::hide()
{
    setSelfVisible(false);
    if (!isParentVisible())
        return;

    AutoPlatformWidgetLocker locker(platformWidget());
    if (locker.isLocked() && !platformWidget()->IsHidden())
        platformWidget()->Hide();
}

void Widget::paint(GraphicsContext*, IntRect const&)
{
    notImplemented();
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

} // namespace WebCore

