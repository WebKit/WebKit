/*
 * Copyright 2007, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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

#include "Font.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreViewBridge.h"
#include "WebViewCore.h"

namespace WebCore {

Widget::Widget(PlatformWidget widget)
{
    init(widget);
}

Widget::~Widget()
{
    ASSERT(!parent());
    releasePlatformWidget();
}

IntRect Widget::frameRect() const
{
    // FIXME: use m_frame instead?
    if (!platformWidget())
        return IntRect(0, 0, 0, 0);
    return platformWidget()->getBounds();
}

void Widget::setFocus(bool focused)
{
    notImplemented();
}

void Widget::paint(GraphicsContext* ctx, const IntRect& r)
{
    // FIXME: in what case, will this be called for the top frame?
    if (!platformWidget())
        return;
    platformWidget()->draw(ctx, r);
}

void Widget::releasePlatformWidget()
{
    Release(platformWidget());
}

void Widget::retainPlatformWidget()
{
    Retain(platformWidget());
}

void Widget::setCursor(const Cursor& cursor)
{
    notImplemented();
}

void Widget::show()
{
    notImplemented(); 
}

void Widget::hide()
{
    notImplemented(); 
}

void Widget::setFrameRect(const IntRect& rect)
{
    // FIXME: set m_frame instead?
    // platformWidget() is 0 when called from Scrollbar
    if (!platformWidget())
        return;
    platformWidget()->setLocation(rect.x(), rect.y());
    platformWidget()->setSize(rect.width(), rect.height());
}

void Widget::setIsSelected(bool isSelected)
{
    notImplemented();
}

int Widget::screenWidth() const
{
    const Widget* widget = this;
    while (!widget->isFrameView()) {
        widget = widget->parent();
        if (!widget)
            break;
    }
    if (!widget)
        return 0;
    android::WebViewCore* core = android::WebViewCore::getWebViewCore(
        static_cast<const ScrollView*>(widget));
    if (!core)
        return 0;
    return core->screenWidth();
}

} // WebCore namepsace
