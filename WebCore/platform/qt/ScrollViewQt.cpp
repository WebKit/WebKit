/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
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
#include "ScrollView.h"

#include "FrameView.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "IntPoint.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "NotImplemented.h"
#include "Frame.h"
#include "Page.h"
#include "GraphicsContext.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"

#include <QDebug>
#include <QWidget>
#include <QPainter>
#include <QApplication>
#include <QPalette>
#include <QStyleOption>

#ifdef Q_WS_MAC
#include <Carbon/Carbon.h>
#endif

#include "qwebframe.h"
#include "qwebpage.h"

// #define DEBUG_SCROLLVIEW

namespace WebCore {

class ScrollView::ScrollViewPrivate  {
public:
    ScrollViewPrivate(ScrollView* view)
      : m_view(view)
      , m_platformWidgets(0)
      , m_inUpdateScrollbars(false)
    {
    }

    ~ScrollViewPrivate()
    {
    }

    void scrollBackingStore(const IntSize& scrollDelta);

    ScrollView* m_view;
    int  m_platformWidgets;
};

void ScrollView::ScrollViewPrivate::scrollBackingStore(const IntSize& scrollDelta)
{
    // Since scrolling is double buffered, we will be blitting the scroll view's intersection
    // with the clip rect every time to keep it smooth.
    IntRect clipRect = m_view->windowClipRect();
    IntRect scrollViewRect = m_view->convertToContainingWindow(IntRect(0, 0, m_view->visibleWidth(), m_view->visibleHeight()));

    IntRect updateRect = clipRect;
    updateRect.intersect(scrollViewRect);

    if (m_view->canBlitOnScroll() && !m_view->root()->hasNativeWidgets()) {
       m_view->scrollBackingStore(-scrollDelta.width(), -scrollDelta.height(),
                                  scrollViewRect, clipRect);
    } else  {
       // We need to go ahead and repaint the entire backing store.
       m_view->addToDirtyRegion(updateRect);
       m_view->updateBackingStore();
    }

    m_view->frameRectsChanged();
}

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate(this))
{
    init();
}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::scrollContents(const IntSize& size)
{
    m_data->scrollBackingStore(size);
}

void ScrollView::platformAddChild(Widget* child)
{
    root()->incrementNativeWidgetCount();
}

void ScrollView::platformRemoveChild(Widget* child)
{
    child->hide();
    root()->decrementNativeWidgetCount();
}

void ScrollView::addToDirtyRegion(const IntRect& containingWindowRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->addToDirtyRegion(containingWindowRect);
}

void ScrollView::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->scrollBackingStore(dx, dy, scrollViewRect, clipRect);
}

void ScrollView::updateBackingStore()
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->updateBackingStore();
}

void ScrollView::incrementNativeWidgetCount()
{
    ++m_data->m_platformWidgets;
}

void ScrollView::decrementNativeWidgetCount()
{
    --m_data->m_platformWidgets;
}

bool ScrollView::hasNativeWidgets() const
{
    return m_data->m_platformWidgets != 0;
}

}

// vim: ts=4 sw=4 et
