/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Brent Fulgham
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

#if !USE(SAFARI_THEME)

#include "PlatformScrollBar.h"

#include "EventHandler.h"
#include "FrameView.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "PlatformMouseEvent.h"
#include "SoftLinking.h"
#include "RenderThemeWin.h"

namespace WebCore {

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
    : Scrollbar(client, orientation, size)
{
}

PlatformScrollbar::~PlatformScrollbar()
{
}

void PlatformScrollbar::setFrameGeometry(const IntRect& rect)
{
    // Get our window resizer rect and see if we overlap.  Adjust to avoid the overlap
    // if necessary.
    IntRect adjustedRect(rect);
    if (parent() && parent()->isFrameView()) {
        bool overlapsResizer = false;
        FrameView* view = static_cast<FrameView*>(parent());
        IntRect resizerRect = view->windowResizerRect();
        resizerRect.setLocation(view->convertFromContainingWindow(resizerRect.location()));
        if (rect.intersects(resizerRect)) {
            if (orientation() == HorizontalScrollbar) {
                int overlap = rect.right() - resizerRect.x();
                if (overlap > 0 && resizerRect.right() >= rect.right()) {
                    adjustedRect.setWidth(rect.width() - overlap);
                    overlapsResizer = true;
                }
            } else {
                int overlap = rect.bottom() - resizerRect.y();
                if (overlap > 0 && resizerRect.bottom() >= rect.bottom()) {
                    adjustedRect.setHeight(rect.height() - overlap);
                    overlapsResizer = true;
                }
            }
        }

        if (overlapsResizer != m_overlapsResizer) {
            m_overlapsResizer = overlapsResizer;
            view->adjustOverlappingScrollbarCount(m_overlapsResizer ? 1 : -1);
        }
    }

    Widget::setFrameGeometry(adjustedRect);
}

void PlatformScrollbar::setParent(ScrollView* parentView)
{
    if (!parentView && m_overlapsResizer && parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->adjustOverlappingScrollbarCount(-1);
    Widget::setParent(parentView);
}

IntRect PlatformScrollbar::windowClipRect() const
{
    IntRect clipRect(0, 0, width(), height());

    clipRect = convertToContainingWindow(clipRect);
    if (m_client)
        clipRect.intersect(m_client->windowClipRect());

    return clipRect;
}

}

#endif

