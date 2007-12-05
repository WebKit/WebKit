/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "Document.h"
#include "Element.h"
#include "GraphicsContext.h"
#include "FrameWin.h"
#include "IntRect.h"
#include "FrameView.h"
#include "WidgetClient.h"
#include <winsock2.h>
#include <windows.h>

namespace WebCore {

class WidgetPrivate
{
public:
    WidgetClient* client;
    ScrollView* parent;
    HWND containingWindow;
    IntRect frameRect;
    bool enabled;
    Widget* capturingChild;
    bool suppressInvalidation;
};

Widget::Widget()
    : data(new WidgetPrivate)
{
    data->client = 0;
    data->parent = 0;
    data->containingWindow = 0;
    data->enabled = true;
    data->capturingChild = 0;
    data->suppressInvalidation = false;
}

Widget::~Widget() 
{
    ASSERT(!parent());
    delete data;
}

void Widget::setContainingWindow(HWND containingWindow)
{
    data->containingWindow = containingWindow;
}

HWND Widget::containingWindow() const
{
    return data->containingWindow;
}

void Widget::setClient(WidgetClient* c)
{
    data->client = c;
}

WidgetClient* Widget::client() const
{
    return data->client;
}

IntRect Widget::frameGeometry() const
{
    return data->frameRect;
}

void Widget::setFrameGeometry(const IntRect &rect)
{
    data->frameRect = rect;
}

void Widget::setParent(ScrollView* v)
{
    if (!v || !v->isAttachedToWindow())
        detachFromWindow();
    data->parent = v;
    if (v && v->isAttachedToWindow())
        attachToWindow();
}

ScrollView* Widget::parent() const
{
    return data->parent;
}

void Widget::removeFromParent()
{
    if (parent())
        parent()->removeChild(this);
}

void Widget::show()
{
}

void Widget::hide()
{
}

HCURSOR lastSetCursor = 0;
bool ignoreNextSetCursor = false;

void Widget::setCursor(const Cursor& cursor)
{
    // This is set by PluginViewWin so it can ignore set setCursor call made by
    // EventHandler.cpp.
    if (ignoreNextSetCursor) {
        ignoreNextSetCursor = false;
        return;
    }

    if (HCURSOR c = cursor.impl()->nativeCursor()) {
        lastSetCursor = c;
        SetCursor(c);
    }
}

IntPoint Widget::convertToContainingWindow(const IntPoint& point) const
{
    IntPoint windowPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent())
        windowPoint = parentWidget->convertChildToSelf(childWidget, windowPoint);
    return windowPoint;
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& point) const
{
    IntPoint widgetPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent())
        widgetPoint = parentWidget->convertSelfToChild(childWidget, widgetPoint);
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

void Widget::paint(GraphicsContext*, const IntRect&)
{
}

bool Widget::isEnabled() const
{
    return data->enabled;
}

void Widget::setEnabled(bool e)
{
    if (e != data->enabled) {
        data->enabled = e;
        invalidate();
    }
}

bool Widget::suppressInvalidation() const
{
    return data->suppressInvalidation;
}

void Widget::setSuppressInvalidation(bool suppress)
{
    data->suppressInvalidation = suppress;
}

void Widget::invalidate()
{
    invalidateRect(IntRect(0, 0, width(), height()));
}

void Widget::invalidateRect(const IntRect& r)
{
    if (data->suppressInvalidation)
        return;

    if (!parent()) {
        RECT rect = r;
        ::InvalidateRect(containingWindow(), &rect, false);
        if (isFrameView())
            static_cast<FrameView*>(this)->addToDirtyRegion(r);
        return;
    }

    // Get the root widget.
    ScrollView* outermostView = parent();
    while (outermostView && outermostView->parent())
        outermostView = outermostView->parent();
    if (!outermostView)
        return;

    IntRect windowRect = convertToContainingWindow(r);

    // Get our clip rect and intersect with it to ensure we don't invalidate too much.
    IntRect clipRect = windowClipRect();
    windowRect.intersect(clipRect);

    RECT rect = windowRect;
    ::InvalidateRect(containingWindow(), &rect, false);
    outermostView->addToDirtyRegion(windowRect);
}

void Widget::setFocus()
{
}

void Widget::setIsSelected(bool)
{
}

} // namespace WebCore
