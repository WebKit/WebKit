/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FramelessScrollView.h"

#include "FramelessScrollViewClient.h"

namespace WebCore {

FramelessScrollView::~FramelessScrollView()
{
    // Remove native scrollbars now before we lose the connection to the HostWindow.
    setHasHorizontalScrollbar(false);
    setHasVerticalScrollbar(false);
}

void FramelessScrollView::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    // Add in our offset within the ScrollView.
    IntRect dirtyRect = rect;
    dirtyRect.move(scrollbar->x(), scrollbar->y());
    invalidateRect(dirtyRect);
}

bool FramelessScrollView::isActive() const
{
    // FIXME
    return true;
}

void FramelessScrollView::invalidateRect(const IntRect& rect)
{
    if (HostWindow* h = hostWindow())
        h->invalidateContentsAndWindow(rect, false /*immediate*/);
}

HostWindow* FramelessScrollView::hostWindow() const
{
    return const_cast<FramelessScrollViewClient*>(m_client);
}

IntRect FramelessScrollView::windowClipRect(bool clipToContents) const
{
    return contentsToWindow(visibleContentRect(!clipToContents));
}

void FramelessScrollView::paintContents(GraphicsContext*, const IntRect& damageRect)
{
}

void FramelessScrollView::contentsResized()
{
}

void FramelessScrollView::visibleContentsResized()
{
}

} // namespace WebCore
