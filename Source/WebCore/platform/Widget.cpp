/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "FrameView.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include <wtf/Assertions.h>

namespace WebCore {

void Widget::init(PlatformWidget widget)
{
    m_selfVisible = false;
    m_parentVisible = false;
    m_widget = widget;
}

ScrollView* Widget::parent() const
{
    return m_parent.get();
}

RefPtr<ScrollView> Widget::protectedParent() const
{
    return m_parent.get();
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

FrameView* Widget::root() const
{
    const Widget* top = this;
    while (top->parent())
        top = top->parent();
    if (auto* frameView = dynamicDowncast<FrameView>(top))
        return const_cast<FrameView*>(frameView);
    return nullptr;
}
    
void Widget::removeFromParent()
{
    if (parent())
        parent()->removeChild(*this);
}

void Widget::setCursor(const Cursor& cursor)
{
    if (RefPtr view = root())
        view->hostWindow()->setCursor(cursor);
}

// MARK: -

IntPoint Widget::convertToRootView(IntPoint localPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentPoint = convertToContainingView(localPoint);
        return parentScrollView->convertToRootView(parentPoint);
    }
    return localPoint;
}

FloatPoint Widget::convertToRootView(FloatPoint localPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentPoint = convertToContainingView(localPoint);
        return parentScrollView->convertToRootView(parentPoint);
    }
    return localPoint;
}

DoublePoint Widget::convertToRootView(DoublePoint localPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentPoint = convertToContainingView(localPoint);
        return parentScrollView->convertToRootView(parentPoint);
    }
    return localPoint;
}

IntRect Widget::convertToRootView(const IntRect& localRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        IntRect parentRect = convertToContainingView(localRect);
        return parentScrollView->convertToRootView(parentRect);
    }
    return localRect;
}

FloatRect Widget::convertToRootView(const FloatRect& localRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        FloatRect parentRect = convertToContainingView(localRect);
        return parentScrollView->convertToRootView(parentRect);
    }
    return localRect;
}

// MARK: -

IntPoint Widget::convertFromRootView(IntPoint rootPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        IntPoint parentPoint = parentScrollView->convertFromRootView(rootPoint);
        return convertFromContainingView(parentPoint);
    }
    return rootPoint;
}

FloatPoint Widget::convertFromRootView(FloatPoint rootPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        FloatPoint parentPoint = parentScrollView->convertFromRootView(rootPoint);
        return convertFromContainingView(parentPoint);
    }
    return rootPoint;
}

DoublePoint Widget::convertFromRootView(DoublePoint rootPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        DoublePoint parentPoint = parentScrollView->convertFromRootView(rootPoint);
        return convertFromContainingView(parentPoint);
    }
    return rootPoint;
}

IntRect Widget::convertFromRootView(const IntRect& rootRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        IntRect parentRect = parentScrollView->convertFromRootView(rootRect);
        return convertFromContainingView(parentRect);
    }
    return rootRect;
}

FloatPoint Widget::convertFromContainingWindow(FloatPoint windowPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentPoint = parentScrollView->convertFromContainingWindow(windowPoint);
        return convertFromContainingView(parentPoint);
    }
    return convertFromContainingWindowToRoot(this, windowPoint);
}

DoublePoint Widget::convertFromContainingWindow(DoublePoint windowPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentPoint = parentScrollView->convertFromContainingWindow(windowPoint);
        return convertFromContainingView(parentPoint);
    }
    return convertFromContainingWindowToRoot(this, windowPoint);
}

FloatRect Widget::convertFromRootView(const FloatRect& rootRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        FloatRect parentRect = parentScrollView->convertFromRootView(rootRect);
        return convertFromContainingView(parentRect);
    }
    return rootRect;
}

// MARK: -

IntPoint Widget::convertToContainingWindow(IntPoint localPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentPoint = convertToContainingView(localPoint);
        return parentScrollView->convertToContainingWindow(parentPoint);
    }
    return convertFromRootToContainingWindow(this, localPoint);
}

FloatPoint Widget::convertToContainingWindow(FloatPoint localPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentPoint = convertToContainingView(localPoint);
        return parentScrollView->convertToContainingWindow(parentPoint);
    }
    return convertFromRootToContainingWindow(this, localPoint);
}

IntRect Widget::convertToContainingWindow(const IntRect& localRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentRect = convertToContainingView(localRect);
        return parentScrollView->convertToContainingWindow(parentRect);
    }
    return convertFromRootToContainingWindow(this, localRect);
}

FloatRect Widget::convertToContainingWindow(const FloatRect& localRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentRect = convertToContainingView(localRect);
        return parentScrollView->convertToContainingWindow(parentRect);
    }
    return convertFromRootToContainingWindow(this, localRect);
}

// MARK: -

IntPoint Widget::convertFromContainingWindow(IntPoint windowPoint) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentPoint = parentScrollView->convertFromContainingWindow(windowPoint);
        return convertFromContainingView(parentPoint);
    }
    return convertFromContainingWindowToRoot(this, windowPoint);
}

IntRect Widget::convertFromContainingWindow(const IntRect& windowRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentRect = parentScrollView->convertFromContainingWindow(windowRect);
        return convertFromContainingView(parentRect);
    }
    return convertFromContainingWindowToRoot(this, windowRect);
}

FloatRect Widget::convertFromContainingWindow(const FloatRect& windowRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentRect = parentScrollView->convertFromContainingWindow(windowRect);
        return convertFromContainingView(parentRect);
    }
    return convertFromContainingWindowToRoot(this, windowRect);
}

// MARK: -

IntPoint Widget::convertToContainingView(IntPoint localPoint) const
{
    if (const RefPtr parentScrollView = parent())
        return parentScrollView->convertChildToSelf(this, localPoint);

    return localPoint;
}

FloatPoint Widget::convertToContainingView(FloatPoint localPoint) const
{
    if (const RefPtr parentScrollView = parent())
        return parentScrollView->convertChildToSelf(this, localPoint);

    return localPoint;
}

DoublePoint Widget::convertToContainingView(DoublePoint localPoint) const
{
    if (const RefPtr parentScrollView = parent())
        return parentScrollView->convertChildToSelf(this, localPoint);

    return localPoint;
}

IntRect Widget::convertToContainingView(const IntRect& localRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentRect = localRect;
        parentRect.setLocation(parentScrollView->convertChildToSelf(this, localRect.location()));
        return parentRect;
    }
    return localRect;
}

FloatRect Widget::convertToContainingView(const FloatRect& localRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto parentRect = localRect;
        parentRect.setLocation(parentScrollView->convertChildToSelf(this, localRect.location()));
        return parentRect;
    }
    return localRect;
}

// MARK: -

IntPoint Widget::convertFromContainingView(IntPoint parentPoint) const
{
    if (const RefPtr parentScrollView = parent())
        return parentScrollView->convertSelfToChild(this, parentPoint);

    return parentPoint;
}


FloatPoint Widget::convertFromContainingView(FloatPoint parentPoint) const
{
    if (const RefPtr parentScrollView = parent())
        return parentScrollView->convertSelfToChild(this, parentPoint);

    return parentPoint;
}

DoublePoint Widget::convertFromContainingView(DoublePoint parentPoint) const
{
    if (const RefPtr parentScrollView = parent())
        return parentScrollView->convertSelfToChild(this, parentPoint);

    return parentPoint;
}

IntRect Widget::convertFromContainingView(const IntRect& parentRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto localRect = parentRect;
        localRect.setLocation(parentScrollView->convertSelfToChild(this, localRect.location()));
        return localRect;
    }

    return parentRect;
}

FloatRect Widget::convertFromContainingView(const FloatRect& parentRect) const
{
    if (const RefPtr parentScrollView = parent()) {
        auto localRect = parentRect;
        localRect.setLocation(parentScrollView->convertSelfToChild(this, localRect.location()));
        return localRect;
    }
    return parentRect;
}

// MARK: -

#if !PLATFORM(COCOA)

Widget::Widget(PlatformWidget widget)
{
    init(widget);
}

Widget::~Widget()
{
    ASSERT(!parent());
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;
}

IntRect Widget::frameRect() const
{
    return m_frame;
}

void Widget::show()
{
}

void Widget::hide()
{
}

void Widget::setFocus(bool)
{
}

void Widget::setIsSelected(bool)
{
}

void Widget::paint(GraphicsContext&, const IntRect&, SecurityOriginPaintPolicy, RegionContext*)
{
}

// MARK: -

IntPoint Widget::convertFromRootToContainingWindow(const Widget*, IntPoint point)
{
    return point;
}

FloatPoint Widget::convertFromRootToContainingWindow(const Widget*, FloatPoint point)
{
    return point;
}

IntRect Widget::convertFromRootToContainingWindow(const Widget*, const IntRect& rect)
{
    return rect;
}

FloatRect Widget::convertFromRootToContainingWindow(const Widget*, const FloatRect& rect)
{
    return rect;
}

// MARK: -

IntPoint Widget::convertFromContainingWindowToRoot(const Widget*, IntPoint point)
{
    return point;
}

FloatPoint Widget::convertFromContainingWindowToRoot(const Widget*, FloatPoint point)
{
    return point;
}

DoublePoint Widget::convertFromContainingWindowToRoot(const Widget*, DoublePoint point)
{
    return point;
}

IntRect Widget::convertFromContainingWindowToRoot(const Widget*, const IntRect& rect)
{
    return rect;
}

FloatRect Widget::convertFromContainingWindowToRoot(const Widget*, const FloatRect& rect)
{
    return rect;
}

#endif // !PLATFORM(COCOA)

} // namespace WebCore
