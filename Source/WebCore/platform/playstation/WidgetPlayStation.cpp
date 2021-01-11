/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Widget.h"

#include "FrameView.h"
#include "HostWindow.h"
#include "NotImplemented.h"

namespace WebCore {

Widget::~Widget()
{
    ASSERT(!parent());
    notImplemented();
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;
    notImplemented();
}

void Widget::paint(GraphicsContext&, const IntRect&, SecurityOriginPaintPolicy, EventRegionContext*)
{
    notImplemented();
}

void Widget::setFocus(bool)
{
    notImplemented();
}

void Widget::setCursor(const Cursor& cursor)
{
    FrameView* view = root();
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

void Widget::setIsSelected(bool)
{
    notImplemented();
}

} // namespace WebCore
