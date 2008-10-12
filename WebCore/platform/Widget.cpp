/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "IntRect.h"
#include "ScrollView.h"

#include <wtf/Assertions.h>

namespace WebCore {

void Widget::init(PlatformWidget widget)
{
    m_parent = 0;
    m_selfVisible = false;
    m_parentVisible = false;
    m_widget = widget;
    if (m_widget)
        retainPlatformWidget();
}

void Widget::setParent(ScrollView* view)
{
    ASSERT(!view || !m_parent);
    if (!view || !view->isVisible())
        setParentVisible(false);
    m_parent = view;
    if (view && view->isVisible())
        setParentVisible(true);
}

ScrollView* Widget::root() const
{
    const Widget* top = this;
    while (top->parent())
        top = top->parent();
    if (top->isFrameView())
        return const_cast<ScrollView*>(static_cast<const ScrollView*>(top));
    return 0;
}
    
void Widget::removeFromParent()
{
    if (parent())
        parent()->removeChild(this);
}

#if !PLATFORM(MAC)

IntRect Widget::convertToContainingWindow(const IntRect& rect) const
{
    IntRect convertedRect = rect;
    convertedRect.setLocation(convertToContainingWindow(convertedRect.location()));
    return convertedRect;
}

IntPoint Widget::convertToContainingWindow(const IntPoint& point) const
{
    IntPoint windowPoint = point;
    const Widget* childWidget = this;
    for (const ScrollView* parentScrollView = parent();
         parentScrollView;
         childWidget = parentScrollView, parentScrollView = parentScrollView->parent())
        windowPoint = parentScrollView->convertChildToSelf(childWidget, windowPoint);
    return windowPoint;
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& point) const
{
    IntPoint widgetPoint = point;
    const Widget* childWidget = this;
    for (const ScrollView* parentScrollView = parent();
         parentScrollView;
         childWidget = parentScrollView, parentScrollView = parentScrollView->parent())
        widgetPoint = parentScrollView->convertSelfToChild(childWidget, widgetPoint);
    return widgetPoint;
}

IntRect Widget::convertFromContainingWindow(const IntRect& rect) const
{
    IntRect result = rect;
    result.setLocation(convertFromContainingWindow(rect.location()));
    return result;
}
#endif

#if !PLATFORM(MAC) && !PLATFORM(GTK)
void Widget::releasePlatformWidget()
{
}

void Widget::retainPlatformWidget()
{
}
#endif

}
